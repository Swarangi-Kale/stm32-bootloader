#include<stdint.h>
#include <stdbool.h>
#include "flash.h"

#define TEST_FLASH_ADDR 0x0801F800 /*Main memory of Flash - last page*/

uint64_t test_values[4] = {0x1234567891011123, 0xabcdef, 0x123456789ABCDEF0, 0xFEDCBA9876543210};

int main(){
    uint64_t flash_pg_data;
    read_flash_line(&flash_pg_data, 0x40022014);
    
    bool whether_true = flash_unlock();
    read_flash_line(&flash_pg_data, 0x40022014);

    erase_flash_page();

    FLASH_STATUS status = write_flash_line(test_values[0], TEST_FLASH_ADDR);

    return 0;
}