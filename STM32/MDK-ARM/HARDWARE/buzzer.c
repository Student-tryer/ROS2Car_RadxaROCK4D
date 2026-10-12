#include "buzzer.h"

void buzzer_ring(u16 period)
{
    HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN);
    HAL_Delay(period);
    HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN);
    HAL_Delay(period);
    //HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

void buzzer_start(void)
{
    for(u8 i = 0; i<3; i++)
    {
        buzzer_ring(200);
    }
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

void buzzer_lowvoltage(void)
{
    for(u8 i = 0; i<10; i++)
    {
        buzzer_ring(100);
    }
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}