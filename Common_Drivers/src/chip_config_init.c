#include<stdint.h>
#include "chip_config_init.h"

void chip_clock_config()
{
    RCC_CR |= (1U << 8);          // HSION. enable HSI clock. 16MHz
    while(!(RCC_CR & (1U << 10))); // HSIRDY. wait for HSI clock to be ready
    while((RCC_CFGR & 0xCU) != 0x0U); //verifying that HSI is actually the system clock source.

}

