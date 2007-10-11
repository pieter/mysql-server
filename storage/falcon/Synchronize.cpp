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
//#include <AFX.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#ifdef _PTHREADS
#include <pthread.h>
#include <errno.h>
#endif

#include "Engine.h"
#include "Synchronize.h"

#ifdef ENGINE
#include "Log.h"
#define CHECK_RET(text,code)	if (ret) Error::error (text,code)
#else
#define CHECK_RET(text,code)	
#endif

#define NANO		1000000000
#define MICRO		1000000

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
	sleeping = true;
#ifdef _WIN32
#ifdef _DEBUG
	for (;;)
		{
		int n = WaitForSingleObject (event, 10000);
		sleeping = false;
		if (n != WAIT_TIMEOUT)
			return;
		}
#else
	sleep (INFINITE);
#endif
#endif

#ifdef _PTHREADS
	int ret = pthread_mutex_lock (&mutex);
	CHECK_RET ("pthread_mutex_lock failed, errno %d", errno);

	while (!wakeup)
		pthread_cond_wait (&condition, &mutex);

	wakeup = false;
	ret = pthread_mutex_unlock (&mutex);
	CHECK_RET ("pthread_mutex_unlock failed, errno %d", errno);
#endif
	sleeping = false;
}

bool Synchronize::sleep(int milliseconds)
{
	sleeping = true;

#ifdef _WIN32
	int n = WaitForSingleObject(event, milliseconds);
	sleeping = false;

	return n != WAIT_TIMEOUT;
#endif

#ifdef _PTHREADS
	struct timeval microTime;
	int ret = gettimeofday(&microTime, NULL);
	int64 nanos = (int64) microTime.tv_sec * NANO + microTime.tv_usec * 1000 +
				 (int64) milliseconds * 1000000;
	struct timespec nanoTime;
	nanoTime.tv_sec = nanos / NANO;
	nanoTime.tv_nsec = nanos % NANO;
	ret = pthread_mutex_lock (&mutex);
	CHECK_RET("pthread_mutex_lock failed, errno %d", errno);

	while (!wakeup)
		{
		ret = pthread_cond_timedwait(&condition, &mutex, &nanoTime);
		
		if (ret == ETIMEDOUT)
			{
			/***
			struct timeval endTime;
			gettimeofday (&endTime, NULL);
			waitTime = ((int64) endTime.tv_sec * MICRO + endTime.tv_usec) -
					   ((int64) microTime.tv_sec * MICRO + microTime.tv_usec);
			***/
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
	pthread_mutex_unlock(&mutex);

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
