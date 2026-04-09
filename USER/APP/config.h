/*
 * config.h
 *
 * 芯片选择与测试参数配置
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_

/*
 * 芯片选择
 * 如未在 Keil 预处理宏中定义，可在此处取消注释选择。
 */
//#define USE_M24M02
//#define USE_M24512E
//#define USE_FM24C02C
//#define USE_FM24CL64B
#define USE_FM24NM01AI3

#if !defined(USE_M24M02) && !defined(USE_M24512E) && !defined(USE_FM24C02C) && !defined(USE_FM24CL64B) && !defined(USE_FM24NM01AI3)
#define USE_M24M02
#endif

#ifdef USE_M24M02
#define EEPROM_SIZE              (256U * 1024U)     // M24M02总容量为256KB
#define EEPROM_PAGE_SIZE         256U               // M24M02页大小为256字节
#define EEPROM_WRITE_DELAY_MS    10U                // EEPROM写周期等待时间10ms
#define EEPROM_ADDRESS_BYTES     2U                 // 使用2字节内部地址
#define TARGET_EEPROM_ADDRESS    0x000200UL         // 单点读写演示地址
#define LIFE_TEST_ADDRESS_START  0x000066UL         // KEY0寿命测试起始地址
#define PAGE_TEST_ADDRESS        0x000800UL         // KEY1整页测试起始地址
#define CHIP_NAME                "M24M02"           // 当前芯片名称字符串
#endif

#ifdef USE_M24512E
#define EEPROM_SIZE              (64U * 1024U)      // M24512E总容量为64KB
#define EEPROM_PAGE_SIZE         128U               // M24512E页大小为128字节
#define EEPROM_WRITE_DELAY_MS    10U                // EEPROM写周期等待时间10ms
#define EEPROM_ADDRESS_BYTES     2U                 // 使用2字节内部地址
#define TARGET_EEPROM_ADDRESS    0x000200UL         // 单点读写演示地址
#define LIFE_TEST_ADDRESS_START  0x000066UL         // KEY0寿命测试起始地址
#define PAGE_TEST_ADDRESS        0x000800UL         // KEY1整页测试起始地址
#define CHIP_NAME                "M24512E"          // 当前芯片名称字符串
#endif

#ifdef USE_FM24C02C
#define EEPROM_SIZE              256U               // FM24C02C总容量为256字节
#define EEPROM_PAGE_SIZE         8U                 // FM24C02C页大小为8字节
#define EEPROM_WRITE_DELAY_MS    0U                 // FRAM无需写延时
#define EEPROM_ADDRESS_BYTES     1U                 // 使用1字节内部地址
#define TARGET_EEPROM_ADDRESS    0x000020UL         // 单点读写演示地址
#define LIFE_TEST_ADDRESS_START  0x000066UL         // KEY0寿命测试起始地址
#define PAGE_TEST_ADDRESS        0x000080UL         // KEY1整页测试起始地址
#define CHIP_NAME                "FM24C02C"         // 当前芯片名称字符串
#endif

#ifdef USE_FM24CL64B
#define EEPROM_SIZE              (8U * 1024U)       // FM24CL64B总容量为8KB
#define EEPROM_PAGE_SIZE         32U                // FM24CL64B页大小为32字节
#define EEPROM_WRITE_DELAY_MS    0U                 // FRAM无需写延时
#define EEPROM_ADDRESS_BYTES     2U                 // 使用2字节内部地址
#define TARGET_EEPROM_ADDRESS    0x000200UL         // 单点读写演示地址
#define LIFE_TEST_ADDRESS_START  0x000000UL         // KEY0寿命测试起始地址
#define PAGE_TEST_ADDRESS        0x000800UL         // KEY1整页测试起始地址
#define CHIP_NAME                "FM24CL64B-GTR"    // 当前芯片名称字符串
#endif

#ifdef USE_FM24NM01AI3
/*
 * FM24NM01AI3:
 * 1Mbit EEPROM = 128KB
 * 页大小 256 字节
 * A2/A1/A0 全部接地，器件基地址为 0xA0
 * 额外高地址位通过器件地址页位选择
 */
#define EEPROM_SIZE              (128U * 1024U)     // FM24NM01AI3总容量为128KB
#define EEPROM_PAGE_SIZE         256U               // FM24NM01AI3页大小为256字节
#define EEPROM_WRITE_DELAY_MS    5U                 // 兼容处理保留5ms写后等待
#define EEPROM_ADDRESS_BYTES     2U                 // 使用2字节内部地址
#define TARGET_EEPROM_ADDRESS    0x000200UL         // 单点读写演示地址
#define LIFE_TEST_ADDRESS_START  0x000000UL         // KEY0寿命测试起始地址
#define PAGE_TEST_ADDRESS        0x000100UL         // KEY1整页测试起始地址
#define CHIP_NAME                "FM24NM01AI3"      // 当前芯片名称字符串
#endif

#define EEPROM_TIMEOUT           100000U            // I2C事件等待超时计数

#define STATUS_OK                0U                 // 通用成功状态码
#define STATUS_ERROR             1U                 // 通用失败状态码

#define TARGET_EEPROM_VALUE      0x6AU              // 单点演示写入目标值

/*
 * KEY0 字节寿命测试参数
 * FM24NM01AI3 使用整轮模式:
 * 先对长度范围写入并校验第一组数据，再写入并校验第二组数据。
 */
#define LIFE_TEST_LENGTH         1U                 // KEY0每轮测试地址长度
#define LIFE_DATA_FIRST          0xAAU              // KEY0第一组测试数据
#define LIFE_DATA_SECOND         0x55U              // KEY0第二组测试数据

/*
 * 兼容原项目旧宏命名
 */
#define LIFE_DATA_AA             LIFE_DATA_FIRST    // 兼容旧宏名AA数据
#define LIFE_DATA_55             LIFE_DATA_SECOND   // 兼容旧宏名55数据

/*
 * KEY1 整页寿命测试参数
 */
#define PAGE_TEST_LENGTH         EEPROM_PAGE_SIZE   // KEY1测试长度为整页大小
#define PAGE_DATA_FIRST          0xAAU              // KEY1第一组页测试数据
#define PAGE_DATA_SECOND         0x55U              // KEY1第二组页测试数据

#endif
