#include <stdint.h>
#include "blinky.h"
#include "chip_config_init.h"


static inline void __enable_irq(void) {
    __asm volatile ("cpsie i");
}

int main(){
    
    chip_clock_config();//done in bootloader, so no need to do it again in app (I guesss...)
    gpio_portA_init();
    tim3_polling_init();
    __enable_irq(); // Enable global interrupts

    while(1)
    {
            *(volatile uint32_t*)(0x50000014) |= (1<<5); // set PA5
            delay_ms(1000);
            *(volatile uint32_t*)(0x50000014) &= ~(1<<5); // clear PA5
            delay_ms(1000);
    }
}