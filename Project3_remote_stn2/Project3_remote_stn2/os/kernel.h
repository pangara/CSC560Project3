/*
 * kernel.h
 *
 * Created: 12/03/2016 2:13:54 PM
 *  Author: pangara
 */

#ifndef KERNEL_H_
#define KERNEL_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include "error_code.h"
#include "os.h"

#define F_CPU 16000000UL
#define Disable_Interrupt()     asm volatile ("cli"::)
#define Enable_Interrupt()     asm volatile ("sei"::)

/** LEDs for OS_Abort() */
#define LED_RED_MASK    _BV(PB1)	// Pin 13 on 2560

/** LEDs for OS_Abort() */
#define LED_GREEN_MASK    _BV(PB2)

//Are we using this?
#define TICK_CYCLES     (F_CPU / 1000 * MSECPERTICK)
/**
 * @brief This is the set of states that a task can be in at any given time.
 */

typedef enum
{
	DEAD = 0,
	RUNNING,
	READY,
	WAITING,
	SLEEPING
} task_state_t;

typedef void (*voidfuncvoid_ptr)(void);      /* pointer to void f(void) */

typedef enum
{
	NONE = 0,
	TIMER_EXPIRED, //done
	//task
	TASK_CREATE,  //done
	TASK_TERMINATE, //done
	TASK_YIELD,  //done
	TASK_GET_ARG, //done
	TASK_SUSPEND,
	TASK_RESUME,
	TASK_SLEEP, //done
	//event
	EVENT_INIT, //done
	EVENT_WAIT, //done
	EVENT_SIGNAL, //done
	//mutex
	MUTEX_INIT,
	MUTEX_LOCK,
	MUTEX_UNLOCK
} kernel_request_t;


/**
 * @brief The arguments required to create a task.
 */

typedef struct
{
    /** The code the new task is to run.*/
    voidfuncvoid_ptr f;
    /** A new task may be created with an argument that it can retrieve later. */
    int arg;
    /** Priority of the new task */
    PRIORITY py;
} create_args_t;


typedef struct td_struct task_descriptor_t;

/**
 * @brief All the data needed to describe the task, including its context.
 */
struct td_struct
{
    /** The stack used by the task. SP points in here when task is RUNNING. */
    uint8_t                         stack[WORKSPACE];
	uint8_t							proc_id;
    /** A variable to save the hardware SP into when the task is suspended. */
    uint8_t*               volatile sp;   /* stack pointer into the "workSpace" */
    /** The state of the task in this descriptor. */
    task_state_t                    state;
    /** The argument passed to Task_Create for this task. */
    //P: I don't know what arg is used for.
    int                             arg;
    /** The priority (type) of this task. */
    PRIORITY                         py;
    /** A link to the next task descriptor in the queue holding this task. */
    task_descriptor_t*              next;
    //A link to the next task descriptor in the queue holding these sleeping tasks
    task_descriptor_t*              sleep_next;
	//If a task is suspended, is_suspended = 1
	uint8_t							is_suspended;
	PRIORITY						orig_py;
    //The number of ticks remaining after the previous item in the queue is released. If it is the first item in the queue it is the number of ticks until it is released.
    unsigned int                    diff_ticks_remaining;
};

typedef struct
{
	/** The first item in the queue. NULL if the queue is empty. */
	task_descriptor_t*  head;
	/** The last item in the queue. Undefined if the queue is empty. */
	task_descriptor_t*  tail;
}
queue_t;

typedef struct
{
	PRIORITY py;
	queue_t py_queue;
}queue_p;

//Modify this to include recursive mutex desc/priority inheritance
typedef struct {
	//pointer to the task that currently owns this mutex. NULL if the mutex is free.
	task_descriptor_t *owner;
	//a queue that stores waiting tasks in FIFO ordering
	queue_t mutex_queue;
	//a counter that keeps track of recursive locking and unlocking of the mutex
	uint8_t rec_count;
	//previous priority: required for priority inheritance.
	PRIORITY orig_py;
	PRIORITY inherited_py;
} MUTEX_DATA;


#endif /* KERNEL_H_ */

