/**
 * @file   error_code.h
 *
 * @brief Error messages returned in OS_Abort().
 *        Green errors are initialization errors
 *        Red errors are runt time errors
 *
 * CSC 460/560 Real Time Operating Systems - Mantis Cheng
 *
 * @author Scott Craig
 * @author Justin Tanner
 */
#ifndef __ERROR_CODE_H__
#define __ERROR_CODE_H__

enum  	{
			ERR_RUN_1_USER_CALLED_OS_ABORT, 
			ERR_RUN_2_TOO_MANY_TASKS, 
			ERR_RUN_3_PID_DOES_NOT_EXIST,
			ERR_RUN_4_SIGNAL_ON_BAD_EVENT, 
			ERR_RUN_5_WAIT_ON_BAD_EVENT, 
			ERR_RUN_6_ILLEGAL_ISR_KERNEL_REQUEST,
			ERR_RUN_8_RTOS_INTERNAL_ERROR, 
			ERR_RUN_9_LOCK_ON_BAD_MUTEX, 
			ERR_RUN_10_UNLOCK_ON_BAD_MUTEX,
			ERR_RUN_12_UNLOCK_ON_UNOWNED_MUTEX,
			MULTIPLE_TASKS_WAITING
};


#endif
