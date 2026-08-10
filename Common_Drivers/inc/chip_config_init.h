#ifndef INC_CHIP_CONFIG_INIT_H
#define INC_CHIP_CONFIG_INIT_H

#include<stdio.h>

#define RCC_CR (*(volatile uint32_t*)0x40021000)
#define RCC_CFGR (*(volatile uint32_t*)0x40021008)

#define SYSTEM_CLOCK_CORE 16000000 //HSI

void chip_clock_config(); /*HSI is the default after reset. So technically don't need this since i am using the original HSI clock itself. But created it for forward compatibility when I need to change the clock source */

#endif /* INC_CHIP_CONFIG_INIT_H */