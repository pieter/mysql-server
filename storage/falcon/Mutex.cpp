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



#ifdef _WIN32
#include <windows.h>
#undef ERROR
#undef ASSERT
#endif

#include "Engine.h"
#include "Mutex.h"

#ifndef ASSERT
#define ASSERT(c)
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Mutex::Mutex()
{
#ifdef _WIN32
	//mutex = CreateMutex (NULL, false, NULL);
	InitializeCriticalSection (&criticalSection);
#endif

#ifdef _PTHREADS
	//int ret = 
	pthread_mutex_init (&mutex, NULL);
#endif

#ifdef SOLARIS_MT
	int ret = mutex_init (&mutex, USYNC_THREAD, NULL);
#endif

	holder = NULL;
}

Mutex::~Mutex()
{
#ifdef _WIN32
	//CloseHandle (mutex);
	DeleteCriticalSection (&criticalSection);
#endif

#ifdef _PTHREADS
	//int ret = 
	pthread_mutex_destroy (&mutex);
#endif

#ifdef SOLARIS_MT
	int ret = mutex_destroy (&mutex);
#endif
}

void Mutex::lock()
{
#ifdef _WIN32
	//int result = WaitForSingleObject (mutex, INFINITE);
	EnterCriticalSection (&criticalSection);
#endif

#ifdef _PTHREADS
	int ret = pthread_mutex_lock (&mutex);
	ASSERT(ret == 0);
#endif

#ifdef SOLARIS_MT
	int ret = mutex_lock (&mutex);
#endif
}

void Mutex::release()
{
#ifdef _WIN32
	//ReleaseMutex (mutex);
	LeaveCriticalSection (&criticalSection);
#endif

#ifdef _PTHREADS
	int ret = pthread_mutex_unlock (&mutex);
	ASSERT(ret == 0);
#endif

#ifdef SOLARIS_MT
	int ret = mutex_unlock (&mutex);
#endif
}

void Mutex::unlock(Sync* sync, LockType type)
{
	holder = NULL;
	release();
}

void Mutex::unlock()
{
	holder = NULL;
	release();
}

void Mutex::lock(Sync* sync, LockType type, int timeout)
{
	lock();
	holder = sync;
}

void Mutex::findLocks(LinkedList& threads, LinkedList& syncObjects)
{
}
