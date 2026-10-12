#include "motor.h"
#include "encoder.h"


void M1_setdir(u8 dir)
{
    if(dir == 0)
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
    }
}

void M2_setdir(u8 dir)
{
    if(dir == 0)
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
    }
}

void Motor_Init(void)
{
    HAL_TIM_Base_Init(&htim1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, 0);
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_2, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);    //使能电机
}

void Motor_Start(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);  //使能电机
}

void Motor_Stop(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);  //使能电机
}

void M_setspeed(int16_t speed1, int16_t speed2)
{
    if(speed1 >=0){M1_setdir(0);} else{M1_setdir(1);speed1 = -speed1;}
    if(speed2 >=0){M2_setdir(0);} else{M2_setdir(1);speed2 = -speed2;}

    u16 ccr1, ccr2;
    ccr1 = speed1;
    ccr2 = speed2;

    if(ccr1 > PWM_MAX){ccr1 = PWM_MAX;}
    if(ccr2 > PWM_MAX){ccr2 = PWM_MAX;}


    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, ccr1);
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_2, ccr2);
}

float a = 0.5f;              /* 低通滤波系数: 0~1, 0.2 起步 */
float Velocity_Kp = 40;      /* 起步值, 使 PWM 能突破死区 PWM_MIN=20 */
float Velocity_Ki = 2;
float Velocity_Kd = 0;

int16_t Velocity1(float target, float encoder)   /* 轮1 = htim3 */
{
    int16_t PWM_out;
    static float Encoder_S, Encoder_Err_Lowout_last, Encoder_Err_Lowout;

    float Encoder_Err = target - encoder;
    Encoder_Err_Lowout = (1-a) * Encoder_Err_Lowout_last + a * Encoder_Err;
    Encoder_Err_Lowout_last = Encoder_Err_Lowout;
    Encoder_S += Encoder_Err_Lowout;
    Encoder_S = Encoder_S>100?100:(Encoder_S<(-100)?(-100):Encoder_S);

    //if(encoder == 0){Encoder_S = 0;}

    PWM_out = (int16_t)(Velocity_Kp * Encoder_Err_Lowout + Velocity_Ki * Encoder_S);

    return PWM_out;
}

int16_t Velocity2(float target, float encoder)   /* 轮2 = htim2, 独立static */
{
    int16_t PWM_out;
    static float Encoder_S, Encoder_Err_Lowout_last, Encoder_Err_Lowout;

    float Encoder_Err = target - encoder;
    Encoder_Err_Lowout = (1-a) * Encoder_Err_Lowout_last + a * Encoder_Err;
    Encoder_Err_Lowout_last = Encoder_Err_Lowout;
    Encoder_S += Encoder_Err_Lowout;
    Encoder_S = Encoder_S>100?100:(Encoder_S<(-100)?(-100):Encoder_S);

    //if(encoder == 0){Encoder_S = 0;}

    PWM_out = (int16_t)(Velocity_Kp * Encoder_Err_Lowout + Velocity_Ki * Encoder_S);

    return PWM_out;
}

float Position_Kp = 0.05f;
float Position_Ki = 0;
float Position_Kd = 0;

int16_t Position(int32_t target_position, int32_t M1_encoder, int32_t M2_encoder)
{
    int16_t target_speed;
    static int16_t Position_Err, Position_Err_last;
    int16_t current_position;
    
    current_position = (M1_encoder + M2_encoder)/2;

    Position_Err = target_position - current_position;

    target_speed = ROUND_F(Position_Kp * Position_Err + Position_Kd * (Position_Err - Position_Err_last));

    Position_Err_last = Position_Err;

    target_speed = target_speed > 5000 ? 5000 : (target_speed < (-5000) ? (-5000) : target_speed);

    return target_speed;
}

float Angle_Kp = 4.00f;
float Angle_Ki = 2;
float Angle_Kd = 0;

int16_t Angle(float target_angle, float current_angle, float current_velocity)
{
    int32_t PWM_out;
    static float Angle_Err;
    /* 角度误差取最短路径 [-180,180): 避免 yaw 在 0↔360 回绕时的±360跳变 */
    Angle_Err = target_angle - current_angle;
    while (Angle_Err > 180.0f)  Angle_Err -= 360.0f;
    while (Angle_Err < -180.0f) Angle_Err += 360.0f;
    
    PWM_out = ROUND_F(Angle_Kp * Angle_Err + Angle_Kd * current_velocity);

    return PWM_out;
}
