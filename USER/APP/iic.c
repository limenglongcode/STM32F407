/*
 * iic.c
 *
 * I2C读写函数
 */
#include <stddef.h>
#include <stdint.h>
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_i2c.h"
#include "delay.h"
#include "config.h"
#include "uart.h"

#define EEPROM_I2C               I2C1                // 选择I2C1作为EEPROM通信外设
#define EEPROM_I2C_CLOCK         RCC_APB1Periph_I2C1 // I2C1对应APB1时钟
#define EEPROM_GPIO_PORT         GPIOB               // I2C引脚所在GPIO端口
#define EEPROM_GPIO_CLOCK        RCC_AHB1Periph_GPIOB // GPIOB对应AHB1时钟
#define EEPROM_SCL_PIN           GPIO_Pin_8          // I2C时钟线SCL使用PB8
#define EEPROM_SDA_PIN           GPIO_Pin_9          // I2C数据线SDA使用PB9
#define EEPROM_I2C_SPEED         400000U             // I2C总线速率400kHz

#define EEPROM_BASE_ADDRESS      0xA0U               // EEPROM器件7位地址左移后的基地址

static uint8_t I2C_WaitEvent(uint32_t event);        // 等待I2C事件并带超时保护
static uint8_t GetDeviceAddress(uint32_t addr);      // 根据地址计算目标器件地址
static uint8_t EEPROM_WaitReady(uint8_t dev_addr);   // 轮询EEPROM写周期完成状态
static uint8_t EEPROM_WritePage(uint32_t addr, const uint8_t *data, uint16_t len); // 按页写入连续数据
static void LifeTestStart(void);                     // 启动字节寿命测试
static void LifeTestStop(const char *reason);        // 停止字节寿命测试并上报原因
static void LifeTestStep(void);                      // 执行一次字节寿命测试步进
static void LifeTestReportMismatch(uint32_t addr, uint8_t expected, uint8_t actual, const char *stage); // 输出字节测试失配信息
static void PageTestStart(void);                     // 启动整页寿命测试
static void PageTestStop(const char *reason);        // 停止整页寿命测试并上报原因
static void PageTestStep(void);                      // 执行一次整页寿命测试步进
static void PageTestReportMismatch(uint32_t addr, uint8_t expected, uint8_t actual); // 输出页测试失配信息

void SystemClock_Config(void);                       // 系统时钟配置接口声明
void I2C_EEPROM_Init(void);                          // EEPROM所用I2C初始化接口
uint8_t EEPROM_WriteByte(uint32_t addr, uint8_t data); // 单字节写入接口
uint8_t EEPROM_ReadByte(uint32_t addr, uint8_t *data); // 单字节读取接口
void EEPROM_RunSingleAddressDemo(void);              // 单地址读写演示接口
void EEPROM_LifeTestToggle(void);                    // KEY0触发字节寿命测试入口
void EEPROM_LifeTestStep(void);                      // 主循环调用字节测试步进入口
void EEPROM_PageTestToggle(void);                    // KEY1触发整页寿命测试入口
void EEPROM_PageTestStep(void);                      // 主循环调用页测试步进入口

static uint8_t life_test_running = 0U;               // 字节寿命测试运行标志
static uint32_t life_test_counter = 0U;              // 字节寿命测试成功轮次计数
static uint8_t life_test_phase = 0U;                 // 字节测试当前写入模式阶段
static uint16_t life_test_offset = 0U;               // 字节测试当前地址偏移
static uint8_t page_test_running = 0U;               // 整页寿命测试运行标志
static uint32_t page_test_counter = 0U;              // 整页寿命测试成功轮次计数
static uint8_t page_test_pattern = 0U;               // 整页测试当前模式索引
static volatile uint8_t page_stop_request = 0U;      // 整页测试停止请求标志
static uint8_t page_write_buffer[EEPROM_PAGE_SIZE];  // 页写缓存区

/* 初始化当前存储芯片使用的 I2C 外设与 GPIO。 */
void I2C_EEPROM_Init(void)
{
    GPIO_InitTypeDef gpio;                          // 定义GPIO配置结构体
    I2C_InitTypeDef i2c;                            // 定义I2C配置结构体

    RCC_APB1PeriphClockCmd(EEPROM_I2C_CLOCK, ENABLE); // 开启I2C外设时钟
    RCC_AHB1PeriphClockCmd(EEPROM_GPIO_CLOCK, ENABLE); // 开启GPIO端口时钟

    gpio.GPIO_Pin = EEPROM_SCL_PIN | EEPROM_SDA_PIN; // 选择SCL和SDA引脚
    gpio.GPIO_Mode = GPIO_Mode_AF;                 // 设置为复用功能模式
    gpio.GPIO_OType = GPIO_OType_OD;               // I2C总线使用开漏输出
    gpio.GPIO_Speed = GPIO_Speed_50MHz;            // 设置引脚速度为50MHz
    gpio.GPIO_PuPd = GPIO_PuPd_UP;                 // 使能上拉保持总线空闲高电平
    GPIO_Init(EEPROM_GPIO_PORT, &gpio);            // 初始化I2C对应GPIO

    GPIO_PinAFConfig(EEPROM_GPIO_PORT, GPIO_PinSource8, GPIO_AF_I2C1); // 配置PB8复用为I2C1_SCL
    GPIO_PinAFConfig(EEPROM_GPIO_PORT, GPIO_PinSource9, GPIO_AF_I2C1); // 配置PB9复用为I2C1_SDA

    i2c.I2C_ClockSpeed = EEPROM_I2C_SPEED;         // 设置I2C通信速率
    i2c.I2C_Mode = I2C_Mode_I2C;                   // 选择标准I2C模式
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;           // 配置快速模式占空比
    i2c.I2C_OwnAddress1 = 0x00U;                   // 主机模式下本机地址占位
    i2c.I2C_Ack = I2C_Ack_Enable;                  // 使能接收应答
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit; // 使用7位地址模式
    I2C_Init(EEPROM_I2C, &i2c);                    // 初始化I2C外设寄存器
    I2C_Cmd(EEPROM_I2C, ENABLE);                   // 使能I2C外设
}

/* 等待指定 I2C 事件，防止总线异常时死循环。 */
static uint8_t I2C_WaitEvent(uint32_t event)
{
    uint32_t timeout = EEPROM_TIMEOUT;             // 设置事件等待超时计数

    while (!I2C_CheckEvent(EEPROM_I2C, event)) {
        if (timeout-- == 0U) {
            return STATUS_ERROR;                    // 超时返回错误避免死循环
        }
    }

    return STATUS_OK;                               // 等待到目标事件返回成功
}

/* 根据芯片容量与寻址方式，计算当前地址对应的器件地址。 */
static uint8_t GetDeviceAddress(uint32_t addr)
{
#if defined(USE_FM24C02C) || defined(USE_M24512E) || defined(USE_FM24CL64B)
    (void)addr;
    return EEPROM_BASE_ADDRESS;
#elif defined(USE_FM24NM01AI3)
    return (uint8_t)(EEPROM_BASE_ADDRESS + (((addr >> 16) & 0x01U) << 1));
#else
    return (uint8_t)(EEPROM_BASE_ADDRESS + (((addr >> 16) & 0x03U) << 1));
#endif
}

/* EEPROM 写入后轮询 ACK，确认内部写周期已完成。 */
static uint8_t EEPROM_WaitReady(uint8_t dev_addr)
{
    uint32_t timeout = 10U;                        // 最多轮询10次ACK应答

    while (timeout--) {
        I2C_GenerateSTART(EEPROM_I2C, ENABLE);     // 发送START开始一次探测
        if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) {
            I2C_GenerateSTOP(EEPROM_I2C, ENABLE);  // 起始失败时发送STOP恢复总线
            continue;                               // 继续下一轮轮询
        }

        I2C_Send7bitAddress(EEPROM_I2C, dev_addr, I2C_Direction_Transmitter); // 发送设备地址尝试写模式寻址
        if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == STATUS_OK) {
            I2C_GenerateSTOP(EEPROM_I2C, ENABLE);  // 收到ACK后结束本次探测
            return STATUS_OK;                       // 设备就绪返回成功
        }

        I2C_ClearFlag(EEPROM_I2C, I2C_FLAG_AF);    // 清除NACK失败标志
        I2C_GenerateSTOP(EEPROM_I2C, ENABLE);      // 当前探测失败后发送STOP
        Delay_us(100);                              // 稍作延时后再轮询
    }

    return STATUS_ERROR;                            // 超出轮询次数仍未就绪
}

/* 向指定地址写入一个字节。 */
uint8_t EEPROM_WriteByte(uint32_t addr, uint8_t data)
{
    if (addr >= EEPROM_SIZE) {
        return STATUS_ERROR;                        // 地址越界直接返回失败
    }

    return EEPROM_WritePage(addr, &data, 1U);      // 复用页写接口写入单字节
}

/* 按当前页写时序连续写入多个字节。 */
static uint8_t EEPROM_WritePage(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint16_t offset;                                // 页内偏移地址
    uint8_t dev;                                    // 设备地址
    uint16_t index;                                 // 数据写入循环索引

    if ((data == NULL) || (len == 0U)) {
        return STATUS_ERROR;                        // 数据指针为空或长度为0
    }
    if ((addr + len) > EEPROM_SIZE) {
        return STATUS_ERROR;                        // 写入范围超过芯片总容量
    }
    if (((addr % EEPROM_PAGE_SIZE) + len) > EEPROM_PAGE_SIZE) {
        return STATUS_ERROR;                        // 跨页写入不允许
    }

    offset = (uint16_t)(addr & 0xFFFFU);           // 取当前芯片内16位地址偏移
    dev = GetDeviceAddress(addr);                  // 计算当前页对应器件地址

    I2C_GenerateSTART(EEPROM_I2C, ENABLE);         // 发送起始信号
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) {
        return STATUS_ERROR;                        // 等待主机模式选择事件失败
    }

    I2C_Send7bitAddress(EEPROM_I2C, dev, I2C_Direction_Transmitter); // 发送器件地址进入写模式
    if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != STATUS_OK) {
        return STATUS_ERROR;                        // 地址阶段未收到应答
    }

#if (EEPROM_ADDRESS_BYTES == 2U)
    I2C_SendData(EEPROM_I2C, (uint8_t)(offset >> 8)); // 发送高位地址字节
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) {
        return STATUS_ERROR;                        // 高位地址发送失败
    }
#endif

    I2C_SendData(EEPROM_I2C, (uint8_t)(offset & 0xFFU)); // 发送低位地址字节
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) {
        return STATUS_ERROR;                        // 低位地址发送失败
    }

    for (index = 0U; index < len; index++) {
        I2C_SendData(EEPROM_I2C, data[index]);      // 按顺序发送页内数据
        if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) {
            return STATUS_ERROR;                    // 任一数据字节发送失败
        }
    }

    I2C_GenerateSTOP(EEPROM_I2C, ENABLE);          // 发送停止信号完成写传输

    if (EEPROM_WRITE_DELAY_MS > 0U) {
        Delay_ms(EEPROM_WRITE_DELAY_MS);           // 按配置等待内部写周期
    }

#if !defined(USE_FM24C02C) && !defined(USE_FM24CL64B)
    if (EEPROM_WaitReady(dev) != STATUS_OK) {
        return STATUS_ERROR;                        // 写后轮询就绪失败
    }
#endif

    return STATUS_OK;                               // 页写流程成功完成
}

/* 从指定地址读取一个字节。 */
uint8_t EEPROM_ReadByte(uint32_t addr, uint8_t *data)
{
    uint16_t offset;                                // 芯片内部地址偏移
    uint8_t dev;                                    // 目标器件地址

    if ((addr >= EEPROM_SIZE) || (data == NULL)) {
        return STATUS_ERROR;                        // 地址或指针非法
    }

    offset = (uint16_t)(addr & 0xFFFFU);           // 取有效地址低16位
    dev = GetDeviceAddress(addr);                  // 计算当前地址所属器件地址

    I2C_GenerateSTART(EEPROM_I2C, ENABLE);         // 先发START进入写地址阶段
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) {
        return STATUS_ERROR;                        // 起始阶段失败
    }

    I2C_Send7bitAddress(EEPROM_I2C, dev, I2C_Direction_Transmitter); // 发送器件地址并准备写入内部地址
    if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != STATUS_OK) {
        return STATUS_ERROR;                        // 地址应答失败
    }

#if (EEPROM_ADDRESS_BYTES == 2U)
    I2C_SendData(EEPROM_I2C, (uint8_t)(offset >> 8)); // 发送高位地址
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) {
        return STATUS_ERROR;                        // 高位地址发送失败
    }
#endif

    I2C_SendData(EEPROM_I2C, (uint8_t)(offset & 0xFFU)); // 发送低位地址
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) {
        return STATUS_ERROR;                        // 低位地址发送失败
    }

    I2C_GenerateSTART(EEPROM_I2C, ENABLE);         // 重启总线发起读方向访问
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) {
        return STATUS_ERROR;                        // 重起始失败
    }

    I2C_Send7bitAddress(EEPROM_I2C, dev, I2C_Direction_Receiver); // 发送器件地址进入读模式
    if (I2C_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != STATUS_OK) {
        return STATUS_ERROR;                        // 接收模式选择失败
    }

    I2C_AcknowledgeConfig(EEPROM_I2C, DISABLE);    // 单字节读取需提前关闭ACK
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_RECEIVED) != STATUS_OK) {
        I2C_AcknowledgeConfig(EEPROM_I2C, ENABLE); // 异常时恢复ACK默认状态
        return STATUS_ERROR;                        // 等待接收数据失败
    }

    *data = I2C_ReceiveData(EEPROM_I2C);           // 读取接收到的1字节数据
    I2C_GenerateSTOP(EEPROM_I2C, ENABLE);          // 发送STOP结束传输
    I2C_AcknowledgeConfig(EEPROM_I2C, ENABLE);     // 恢复ACK使能供后续通信使用

    return STATUS_OK;                               // 读字节成功返回
}

/* 执行 WK_UP 对应的单地址读写演示。 */
void EEPROM_RunSingleAddressDemo(void)
{
    uint8_t verify = 0U;                            // 写后校验读取值
    uint8_t read_value = 0U;                        // 独立读取演示值

    USART_SendString("\r\n[INFO] Start write address "); // 打印写入动作提示
    USART_SendHex(TARGET_EEPROM_ADDRESS, 6);        // 打印目标地址
    USART_SendString(" data ");                     // 打印数据提示
    USART_SendHex(TARGET_EEPROM_VALUE, 2);          // 打印待写入数据

    if (EEPROM_WriteByte(TARGET_EEPROM_ADDRESS, TARGET_EEPROM_VALUE) == STATUS_OK) {
        USART_SendString(" -> write success\r\n");  // 打印写入成功
        if (EEPROM_ReadByte(TARGET_EEPROM_ADDRESS, &verify) == STATUS_OK) {
            USART_SendString("[INFO] Verify after write: "); // 打印写后校验提示
            USART_SendHex(verify, 2);               // 打印校验读取值
            if (verify == TARGET_EEPROM_VALUE) {
                USART_SendString(" -> verify ok\r\n"); // 校验一致
            } else {
                USART_SendString(" -> verify failed\r\n"); // 校验不一致
            }
        } else {
            USART_SendString("[ERROR] verify read failed\r\n"); // 校验读取失败
        }
    } else {
        USART_SendString(" -> write failed\r\n");   // 写入失败提示
        return;                                     // 写失败直接退出演示
    }

    USART_SendString("[INFO] Standalone read address "); // 打印独立读取提示
    USART_SendHex(TARGET_EEPROM_ADDRESS, 6);        // 打印独立读取地址
    USART_SendString(" -> ");                       // 打印箭头分隔符
    if (EEPROM_ReadByte(TARGET_EEPROM_ADDRESS, &read_value) == STATUS_OK) {
        USART_SendString("read success value ");    // 打印读取成功提示
        USART_SendHex(read_value, 2);               // 打印读取到的数据
        USART_SendString("\r\n");                   // 换行
    } else {
        USART_SendString("read failed\r\n");        // 打印读取失败
    }
}

/* KEY0 按键启动字节寿命测试。 */
void EEPROM_LifeTestToggle(void)
{
    if (!life_test_running) {
        LifeTestStart();                             // 未运行时启动字节寿命测试
    }
}

/* 主循环调用的字节寿命测试步进函数。 */
void EEPROM_LifeTestStep(void)
{
    if (life_test_running) {
        LifeTestStep();                              // 运行状态下执行一次测试步进
    }
}

/* KEY1 按键启动整页寿命测试。 */
void EEPROM_PageTestToggle(void)
{
    if (!page_test_running) {
        PageTestStart();                             // 未运行时启动整页寿命测试
    }
}

/* 主循环调用的整页寿命测试步进函数。 */
void EEPROM_PageTestStep(void)
{
    if (page_test_running) {
        PageTestStep();                              // 运行状态下执行一次页测试步进
    }
}

/* 初始化 KEY0 测试状态并输出启动信息。 */
static void LifeTestStart(void)
{
    life_test_running = 1U;                          // 标记字节寿命测试已启动
    life_test_counter = 0U;                          // 清零成功轮次计数

#ifdef USE_FM24NM01AI3
    USART_SendString("\r\n[KEY0] byte life test start addr "); // 打印测试启动信息
    USART_SendHex(LIFE_TEST_ADDRESS_START, 6);       // 打印测试起始地址
    USART_SendString(" len ");                       // 打印长度标签
    USART_SendHex(LIFE_TEST_LENGTH, 4);              // 打印测试长度
    USART_SendString(" pattern ");                   // 打印模式标签
    USART_SendHex(LIFE_DATA_FIRST, 2);               // 打印第一组模式值
    USART_SendString("/");                           // 打印分隔符
    USART_SendHex(LIFE_DATA_SECOND, 2);              // 打印第二组模式值
    USART_SendString("\r\n");                        // 输出换行
#else
    life_test_phase = 0U;                            // 初始化当前写入模式阶段
    life_test_offset = 0U;                           // 初始化地址偏移为0
    USART_SendString("\r\n[4byte] life test start addr "); // 打印测试启动信息
    USART_SendHex(LIFE_TEST_ADDRESS_START, 6);       // 打印测试起始地址
    USART_SendString(" len ");                       // 打印长度标签
    USART_SendHex(LIFE_TEST_LENGTH, 2);              // 打印测试长度
    USART_SendString(" pattern AA/55\r\n");          // 打印固定测试模式
    USART_SendString("\r\n");                        // 输出空行分隔
#endif
}

/* 停止 KEY0 测试并输出停止原因。 */
static void LifeTestStop(const char *reason)
{
    life_test_running = 0U;                          // 标记字节寿命测试停止

#ifdef USE_FM24NM01AI3
    USART_SendString("[KEY0] byte life test stop reason: "); // 打印停止原因前缀
    USART_SendString(reason);                        // 打印停止原因
    USART_SendString(" success cycles ");            // 打印计数标签
    USART_SendDec(life_test_counter);                // 打印成功轮次
    USART_SendString("\r\n");                        // 换行结束
#else
    USART_SendString("[4byte] life test stop reason: "); // 打印停止原因前缀
    USART_SendString(reason);                        // 打印停止原因
    USART_SendString(" total writes ");              // 打印总写入标签
    USART_SendDec(life_test_counter);                // 打印写入次数
    USART_SendString("\r\n");                        // 换行结束
#endif
}

/* 输出 KEY0 校验失败时的详细地址和数据。 */
static void LifeTestReportMismatch(uint32_t addr, uint8_t expected, uint8_t actual, const char *stage)
{
    USART_SendString("[KEY0] verify fail stage ");
    USART_SendString(stage);
    USART_SendString(" addr ");
    USART_SendHex(addr, 6);
    USART_SendString(" target ");
    USART_SendHex(expected, 2);
    USART_SendString(" read ");
    USART_SendHex(actual, 2);
    USART_SendString("\r\n");
}

/* 执行 KEY0 字节寿命测试。 */
static void LifeTestStep(void)
{
#ifdef USE_FM24NM01AI3
    uint32_t offset;
    uint32_t addr;
    uint8_t readback;

    if ((LIFE_TEST_ADDRESS_START + LIFE_TEST_LENGTH) > EEPROM_SIZE) {
        LifeTestStop("range overflow");
        return;
    }

    for (offset = 0U; offset < LIFE_TEST_LENGTH; offset++) {
        addr = LIFE_TEST_ADDRESS_START + offset;

        if (EEPROM_WriteByte(addr, LIFE_DATA_FIRST) != STATUS_OK) {
            USART_SendString("[KEY0] write fail stage pattern1 addr ");
            USART_SendHex(addr, 6);
            USART_SendString(" target ");
            USART_SendHex(LIFE_DATA_FIRST, 2);
            USART_SendString("\r\n");
            LifeTestStop("write error");
            return;
        }
        if (EEPROM_ReadByte(addr, &readback) != STATUS_OK) {
            USART_SendString("[KEY0] read fail stage pattern1 addr ");
            USART_SendHex(addr, 6);
            USART_SendString("\r\n");
            LifeTestStop("read error");
            return;
        }
        if (readback != LIFE_DATA_FIRST) {
            LifeTestReportMismatch(addr, LIFE_DATA_FIRST, readback, "pattern1");
            LifeTestStop("verify mismatch");
            return;
        }
    }

    for (offset = 0U; offset < LIFE_TEST_LENGTH; offset++) {
        addr = LIFE_TEST_ADDRESS_START + offset;

        if (EEPROM_WriteByte(addr, LIFE_DATA_SECOND) != STATUS_OK) {
            USART_SendString("[KEY0] write fail stage pattern2 addr ");
            USART_SendHex(addr, 6);
            USART_SendString(" target ");
            USART_SendHex(LIFE_DATA_SECOND, 2);
            USART_SendString("\r\n");
            LifeTestStop("write error");
            return;
        }
        if (EEPROM_ReadByte(addr, &readback) != STATUS_OK) {
            USART_SendString("[KEY0] read fail stage pattern2 addr ");
            USART_SendHex(addr, 6);
            USART_SendString("\r\n");
            LifeTestStop("read error");
            return;
        }
        if (readback != LIFE_DATA_SECOND) {
            LifeTestReportMismatch(addr, LIFE_DATA_SECOND, readback, "pattern2");
            LifeTestStop("verify mismatch");
            return;
        }
    }

    life_test_counter++;
    if ((life_test_counter % 100U) == 0U) {
        USART_SendString("[KEY0] success cycles ");
        USART_SendDec(life_test_counter);
        USART_SendString("\r\n");
    }
#else
    uint32_t addr = LIFE_TEST_ADDRESS_START + life_test_offset;
    uint8_t target = (life_test_phase == 0U) ? LIFE_DATA_AA : LIFE_DATA_55;
    uint8_t readback = 0U;

    if (EEPROM_WriteByte(addr, target) != STATUS_OK) {
        USART_SendString("Write error @ 0x");
        USART_SendHex(addr, 6);
        USART_SendString(" data 0x");
        USART_SendHex(target, 2);
        USART_SendString("\r\n");
        LifeTestStop("write error");
        return;
    }
    if (EEPROM_ReadByte(addr, &readback) != STATUS_OK) {
        USART_SendString("Read error @ 0x");
        USART_SendHex(addr, 6);
        USART_SendString("\r\n");
        LifeTestStop("read error");
        return;
    }
    if (readback != target) {
        uint32_t fail_round = life_test_counter + 1U;
        USART_SendString("Data mismatch @ 0x");
        USART_SendHex(addr, 6);
        USART_SendString(" Expected:0x");
        USART_SendHex(target, 2);
        USART_SendString(" Actual:0x");
        USART_SendHex(readback, 2);
        USART_SendString("test number ");
        USART_SendDec(fail_round);
        USART_SendString(" NO ");
        USART_SendString("\r\n");
        LifeTestStop("data mismatch");
        return;
    }

    life_test_offset++;
    if (life_test_offset >= LIFE_TEST_LENGTH) {
        life_test_offset = 0U;
        life_test_phase ^= 0x01U;
        life_test_counter++;
        if ((life_test_counter % 100U) == 0U) {
            USART_SendString("test number ");
            USART_SendDec(life_test_counter);
            USART_SendString(" OK\r\n");
        }
    }
#endif
}

/* 初始化 KEY1 整页寿命测试状态。 */
static void PageTestStart(void)
{
    page_test_running = 1U;                          // 标记整页寿命测试已启动
    page_test_counter = 0U;                          // 清零整页测试计数
    page_test_pattern = 0U;                          // 初始化为第一组测试模式
    page_stop_request = 0U;                          // 清除停止请求标志

    USART_SendString("\r\n[page] page test start addr "); // 打印整页测试启动信息
    USART_SendHex(PAGE_TEST_ADDRESS, 6);             // 打印页测试起始地址
    USART_SendString(" len ");                       // 打印长度标签
    USART_SendHex(PAGE_TEST_LENGTH, 4);              // 打印页长度
    USART_SendString(" pattern ");                   // 打印模式标签
    USART_SendHex(PAGE_DATA_FIRST, 2);               // 打印第一模式值
    USART_SendString("/");                           // 打印分隔符
    USART_SendHex(PAGE_DATA_SECOND, 2);              // 打印第二模式值
    USART_SendString("\r\n");                        // 换行结束
}

/* 停止 KEY1 整页测试并输出原因。 */
static void PageTestStop(const char *reason)
{
    page_test_running = 0U;                          // 标记整页寿命测试停止
    page_stop_request = 0U;                          // 清除停止请求标志
    USART_SendString("[page] page test stop reason: "); // 打印停止原因前缀
    USART_SendString(reason);                        // 打印停止原因文本
    USART_SendString(" total cycles ");              // 打印总轮次标签
    USART_SendDec(page_test_counter);                // 打印当前完成轮次
    USART_SendString("\r\n");                        // 换行结束
}

/* 输出 KEY1 整页校验失败时的详细地址和数据。 */
static void PageTestReportMismatch(uint32_t addr, uint8_t expected, uint8_t actual)
{
    USART_SendString("[page] verify fail addr ");
    USART_SendHex(addr, 6);
    USART_SendString(" target ");
    USART_SendHex(expected, 2);
    USART_SendString(" read ");
    USART_SendHex(actual, 2);
    USART_SendString("\r\n");
}

/* 执行 KEY1 整页寿命测试。 */
static void PageTestStep(void)
{
    uint32_t start_addr = PAGE_TEST_ADDRESS;
    uint8_t pattern = (page_test_pattern == 0U) ? PAGE_DATA_FIRST : PAGE_DATA_SECOND;
    uint16_t offset;

    if ((start_addr + PAGE_TEST_LENGTH) > EEPROM_SIZE) {
        PageTestStop("range overflow");
        return;
    }

#ifdef USE_FM24NM01AI3
    if ((start_addr % EEPROM_PAGE_SIZE) != 0U) {
        PageTestStop("page addr unaligned");
        return;
    }

    for (offset = 0U; offset < PAGE_TEST_LENGTH; offset++) {
        page_write_buffer[offset] = pattern;
    }

    if (EEPROM_WritePage(start_addr, page_write_buffer, PAGE_TEST_LENGTH) != STATUS_OK) {
        PageTestStop("page write error");
        return;
    }

    for (offset = 0U; offset < PAGE_TEST_LENGTH; offset++) {
        uint8_t readback = 0U;
        uint32_t addr = start_addr + offset;

        if (EEPROM_ReadByte(addr, &readback) != STATUS_OK) {
            PageTestStop("read error");
            return;
        }
        if (readback != pattern) {
            PageTestReportMismatch(addr, pattern, readback);
            PageTestStop("verify mismatch");
            return;
        }
    }

    pattern = (page_test_pattern == 0U) ? PAGE_DATA_SECOND : PAGE_DATA_FIRST;
    for (offset = 0U; offset < PAGE_TEST_LENGTH; offset++) {
        page_write_buffer[offset] = pattern;
    }

    if (EEPROM_WritePage(start_addr, page_write_buffer, PAGE_TEST_LENGTH) != STATUS_OK) {
        PageTestStop("page write error");
        return;
    }

    for (offset = 0U; offset < PAGE_TEST_LENGTH; offset++) {
        uint8_t readback = 0U;
        uint32_t addr = start_addr + offset;

        if (EEPROM_ReadByte(addr, &readback) != STATUS_OK) {
            PageTestStop("read error");
            return;
        }
        if (readback != pattern) {
            PageTestReportMismatch(addr, pattern, readback);
            PageTestStop("verify mismatch");
            return;
        }
    }

    page_test_counter++;
    USART_SendString("[page] Count ");
    USART_SendDec(page_test_counter);
    USART_SendString(" OK\r\n");
#else
    for (offset = 0U; offset < PAGE_TEST_LENGTH; offset++) {
        if (page_stop_request) {
            PageTestStop("manual stop");
            return;
        }
        if (EEPROM_WriteByte(start_addr + offset, pattern) != STATUS_OK) {
            PageTestStop("write error");
            return;
        }
    }

    for (offset = 0U; offset < PAGE_TEST_LENGTH; offset++) {
        uint8_t readback = 0U;
        uint32_t addr = start_addr + offset;

        if (page_stop_request) {
            PageTestStop("manual stop");
            return;
        }
        if (EEPROM_ReadByte(addr, &readback) != STATUS_OK) {
            PageTestStop("read error");
            return;
        }
        if (readback != pattern) {
            USART_SendString("Count ");
            USART_SendDec(page_test_counter + 1U);
            USART_SendString(" NO ");
            USART_SendHex(readback, 2);
            USART_SendString("\r\n");
            PageTestStop("data mismatch");
            return;
        }
    }

    if (page_stop_request) {
        PageTestStop("manual stop");
        return;
    }

    page_test_counter++;
    page_test_pattern ^= 0x01U;
    USART_SendString("[page] Count ");
    USART_SendDec(page_test_counter);
    USART_SendString(" OK\r\n");
#endif
}
