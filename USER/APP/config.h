/*
 * config.h
 *
 * Chip selection and common test configuration.
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_

/*
 * Select one chip here, or define it in Keil Preprocessor Symbols.
 */
//#define USE_M24M02
//#define USE_M24512E
//#define USE_FM24C02C
#define USE_FM24CL64B

#if !defined(USE_M24M02) && !defined(USE_M24512E) && !defined(USE_FM24C02C) && !defined(USE_FM24CL64B)
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

#define EEPROM_TIMEOUT           100000U  //ee³¬Ê±Ê±¼ä

#define STATUS_OK                0U
#define STATUS_ERROR             1U

#define TARGET_EEPROM_VALUE      0x6AU

/*
 * KEY0 byte life test settings.
 * One cycle = write/verify LIFE_DATA_FIRST for LIFE_TEST_LENGTH bytes,
 * then write/verify LIFE_DATA_SECOND for the same range.
 */
#define LIFE_TEST_LENGTH         100U
#define LIFE_DATA_FIRST          0xAAU
#define LIFE_DATA_SECOND         0x55U

#define PAGE_TEST_LENGTH         EEPROM_PAGE_SIZE
#define PAGE_DATA_FIRST          0x55U
#define PAGE_DATA_SECOND         0xAAU

#endif
