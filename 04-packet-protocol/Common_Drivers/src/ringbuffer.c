/*
 * ringbuffer.c
 *
 *  Created on: 18-May-2026
 *      Author: Swarangi
 */

#include "ringbuffer.h"
#include <stdio.h>

void rb_init(RingBuffer* rb, uint8_t* buffer_location, int buff_size){ // a little confusion on whether to pass the buffer size or take it directly from the definition above. also i can pass teh initialization location for read, write and count value (the buffer need not necessarily be empty at teh start or does it?)
    rb->buffer = buffer_location;
    rb->r_size = buff_size;
    rb->read = 0;
    rb->write = 0;
    rb->count = 0;

    return;
}


bool rb_push(RingBuffer* rb, uint8_t data){ //should i be using d_index here? in my case since the data is an array, I guess yes, but when continuous data is coming or what other cases are there when this won't be the proper way? also should i increment d_index inside push?
    if(rb->count<rb->r_size){
        rb->buffer[rb->write] = data;
        rb->write = ((rb->write)+1) % rb->r_size;
        rb->count++;

    }
    else{
        return false;
    }
    return true;
}

bool rb_pop(RingBuffer* rb, uint8_t* popped_val){
    // __asm volatile ("cpsid i");
    if(rb->count > 0){
        *popped_val = rb->buffer[rb->read]; //read teh value. Later can change printf to a read value function that can carry out whatever reading means (not just printing. Maybe can involve storing data)
        rb->read = ((rb->read)+1) % rb->r_size; //shift read index
        rb->count--; // decrement counter
        // __asm volatile ("cpsie i");
        return true;

    }
    // __asm volatile ("cpsie i");
    return false;
}

bool rb_peek(RingBuffer* rb, uint8_t* peek_val){
    if(rb->count>0){
        *peek_val = rb->buffer[rb->read];
    }
    else return false;

    return true;
}

bool rb_isEmpty(RingBuffer* rb){
	if(rb->count == 0){
		return true;
	}
	else return false;
}

