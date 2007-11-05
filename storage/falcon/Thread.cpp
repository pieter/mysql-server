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

// Thread.cpp: implementation of the Thread class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#undef ERROR
#undef ASSERT
#endif

#include <errno.h>
#include "Engine.h"
#include "Thread.h"
#include "Threads.h"
#include "SyncObject.h"
#include "Sync.h"
#include "Log.h"
#include "SyncWait.h"
#include "LinkedList.h"
#include "SQLError.h"
#include "MemMgr.h"
#include "NetfraVersion.h"
#include "Interlock.h"
#include "Mutex.h"

#ifdef _WIN32
static int threadIndex = TlsAlloc();
#endif

#ifdef _PTHREADS
static pthread_key_t	threadIndex;
#endif

static Mutex	exitMutex;
static int		initThreads();
static int		initialized = initThreads();

#ifndef ASSERT
#define ASSERT(bool)
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static int initThreads()
{
#ifdef _PTHREADS
	//int keyCreateRet = 
	pthread_key_create (&threadIndex, NULL);
#endif

#ifdef _WIN32
	threadIndex = TlsAlloc();
#endif

	return 1;
}


Thread::Thread(const char *desc)
{
	init(desc);
}


//Thread::Thread(const char *desc, Threads *threads, void (*fn)(void*), void *arg, Threads *barn)
Thread::Thread(const char *desc, Threads *barn)
{
	init(desc);
	setThreadBarn (barn);
	//createThread (fn, arg);
}

void Thread::init(const char *desc)
{
	//printf ("Thread::init %s %x\n", desc, this);
	description = desc;
	useCount = 1;
	activeLocks = 0;
	locks = NULL;
	lockPending = NULL;
	syncWait = NULL;
	lockType = None;
	defaultTimeZone = NULL;
	javaThread = NULL;
	threadId = 0;
	threadBarn = NULL;
	where = NULL;
	pageMarks = 0;
	lockGranted = false;
	prior = NULL;
	next = NULL;
}

Thread::~Thread()
{
#ifdef ENGINE
	//Log::log ("deleting thread %x: %s\n", threadId, description);
#endif

	ASSERT(useCount == 0);
	ASSERT(activeLocks == 0);
	ASSERT(!lockPending);
	ASSERT(!locks);
	ASSERT(!threadBarn);
	ASSERT(!pageMarks);
	
	if (threadBarn)
		setThreadBarn (NULL);

	setThread (NULL);
}

THREAD_RET Thread::thread(void * parameter)
{
	Thread *thread = (Thread*) parameter;
	thread->thread();
	lockExitMutex();
	unlockExitMutex();
	thread->release();

	return 0;
}

void Thread::thread()
{
	setThread (this);

	try
		{
		while (!shutdownInProgress)
			{
			locks = NULL;
			lockPending = NULL;

			if (function)
				{
				(*function)(argument);

				if (!shutdownInProgress)
					{
					ASSERT (locks == NULL);
					ASSERT (javaThread == NULL);
					}

				function = NULL;

				if (!shutdownInProgress && threadBarn)
					threadBarn->inactive (this);
				}

			if (shutdownInProgress)
				break;

			/***
			if (activeLocks)
				activeLocks = 0;
			***/

			description = "sleeping";
			//ASSERT (threadBarn == NULL);

			if (!function)
				sleep();
			}
		}
	catch (SQLException& exception)
		{
#ifdef ENGINE
		Log::log ("Thread::thread: thread %d: %s\n", threadId, exception.getText());
#endif
		release();
		throw;
		}
	catch (...)
		{
#ifdef ENGINE
		Log::log ("Thread::thread: unexpected exception, rethrowing, thread %d\n", threadId);
#endif
		release();
		throw;
		}

	if (threadBarn)
		setThreadBarn (NULL);

#ifdef ENGINE
	if (shutdownInProgress)
		; //Log::log ("Thread::thread: %x exitting\n", threadId);
	else
		Log::log ("Thread::thread: %x exitting unexpectedly\n", threadId);
#endif

	release();
}

void Thread::start(const char *desc, void (*fn)(void*), void * arg)
{
	description = desc;
	function = fn;
	argument = arg;
	wake();
}

Thread* Thread::getThread(const char *desc)
{
	if (!initialized)
		return NULL;

	Thread *thread = findThread();

	if (!thread)
		{
		thread = new Thread (desc);
		setThread (thread);
		}

#ifdef _WIN32
	if (!thread->threadId)
		thread->threadId = GetCurrentThreadId();
#endif

#ifdef _PTHREADS
	if (!thread->threadId)
		thread->threadId = pthread_self();
#endif

	return thread;
}


void Thread::deleteThreadObject()
{
	Thread *thread = findThread();

	if (thread)
		{
		thread->setThreadBarn (NULL);
		thread->release();
		}
}

void Thread::setThread(Thread * thread)
{
#ifdef _WIN32
	/***
	ASSERT (!(thread == (Thread*) 0x93e0f0 && thread->threadId == 0));
	Thread *old = (Thread*) TlsGetValue (threadIndex);
	ASSERT (thread == NULL || old == NULL || old == thread);

	if (thread)
		printf ("setting %x id %x\n", thread, thread->threadId);
	else
		printf ("clearing %x\n", old);
	***/

	TlsSetValue (threadIndex, thread);
#endif

#ifdef _PTHREADS
	//int ret = 
	pthread_setspecific (threadIndex, thread);
#endif
}

void Thread::grantLock(SyncObject *lock)
{
	ASSERT (!lockGranted);
	ASSERT (!lockPending || lockPending->syncObject == lock);
	lockGranted = true;
	lockPending = NULL;

	wake();
}

void Thread::validateLocks()
{
	Thread *thread = getThread("Thread::validateLocks");

	if (thread->locks)
		{
		LOG_DEBUG ("thread %d has active locks:\n", thread->threadId);
		for (Sync *sync = thread->locks; sync; sync = sync->prior)
			LOG_DEBUG ("   %s\n", sync->where);
		}

}

void Thread::createThread(void (*fn)(void *), void *arg)
{
	function = fn;
	argument = arg;
	addRef();

	/***
	if (description && strcmp(description, "PageWriter::start") == 0)
		throw SQLError(RUNTIME_ERROR, "simulated thread failure");
	***/
	
#ifdef _WIN32
	if (!(threadHandle = CreateThread (NULL, 0, thread, this, 0, &threadId)))
		{
		LOG_DEBUG ("CreateThread failed, last error %d\n", GetLastError());
		
		if (threadBarn)
			threadBarn->print();
			
		throw SQLError (RUNTIME_ERROR, "CreateThread failed (%d)\n", GetLastError());
		}
#endif

#ifdef _PTHREADS
	pthread_attr_t attributes;
	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
	int ret = pthread_create ((pthread_t *) &threadId, &attributes, thread, this);
	pthread_attr_destroy(&attributes);
	
	if (ret)
		{
		LOG_DEBUG ("pthread_create failed, errno %d\n", errno);
		
		if (threadBarn)
			threadBarn->print();
			
#ifdef ENGINE
		MemMgrLogDump();
#endif

		throw SQLError (RUNTIME_ERROR, "pthread_create failed, ret %d, errno %d\n", ret, errno);
		}
#endif
}

void Thread::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void Thread::release()
{
	ASSERT (useCount > 0);

	if (INTERLOCKED_DECREMENT (useCount) == 0)
		delete this;
}

void Thread::setLock(Sync *sync)
{
	ASSERT (sync != locks);
	ASSERT (!locks || (locks->state == Shared || locks->state == Exclusive));
	ASSERT (sync->request == Shared || sync->request == Exclusive);
	sync->prior = locks;
	locks = sync;
	where = locks->where;
}

void Thread::clearLock(Sync *sync)
{
	if (locks != sync)
		{
		ASSERT (locks == sync);
		}

	ASSERT (sync->state == Shared || sync->state == Exclusive);

	if ( (locks = sync->prior) )
		{
		ASSERT (locks->state == Shared || locks->state == Exclusive);
		where = locks->where;
		}
}

void Thread::findLocks(LinkedList &threads, LinkedList &syncObjects)
{
	if (threads.appendUnique (this))
		{
		for (Sync *sync = locks; sync; sync = sync->prior)
			sync->findLocks(threads, syncObjects);
			
		if (lockPending)
			lockPending->findLocks(threads, syncObjects);
		}
}

void Thread::print()
{
#ifdef STORAGE_ENGINE
	LOG_DEBUG ("  Thread %p (%d) sleep=%d, grant=%d, locks=%d, who %d, parent=%p\n",
				this, threadId, sleeping, lockGranted, activeLocks, lockGranted, threadBarn);
#else
	LOG_DEBUG ("  Thread %p (%d) sleep=%d, grant=%d, locks=%d, who %d, javaThread %p\n"
			   "      prior=%p, next=%p, parent=%p\n",
				this, threadId, sleeping, lockGranted, activeLocks, lockGranted, javaThread,
				prior, next, threadBarn);
#endif

	for (Sync *sync = locks; sync; sync = sync->prior)
		sync->print ("    Holding");

	if (lockPending)
		lockPending->print ("    Pending");
}

/***
void Thread::print(int level)
{
	marked = true;
	LOG_DEBUG ("%*sThread %d (%x) sleeping=%d, granted=%d, locks=%d, who %d\n", level * 2, "", 
				threadId, this, sleeping, lockGranted, activeLocks, lockGranted);
	Sync *sync;
	++level;

	for (sync = locks; sync; sync = sync->prior)
		{
		LOG_DEBUG ("%*sHolding:\n", level * 2, "");
		sync->print(level + 1);
		}

	if (sync = lockPending)
		{
		LOG_DEBUG ("%*sPending:\n", level * 2, "");
		sync->print(level + 1);
		}

	if (syncWait)
		{
		LOG_DEBUG ("%*sWaiting:\n", level * 2, "");
		syncWait->print(level + 1);
		}
}
***/

void Thread::print(const char *label)
{
	LOG_DEBUG ("%s %x (%d), type %d; %s\n", label, this, threadId, lockType, getWhere()); 
}

const char* Thread::getWhere()
{
	if (lockPending && lockPending->where)
		return lockPending->where;

	return "";
}

void Thread::setTimeZone(const TimeZone *timeZone)
{
	defaultTimeZone = timeZone;
}

Thread* Thread::findThread()
{
	if (!initialized)
		return NULL;

#ifdef _WIN32
	return (Thread*) TlsGetValue (threadIndex);
#endif

#ifdef _PTHREADS
	return (Thread*) pthread_getspecific (threadIndex);
#endif
}

void Thread::setThreadBarn(Threads *newBarn)
{
	if (newBarn)
		newBarn->checkInactive (this);

	if (newBarn == threadBarn)
		return;

	if (threadBarn)
		{
		ASSERT(useCount > 1);
		threadBarn->leave (this);
		threadBarn->release();
		}

	if ( (threadBarn = newBarn) )
		{
		threadBarn->addRef();
		threadBarn->enter (this);
		}
}

void Thread::lockExitMutex(void)
{
	exitMutex.lock();
}

void Thread::unlockExitMutex(void)
{
	exitMutex.unlock();
}
