#include <stdint.h>
#include "blinky.h"

#define RCC_CR (*(volatile uint32_t*)0x40021000)
#define RCC_CFGR (*(volatile uint32_t*)0x40021008)

void chip_clock_config(void); /*HSI is the default after reset. So technically don't need this since i am using the original HSI clock itself. But created it for forward compatibility when I need to change the clock source */

int main(){
    
    chip_clock_config();
    gpio_portA_init();
    tim3_polling_init();

    while(1)
    {
            *(volatile uint32_t*)(0x50000014) |= (1<<5); // set PA5
            // delay_ms(100);
            // *(volatile uint32_t*)(0x50000014) &= ~(1<<5); // clear PA5
            // delay_ms(100);
    }
}

void chip_clock_config(void)
{
    RCC_CR |= (1U << 8);          // HSION. enable HSI clock
    while(!(RCC_CR & (1U << 10))); // HSIRDY. wait for HSI clock to be ready
    while((RCC_CFGR & 0xCU) != 0x0U); //verifying that HSI is actually the system clock source.

}