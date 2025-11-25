/*
 * key.c
 *
 *  Created on: 2025年11月20日
 *  Author: 
 *  功能 ：按键函数
 */
#include "key.h"
#include "delay.h"

void KEY_Init(void)
{
    RCC_AHB1PeriphClockCmd(KEY_WKUP_GPIO_CLOCK, ENABLE);
    RCC_AHB1PeriphClockCmd(KEY0_GPIO_CLOCK, ENABLE);
    RCC_AHB1PeriphClockCmd(KEY1_GPIO_CLOCK, ENABLE);

    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = KEY_WKUP_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_DOWN;      // WK_UP: 上拉按下为高，空闲保持低
    GPIO_Init(KEY_WKUP_GPIO_PORT, &gpio);

    gpio.GPIO_Pin = KEY0_PIN;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;        // KEY0: 上拉，按下为低
    GPIO_Init(KEY0_GPIO_PORT, &gpio);

    gpio.GPIO_Pin = KEY1_PIN;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;        // KEY1: 上拉，按下为低
    GPIO_Init(KEY1_GPIO_PORT, &gpio);
}

uint8_t KEY_Scan(void) //复位按键
{
    if (GPIO_ReadInputDataBit(KEY_WKUP_GPIO_PORT, KEY_WKUP_PIN) == Bit_SET) //按下为高
    {
        Delay_ms(10);
        if (GPIO_ReadInputDataBit(KEY_WKUP_GPIO_PORT, KEY_WKUP_PIN) == Bit_SET) //按下为高
        {
            while (GPIO_ReadInputDataBit(KEY_WKUP_GPIO_PORT, KEY_WKUP_PIN) == Bit_SET); //等待按键释放
            return 1U;
        }
    }
    return 0U;
}

uint8_t KEY0_Scan(void) //4字节寿命测试按键
{
    if (GPIO_ReadInputDataBit(KEY0_GPIO_PORT, KEY0_PIN) == Bit_RESET) //按下为低
    {
        Delay_ms(10);
        if (GPIO_ReadInputDataBit(KEY0_GPIO_PORT, KEY0_PIN) == Bit_RESET) //按下为低
        {
            while (GPIO_ReadInputDataBit(KEY0_GPIO_PORT, KEY0_PIN) == Bit_RESET); //等待按键释放
            return 1U;
        }
    }
    return 0U;
}

uint8_t KEY1_Scan(void) //页写寿命测试按键
{
    if (GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_PIN) == Bit_RESET) //按下为低
    {
        Delay_ms(10);
        if (GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_PIN) == Bit_RESET) //按下为低
        {
            while (GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_PIN) == Bit_RESET); //等待按键释放
            return 1U;
        }
    }
    return 0U;
}
