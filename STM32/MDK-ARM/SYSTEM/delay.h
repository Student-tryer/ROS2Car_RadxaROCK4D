#ifndef __DELAY_H
#define __DELAY_H
#include "sys.h"

// DWT微秒延时 + HAL毫秒延时
// DWT使用Cortex-M4内核周期计数器，不占用任何定时器，与HAL_Delay互不干扰

void delay_init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

#endif
