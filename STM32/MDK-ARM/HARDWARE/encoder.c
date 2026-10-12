#include "encoder.h"
#include "delay.h"
#include "motor.h"

#define ENCODER_SAMPLE_MS  5   /* 采样窗口 */

void Encoder_Start()
{
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
}

int32_t Encoder_Read(TIM_HandleTypeDef *htim)
{
    return (int32_t)__HAL_TIM_GET_COUNTER(htim);
}

void Encoder_Reset(TIM_HandleTypeDef *htim)
{
    __HAL_TIM_SET_COUNTER(htim, 0);
}

/* 采样 ENCODER_SAMPLE_MS 毫秒内的脉冲增量, 换算为 RPM */
void Encoder_GetRPM(int32_t *rpm1, int32_t *rpm2)
{
    int32_t start1 = Encoder_Read(&htim3);
    int32_t start2 = Encoder_Read(&htim2);

    delay_ms(ENCODER_SAMPLE_MS);

    int32_t delta1 = Encoder_Read(&htim3) - start1;
    int32_t delta2 = Encoder_Read(&htim2) - start2;

    /* RPM = (delta / PPR) / (采样秒) * 60 */
    *rpm1 = (delta1 * 60000L) / ((int32_t)ENCODER_PPR * ENCODER_SAMPLE_MS);
    *rpm2 = (delta2 * 60000L) / ((int32_t)ENCODER_PPR * ENCODER_SAMPLE_MS);
}

