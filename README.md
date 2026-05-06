# Smart Pill Box 智能药盒

基于 STM32F103C8T6 的智能药盒系统，集成多种传感器、语音提醒、WiFi 远程控制和 Android APP 配套应用。

## 功能特性

### 核心功能
- **定时服药提醒** - 支持最多 3 组定时提醒（默认 8:00、12:00、18:00），语音播报"该吃药啦"
- **智能药盒控制** - 舵机驱动药盒开关，支持按键和远程控制
- **药品余量监测** - HX711 称重传感器实时监测药品剩余量，低于 10% 时语音提醒
- **环境监测** - DHT11 温湿度传感器 + 光照传感器，自动调节存储环境

### 自动控制
- **温度控制** - 高温自动开启风扇散热，低温自动开启加热器保温
- **光照控制** - 根据环境光线自动开关 LED 照明
- **阈值可调** - 支持通过 API 远程调整温度、光照控制阈值

### 远程控制
- **WiFi 连接** - ESP8266 模块提供无线网络连接
- **HTTP API** - 提供完整的 RESTful API 接口
- **实时状态** - 温度、湿度、药品余量、电池电量、光照强度等数据实时上报
- **远程操作** - 开关药盒、调节音量、设置提醒时间、同步 RTC 时间

### 人机交互
- **OLED 显示屏** - 128x64 点阵屏显示时间、温湿度、药品余量、电池电量、WiFi 状态
- **语音播报** - TW-TTS 语音合成模块，支持中文语音提醒
- **按键控制** - 3 个独立按键（开关药盒、音量加、音量减）
- **音量调节** - 0-100 级音量调节，支持按键和远程控制

### 电源管理
- **电池电量监测** - CW2015 电量计芯片实时监测电池电量和电压
- **RTC 实时时钟** - 低功耗时钟芯片，支持网络时间同步

## 硬件架构

### 主控芯片
- **STM32F103C8T6** - ARM Cortex-M3 内核，72MHz 主频

### 传感器模块
| 模块 | 功能 | 接口 |
|------|------|------|
| DHT11 | 温湿度检测 | GPIO (PA8) |
| HX711 | 重量检测 | GPIO (PA4/PA5) |
| CW2015 | 电池电量计 | I2C1 (PB6/PB7) |
| 光敏电阻 | 光照检测 | ADC1 (PA1) |

### 执行器模块
| 模块 | 功能 | 接口 |
|------|------|------|
| SG90 舵机 | 药盒开关 | PWM (PA6) |
| 风扇 | 散热 | GPIO (PA7) |
| 加热器 | 保温 | GPIO (PB0) |
| LED | 照明 | GPIO (PB1) |

### 通信模块
| 模块 | 功能 | 接口 |
|------|------|------|
| ESP8266 | WiFi 通信 | USART1 (PA9/PA10) |
| TW-TTS | 语音合成 | USART2 (PA2/PA3) |
| OLED | 显示 | I2C2 (PB10/PB11) |

### 人机交互
| 模块 | 功能 | 接口 |
|------|------|------|
| OLED 128x64 | 信息显示 | I2C2 |
| 按键 x3 | 手动控制 | GPIO (PB12/PB13/PB14) |

## 软件架构

### 项目结构
```
PILLBOX/
├── Core/
│   ├── Inc/           # 头文件
│   │   ├── main.h
│   │   ├── hardware_config.h    # 硬件配置
│   │   ├── wifi.h               # WiFi 模块驱动
│   │   ├── TW_TTS.h             # TTS 语音模块驱动
│   │   ├── servo.h              # 舵机驱动
│   │   ├── actuator.h           # 执行器控制
│   │   ├── dht11.h              # DHT11 温湿度驱动
│   │   ├── hx711.h              # HX711 称重驱动
│   │   ├── cw2015.h             # CW2015 电量计驱动
│   │   ├── oled.h               # OLED 显示驱动
│   │   ├── key.h                # 按键驱动
│   │   ├── rtc.h                # RTC 时钟驱动
│   │   └── light_sensor.h       # 光照传感器驱动
│   └── Src/           # 源文件
│       └── ...
├── Drivers/           # HAL 库和 CMSIS
├── MDK-ARM/           # Keil 工程文件
├── SmartPillBoxApp/   # Android APP 源码
└── PILLBOX.ioc        # STM32CubeMX 配置文件
```

### 主循环架构
```c
while (1)
{
    // 100ms 周期任务调度
    Key_Process();           // 按键处理
    DHT11_ReadData();        // 温湿度读取（2秒间隔）
    Actuator_Update();       // 执行器自动控制
    HX711_GetWeight();       // 重量读取
    CW2015_ReadData();       // 电量读取
    WiFi_UpdateStatus();     // 状态上报
    TTS_CheckReminders();    // 服药提醒检查
    WiFi_ProcessBuffer();    // WiFi 数据处理
    OLED_UpdateDisplay();    // 屏幕刷新
}
```

## API 接口

### 基础接口
| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/status` | GET | 获取设备完整状态 |
| `/api/open` | GET | 打开药盒 |
| `/api/close` | GET | 关闭药盒 |

### 音量控制
| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/volume/up` | GET | 音量增加 |
| `/api/volume/down` | GET | 音量减小 |
| `/api/volume?set=50` | GET | 设置音量 (0-100) |

### 提醒设置
| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/reminder?set=0&h=8&m=0` | GET | 设置第 0 组提醒时间为 8:00 |

### 阈值设置
| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/threshold?temp_high=30&temp_low=15&light_on=60&light_off=65` | GET | 设置控制阈值 |

### 时间同步
| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/settime?y=26&mo=4&d=19&h=12&m=0&s=0` | GET | 同步 RTC 时间 |

### 状态返回示例
```json
{
  "code": 0,
  "msg": "ok",
  "data": {
    "temp": 25,
    "hum": 60,
    "medicine": 85,
    "light": 45,
    "volume": 50,
    "battery": 80,
    "voltage": 3700,
    "led": 1,
    "fan": 0,
    "heater": 0,
    "box_open": 0,
    "rtc_synced": 1
  }
}
```

## Android APP

配套 Android 应用程序，提供以下功能：
- 设备状态实时监控
- 远程开关药盒
- 音量调节
- 服药提醒时间设置
- 环境阈值配置
- 时间同步

APP 源码位于 `SmartPillBoxApp/` 目录。

## 开发环境

### 硬件开发
- **IDE**: Keil MDK-ARM V5
- **配置工具**: STM32CubeMX V6.15.0
- **固件库**: STM32Cube FW_F1 V1.8.7
- **调试器**: ST-Link V2

### 软件开发
- **主控**: STM32F103C8T6
- **时钟**: HSE 8MHz，系统时钟 72MHz
- **编译器**: ARM Compiler V5/V6

### Android APP 开发
- **IDE**: Android Studio
- **语言**: Java/Kotlin
- **最低 SDK**: Android 8.0 (API 26)

## 配置说明

### WiFi 配置
在 `Core/Inc/hardware_config.h` 中修改：
```c
#define WIFI_SSID     "Your_SSID"
#define WIFI_PASSWORD "Your_Password"
```

### 提醒时间配置
在 `Core/Src/TW_TTS.c` 的 `TTS_Init()` 中修改：
```c
TTS_SetReminder(0, 8, 0);   // 早上 8:00
TTS_SetReminder(1, 12, 0);  // 中午 12:00
TTS_SetReminder(2, 18, 0);  // 晚上 18:00
```

### 控制阈值配置
在 `Core/Inc/hardware_config.h` 中修改：
```c
// 温度阈值
#define TEMP_HIGH_THRESHOLD  30    // 高温阈值（摄氏度）
#define TEMP_LOW_THRESHOLD   15    // 低温阈值（摄氏度）

// 光照阈值
#define LIGHT_ON_THRESHOLD   60    // 光线开启 LED 阈值（百分比）
#define LIGHT_OFF_THRESHOLD  65    // 光线关闭 LED 阈值（百分比）
```

## 引脚定义

详见 `Core/Inc/hardware_config.h`

| 功能 | 引脚 | 说明 |
|------|------|------|
| LED | PB1 | 照明 LED |
| 风扇 | PA7 | 散热风扇 |
| 加热器 | PB0 | 加热器 |
| 舵机 | PA6 | TIM3_CH1 PWM |
| 按键1 | PB12 | 开关药盒 |
| 按键2 | PB13 | 音量加 |
| 按键3 | PB14 | 音量减 |
| DHT11 | PA8 | 温湿度传感器 |
| HX711_DT | PA5 | 称重数据 |
| HX711_SCK | PA4 | 称重时钟 |
| 光照 | PA1 | ADC1 通道1 |
| OLED_SCL | PB10 | I2C2 时钟 |
| OLED_SDA | PB11 | I2C2 数据 |
| CW2015_SCL | PB6 | I2C1 时钟 |
| CW2015_SDA | PB7 | I2C1 数据 |
| ESP8266_TX | PA9 | USART1 TX |
| ESP8266_RX | PA10 | USART1 RX |
| TTS_TX | PA2 | USART2 TX |
| TTS_RX | PA3 | USART2 RX |

## 许可证

MIT License - 详见 [LICENSE](LICENSE) 文件

## 作者

智能药盒开发团队

---

**注意**: 本项目为开源学习项目，请勿用于商业用途。使用本设备管理药物时，请务必保持人工监督，确保用药安全。
