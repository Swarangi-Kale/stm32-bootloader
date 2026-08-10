#include <stdint.h>

#ifndef FLASH_H_
#define FLASH_H_

#define FLASH_ORIGIN 0x40022000
#define FLASH_KEYR 0x40022008
#define FLASH_SR 0x40022010
#define FLASH_CR 0x40022014

#define FLASH_LINE_LENGTH 8 //bytes (64 bits). can write only 4bytes in one go

typedef enum{
    FLASH_OK = 0,
    OPERR,
    PROGERR,
    WRPERR,
    PGAERR,
    SIZERR,
    PGSERR,
    MISSERR,
    FASTERR
} FLASH_STATUS;

bool flash_unlock();
bool flash_lock();
void read_flash_line(uint64_t*, uint32_t);
void erase_flash_page(uint8_t page_number);
void erase_main_app();
FLASH_STATUS write_flash_line(uint64_t*, uint32_t);
bool flash_program(uint32_t flash_addr, uint8_t* data, uint16_t length);
void flash_clear_errors(void);
#endif /* FLASH_H_ */