#include "key.h"
#include "hardware_config.h"
#include "servo.h"
#include "TW_TTS.h"
#include "actuator.h"
#include "usart.h"
#include "stdio.h"

extern volatile uint8_t uart2_busy;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t last_state;
    uint8_t stable_state;
    uint32_t last_tick;
    uint8_t long_press_active;
    uint32_t long_press_tick;
} Key_State;

static Key_State keys[3];

/**
 * @brief 初始化按键系统
 * @details 该函数初始化三个按键的状态结构体，设置按键的 GPIO 端口和引脚，
 *          并初始化按键状态变量，为后续的按键处理做准备
 * @param 无
 * @return 无
 * @note 按键使用上拉电阻，默认状态为高电平（1），按下时为低电平（0）
 */
void Key_Init(void)
{
    // 初始化按键 1 的状态
    keys[0].port = KEY1_PORT;
    keys[0].pin = KEY1_PIN;
    keys[0].last_state = 1;           
    keys[0].stable_state = 1;      
    keys[0].last_tick = 0;         
    keys[0].long_press_active = 0;     
    keys[0].long_press_tick = 0;     

    // 初始化按键 2 的状态
    keys[1].port = KEY2_PORT;
    keys[1].pin = KEY2_PIN;
    keys[1].last_state = 1;
    keys[1].stable_state = 1;
    keys[1].last_tick = 0;
    keys[1].long_press_active = 0;
    keys[1].long_press_tick = 0;

    // 初始化按键 3 的状态
    keys[2].port = KEY3_PORT;
    keys[2].pin = KEY3_PIN;
    keys[2].last_state = 1;
    keys[2].stable_state = 1;
    keys[2].last_tick = 0;
    keys[2].long_press_active = 0;
    keys[2].long_press_tick = 0;
}



void TTS_PLAY1(uint8_t i)
{
    if(i==0)
    TTS_play("开启药盒") ;
    else if(i==1)
        TTS_play("关闭药盒");
    else if(i==2)
        TTS_play("药品不足");
        else if(i==3)
            TTS_play("该吃药了");
            
}
/**
 * @brief 处理按键按下事件
 * @details 该函数根据按键索引执行相应的操作，包括药盒开关控制、音量调节等，
 *          并通过语音反馈和日志记录来确认操作执行
 * @param index 按键索引（0-药盒开关，1-音量增加，2-音量减小）
 * @return 无
 * @note 函数会根据药盒的当前状态播放相应的语音提示
 */


static void Key_OnPress(uint8_t index)
{
    if (index == 0)
    {
        if (Servo_IsOpen())
        {
            Servo_Toggle();
            TTS_PLAY1(1) ;
        }
        else
        {
            Servo_Toggle();
            TTS_PLAY1(0);
        }

        if (!uart2_busy)
        {
            char log_buf[64];
            int log_len = snprintf(log_buf, sizeof(log_buf), "[KEY] Btn1: Toggle + TTS\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t*)log_buf, log_len, 500);
        }
    }
    else if (index == 1)
    {
        TTS_VolumeUp();
        device_state.current_volume = TTS_GetVolume();
    }
    else if (index == 2)
    {
        TTS_VolumeDown();
        device_state.current_volume = TTS_GetVolume();
    }
}

/**
 * @brief 处理按键事件
 * @details 该函数在主循环中定期调用，检测按键状态变化，实现按键去抖动、
 *          长按检测和按键重复功能
 * @param 无
 * @return 无
 * @note 按键 1（开/关药盒）不支持长按和重复功能，按键 2 和 3（音量控制）支持
 */
void Key_Process(void)
{
    uint8_t i;  
    for (i = 0; i < 3; i++)
    {
        uint8_t cur = HAL_GPIO_ReadPin(keys[i].port, keys[i].pin);

        if (cur != keys[i].last_state)
        {
            keys[i].last_tick = HAL_GetTick();
            keys[i].last_state = cur;
        }

                 if (cur != keys[i].stable_state)
            {
                keys[i].stable_state = cur;

                if (cur == GPIO_PIN_RESET)
                {
                    Key_OnPress(i);
                    keys[i].long_press_active = 0;
                    keys[i].long_press_tick = HAL_GetTick();
                }
                else
                {
                    keys[i].long_press_active = 0;
                }
            }
        

        if (keys[i].stable_state == GPIO_PIN_RESET && i != 0)
        {
            if (!keys[i].long_press_active)
            {
                if ((HAL_GetTick() - keys[i].long_press_tick) >= KEY_LONG_PRESS_MS)
                {
                    keys[i].long_press_active = 1;
                    keys[i].long_press_tick = HAL_GetTick();
                    Key_OnPress(i);
                }
            }
            else
            {
                if ((HAL_GetTick() - keys[i].long_press_tick) >= KEY_REPEAT_MS)
                {
                    keys[i].long_press_tick = HAL_GetTick();
                    Key_OnPress(i);
                }
            }
        }
    }
}
