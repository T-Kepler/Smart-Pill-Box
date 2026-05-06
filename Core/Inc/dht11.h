#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"
#include "hardware_config.h"

typedef struct {
    uint8_t temp;
    uint8_t hum;
} DHT11_Data;

void DHT11_Init(void);
uint8_t DHT11_ReadData(DHT11_Data *dat);

#endif
