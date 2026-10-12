#ifndef __SYS_H__
#define __SYS_H__

#include "stm32f1xx_hal.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef unsigned char uchar;

//位带操作: 把 (addr, bitnum) 转换为位带别名区地址并取内容
#define BITBAND(addr, bitnum)  ((addr & 0xF0000000) + 0x2000000 + ((addr & 0xFFFFF) << 5) + (bitnum << 2))
#define MEM_ADDR(addr)         (*(volatile unsigned long *)(addr))
#define BIT_ADDR(addr, bitnum) MEM_ADDR(BITBAND(addr, bitnum))

//ODR为输出寄存器，偏移地址为 0x0C
#define GPIOA_ODR_Addr    (GPIOA_BASE+0x0C)  // 0x4001080C
#define GPIOB_ODR_Addr    (GPIOB_BASE+0x0C)  // 0x40010C0C
#define GPIOC_ODR_Addr    (GPIOC_BASE+0x0C)  // 0x4001100C
#define GPIOD_ODR_Addr    (GPIOD_BASE+0x0C)  // 0x4001140C
#define GPIOE_ODR_Addr    (GPIOE_BASE+0x0C)  // 0x4001180C

//IDR为输入寄存器，偏移地址为 0x08
#define GPIOA_IDR_Addr    (GPIOA_BASE+0x08)  // 0x40010808
#define GPIOB_IDR_Addr    (GPIOB_BASE+0x08)  // 0x40010C08
#define GPIOC_IDR_Addr    (GPIOC_BASE+0x08)  // 0x40011008
#define GPIOD_IDR_Addr    (GPIOD_BASE+0x08)  // 0x40011408
#define GPIOE_IDR_Addr    (GPIOE_BASE+0x08)  // 0x40011808

#define PAout(n)    BIT_ADDR(GPIOA_ODR_Addr, n)
#define PAin(n)     BIT_ADDR(GPIOA_IDR_Addr, n)

#define PBout(n)    BIT_ADDR(GPIOB_ODR_Addr,n)  //输出 
#define PBin(n)     BIT_ADDR(GPIOB_IDR_Addr,n)  //输入 

#define PCout(n)    BIT_ADDR(GPIOC_ODR_Addr,n)  //输出 
#define PCin(n)     BIT_ADDR(GPIOC_IDR_Addr,n)  //输入 

#define PDout(n)    BIT_ADDR(GPIOD_ODR_Addr,n)  //输出 
#define PDin(n)     BIT_ADDR(GPIOD_IDR_Addr,n)  //输入 

#define PEout(n)    BIT_ADDR(GPIOE_ODR_Addr,n)  //输出 
#define PEin(n)     BIT_ADDR(GPIOE_IDR_Addr,n)  //输入



#endif

