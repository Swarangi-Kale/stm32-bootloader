#ifndef BOOT_STATUS_H
#define BOOT_STATUS_H

#include <stdint.h>

#define BOOT_STATUS_REG   (*(volatile uint32_t*)(0x4000B100))   // dedicate register 0 to boot status


#define BOOT_MAGIC_PENDING    0xB00710AD  // written by bootloader just before jumping
#define BOOT_MAGIC_CONFIRMED  0xC0FFEE00  // written by app once it's alive
#define BOOT_MAGIC_FAILED     0xFA17ED00  // optional: written by fault handler

#define RCC_APBENR1 (*(volatile uint32_t*)(0x4002103C))
#define PWR_CR1 (*(volatile uint32_t*)(0x40007000))

#define IWDG_KR (*(volatile uint32_t*)(0x40003000))
#define IWDG_PR (*(volatile uint32_t*)(0x40003004))
#define IWDG_RLR (*(volatile uint32_t*)(0x40003008))
#define IWDG_SR (*(volatile uint32_t*)(0x4000300C))

static inline void backup_domain_unlock(void) {
    RCC_APBENR1 |= (1<<28);   // enable PWR clock
    PWR_CR1 |= (1<<8);             // disable backup domain write protection
    RCC_APBENR1 |= (1<<10); // enable RTC/TAMP bus clock
}

static inline void boot_status_write(uint32_t status) {
    backup_domain_unlock();
    BOOT_STATUS_REG = status;
}

static inline uint32_t boot_status_read(void) {
    return BOOT_STATUS_REG;
}

#endif