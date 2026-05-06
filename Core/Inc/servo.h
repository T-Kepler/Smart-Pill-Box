#ifndef __SERVO_H
#define __SERVO_H

#include "main.h"

void Servo_Init(void);
void Servo_Open(void);
void Servo_Close(void);
void Servo_Toggle(void);
uint8_t Servo_GetState(void);
uint8_t Servo_IsOpen(void);

#endif
