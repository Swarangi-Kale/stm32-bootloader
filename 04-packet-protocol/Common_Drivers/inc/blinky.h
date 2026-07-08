#include <stdint.h>

#ifndef BLINKY_H_
#define BLINKY_H_

#define TOGGLE_LED_PA5 (*(volatile uint32_t*)(0x50000014) ^= (1<<5))

void gpio_portA_init();
void tim3_polling_init(void);
void delay_us(uint16_t us);
void delay_ms(uint16_t ms);

#endif /* BLINKY_H_ */