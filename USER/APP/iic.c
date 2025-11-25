/*
 * iic.c
 *
 *  Created on: 2025年11月20日
 *  Author: 
 *  功能 ：I2C读写函数
 */
#include <stdint.h>                           // 引入标准整数类型定义
#include <stddef.h>                           // 引入stddef定义NULL指针常量
#include "stm32f4xx.h"                       // 引入核心设备头文件，提供寄存器定义与系统配置函数
#include "stm32f4xx_gpio.h"                  // 引入GPIO标准外设库头文件
#include "stm32f4xx_rcc.h"                   // 引入RCC头文件以便配置时钟
#include "stm32f4xx_i2c.h"                   // 引入I2C标准外设库头文件
#include "delay.h"                           // 使用 SysTick 延时函数
#include "config.h"
#include "uart.h"

#define EEPROM_I2C               I2C1
#define EEPROM_I2C_CLOCK         RCC_APB1Periph_I2C1
#define EEPROM_GPIO_PORT         GPIOB
#define EEPROM_GPIO_CLOCK        RCC_AHB1Periph_GPIOB
#define EEPROM_SCL_PIN           GPIO_Pin_8
#define EEPROM_SDA_PIN           GPIO_Pin_9
#define EEPROM_I2C_SPEED         400000U

#define EEPROM_BASE_ADDRESS      0xA0        // E0/E1/E2接地后的器件地址基值

static uint8_t I2C_WaitEvent(uint32_t event);// 声明I2C事件等待函数
static uint8_t GetDeviceAddress(uint32_t addr); // 声明根据地址获取器件地址的函数
static uint8_t EEPROM_WaitReady(uint8_t dev_addr); // 声明写后ACK轮询函数
static void LifeTestStart(void);             // 声明寿命测试启动函数
static void LifeTestStop(const char *reason);// 声明寿命测试停止函数
static void LifeTestStep(void);              // 声明寿命测试执行函数
static void PageTestStart(void);             // 声明页寿命测试启动函数
static void PageTestStop(const char *reason);// 声明页寿命测试停止函数
static void PageTestStep(void);              // 声明页寿命测试执行函数

void SystemClock_Config(void);               // 声明系统时钟配置函数
 void I2C_EEPROM_Init(void);                  // 声明I2C初始化函数
 uint8_t EEPROM_WriteByte(uint32_t addr, uint8_t data); // 声明EEPROM写函数
 uint8_t EEPROM_ReadByte(uint32_t addr, uint8_t *data); // 声明EEPROM读函数

 // 测试控制接口（供 main.c 调用）
 void EEPROM_RunSingleAddressDemo(void);
 void EEPROM_LifeTestToggle(void);
 void EEPROM_LifeTestStep(void);
 void EEPROM_PageTestToggle(void);
 void EEPROM_PageTestStep(void);

 // 寿命/页测试状态变量
 static uint8_t life_test_running = 0U;       // 寿命测试状态标志
 static uint32_t life_test_counter = 0U;      // 寿命测试已写次数
 static uint8_t life_test_phase = 0U;         // 0表示写AA,1表示写55
 static uint8_t life_test_offset = 0U;        // 当前处理的字节偏移
 static uint8_t page_test_running = 0U;       // 页寿命测试状态
 static uint32_t page_test_counter = 0U;      // 页寿命测试轮次
 static uint8_t page_test_pattern = 0U;       // 页寿命测试当前数据模式
 static volatile uint8_t page_stop_request = 0U; // 页寿命测试停止请求

void I2C_EEPROM_Init(void) {                 // 定义I2C初始化函数
    RCC_APB1PeriphClockCmd(EEPROM_I2C_CLOCK, ENABLE); // 使能I2C1时钟
    RCC_AHB1PeriphClockCmd(EEPROM_GPIO_CLOCK, ENABLE); // 使能GPIOB时钟
    GPIO_InitTypeDef gpio;                   // 创建GPIO配置结构体
    gpio.GPIO_Pin = EEPROM_SCL_PIN | EEPROM_SDA_PIN; // 选择PB8与PB9
    gpio.GPIO_Mode = GPIO_Mode_AF;           // 配置为复用模式
    gpio.GPIO_OType = GPIO_OType_OD;         // 设置成开漏输出
    gpio.GPIO_Speed = GPIO_Speed_50MHz;      // 设置端口速度
    gpio.GPIO_PuPd = GPIO_PuPd_UP;           // 使能上拉
    GPIO_Init(EEPROM_GPIO_PORT, &gpio);      // 初始化GPIOB
    GPIO_PinAFConfig(EEPROM_GPIO_PORT, GPIO_PinSource8, GPIO_AF_I2C1); // PB8映射到I2C1_SCL
    GPIO_PinAFConfig(EEPROM_GPIO_PORT, GPIO_PinSource9, GPIO_AF_I2C1); // PB9映射到I2C1_SDA
    I2C_InitTypeDef i2c;                     // 创建I2C配置结构体
    i2c.I2C_ClockSpeed = EEPROM_I2C_SPEED;   // 设置I2C速率
    i2c.I2C_Mode = I2C_Mode_I2C;             // 选择I2C模式
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;     // 设置占空比
    i2c.I2C_OwnAddress1 = 0x00;              // 主机地址设置为0
    i2c.I2C_Ack = I2C_Ack_Enable;            // 使能ACK
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit; // 使用7位地址
    I2C_Init(EEPROM_I2C, &i2c);              // 初始化I2C1
    I2C_Cmd(EEPROM_I2C, ENABLE);             // 使能I2C1外设
}                                            // 结束I2C_EEPROM_Init函数

static uint8_t I2C_WaitEvent(uint32_t event) { // 定义I2C事件等待函数
    uint32_t timeout = EEPROM_TIMEOUT;       // 初始化超时计数
    while (!I2C_CheckEvent(EEPROM_I2C, event)) { // 轮询事件状态
        if (timeout-- == 0U) {               // 判断是否超时
            return STATUS_ERROR;             // 超时返回错误
        }                                    // 超时则提前结束
    }                                        // 事件满足后退出循环
    return STATUS_OK;                        // 返回成功状态
}                                            // 结束I2C_WaitEvent函数

static uint8_t GetDeviceAddress(uint32_t addr) { // 定义器件地址获取函数
    uint8_t page = (uint8_t)((addr >> 16) & 0x03U); // 提取高两位页号
    return (uint8_t)(EEPROM_BASE_ADDRESS + (page << 1)); // 根据页号计算器件地址
}                                            // 结束GetDeviceAddress函数

static uint8_t EEPROM_WaitReady(uint8_t dev_addr) { // 定义写后轮询ACK函数
    uint32_t timeout = 10U;                  // 快速轮询，最多10次尝试（约5-10ms）
    while (timeout--) {                      // 循环尝试
        I2C_GenerateSTART(EEPROM_I2C, ENABLE); // 发起START
        if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) {
            I2C_GenerateSTOP(EEPROM_I2C, ENABLE); // 失败时释放总线
            continue;                        // 未进入主模式继续重试
        }
        I2C_Send7bitAddress(EEPROM_I2C, dev_addr, I2C_Direction_Transmitter); // 发送写命令探测ACK
        if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == STATUS_OK) {
            I2C_GenerateSTOP(EEPROM_I2C, ENABLE); // 设备应答，退出
            return STATUS_OK;
        }
        I2C_ClearFlag(EEPROM_I2C, I2C_FLAG_AF); // 清除无应答标志
        I2C_GenerateSTOP(EEPROM_I2C, ENABLE);  // 释放总线
        // 无延时快速重试，典型情况下3-5ms内就能完成
        Delay_us(100);
    }
    return STATUS_ERROR;                      // 超时仍未就绪
}                                            // 结束EEPROM_WaitReady函数

uint8_t EEPROM_WriteByte(uint32_t addr, uint8_t data) { // 定义EEPROM写函数
    if (addr >= EEPROM_SIZE) {               // 检查地址是否越界
        return STATUS_ERROR;                 // 越界返回错误
    }                                        // 合法则继续
    uint16_t offset = (uint16_t)(addr & 0xFFFFU); // 计算页内地址
    uint8_t dev = GetDeviceAddress(addr);    // 获取器件地址
    I2C_GenerateSTART(EEPROM_I2C, ENABLE);   // 发送起始信号
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) { // 等待进入主模式
        return STATUS_ERROR;                 // 失败返回错误
    }                                        // 成功继续
    I2C_Send7bitAddress(EEPROM_I2C, dev, I2C_Direction_Transmitter); // 发送写命令
    if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != STATUS_OK) { // 等待ACK
        return STATUS_ERROR;                 // 失败返回错误
    }                                        // 成功继续
    I2C_SendData(EEPROM_I2C, (uint8_t)(offset >> 8)); // 发送地址高字节
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) { // 等待发送完毕
        return STATUS_ERROR;                 // 失败返回错误
    }                                        // 成功继续
    I2C_SendData(EEPROM_I2C, (uint8_t)(offset & 0xFFU)); // 发送地址低字节
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) { // 等待发送完毕
        return STATUS_ERROR;                 // 失败返回错误
    }                                        // 成功继续
    I2C_SendData(EEPROM_I2C, data);          // 发送实际数据
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) { // 等待发送完毕
        return STATUS_ERROR;                 // 失败返回错误
    }                                        // 成功继续
    I2C_GenerateSTOP(EEPROM_I2C, ENABLE);    // 发送停止信号
    Delay_ms(EEPROM_WRITE_DELAY_MS);          // 等待写入完成
	if (EEPROM_WaitReady(dev) != STATUS_OK) { // 轮询器件ACK确保就绪
        return STATUS_ERROR;                 // 设备超时未准备好
    }
    return STATUS_OK;                        // 返回成功
}                                            // 结束EEPROM_WriteByte函数

uint8_t EEPROM_ReadByte(uint32_t addr, uint8_t *data) { // 定义EEPROM读函数
    if ((addr >= EEPROM_SIZE) || (data == NULL)) { // 检查参数合法性
        return STATUS_ERROR;                 // 参数非法返回错误
    }                                        // 参数有效则继续
    uint16_t offset = (uint16_t)(addr & 0xFFFFU); // 计算页内偏移
    uint8_t dev = GetDeviceAddress(addr);    // 获取器件地址
    I2C_GenerateSTART(EEPROM_I2C, ENABLE);   // 发送第一次起始
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) { // 等待成功
        return STATUS_ERROR;                 // 失败返回错误
    }                                        // 成功继续
    I2C_Send7bitAddress(EEPROM_I2C, dev, I2C_Direction_Transmitter); // 发送写命令
    if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != STATUS_OK) { // 等待ACK
        return STATUS_ERROR;                 // 无应答返回错误
    }                                        // 成功继续
    I2C_SendData(EEPROM_I2C, (uint8_t)(offset >> 8)); // 发送地址高字节
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) { // 等待完成
        return STATUS_ERROR;                 // 失败返回错误
    }                                        // 成功继续
    I2C_SendData(EEPROM_I2C, (uint8_t)(offset & 0xFFU)); // 发送地址低字节
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != STATUS_OK) { // 等待完成
        return STATUS_ERROR;                 // 失败返回错误
    }                                        // 成功继续
    I2C_GenerateSTART(EEPROM_I2C, ENABLE);   // 发送重复起始
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != STATUS_OK) { // 等待完成
        return STATUS_ERROR;                 // 失败返回错误
    }                                        // 成功继续
    I2C_Send7bitAddress(EEPROM_I2C, dev, I2C_Direction_Receiver); // 发送读命令
    if (I2C_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != STATUS_OK) { // 等待进入接收模式
        return STATUS_ERROR;                 // 失败返回错误
    }                                        // 成功继续
    I2C_AcknowledgeConfig(EEPROM_I2C, DISABLE); // 关闭ACK准备接收最后一个字节
    if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_RECEIVED) != STATUS_OK) { // 等待数据到达
        I2C_AcknowledgeConfig(EEPROM_I2C, ENABLE); // 恢复ACK防止影响后续通信
        return STATUS_ERROR;                 // 返回错误
    }                                        // 成功继续
    *data = I2C_ReceiveData(EEPROM_I2C);     // 读取数据并写入指针
    I2C_GenerateSTOP(EEPROM_I2C, ENABLE);    // 发送停止信号
    I2C_AcknowledgeConfig(EEPROM_I2C, ENABLE); // 恢复ACK功能
    return STATUS_OK;                        // 返回成功
}                                            // 结束EEPROM_ReadByte函数

void EEPROM_RunSingleAddressDemo(void) {     // 单地址读写演示函数
    USART_SendString("\r\n[INFO] Start write address "); // 打印提示
    USART_SendHex(TARGET_EEPROM_ADDRESS, 6); // 打印目标地址
    USART_SendString(" data ");              // 打印说明
    USART_SendHex(TARGET_EEPROM_VALUE, 2);   // 打印目标数据
    if (EEPROM_WriteByte(TARGET_EEPROM_ADDRESS, TARGET_EEPROM_VALUE) == STATUS_OK) { // 调用写函数
        USART_SendString(" -> write success\r\n"); // 打印成功信息
        uint8_t verify = 0U;                 // 创建变量保存校验值
        if (EEPROM_ReadByte(TARGET_EEPROM_ADDRESS, &verify) == STATUS_OK) { // 读取同一地址
        USART_SendString("[INFO] Verify after write: "); // 打印提示
            USART_SendHex(verify, 2);        // 打印校验数据
            if (verify == TARGET_EEPROM_VALUE) { // 比对读写数据
                USART_SendString(" -> verify ok\r\n"); // 数据一致的提示
            } else {                         // 数据不一致处理
                USART_SendString(" -> verify failed\r\n"); // 打印失败信息
            }                                 // 结束比较
        } else {                             // 读取失败分支
            USART_SendString("[ERROR] verify read failed\r\n"); // 打印错误
        }                                    // 结束读取判断
    } else {                                 // 写失败分支
        USART_SendString(" -> write failed\r\n"); // 打印写入失败信息
        return;                               // 终止演示流程
    }                                        // 结束写入判断
    uint8_t read_value = 0U;                 // 创建变量保存独立读值
    USART_SendString("[INFO] Standalone read address "); // 打印独立读提示
    USART_SendHex(TARGET_EEPROM_ADDRESS, 6); // 打印读地址
    USART_SendString(" -> ");                // 格式化输出
    if (EEPROM_ReadByte(TARGET_EEPROM_ADDRESS, &read_value) == STATUS_OK) { // 调用读函数
        USART_SendString("read success value ");    // 打印成功信息
        USART_SendHex(read_value, 2);        // 打印读取的数据
        USART_SendString("\r\n");            // 换行
    } else {                                 // 读失败分支
        USART_SendString("read failed\r\n");     // 打印错误
    }                                        // 结束读判断
}                                            // 结束EEPROM_RunSingleAddressDemo函数

void EEPROM_LifeTestToggle(void) {           // 4 字节寿命测试启停控制
    if (!life_test_running) {
        LifeTestStart();
    }
}

void EEPROM_LifeTestStep(void) {             // 寿命测试步进执行
    if (life_test_running) {
        LifeTestStep();                      // 执行寿命测试步进
    }
}

void EEPROM_PageTestToggle(void) {           // 整页寿命测试启停控制
    if (!page_test_running) {
    PageTestStart(); 						 //开始整页寿命测试
    }
}

void EEPROM_PageTestStep(void) {             // 整页寿命测试步进执行
    if (page_test_running) {
        PageTestStep();                      // 执行整页寿命测试步进
    }
}

static void LifeTestStart(void) {            // 定义寿命测试启动函数
    life_test_running = 1U;                  // 标记状态
    life_test_counter = 0U;                  // 清零计数
    life_test_phase = 0U;                    // 默认从AA开始
    life_test_offset = 0U;                   // 默认从第1字节开始
    USART_SendString("\r\n[4byte] life test start addr "); // 打印提示
    USART_SendHex(LIFE_TEST_ADDRESS_START, 6); // 输出起始地址
    USART_SendString(" len ");               // 说明长度
    USART_SendHex(LIFE_TEST_LENGTH, 2);      // 输出4字节长度
    USART_SendString(" pattern AA/55\r\n");  // 说明交替数据
    USART_SendString("\r\n");                // 换行
}                                            // 结束LifeTestStart函数

static void LifeTestStop(const char *reason) { // 定义寿命测试停止函数
    life_test_running = 0U;                  // 清除运行标志
    USART_SendString("[4byte] life test stop reason: "); // 打印原因
    USART_SendString(reason);                // 输出传入原因
    USART_SendString(" total writes ");      // 输出次数
    USART_SendDec(life_test_counter);        // 以十进制输出计数
    USART_SendString("\r\n");                // 换行
}                                            // 结束LifeTestStop函数

static void LifeTestStep(void) {             // 定义寿命测试执行函数
    uint32_t addr = LIFE_TEST_ADDRESS_START + life_test_offset; // 当前字节实际地址
    uint8_t target = (life_test_phase == 0U) ? LIFE_DATA_AA : LIFE_DATA_55; // 选择写入数据
    if (EEPROM_WriteByte(addr, target) != STATUS_OK) { // 写失败
         //门门加1
		USART_SendString("Write error @ 0x");
        USART_SendHex(addr, 6);  // 输出故障地址
        USART_SendString(" data 0x");
        USART_SendHex(target, 2);  // 输出目标数据
        USART_SendString("\r\n");
		//门门加2
		LifeTestStop("write error");         // 停止并提示
        return;
    }
    uint8_t readback = 0U;                   // 保存读取值
    if (EEPROM_ReadByte(addr, &readback) != STATUS_OK) { // 读失败
        //门门加1
		USART_SendString("Read error @ 0x");
        USART_SendHex(addr, 6);  // 输出故障地址
        USART_SendString("\r\n");
		//门门加2
		LifeTestStop("read error");          // 停止并提示
        return;
    }
    if (readback != target) {                // 数据不一致
        uint32_t fail_round = life_test_counter + 1U;
         //门门加1
		USART_SendString("Data mismatch @ 0x");
        USART_SendHex(addr, 6);  // 输出故障地址
        USART_SendString(" Expected:0x");
        USART_SendHex(target, 2);  // 输出期望值
        USART_SendString(" Actual:0x");
        USART_SendHex(readback, 2);  // 输出实际值
		 //门门加2
		USART_SendString("test number "); // 测试次数：第
        USART_SendDec(fail_round);
        USART_SendString(" NO "); // 次 NO
        USART_SendString("\r\n");
        LifeTestStop("data mismatch");       // 停止测试
        return;
    }
    life_test_offset++;                      // 移动至下一个字节
    if (life_test_offset >= LIFE_TEST_LENGTH) { // 如果本轮4字节写完
        life_test_offset = 0U;               // 重新从起始地址开始
        life_test_phase ^= 0x01U;            // 切换写入数据AA/55
        life_test_counter++;                 // 轮次+1
        if ((life_test_counter % 100U) == 0U) {
            USART_SendString("test number "); // 测试次数：第
            USART_SendDec(life_test_counter);
            USART_SendString(" OK\r\n"); // 次 OK
        }
    }
}                                            // 结束LifeTestStep函数

static void PageTestStart(void) {            // 定义整页寿命测试启动函数
    page_test_running = 1U;
    page_test_counter = 0U;
    page_test_pattern = 0U;                  // 从PAGE_DATA_FIRST开始
    page_stop_request = 0U;                  // 清除停止请求
    USART_SendString("\r\n[page] page test start addr ");
    USART_SendHex(PAGE_TEST_ADDRESS, 6);
    USART_SendString(" len ");
    USART_SendHex(PAGE_TEST_LENGTH, 4);
    USART_SendString(" pattern ");
    USART_SendHex(PAGE_DATA_FIRST, 2);
    USART_SendString("/");
    USART_SendHex(PAGE_DATA_SECOND, 2);
    USART_SendString("\r\n");
}                                            // 结束PageTestStart函数

static void PageTestStop(const char *reason) { // 定义整页寿命测试停止函数
    page_test_running = 0U;
    page_stop_request = 0U;
    USART_SendString("[page] page test stop reason: ");
    USART_SendString(reason);
    USART_SendString(" total cycles ");
    USART_SendDec(page_test_counter);
    USART_SendString("\r\n");
}                                            // 结束PageTestStop函数

static void PageTestStep(void) {             // 定义整页寿命测试执行函数
    uint8_t pattern = (page_test_pattern == 0U) ? PAGE_DATA_FIRST : PAGE_DATA_SECOND;
    uint32_t start_addr = PAGE_TEST_ADDRESS;
    for (uint16_t offset = 0U; offset < PAGE_TEST_LENGTH; offset++) {
        if (page_stop_request) {
            PageTestStop("manual stop");
            return;
        }
        if (EEPROM_WriteByte(start_addr + offset, pattern) != STATUS_OK) {
            PageTestStop("write error");
            return;
        }
    }
    for (uint16_t offset = 0U; offset < PAGE_TEST_LENGTH; offset++) {
        if (page_stop_request) {
            PageTestStop("manual stop");
            return;
        }
        uint8_t readback = 0U;
        uint32_t addr = start_addr + offset;
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
    if (page_stop_request) {                 // 检查是否请求停止
        PageTestStop("manual stop");
        return;
    }
    page_test_counter++;
    page_test_pattern ^= 0x01U;              // 交替数据
    USART_SendString("[page] Count ");
    USART_SendDec(page_test_counter);
    USART_SendString(" OK\r\n");
}                                            // 结束PageTestStep函数
