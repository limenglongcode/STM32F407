/*
 * main.c
 *
 *  Created on: 2025年11月20日
 *  Author: 
 *  功能 ：主函数
 */
/*C 库   */
#include "stm32f4xx.h"  
#include <stdio.h>
#include <string.h>

// 业务代码
#include "delay.h"
#include "key.h"
#include "iic.h"
#include "uart.h"
#include "config.h"

void SystemClock_Config(void)
{
    RCC_DeInit();
    RCC_HSEConfig(RCC_HSE_ON);
    while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET);
    RCC_PLLConfig(RCC_PLLSource_HSE, 8, 336, 2, 7);
    RCC_PLLCmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while (RCC_GetSYSCLKSource() != 0x08);
    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    RCC_PCLK1Config(RCC_HCLK_Div4);
    RCC_PCLK2Config(RCC_HCLK_Div2);
    SystemCoreClockUpdate();
}

int main(void)
{
    SystemClock_Config();   //系统时钟初始化
    SysTick_Init();         //系统定时器初始化
    KEY_Init();             //按键初始化
    USART1_Init();          //串口初始化
    I2C_EEPROM_Init();      //I2C初始化

    USART_SendString("\r\n==== ");
    USART_SendString(CHIP_NAME);
    USART_SendString(" test ====\r\n");
    USART_SendString("WK_UP: single address R/W demo\r\n");
    USART_SendString("KEY0: byte life test start\r\n");
    USART_SendString("KEY1: page life test start\r\n");

    while (1)
    {
        if (KEY_Scan())//复位按键
        {
            EEPROM_RunSingleAddressDemo();
        }

        if (KEY0_Scan())//4字节寿命测试
        {
            EEPROM_LifeTestToggle();
        }

        if (KEY1_Scan())//页写寿命测试
        {
            EEPROM_PageTestToggle();
        }

        EEPROM_LifeTestStep();
        EEPROM_PageTestStep();

        Delay_ms(1);
    }
}
