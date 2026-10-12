#ifndef __BUZZER_H__
#define __BUZZER_H__

#include "sys.h"

#define BUZZER_PORT GPIOB
#define BUZZER_PIN  GPIO_PIN_0

void buzzer_ring(u16 period);
void buzzer_start(void);
void buzzer_lowvoltage(void);



#endif
