# PlatformIO 配置说明

## 项目概述
这是一个基于STM32F103C8T6的项目，使用HAL库开发，包含OLED显示屏驱动（I2C接口）。

## 安装 PlatformIO

### 方法1: VSCode扩展安装（推荐）
1. 打开VSCode
2. 进入扩展商店（Ctrl+Shift+X）
3. 搜索 "PlatformIO IDE"
4. 点击安装

### 方法2: 命令行安装
```bash
# 使用pip安装
pip install platformio

# 或使用Python3
python -m pip install platformio
```

## 项目结构

```
project1/
├── platformio.ini          # PlatformIO配置文件
├── src/                    # 主源文件目录（PlatformIO标准）
│   ├── main.c             # 主程序
│   ├── syscalls.c         # 系统调用
│   └── sysmem.c           # 内存管理
├── Core/                   # STM32CubeMX生成的核心文件
│   ├── Inc/               # 头文件
│   └── Src/               # 源文件
├── Hardware/              # 硬件驱动
│   ├── OLED.c            # OLED驱动实现
│   └── OLED.h            # OLED驱动头文件
├── Drivers/              # STM32 HAL驱动库
│   ├── STM32F1xx_HAL_Driver/
│   └── CMSIS/
├── Startup/              # 启动文件
│   └── startup_stm32f103c8tx.s
└── STM32F103C8TX_FLASH.ld  # 链接脚本
```

## 硬件配置

- **MCU**: STM32F103C8T6
- **时钟**: HSI 8MHz（内部RC振荡器）
- **LED**: PC13（板载LED）
- **I2C1**: 
  - SCL: PB6
  - SDA: PB7
- **OLED**: SSD1306，I2C地址0x78

## PlatformIO 使用命令

### 编译项目
```bash
# 在项目根目录执行
pio run
```

### 上传程序到开发板
```bash
# 使用ST-Link上传
pio run --target upload
```

### 清理编译文件
```bash
pio run --target clean
```

### 串口监视器
```bash
pio device monitor
```

### 编译并上传
```bash
pio run -t upload
```

## 调试配置

项目已配置ST-Link作为调试工具。在VSCode中：
1. 点击左侧的PlatformIO图标
2. 选择 "Debug" > "Start Debugging"
3. 或按F5开始调试

## 常见问题

### 1. 找不到ST-Link
- 确保ST-Link驱动已安装
- 检查USB连接
- Windows: 安装 [ST-Link驱动](https://www.st.com/en/development-tools/stsw-link009.html)

### 2. 编译错误
- 检查所有源文件路径是否正确
- 确保HAL库配置正确

### 3. 上传失败
```bash
# 尝试手动擦除芯片
pio run -t erase

# 然后重新上传
pio run -t upload
```

## 功能说明

当前程序功能：
1. 初始化HAL库和系统时钟（HSI 8MHz）
2. 初始化I2C1外设（PB6-SCL, PB7-SDA）
3. 初始化OLED显示屏（SSD1306）
4. 在OLED上显示 "Hello World"
5. PC13 LED每秒闪烁一次

## 下一步

- 使用 `pio run` 编译项目
- 使用 `pio run -t upload` 上传到开发板
- 修改 `src/main.c` 来添加你的功能
