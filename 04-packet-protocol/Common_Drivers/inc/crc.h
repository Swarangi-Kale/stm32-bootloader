#ifndef INC_CRC_H
#define INC_CRC_H

#include <stdint.h>

uint8_t crc8(uint8_t* data, uint8_t length);
uint32_t crc32(const uint8_t* data, const uint32_t length);

#endif // INC_CRC_H