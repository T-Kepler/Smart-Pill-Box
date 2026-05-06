#ifndef __ACTUATOR_H
#define __ACTUATOR_H

#include "main.h"
#include "hardware_config.h"

typedef struct {
    uint8_t led_state;
    uint8_t fan_state;
    uint8_t heater_state;
    uint8_t current_volume;
    uint8_t light_percent;
} Device_State;

typedef struct {
    uint8_t temp_high;
    uint8_t temp_low;
    uint8_t light_on;
    uint8_t light_off;
} Actuator_Thresholds;

extern Device_State device_state;

void Actuator_Init(void);
void Actuator_Update(uint8_t temp, uint8_t hum, uint8_t light_percent);
void Actuator_GetThresholds(Actuator_Thresholds *th);
void Actuator_SetThresholds(Actuator_Thresholds *th);

#endif
