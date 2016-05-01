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
char SendBt_Buffer[BUFFER_SIZE];
int IO_front, IO_rear, SendBt_front, SendBt_rear;
char received_packet[3];
//MUTEX *communication;
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
		return -1; /* Queue Empty - nothing to get*/
	}

	old = Queue[*rear];

	*rear = (*rear + 1) % BUFFER_SIZE;

	return old; // No errors
}



void Bluetooth_fromBase(void)
{

	for (;;)
	{
	//	Mutex_Lock(communication);
		roomba_dir = UART_Receive(UART_CH_1);
		Buffer_Enqueue(IOBuffer, roomba_dir, &IO_front, &IO_rear);
		Buffer_Enqueue(SendBt_Buffer, roomba_dir, &SendBt_front, &SendBt_rear);
	//	Mutex_Unlock(communication);
		Task_Sleep(10);
	}
}
void Send_to_secondary_roomba(void)
{
	for (;;)
	{
		char b_data;
	//	Mutex_Lock(communication);
		b_data = Buffer_Dequeue(SendBt_Buffer, &SendBt_front, &SendBt_rear);
		UART_Transmit(UART_CH_3, b_data);
//		Mutex_Unlock(communication);
		Task_Sleep(10);
	}
}
void Manual_Drive(void){
	for (;;)
	{
		int b_data;
	//	Mutex_Lock(communication);
		b_data = Buffer_Dequeue(IOBuffer, &IO_front, &IO_rear);
		UART_Transmit(UART_CH_0, b_data);
	//	Mutex_Unlock(communication);
		switch(b_data){
			case 'f': Roomba_Drive(200, 32768);
						break;
			case 'b': Roomba_Drive(-200, 32768);
						break;
			case 'r': Roomba_Drive(200, -1);
						break;
			case 'l':Roomba_Drive(-200, 1);
						break;
			case 'x': Roomba_Drive(0, 0);
						break;
			default: break;
		}
		Task_Sleep(20);
	}
}
void Auto_Drive(void){
	char secondary_roomba_data;
	for(;;){
		
		Roomba_Drive(500, 0xFF38);
	//	Mutex_Lock(communication);
		secondary_roomba_data = 'c';
		Buffer_Enqueue(SendBt_Buffer, secondary_roomba_data, &SendBt_front, &SendBt_rear);
	//	Mutex_Unlock(communication);
		_delay_ms(2100);
		Roomba_Drive(500, 0x00C8);
	//	Mutex_Lock(communication);
		secondary_roomba_data = 'w';
		Buffer_Enqueue(SendBt_Buffer, secondary_roomba_data, &SendBt_front, &SendBt_rear);
	//	Mutex_Unlock(communication);]
		_delay_ms(2100);
	}
}
void a_main(){
	
	DDRL = 0xFF;
	UART_Init(UART_CH_0, 9600);
	UART_print(UART_CH_0, "Boot");
	//Channel 1 paired with base
	UART_Init(UART_CH_1, 9600);
	//Chnnel3 paired with secondary roomba
	UART_Init(UART_CH_3, 9600);
	UART_print(UART_CH_3, "Boot Ch3");
	Roomba_Init(); // initialize the roomba
	BufferInit(&IO_front, &IO_rear);
	BufferInit(&SendBt_front, &SendBt_rear);
//	communication = Mutex_Init();
	while(UART_Receive(UART_CH_3) != 'g'){
		//Wait till the dummy roomba is ready
	}
	UART_Transmit(UART_CH_0, 'g');
	sei();
	Task_Create(Bluetooth_fromBase, 1, 0);
	Task_Create(Send_to_secondary_roomba, 1, 0);
	Task_Create(Manual_Drive, 2, 0);
	Task_Create(Auto_Drive, 3, 0);
	
}