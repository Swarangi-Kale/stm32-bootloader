#include <stdint.h>
#include <stdbool.h>
#include "chip_config_init.h"
#include "uart.h"
#include "comms.h"
#include "blinky.h"

// #define FLASH_BASE_ADDR   (0x08000000U)
// #define BOOTLOADER_SIZE   (0x8000U)
// #define APP_START_ADDR    (FLASH_BASE_ADDR + BOOTLOADER_SIZE)
#define CLK_SPEED         16000000U
#define BAUD_RATE         115200U

static inline void system_enable_irq(void)
{
    __asm volatile ("cpsie i");
}

int main(void)
{
    chip_clock_config();
    gpio_portA_init();
    tim3_polling_init();
    rb_init(&rb, ring, RING_BUFFER_SIZE);
    uart_init(CLK_SPEED, BAUD_RATE);
    uart_interrupt_init();
    system_enable_irq();
    comms_setup();
    uart_transmit_byte('S');

    comms_packet_t incoming_packet;
    uint8_t *raw = (uint8_t *)&incoming_packet;
    int i = 0;

    while (true) {
        if (!rb_isEmpty(&rb)) {
            rb_pop(&rb, &raw[i]);
            i++;
        }

        if (i >= PACKET_TOTAL_SIZE) {
            comms_update(&incoming_packet);
            i = 0;
        }
    }

    return 0;
}