## M24512E 配置说明

本配置适配 **M24512E** (ST/Microchip) - 512K-bit (64KB) EEPROM

### 芯片规格

- **型号**: M24512E
- **容量**: 64 KB (512K-bit)
- **页大小**: 128 字节
- **I2C 地址**: 0xA0 (默认，可通过 CDA 寄存器配置)
- **工作电压**: 1.7V - 5.5V
- **写周期时间**: 最大 5ms
- **擦写寿命**: 4,000,000 次

### 使用方法

1. **配置切换**  
   将 `config_M24512E.h` 复制为 `config.h`：
   ```powershell
   copy USER\APP\config_M24512E.h USER\APP\config.h
   ```

2. **硬件连接**  
   - PB8 (I2C1_SCL) → M24512E SCL
   - PB9 (I2C1_SDA) → M24512E SDA
   - 确保正确连接 VCC、GND
   - E0/E1/E2 引脚接地（使用默认地址 0xA0）

3. **编译下载**  
   在 Keil MDK 中打开工程并编译下载到 STM32F407

### 配置参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `EEPROM_SIZE` | 65536 字节 | 64KB 总容量 |
| `EEPROM_PAGE_SIZE` | 128 字节 | 单页大小 |
| `TARGET_EEPROM_ADDRESS` | 0x000200 | 单地址测试地址 |
| `LIFE_TEST_ADDRESS_START` | 0x000066 | 4字节寿命测试起始地址 |
| `PAGE_TEST_ADDRESS` | 0x000800 | 整页寿命测试起始地址 |
| `PAGE_TEST_LENGTH` | 128 字节 | 整页测试长度 |

### 重要提示

> **可配置设备地址 (CDA)**  
> M24512E 支持通过软件配置 I2C 设备地址。本配置假定使用出厂默认地址 0xA0。如果您的芯片已配置为其他地址，需要修改 `iic.c` 中的 `EEPROM_BASE_ADDRESS` 宏定义。

> **地址范围**  
> M24512E 的 64KB 容量只需使用单个 I2C 设备地址 (0xA0)，与 M24M02 的 4 个地址 (0xA0/A2/A4/A6) 不同。代码中的 `GetDeviceAddress()` 函数会自动处理。

### 预期测试输出

#### 单地址读写演示 (WK_UP)
```
[INFO] Start write address 000200 data 6A -> write success
[INFO] Verify after write: 6A -> verify ok
[INFO] Standalone read address 000200 -> read success value 6A
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
[page] page test start addr 000800 len 0080 pattern 55/AA
[page] Count 1 OK
[page] Count 2 OK
...
```
