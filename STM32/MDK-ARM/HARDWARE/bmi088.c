/*
 * bmi088.c
 * 自包含软件 SPI 驱动 (STM32F103), 引脚控制使用位带操作 (sys.h)
 * BMI088: 加速度计 BMI08A + 陀螺仪 BMI08G, 共用一条 SPI 总线, 各自独立片选
 * SPI 模式 0 (CPOL=0, CPHA=0), MSB first
 */
#include "bmi088.h"

static float bmi088_yaw_angle;
static float bmi088_gyro_z_bias;
static uint32_t bmi088_yaw_last_tick;
static uint8_t bmi088_yaw_started;

/* 微秒延时: 空循环粗略延时, 按 72MHz 估算. 如需精确可改用 SysTick/DWT */
static void bmi088_delay_us(uint32_t us)
{
    volatile uint32_t n = us * 8;
    while (n--) {
        __NOP();
    }
}

/* SPI 引脚模式配置: PA5/PA7 推挽输出, PA6 浮空输入, PC4/PC5 推挽输出
 * (位带宏只能读写单 bit, 引脚方向仍用 GPIO_Init 配置) */
static void bmi088_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* SCLK / MOSI 输出 */
    gpio.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* MISO 输入 */
    gpio.Pin = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* 片选输出 */
    gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* 空闲状态: 片选拉高, 时钟低 */
    ACCEL_CS = 1;
    GYRO_CS = 1;
    BMI088_SCLK = 0;
}

/* 软件 SPI 收发一个字节 */
static uint8_t bmi088_spi_rw(uint8_t byte)
{
    uint8_t i, rx = 0;

    for (i = 0; i < 8; i++) {
        BMI088_MOSI = (byte & 0x80) ? 1 : 0;
        byte <<= 1;
        bmi088_delay_us(1);

        BMI088_SCLK = 1;
        bmi088_delay_us(1);
        rx <<= 1;
        if (BMI088_MISO) {
            rx |= 0x01;
        }
        bmi088_delay_us(1);

        BMI088_SCLK = 0;
        bmi088_delay_us(1);
    }
    return rx;
}

/*
 * 读寄存器
 * is_accel: 1=加速度计 0=陀螺仪
 * 加速度计读操作在地址字节后需要一个 dummy 字节, 陀螺仪不需要
 */
static void bmi088_read_reg(uint8_t reg, uint8_t *buf, uint16_t len, uint8_t is_accel)
{
    uint16_t i;

    if (is_accel) {
        ACCEL_CS = 0;
    } else {
        GYRO_CS = 0;
    }

    /* BMI088 requires a short CS setup time before the address byte. */
    bmi088_delay_us(2);

    bmi088_spi_rw(reg | 0x80);          /* 读命令 (最高位置 1) */
    if (is_accel) {
        bmi088_spi_rw(0x00);            /* 加速度计读需 dummy 字节 */
    }
    for (i = 0; i < len; i++) {
        buf[i] = bmi088_spi_rw(0x00);
    }

    if (is_accel) {
        ACCEL_CS = 1;
    } else {
        GYRO_CS = 1;
    }

    bmi088_delay_us(2);
}

/* 写寄存器 (两次写之间间隔至少 2us) */
static void bmi088_write_reg(uint8_t reg, uint8_t data, uint8_t is_accel)
{
    if (is_accel) {
        ACCEL_CS = 0;
    } else {
        GYRO_CS = 0;
    }

    bmi088_spi_rw(reg);                 /* 写命令 (最高位为 0) */
    bmi088_spi_rw(data);

    if (is_accel) {
        ACCEL_CS = 1;
    } else {
        GYRO_CS = 1;
    }

    bmi088_delay_us(2);
}

/* 读加速度计原始数据 */
void bmi088_read_accel(int16_t accel[3])
{
    uint8_t buf[6];

    bmi088_read_reg(BMI088_ACC_DATA, buf, 6, 1);
    accel[0] = (int16_t)(buf[0] | (buf[1] << 8));
    accel[1] = (int16_t)(buf[2] | (buf[3] << 8));
    accel[2] = (int16_t)(buf[4] | (buf[5] << 8));
}

/* 读陀螺仪原始数据 */
void bmi088_read_gyro(int16_t gyro[3])
{
    uint8_t buf[6];

    bmi088_read_reg(BMI088_GYRO_DATA, buf, 6, 0);
    gyro[0] = (int16_t)(buf[0] | (buf[1] << 8));
    gyro[1] = (int16_t)(buf[2] | (buf[3] << 8));
    gyro[2] = (int16_t)(buf[4] | (buf[5] << 8));
}

void bmi088_read_all(bmi088_data_t *data)
{
    bmi088_read_accel(data->accel);
    bmi088_read_gyro(data->gyro);
}

/* 读芯片 ID: 加速度计 0x1E, 陀螺仪 0x0F */
void bmi088_read_id(uint8_t *accel_id, uint8_t *gyro_id)
{
    bmi088_read_reg(BMI088_ACC_CHIP_ID, accel_id, 1, 1);
    bmi088_read_reg(BMI088_GYRO_CHIP_ID, gyro_id, 1, 0);
}

/* 初始化 */
uint8_t bmi088_init(void)
{
    uint8_t id;

    bmi088_gpio_init();

    /* 加速度计软复位 */
    bmi088_write_reg(BMI088_ACC_SOFTRESET, 0xB6, 1);
    HAL_Delay(50);

    /* 陀螺仪软复位 */
    bmi088_write_reg(BMI088_GYRO_SOFTRESET, 0xB6, 0);
    HAL_Delay(30);

    /* 加速度计: 配置为工作模式 */
    bmi088_write_reg(BMI088_ACC_PWR_CONF, 0x00, 1);     /* 0x00=active */
    HAL_Delay(5);
    bmi088_write_reg(BMI088_ACC_PWR_CTRL, 0x04, 1);     /* 使能加速度计 */
    HAL_Delay(50);
    bmi088_write_reg(BMI088_ACC_RANGE, 0x00, 1);        /* 量程 ±3g */
    bmi088_write_reg(BMI088_ACC_CONF, 0xA8, 1);         /* ODR 800Hz */

    /* 陀螺仪: 正常工作模式 */
    bmi088_write_reg(BMI088_GYRO_LPM1, 0x00, 0);        /* normal mode */
    HAL_Delay(30);
    bmi088_write_reg(BMI088_GYRO_RANGE, 0x00, 0);       /* 量程 ±2000dps */
    bmi088_write_reg(BMI088_GYRO_BANDWIDTH, 0x81, 0);   /* ODR 1000Hz */

    /* 校验芯片 ID */
    bmi088_read_reg(BMI088_ACC_CHIP_ID, &id, 1, 1);
    if (id != BMI088_ACC_CHIP_ID_VALUE) {
        return 1;
    }

    bmi088_read_reg(BMI088_GYRO_CHIP_ID, &id, 1, 0);
    if (id != BMI088_GYRO_CHIP_ID_VALUE) {
        return 1;
    }

    return 0;
}

uint8_t bmi088_yaw_calibrate(uint16_t samples, uint16_t interval_ms)
{
    int16_t gyro[3];
    int32_t sum = 0;
    uint16_t i;

    if (samples == 0U) return 1;
    for (i = 0; i < samples; i++) {
        bmi088_read_gyro(gyro);
        sum += gyro[2];
        HAL_Delay(interval_ms);
    }
    bmi088_gyro_z_bias = (float)sum / (float)samples;
    bmi088_yaw_reset();
    return 0;
}

void bmi088_yaw_reset(void)
{
    bmi088_yaw_angle = 0.0f;
    bmi088_yaw_last_tick = HAL_GetTick();
    bmi088_yaw_started = 1U;
}

float bmi088_yaw_update(void)
{
    int16_t gyro[3];
    uint32_t now;
    float dt;

    if (!bmi088_yaw_started) bmi088_yaw_reset();
    now = HAL_GetTick();
    dt = (float)(now - bmi088_yaw_last_tick) * 0.001f;
    bmi088_yaw_last_tick = now;
    bmi088_read_gyro(gyro);
    bmi088_yaw_angle += ((float)gyro[2] - bmi088_gyro_z_bias) / 16.4f * dt;
    while (bmi088_yaw_angle >= 360.0f) bmi088_yaw_angle -= 360.0f;
    while (bmi088_yaw_angle < 0.0f) bmi088_yaw_angle += 360.0f;
    return bmi088_yaw_angle;
}

float bmi088_yaw_get(void)
{
    return bmi088_yaw_angle;
}

/* 巡检: 若传感器挂死(读ID异常/0xFF)则完整重新初始化, 返回0=正常 */
uint8_t bmi088_check_and_recover(void)
{
    uint8_t id;

    bmi088_read_reg(BMI088_GYRO_CHIP_ID, &id, 1, 0);
    if (id != BMI088_GYRO_CHIP_ID_VALUE)
    {
        bmi088_read_reg(BMI088_ACC_CHIP_ID, &id, 1, 1);
        if (id != BMI088_ACC_CHIP_ID_VALUE)
        {
            return bmi088_init();   /* 双ID都异常 → 重初始化 */
        }
    }
    return 0;
}
