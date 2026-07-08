#include "comms.h"
#include <stdint.h>
#include <stdbool.h>

#include "uart.h"
#include "ringbuffer.h"
#include "crc.h"
#include "blinky.h"

#define PACKET_BUFFER_LENGTH 8

typedef enum comms_state_t {
    CommsState_Length,
    CommsState_Data,
    CommsState_CRC,
} comms_state_t;

comms_state_t state = CommsState_Length;
uint8_t data_byte_count = 0;
comms_packet_t temp_packet = {.length = 0, .data = {0}, .crc = 0};
comms_packet_t ReTx_Packet = {.length = 0, .data = {0}, .crc = 0};
comms_packet_t Ack_Packet = {.length = 0, .data = {0}, .crc = 0};
comms_packet_t last_transmitted_Packet = {.length = 0, .data = {0}, .crc = 0};
comms_packet_t packet_buffer[PACKET_BUFFER_LENGTH];
uint8_t buffer_read_index = 0;
uint8_t buffer_write_index = 0;
uint8_t buffer_mask = PACKET_BUFFER_LENGTH - 1;/*To check for special packets wih specific purpose. i.e ack or ReTx packets (They have single byte indicating their type])*/

void comms_setup(void){
    ReTx_Packet.length = 1;
    ReTx_Packet.data[0] = PACKET_RETX_DATA0;
    for(uint8_t i = 1; i < PACKET_DATA_SIZE; i++){
        ReTx_Packet.data[i] = 0xff;
    }
    ReTx_Packet.crc = comms_compute_crc(&ReTx_Packet);

    Ack_Packet.length = 1;
    Ack_Packet.data[0] = PACKET_ACK_DATA0;
    for(uint8_t i = 1; i < PACKET_DATA_SIZE; i++){
        Ack_Packet.data[i] = 0xff;
    }
    Ack_Packet.crc = comms_compute_crc(&Ack_Packet);
}


void comms_update(comms_packet_t *packet)
{
    uint8_t computed_crc = comms_compute_crc(packet);
 
    if (packet->crc != computed_crc) {
        TOGGLE_LED_PA5;
        comms_write(&ReTx_Packet);
        return;
    }
 
    if (comms_is_single_byte_packet(packet, PACKET_ACK_DATA0)) {
        return;
    }
 
    if (comms_is_single_byte_packet(packet, PACKET_RETX_DATA0)) {
        comms_write(&last_transmitted_Packet);
        return;
    }
 
    uint32_t next_write_index = (buffer_write_index + 1) & buffer_mask;
    if (next_write_index == buffer_read_index) {
        __asm__("BKPT #0"); /* Buffer overflow — halts if a debugger is attached */
    }
 
    comms_packet_copy(packet, &packet_buffer[buffer_write_index]);
    buffer_write_index = next_write_index;
    comms_write(&Ack_Packet);
}

bool comms_is_packet_available(void){
    if(buffer_read_index == buffer_write_index) return false;
    return true;
}

void comms_write(comms_packet_t* packet){
    uart_write((uint8_t*)packet, PACKET_TOTAL_SIZE);
    comms_packet_copy(packet, &last_transmitted_Packet);
}

void comms_read(comms_packet_t* packet){
    comms_packet_copy(&packet_buffer[buffer_read_index], packet); //
    buffer_read_index = (buffer_read_index + 1) & buffer_mask;
}

uint8_t comms_compute_crc(comms_packet_t* packet){
    return crc8((uint8_t*)packet, (PACKET_TOTAL_SIZE - PACKET_CRC_SIZE));
}

bool comms_is_single_byte_packet(comms_packet_t* packet, uint8_t expected_data0){ 
    if(packet->length != 1) return false;
    if(packet->data[0] != expected_data0) return false;
    for(uint8_t i = 1; i < PACKET_DATA_SIZE; i++){
        if(packet->data[i] != 0xff) return false;
    }

    return true;
}

void comms_packet_copy(const comms_packet_t* src, comms_packet_t* dest){
    dest->length = src->length;
    for(uint8_t i = 0; i < PACKET_DATA_SIZE; i++){
        dest->data[i] = src->data[i];
    }
    dest->crc = src->crc;
}