/*
 * uart.h
 *
 *  Created on: 2025年11月20日
 *  Author: 
 *  功能 ：串口函数
 */
 #ifndef UART_H
 #define UART_H

 #include "stm32f4xx.h"

 void USART1_Init(void);                            //串口初始化
 void USART_SendChar(char c);                       //串口打印字符
 void USART_SendString(const char *str);            //串口打印字符串
 void USART_SendHex(uint32_t value, uint8_t digits);//串口打印16进制数据
 void USART_SendDec(uint32_t value);                //串口打印10进制数据

 #endif /* UART_H */
 
 
