/******************************************************
The interface to the operating system
process and thread control primitives

(c) 1995 Innobase Oy

Created 9/8/1995 Heikki Tuuri
*******************************************************/

#include "os0thread.h"
#ifdef UNIV_NONINL
#include "os0thread.ic"
#endif

#ifdef __WIN__
#include <windows.h>
#endif

/*********************************************************************
Returns the thread identifier of current thread. */

os_thread_id_t
os_thread_get_curr_id(void)
/*=======================*/
{
#ifdef __WIN__
	return(GetCurrentThreadId());
#else
	return((os_thread_id_t) pthread_self());
#endif
}

/* Define a function pointer type to use in a typecast */
typedef void* (*os_posix_f_t) (void*);

/********************************************************************
Creates a new thread of execution. The execution starts from
the function given. The start function takes a void* parameter
and returns an ulint. */

os_thread_t
os_thread_create(
/*=============*/
						/* out: handle to the thread */
	ulint (*start_f)(void*),		/* in: pointer to function
						from which to start */
	void*			arg,		/* in: argument to start
						function */
	os_thread_id_t*		thread_id)	/* out: id of created
						thread */	
{
#ifdef __WIN__
	os_thread_t	thread;

	thread = CreateThread(NULL,	/* no security attributes */
				0,	/* default size stack */
				(LPTHREAD_START_ROUTINE)start_f,
				arg,
				0,	/* thread runs immediately */
				thread_id);
	ut_a(thread);

	return(thread);
#else
	int		ret;
	os_thread_t	pthread;

	/* Note that below we cast the start function returning an integer
	to a function returning a pointer: this may cause error
	if the return value is used somewhere! */

	ret = pthread_create(&pthread, NULL, (os_posix_f_t) start_f, arg);

	return(pthread);
#endif
}

/*********************************************************************
Returns handle to the current thread. */

os_thread_t
os_thread_get_curr(void)
/*=======================*/
{
#ifdef __WIN__
	return(GetCurrentThread());
#else
	return(pthread_self());
#endif
}

/*********************************************************************
Converts a thread id to a ulint. */

ulint
os_thread_conv_id_to_ulint(
/*=======================*/
				/* out: converted to ulint */
	os_thread_id_t	id)	/* in: thread id */
{
	return((ulint)id);
}
	
/*********************************************************************
Advises the os to give up remainder of the thread's time slice. */

void
os_thread_yield(void)
/*=================*/
{
#ifdef __WIN__	
	Sleep(0);
#else
	os_thread_sleep(0);
#endif
}

/*********************************************************************
The thread sleeps at least the time given in microseconds. */

void
os_thread_sleep(
/*============*/
	ulint	tm)	/* in: time in microseconds */
{
#ifdef __WIN__
	Sleep(tm / 1000);
#else
	struct timeval	t;

	t.tv_sec = 0;
	t.tv_usec = tm;
	
	select(0, NULL, NULL, NULL, &t);
#endif
}

/**********************************************************************
Sets a thread priority. */

void
os_thread_set_priority(
/*===================*/
	os_thread_t	handle,	/* in: OS handle to the thread */
	ulint		pri)	/* in: priority */
{
#ifdef __WIN__
	int	os_pri;

	if (pri == OS_THREAD_PRIORITY_BACKGROUND) {
		os_pri = THREAD_PRIORITY_BELOW_NORMAL;
	} else if (pri == OS_THREAD_PRIORITY_NORMAL) {
		os_pri = THREAD_PRIORITY_NORMAL;
	} else if (pri == OS_THREAD_PRIORITY_ABOVE_NORMAL) {
		os_pri = THREAD_PRIORITY_HIGHEST;
	} else {
		ut_error;
	}

	ut_a(SetThreadPriority(handle, os_pri));
#else
	UT_NOT_USED(handle);
	UT_NOT_USED(pri);
#endif
}

/**********************************************************************
Gets a thread priority. */

ulint
os_thread_get_priority(
/*===================*/
				/* out: priority */
	os_thread_t	handle)	/* in: OS handle to the thread */
{
#ifdef __WIN__
	int	os_pri;
	ulint	pri;

	os_pri = GetThreadPriority(handle);

	if (os_pri == THREAD_PRIORITY_BELOW_NORMAL) {
		pri = OS_THREAD_PRIORITY_BACKGROUND;
	} else if (os_pri == THREAD_PRIORITY_NORMAL) {
		pri = OS_THREAD_PRIORITY_NORMAL;
	} else if (os_pri == THREAD_PRIORITY_HIGHEST) {
		pri = OS_THREAD_PRIORITY_ABOVE_NORMAL;
	} else {
		ut_error;
	}

	return(pri);
#else
	return(0);
#endif
}

/**********************************************************************
Gets the last operating system error code for the calling thread. */

ulint
os_thread_get_last_error(void)
/*==========================*/
{
#ifdef __WIN__
	return(GetLastError());
#else
	return(0);
#endif
}
