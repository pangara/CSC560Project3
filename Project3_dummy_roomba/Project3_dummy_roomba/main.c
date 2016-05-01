/*
 * Project3_remote_stn2.c
 *
 * Created: 26/04/2016 10:52:00 AM
 * Author : pangara
 */ 
#include <avr/interrupt.h>
#include <stdbool.h>
#include <avr/io.h>
#include "uart/BlockingUART.h"
#include "roomba/roomba.h"
#include "roomba/roomba_sci.h"
#include <util/delay.h>
//#define clock8MHz()			cli(); CLKPR = _BV(CLKPCE); CLKPR = 0x00; sei();
#include "os/kernel.h"
#include "os/os.h"

#include <util/delay.h>

unsigned char roomba_dir;
#define MAX_BUFFER 10
#define BUFFER_SIZE (MAX_BUFFER + 1)
char IOBuffer[BUFFER_SIZE];
int IO_front, IO_rear, SendBt_front, SendBt_rear;
roomba_sensor_data_t sensor_data;
void BufferInit(int *front, int *rear)
{
	*front = *rear = 0;
}
int Buffer_Enqueue(char *Queue, char new, int *front, int *rear)
{
	if(*front == (( *rear - 1 + BUFFER_SIZE) % BUFFER_SIZE))
	{
		return -1; /* Queue Full*/
	}

	Queue[*front] = new;

	*front = (*front + 1) % BUFFER_SIZE;

	return 0; // No errors
}
char Buffer_Dequeue(char *Queue, int *front, int *rear)
{
	char old;
	if(*front == *rear)
	{
		return -1; /* Queu2e Empty - nothing to get*/
	}

	old = Queue[*rear];

	*rear = (*rear + 1) % BUFFER_SIZE;

	return old; // No errors
}


void Bluetooth_Task(void)
{
	for (;;)
	{
		roomba_dir = UART_Receive(UART_CH_1);
		UART_Transmit(UART_CH_0, roomba_dir);
		Buffer_Enqueue(IOBuffer, roomba_dir, &IO_front, &IO_rear);
		Task_Sleep(10);
	}
}

void Manual_Drive(void){
	for (;;)
	{
		int b_data;
//		char sense_data[20];
		b_data = Buffer_Dequeue(IOBuffer, &IO_front, &IO_rear);
		switch(b_data){
			case 'f': Roomba_Drive(200, 32768);
						break;
			case 'b': Roomba_Drive(-200, 32768);
						break;
			case 'r': Roomba_Drive(200, -1);
						break;
			case 'l': Roomba_Drive(-200, 1);
						break;
			case 'c': Roomba_Drive(500, 0xFF38);
						break;
			case 'w': Roomba_Drive(500, 0x00C8);
						break;
			case 'x': Roomba_Drive(0, 0);
						break;
			default: break;
		}
	}
}
void a_main(){
	
	DDRL = 0xFF;
	
	UART_Init(UART_CH_0, 9600);
	UART_Init(UART_CH_1, 9600);
	BufferInit(&IO_front, &IO_rear);
	Roomba_Init();// initialize the roomba
	UART_Transmit(UART_CH_1,'g'); 
	sei();
	// UART test - drive straight forward at 100 mm/s for 0.5 second
	
//	Roomba_Drive(500, 0xFF38);
//	_delay_ms(5000);
//	Roomba_Drive(0, 0);
	Task_Create(Bluetooth_Task, 1, 0);
	Task_Create(Manual_Drive, 2, 0);
}