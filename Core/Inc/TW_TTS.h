#ifndef __TW_TTS_H
#define __TW_TTS_H

#include "main.h"
#include "key.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define TTS_STATE_ERROR   0
#define TTS_STATE_PLAYING 1
#define TTS_STATE_IDLE    2
#define TTS_STATE_INIT    3
#define TTS_STATE_CHECK   4

#define TTS_REMINDER_COUNT 3

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t triggered;
} TTS_Reminder;

void tts_writeData(unsigned char *DAT, unsigned char len);
void TTS_play(unsigned char* str);
void TTS_volume(uint8_t vol);
void TTS_SetVolume100(uint8_t volume);
void TTS_speed(uint8_t speed);
void TTS_tone(uint8_t tone);
void TTS_alert(uint8_t alert);
void TTS_play_msg(uint8_t msg);
void TTS_play_ring(uint8_t ring);
void TTS_stop(void);
void TTS_pause(void);
void TTS_resume(void);
uint8_t TTS_queryState(uint8_t response);

void TTS_Init(void);
void TTS_VolumeUp(void);
void TTS_VolumeDown(void);
uint8_t TTS_GetVolume(void);
void TTS_SetReminder(uint8_t index, uint8_t h, uint8_t m);
void TTS_CheckReminders(void);

#endif
