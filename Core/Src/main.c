/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "rtc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "hardware_config.h"
#include "oled.h"
#include "cw2015.h"
#include "wifi.h"
#include "TW_TTS.h"
#include "servo.h"
#include "actuator.h"
#include "dht11.h"
#include "hx711.h"
#include "light_sensor.h"
#include "key.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static DHT11_Data dht11_data = {0, 0};
static CW2015_Data cw2015_data = {0, 0, 0};
static uint32_t last_dht11_tick = 0;
static uint32_t last_loop_tick = 0;
static uint8_t medicine_percent = 0;
static float max_weight = 1000.0f;
static uint8_t medicine_low_warned = 0;
static uint32_t last_medicine_warn_tick = 0;
static uint32_t last_wifi_check_tick = 0;
static uint32_t wifi_reconnect_backoff = 5000;
static uint32_t last_cw2015_reinit_tick = 0;
static volatile uint8_t uart2_busy = 0;
volatile uint8_t rtc_time_synced = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief 微秒级延时函数
 * @details 该函数实现微秒级的精确延时，用于需要精确时序控制的场景，
 *          如 DHT11 传感器通信等
 * @param us 延时时间（单位：微秒）
 * @return 无
 * @note 该函数使用系统时钟频率计算延时周期，通过空操作实现延时
 */
void HAL_Delay_us(uint32_t us)
{
    // 计算延时所需的时钟周期数（考虑系统时钟频率）
    uint32_t ticks = us * (SystemCoreClock / 1000000) / 5;
    // 循环执行空操作，消耗时钟周期
    while (ticks--)
    {
        __NOP();  // 空操作指令
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_RTC_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  OLED_Init();
  CW2015_Init();
  DHT11_Init();
  HX711_Init();
  Servo_Init();
  Actuator_Init();
  TTS_Init();
  Key_Init();
  WiFi_Init();

  OLED_Clear();
  OLED_ShowString(0, 0, "PILLBOX");
  OLED_ShowString(0, 1, "Initializing...");
  OLED_UpdateDisplay();
  HAL_Delay(500);

  WiFi_Connect(WIFI_SSID, WIFI_PASSWORD);

  OLED_Clear();
  OLED_UpdateDisplay();
  last_loop_tick = HAL_GetTick();
  last_dht11_tick = HAL_GetTick();
  last_wifi_check_tick = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // ==================== 主循环任务调度（非阻塞） ====================
// 检查是否到达主循环执行周期（通过时间差判断，避免阻塞）
if ((int32_t)(HAL_GetTick() - last_loop_tick) >= MAIN_LOOP_PERIOD_MS)
{
    last_loop_tick = HAL_GetTick(); // 更新上一次主循环的执行时间戳

    // ==================== 基础外设处理 ====================
    Key_Process();   // 按键扫描与逻辑处理（如按键消抖、功能触发）
//    Servo_Process(); // 舵机状态更新与控制（如开关盒动作执行）

    // ==================== DHT11 温湿度读取（定时） ====================
    // 检查是否到达 DHT11 读取间隔
    if ((int32_t)(HAL_GetTick() - last_dht11_tick) >= DHT11_READ_INTERVAL_MS)
    {
        last_dht11_tick = HAL_GetTick(); // 更新 DHT11 读取时间戳
        DHT11_ReadData(&dht11_data);     // 读取温湿度数据并存入结构体
    }

    // ==================== 光照传感器与执行器控制 ====================
    uint8_t light_pct = LightSensor_GetPercent();                     // 获取光照强度百分比
    Actuator_Update(dht11_data.temp, dht11_data.hum, light_pct);     // 根据环境数据更新执行器（风扇/加热器/LED等）

    // ==================== HX711 重量传感器与药品剩余计算 ====================
    HX711_GetWeight();
    medicine_percent = (weight/10/max_weight)*100.0f;

    // ==================== CW2015 电池电量计读取 ====================
    CW2015_ReadData(&cw2015_data);
    if (!cw2015_data.comm_ok)
    {
        if ((int32_t)(HAL_GetTick() - last_cw2015_reinit_tick) >= 10000)
        {
            last_cw2015_reinit_tick = HAL_GetTick();
            CW2015_Init();
        }
    }

    // ==================== 设备状态打包与 WiFi 上报 ====================
    {
        Device_Status dev_status;
        dev_status.temp = dht11_data.temp;
        dev_status.hum = dht11_data.hum;
        dev_status.medicine_percent = medicine_percent;
        dev_status.light_percent = device_state.light_percent;
        dev_status.volume = device_state.current_volume;
        dev_status.battery_soc = cw2015_data.soc;
        dev_status.battery_voltage = cw2015_data.voltage_mv;
        dev_status.led_state = device_state.led_state;
        dev_status.fan_state = device_state.fan_state;
        dev_status.heater_state = device_state.heater_state;
        dev_status.box_open = Servo_IsOpen();
        dev_status.rtc_synced = rtc_time_synced;
        WiFi_UpdateStatus(&dev_status);
    }

    // ==================== TTS 服药提醒检查 ====================
    TTS_CheckReminders(); // 检查是否到达定时服药时间，触发 TTS 语音提醒

    // ==================== WiFi 数据缓冲区处理 ====================
    WiFi_ProcessBuffer(); // 处理 WiFi 模块接收的数据（如解析服务器下发的指令）

    // ==================== WiFi 连接状态检查与重连（每5秒） ====================
    if ((int32_t)(HAL_GetTick() - last_wifi_check_tick) >= wifi_reconnect_backoff)
    {
        last_wifi_check_tick = HAL_GetTick();
        WiFi_State *ws = WiFi_GetState();
        if (!ws->connected)
        {
            WiFi_Connect(WIFI_SSID, WIFI_PASSWORD);
            if (wifi_reconnect_backoff < 60000)
                wifi_reconnect_backoff *= 2;
            else
                wifi_reconnect_backoff = 60000;
        }
        else
        {
            wifi_reconnect_backoff = 5000;
        }
    }

    // ==================== WiFi 命令解析与执行 ====================
    char wifi_cmd_buf[4] = {0};
    uint8_t wifi_cmd_len = WiFi_GetCommand(wifi_cmd_buf); // 从 WiFi 模块获取下发的命令
    if (wifi_cmd_len > 0)
    {
        char wifi_cmd = wifi_cmd_buf[0]; // 提取命令字（首字节）
        
        // 命令 'O'：切换药盒开关（开→关 / 关→开）
        if (wifi_cmd == 'O')
        {
            if (Servo_IsOpen())
            {
                Servo_Toggle();       // 切换舵机状态（关闭药盒）
                TTS_PLAY1(1); // TTS 语音提示
            }
            else
            {
                Servo_Toggle();       // 切换舵机状态（打开药盒）
                TTS_PLAY1(0);// TTS 语音提示
            }
        }
        // 命令 'C'：关闭药盒（仅当药盒打开时执行）
        else if (wifi_cmd == 'C')
        {
            if (Servo_IsOpen())
            {
                Servo_Toggle();       // 关闭药盒
                TTS_PLAY1(1); // TTS 语音提示
            }
        }
        // 命令 'U'：音量增加
        else if (wifi_cmd == 'U')
        {
            TTS_VolumeUp();                          // TTS 音量 +1
            device_state.current_volume = TTS_GetVolume(); // 保存当前音量到全局状态
        }
        // 命令 'D'：音量减小
        else if (wifi_cmd == 'D')
        {
            TTS_VolumeDown();                        // TTS 音量 -1
            device_state.current_volume = TTS_GetVolume(); // 保存当前音量到全局状态
        }
        // 命令 'V'：设置指定音量（0-100）
        else if (wifi_cmd == 'V')
        {
            uint8_t vol = wifi_cmd_buf[1];          // 提取音量值（命令第二字节）
            TTS_SetVolume100(vol);                   // 设置 TTS 音量（范围 0-100）
            device_state.current_volume = TTS_GetVolume(); // 保存当前音量到全局状态
        }
    }

    // ==================== 药品不足提醒（定时 TTS） ====================
    if (medicine_percent < 10)
    {
        // 若未提醒过，或距离上次提醒超过 60 秒，触发提醒
        if (!medicine_low_warned || (int32_t)(HAL_GetTick() - last_medicine_warn_tick) >= 60000)
        {
            TTS_PLAY1(2);              // TTS 播放“药品不足”提醒
            medicine_low_warned = 1;             // 标记“已提醒”
            last_medicine_warn_tick = HAL_GetTick(); // 更新提醒时间戳
        }
    }
    else
    {
        medicine_low_warned = 0; // 药品充足时，重置“已提醒”标记
    }

    // ==================== OLED 显示屏更新 ====================
    OLED_Clear(); // 清空 OLED 显示缓冲区（准备绘制新内容）
    // 获取 RTC 实时时钟时间与日期
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN); // 获取时间（二进制格式）
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); // 获取日期（二进制格式，本代码未用日期）

    // -------------------- 第 0 行：时间 + 音量 --------------------
    OLED_ShowString(0, 0, "T:");       // 显示 "T:"（时间前缀）
    OLED_ShowNum(12, 0, sTime.Hours, 2);   // 显示小时（2位）
    OLED_ShowString(24, 0, ":");       // 显示 ":"
    OLED_ShowNum(30, 0, sTime.Minutes, 2); // 显示分钟（2位）
    OLED_ShowString(42, 0, ":");       // 显示 ":"
    OLED_ShowNum(48, 0, sTime.Seconds, 2); // 显示秒（2位）

    OLED_ShowString(66, 0, "V:");      // 显示 "V:"（音量前缀）
    OLED_ShowNum(78, 0, device_state.current_volume, 3); // 显示音量（3位）

    // -------------------- 第 1 行：温度 + 湿度 --------------------
    OLED_ShowString(0, 1, "Temp:");    // 显示 "Temp:"（温度前缀）
    OLED_ShowNum(30, 1, dht11_data.temp, 2); // 显示温度（2位）
    OLED_ShowString(42, 1, "C");       // 显示 "C"（摄氏度单位）

    OLED_ShowString(66, 1, "Hum:");     // 显示 "Hum:"（湿度前缀）
    OLED_ShowNum(90, 1, dht11_data.hum, 2);  // 显示湿度（2位）
    OLED_ShowString(102, 1, "%");       // 显示 "%"（百分比单位）

    // -------------------- 第 2 行：药品剩余 + 电池电量 --------------------
    OLED_ShowString(0, 2, "Med:");     // 显示 "Med:"（药品前缀）
    OLED_ShowPercent(24, 2, medicine_percent); // 显示药品剩余百分比

    OLED_ShowString(66, 2, "Bat:");     // 显示 "Bat:"（电池前缀）
    OLED_ShowPercent(90, 2, cw2015_data.soc); // 显示电池电量百分比

    // -------------------- 第 3 行：光照强度 --------------------
    OLED_ShowString(0, 3, "Light:");   // 显示 "Light:"（光照前缀）
    OLED_ShowPercent(36, 3, device_state.light_percent); // 显示光照百分比
    
    // -------------------- 第 4 行：WiFi 状态与 IP --------------------
    WiFi_State *ws = WiFi_GetState();   // 获取 WiFi 状态
    if (ws->connected)
    {
        OLED_ShowString(0, 4, "IP:");   // 显示 "IP:"
        if (ws->ip[0] != '\0')
        {
            OLED_ShowString(18, 4, ws->ip); // 显示已获取的 IP 地址
        }
        else
        {
            OLED_ShowString(18, 4, "getting..."); // 正在获取 IP 中
        }
    }
    else
    {
        OLED_ShowString(0, 4, "Connecting..."); // 正在连接 WiFi
    }

    OLED_UpdateDisplay(); // 将缓冲区内容刷新到 OLED 屏幕（实际显示更新）
}
}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
extern volatile uint8_t wifi_buffer[512];
extern volatile uint16_t wifi_buffer_index;
extern uint8_t wifi_rx_byte;

/**
 * @brief UART 接收完成回调函数
 * @details 该函数在 UART 接收完成时被 HAL 库自动调用，
 *          用于处理 WiFi 模块的数据接收
 * @param huart UART 句柄指针，指示哪个 UART 端口接收完成
 * @return 无
 * @note 该函数在中断上下文中执行，需要快速处理
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    // 检查是否是 USART1（WiFi 模块使用的串口）
    if (huart->Instance == USART1)
    {
        // 检查缓冲区是否还有空间
        if (wifi_buffer_index < 511)
        {
            // 将接收到的字节存入缓冲区
            wifi_buffer[wifi_buffer_index++] = wifi_rx_byte;
        }
        // 重新启动中断接收，准备接收下一个字节
        HAL_UART_Receive_IT(&huart1, &wifi_rx_byte, 1);
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
