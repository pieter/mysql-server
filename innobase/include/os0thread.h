/******************************************************
The interface to the operating system
process and thread control primitives

(c) 1995 Innobase Oy

Created 9/8/1995 Heikki Tuuri
*******************************************************/

#ifndef os0thread_h
#define os0thread_h

#include "univ.i"

/* Maximum number of threads which can be created in the program;
this is also the size of the wait slot array for MySQL threads which
can wait inside InnoDB */
#ifdef __WIN__
/* Windows 95/98/ME seemed to have difficulties creating the all
the event semaphores for the wait array slots. If the computer had
<= 64 MB memory, InnoDB startup could take minutes or even crash.
That is why we set this to only 1000 in Windows. */

#define	OS_THREAD_MAX_N		1000
#else
#define	OS_THREAD_MAX_N		10000
#endif

/* Possible fixed priorities for threads */
#define OS_THREAD_PRIORITY_NONE		100
#define OS_THREAD_PRIORITY_BACKGROUND	1
#define OS_THREAD_PRIORITY_NORMAL	2
#define OS_THREAD_PRIORITY_ABOVE_NORMAL	3

#ifdef __WIN__
typedef void*			os_thread_t;
typedef ulint			os_thread_id_t;	/* In Windows the thread id
						is an unsigned long int */
#else
typedef pthread_t               os_thread_t;
typedef os_thread_t          	os_thread_id_t;	/* In Unix we use the thread
						handle itself as the id of
						the thread */
#endif


/* Define a function pointer type to use in a typecast */
typedef void* (*os_posix_f_t) (void*);

/*******************************************************************
Compares two thread ids for equality. */

ibool
os_thread_eq(
/*=========*/
				/* out: TRUE if equal */
	os_thread_id_t	a,	/* in: OS thread or thread id */
	os_thread_id_t	b);	/* in: OS thread or thread id */
/********************************************************************
Converts an OS thread id to a ulint. It is NOT guaranteed that the ulint is
unique for the thread though! */

ulint
os_thread_pf(
/*=========*/
				/* out: unsigned long int */
	os_thread_id_t	a);	/* in: thread or thread id */
/********************************************************************
Creates a new thread of execution. The execution starts from
the function given. The start function takes a void* parameter
and returns a ulint. */

os_thread_t
os_thread_create(
/*=============*/
						/* out: handle to the thread */
#ifndef __WIN__
		 os_posix_f_t            start_f,
#else
	ulint (*start_f)(void*),		/* in: pointer to function
						from which to start */
#endif
	void*			arg,		/* in: argument to start
						function */
	os_thread_id_t*		thread_id);	/* out: id of the created
						thread */
/*********************************************************************
A thread calling this function ends its execution. */

void
os_thread_exit(
/*===========*/
	ulint	code);	/* in: exit code */
/*********************************************************************
Returns the thread identifier of current thread. */

os_thread_id_t
os_thread_get_curr_id(void);
/*========================*/
/*********************************************************************
Returns handle to the current thread. */

os_thread_t
os_thread_get_curr(void);
/*====================*/
/*********************************************************************
Waits for a thread to terminate. */

void
os_thread_wait(
/*===========*/
	os_thread_t	thread);	/* in: thread to wait */
/*********************************************************************
Advises the os to give up remainder of the thread's time slice. */

void
os_thread_yield(void);
/*=================*/
/*********************************************************************
The thread sleeps at least the time given in microseconds. */

void
os_thread_sleep(
/*============*/
	ulint	tm);	/* in: time in microseconds */
/**********************************************************************
Gets a thread priority. */

ulint
os_thread_get_priority(
/*===================*/
				/* out: priority */
	os_thread_t	handle);/* in: OS handle to the thread */
/**********************************************************************
Sets a thread priority. */

void
os_thread_set_priority(
/*===================*/
	os_thread_t	handle,	/* in: OS handle to the thread */
	ulint		pri);	/* in: priority: one of OS_PRIORITY_... */
/**********************************************************************
Gets the last operating system error code for the calling thread. */

ulint
os_thread_get_last_error(void);
/*==========================*/


#ifndef UNIV_NONINL
#include "os0thread.ic"
#endif

#endif 
