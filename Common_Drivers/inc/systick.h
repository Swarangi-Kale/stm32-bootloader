#ifndef INC_SYSTICK_CONFIG_INIT_H
#define INC_SYSTICK_CONFIG_INIT_H

#include<stdio.h>
#include <stdint.h>
#include "chip_config_init.h"

#define SYST_CSR   (*(volatile uint32_t*)0xE000E010)
#define SYST_RVR   (*(volatile uint32_t*)0xE000E014)
#define SYST_CVR   (*(volatile uint32_t*)0xE000E018)

#define SYST_RVR_MAX        0x00FFFFFFUL   // 24-bit reload max

int systick_calc_reload_ticks(uint16_t tick_freq_hz, uint32_t* reload_ticks);
int systick_init();
void systick_stop();
void SysTick_Handler();
uint64_t systick_get_ticks();


#endif // INC_SYSTICK_CONFIG_INIT_H
