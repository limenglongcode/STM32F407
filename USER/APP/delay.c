/*
 * delay.c
 *
 *  Created on: 2025年11月20日
 *  Author: 
 *  功能 ：延时函数
 */
#include "delay.h"

//系统定时器的初始化
void  SysTick_Init(void)
{
	//把168MHZ经过8分频 得到21MHZ  1000000us振荡21000000次  ---  1us振荡21次
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
}

//延时微秒  不能超过798900us
void Delay_us(uint16_t nus)
{
	SysTick->CTRL &= ~(1<<0); 			// 关闭定时器
	SysTick->LOAD = (21*nus) - 1; 		// 设置重装载值 value-1
	SysTick->VAL  = 0; 					// 清空当前计数值
	SysTick->CTRL |= (1<<0); 			// 开启定时器  开始倒数
	while ( (SysTick->CTRL & 0x00010000)== 0 );// 等待倒数完成
	SysTick->CTRL = 0;					// 关闭定时器
	SysTick->VAL  = 0; 					// 清空当前计数值
}

//延时毫秒  不能超过798.9ms
void Delay_ms(uint16_t nms)
{
	SysTick->CTRL &= ~(1<<0); 			// 关闭定时器
	SysTick->LOAD = (21*1000*nms) - 1; 	// 设置重装载值 value-1
	SysTick->VAL  = 0; 					// 清空当前计数值
	SysTick->CTRL |= (1<<0); 			// 开启定时器  开始倒数
	while ( (SysTick->CTRL & 0x00010000)== 0 );// 等待倒数完成
	SysTick->CTRL = 0;					// 关闭定时器
	SysTick->VAL  = 0; 					// 清空当前计数值
}
