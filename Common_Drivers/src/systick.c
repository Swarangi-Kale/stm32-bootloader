#include "systick.h"

uint64_t ticks = 0;


int systick_calc_reload_ticks(uint16_t tick_freq_hz, uint32_t* reload_ticks){

    if(tick_freq_hz == 0 || reload_ticks == NULL){
        return -2;
    }

    uint32_t reload_ticks_internal = SYSTEM_CLOCK_CORE/tick_freq_hz;
    
    if(reload_ticks_internal == 0 || reload_ticks_internal > SYST_RVR_MAX){
        return -1; //freq too high or too low
    }

    *reload_ticks = reload_ticks_internal;
    return 0;
}

int systick_init() {
    uint32_t reload_ticks = 0;
    
    /*Frequency of systick = 1kHz or time period = 1ms*/
    int whether_successful = systick_calc_reload_ticks(1000, &reload_ticks);

    if(reload_ticks == 0 || reload_ticks > SYST_RVR_MAX){
        return -1; //freq too high or too low
    }
    SYST_CSR &= ~(1<<0); //disable when configuring
    SYST_RVR = (reload_ticks - 1) & SYST_RVR_MAX; // 24-bit max
    SYST_CVR = 0;                                // clear current value
    SYST_CSR = (1 << 2) | (1 << 1) | (1 << 0);   // CLKSOURCE | TICKINT | ENABLE

    return 0;
}

void systick_stop(){
    SYST_CSR = 0; // needed before bootloader->app jump
}

void SysTick_Handler(){
    ticks++;
}

uint64_t systick_get_ticks(){
    return ticks;
}