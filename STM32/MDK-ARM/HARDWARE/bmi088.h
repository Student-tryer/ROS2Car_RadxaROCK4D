#ifndef __BMI088_H__
#define __BMI088_H__

#include "sys.h"

/* ==================== 引脚定义 (软件 SPI, 位带操作) ==================== */
#define ACCEL_CS    PCout(4)    /* 加速度计片选 */
#define GYRO_CS     PCout(5)    /* 陀螺仪片选   */
#define BMI088_MOSI PAout(7)    /* MOSI 输出     */
#define BMI088_MISO PAin(6)     /* MISO 输入     */
#define BMI088_SCLK PAout(5)    /* 时钟输出      */

/* ==================== 引脚操作宏 ==================== */
/* 片选 */
#define ACCEL_CS_LOW()    (ACCEL_CS = 0)
#define ACCEL_CS_HIGH()   (ACCEL_CS = 1)
#define GYRO_CS_LOW()     (GYRO_CS = 0)
#define GYRO_CS_HIGH()    (GYRO_CS = 1)

/* 时钟 */
#define BMI088_SCLK_LOW() (BMI088_SCLK = 0)
#define BMI088_SCLK_HIGH()(BMI088_SCLK = 1)

/* MOSI */
#define BMI088_MOSI_LOW() (BMI088_MOSI = 0)
#define BMI088_MOSI_HIGH()(BMI088_MOSI = 1)

/* MISO 读取 */
#define BMI088_MISO_READ()  (BMI088_MISO)

/* 便捷选择: 选择加速度计/陀螺仪片选 */
#define BMI088_CS_SELECT(is_accel)   do { if (is_accel) { ACCEL_CS_LOW(); } else { GYRO_CS_LOW(); } } while (0)
#define BMI088_CS_RELEASE(is_accel)  do { if (is_accel) { ACCEL_CS_HIGH(); } else { GYRO_CS_HIGH(); } } while (0)

/* ==================== 寄存器地址 (见 BMI088 数据手册) ==================== */
/* 加速度计 BMI08A */
#define BMI088_ACC_CHIP_ID      0x00
#define BMI088_ACC_ERR_REG      0x02
#define BMI088_ACC_STATUS       0x03
#define BMI088_ACC_DATA         0x12    /* 6 字节: X_L X_H Y_L Y_H Z_L Z_H */
#define BMI088_ACC_CONF         0x40
#define BMI088_ACC_RANGE        0x41
#define BMI088_ACC_PWR_CONF     0x7C
#define BMI088_ACC_PWR_CTRL     0x7D
#define BMI088_ACC_SOFTRESET    0x7E

/* 陀螺仪 BMI08G */
#define BMI088_GYRO_CHIP_ID     0x00
#define BMI088_GYRO_DATA        0x02    /* 6 字节: X_L X_H Y_L Y_H Z_L Z_H */
#define BMI088_GYRO_RANGE       0x0F
#define BMI088_GYRO_BANDWIDTH   0x10
#define BMI088_GYRO_LPM1        0x11
#define BMI088_GYRO_SOFTRESET   0x14

/* 芯片 ID */
#define BMI088_ACC_CHIP_ID_VALUE   0x1E
#define BMI088_GYRO_CHIP_ID_VALUE  0x0F

/* ==================== 数据结构 ==================== */
typedef struct {
    int16_t accel[3];   /* x y z, 单位 LSB */
    int16_t gyro[3];    /* x y z, 单位 LSB */
} bmi088_data_t;

/* ==================== 接口 ==================== */
uint8_t bmi088_init(void);                              /* 返回 0 成功, 非 0 失败 */
uint8_t bmi088_check_and_recover(void);                 /* 巡检, 挂死时自动重初始化 */
void    bmi088_read_id(uint8_t *accel_id, uint8_t *gyro_id); /* 读加速度/陀螺仪芯片 ID */
void    bmi088_read_accel(int16_t accel[3]);
void    bmi088_read_gyro(int16_t gyro[3]);
void    bmi088_read_all(bmi088_data_t *data);
uint8_t bmi088_yaw_calibrate(uint16_t samples, uint16_t interval_ms);
void    bmi088_yaw_reset(void);
float   bmi088_yaw_update(void);
float   bmi088_yaw_get(void);

#endif
