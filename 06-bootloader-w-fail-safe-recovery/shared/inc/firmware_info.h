#ifndef INC_FIRMWARE_INFO_H
#define INC_FIRMWARE_INFO_H

#include <stdint.h>

// extern uint32_t __isr_vector_start;
// extern uint32_t __isr_vector_end;

#define DEVICE_ID 0x92
#define FWINFO_SENTINEL 0xDEADC0DE

#define FLASH_BASE_ADDR (0x08000000U)
#define BOOTLOADER_SIZE (0x8000)

#define APP_HEADER_ADDR           (FLASH_BASE_ADDR + BOOTLOADER_SIZE)
#define APP_HEADER_SIZE           (9 * 4) // 9 fields, each of 4 bytes

#define VECTOR_TABLE_START_OFFSET (0X80) // must match ". = ALIGN(0x80)" in linker script

#define FW_IMAGE_START_ADDR       (APP_HEADER_ADDR) //including the header prepended at the begining
#define APP_START_ADDR            (APP_HEADER_ADDR + VECTOR_TABLE_START_OFFSET) // = 0x08008080
#define VECTOR_TABLE_SIZE         0xb8//((uint32_t)&__isr_vector_end - (uint32_t)&__isr_vector_start) // = 0xb8. Hardcoded in fw_updater. Got it from the .map file generated.
#define APP_VALIDATION_FROM       (APP_START_ADDR + VECTOR_TABLE_SIZE)

//All fields are in little endian byte order.
typedef struct firmware_info_t{
    uint32_t sentinel;
    uint32_t device_id; // the target device for the firmware update
    uint32_t version;
    uint32_t fw_length; //excluding the metadata
    uint32_t reserved0; //reserved for future use
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
    uint32_t crc32;
} firmware_info_t;


#endif //INC_FIRMWARE_INFO_H