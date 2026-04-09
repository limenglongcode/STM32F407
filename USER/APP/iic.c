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

#define EEPROM_I2C               I2C1
#define EEPROM_I2C_CLOCK         RCC_APB1Periph_I2C1
#define EEPROM_GPIO_PORT         GPIOB
#define EEPROM_GPIO_CLOCK        RCC_AHB1Periph_GPIOB
#define EEPROM_SCL_PIN           GPIO_Pin_8
#define EEPROM_SDA_PIN           GPIO_Pin_9
#define EEPROM_I2C_SPEED         400000U

#define EEPROM_BASE_ADDRESS      0xA0U

static uint8_t I2C_WaitEvent(uint32_t event);
static uint8_t GetDeviceAddress(uint32_t addr);
static uint8_t EEPROM_WaitReady(uint8_t dev_addr);
static uint8_t EEPROM_WritePage(uint32_t addr, const uint8_t *data, uint16_t len);
static void LifeTestStart(void);
static void LifeTestStop(const char *reason);
static void LifeTestStep(void);
static void LifeTestReportMismatch(uint32_t addr, uint8_t expected, uint8_t actual, const char *stage);
static void PageTestStart(void);
static void PageTestStop(const char *reason);
static void PageTestStep(void);
static void PageTestReportMismatch(uint32_t addr, uint8_t expected, uint8_t actual);

void SystemClock_Config(void);
void I2C_EEPROM_Init(void);
uint8_t EEPROM_WriteByte(uint32_t addr, uint8_t data);
uint8_t EEPROM_ReadByte(uint32_t addr, uint8_t *data);
void EEPROM_RunSingleAddressDemo(void);
void EEPROM_LifeTestToggle(void);
void EEPROM_LifeTestStep(void);
void EEPROM_PageTestToggle(void);
void EEPROM_PageTestStep(void);

static uint8_t life_test_running = 0U;
static uint32_t life_test_counter = 0U;
static uint8_t life_test_phase = 0U;
static uint16_t life_test_offset = 0U;
static uint8_t page_test_running = 0U;
static uint32_t page_test_counter = 0U;
static uint8_t page_test_pattern = 0U;
static volatile uint8_t page_stop_request = 0U;
static uint8_t page_write_buffer[EEPROM_PAGE_SIZE];

/* 初始化当前存储芯片使用的 I2C 外设与 GPIO。 */
void I2C_EEPROM_Init(void)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;

    RCC_APB1PeriphClockCmd(EEPROM_I2C_CLOCK, ENABLE);
    RCC_AHB1PeriphClockCmd(EEPROM_GPIO_CLOCK, ENABLE);

    gpio.GPIO_Pin = EEPROM_SCL_PIN | EEPROM_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(EEPROM_GPIO_PORT, &gpio);

    GPIO_PinAFConfig(EEPROM_GPIO_PORT, GPIO_PinSource8, GPIO_AF_I2C1);
    GPIO_PinAFConfig(EEPROM_GPIO_PORT, GPIO_PinSource9, GPIO_AF_I2C1);

    i2c.I2C_ClockSpeed = EEPROM_I2C_SPEED;
    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0x00U;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(EEPROM_I2C, &i2c);
    I2C_Cmd(EEPROM_I2C, ENABLE);
}

/* 等待指定 I2C 事件，防止总线异常时死循环。 */
static uint8_t I2C_WaitEvent(uint32_t event)
{
    uint32_t timeout = EEPROM_TIMEOUT;

    while (!I2C_CheckEvent(EEPROM_I2C, event)) {
        if (timeout-- == 0U) {
            return STATUS_ERROR;
        }
    }

    return STATUS_OK;
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
    uint32_t timeout = 10U;

    while (timeout--) {
        I2C_GenerateSTART(EEPROM_I2C, ENABLE);
        if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) {
            I2C_GenerateSTOP(EEPROM_I2C, ENABLE);
            continue;
        }

        I2C_Send7bitAddress(EEPROM_I2C, dev_addr, I2C_Direction_Transmitter);
        if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == STATUS_OK) {
            I2C_GenerateSTOP(EEPROM_I2C, ENABLE);
            return STATUS_OK;
        }

        I2C_ClearFlag(EEPROM_I2C, I2C_FLAG_AF);
        I2C_GenerateSTOP(EEPROM_I2C, ENABLE);
        Delay_us(100);
    }

    return STATUS_ERROR;
}

/* 向指定地址写入一个字节。 */
uint8_t EEPROM_WriteByte(uint32_t addr, uint8_t data)
{
    if (addr >= EEPROM_SIZE) {
        return STATUS_ERROR;
    }

    return EEPROM_WritePage(addr, &data, 1U);
}

/* 按当前页写时序连续写入多个字节。 */
static uint8_t EEPROM_WritePage(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint16_t offset;
    uint8_t dev;
    uint16_t index;

    if ((data == NULL) || (len == 0U)) {
        return STATUS_ERROR;
    }
    if ((addr + len) > EEPROM_SIZE) {
        return STATUS_ERROR;
    }
    if (((addr % EEPROM_PAGE_SIZE) + len) > EEPROM_PAGE_SIZE) {
        return STATUS_ERROR;
    }

    offset = (uint16_t)(addr & 0xFFFFU);
    dev = GetDeviceAddress(addr);

    I2C_GenerateSTART(EEPROM_I2C, ENABLE);
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) {
        return STATUS_ERROR;
    }

    I2C_Send7bitAddress(EEPROM_I2C, dev, I2C_Direction_Transmitter);
    if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != STATUS_OK) {
        return STATUS_ERROR;
    }

#if (EEPROM_ADDRESS_BYTES == 2U)
    I2C_SendData(EEPROM_I2C, (uint8_t)(offset >> 8));
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) {
        return STATUS_ERROR;
    }
#endif

    I2C_SendData(EEPROM_I2C, (uint8_t)(offset & 0xFFU));
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) {
        return STATUS_ERROR;
    }

    for (index = 0U; index < len; index++) {
        I2C_SendData(EEPROM_I2C, data[index]);
        if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) {
            return STATUS_ERROR;
        }
    }

    I2C_GenerateSTOP(EEPROM_I2C, ENABLE);

    if (EEPROM_WRITE_DELAY_MS > 0U) {
        Delay_ms(EEPROM_WRITE_DELAY_MS);
    }

#if !defined(USE_FM24C02C) && !defined(USE_FM24CL64B)
    if (EEPROM_WaitReady(dev) != STATUS_OK) {
        return STATUS_ERROR;
    }
#endif

    return STATUS_OK;
}

/* 从指定地址读取一个字节。 */
uint8_t EEPROM_ReadByte(uint32_t addr, uint8_t *data)
{
    uint16_t offset;
    uint8_t dev;

    if ((addr >= EEPROM_SIZE) || (data == NULL)) {
        return STATUS_ERROR;
    }

    offset = (uint16_t)(addr & 0xFFFFU);
    dev = GetDeviceAddress(addr);

    I2C_GenerateSTART(EEPROM_I2C, ENABLE);
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) {
        return STATUS_ERROR;
    }

    I2C_Send7bitAddress(EEPROM_I2C, dev, I2C_Direction_Transmitter);
    if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != STATUS_OK) {
        return STATUS_ERROR;
    }

#if (EEPROM_ADDRESS_BYTES == 2U)
    I2C_SendData(EEPROM_I2C, (uint8_t)(offset >> 8));
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) {
        return STATUS_ERROR;
    }
#endif

    I2C_SendData(EEPROM_I2C, (uint8_t)(offset & 0xFFU));
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) {
        return STATUS_ERROR;
    }

    I2C_GenerateSTART(EEPROM_I2C, ENABLE);
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) {
        return STATUS_ERROR;
    }

    I2C_Send7bitAddress(EEPROM_I2C, dev, I2C_Direction_Receiver);
    if (I2C_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != STATUS_OK) {
        return STATUS_ERROR;
    }

    I2C_AcknowledgeConfig(EEPROM_I2C, DISABLE);
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_RECEIVED) != STATUS_OK) {
        I2C_AcknowledgeConfig(EEPROM_I2C, ENABLE);
        return STATUS_ERROR;
    }

    *data = I2C_ReceiveData(EEPROM_I2C);
    I2C_GenerateSTOP(EEPROM_I2C, ENABLE);
    I2C_AcknowledgeConfig(EEPROM_I2C, ENABLE);

    return STATUS_OK;
}

/* 执行 WK_UP 对应的单地址读写演示。 */
void EEPROM_RunSingleAddressDemo(void)
{
    uint8_t verify = 0U;
    uint8_t read_value = 0U;

    USART_SendString("\r\n[INFO] Start write address ");
    USART_SendHex(TARGET_EEPROM_ADDRESS, 6);
    USART_SendString(" data ");
    USART_SendHex(TARGET_EEPROM_VALUE, 2);

    if (EEPROM_WriteByte(TARGET_EEPROM_ADDRESS, TARGET_EEPROM_VALUE) == STATUS_OK) {
        USART_SendString(" -> write success\r\n");
        if (EEPROM_ReadByte(TARGET_EEPROM_ADDRESS, &verify) == STATUS_OK) {
            USART_SendString("[INFO] Verify after write: ");
            USART_SendHex(verify, 2);
            if (verify == TARGET_EEPROM_VALUE) {
                USART_SendString(" -> verify ok\r\n");
            } else {
                USART_SendString(" -> verify failed\r\n");
            }
        } else {
            USART_SendString("[ERROR] verify read failed\r\n");
        }
    } else {
        USART_SendString(" -> write failed\r\n");
        return;
    }

    USART_SendString("[INFO] Standalone read address ");
    USART_SendHex(TARGET_EEPROM_ADDRESS, 6);
    USART_SendString(" -> ");
    if (EEPROM_ReadByte(TARGET_EEPROM_ADDRESS, &read_value) == STATUS_OK) {
        USART_SendString("read success value ");
        USART_SendHex(read_value, 2);
        USART_SendString("\r\n");
    } else {
        USART_SendString("read failed\r\n");
    }
}

/* KEY0 按键启动字节寿命测试。 */
void EEPROM_LifeTestToggle(void)
{
    if (!life_test_running) {
        LifeTestStart();
    }
}

/* 主循环调用的字节寿命测试步进函数。 */
void EEPROM_LifeTestStep(void)
{
    if (life_test_running) {
        LifeTestStep();
    }
}

/* KEY1 按键启动整页寿命测试。 */
void EEPROM_PageTestToggle(void)
{
    if (!page_test_running) {
        PageTestStart();
    }
}

/* 主循环调用的整页寿命测试步进函数。 */
void EEPROM_PageTestStep(void)
{
    if (page_test_running) {
        PageTestStep();
    }
}

/* 初始化 KEY0 测试状态并输出启动信息。 */
static void LifeTestStart(void)
{
    life_test_running = 1U;
    life_test_counter = 0U;

#ifdef USE_FM24NM01AI3
    USART_SendString("\r\n[KEY0] byte life test start addr ");
    USART_SendHex(LIFE_TEST_ADDRESS_START, 6);
    USART_SendString(" len ");
    USART_SendHex(LIFE_TEST_LENGTH, 4);
    USART_SendString(" pattern ");
    USART_SendHex(LIFE_DATA_FIRST, 2);
    USART_SendString("/");
    USART_SendHex(LIFE_DATA_SECOND, 2);
    USART_SendString("\r\n");
#else
    life_test_phase = 0U;
    life_test_offset = 0U;
    USART_SendString("\r\n[4byte] life test start addr ");
    USART_SendHex(LIFE_TEST_ADDRESS_START, 6);
    USART_SendString(" len ");
    USART_SendHex(LIFE_TEST_LENGTH, 2);
    USART_SendString(" pattern AA/55\r\n");
    USART_SendString("\r\n");
#endif
}

/* 停止 KEY0 测试并输出停止原因。 */
static void LifeTestStop(const char *reason)
{
    life_test_running = 0U;

#ifdef USE_FM24NM01AI3
    USART_SendString("[KEY0] byte life test stop reason: ");
    USART_SendString(reason);
    USART_SendString(" success cycles ");
    USART_SendDec(life_test_counter);
    USART_SendString("\r\n");
#else
    USART_SendString("[4byte] life test stop reason: ");
    USART_SendString(reason);
    USART_SendString(" total writes ");
    USART_SendDec(life_test_counter);
    USART_SendString("\r\n");
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
    page_test_running = 1U;
    page_test_counter = 0U;
    page_test_pattern = 0U;
    page_stop_request = 0U;

    USART_SendString("\r\n[page] page test start addr ");
    USART_SendHex(PAGE_TEST_ADDRESS, 6);
    USART_SendString(" len ");
    USART_SendHex(PAGE_TEST_LENGTH, 4);
    USART_SendString(" pattern ");
    USART_SendHex(PAGE_DATA_FIRST, 2);
    USART_SendString("/");
    USART_SendHex(PAGE_DATA_SECOND, 2);
    USART_SendString("\r\n");
}

/* 停止 KEY1 整页测试并输出原因。 */
static void PageTestStop(const char *reason)
{
    page_test_running = 0U;
    page_stop_request = 0U;
    USART_SendString("[page] page test stop reason: ");
    USART_SendString(reason);
    USART_SendString(" total cycles ");
    USART_SendDec(page_test_counter);
    USART_SendString("\r\n");
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
