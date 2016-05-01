/*
 * os.c
 *
 * Created: 12/03/2016 2:14:04 PM
 *  Author: pangara
 */ 
#include <avr/io.h>
#include <avr/interrupt.h>
#include "os.h"
#include "error_code.h"
#include "kernel.h"
#include "testing.h"
#include <util/delay.h>
extern int a_main(void);

// Task manipulation
static task_descriptor_t* cur_task = NULL;
volatile uint16_t kernel_sp;
static task_descriptor_t task_desc[MAXTHREAD + 1];
static task_descriptor_t* idle_task = &task_desc[MAXTHREAD];
static volatile kernel_request_t kernel_request;
static volatile create_args_t kernel_request_create_args;
static volatile PID task_pid = 0;
static volatile PID kernel_request_pid;
static volatile int kernel_request_retval;

// For Task Sleep
static volatile unsigned int kernel_request_sleep_ticks;

//Timer Interrupt
void TIMER0_COMPA_vect(void) __attribute__ ((signal, naked));


/** Argument and return value for Event class of requests. */
static volatile EVENT* kernel_request_event_ptr;
/** Number of events created so far */
static uint8_t num_events_created = 0;
/** An array of task descriptors for tasks waiting on events. */
//Since this is idempotent, we can have only one task waiting on an event,
static task_descriptor_t *event_queue[MAXEVENT];
//For checking whether an event has occurred
static volatile uint16_t events;

//Mutex pointer
static volatile MUTEX* kernel_request_mutex_ptr;


/** Number of mutexes created so far */
static uint8_t num_mutex_created = 0;
//List of mutexes
static MUTEX_DATA mutex_list[MAXMUTEX];

/* mutexes */
static void kernel_mutex_lock(void);
static void kernel_mutex_unlock(void);


static queue_t dead_pool_queue;
static queue_p ready_queue[MINPRIORITY];
//The sleep queue
static queue_t sleep_queue;

static unsigned int cur_ticks = 0;

static uint8_t volatile error_msg = ERR_RUN_1_USER_CALLED_OS_ABORT;

/* Forward declarations */
/* kernel */
static void kernel_main_loop(void);
static void kernel_dispatch(void);
static void kernel_handle_request(void);
/* context switching */
static void exit_kernel(void) __attribute((naked));
static void enter_kernel(void) __attribute((naked));


//Tasks
static int kernel_create_task(void);
static void kernel_terminate_task(void);

/* suspend/resume */
static void kernel_task_suspend(void);
static void kernel_task_resume(void);

/* events */
static void kernel_event_wait(void);
static void kernel_event_signal(void);

static void enqueue(queue_t* queue_ptr, task_descriptor_t* task_to_add);
static task_descriptor_t* dequeue(queue_t* queue_ptr);

//Sleep enqueue/dequeue
static void sleep_enqueue(queue_t*, task_descriptor_t*);
static task_descriptor_t* sleep_dequeue(queue_t*);

//Fires whenever the timer interrupt occurs
static void kernel_update_ticker(void);

//idle task
static void idle(void);
/* sleeping */
static void kernel_sleep_task(void);

/*
* FUNCTIONS
*/
/**
*  @brief The idle task does nothing but busy loop.
*/
static void idle (void)
{
	for(;;)
	{
		TEST_BLIP(BROWN);
		};
}

/*
* KERNEL FUNCTIONS
*/
static void kernel_main_loop(void)
{
    for(;;)
    {
				
        kernel_dispatch();
		
	
        exit_kernel();
	
        /* if this task makes a system call, or is interrupted,
        * the thread of control will return to here. */

        kernel_handle_request();
    }
}

static void kernel_dispatch(void)
{
    /* If the current state is RUNNING, then select it to run again.
    * kernel_handle_request() has already determined it should be selected.
    */
    uint8_t i=0;//, j;

	//task_descriptor_t *prev_td, *nex_td;
    if((cur_task->state != RUNNING) || cur_task == idle_task)
    {
        //If there is a task waiting in the priority queue, make it the current task 
        //Changes in the structure of the priority queue
        /* if(priority_queue.head != NULL)
        {
            cur_task = dequeue(&priority_queue);
        } */

        //Iterate over the list until a task is found at a priority i.e. till py_queue.head is not NULL
	
        while(ready_queue[i].py_queue.head == NULL){
            i++;
        }
		if(i == MINPRIORITY){
			/* No task available, so idle. */
			cur_task = idle_task;
		}
		else
		{
					
			cur_task = dequeue(&(ready_queue[i].py_queue));
			while(cur_task->is_suspended == 1){
				if(ready_queue[i].py_queue.head ==NULL){
					i++;
				}
				if(i==9){
					cur_task = idle_task;
					break;
				}
				enqueue(&(ready_queue[9].py_queue), cur_task);
				cur_task = dequeue(&(ready_queue[i].py_queue));
				
			}
			//This bunch of code checks for the first ready task, since we are using suspend flags.
			//If a task is found to be ready, but is suspended, check the next task
			//How we do this:
				//Iterate over all the queues at each priority level to find the first ready task whose suspended
				//flag is not high.
			//This cannot use the generic dequeue, since we're not popping out the first task available.
			/*for(j=i;j<MINPRIORITY;j++){
				prev_td = ready_queue[j].py_queue.head;
				nex_td = ready_queue[j].py_queue.head;
				if (nex_td->is_suspended == 0){
					cur_task = dequeue(&ready_queue[j].py_queue);
				}
				else{
					while(nex_td->is_suspended == 1){
						prev_td = nex_td;
						nex_td = nex_td->next;
					}
				}
				if(nex_td->is_suspended == 0){
					cur_task = nex_td;
					prev_td->next = nex_td->next;
					break;
				}
			}
		} */
	
		
	}	
	i=0;
	//Change the current task's status to RUNNING
	cur_task->state = RUNNING;	
	}
}


static void kernel_handle_request(void)
{
	switch(kernel_request){
		case NONE:
		/* Should not happen. */
			break;
		case TIMER_EXPIRED:
			kernel_update_ticker();
			break;
		case TASK_CREATE:
			kernel_request_retval = kernel_create_task();
		
			if (kernel_request_retval>0) {
				if(kernel_request_create_args.py < cur_task->py) {
					cur_task->state = READY;
					enqueue(&(ready_queue[cur_task->py].py_queue), cur_task);
				}
			}
			break;	
		
		case TASK_TERMINATE:
			if (cur_task != idle_task) {
				kernel_terminate_task();
			}
			break;
		case TASK_YIELD:
		//Check if the current task is the idle task. We do not enqueue the idle task
			if(cur_task != idle_task){
				enqueue(&(ready_queue[cur_task->py].py_queue), cur_task);
				cur_task->state = READY;
			}
			break;
		case TASK_SLEEP:
			/* idle_task does not sleep. */
			if (cur_task != idle_task) {
				kernel_sleep_task();
			}
			break;
		case TASK_SUSPEND:
			if (cur_task != idle_task){
				kernel_task_suspend();
			}
			break;
		case TASK_RESUME:
			kernel_task_resume();
			break;
		case EVENT_INIT:
			kernel_request_event_ptr = NULL;
			if (num_events_created < MAXEVENT) {
				/* Pass a number back to the task, but pretend it is a pointer.
				* It is the index of the event_queue plus 1.
				* (0 is return value for failure.)
				*/
				kernel_request_event_ptr = (EVENT *) (uint16_t) (num_events_created + 1);
				/*
				event_queue[num_events_created].head = NULL;
				event_queue[num_events_created].tail = NULL;
				*/
				++num_events_created;
			}
			else {
				kernel_request_event_ptr = (EVENT *) (uint16_t) 0;
			}
		break;
	case EVENT_WAIT:
		/* idle_task does not wait. */
		if (cur_task != idle_task) {
			kernel_event_wait();
		}
		break;

	case EVENT_SIGNAL:
		kernel_event_signal();
		break;
	case MUTEX_INIT:
		kernel_request_mutex_ptr = NULL;
        if (num_mutex_created < MAXMUTEX) {
			/* Pass a number back to the task, but pretend it is a pointer.
			* It is the index of the mutexes plus 1.
			* (0 is return value for failure.)
			*/
			kernel_request_mutex_ptr = (MUTEX *) (uint16_t) (num_mutex_created + 1);
			++num_mutex_created;
        } 
		
		else {
			kernel_request_mutex_ptr = (MUTEX *) (uint16_t) 0;
        }
		break;
	
	case MUTEX_LOCK:
	    if (cur_task != idle_task) {
		    kernel_mutex_lock();
	    }

	    break;
	
	case MUTEX_UNLOCK:
	    kernel_mutex_unlock();
	    break;
	default:
		/* Should never happen */
		error_msg = ERR_RUN_8_RTOS_INTERNAL_ERROR;
		OS_Abort();
		break;
	}
	kernel_request = NONE;
}

/*
 * Context switching
 */
#define    SAVE_CTX_TOP()       asm volatile (\
	"push   r31             \n\t"\
	"in     r31,__SREG__    \n\t"\
	"cli                    \n\t"::) /* Disable interrupts, just in case, for the actual context switch */

#define STACK_SREG_SET_I_BIT()    asm volatile (\
	"ori    r31, 0x80        \n\t"::)

#define    SAVE_CTX_BOTTOM()       asm volatile (\
	"push   r31             \n\t"\
	"in     r31,0x3C		\n\t"\
	"push   r31             \n\t"\
	"push   r30             \n\t"\
	"push   r29             \n\t"\
	"push   r28             \n\t"\
	"push   r27             \n\t"\
	"push   r26             \n\t"\
	"push   r25             \n\t"\
	"push   r24             \n\t"\
	"push   r23             \n\t"\
	"push   r22             \n\t"\
	"push   r21             \n\t"\
	"push   r20             \n\t"\
	"push   r19             \n\t"\
	"push   r18             \n\t"\
	"push   r17             \n\t"\
	"push   r16             \n\t"\
	"push   r15             \n\t"\
	"push   r14             \n\t"\
	"push   r13             \n\t"\
	"push   r12             \n\t"\
	"push   r11             \n\t"\
	"push   r10             \n\t"\
	"push   r9              \n\t"\
	"push   r8              \n\t"\
	"push   r7              \n\t"\
	"push   r6              \n\t"\
	"push   r5              \n\t"\
	"push   r4              \n\t"\
	"push   r3              \n\t"\
	"push   r2              \n\t"\
	"push   r1              \n\t"\
	"push   r0              \n\t"::)

#define    SAVE_CTX()    SAVE_CTX_TOP();SAVE_CTX_BOTTOM();


#define    RESTORE_CTX()    asm volatile (\
	"pop    r0                \n\t"\
	"pop    r1                \n\t"\
	"pop    r2                \n\t"\
	"pop    r3                \n\t"\
	"pop    r4                \n\t"\
	"pop    r5                \n\t"\
	"pop    r6                \n\t"\
	"pop    r7                \n\t"\
	"pop    r8                \n\t"\
	"pop    r9                \n\t"\
	"pop    r10             \n\t"\
	"pop    r11             \n\t"\
	"pop    r12             \n\t"\
	"pop    r13             \n\t"\
	"pop    r14             \n\t"\
	"pop    r15             \n\t"\
	"pop    r16             \n\t"\
	"pop    r17             \n\t"\
	"pop    r18             \n\t"\
	"pop    r19             \n\t"\
	"pop    r20             \n\t"\
	"pop    r21             \n\t"\
	"pop    r22             \n\t"\
	"pop    r23             \n\t"\
	"pop    r24             \n\t"\
	"pop    r25             \n\t"\
	"pop    r26             \n\t"\
	"pop    r27             \n\t"\
	"pop    r28             \n\t"\
	"pop    r29             \n\t"\
	"pop    r30             \n\t"\
	"pop    r31             \n\t"\
	"out    0x3C, r31    	\n\t"\
	"pop    r31             \n\t"\
	"out    __SREG__, r31   \n\t"\
	"pop    r31             \n\t"::)

	

static void exit_kernel(void) {

	/*
	 * The PC was pushed on the stack with the call to this function.
	 * Now push on the I/O registers and the SREG as well.
	 */
	TEST_HIGH(BLACK);
	SAVE_CTX();

	/*
	 * The last piece of the context is the SP. Save it to a variable.
	 */
	kernel_sp = SP;

	/*
	 * Now restore the task's context, SP first.
	 */
	SP = (uint16_t) (cur_task->sp);

	/*
	 * Now restore I/O and SREG registers.
	 */
	RESTORE_CTX();


	
	/*
	 * return explicitly required as we are "naked".
	 * Interrupts are enabled or disabled according to SREG
	 * recovered from stack, so we don't want to explicitly
	 * enable them here.
	 *
	 * The last piece of the context, the PC, is popped off the stack
	 * with the ret instruction.
	 */	
	TEST_LOW(BLACK);
	asm volatile ("ret\n"::);
}

/**
 * @fn enter_kernel
 *
 * @brief All system calls eventually enter here.
 *
 * Assumption: We are still executing on cur_task's stack.
 * The return address of the caller of enter_kernel() is on the
 * top of the stack.
 *
 * Code written by Scott Craig, Justin Tanner and Matt Campbell.
 */
void enter_kernel(void) {

	
	/*
	 * The PC was pushed on the stack with the call to this function.
	 * Now push on the I/O registers and the SREG as well.
	 */
	TEST_HIGH(BLACK);
	SAVE_CTX();

	/*
	 * The last piece of the context is the SP. Save it to a variable.
	 */
	cur_task->sp = (uint8_t*) SP;

	/*
	 * Now restore the kernel's context, SP first.
	 */
	SP = kernel_sp;

	/*
	 * Now restore I/O and SREG registers.
	 */
	RESTORE_CTX();
	
	TEST_LOW(BLACK);

	/*
	 * return explicitly required as we are "naked".
	 *
	 * The last piece of the context, the PC, is popped off the stack
	 * with the ret instruction.
	 */
	asm volatile ("ret\n"::);
}


static int kernel_create_task() {
       /* The new task. */
       task_descriptor_t *p;
       uint8_t* stack_bottom;
	   task_pid++;
       if (dead_pool_queue.head == NULL) {
               /* Too many tasks! */
               return 0;
       }

       
       /* idling "task" goes in last descriptor. has a priority NULL */
       if (kernel_request_create_args.py == 9) {
               p = &task_desc[MAXTHREAD];
       }

       /* Find an unused descriptor. */
       else {
               p = dequeue(&dead_pool_queue);
       }

       stack_bottom = &(p->stack[WORKSPACE - 1]);

	/* The stack grows down in memory, so the stack pointer is going to end up
	 * pointing to the location 31 + 1 + 1 + 1 + 3 + 3 = 40 bytes above the bottom, to make
	 * room for (from bottom to top):
	 *   - 3 bytes: the address of Task_Terminate() to destroy the task if it ever returns,
	 *   - 3 bytes: the address of the start of the task to "return" to the first time it runs
	 *   - 1 byte: register 31,
	 *   - 1 byte: the stored SREG, and
	 *   - 1 byte: the stored EIND, and
	 *   - 31 bytes: registers 30 to 0.
	 */
       uint8_t* stack_top = stack_bottom - (31 + 1 + 1 + 1  + 3 + 3);

       /* Not necessary to clear the task descriptor. */
       /* memset(p,0,sizeof(task_descriptor_t)); */
		for( int i = 0; i < 31; i++ ){
			stack_top[i+1] = (uint8_t) i;
		}
       /* stack_top[0] is the byte above the stack.
        * stack_top[1] is r0. */
       stack_top[2] = (uint8_t) 0; /* r1 is the AVR "zero" register. */
      


       /* stack_top[31] is r30. */
       /* stacck_top[32] is EIND */
       stack_top[33] = (uint8_t) _BV(SREG_I); /* set SREG_I bit in stored SREG. */
       /* stack_top[34] is r31. */
	   
		stack_top[34] = (uint8_t)31;
		
		stack_top[35] = (uint8_t)0;	// EIND
		stack_top[36] = (uint8_t) ((uint16_t) (kernel_request_create_args.f) >> 8);	
		stack_top[37] = (uint8_t) (uint16_t) (kernel_request_create_args.f);
		
		stack_top[38] = (uint8_t)0;	// EIND
		stack_top[39] = (uint8_t) ((uint16_t) Task_Terminate >> 8);
		stack_top[40] = (uint8_t) (uint16_t) Task_Terminate;

       /*
        * Make stack pointer point to cell above stack (the top).
        * Make room for 32 registers, SREG and three return addresses.
        */
       p->sp = stack_top;

       p->state = READY;
       p->arg = kernel_request_create_args.arg;
       p->py = kernel_request_create_args.py;
	   p->orig_py = p->py;
	   p->proc_id = task_pid;
	   if (p == idle_task){
			p->py = NULL;
	   }
	   else {
			enqueue(&(ready_queue[p->py].py_queue), p);
	   }
       return task_pid;
}

static void kernel_terminate_task(void) {
	/* deallocate all resources used by this task */
	cur_task->state = DEAD;
	enqueue(&dead_pool_queue, cur_task);
	//Add case of the cur_task owning mutex?
}
static void kernel_sleep_task(void) {
	cur_task->state = SLEEPING;
	sleep_enqueue(&sleep_queue, cur_task);
}
static void kernel_event_wait(void) {
	/* Check the handle of the event to ensure that it is initialized. */
	uint8_t handle = (uint8_t) ((uint16_t) (kernel_request_event_ptr) - 1);

	if (handle >= num_events_created) {
		/* Error code. */
		error_msg = ERR_RUN_5_WAIT_ON_BAD_EVENT;
		OS_Abort();
	}
	else {
		/* check if some other task is already waiting for this task */
		if (event_queue[handle] != NULL){
			/* A task is already waiting for this event */
			error_msg = MULTIPLE_TASKS_WAITING;
			PORTB = (1 << PB7);
			OS_Abort();
		
			} else if( events & (1 << handle)){
			/* event already took place */
			events &= ~(1 << handle);
			} else {
			cur_task->state = WAITING;
			event_queue[handle] = cur_task;
		
		}
	}
}

static void kernel_event_signal() {
	/* Check the handle of the event to ensure that it is initialized. */
	uint8_t handle = (uint8_t) ((uint16_t) (kernel_request_event_ptr) - 1);
	
	if (handle >= num_events_created) {
		/* Error code. */
		error_msg = ERR_RUN_4_SIGNAL_ON_BAD_EVENT;
		OS_Abort();
		//PORTB = (1 << PB7);
		
	} 
		uint8_t make_ready = 0;
		
		if (event_queue[handle] != NULL) {
			/* The signalled task */
			task_descriptor_t* task_ptr = event_queue[handle];
			task_ptr->state = READY;
			
			//enqueue(&priority_queue, task_ptr);
			enqueue(&(ready_queue[task_ptr->py].py_queue), task_ptr);
			
			/* Check to see if current task needs to be pre-empted */
			if (cur_task != idle_task && !make_ready) {
				if (cur_task->py > task_ptr->py) {
					make_ready = 1;
					
				}
			

		}
		

		if (make_ready && cur_task != idle_task) {
			cur_task->state = READY;
			//enqueue(&priority_queue, cur_task);
			enqueue(&(ready_queue[cur_task->py].py_queue), cur_task);
			
		}
			
	}

}
static void kernel_mutex_lock(void) {
	uint8_t handle = (uint8_t) ((uint16_t) (kernel_request_mutex_ptr) - 1);

	if (handle >= num_mutex_created) {
		/* Error code. */
		error_msg = ERR_RUN_9_LOCK_ON_BAD_MUTEX;
		OS_Abort();
	}
	
	if (mutex_list[handle].owner == NULL) {
		/* Mutex is free */
		mutex_list[handle].owner = cur_task;
		mutex_list[handle].rec_count = 1;
		mutex_list[handle].orig_py = cur_task->py;
		mutex_list[handle].inherited_py = mutex_list[handle].orig_py;
	}
	else if (cur_task == mutex_list[handle].owner) {
		/* Mutex is already locked and caller is the owner*/
		mutex_list[handle].rec_count++;
	}
	//priority inheritance
	else if ((mutex_list[handle].inherited_py > cur_task->py))
	{
		mutex_list[handle].inherited_py = cur_task->py;
		mutex_list[handle].owner->py = cur_task->py;
		cur_task->state = WAITING;
		enqueue( &(mutex_list[handle].mutex_queue), cur_task);
	}
	else {
		/* Mutex is already locked and the caller is NOT the owner*/
		cur_task->state = WAITING;
		enqueue( &(mutex_list[handle].mutex_queue), cur_task);
	}
}

static void kernel_task_suspend(){
	//Iterate through the priority queue to search for the task
	uint8_t i=0, j, check =0;
	task_descriptor_t* task_ptr;
	if (cur_task->proc_id == kernel_request_pid){
		cur_task->is_suspended = 1;
		cur_task->state = READY;
		enqueue(&ready_queue[i].py_queue, cur_task);
	} 
	else{
		//Check ready queue
		while(ready_queue[i].py_queue.head == NULL){
			i++;
		}
	
		for (j=i; j<MINPRIORITY;j++){
			task_ptr = ready_queue[i].py_queue.head;
			if(task_ptr->proc_id == kernel_request_pid){
				task_ptr->is_suspended = 1;
				check = 1;
				break;
			}
			else{
				while(task_ptr->next!=NULL){
					task_ptr=task_ptr->next;
					if(task_ptr->proc_id == kernel_request_pid){
						task_ptr->is_suspended = 1;
						check = 1;
						break;
					}
				}
			}
			
			if(check == 1){
				break;
			}
		}
		if(check!=1){
			//check mutex queue
			for(j=0;j<num_mutex_created;j++){
				task_ptr = mutex_list[j].mutex_queue.head;
				if(task_ptr->proc_id == kernel_request_pid){
					task_ptr->is_suspended = 1;
					check = 1;
					break;
				}
				else{
					while(task_ptr->next!=NULL){
						task_ptr=task_ptr->next;
						if(task_ptr->proc_id == kernel_request_pid){
							task_ptr->is_suspended = 1;
							check = 1;
							break;
						}
					}
				}
				if(check ==1){
					break;
				}
			}
		}
		//Check event queue
		if(check != 1){
			for (j=0;j<num_events_created;j++)
			{
				task_ptr=event_queue[j];
				if(task_ptr->proc_id==kernel_request_pid){
					task_ptr->is_suspended = 1;
					check = 1;
					break;
				}
			}
		}
	}
	
}
static void kernel_task_resume(){
	//Iterate through the priority queue to search for the task
	uint8_t i=0, j, check =0;
	task_descriptor_t* task_ptr;
	//Check ready queue
	while(ready_queue[i].py_queue.head==NULL){
		i++;
	}
	for (j=i; j<MINPRIORITY-1; j++)
	{
	
		task_ptr = ready_queue[j].py_queue.head;
		if(task_ptr->proc_id == kernel_request_pid){
			task_ptr->is_suspended = 0;
			check = 1;
			task_ptr->py = task_ptr->orig_py;
			break;
			//Since this is the head of the list we can dequeue it.
			task_ptr = dequeue(&(ready_queue[9].py_queue));
			
			
		}
		else{
			while(task_ptr->next!=NULL){
				task_ptr=task_ptr->next;
				if(task_ptr->proc_id == kernel_request_pid){
					task_ptr->is_suspended = 0;
					check = 1;
					task_ptr->py = task_ptr->orig_py;
					break;
				}
			}
		}
	}
	if(check!=1){
		task_ptr = dequeue(&ready_queue[9].py_queue);
		for(i = 0; i<MAXTHREAD;i++){
		
			if((task_ptr->proc_id) == kernel_request_pid){
				task_ptr->py = task_ptr->orig_py;
				task_ptr->is_suspended = 0;
				enqueue(&(ready_queue[task_ptr->py].py_queue), task_ptr);
				check = 1;
				break;
			}
			else{
				enqueue(&(ready_queue[9].py_queue), task_ptr);
				task_ptr = dequeue(&ready_queue[9].py_queue);
			}
		}
	}
		
		
	
	if(check!=1){
		//check mutex queue
		for(j=0;j<num_mutex_created;j++){
			task_ptr = mutex_list[j].mutex_queue.head;
			if(task_ptr->proc_id == kernel_request_pid){
				task_ptr->is_suspended = 0;
				check = 1;
				break;
			}
			else{
				while(task_ptr->next!=NULL){
					task_ptr=task_ptr->next;
					if(task_ptr->proc_id == kernel_request_pid){
						task_ptr->is_suspended = 0;
						check = 1;
						break;
					}
				}
			}
			if(check ==1){
				break;
			}
		}
	}
	//Check event queue
	if(check != 1){
		for (j=0;j<num_events_created;j++)
		{
			task_ptr=event_queue[j];
			if(task_ptr->proc_id==kernel_request_pid){
				task_ptr->is_suspended = 0;
				check = 1;
				break;
			}
		}
	}
	if (check!=1) {
		/* Error code. */
		error_msg = ERR_RUN_3_PID_DOES_NOT_EXIST;
		OS_Abort();
	}
	
}
static void kernel_mutex_unlock(void) {
	uint8_t handle = (uint8_t) ((uint16_t) (kernel_request_mutex_ptr) - 1);
	if (handle >= num_mutex_created) {
		/* Error code. */
		error_msg = ERR_RUN_10_UNLOCK_ON_BAD_MUTEX;
		OS_Abort();
	}

	if (cur_task != mutex_list[handle].owner) {
		/* Error code. */
		error_msg = ERR_RUN_12_UNLOCK_ON_UNOWNED_MUTEX;
		OS_Abort();
	}

	else if (mutex_list[handle].rec_count > 1) {
		/* decrement lock count */
		mutex_list[handle].rec_count--;
	}
	else {  //rec_count == 1, release the lock
			cur_task->py = mutex_list[handle].orig_py;
			cur_task->orig_py = mutex_list[handle].orig_py;
		if (mutex_list[handle].mutex_queue.head == NULL ) {
			// free the mutex
			mutex_list[handle].rec_count = 0;
			mutex_list[handle].orig_py = 0;
			mutex_list[handle].inherited_py = 0;
			mutex_list[handle].owner = NULL;
		}
		else {
			
			/* transfer ownership */
			task_descriptor_t* new_owner = dequeue(&(mutex_list[handle].mutex_queue));
			//Check if the current task needs to be pre-empted
			if(cur_task->py > new_owner->py){
				cur_task->state = READY;
				enqueue(&(ready_queue[cur_task->py].py_queue), cur_task);
			}
			mutex_list[handle].owner = new_owner;
			mutex_list[handle].rec_count = 1;
			mutex_list[handle].orig_py = new_owner->py;
			mutex_list[handle].inherited_py = mutex_list[handle].orig_py;
			new_owner->state = READY;
			//enqueue(&priority_queue, new_owner);
			enqueue(&(ready_queue[new_owner->py].py_queue), new_owner);
		}
	}
}

static void kernel_update_ticker(void)
{
	/* PORTD ^= LED_D5_RED; */
	PORTB = (1<<PB7);
	cur_ticks++;
	/* Check sleeping tasks */
	if (sleep_queue.head != NULL) {
		--(sleep_queue.head->diff_ticks_remaining);
		uint8_t make_ready = 0;
		while (sleep_queue.head->diff_ticks_remaining == 0) {
			/* The waking task */
			task_descriptor_t* task_ptr = sleep_dequeue(&sleep_queue);

			task_ptr->state = READY;
			enqueue(&(ready_queue[task_ptr->py].py_queue), task_ptr);
			
			/* Check to see if current task needs to be pre-empted */
			if (cur_task != idle_task && !make_ready) {
				if (cur_task->py > task_ptr->py) {
					make_ready = 1;
				}
			}
			if (sleep_queue.head == NULL) {
				break;
			}
		}
		if (make_ready && cur_task != idle_task) {
			cur_task->state = READY;
			enqueue(&(ready_queue[cur_task->py].py_queue), cur_task);
		}

	}
	PORTB = (0<<PB7);
}
static void enqueue(queue_t* queue_ptr, task_descriptor_t* task_to_add) {
	task_to_add->next = NULL;

	if (queue_ptr->head == NULL) {
		/* empty queue */
		queue_ptr->head = task_to_add;
		queue_ptr->tail = task_to_add;
		} else {
		/* put task at the back of the queue */
		queue_ptr->tail->next = task_to_add;
		queue_ptr->tail = task_to_add;
	}
}


static task_descriptor_t* dequeue(queue_t* queue_ptr) {
	task_descriptor_t* task_ptr = queue_ptr->head;

	if (queue_ptr->head != NULL) {
		queue_ptr->head = queue_ptr->head->next;
			task_ptr->next = NULL;
	}

	return task_ptr;
}
static void sleep_enqueue(queue_t* queue_ptr, task_descriptor_t* task_to_add) {
	task_descriptor_t* next = queue_ptr->head;
	task_descriptor_t* prev = NULL;
	unsigned int tick_count = 0;
	if (queue_ptr->head == NULL) {
		queue_ptr->head = task_to_add;
		task_to_add->sleep_next = NULL;
		task_to_add->diff_ticks_remaining = kernel_request_sleep_ticks;
		return;
	}

	while (next != NULL) {
		if ((tick_count + next->diff_ticks_remaining)
		>= kernel_request_sleep_ticks) {
			task_to_add->sleep_next = next;
			task_to_add->diff_ticks_remaining = kernel_request_sleep_ticks
			- tick_count;
			next->diff_ticks_remaining -= task_to_add->diff_ticks_remaining;
			if (prev == NULL) {
				queue_ptr->head = task_to_add;
				} else {
				prev->sleep_next = task_to_add;
			}
			break;
		}
		tick_count += next->diff_ticks_remaining;
		prev = next;
		next = next->sleep_next;
	}
	if (next == NULL) {
		prev->sleep_next = task_to_add;
		task_to_add->sleep_next = NULL;
		queue_ptr->tail = task_to_add;
		task_to_add->diff_ticks_remaining = kernel_request_sleep_ticks
		- tick_count;
	}
}

static task_descriptor_t* sleep_dequeue(queue_t* queue_ptr) {
	task_descriptor_t* task_ptr = queue_ptr->head;

	if (queue_ptr->head != NULL) {
		queue_ptr->head = queue_ptr->head->sleep_next;
		task_ptr->sleep_next = NULL;
	}

	return task_ptr;
}


void OS_Init(){
    int i;

    /* Set up the clocks */
    //CLOCK8MHZ();

    TCCR1B &= ~(_BV(CS12)| _BV(CS11));
    TCCR1B |= (_BV(CS10));

    /*
    * Initialize dead pool to contain all but last task descriptor.
    *
    * DEAD == 0, already set in .init4
    */
    for (i = 0; i < MAXTHREAD - 1; i++) {
           task_desc[i].state = DEAD;
		   task_desc[i].py = NULL;
		   task_desc[i].orig_py = NULL;
		   
		   task_desc[i].proc_id =0;
		   task_desc[i].is_suspended = 0;
           task_desc[i].next = &task_desc[i + 1];
    }
	task_desc[MAXTHREAD - 1].next = NULL;
   for (i = 0; i < MAXMUTEX; i++){
           mutex_list[i].owner = NULL;
           mutex_list[i].mutex_queue.head = NULL;
           mutex_list[i].mutex_queue.tail = NULL;
           mutex_list[i].rec_count = 0;
           mutex_list[i].orig_py = 0;
		   mutex_list[i].inherited_py = 0;
    } 
    for(i=0; i < MINPRIORITY; i++){
        ready_queue[i].py = i;
        ready_queue[i].py_queue.head = NULL;
        ready_queue[i].py_queue.tail = NULL;

    }
	// initialize event queue
	for(i = 0; i < MAXEVENT; i++){
		event_queue[i] = NULL;
	}
  
    dead_pool_queue.head = &task_desc[0];
    dead_pool_queue.tail = &task_desc[MAXTHREAD - 1];

    /* Create idle "task" */
    kernel_request_create_args.f = (voidfuncvoid_ptr) idle;
    kernel_request_create_args.py = 9;
    kernel_create_task();

    /* Create "a_main" task as a task with the highest priority. */
    kernel_request_create_args.f = (voidfuncvoid_ptr) a_main;
    kernel_request_create_args.py = (PRIORITY) 0;
    kernel_create_task();

    /* First time through. Select "main" task to run first. */
    cur_task = task_desc;
    cur_task->state = RUNNING;
    dequeue(&(ready_queue[0].py_queue));

    /* Set up Timer 1 Output Compare interrupt,the TICK clock. */
	//TIMSK1 |= _BV(OCIE1A);
    //OCR1A = TCNT1 + 40000;
    /* Clear flag. */
    //TIFR1 = _BV(OCF1A); */
		TCCR0A = (1<<WGM01); //Set CTC bit
		OCR0A = 157;//Output compare register
		TIMSK0 = (1<<OCIE0A);
		TCCR0B = (1<<CS02) | (1<<CS00);

    /* Initialize the now counter */
    cur_ticks = 0; 

    /*
    * The main loop of the RTOS kernel.
    */
    kernel_main_loop();
}

void OS_Abort(){
	
	uint8_t i, j;
	uint8_t flashes, mask;

	Disable_Interrupt();

	/* Initialize port for output */
	DDRB = LED_RED_MASK | LED_GREEN_MASK;

	if (error_msg < ERR_RUN_1_USER_CALLED_OS_ABORT) {
		flashes = error_msg + 1;
		mask = LED_GREEN_MASK;
		} else {
		flashes = error_msg + 1 - ERR_RUN_1_USER_CALLED_OS_ABORT;
		mask = LED_RED_MASK;
	}

	for (;;) {
		DDRB = (uint8_t) (LED_RED_MASK| LED_GREEN_MASK);

		for (i = 0; i < 100; ++i) {
			_delay_ms(25);
		}

		DDRB = (uint8_t) 0;

		for (i = 0; i < 40; ++i) {
			_delay_ms(25);
		}

		for (j = 0; j < flashes; ++j) {
			DDRB = mask;

			for (i = 0; i < 10; ++i) {
				_delay_ms(25);
			}

			DDRB = (uint8_t) 0;

			for (i = 0; i < 10; ++i) {
				_delay_ms(25);
			}
		}

		for (i = 0; i < 20; ++i) {
			_delay_ms(25);
		}
	}
}

PID Task_Create(void(*f)(void), PRIORITY py, int arg) {
	
	//TEST_HIGH(RED);
	int retval;
	uint8_t sreg;

	sreg = SREG;
	Disable_Interrupt();

	kernel_request_create_args.f = (voidfuncvoid_ptr) f;
	kernel_request_create_args.arg = arg;
	kernel_request_create_args.py = (PRIORITY) py;
	
	kernel_request = TASK_CREATE;
	enter_kernel();

	retval = kernel_request_retval;
	SREG = sreg;

	return retval;
}


void Task_Terminate() {
	uint8_t sreg;

	sreg = SREG;
	Disable_Interrupt();

	kernel_request = TASK_TERMINATE;
	enter_kernel();

	SREG = sreg;
}

void Task_Yield() {
	uint8_t volatile sreg;
	
	sreg = SREG;
	Disable_Interrupt();

	kernel_request = TASK_YIELD;
	enter_kernel();

	SREG = sreg;
}
void Task_Suspend(PID p) {
	uint8_t volatile sreg;
	
	sreg = SREG;
	Disable_Interrupt();
	kernel_request_pid = p;
	kernel_request = TASK_SUSPEND;
	enter_kernel();

	SREG = sreg;
}
void Task_Resume(PID p) {
	uint8_t volatile sreg;
	
	sreg = SREG;
	Disable_Interrupt();
	kernel_request_pid = p;
	kernel_request = TASK_RESUME;
	enter_kernel();

	SREG = sreg;
}
int Task_GetArg(void) {
	int arg;
	uint8_t sreg;

	sreg = SREG;
	Disable_Interrupt();

	arg = cur_task->arg;

	SREG = sreg;

	return arg;
}

void Task_Sleep(unsigned int t) {
	uint8_t sreg;
	sreg = SREG;
	Disable_Interrupt();
	kernel_request = TASK_SLEEP;
	kernel_request_sleep_ticks = t;
	enter_kernel();
	SREG = sreg;
}


int main(void){
	
	
	DDRB = (1<<PB7);
	PORTB = (0<<PB7);
	OS_Init();

	
	for (;;);
	return 0;
}

EVENT* Event_Init(void) {
	EVENT* event_ptr;
	uint8_t sreg;

	sreg = SREG;
	Disable_Interrupt();

	kernel_request = EVENT_INIT;
	enter_kernel();

	event_ptr = (EVENT *) kernel_request_event_ptr;

	SREG = sreg;

	return event_ptr;
}

void Event_Wait(EVENT *e) {
	uint8_t sreg;

	sreg = SREG;
	Disable_Interrupt();

	kernel_request = EVENT_WAIT;
	kernel_request_event_ptr = e;
	enter_kernel();

	SREG = sreg;
}

void Event_Signal(EVENT *e) {
	uint8_t sreg;

	sreg = SREG;
	Disable_Interrupt();

	kernel_request = EVENT_SIGNAL;
	kernel_request_event_ptr = e;
	enter_kernel();

	SREG = sreg;
}
MUTEX* Mutex_Init(void) {
	uint8_t sreg;

	sreg = SREG;
	Disable_Interrupt();

	kernel_request = MUTEX_INIT;
	enter_kernel();

	MUTEX* mutex_return = (MUTEX *) kernel_request_mutex_ptr;

	SREG = sreg;

	return mutex_return;
}

void Mutex_Lock(MUTEX *m) {
	uint8_t sreg;

	sreg = SREG;
	Disable_Interrupt();

	kernel_request = MUTEX_LOCK;
	kernel_request_mutex_ptr = m;
	enter_kernel();

	SREG = sreg;
}

void Mutex_Unlock(MUTEX *m) {
	uint8_t sreg;

	sreg = SREG;
	Disable_Interrupt();

	kernel_request = MUTEX_UNLOCK;
	kernel_request_mutex_ptr = m;
	enter_kernel();

	SREG = sreg;
}



void TIMER0_COMPA_vect(void) {
        /*
         * Save the interrupted task's context on its stack,
         * and save the stack pointer.
         *
         * On the cur_task's stack, the registers and SREG are
         * saved in the right order, but we have to modify the stored value
         * of SREG. We know it should have interrupts enabled because this
         * ISR was able to execute, but it has interrupts disabled because
         * it was stored while this ISR was executing. So we set the bit (I = bit 7)
         * in the stored value.
         */
		
		//PORTB =(1<<PB7);
        SAVE_CTX_TOP();

        STACK_SREG_SET_I_BIT();

        SAVE_CTX_BOTTOM();

        cur_task->sp = (uint8_t*) SP;

        /*
         * Now that we already saved a copy of the stack pointer
         * for every context including the kernel, we can move to
         * the kernel stack and use it. We will restore it again later.
         */
        SP = kernel_sp;

        /*
         * Inform the kernel that this task was interrupted.
         */
        kernel_request = TIMER_EXPIRED;

        /*
         * Prepare for next tick interrupt.
         */
        //OCR1A += TICK_CYCLES;

        /*
         * Restore the kernel context. (The stack pointer is restored again.)
         */
        SP = kernel_sp;
		
        /*
         * Now restore I/O and SREG registers.
         */
	
        RESTORE_CTX();
		
        /*
         * We use "ret" here, not "reti", because we do not want to
         * enable interrupts inside the kernel.
         * Explicitly required as we are "naked".
         *
         * The last piece of the context, the PC, is popped off the stack
         * with the ret instruction.
         */
        asm volatile ("ret\n"::);
}