#include <stdint.h>
#include "blinky.h"
#include "chip_config_init.h"
#include "boot_status.h"
#include "ringbuffer.h"
#include "uart.h"

static inline void __enable_irq(void) {
    __asm volatile ("cpsie i");
}

int main(){
    
    chip_clock_config();//done in bootloader, so no need to do it again in app (I guesss...)
    gpio_portA_init();
    tim3_polling_init();
    rb_init(&rb, ring, RING_BUFFER_SIZE);
    uart_init(16000000, 115200);
    uart_interrupt_init();
    __enable_irq(); // Enable global interrupts

    backup_domain_unlock();
    boot_status_write(BOOT_MAGIC_CONFIRMED);  // "I'm alive, don't roll me back"

    while(1)
    {       
            IWDG_KR = 0xAAAA;
            uart_transmit_byte('A');
            *(volatile uint32_t*)(0x50000014) |= (1<<5); // set PA5
            delay_ms(1000);
            *(volatile uint32_t*)(0x50000014) &= ~(1<<5); // clear PA5
            delay_ms(1000);
    }
}