#ifndef __VOLTAGE_H__
#define __VOLTAGE_H__

#include "stm32f1xx_hal.h"

uint16_t voltage_read_raw(void);
float    voltage_read_battery(void);

#endif
