#include<stdint.h>
#include <stdbool.h>

#include "flash.h"

bool flash_unlock(){
    if(*(volatile uint32_t*)FLASH_CR & (1<<31)){
        *(volatile uint32_t*)FLASH_KEYR = 0x45670123; /*Key 1*/
        *(volatile uint32_t*)FLASH_KEYR =  0xCDEF89AB; /*Key 2*/
    }
    if(*(volatile uint32_t*)FLASH_CR & (1<<31)){
        return false; /*Unlock failed*/
    }
    return true; /*Unlock successful*/
}

bool flash_lock(){
    *(volatile uint32_t*)FLASH_CR |= (1<<31);
    if(*(volatile uint32_t*)FLASH_CR & (1<<31)){
        return true; /*Lock successful*/
    }
    return false; /*Lock failed*/
}

void read_flash_line(uint64_t* flash_pg_data, uint32_t flash_addr){
    *flash_pg_data = *(uint64_t*)flash_addr;
}

void erase_flash_page(uint8_t page_number){
    if((*(volatile uint32_t*)FLASH_SR & (1<<18)) == 0){ /*Check if flash is not currently being modified with a erase or write pgm. CFGBSY flag*/
        *(volatile uint32_t*)FLASH_CR |= (1<<1); /*Set PER bit to enable page erase*/

        *(volatile uint32_t*)FLASH_CR &= ~(0xFF<<3); //clear the page number bits in the CR register
        *(volatile uint32_t*)FLASH_CR |= (page_number<<3); /*Set the page number to be erased. Here, 0x3F is the last page of flash memory*/
        *(volatile uint32_t*)FLASH_CR |= (1<<16); /*Set STRT bit to start the page erase operation*/
        while((*(volatile uint32_t*)FLASH_SR & (1<<18)) != 0); /*Wait for the page erase operation to complete. CFGBSY flag*/
        *(volatile uint32_t*)FLASH_CR &= ~(1<<1); /*Clear PER bit to disable page erase*/
    } 
}

void erase_main_app(){
    flash_unlock(); // need to unlock flash before writing to FLASH_CR register
    for(uint8_t page_num = 16; page_num <= 30; page_num++){
        erase_flash_page(page_num);
    }
    flash_lock();

}

FLASH_STATUS write_flash_line(uint64_t* flash_pg_data, uint32_t flash_addr){
    flash_clear_errors();
    while((*(volatile uint32_t*)FLASH_SR & (1<<16)) != 0);/*Wait for the programming operation to complete. BSY1 flag*/

    // *(volatile uint32_t*)FLASH_CR |= (1<<24); /*set the EOPIE bit to later use the EOP flag*/
    *(volatile uint32_t*)FLASH_CR |= (1<<0); /*Set PG bit to enable programming*/

    /*since the cortex-M0+ has 32 bit address bus, writing the 64 bit data (length of flash memory line) as two words*/
    *(volatile uint32_t*)flash_addr = (uint32_t)*flash_pg_data; /*Write the lower 32 bits */
    *(volatile uint32_t*)(flash_addr + 4) = (uint32_t)(*flash_pg_data >> 32); /*Write the upper 32 bits */

    while((*(volatile uint32_t*)FLASH_SR & (1<<16)) != 0); /*Wait for the programming operation to complete. BSY1 flag*/

    // while((*(volatile uint32_t*)FLASH_SR & (1<<0)) != 1); /*wait till the EOP flag is set*/
    // *(volatile uint32_t*)FLASH_SR &= ~(1<<0); /*Clear the EOP flag by software*/

    *(volatile uint32_t*)FLASH_CR &= ~(1<<0); /*Clear PG bit to disable programming*/

    /*check for errors*/

    uint32_t sr = *(volatile uint32_t*)FLASH_SR;
    if(sr & (1<<1)) return OPERR; /*Operation error*/
    if(sr & (1<<3)) return PROGERR; /*Programming error*/
    if(sr & (1<<4)) return WRPERR; /*Write protection error*/
    if(sr & (1<<5)) return PGAERR; /*Programming alignment error*/
    if(sr & (1<<6)) return SIZERR; /*Size error*/
    if(sr & (1<<7)) return PGSERR; /*Programming sequence error*/
    if(sr & (1<<8)) return MISSERR; /*Missing address error*/
    if(sr & (1<<9)) return FASTERR; /*Fast programming error*/

    return FLASH_OK; /*No errors*/
}

bool flash_program(uint32_t flash_addr, uint8_t* data, uint16_t length){ //length is in "number of bytes" of data
    uint8_t flash_lines_total = length/FLASH_LINE_LENGTH;
    for(uint8_t i = 0; i < flash_lines_total; i++){
        if(write_flash_line((uint64_t*)(data + 8*i), (flash_addr + i*8)) != FLASH_OK){
            return false;
        }
    }
    return true;
}

void flash_clear_errors(void){
    /* Write 1 to clear each error flag in FLASH_SR */
    *(volatile uint32_t*)FLASH_SR |= (1<<1)  | /*OPERR*/
                                      (1<<3)  | /*PROGERR*/
                                      (1<<4)  | /*WRPERR*/
                                      (1<<5)  | /*PGAERR*/
                                      (1<<6)  | /*SIZERR*/
                                      (1<<7)  | /*PGSERR*/
                                      (1<<8)  | /*MISSERR*/
                                      (1<<9);   /*FASTERR*/
}