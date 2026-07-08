#ifndef INC_COMMS_H
#define INC_COMMS_H

#include <stdint.h>
#include <stdbool.h>


#define PACKET_LENGTH_SIZE   1U
#define PACKET_DATA_SIZE     16U
#define PACKET_CRC_SIZE      1U
#define PACKET_TOTAL_SIZE (PACKET_LENGTH_SIZE + PACKET_DATA_SIZE + PACKET_CRC_SIZE)

#define PACKET_ACK_DATA0 0x15
#define PACKET_RETX_DATA0 0x19

typedef struct __attribute__((packed)) comms_packet_t {
    uint8_t length;
    uint8_t data[PACKET_DATA_SIZE];
    uint8_t crc;
} comms_packet_t;

void comms_setup(void);
void comms_update(comms_packet_t *packet);

bool comms_is_packet_available(void);
void comms_write(comms_packet_t* packet);
void comms_read(comms_packet_t* packet);
uint8_t comms_compute_crc(comms_packet_t* packet);
bool comms_is_single_byte_packet(comms_packet_t* packet, uint8_t expected_data0);
void comms_packet_copy(const comms_packet_t* src, comms_packet_t* dest);


#endif // INC_COMMS_H