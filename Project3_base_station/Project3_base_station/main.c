#include <avr/interrupt.h>
#include <stdbool.h>
#include <avr/io.h>
#include <string.h>
#include <stdlib.h>
#include "uart/BlockingUART.h"
#include "roomba/roomba.h"
#include "roomba/roomba_sci.h"
#include <util/delay.h>	
#define clock8MHz()			cli(); CLKPR = _BV(CLKPCE); CLKPR = 0x00; sei();
#include "os/kernel.h"
#include "os/os.h"

#include <util/delay.h>
EVENT *send_joystick_coords;
char x_coord[10], y_coord[10];
void startConversion()
{
	ADCSRA |= (1 << ADSC);
}
void select_ADC_channel0(){
	ADMUX = 0x00;
	ADMUX |= (1 << REFS0);
	ADMUX |= (1 <<ADLAR);
}

void select_ADC_channel1(){
	ADMUX = 0x00;
	ADMUX |= (1 << REFS0);
	ADMUX |= (1 << ADLAR);
	ADMUX |= (1 << MUX0);
}
void Base_Task(void)
{
	//uint8_t i = 0;
	uint8_t sum_x=0, sum_y =0; //, x, y; 
	for (;;){
		select_ADC_channel0();

		itoa(ADCH, x_coord, 10);
		sum_x = ADCH;
		UART_print(UART_CH_0, x_coord);
		UART_print(UART_CH_0, ",");
		select_ADC_channel1();
		itoa(ADCH, y_coord, 10);	
		sum_y = ADCH;	
		UART_print(UART_CH_0, y_coord);
		UART_print(UART_CH_0, "\n");

		if(!(PINA  & 0x01) ){
			UART_Transmit(UART_CH_1, 'x');
			UART_Transmit(UART_CH_0, 'x');
		}
		else{ 
		if(sum_x ==0 && sum_y ==0){
			UART_Transmit(UART_CH_1, 'l');
			UART_Transmit(UART_CH_0, 'l');
		}
		else if(sum_x == 255 && sum_y == 127){
			UART_Transmit(UART_CH_1, 'r');
			UART_Transmit(UART_CH_0, 'r');
		}
		else if(sum_x == 127 && sum_y == 255){
			UART_Transmit(UART_CH_1, 'b');
			UART_Transmit(UART_CH_0, 'b');
		}
		else if(sum_x == 127 && sum_y == 0){
			UART_Transmit(UART_CH_1, 'f');
			UART_Transmit(UART_CH_0, 'f');
		}
		else{
			UART_Transmit(UART_CH_1, 'd');
			UART_Transmit(UART_CH_0, 'd');
		}
		}

		Task_Sleep(25); 
	}
}

void setupADC()
{
	ADMUX |= (1 << REFS0); //Set ADC reference to AVCC
	ADMUX |= (1 <<ADLAR);
	
	ADCSRA |= (1 << ADEN); //Enable ADC 
	ADCSRA |= (1 << ADATE); //Needed for putting ADC in free running mode
	
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); //Set ADC pre-scaler to 128 - 125 KHz sample rate @ 16MHz
	ADCSRA |= (1 << ADSC); // Start A2D conversions
	ADCSRB &= ~(_BV(ADTS2) | _BV(ADTS1) | _BV(ADTS0));
	
	startConversion();
}



void a_main(){
	
	//clock8MHz();
	cli();
	setupADC();
	DDRA = (0<<PA0);
	PORTA = 0x01;
	UART_Init(UART_CH_0, 9600);
	UART_print(UART_CH_0, "Boot");
	UART_Init(UART_CH_1, 9600);
	sei();
	UART_print(UART_CH_1, "Bluetooth Boot");
	Task_Create(Base_Task, 1, 0);
}

