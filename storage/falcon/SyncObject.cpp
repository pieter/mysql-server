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

// SyncObject.cpp: implementation of the SyncObject class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <memory.h>

#ifdef _WIN32
#include <windows.h>
#undef ERROR
#undef ASSERT
#undef TRACE
#endif

#ifdef ENGINE
#define TRACE
#else
#define ASSERT(b)
#endif

#include "Engine.h"
#include "SyncObject.h"
#include "Thread.h"
#include "Threads.h"
#include "Sync.h"
#include "Interlock.h"
#include "LinkedList.h"
#include "Log.h"
#include "LogLock.h"
#include "SQLError.h"
#include "Stream.h"
#include "InfoTable.h"

//#define STALL_THRESHOLD	1000

#define BACKOFF	\
		if (false)\
			thread->sleep(1);\
		else\
			thread = Thread::getThread("SyncObject::lock")

#ifdef TRACE_SYNC_OBJECTS

#define BUMP(counter)							++counter
#define BUMP_INTERLOCKED(counter)				INTERLOCKED_INCREMENT(counter)

static const int				MAX_SYNC_OBJECTS = 300000;
static volatile INTERLOCK_TYPE	nextSyncObjectId;
static SyncObject				*syncObjects[MAX_SYNC_OBJECTS];

#else
#define BUMP(counter)
#define BUMP_INTERLOCKED(counter)
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#ifdef EMULATE

static int cas_emulation (volatile int *state, int compare, int exchange);

#undef COMPARE_EXCHANGE
#define COMPARE_EXCHANGE(target,compare,exchange) \
	(cas_emulation(target,compare,exchange) == compare)


int cas_emulation (volatile int *state, int compare, int exchange)
{
	int result = *state;

	if (result == compare)
		*state = exchange;
	
	return result;
}
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SyncObject::SyncObject()
{
	waiters = 0;
	lockState = 0;
	que = NULL;
	monitorCount = 0;
	stalls = 0;
	exclusiveThread = NULL;

#ifdef TRACE_SYNC_OBJECTS
	sharedCount = 0;
	exclusiveCount = 0;
	waitCount = 0;
	queueLength = 0;
	where = NULL;
	name = NULL;
	objectId = INTERLOCKED_INCREMENT(nextSyncObjectId);
	
	if (objectId < MAX_SYNC_OBJECTS)
		syncObjects[objectId] = this;
#endif
}

SyncObject::~SyncObject()
{
	ASSERT(lockState == 0);
	
#ifdef TRACE_SYNC_OBJECTS
	if (objectId < MAX_SYNC_OBJECTS)
		syncObjects[objectId] = NULL;
#endif
}

void SyncObject::lock(Sync *sync, LockType type, int timeout)
{
	Thread *thread;

#ifdef TRACE_SYNC_OBJECTS
	if (sync)
		where = sync->where;
#endif
	
	if (type == Shared)
		{
		thread = NULL;
		BUMP_INTERLOCKED(sharedCount);

		for(;;)
			{
			INTERLOCK_TYPE oldState = lockState;

			if (oldState < 0)
				break;

			INTERLOCK_TYPE newState = oldState + 1;

			if (COMPARE_EXCHANGE(&lockState, oldState, newState))
				{
				DEBUG_FREEZE;
				return;
				}
				
			BACKOFF;
			}

		mutex.lock();
		bumpWaiters(1);

		for(;;)
			{
			INTERLOCK_TYPE oldState = lockState;

			if (oldState < 0)
				break;
				
			INTERLOCK_TYPE newState = oldState + 1;

			if (COMPARE_EXCHANGE(&lockState, oldState, newState))
				{
				bumpWaiters(-1);
				mutex.release();
				DEBUG_FREEZE;
				
				return;
				}

			BACKOFF;
			}

		thread = Thread::getThread("SyncObject::lock");

		if (thread == exclusiveThread)
			{
			++monitorCount;
			bumpWaiters(-1);
			mutex.release();
			DEBUG_FREEZE;
			
			return;
			}
		}
	else
		{
		thread = Thread::getThread("SyncObject::lock");
		ASSERT(thread);

		if (thread == exclusiveThread)
			{
			++monitorCount;
			BUMP(exclusiveCount);
			DEBUG_FREEZE;
			
			return;
			}

		while (waiters == 0)
			{
			INTERLOCK_TYPE oldState = lockState;
			
			if (oldState != 0)
				break;
				
			if (COMPARE_EXCHANGE(&lockState, oldState, -1))
				{
				exclusiveThread = thread;
				BUMP(exclusiveCount);
				DEBUG_FREEZE;
				
				return; 
				}
			
			BACKOFF;
			}
			
		mutex.lock();
		bumpWaiters(1);
		BUMP(exclusiveCount);
		
		while (que == NULL)
			{
			INTERLOCK_TYPE oldState = lockState;
			
			if (oldState != 0)
				break;
				
			if (COMPARE_EXCHANGE(&lockState, oldState, -1))
				{
				exclusiveThread = thread;
				bumpWaiters(-1);
				mutex.release();
				DEBUG_FREEZE;
				
				return;
				}
			
			BACKOFF;
			}
		}

	wait(type, thread, sync, timeout);
	DEBUG_FREEZE;
}

void SyncObject::unlock(Sync *sync, LockType type)
{
	//ASSERT(lockState != 0);
	
	if (monitorCount)
		{
		//ASSERT (monitorCount > 0);
		--monitorCount;
		DEBUG_FREEZE;

		return;
		}
	
	Thread *thread = NULL;
	
	for (;;)
		{
		//ASSERT ((type == Shared && lockState > 0) || (type == Exclusive && lockState == -1));
		long oldState = lockState;
		long newState = (type == Shared) ? oldState - 1 : 0;
		exclusiveThread = NULL;
		
		if (COMPARE_EXCHANGE(&lockState, oldState, newState))
			{
			DEBUG_FREEZE;

			if (waiters)
				grantLocks();
				
			return;
			}
			
		BACKOFF;
		}

	DEBUG_FREEZE;
}

void SyncObject::downGrade(LockType type)
{
	ASSERT (monitorCount == 0);
	ASSERT (type == Shared);
	ASSERT (lockState == -1);
	
	for (;;)
		if (COMPARE_EXCHANGE(&lockState, -1, 1))
			{
			exclusiveThread = NULL;
			DEBUG_FREEZE;
			
			if (waiters)
				grantLocks();

			return;
			}
}

void SyncObject::wait(LockType type, Thread *thread, Sync *sync, int timeout)
{
	++stalls;
	BUMP(waitCount);

#ifdef STALL_THRESHOLD
	if ((stalls % STALL_THRESHOLD) == STALL_THRESHOLD - 1)
		frequentStaller(thread, sync);
#endif

	Thread *volatile *ptr;
	
	for (ptr = &que; *ptr; ptr = &(*ptr)->que)
		{
		BUMP(queueLength);
		
		if (*ptr == thread)
			{
			LOG_DEBUG ("Apparent single thread deadlock for thread %d (%x)\n", thread->threadId, thread);
			
			for (Thread *thread = que; thread; thread = thread->que)
				thread->print();
				
			mutex.release();
			throw SQLEXCEPTION (BUG_CHECK, "Single thread deadlock");
			}
		}

	thread->que = NULL;
	thread->lockType = type;
	*ptr = thread;
	thread->lockGranted = false;
	thread->lockPending = sync;
	++thread->activeLocks;
	mutex.release();

	if (timeout)
		for (;;)
			{
			thread->sleep (timeout);
			
			if (thread->lockGranted)
				return;
			
			mutex.lock();
			
			if (thread->lockGranted)
				{
				mutex.unlock();
				
				return;
				}
			
			for (ptr = &que; *ptr; ptr = &(*ptr)->que)
				if (*ptr == thread)
					{
					*ptr = thread->que;
					--waiters;
					break;
					}
			
			mutex.unlock();
			
			throw SQLError(LOCK_TIMEOUT, "lock timed out after %d milliseconds\n", timeout);
			}
		
		
	while (!thread->lockGranted)
		{
		bool wakeup = thread->sleep (10000);
		
		if (thread->lockGranted)
			{
			if (!wakeup)
				Log::debug("Apparent lost thread wakeup\n");

			break;
			}
			
		if (!wakeup)
			{
			stalled (thread);
			break;
			}
		}

	while (!thread->lockGranted)
		thread->sleep();
}

bool SyncObject::isLocked()
{
	return lockState != 0;
}


void SyncObject::stalled(Thread *thread)
{
#ifdef TRACE
	mutex.lock();
	LogLock logLock;
	LinkedList threads;
	LinkedList syncObjects;
	thread->findLocks (threads, syncObjects);

	LOG_DEBUG ("Stalled threads\n");

	FOR_OBJECTS (Thread*, thrd, &threads)
		thrd->print();
	END_FOR;

	LOG_DEBUG ("Stalled synchronization objects:\n");

	FOR_OBJECTS (SyncObject*, syncObject, &syncObjects)
		syncObject->print();
	END_FOR;

	LOG_DEBUG ("------------------------------------\n");
	mutex.release();
#endif
}

void SyncObject::findLocks(LinkedList &threads, LinkedList &syncObjects)
{
#ifdef TRACE
	if (syncObjects.appendUnique(this))
		{
		if (exclusiveThread)
			exclusiveThread->findLocks(threads, syncObjects);

		for (Thread *thread = que; thread; thread = thread->que)
			thread->findLocks(threads, syncObjects);
		}
#endif
}

void SyncObject::print()
{
#ifdef TRACE
	LOG_DEBUG ("  SyncObject %lx: state %d, monitor %d, waiters %d\n", 
				this, lockState, monitorCount, waiters);

	if (exclusiveThread)
		exclusiveThread->print ("    Exclusive thread");

	for (Thread *volatile thread = que; thread; thread = thread->que)
		thread->print ("    Waiting thread");
#endif
}


void SyncObject::sysServiceFailed(const char* service, int code)
{
	throw SQLEXCEPTION (BUG_CHECK, "Single thread deadlock");
}

void SyncObject::bumpWaiters(int delta)
{
	if (delta == 1)
		INTERLOCKED_INCREMENT(waiters);
	else if (delta == -1)
		INTERLOCKED_DECREMENT(waiters);
	else
		for (;;)
			{
			INTERLOCK_TYPE oldValue = waiters;
			INTERLOCK_TYPE newValue = waiters + delta;
			
			if (COMPARE_EXCHANGE(&waiters, oldValue, newValue))
				return;
			}
}

void SyncObject::grantLocks(void)
{
	mutex.lock();
	ASSERT ((waiters && que) || (!waiters && !que));
	const char *description = NULL;
	Thread *thread = NULL;
	
	for (Thread *waiter = que, *prior = NULL, *next; waiter; waiter = next)
		{
		description = waiter->description;
		bool granted = false;
		next = waiter->que;
				
		if (waiter->lockType == Shared)
			for (int oldState; (oldState = lockState) >= 0;)
				{
				long newState = oldState + 1;
				
				if (COMPARE_EXCHANGE(&lockState, oldState, newState))
					{
					granted = true;
					exclusiveThread = NULL;
					break;
					}
				
				BACKOFF;
				}
		else
			{
			ASSERT(waiter->lockType == Exclusive);
			
			while (lockState == 0)
				{
				if (COMPARE_EXCHANGE(&lockState, 0, -1))
					{
					granted = true;
					exclusiveThread = waiter;
					break;
					}
				
				BACKOFF;
				}
			}
		
		if (granted)
			{
			if (prior)
				prior->que = next;
			else
				que = next;
			
			bool shutdownInProgress = waiter->shutdownInProgress;
			
			if (shutdownInProgress)
				Thread::lockExitMutex();
					
			bumpWaiters(-1);
			--waiter->activeLocks;
			waiter->grantLock (this);
			
			if (shutdownInProgress)
				Thread::unlockExitMutex();
			}
		else
			prior = waiter;
		}
			
	mutex.release();
}

int SyncObject::getState(void)
{
	return lockState;
}

void SyncObject::validate(LockType lockType)
{
	switch (lockType)
		{
		case None:
			ASSERT (lockState == 0);
			break;
		
		case Shared:
			ASSERT (lockState > 0);
			break;
		
		case Exclusive:
			ASSERT (lockState == -1);
			break;
		
		case Invalid:
			break;
		}
}

void SyncObject::unlock(void)
{
	if (lockState > 0)
		unlock (NULL, Shared);
	else if (lockState == -1)
		unlock (NULL, Exclusive);
	else
		ASSERT(false);
}

bool SyncObject::ourExclusiveLock(void)
{
	if (lockState != -1)
		return false;

	return exclusiveThread == Thread::getThread("SyncObject::ourExclusiveLock");
}

void SyncObject::frequentStaller(Thread *thread, Sync *sync)
{
	Thread *threadQue = thread->que;
	LockType lockType = thread->lockType;
	bool lockGranted = thread->lockGranted;
	Sync *lockPending = thread->lockPending;

	if (sync)
		LOG_DEBUG("Frequent stall from %s\n", sync->where);
	else
		LOG_DEBUG("Frequent stall from unknown\n");
		
	thread->que = threadQue;
	thread->lockType = lockType;
	thread->lockGranted = lockGranted;
	thread->lockPending = lockPending;
}
void SyncObject::analyze(Stream* stream)
{
#ifdef TRACE_SYNC_OBJECTS
	SyncObject *syncObject;
	stream->format("Where\tShares\tExclusives\tWaits\tAverage Queue\n");
	
	for (int n = 1; n < MAX_SYNC_OBJECTS; ++n)
		if ( (syncObject = syncObjects[n]) && syncObject->where)
			stream->format("%s\t%d\t%d\t%d\t%d\t\n",
					syncObject->where,
					syncObject->sharedCount,
					syncObject->exclusiveCount,
					syncObject->waitCount,
					(syncObject->waitCount) ? syncObject->queueLength / syncObject->waitCount : 0);
#endif
}

void SyncObject::dump(void)
{
#ifdef TRACE_SYNC_OBJECTS
	FILE *out = fopen("SyncObject.dat", "w");
	
	if (!out)
		return;
		
	fprintf(out, "Where\tShares\tExclusives\tWaits\tAverage Queue\n");
	SyncObject *syncObject;
	
	for (int n = 1; n < MAX_SYNC_OBJECTS; ++n)
		if ( (syncObject = syncObjects[n]) )
			{
			const char *name = (syncObject->name) ? syncObject->name : syncObject->where;
			
			if (name)
				fprintf(out, "%s\t%d\t%d\t%d\t%d\t\n",
						name,
						syncObject->sharedCount,
						syncObject->exclusiveCount,
						syncObject->waitCount,
						(syncObject->waitCount) ? syncObject->queueLength / syncObject->waitCount : 0);

			syncObject->sharedCount = 0;
			syncObject->exclusiveCount = 0;
			syncObject->waitCount = 0;
			syncObject->queueLength = 0;
			}
	
	fclose(out);
#endif
}

void SyncObject::getSyncInfo(InfoTable* infoTable)
{
	SyncObject *syncObject;
	
	for (int index = 1; index < MAX_SYNC_OBJECTS; ++index)
		if ( (syncObject = syncObjects[index]) && syncObject->where)
			{
			int n = 0;
			infoTable->putString(n++, syncObject->where);
			infoTable->putInt(n++, syncObject->sharedCount);
			infoTable->putInt(n++, syncObject->exclusiveCount);
			infoTable->putInt(n++, syncObject->waitCount);
			int queueLength = (syncObject->waitCount) ? syncObject->queueLength / syncObject->waitCount : 0;
			infoTable->putInt(n++, queueLength);
			infoTable->putRecord();
			}
}

void SyncObject::setName(const char* string)
{
#ifdef TRACE_SYNC_OBJECTS
	name = string;
#endif
}
