/*
 * main.c
 *
 * 主函数
 */
#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

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
    SystemClock_Config();
    SysTick_Init();
    KEY_Init();
    USART1_Init();
    I2C_EEPROM_Init();

    USART_SendString("\r\n==== ");
    USART_SendString(CHIP_NAME);
    USART_SendString(" test ====\r\n");
    USART_SendString("WK_UP: single address R/W demo\r\n");
#ifdef USE_FM24NM01AI3
    USART_SendString("KEY0: byte life test start\r\n");
#else
    USART_SendString("KEY0: 4-byte life test start\r\n");
#endif
    USART_SendString("KEY1: page life test start\r\n");

    while (1)
    {
        if (KEY_Scan()) {
            EEPROM_RunSingleAddressDemo();
        }

        if (KEY0_Scan()) {
            EEPROM_LifeTestToggle();
        }

        if (KEY1_Scan()) {
            EEPROM_PageTestToggle();
        }

        EEPROM_LifeTestStep();
        EEPROM_PageTestStep();

        Delay_ms(1);
    }
}
