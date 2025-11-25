/*
 * uart.c
 *
 *  Created on: 2025年11月20日
 *  Author: 
 *  功能 ：串口函数
 */
 #include "uart.h"
 #include "stm32f4xx_gpio.h"
 #include "stm32f4xx_rcc.h"
 #include "stm32f4xx_usart.h"

 #define USART_PORT               USART1      // 使用USART1做串口输出
 #define USART_GPIO_PORT          GPIOA       // USART1引脚位于GPIOA
 #define USART_GPIO_CLOCK         RCC_AHB1Periph_GPIOA // USART引脚所需时钟
 #define USART_CLOCK              RCC_APB2Periph_USART1 // USART1位于APB2总线
 #define USART_TX_PIN             GPIO_Pin_9  // USART1 TX引脚
 #define USART_RX_PIN             GPIO_Pin_10 // USART1 RX引脚
 #define USART_BAUDRATE           115200U     // 串口波特率115200

 void USART1_Init(void) {                     // 定义串口初始化函数
    RCC_APB2PeriphClockCmd(USART_CLOCK, ENABLE); // 开启USART1时钟
    RCC_AHB1PeriphClockCmd(USART_GPIO_CLOCK, ENABLE); // 开启GPIOA时钟
    GPIO_InitTypeDef gpio;                   // 创建GPIO配置结构体
    gpio.GPIO_Pin = USART_TX_PIN | USART_RX_PIN; // 选择PA9和PA10
    gpio.GPIO_Mode = GPIO_Mode_AF;           // 设置为复用功能
    gpio.GPIO_OType = GPIO_OType_PP;         // 配置为推挽输出
    gpio.GPIO_Speed = GPIO_Speed_50MHz;      // 设置速度为50MHz
    gpio.GPIO_PuPd = GPIO_PuPd_UP;           // 使能上拉保证空闲高电平
    GPIO_Init(USART_GPIO_PORT, &gpio);       // 初始化GPIOA
    GPIO_PinAFConfig(USART_GPIO_PORT, GPIO_PinSource9, GPIO_AF_USART1); // 将PA9映射到USART1_TX
    GPIO_PinAFConfig(USART_GPIO_PORT, GPIO_PinSource10, GPIO_AF_USART1); // 将PA10映射到USART1_RX
    USART_InitTypeDef usart;                 // 创建USART配置结构体
    usart.USART_BaudRate = USART_BAUDRATE;   // 设置波特率
    usart.USART_WordLength = USART_WordLength_8b; // 设置数据位为8
    usart.USART_StopBits = USART_StopBits_1; // 设置停止位为1
    usart.USART_Parity = USART_Parity_No;    // 不启用校验位
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 不使用硬件流控
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx; // 使能收发
    USART_Init(USART_PORT, &usart);          // 初始化USART1
    USART_Cmd(USART_PORT, ENABLE);           // 使能USART1外设
 }                                            // 结束USART1_Init函数

 void USART_SendChar(char c) {                // 定义串口发送单字符函数
    while (USART_GetFlagStatus(USART_PORT, USART_FLAG_TXE) == RESET); // 等待发送寄存器为空
    USART_SendData(USART_PORT, (uint16_t)c); // 将数据写入发送寄存器
 }                                            // 结束USART_SendChar函数

 void USART_SendString(const char *str) {     // 定义串口发送字符串函数
    while (str && *str) {                    // 确保指针有效并遍历至字符串结束
        USART_SendChar(*str++);              // 逐字节发送
    }                                        // 字符串发送完成退出
 }                                            // 结束USART_SendString函数

 void USART_SendHex(uint32_t value, uint8_t digits) { // 定义十六进制打印函数
    static const char table[] = "0123456789ABCDEF"; // 创建查找表
    char buffer[10];                         // 创建本地缓冲
    for (int8_t i = (int8_t)digits - 1; i >= 0; i--) { // 从最低位开始填充
        buffer[i] = table[value & 0x0FU];    // 提取当前4位
        value >>= 4;                         // 右移准备下一位
    }                                        // 填充完成
    buffer[digits] = '\0';                   // 添加字符串结束符
    USART_SendString("0x");                  // 输出前缀0x
    USART_SendString(buffer);                // 输出转换结果
 }                                            // 结束USART_SendHex函数

 void USART_SendDec(uint32_t value) {         // 定义十进制打印函数
    char buffer[11];                         // 最多打印10位十进制
    int8_t index = 0;                        // 当前缓冲索引
    if (value == 0U) {                       // 处理0的特殊情况
        USART_SendChar('0');                 // 直接发送字符0
        return;
    }
    while (value > 0U && index < 10) {       // 循环提取各位
        buffer[index++] = (char)('0' + (value % 10U)); // 保存当前最低位
        value /= 10U;                        // 去掉最低位
    }
    while (index-- > 0) {                    // 逆序输出
        USART_SendChar(buffer[index]);       // 发送字符
    }
 }                                            // 结束USART_SendDec函数
