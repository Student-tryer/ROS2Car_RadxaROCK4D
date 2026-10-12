#ifndef __ENCODER_H__
#define __ENCODER_H__

#include "tim.h"

/* 每转脉冲数(霍尔线数 × 减速比), 需按实际校准 */
//#define ENCODER_PPR  13

void    Encoder_Start(void);
int32_t Encoder_Read(TIM_HandleTypeDef *htim);
void    Encoder_Reset(TIM_HandleTypeDef *htim);
void Encoder_GetRPM(int32_t *rpm1, int32_t *rpm2);

#endif
