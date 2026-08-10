#include <stdint.h>
#include "bl_jump.h"
#include "app_header.h"
#include "mem_layout.h"

#define APP_MAGIC_NUMBER 0xDEADBEEF

void jump_to_app(){
    typedef void (*void_fn)(void);

    uint32_t* app_reset_vector_entry = (uint32_t*)(APP_START_ADDR + 4U);
    uint32_t* reset_vector = (uint32_t*)*app_reset_vector_entry;

    void_fn jump_fn = (void_fn)reset_vector;

    jump_fn();
}

int sp_pc_sanity_check(){
    uint32_t app_sp = *(uint32_t*)APP_START_ADDR;
    uint32_t app_pc = *(uint32_t*)(APP_START_ADDR + 4U);
    
    if((app_sp < SRAM_START_ADDR) || (app_sp > SRAM_END_ADDR)){
        return 1; // Invalid SP
    }

    if(app_pc < APP_START_ADDR || app_pc > (FLASH_MEM_END)){ //Thumb bit is set in pc since cortex M0+ uses thumb instruction set
        return 2; // Invalid PC
    }
    return 0; // Valid SP and PC
}

int is_app_valid(){
    uint32_t header_addr = APP_HEADER_START;
    app_header_t* app_hdr = (app_header_t*)header_addr;

    if(app_hdr->magic_number != APP_MAGIC_NUMBER){
        return 1; // Invalid app
    }

    return 0; // Valid app
}

