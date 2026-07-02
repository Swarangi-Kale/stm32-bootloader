#include <stdint.h>


#define BOOTLOADER_SIZE (0x8000U) 
#define APP_START_ADDR (0x08008000U)

static void jump_to_app(){
    typedef void (*void_fn)(void);

    uint32_t* app_reset_vector_entry = (uint32_t*)(APP_START_ADDR + 4U);
    uint32_t* reset_vector = (uint32_t*)*app_reset_vector_entry;

    void_fn jump_fn = (void_fn)reset_vector;

    jump_fn();
}

int main(){

    // Relocate vector table to app
    *(volatile uint32_t*)0xE000ED08 = 0x08008000;
    
    jump_to_app();

    //Never return

    return 0;
}