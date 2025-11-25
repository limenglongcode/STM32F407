/*
 * key.h
 *
 *  Created on: 2025年11月20日
 *  Author: 
 *  功能 ：按键函数
 */
#ifndef _KEY_H_
#define _KEY_H_

#include "stm32f4xx.h"
#include <stdint.h>

// WK_UP: PA0, KEY0: PE4, KEY1: PE3
#define KEY_WKUP_GPIO_PORT   GPIOA
#define KEY_WKUP_GPIO_CLOCK  RCC_AHB1Periph_GPIOA
#define KEY_WKUP_PIN         GPIO_Pin_0

#define KEY0_GPIO_PORT       GPIOE
#define KEY0_GPIO_CLOCK      RCC_AHB1Periph_GPIOE
#define KEY0_PIN             GPIO_Pin_4

#define KEY1_GPIO_PORT       GPIOE
#define KEY1_GPIO_CLOCK      RCC_AHB1Periph_GPIOE
#define KEY1_PIN             GPIO_Pin_3

void KEY_Init(void);
uint8_t KEY_Scan(void);   // WK_UP
uint8_t KEY0_Scan(void);  // KEY0
uint8_t KEY1_Scan(void);  // KEY1

#endif
