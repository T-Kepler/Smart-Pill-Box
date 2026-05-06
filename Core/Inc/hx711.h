#ifndef __HX711_H
#define __HX711_H

#include "stm32f1xx_hal.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

void HX711_Init(void);
int32_t HX711_ReadRaw(void);
void HX711_Tare(uint8_t times);
int32_t HX711_GetWeight(void);
void HX711_SetCoefficient(float coeff);
uint8_t HX711_IsReady(void);

void HX711_GPIO_Init(void);
void Get_Tare(void);
void Get_Weight(void);

extern u32 weight;
extern u32 pi_weight;
extern u32 hx711_xishu;
extern int32_t g_last_raw;

#endif
