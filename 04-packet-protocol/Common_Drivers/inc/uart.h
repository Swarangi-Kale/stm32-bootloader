/*
 * uart.h
 *
 *  Created on: 25-May-2026
 *      Author: Swarangi
 */

#ifndef SRC_UART_H_
#define SRC_UART_H_

#include "ringbuffer.h"

#define NVIC_ISER_REG_ADDR (0xE000E100UL)
#define USART2_IRQn 28
#define RING_BUFFER_SIZE 36

extern RingBuffer rb;
extern uint8_t ring[RING_BUFFER_SIZE];

void uart_init(int clock_speed, int baud_rate);
void uart_interrupt_init();
void USART2_IRQHandler(void);
void uart_transmit_byte(uint8_t);
uint8_t uart_receive_byte();
bool uart_is_data_available();
void NVIC_Enable_USART_IRQ(int);

void uart_write(uint8_t* data, int length);
void uart_read(uint8_t* data, int length);

#endif /* SRC_UART_H_ */
