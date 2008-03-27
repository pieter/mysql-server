/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// Synchronize.cpp: implementation of the Synchronize class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#ifdef _PTHREADS
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#endif

#include "Engine.h"
#include "Synchronize.h"
#include "Interlock.h"
#include "Mutex.h"

#ifdef ENGINE
#include "Log.h"
#define CHECK_RET(text,code)	if (ret) Error::error (text,code)
#else
#define CHECK_RET(text,code)	
#endif

#ifdef SYNCHRONIZE_FREEZE
INTERLOCK_TYPE synchronizeFreeze;
#endif

#define NANO		QUAD_CONSTANT(1000000000)
#define MICRO		1000000

#if defined(_PTHREADS) && !(_POSIX_TIMERS > 0)
extern "C" int64 my_getsystime();
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Synchronize::Synchronize()
{
	shutdownInProgress = false;
	sleeping = false;
	wakeup = false;

#ifdef _WIN32
	event = CreateEvent (NULL, false, false, NULL);
#endif

#ifdef _PTHREADS
	//pthread_mutexattr_t attr = PTHREAD_MUTEX_FAST_NP;
	//int ret = 
	pthread_mutex_init (&mutex, NULL);
	pthread_cond_init (&condition, NULL);
#endif
}

Synchronize::~Synchronize()
{
#ifdef _WIN32
	CloseHandle (event);
#endif

#ifdef _PTHREADS
	int ret = pthread_mutex_destroy (&mutex);
	ret = pthread_cond_destroy (&condition);
#endif
}


void Synchronize::sleep()
{
#ifdef _WIN32
#ifdef _DEBUG

	for (;;)
		{
		sleeping = true;
		int n = WaitForSingleObject (event, 10000);
		sleeping = false;
		
		if (n != WAIT_TIMEOUT)
			{
			DEBUG_FREEZE;
			return;
			}
		}
#else
	sleeping = true;
	sleep (INFINITE);
	sleeping = false;
#endif
#endif

#ifdef _PTHREADS
	sleeping = true;
	int ret = pthread_mutex_lock (&mutex);
	CHECK_RET ("pthread_mutex_lock failed, errno %d", errno);

	while (!wakeup)
		pthread_cond_wait (&condition, &mutex);

	sleeping = false;
	wakeup = false;
	ret = pthread_mutex_unlock (&mutex);
	CHECK_RET ("pthread_mutex_unlock failed, errno %d", errno);
#endif

	DEBUG_FREEZE;
}

bool Synchronize::sleep(int milliseconds)
{
	return sleep(milliseconds, NULL);
}

bool Synchronize::sleep(int milliseconds, Mutex *callersMutex)
{
	sleeping = true;

#ifdef _WIN32
	if (callersMutex)
		callersMutex->release();
	int n = WaitForSingleObject(event, milliseconds);
	if (callersMutex)
		callersMutex->lock();
	sleeping = false;
	DEBUG_FREEZE;

	return n != WAIT_TIMEOUT;
#endif

#ifdef _PTHREADS
	int ret = pthread_mutex_lock (&mutex);
	CHECK_RET("pthread_mutex_lock failed, errno %d", errno);
	if (callersMutex)
		callersMutex->release();
	struct timespec nanoTime;

#if _POSIX_TIMERS > 0
	ret = clock_gettime(CLOCK_REALTIME, &nanoTime);
	CHECK_RET("clock_gettime failed, errno %d", errno);
	int64 start = nanoTime.tv_sec * NANO + nanoTime.tv_nsec;
	int64 nanos = nanoTime.tv_nsec + (int64) milliseconds * 1000000;
	nanoTime.tv_sec += nanos / NANO;
	nanoTime.tv_nsec = nanos % NANO;
#else
	int64 start = my_getsystime();
	int64 ticks = start + (milliseconds * 10000);
	nanoTime.tv_sec = ticks / (NANO/100);
	nanoTime.tv_nsec = (ticks % (NANO/100)) * 100;
#endif

	while (!wakeup)
		{
		ret = pthread_cond_timedwait(&condition, &mutex, &nanoTime);
		
		if (ret == ETIMEDOUT)
			{
#if _POSIX_TIMERS > 0
			clock_gettime(CLOCK_REALTIME, &nanoTime);
			int64 delta = (int64) nanoTime.tv_sec * NANO + nanoTime.tv_nsec - start;
			int millis = delta / 1000000;
#else
			int64 delta = my_getsystime() - start;
			int millis = delta / 10000;
#endif
			
			if (millis < milliseconds)
				Log::debug("Timeout after %d milliseconds (expected %d)\n", millis, milliseconds);
				
			break;
			}
			
		if (!wakeup)
#ifdef ENGINE
			Log::debug ("Synchronize::sleep(milliseconds): unexpected wakeup, ret %d\n", ret);
#else
			printf ("Synchronize::sleep(milliseconds): unexpected wakeup, ret %d\n", ret);
#endif
		}


	sleeping = false;
	wakeup = false;
	if (callersMutex)
		callersMutex->lock();
	pthread_mutex_unlock(&mutex);
	DEBUG_FREEZE;

	return ret != ETIMEDOUT;
#endif
}

void Synchronize::wake()
{
#ifdef _WIN32
	SetEvent (event);
#endif

#ifdef _PTHREADS
	int ret = pthread_mutex_lock (&mutex);
	CHECK_RET ("pthread_mutex_lock failed, errno %d", errno);
	wakeup = true;
	pthread_cond_broadcast (&condition);
	ret = pthread_mutex_unlock (&mutex);
	CHECK_RET ("pthread_mutex_unlock failed, errno %d", errno);
#endif
}

void Synchronize::shutdown()
{
	shutdownInProgress = true;
	wake();
}

void Synchronize::freeze(void)
{
#ifdef SYNCHRONIZE_FREEZE
	COMPARE_EXCHANGE(&synchronizeFreeze, synchronizeFreeze, 0);
#endif
}

void Synchronize::freezeSystem(void)
{
#ifdef SYNCHRONIZE_FREEZE
	COMPARE_EXCHANGE(&synchronizeFreeze, synchronizeFreeze, true);
	freeze();
#endif
}
