/*
 * iic.h
 *
 *  Created on: 2025年11月20日
 *  Author: 
 *  功能 ：I2C读写函数
 */
 #ifndef IIC_H
 #define IIC_H

 #include "stm32f4xx.h"
 #include <stdint.h>

 // 硬件 I2C1 + M24M02 EEPROM 基本读写接口
 void I2C_EEPROM_Init(void);
 uint8_t EEPROM_WriteByte(uint32_t addr, uint8_t data);
 uint8_t EEPROM_ReadByte(uint32_t addr, uint8_t *data);

 // 测试控制接口：由 main.c 根据按键调用
 void EEPROM_RunSingleAddressDemo(void);   // 单地址演示（WK_UP）
 void EEPROM_LifeTestToggle(void);         // 4 字节寿命测试启停（KEY0）
 void EEPROM_LifeTestStep(void);           // 寿命测试步进调用
 void EEPROM_PageTestToggle(void);         // 整页寿命测试启停（KEY1）
 void EEPROM_PageTestStep(void);           // 整页寿命测试步进调用

 #endif /* IIC_H */
