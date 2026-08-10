#include <stdint.h>
#include <string.h>
#include "chip_config_init.h"
#include "blinky.h"
#include "ringbuffer.h"
#include "uart.h"
#include "comms.h"
#include "simple-timer.h"
#include "flash.h"
#include "crc.h"
#include "firmware_info.h"
#include "boot_status.h"


//flash layout
// #define FLASH_BASE_ADDR (0x08000000U)
#define FLASH_MEM_END (0x08020000U) //128kB flash
#define APP_SIZE (1024*96) //96kB
// #define BOOTLOADER_SIZE (0x8000)

// #define APP_HEADER_ADDR (FLASH_BASE_ADDR + BOOTLOADER_SIZE)
// #define APP_HEADER_SIZE  (9 * 4) // 9 fields, each of 4 bytes
// #define FW_IMAGE_START_ADDR  (APP_HEADER_ADDR)
// #define APP_START_ADDR  (APP_HEADER_ADDR + APP_HEADER_SIZE)
// #define VECTOR_TABLE_SIZE  (0xb8)
// #define APP_VERIFICATION_FROM  (APP_STARADDR + VECTOR_TABLE_SIZE)
// #define VALIDATION_PART_LENGTH    (fw_length - APP_HEADER_SIZE - VECTOR_TABLE_SIZE)
#define VALIDATION_PART_LENGTH    (fw_length - 0x80 - VECTOR_TABLE_SIZE)

//SRAM layout
#define SRAM_START_ADDR (0x20000000U)
#define SRAM_END_ADDR (0x20009000U) //36kB SRAM

// #define DEVICE_ID 0x92
// #define FWINFO_SENTINEL 0xDEADC0DE

#define SYNC_DATA0 0x13
#define SYNC_DATA1 0x43
#define SYNC_DATA2 0x2A
#define SYNC_DATA3 0x78

#define DEFAULT_TIMEOUT 1000

#define NVIC_ICER_REG_ADDR 0xE000E180
#define NVIC_ICPR_REG_ADDR 0xE000E280
#define SCB_AIRCR   (*(volatile uint32_t *)0xE000ED0C)

uint8_t aligned_buf[PACKET_DATA_SIZE] __attribute__((aligned(8)));

typedef enum bl_state_t{
    BL_STATE_SYNC,
    BL_STATE_WAIT_FOR_UPDATE_REQ,
    BL_STATE_DEVICE_ID_REQ,
    BL_STATE_DEVICE_ID_RES,
    BL_STATE_FW_LENGTH_REQ,
    BL_STATE_FW_LENGTH_RES,
    BL_STATE_ERASE_APP,
    BL_STATE_RECEIVE_FW,
    BL_STATE_DONE
} bl_state_t;

static bl_state_t state = BL_STATE_SYNC;
uint32_t bytes_written = 0;
static uint8_t sync_msg[4] = {0};
static comms_packet_t temp_packet_bl;
static simple_timer_t timer;
static uint32_t fw_length = 808;
static uint32_t app_end_addr = 0;

// void NVIC_Disable_USART_IRQ(int irqn){
//     *(volatile uint32_t*)(NVIC_ICER_REG_ADDR) = (1 << irqn);
// }

static inline void __enable_irq(void) {
    __asm volatile ("cpsie i");
}

void disable_inits_before_jump(){
    // Disable SysTick completely (counter + interrupt + clock source)
    *(volatile uint32_t*)0xE000E010 = 0x00000000;  // SysTick->CTRL = 0

    // Disable TIM3 counter (bootloader's tim3_polling_init enabled it)
    *(volatile uint32_t*)0x40000400 &= ~(1 << 0); // TIM3_CR1: counter off

    // Fully reset the UART peripheral used by the bootloader (adjust to your actual instance)
    *(volatile uint32_t*)0x4002102c |= (1<<17);   // assert reset
    *(volatile uint32_t*)0x4002102c &= ~(1<<17);  // deassert reset — peripheral is now live again
    
    *(volatile uint32_t *)NVIC_ICER_REG_ADDR = 0xFFFFFFFF;  // disable all IRQs
    *(volatile uint32_t *)NVIC_ICPR_REG_ADDR = 0xFFFFFFFF; // clear all pending IRQs
}


void jump_to_app(){
    typedef void (*void_fn)(void);

    disable_inits_before_jump();
    *(volatile uint32_t*)0xE000ED08 = APP_START_ADDR;

    uint32_t* app_reset_vector_entry = (uint32_t*)(APP_START_ADDR + 4U);
    uint32_t* reset_vector = (uint32_t*)*app_reset_vector_entry;

    void_fn jump_fn = (void_fn)reset_vector;

    jump_fn();
}

void create_four_byte_packet(comms_packet_t* packet, uint32_t data){
    packet->length = 4;
    packet->data[0] = (uint8_t)(data & 0xFF);
    packet->data[1] = (uint8_t)((data >> 8) & 0xFF);
    packet->data[2] = (uint8_t)((data >> 16) & 0xFF);
    packet->data[3] = (uint8_t)((data >> 24) & 0xFF);
    for(uint8_t i = 4; i < PACKET_DATA_SIZE; i++){ /*Can use memset instead*/
        packet->data[i] = 0xff;
    }
    packet->crc = comms_compute_crc(packet);
}

void comms_create_test_packet_16(comms_packet_t* packet){
    packet->length = 15; /* as you specified */

    for(uint8_t i = 0; i < 16; i++){
        packet->data[i] = (i % 2 == 0) ? 0x12 : 0x13;
    }

    for(uint8_t i = 16; i < PACKET_DATA_SIZE; i++){
        packet->data[i] = 0xff; /* pad remainder same as comms_create_single_byte_packet */
    }

    packet->crc = comms_compute_crc(packet);
}

static void check_for_timeout(){
    if(simple_timer_has_elapsed(&timer)){
        comms_create_single_byte_packet(&temp_packet_bl, BL_PACKET_TIMEOUT);
        comms_write(&temp_packet_bl);
        state = BL_STATE_DONE;
    }
}

static void bootloading_failed(){
    check_for_timeout();
}

static bool comms_is_Device_ID_packet(comms_packet_t* packet){
    if(packet->length != 2) return false;
    if(packet->data[0] != BL_PACKET_DEVICE_ID_RES_DATA0) return false;
    for(uint8_t i = 2; i < PACKET_DATA_SIZE; i++){
        if(packet->data[i] != 0xff) return false;
    }

    return true;
}

static bool comms_is_FW_Length_packet(comms_packet_t* packet){
    if(packet->length != 5) return false;
    if(packet->data[0] != BL_PACKET_FW_LENGTH_RES_DATA0) return false;
    for(uint8_t i = 5; i < PACKET_DATA_SIZE; i++){
        if(packet->data[i] != 0xff) return false;
    }

    return true;
}

static bool validate_firmware_image(){
    firmware_info_t* fw_info = (firmware_info_t*) APP_HEADER_ADDR;

    if(fw_info -> sentinel != FWINFO_SENTINEL) return false;
    if(fw_info -> device_id != DEVICE_ID) return false;

    const uint8_t* validation_start_addr = (const uint8_t*) APP_VALIDATION_FROM;
    uint32_t computed_crc = crc32(validation_start_addr, VALIDATION_PART_LENGTH);
    return computed_crc == fw_info -> crc32;
}

void system_reset(){
    // Ensure all memory operations complete before reset
    __asm volatile ("dsb" ::: "memory");

    SCB_AIRCR = (0x05FAUL << 16) | (1UL << 2);

    // Reset takes effect within a few clock cycles, but isn't instant —
    // wait here so execution doesn't fall through to invalid code
    __asm volatile ("dsb" ::: "memory");
    while (1) { /* wait for reset */ }
}

void configure_iwdg(uint32_t timeout_ms) {
    IWDG_KR = 0x5555;              // unlock
    while(IWDG_SR & (0x1));
    IWDG_PR = (110);        // prescaler = 8, tune with reload for your timeout, LSI freq is 32kHz
    IWDG_RLR = 32000/256 * timeout_ms/1000; //clk_freq/prescalar * timeout in seconds
    IWDG_KR = 0xAAAA;              // reload
    IWDG_KR = 0xCCCC;              // start IWDG
}



int main(){

    chip_clock_config();
    systick_init();
    gpio_portA_init();
    tim3_polling_init();
    rb_init(&rb, ring, RING_BUFFER_SIZE);
    uart_init(16000000, 115200);
    uart_interrupt_init();
    __enable_irq(); // Enable global interrupts
    comms_setup();
    *(volatile uint32_t *)0xE000ED0C |= (0x0000 << 16);
    simple_timer_setup(&timer, DEFAULT_TIMEOUT, false);

    backup_domain_unlock();
    uint32_t status = boot_status_read();
    // boot_status_write(0x00000000);

    while(state != BL_STATE_DONE){
        if(state == BL_STATE_SYNC){
            if(uart_is_data_available()){
                sync_msg[0] = sync_msg[1];
                sync_msg[1] = sync_msg[2];
                sync_msg[2] = sync_msg[3];
                sync_msg[3] = uart_receive_byte();

                bool is_match = sync_msg[0] == SYNC_DATA0;
                is_match = is_match && (sync_msg[1] == SYNC_DATA1);
                is_match = is_match && (sync_msg[2] == SYNC_DATA2);
                is_match = is_match && (sync_msg[3] == SYNC_DATA3);

                if(is_match){
                    comms_create_single_byte_packet(&temp_packet_bl, BL_PACKET_SEQ_OBSERVED_DATA0);
                    comms_write(&temp_packet_bl);
                    simple_timer_reset(&timer);
                    state = BL_STATE_WAIT_FOR_UPDATE_REQ;
                } else{
                    check_for_timeout();
                }
            } else{
                check_for_timeout();
            }

            continue;
        }

        comms_update();
        

        switch(state){
            case BL_STATE_WAIT_FOR_UPDATE_REQ: {
                if(comms_is_packet_available()){
                    comms_read(&temp_packet_bl);

                    if(comms_is_single_byte_packet(&temp_packet_bl, BL_PACKET_FW_UPDATE_REQ_DATA0)){
                        comms_create_single_byte_packet(&temp_packet_bl, BL_PACKET_FW_UPDATE_RES_DATA0);
                        comms_write(&temp_packet_bl);
                        simple_timer_reset(&timer);
                        state = BL_STATE_DEVICE_ID_REQ;
                    }
                    else{
                        bootloading_failed();
                    }
                }
                else{
                    check_for_timeout();
                }
            } break;

            case BL_STATE_DEVICE_ID_REQ: {
                comms_create_single_byte_packet(&temp_packet_bl, BL_PACKET_DEVICE_ID_REQ_DATA0);
                comms_write(&temp_packet_bl);
                state = BL_STATE_DEVICE_ID_RES;
            } break;

            case BL_STATE_DEVICE_ID_RES: {
                if(comms_is_packet_available()){
                    comms_read(&temp_packet_bl);

                    if(comms_is_Device_ID_packet(&temp_packet_bl)){
                        simple_timer_reset(&timer);
                        state = BL_STATE_FW_LENGTH_REQ;
                    }
                    else{
                        bootloading_failed();
                    }
                }
                else{
                    check_for_timeout();
                }
            } break;

            case BL_STATE_FW_LENGTH_REQ: {
                comms_create_single_byte_packet(&temp_packet_bl, BL_PACKET_FW_LENGTH_REQ_DATA0);
                comms_write(&temp_packet_bl);
                state = BL_STATE_FW_LENGTH_RES;
            } break;

            case BL_STATE_FW_LENGTH_RES: {
                if(comms_is_packet_available()){
                    comms_read(&temp_packet_bl);
                    if(comms_is_FW_Length_packet(&temp_packet_bl)){
                        
                        fw_length = (temp_packet_bl.data[1])       | 
                                    (temp_packet_bl.data[2] << 8)  |
                                    (temp_packet_bl.data[3] << 16) |
                                    (temp_packet_bl.data[4] << 24);
                        
                        app_end_addr = APP_HEADER_ADDR + fw_length;
                                    
                        simple_timer_reset(&timer);
                        state = BL_STATE_ERASE_APP;
                    }
                    else{
                        bootloading_failed();
                    }
                }
                else{
                    check_for_timeout();
                }

                
            } break;

            case BL_STATE_ERASE_APP: {
                erase_main_app();

                comms_create_single_byte_packet(&temp_packet_bl, BL_PACKET_READY_FOR_DATA_DATA0);
                comms_write(&temp_packet_bl);

                state = BL_STATE_RECEIVE_FW;
            } break;

            case BL_STATE_RECEIVE_FW: {
                comms_update();
                if(bytes_written < fw_length){
                    if(comms_is_packet_available()){
                        comms_read(&temp_packet_bl);

                        const uint8_t packet_length = (temp_packet_bl.length & 0x0f) + 1;
                        const uint32_t curr_addr = APP_HEADER_ADDR + bytes_written;
                        memcpy(aligned_buf, temp_packet_bl.data, packet_length);
                        flash_unlock();
                        bool whether_flashed = flash_program(curr_addr, aligned_buf, packet_length);
                        flash_lock();
                        if(!whether_flashed){
                            comms_create_single_byte_packet(&temp_packet_bl, BL_PACKET_FLASH_NOT_WRITTEN_DATA0);
                            comms_write(&temp_packet_bl);
                            check_for_timeout();
                        }
                        bytes_written += packet_length;
                        delay_ms(5); // the firmware updation was stopping randomly. hence added this delay
                        comms_create_single_byte_packet(&temp_packet_bl, BL_PACKET_READY_FOR_DATA_DATA0);
                        comms_write(&temp_packet_bl);

                        simple_timer_reset(&timer);
                    }
                    else{
                        check_for_timeout();
                    }
                }
                else{
                    comms_create_single_byte_packet(&temp_packet_bl, BL_PACKET_UPDATE_SUCCESSFUL_DATA0);
                    // comms_create_single_byte_packet(&temp_packet_bl, app_end_addr);
                    comms_write(&temp_packet_bl);
                    state = BL_STATE_DONE;
                }
            }
        }
    }
    

    if (status == BOOT_MAGIC_CONFIRMED || ((status & 0x1111) == 0) /* erased/first-ever boot */) {
        // last app boot was good (or this is a fresh chip) -> try to run it
        boot_status_write(BOOT_MAGIC_PENDING);
        configure_iwdg(10000);   // arm a 4s watchdog window
        jump_to_app();
    } else {
        // status is PENDING (never confirmed) or FAILED -> app is bad
        // fall through to your existing UART sync/update-wait loop
        // comms_create_single_byte_packet(&temp_packet_bl, BL_PACKET_UPDATE_SUCCESSFUL_DATA0);
        uart_transmit_byte('N');
    }
    
    //Never return

    return 0;
}