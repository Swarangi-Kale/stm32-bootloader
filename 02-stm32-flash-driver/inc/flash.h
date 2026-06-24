#include <stdint.h>

#ifndef FLASH_H_
#define FLASH_H_

#define FLASH_ORIGIN 0x40022000
#define FLASH_KEYR 0x40022008
#define FLASH_SR 0x40022010
#define FLASH_CR 0x40022014

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
void erase_flash_page();
FLASH_STATUS write_flash_line(uint64_t, uint32_t);
#endif /* FLASH_H_ */