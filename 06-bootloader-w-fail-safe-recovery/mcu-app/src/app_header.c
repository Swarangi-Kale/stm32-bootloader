#include <stdint.h>
#include "firmware_info.h"

__attribute__ ((section(".app_header")))

firmware_info_t app_header = {
    .sentinel = FWINFO_SENTINEL,
    .device_id = DEVICE_ID, // the target device for the firmware update
    .version = 0xffffffff,
    .fw_length = 688, //excluding the metadata
    .reserved0 = 0xffffffff, //reserved for future use
    .reserved1 = 0xffffffff,
    .reserved2 = 0xffffffff,
    .reserved3 = 0xffffffff,
    .crc32 = 0xffffffff
};