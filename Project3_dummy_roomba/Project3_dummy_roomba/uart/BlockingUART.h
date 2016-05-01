/*
 * BlockingUART.h
 *
 *  Created on: Jul 9, 2013
 *      Author: andpol
 */

#ifndef BLOCKINGUART_H_
#define BLOCKINGUART_H_

#include <avr/common.h>

#define TX_BUFFER_SIZE 64
#define F_CPU 16000000UL
typedef enum {
	UART_CH_0,
	UART_CH_1,
	UART_CH_2,
	UART_CH_3
} UART_CHANNEL;

#define MYBRR(baud_rate) (F_CPU / 16 / (baud_rate) - 1)

void UART_Init(UART_CHANNEL ch, uint32_t baud_rate);
void UART_Transmit(UART_CHANNEL ch, unsigned char data);
unsigned char UART_Receive(UART_CHANNEL ch);

void UART_print(UART_CHANNEL ch, const char* fmt, ...);
void UART_send_raw_bytes(UART_CHANNEL ch, const uint8_t num_bytes, const uint8_t* data);

#endif /* BLOCKINGUART_H_ */
