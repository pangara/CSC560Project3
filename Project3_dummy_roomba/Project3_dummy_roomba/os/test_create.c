/* Basic Sanity: Task creation and yield
 * This is a test to create two tasks that yield to each other when their priority is the same.
 * Test 2: Change the priority to 2. Task does not yield. Keeps yielding to itself. 
 * Test 1 -> Brown, Black, Brown, Black
 * Test 2 -> Black, Black, Black, Black
 */	
#include "kernel.h"
#include "testing.h"
#ifdef TEST_CREATE_YIELD
#include <avr/io.h>

#include "os.h"

#include <util/delay.h>

void Task_P1(void)
{
	for (;;){	
		TEST_BLIP(BLACK);
		Task_Yield();
	}
}

void Task_P2(void)
{
	for(;;){
		TEST_BLIP(BROWN);
		Task_Yield();
	}
}


void a_main(){
	
	TESTING_INIT(TESTING_ALL);
	Task_Create(Task_P1, 1, 0);
	Task_Create(Task_P2, 1, 0);
}
#endif