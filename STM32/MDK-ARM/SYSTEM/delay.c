#include "delay.h"


static u32 fac_us = 0;

static void delay_cycles(u32 cycles)
{
    u32 start = DWT->CYCCNT;
    while ((u32)(DWT->CYCCNT - start) < cycles)
        ;
}

void delay_init(void)
{
    CoreDebug->DEMCR |=CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCTAP_Msk;
    fac_us = SystemCoreClock / 1000000;
}

void delay_us(u32 us)
{
    u32 ticks = us * fac_us;
    delay_cycles(ticks);
}

void delay_ms(u32 ms)
{
    HAL_Delay(ms);
}

