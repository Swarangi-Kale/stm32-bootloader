/*
 * ringbuffer.h
 *
 *  Created on: 18-May-2026
 *      Author: Swarangi
 */

#ifndef LIB_RINGBUFFER_H_
#define LIB_RINGBUFFER_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct{
    uint8_t *buffer;
    int r_size;
    int count;
    int read;
    int write;
} RingBuffer;

void rb_init(RingBuffer*, uint8_t*, int);

bool rb_push(RingBuffer*, uint8_t);

bool rb_pop(RingBuffer*, uint8_t*);

bool rb_peek(RingBuffer*, uint8_t*);

bool rb_isEmpty(RingBuffer*);
#endif /* LIB_RINGBUFFER_H_ */
