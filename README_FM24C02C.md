## FM24C02C 配置说明

本配置适配 **FM24C02C** (上海复旦微电子) - 2K-bit (256字节) FRAM

### 芯片规格

- **型号**: FM24C02C
- **容量**: 256 字节 (2K-bit)
- **页大小**: 8 字节
- **I2C 地址**: 0xA0 (标准 24C02 地址)
- **工作电压**: 1.7V - 5.5V
- **写周期时间**: 即时写入 (FRAM 特性)
- **擦写寿命**: 无限次 (10^14 次读写周期)

### 使用方法

1. **配置切换**  
   将 `config_FM24C02C.h` 复制为 `config.h`：
   ```powershell
   copy USER\APP\config_FM24C02C.h USER\APP\config.h
   ```

2. **硬件连接**  
   - PB8 (I2C1_SCL) → FM24C02C SCL
   - PB9 (I2C1_SDA) → FM24C02C SDA
   - 确保正确连接 VCC、GND
   - WP 引脚接 GND（解除写保护）

3. **编译下载**  
   在 Keil MDK 中打开工程并编译下载到 STM32F407

### 配置参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `EEPROM_SIZE` | 256 字节 | 总容量 |
| `EEPROM_PAGE_SIZE` | 8 字节 | 单页大小 |
| `TARGET_EEPROM_ADDRESS` | 0x0020 | 单地址测试地址 |
| `LIFE_TEST_ADDRESS_START` | 0x0066 | 4字节寿命测试起始地址 |
| `PAGE_TEST_ADDRESS` | 0x0080 | 整页寿命测试起始地址 |
| `PAGE_TEST_LENGTH` | 8 字节 | 整页测试长度 |
| `EEPROM_WRITE_DELAY_MS` | 5 ms | 写入延迟（兼容性） |

### FRAM 与 EEPROM 的区别

| 特性 | FRAM (FM24C02C) | EEPROM (M24M02/M24512E) |
|------|-----------------|------------------------|
| 写入速度 | 即时 (无需等待) | 需要 5-10ms 写周期 |
| 擦写寿命 | 无限 (>10^14) | 100万-400万次 |
| 写入延迟 | 无需 ACK 轮询 | 需要 ACK 轮询确认 |
| 功耗 | 低 | 中等 |

### 重要提示

> **内存容量限制**  
> FM24C02C 仅有 256 字节容量（地址范围 0x00-0xFF），远小于 M24M02 的 256KB。所有测试地址已调整到此范围内。

> **即时写入特性**  
> FRAM 的写入是即时完成的，不需要等待写周期。代码中保留了 5ms 延迟以确保与 EEPROM 代码的兼容性，实际可以缩短或移除。

> **无限擦写寿命**  
> FRAM 的擦写寿命远超 EEPROM，非常适合频繁写入的应用场景。寿命测试将不会因擦写次数导致失效。

> **小页面大小**  
> FM24C02C 的页大小为 8 字节，整页寿命测试每轮只写入 8 字节数据，而不是 M24M02 的 256 字节。

### 预期测试输出

#### 单地址读写演示 (WK_UP)
```
[INFO] Start write address 000020 data 6A -> write success
[INFO] Verify after write: 6A -> verify ok
[INFO] Standalone read address 000020 -> read success value 6A
```

#### 4字节寿命测试 (KEY0)
```
[4byte] life test start addr 000066 len 01 pattern AA/55
test number 100 OK
test number 200 OK
...
```

#### 整页寿命测试 (KEY1)
```
[page] page test start addr 000080 len 0008 pattern 55/AA
[page] Count 1 OK
[page] Count 2 OK
...
```

**注意**: 由于 FRAM 的即时写入特性，测试速度会明显快于 EEPROM 版本。
