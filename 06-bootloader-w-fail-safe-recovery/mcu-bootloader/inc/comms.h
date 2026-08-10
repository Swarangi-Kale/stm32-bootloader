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

#define BL_PACKET_SEQ_OBSERVED_DATA0 0X20
#define BL_PACKET_FW_UPDATE_REQ_DATA0 0X28
#define BL_PACKET_FW_UPDATE_RES_DATA0 0X37 //Fw update request accepted
#define BL_PACKET_DEVICE_ID_REQ_DATA0 0X3A
#define BL_PACKET_DEVICE_ID_RES_DATA0 0X3E
#define BL_PACKET_FW_LENGTH_REQ_DATA0 0X42
#define BL_PACKET_FW_LENGTH_RES_DATA0 0X49
#define BL_PACKET_READY_FOR_DATA_DATA0 0X4F
#define BL_PACKET_UPDATE_SUCCESSFUL_DATA0 0X51

#define BL_PACKET_FLASH_NOT_WRITTEN_DATA0 0X77
#define BL_PACKET_BOOT_STATUS_PENDING_DATA0 0x80

#define BL_PACKET_TIMEOUT 0XBB

typedef struct __attribute__((packed)) comms_packet_t {
    uint8_t length;
    uint8_t data[PACKET_DATA_SIZE];
    uint8_t crc;
} comms_packet_t;

void comms_setup(void);
void comms_update();

bool comms_is_packet_available(void);
void comms_write(comms_packet_t* packet);
void comms_read(comms_packet_t* packet);
uint8_t comms_compute_crc(comms_packet_t* packet);
bool comms_is_single_byte_packet(comms_packet_t* packet, uint8_t expected_data0);
void comms_packet_copy(const comms_packet_t* src, comms_packet_t* dest);
void comms_create_single_byte_packet(comms_packet_t* packet, uint8_t data_byte0);


#endif // INC_COMMS_H