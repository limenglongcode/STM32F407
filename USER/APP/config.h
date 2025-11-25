/*
 * config.h
 *
 *  Created on: 2025年11月20日
 *  Author: 
 *  功能 ：配置文件
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_

// EEPROM global configuration
#define EEPROM_SIZE              (256U * 1024U) // M24M02容量256KB
#define EEPROM_PAGE_SIZE         256U        // 页大小256字节
#define EEPROM_TIMEOUT           100000U     // I2C事件等待的超时计数
#define EEPROM_WRITE_DELAY_MS    10U          // 写入完成最小延迟1ms，配合ACK轮询确保可靠性

#define STATUS_OK                0U          // 状态码：正常
#define STATUS_ERROR             1U          // 状态码：错误

#define TARGET_EEPROM_ADDRESS    0x000200UL  // 目标操作地址
#define TARGET_EEPROM_VALUE      0x6AU       // 目标写入数据

#define LIFE_TEST_ADDRESS_START  0x000066UL
#define LIFE_TEST_LENGTH         1U          // 4字节
#define LIFE_DATA_AA             0xAAU       // 写入数据AA
#define LIFE_DATA_55             0x55U       // 写入数据55

#define PAGE_TEST_ADDRESS        0x000800UL // 页测试起始地址
#define PAGE_TEST_LENGTH         EEPROM_PAGE_SIZE // 测试长度为一页
#define PAGE_DATA_FIRST          0x55U      // 写入数据55
#define PAGE_DATA_SECOND         0xAAU      // 写入数据AA

#endif
