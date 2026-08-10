#include <stdint.h>
// #include "stm32g0xx.h"
#include "blinky.h"

#define RCC_APBENR1 (*(volatile uint32_t*)0x4002103C)

#define TIM3_CR1 (*(volatile uint32_t*)0x40000400)
#define TIM3_EGR (*(volatile uint32_t*)0x40000414)
#define TIM3_CNT (*(volatile uint32_t*)0x40000424)
#define TIM3_PSC (*(volatile uint32_t*)0x40000428)
#define TIM3_ARR (*(volatile uint32_t*)0x4000042C)


void gpio_portA_init(){
	  *(volatile uint32_t*)(0x40021034) |= 0x00000001; // RCC clock enable to port a

	  *(volatile uint32_t*)(0x50000000) &= ~(3<<10); // setting bits 11:10 both to 0
	  *(volatile uint32_t*)(0x50000000) |= (1<<10); // setting bit 10 to 1 so we have (11:10 = 01)
}

void tim3_polling_init(void)
{
    RCC_APBENR1 |= (1 << 1);     // TIM3 clock enable
    TIM3_CR1 &= ~(1 << 0);       // counter off
    TIM3_PSC = 15;               // 16MHz -> 1MHz
    TIM3_ARR = 0xFFFF;
    TIM3_EGR = 1;                // load PSC immediately
    TIM3_CNT = 0;
    TIM3_CR1 |= (1 << 0);        // counter on
}

void delay_us(uint16_t us)
{
	*(volatile uint32_t*)(0x40000424) = 0;
    while((*(volatile uint32_t*)(0x40000424)) < us);
}

void delay_ms(uint16_t ms)
{
    while(ms--)
    {
        delay_us(1000);  // 1000 ticks = 1000us = 1ms at 1MHz TIM3
    }
}