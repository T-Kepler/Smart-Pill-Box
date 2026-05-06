#ifndef __CW2015_H
#define __CW2015_H

#include "main.h"
#include "stdint.h"

typedef struct {
    uint16_t voltage_mv;
    uint8_t  soc;
    uint8_t  comm_ok;
} CW2015_Data;

void CW2015_Init(void);
void CW2015_ReadData(CW2015_Data *data);

#endif
