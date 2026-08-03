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


//flash layout
#define FLASH_BASE_ADDR (0x08000000U)
#define FLASH_MEM_END (0x08020000U) //128kB flash
#define APP_SIZE (1024*96) //96kB

//SRAM layout
#define SRAM_START_ADDR (0x20000000U)
#define SRAM_END_ADDR (0x20009000U) //36kB SRAM

#define DEVICE_ID 0x92

#define SYNC_DATA0 0x13
#define SYNC_DATA1 0x43
#define SYNC_DATA2 0x2A
#define SYNC_DATA3 0x78

#define DEFAULT_TIMEOUT 5000

#define NVIC_ICER_REG_ADDR (0xE000E180U)
#define SCB_AIRCR   (*(volatile uint32_t *)0xE000ED0C)

#define DEVICE_ID 0x92
#define FWINFO_SENTINEL 0xDEADC0DE

#define FLASH_BASE_ADDR (0x08000000U)
#define BOOTLOADER_SIZE (0x8000)

#define APP_START_ADDR (FLASH_BASE_ADDR + BOOTLOADER_SIZE)

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
static uint32_t fw_length = 0;
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
    *(volatile uint32_t*)0x40000400 &= ~(1 << 0);  // TIM3_CR1: counter off
}


void jump_to_app(){
    typedef void (*void_fn)(void);

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

    int i = 0;

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
                    i=0;
                } else if(i>=4) {
                    i = 0;
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
                        
                        app_end_addr = APP_START_ADDR + fw_length;
                                    
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
                        const uint32_t curr_addr = APP_START_ADDR + bytes_written;
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
    
    disable_inits_before_jump();
    // Relocate vector table to app
    *(volatile uint32_t*)0xE000ED08 = APP_START_ADDR;

    jump_to_app();
    //Never return

    return 0;
}