/*
 * delay.h
 *
 *  Created on: 2025年11月20日
 *  Author: 
 *  功能 ：延时函数
 */
#ifndef  _DELAY_H_
#define  _DELAY_H_

#include "stm32f4xx.h"  

void  SysTick_Init(void);
void Delay_ms(uint16_t nms);
void Delay_us(uint16_t nus);
#endif
