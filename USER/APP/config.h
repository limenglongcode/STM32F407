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
#define EEPROM_SIZE              (256U * 1024U)
#define EEPROM_PAGE_SIZE         256U
#define EEPROM_WRITE_DELAY_MS    10U
#define EEPROM_ADDRESS_BYTES     2U
#define TARGET_EEPROM_ADDRESS    0x000200UL
#define LIFE_TEST_ADDRESS_START  0x000066UL
#define PAGE_TEST_ADDRESS        0x000800UL
#define CHIP_NAME                "M24M02"
#endif

#ifdef USE_M24512E
#define EEPROM_SIZE              (64U * 1024U)
#define EEPROM_PAGE_SIZE         128U
#define EEPROM_WRITE_DELAY_MS    10U
#define EEPROM_ADDRESS_BYTES     2U
#define TARGET_EEPROM_ADDRESS    0x000200UL
#define LIFE_TEST_ADDRESS_START  0x000066UL
#define PAGE_TEST_ADDRESS        0x000800UL
#define CHIP_NAME                "M24512E"
#endif

#ifdef USE_FM24C02C
#define EEPROM_SIZE              256U
#define EEPROM_PAGE_SIZE         8U
#define EEPROM_WRITE_DELAY_MS    0U
#define EEPROM_ADDRESS_BYTES     1U
#define TARGET_EEPROM_ADDRESS    0x000020UL
#define LIFE_TEST_ADDRESS_START  0x000066UL
#define PAGE_TEST_ADDRESS        0x000080UL
#define CHIP_NAME                "FM24C02C"
#endif

#ifdef USE_FM24CL64B
#define EEPROM_SIZE              (8U * 1024U)
#define EEPROM_PAGE_SIZE         32U
#define EEPROM_WRITE_DELAY_MS    0U
#define EEPROM_ADDRESS_BYTES     2U
#define TARGET_EEPROM_ADDRESS    0x000200UL
#define LIFE_TEST_ADDRESS_START  0x000000UL
#define PAGE_TEST_ADDRESS        0x000800UL
#define CHIP_NAME                "FM24CL64B-GTR"
#endif

#ifdef USE_FM24NM01AI3
/*
 * FM24NM01AI3:
 * 1Mbit EEPROM = 128KB
 * 页大小 256 字节
 * A2/A1/A0 全部接地，器件基地址为 0xA0
 * 额外高地址位通过器件地址页位选择
 */
#define EEPROM_SIZE              (128U * 1024U)
#define EEPROM_PAGE_SIZE         256U
#define EEPROM_WRITE_DELAY_MS    5U
#define EEPROM_ADDRESS_BYTES     2U
#define TARGET_EEPROM_ADDRESS    0x000200UL
#define LIFE_TEST_ADDRESS_START  0x000000UL
#define PAGE_TEST_ADDRESS        0x000100UL
#define CHIP_NAME                "FM24NM01AI3"
#endif

#define EEPROM_TIMEOUT           100000U

#define STATUS_OK                0U
#define STATUS_ERROR             1U

#define TARGET_EEPROM_VALUE      0x6AU

/*
 * KEY0 字节寿命测试参数
 * FM24NM01AI3 使用整轮模式:
 * 先对长度范围写入并校验第一组数据，再写入并校验第二组数据。
 */
#define LIFE_TEST_LENGTH         1U
#define LIFE_DATA_FIRST          0xAAU
#define LIFE_DATA_SECOND         0x55U

/*
 * 兼容原项目旧宏命名
 */
#define LIFE_DATA_AA             LIFE_DATA_FIRST
#define LIFE_DATA_55             LIFE_DATA_SECOND

/*
 * KEY1 整页寿命测试参数
 */
#define PAGE_TEST_LENGTH         EEPROM_PAGE_SIZE
#define PAGE_DATA_FIRST          0xAAU
#define PAGE_DATA_SECOND         0x55U

#endif
