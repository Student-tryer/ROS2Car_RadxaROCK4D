/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "buzzer.h"
#include "motor.h"
#include "encoder.h"
#include "bmi088.h"
#include "delay.h"
#include "myadc.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
u8 accel_id, gyro_id;
u8 bmi_init_done;

volatile u8 task_flag;

float yaw, yaw1, yaw_velocity, yaw_angle;
int16_t gyro[3];

float speed, current_speed, target_distance;

volatile float target_speed, target_angle, target_gyro; 
volatile u8 connected, request_odom, request_voltage, stop, reset, start;

int16_t rpm1, rpm2;

int16_t now1, now2, last1, last2, speed1, speed2;

uint16_t tim_count;


int32_t pos1, pos2;

int32_t angle_speed_err;
int16_t pwm_fwd;
int16_t pwm_fwd2;

/* 50ms 滑动窗口测速 (SPD_WIN=10 tick × 5ms) */
#define SPD_WIN 10
static int32_t wsum1 = 0, wsum2 = 0;
static int32_t whist1[SPD_WIN], whist2[SPD_WIN];
static uint8_t widx1 = 0, widx2 = 0;



volatile u8 rx_data;
u8 rx_buffer[RX_BUFFER_SIZE];
char spd_buf[16];
int32_t whole;
int32_t frac;
int32_t sgn;
float spd;







/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void FloatToBytes(float value, uint8_t *out_data, uint8_t is_big_endian)
{
    union {
        float f;
        uint32_t u;
        uint8_t b[4];
    } converter;
    
    converter.f = value; // 将浮点数装入联合体，此时联合体内部存储的是小端字节序
    
    if (is_big_endian)
    {
        // 大端：高位在前，低位在后（网络字节序）
        out_data[0] = converter.b[3]; // 最高字节 (MSB)
        out_data[1] = converter.b[2];
        out_data[2] = converter.b[1];
        out_data[3] = converter.b[0]; // 最低字节 (LSB)
    }
    else
    {
        // 小端：低位在前，高位在后（STM32默认字节序）
        out_data[0] = converter.b[0]; // 最低字节 (LSB)
        out_data[1] = converter.b[1];
        out_data[2] = converter.b[2];
        out_data[3] = converter.b[3]; // 最高字节 (MSB)
    }
}

void Int32ToBytes(int32_t value, uint8_t *out)
{
    out[0] = (value >> 24) & 0xFF;
    out[1] = (value >> 16) & 0xFF;
    out[2] = (value >> 8) & 0xFF;
    out[3] = value & 0xFF;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  MX_TIM6_Init();
  //MX_IWDG_Init();
  /* USER CODE BEGIN 2 */
  delay_ms(10);
  bmi088_read_id(&accel_id, &gyro_id);
  delay_ms(10);
  Encoder_Start();
  HAL_TIM_Base_Start_IT(&htim6);
  
  buzzer_start();

  bmi088_yaw_calibrate(1000, 2);
  bmi088_yaw_reset();

  Motor_Init();

  /*启动DMA空闲接收中断,DMA全中断、半中断*/
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, RX_BUFFER_SIZE);
  /*关闭DMA全中断、半中断*/
  __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_TC);

  Motor_Stop();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    //HAL_IWDG_Refresh(&hiwdg);
    if(task_flag == 1)
    {
      /*目标速度(v)、目标角速度(w) 由USART1收到12字节帧后写入, 不再硬编码*/
      yaw_angle = bmi088_yaw_update();  //获取yaw轴角度
      bmi088_read_gyro(gyro);           //获取yaw轴角速度原始值
      yaw_velocity = (float)gyro[2] / 16.4f;   //将yaw轴角速度原始值转换为单位为°/s的角度值

      /* 打印yaw角度字符串 */
/*       {
          float sa = yaw_angle;
          int32_t sgn = (sa < 0) ? -1 : 1;
          float aa = sa * (float)sgn;
          int32_t w = (int32_t)aa;
          int32_t f = (int32_t)((aa - w) * 100.0f + 0.5f);
          char yb[16];
          sprintf(yb, "yaw=%s%d.%02d\r\n", (sgn<0)?"-":"+", w, f);
          HAL_UART_Transmit(&huart1, (uint8_t*)yb, strlen(yb), 10);
      } */
      now1 = -(int16_t)Encoder_Read(&htim3);      //获取编码器读数(int16回绕安全)
      now2 = (int16_t)Encoder_Read(&htim2);

      speed1 = now1 - last1;            //根据5ms的间隔时间及该段时间内检测到的脉冲数计算出脉冲速度
      speed2 = now2 - last2;

      /* 50ms 滑动窗口测速 */
      wsum1 += speed1 - whist1[widx1];  whist1[widx1] = speed1;  widx1 = (widx1 + 1) % SPD_WIN;
      wsum2 += speed2 - whist2[widx2];  whist2[widx2] = speed2;  widx2 = (widx2 + 1) % SPD_WIN;

      /* 窗口平均速度显示 (m/s) 仅测速*/
      spd = (float)wsum2 * WHEEL_CIRC_MM / ((float)ENCODER_PPR * SPD_WIN * 5.0f);
      sgn = (spd < 0) ? -1 : 1;
      float spd_abs = spd * (float)sgn;
      whole = (int32_t)spd_abs;
      frac  = (int32_t)((spd_abs - whole) * 100.0f + 0.5f);
      

      pos1 += (int32_t)speed1;                   //累计脉冲数
      pos2 += (int32_t)speed2;

      last1 = now1;                     
      last2 = now2;

      /* cmd_vel 差速解算: v(线速度), w(角速度) → 左右轮 → 每轮独立速度环
       * 反馈用50ms窗口平均速度(每5ms脉冲), 避免低速时瞬时脉冲为0导致PID失效
       * 缓加减速: 仅线速度每控制步限幅, 角速度直接用命令 */
      {
        #define ACCEL_LIMIT    1.0f   /* 线加速度 m/s² */
        #define V_MAX          0.8f   /* 线速度上限 m/s */
        static float v_ramp = 0;

        float d_v = target_speed - v_ramp;
        if (d_v >  ACCEL_LIMIT * 0.005f) d_v =  ACCEL_LIMIT * 0.005f;
        if (d_v < -ACCEL_LIMIT * 0.005f) d_v = -ACCEL_LIMIT * 0.005f;
        v_ramp += d_v;
        if (v_ramp >  V_MAX) v_ramp =  V_MAX;
        if (v_ramp < -V_MAX) v_ramp = -V_MAX;

        float v_left  = v_ramp - target_gyro * (WHEELBASE_MM / 2000.0f);
        float v_right = v_ramp + target_gyro * (WHEELBASE_MM / 2000.0f);

        float fb1 = (float)wsum1 / SPD_WIN;   /* 窗口平均 → 每5ms脉冲 */
        float fb2 = (float)wsum2 / SPD_WIN;

        pwm_fwd  = Velocity1(mps_to_tick(v_left),  fb1);   /* 轮1 = htim3 */
        pwm_fwd2 = Velocity2(mps_to_tick(v_right), fb2);   /* 轮2 = htim2 */

        //HAL_UART_Transmit(&huart1, (u8 *)&target_speed, 4, 10);

        M_setspeed(pwm_fwd, pwm_fwd2);
      }
      task_flag = 0;
    }

    if(reset == 1)
    {
      target_speed = 0;
      target_gyro = 0;
      Motor_Stop();
      reset = 0;
    }

    if(request_odom == 1)
    {
      //HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
      /*发送实时yaw轴角度以及左右轮速度，以字节格式*/
      u8 send_buff[14];
      send_buff[0] = 0xA1;
      FloatToBytes(yaw_angle, &send_buff[1],1);
      Int32ToBytes(pos1, &send_buff[5]);
      Int32ToBytes(pos2, &send_buff[9]);
      send_buff[13] = 0xB1;
      HAL_UART_Transmit(&huart1, send_buff, 14, 10);
/*       sprintf(spd_buf, "%s%d.%02d\r\n", (sgn<0)?"-":"", whole, frac);
      HAL_UART_Transmit(&huart1, (uint8_t*)spd_buf, strlen(spd_buf), 10); */
      /* 调试: 打印目标/实测/PWM/脉冲增量 */
/*       sprintf(spd_buf, "t=%.2f spd=%.2f pwm=%d,%d s1=%d s2=%d\r\n",
              (double)target_speed, (double)spd, (int)pwm_fwd, (int)pwm_fwd2,
              (int)speed1, (int)speed2);
      HAL_UART_Transmit(&huart1, (uint8_t*)spd_buf, strlen(spd_buf), 10); */
      request_odom = 0;
    }

    if(request_voltage == 1)/* 发送电池电压 */
    {
      float v = voltage_read_battery();
      u8 vb[6];
      vb[0] = 0xA2;
      FloatToBytes(v, &vb[1], 1);
      vb[5] = 0xB2;
      HAL_UART_Transmit(&huart1, vb, 6, 10);
      request_voltage = 0;
    }

    
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
