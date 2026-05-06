/**
 ******************************************************************************
 * @file    TW_TTS.c
 * @author  TTS Driver Team
 * @version V1.0.0
 * @date    2025-04-22
 * @brief   TTS (Text-to-Speech) 文字转语音模块驱动程序
 * @details 该文件实现了 TTS 模块的所有功能函数，包括语音播放、音量控制、
 *          语速调节、音调设置、提醒功能等。通过 UART 接口与 TTS 模块通信，
 *          支持中英文语音合成和播放控制。
 ******************************************************************************
 * @attention
 *          本驱动程序基于 TW-TTS 模块协议开发，使用 UART2 接口进行通信
 *          波特率设置为 9600bps，数据格式为 8N1
 *          所有命令帧以 0xFD 开头，遵循模块通信协议规范
 ******************************************************************************
 */

#include "usart.h"
#include "TW_TTS.h"
#include "rtc.h"
#include "hardware_config.h"
#include <string.h>
#include <stdlib.h>

extern volatile uint8_t uart2_busy;

static uint8_t TTS_VOLUME_CMD[] = {0xfd, 0x00, 0x06, 0x01, 0x01, 0x5b, 0x76, 0x30, 0x5d};
static uint8_t TTS_QUERY_CMD[] = {0xFD, 0x00, 0x02, 0x21};

static uint8_t tts_volume = DEFAULT_VOLUME;
static TTS_Reminder tts_reminders[TTS_REMINDER_COUNT];

/**
 * @brief 通过 UART 发送数据到 TTS 模块
 * @details 该函数通过 UART2 接口将数据发送到 TTS 模块，
 *          发送过程中设置忙标志位，发送完成后清除忙标志位
 * @param DAT 指向要发送的数据缓冲区的指针
 * @param len 要发送的数据长度
 * @return 无
 * @note 发送超时时间设置为 2000ms，发送后延时 100ms 确保数据传输完成
 */
void tts_writeData(unsigned char *DAT, unsigned char len)
{
    uart2_busy = 1;
    HAL_UART_Transmit(&huart2, DAT, len, 2000);
    HAL_Delay(100);
    uart2_busy = 0;
}

/**
 * @brief 播放指定的文本语音
 * @details 该函数将文本字符串发送到 TTS 模块进行语音合成和播放。
 *          函数会构造符合 TTS 协议的数据帧，包含帧头、长度、命令字和文本内容。
 *          支持中英文混合文本播放，最大文本长度为 4094 字节。
 * @param str 指向要播放的文本字符串的指针
 * @return 无
 * @note 文本长度必须在 1-4094 字节之间，否则函数直接返回
 *       数据帧格式：帧头(0xFD) + 长度(2字节) + 命令字(0x01) + 文本编码(0x00) + 文本内容
 *       使用动态内存分配发送缓冲区，使用后需释放内存
 */
void TTS_play(unsigned char* str)
{
    unsigned char Frame_Info[5];
    uint16_t str_len;

    if (str == NULL) return;

    str_len = strlen((char*)str);

    if (str_len == 0 || str_len > 4094) {
        return;
    }

    Frame_Info[0] = 0xFD;
    Frame_Info[1] = (str_len + 2) >> 8;
    Frame_Info[2] = (str_len + 2) & 0xFF;
    Frame_Info[3] = 0x01;
    Frame_Info[4] = 0x00;

    unsigned char *send_buf = (unsigned char*)malloc(5 + str_len);
    if (send_buf == NULL) return;

    memcpy(send_buf, Frame_Info, 5);
    memcpy(send_buf + 5, str, str_len);

    tts_writeData(send_buf, 5 + str_len);

    free(send_buf);
}

/**
 * @brief 设置 TTS 模块的音量（0-9 级）
 * @details 该函数通过发送音量控制命令来设置 TTS 模块的音量级别。
 *          音量级别范围为 0-9，其中 0 为静音，9 为最大音量。
 *          命令格式：[0xFD, 0x00, 0x06, 0x01, 0x01, 0x5B, 0x76, 0x30+vol, 0x5D]
 * @param vol 音量级别（0-9），超过 9 时自动限制为 9
 * @return 无
 * @note 音量设置立即生效，会影响当前正在播放的语音
 */
void TTS_volume(uint8_t vol)
{
    uint8_t volume_buf[9];
    uint8_t i;

    if (vol > 9)
    {
        vol = 9;
    }

    for(i = 0; i < 9; i++)
    {
        volume_buf[i] = TTS_VOLUME_CMD[i];
    }

    volume_buf[7] = 0x30 + vol;
    tts_writeData(volume_buf, ARRAY_LEN(volume_buf));
}

/**
 * @brief 将 0-100 的音量值映射到 0-9 的音量级别
 * @details 该函数将用户友好的 0-100 音量值转换为 TTS 模块支持的 0-9 音量级别。
 *          使用线性映射算法，确保音量调节的平滑性。
 * @param volume_100 0-100 范围的音量值
 * @return 映射后的 0-9 音量级别
 * @note 当 volume_100 为 0 时返回 0（静音），大于等于 100 时返回 9（最大音量）
 */
static uint8_t TTS_MapVolume(uint8_t volume_100)
{
    if (volume_100 >= 100) return 9;
    if (volume_100 == 0) return 0;
    return (volume_100 * 9) / 100;
}

/**
 * @brief 设置 TTS 模块的音量（0-100）
 * @details 该函数设置 TTS 模块的音量，使用 0-100 的用户友好范围。
 *          函数会自动将 0-100 的音量值映射到模块支持的 0-9 音量级别。
 *          音量值会被限制在 VOLUME_MIN 和 VOLUME_MAX 之间。
 * @param volume 音量值（0-100），超出范围时自动限制
 * @return 无
 * @note 音量设置会保存到全局变量 tts_volume 中，供后续音量调节使用
 */
void TTS_SetVolume100(uint8_t volume)
{
    if (volume > VOLUME_MAX) volume = VOLUME_MAX;
    if (volume <= VOLUME_MIN) volume = VOLUME_MIN;

    tts_volume = volume;
    TTS_volume(TTS_MapVolume(volume));
}

/**
 * @brief 查询 TTS 模块的当前状态
 * @details 该函数发送查询命令到 TTS 模块，并根据模块返回的响应判断当前状态。
 *          支持的状态包括：播放中、空闲、初始化中、检查中、错误。
 *          命令格式：[0xFD, 0x00, 0x02, 0x21]
 * @param response TTS 模块返回的响应字节
 * @return TTS 模块的状态码
 *         - TTS_STATE_PLAYING (1): 正在播放
 *         - TTS_STATE_IDLE (2): 空闲
 *         - TTS_STATE_INIT (3): 初始化中
 *         - TTS_STATE_CHECK (4): 检查中
 *         - TTS_STATE_ERROR (0): 错误状态
 * @note 需要通过 UART 接收模块的响应数据，响应字节含义：
 *       0x4E: 播放中, 0x4F: 空闲, 0x4A: 初始化中, 0x41: 检查中
 */
uint8_t TTS_queryState(uint8_t response)
{
    uint8_t query_buf[4];
    uint8_t i;

    for(i = 0; i < 4; i++)
    {
        query_buf[i] = TTS_QUERY_CMD[i];
    }

    tts_writeData(query_buf, ARRAY_LEN(query_buf));

    HAL_Delay(10);

    if (response == 0x4E)
    {
        return TTS_STATE_PLAYING;
    }
    else if (response == 0x4F)
    {
        return TTS_STATE_IDLE;
    }
    else if (response == 0x4A)
    {
        return TTS_STATE_INIT;
    }
    else if (response == 0x41)
    {
        return TTS_STATE_CHECK;
    }
    else
    {
        return TTS_STATE_ERROR;
    }
}

/**
 * @brief 初始化 TTS 语音合成模块
 * @details 该函数初始化语音合成模块的音量和提醒时间，
 *          设置三个默认的服药提醒时间（8:00、12:00、18:00）
 * @param 无
 * @return 无
 * @note 初始化完成后会设置默认音量
 */
void TTS_Init(void)
{
    tts_volume = DEFAULT_VOLUME;

    TTS_SetReminder(0, 8, 0);   // 早上 8:00
    TTS_SetReminder(1, 12, 0);  // 中午 12:00
    TTS_SetReminder(2, 18, 0);  // 晚上 18:00

    HAL_Delay(500);
    TTS_SetVolume100(tts_volume);
}

/**
 * @brief 增加音量
 * @details 该函数将音量增加一个步长值，如果达到最大值则不再增加
 * @param 无
 * @return 无
 * @note 音量步长由 VOLUME_STEP 定义，最大值由 VOLUME_MAX 定义
 */
void TTS_VolumeUp(void)
{
    if (tts_volume <= VOLUME_MAX - VOLUME_STEP)
    {
        tts_volume += VOLUME_STEP;
    }
    else
    {
        tts_volume = VOLUME_MAX;
    }
    TTS_SetVolume100(tts_volume);
}

/**
 * @brief 减小音量
 * @details 该函数将音量减小一个步长值，如果达到最小值则不再减小
 * @param 无
 * @return 无
 * @note 音量步长由 VOLUME_STEP 定义，最小值由 VOLUME_MIN 定义
 */
void TTS_VolumeDown(void)
{
    if (tts_volume >= VOLUME_STEP)
    {
        tts_volume -= VOLUME_STEP;
    }
    else
    {
        tts_volume = VOLUME_MIN;
    }
    TTS_SetVolume100(tts_volume);
}

/**
 * @brief 获取当前音量
 * @details 该函数返回当前语音合成模块的音量值
 * @param 无
 * @return 当前音量值（0-100）
 */
uint8_t TTS_GetVolume(void)
{
    // 返回当前音量值
    return tts_volume;
}

/**
 * @brief 设置提醒时间
 * @details 该函数设置指定索引的提醒时间，用于服药提醒功能
 * @param index 提醒索引（0-2）
 * @param h 小时（0-23）
 * @param m 分钟（0-59）
 * @return 无
 * @note 如果索引超出范围，则不执行任何操作
 */
void TTS_SetReminder(uint8_t index, uint8_t h, uint8_t m)
{
    if (index >= TTS_REMINDER_COUNT) return;
    tts_reminders[index].hour = h;
    tts_reminders[index].minute = m;
    tts_reminders[index].triggered = 0;
}

/**
 * @brief 检查并触发服药提醒
 * @details 该函数定期检查当前时间是否匹配预设的提醒时间。
 *          如果匹配且该提醒尚未触发，则播放语音提示"该吃药啦"。
 *          每个提醒每天只触发一次，触发后会设置 triggered 标志。
 *          当当前分钟不等于任何提醒时间的分钟时，重置所有 triggered 标志，
 *          确保第二天同一时间可以再次触发提醒。
 * @param 无
 * @return 无
 * @note 该函数需要在主循环中定期调用，建议每秒调用一次
 *       依赖 RTC 模块提供准确的当前时间
 *       支持最多 3 个提醒时间，默认为 8:00、12:00、18:00
 */
void TTS_CheckReminders(void)
{
    RTC_TimeTypeDef sTime;
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    RTC_DateTypeDef sDate;
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    uint8_t i;
    for (i = 0; i < TTS_REMINDER_COUNT; i++)
    {
        if (tts_reminders[i].triggered) continue;
        if (sTime.Hours == tts_reminders[i].hour && sTime.Minutes == tts_reminders[i].minute)
        {
            TTS_PLAY1(3);
            tts_reminders[i].triggered = 1;
        }
    }

    if (sTime.Minutes != tts_reminders[0].minute &&
        sTime.Minutes != tts_reminders[1].minute &&
        sTime.Minutes != tts_reminders[2].minute)
    {
        for (i = 0; i < TTS_REMINDER_COUNT; i++)
        {
            tts_reminders[i].triggered = 0;
        }
    }
}
