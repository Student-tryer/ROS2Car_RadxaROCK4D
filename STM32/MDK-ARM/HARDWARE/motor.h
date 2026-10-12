#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "tim.h"
#include "sys.h"

/* ==================== 单位换算常数 ==================== */
#define ENCODER_PPR     336        /* TI1模式: 输出轴转一圈 = 168 counts */
#define WHEEL_CIRC_MM   218.34f    /* 轮周长 = π × 69.5mm */
#define LOOP_HZ         200        /* 速度环频率 */

/* 四舍五入 (支持负数) */
#define ROUND_F(x)      ((int32_t)((x) < 0 ? (x) - 0.5f : (x) + 0.5f))

#define SPD_WIN         10        /* 测速窗口: 10 tick × 5ms = 50ms */
#define WHEELBASE_MM    168.72f   /* 轮距: 差速运动学用 */
#define PWM_MAX        200
#define PWM_MIN        0

extern float Velocity_Kp;
extern float Velocity_Ki;
extern float Velocity_Kd;

extern float Position_Kp;
extern float Position_Ki;
extern float Position_Kd;

extern float Angle_Kp;
extern float Angle_Ki;
extern float Angle_Kd;

/* 位置: mm → 编码器计数 这里的count就是每mm计数多少次的意思*/
static inline int32_t mm_to_count(float mm)
{
    return ROUND_F(mm * ENCODER_PPR / WHEEL_CIRC_MM);
}

/* 位置反馈: 编码器计数 → mm */
static inline float count_to_mm(int32_t count)
{
    return (float)count * WHEEL_CIRC_MM / ENCODER_PPR;
}

/* 速度目标: m/s → 每周期编码器增量 (与编码器实测增量同单位) */
static inline float mps_to_tick(float v_mps)
{
    return (v_mps * 1000.0f * ENCODER_PPR / (WHEEL_CIRC_MM * LOOP_HZ));
}

/* 速度反馈: 每周期增量 → m/s */
static inline float tick_to_mps(int32_t tick)
{
    return (float)tick * WHEEL_CIRC_MM * LOOP_HZ / (1000.0f * ENCODER_PPR);
}

/* ==================== 电机控制接口 ==================== */
void Motor_Init(void);
void Motor_Start(void);
void Motor_Stop(void);
void M_setspeed(int16_t speed1, int16_t speed2);



int16_t Velocity1(float target, float encoder);   /* 轮1(htim3)独立速度环 */
int16_t Velocity2(float target, float encoder);   /* 轮2(htim2)独立速度环 */

int16_t Position(int32_t target_position, int32_t M1_encoder, int32_t M2_encoder);

int16_t Angle(float target_angle, float current_angle, float current_velocity);

#endif
