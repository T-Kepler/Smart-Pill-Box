/**
 * @file    hardware_config.h
 * @brief   智能药盒硬件配置文件
 * @details  该文件定义了所有硬件相关的配置参数，包括 GPIO 引脚、
 *           传感器参数、执行器参数、通信参数等
 * @author  智能药盒开发团队
 * @date    2026-04-19
 */

#ifndef __HARDWARE_CONFIG_H
#define __HARDWARE_CONFIG_H

#include "stm32f1xx_hal.h"

// ============= GPIO 引脚定义 =============

/** LED 指示灯引脚配置 */
#define LED_PIN          GPIO_PIN_1  // LED 引脚号
#define LED_PORT         GPIOB       // LED 端口

/** 风扇控制引脚配置 */
#define FAN_PIN          GPIO_PIN_7  // 风扇引脚号
#define FAN_PORT         GPIOA       // 风扇端口

/** 加热器控制引脚配置 */
#define HEATER_PIN       GPIO_PIN_0  // 加热器引脚号
#define HEATER_PORT      GPIOB       // 加热器端口

/** 舵机控制引脚配置 */
#define SERVO_PIN        GPIO_PIN_6  // 舵机引脚号
#define SERVO_PORT       GPIOA       // 舵机端口
#define SERVO_TIM_CHANNEL TIM_CHANNEL_1  // 舵机定时器通道

/** 按键引脚配置 */
#define KEY1_PIN         GPIO_PIN_12  // 按键1引脚号（开/关药盒）
#define KEY1_PORT        GPIOB       // 按键1端口
#define KEY2_PIN         GPIO_PIN_13  // 按键2引脚号（音量增加）
#define KEY2_PORT        GPIOB       // 按键2端口
#define KEY3_PIN         GPIO_PIN_14  // 按键3引脚号（音量减小）
#define KEY3_PORT        GPIOB       // 按键3端口

/** DHT11 温湿度传感器引脚配置 */
#define DHT11_PIN        GPIO_PIN_8  // DHT11引脚号
#define DHT11_PORT       GPIOA       // DHT11端口

/** HX711 称重传感器引脚配置 */
#define HX711_DT_PIN    GPIO_PIN_5  // HX711数据引脚号
#define HX711_DT_PORT   GPIOA       // HX711数据端口
#define HX711_SCK_PIN   GPIO_PIN_4  // HX711时钟引脚号
#define HX711_SCK_PORT  GPIOA       // HX711时钟端口

/** 光线传感器 ADC 通道配置 */
#define LIGHT_ADC_CHANNEL ADC_CHANNEL_1  // 光线传感器ADC通道

// ============= 音量控制配置 =============

/** 默认音量值 */
#define DEFAULT_VOLUME   50   // 默认音量（0-100）
#define VOLUME_STEP      5    // 音量调节步长
#define VOLUME_MAX       100   // 最大音量
#define VOLUME_MIN       0     // 最小音量

// ============= 自动控制阈值配置 =============

/** 光线传感器阈值配置 */
#define LIGHT_ON_THRESHOLD   60   // 光线开启LED阈值（百分比）
#define LIGHT_OFF_THRESHOLD  65   // 光线关闭LED阈值（百分比）

/** 温度控制阈值配置 */
#define TEMP_HIGH_THRESHOLD  30    // 高温阈值（摄氏度）
#define TEMP_HIGH_HYSTERESIS 28    // 高温滞回阈值（摄氏度）
#define TEMP_LOW_THRESHOLD   15    // 低温阈值（摄氏度）
#define TEMP_LOW_HYSTERESIS  17    // 低温滞回阈值（摄氏度）

// ============= 舵机配置 =============

/** 舵机 PWM 脉宽配置 */
#define SERVO_OPEN_US    2000  // 舵机打开位置脉宽（微秒）
#define SERVO_CLOSE_US   500   // 舵机关闭位置脉宽（微秒）
#define SERVO_AUTO_CLOSE_MS 2000  // 舵机自动关闭时间（毫秒）

// ============= 传感器采样间隔配置 =============

/** DHT11 温湿度传感器采样间隔 */
#define DHT11_READ_INTERVAL_MS 2000  // DHT11读取间隔（毫秒）

// ============= 按键配置 =============

/** 按键长按配置 */
#define KEY_LONG_PRESS_MS  500  // 按键长按检测时间（毫秒）
#define KEY_REPEAT_MS      200  // 按键重复检测时间（毫秒）

// ============= 主循环配置 =============

/** 主循环周期 */
#define MAIN_LOOP_PERIOD_MS 100  // 主循环周期（毫秒）

// ============= WiFi 配置 =============

/** WiFi 连接配置 */
#define WIFI_USE_STATIC_IP    0    // 是否使用静态IP（0-DHCP，1-静态IP）
#define WIFI_CONNECT_RETRY  3    // WiFi连接重试次数
#define WIFI_CONNECT_TIMEOUT_MS 15000  // WiFi连接超时时间（毫秒）

/** WiFi 网络配置 */
#define WIFI_SSID             "T"  // WiFi网络名称
#define WIFI_PASSWORD         "chen18455222142"  // WiFi密码


#endif
