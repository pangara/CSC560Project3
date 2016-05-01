/*
 * \file	testing.h
 * \brief	Used to test the RTOS functionality.
 *
 *			Tests are activated or deactivated by [un]commenting them.
 *			Most tests require they be the only ones running, as many will
 *			intentionally break the RTOS to ensure disallowed behavior
 *			is actually disallowed and the resulting errors are handled.
 *
 *	@date 07/13
 *
 *	@author	Gordon Meyer
 *	@author Daniel McIlvaney
 *	@author Cale McNulty
 */ 


#ifndef TESTING_H_
#define TESTING_H_
#include <util/delay.h>
#include <avr/io.h>
#define BLACK	(1<<PA0) //digital pin 22 -> Ch6
#define BROWN	(1<<PA1) //digital pin 23 -> Ch1
#define RED		(1<<PA2) //digital pin 24 -> Ch0
#define ORANGE	(1<<PA3) //digital pin 25 -> Ch3
#define YELLOW	(1<<PA4)
#define GREEN	(1<<PA5)
#define BLUE	(1<<PA6)
#define PURPLE	(1<<PA7)

#define TESTING_ALL	0xff
#define TESTING_INIT(pin)	(DDRA |= pin);(PORTA &= ~(pin))
#define TEST_HIGH(pin)		(PORTA |= pin);
#define TEST_LOW(pin)		(PORTA &= ~(pin));

#define TEST_BLIP(wire)		TEST_HIGH(wire);_delay_us(2);TEST_LOW(wire)

//#define TEST_CREATE_YIELD //done //done
//#define TEST_PREEMPTION  //done //done
//#define TEST_MUTEX  //done //done
//#define TEST_EVENT //done //done
//#define TEST_SUSPEND //done //done
//#define TEST_SLEEP 
//#define TEST1
//#define TEST2
#define TEST3
//#define TEST4
#endif /* TESTING_H_ */