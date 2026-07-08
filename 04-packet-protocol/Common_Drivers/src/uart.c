/*
 * uart.c
 *
 *  Created on: 25-May-2026
 *      Author: Swarangi
 */

#include "ringbuffer.h"
// #include "stm32g0xx.h"
#include <stdint.h>
#include "uart.h"

RingBuffer rb;
uint8_t ring[RING_BUFFER_SIZE];

void uart_init(int clock_speed, int baud_rate){
	  *(volatile uint32_t*)(0x40021034) |= 0x00000001; //give clock to GPIO A for PA2 and PA3


	  *(volatile uint32_t*)(0x50000000) &= ~(0x000000F0); // set moder as alternate function for PA2 and PA3
	  *(volatile uint32_t*)(0x50000000) |= 0x000000A0;

	  *(volatile uint32_t*)(0x50000020) &= ~(0x0000FF00);//set AFRL to AF1 fro PA2 and PA3
	  *(volatile uint32_t*)(0x50000020) |= 0x00001100;

	  *(volatile uint32_t*)(0x4002103C) |= 0x00020000; // give clock to USART2 peripheral
	  *(volatile uint32_t*)(0x40004400) &= ~(0x00000001) ; //disable UART

	  int scaler = clock_speed/baud_rate;

	  *(volatile uint32_t*)(0x4000440C) = scaler; //set baud rate. clock f is 16MHz. reqd baud rate = 115200. thus divisor value reqd = 138.88 = 139 approx
	  *(volatile uint32_t*)(0x40004400) |= 0x0000000D; // In CR1 reg: enable transmission. enable UART. enable receiver.
  }

void uart_interrupt_init(){
	*(volatile uint32_t*)(0x40004400) |= 0x00000020;
	NVIC_Enable_USART_IRQ(USART2_IRQn);
}

volatile uint32_t count_ore = 0;
volatile uint32_t count_rxne = 0;

void USART2_IRQHandler(void){
	if(((*(volatile uint32_t*)(0x4000441C)) & (0x00000020))){
		uint8_t data = *(volatile uint8_t*)(0x40004424);
		rb_push(&rb, data);
	}
	
}


void uart_transmit_byte(uint8_t data){
	  while(!((*(volatile uint32_t*)(0x4000441C))&(0x00000080))); // only start transmitting when TXE bit of USART_ISR reg is set

	  *(volatile uint8_t*)(0x40004428) = data; //teh register is 32 bits wide but we are transmitting 8 bit data

	  while(!((*(volatile uint32_t*)(0x4000441C))&(0x00000040))); // wait for TC bit
}

uint8_t uart_receive_byte(){
	  uint8_t data;
	//   while(rb_isEmpty(&rb));   // wait for ISR to have pushed something
	  rb_pop(&rb, &data);
	  return data;
}

bool uart_is_data_available(){
	return !rb_isEmpty(&rb);
}

void NVIC_Enable_USART_IRQ(int irqn){
	*(volatile uint32_t*)(NVIC_ISER_REG_ADDR) |= (1<<irqn);
}

void uart_write(uint8_t* data, int length){
	for(int i = 0; i < length; i++){
		uart_transmit_byte(data[i]);
	}
}

void uart_read(uint8_t* data, int length){
	for(int i = 0; i < length; i++){
		data[i] = uart_receive_byte();
	}
}
