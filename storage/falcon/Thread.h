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

// Thread.h: interface for the Thread class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_THREAD_H__84FD1988_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_THREAD_H__84FD1988_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifdef _WIN32
#define THREAD_ID		unsigned long
#define THREAD_RET		unsigned long
#else
#define _stdcall
#endif

#ifdef _PTHREADS
#include <pthread.h>
#define THREAD_ID		pthread_t
#define THREAD_RET		void*
#endif

#include "Synchronize.h"
#include "SynchronizationObject.h"


class Threads;
class Sync;
class SyncObject;
class SyncWait;
class LinkedList;
class JavaThread;

struct TimeZone;

class Thread : public Synchronize
{
public:
	Thread(const char *desc);
	//Thread(const char *desc, Threads *threads, void (*fn)(void*), void *arg, Threads *barn);
	Thread(const char *desc, Threads *barn);
	virtual ~Thread();

	void			setTimeZone (const TimeZone *timeZone);
	const char*		getWhere();
	void			print (const char *label);
	void			print();
	void			findLocks (LinkedList &threads, LinkedList& syncObjects);
	void			clearLock (Sync *sync);
	void			setLock (Sync *sync);
	void			release();
	void			addRef();
	void			createThread (void (*fn)(void*), void *arg);
	//void			print(int level);
	void			grantLock(SyncObject *lock);
	void			init(const char *description);
	void			start (const char *desc, void (*fn)(void*), void*arg);
	void			thread();
	void			setThreadBarn (Threads *newBarn);

	static THREAD_RET _stdcall thread (void* parameter);
	static void		deleteThreadObject();
	static void		validateLocks();
	static Thread*	getThread(const char *desc);
	static Thread*	findThread();

	static void		lockExitMutex(void);
	static void		unlockExitMutex(void);

	void			*argument;
	void			(* volatile function)(void*);
	void*			threadHandle;

	THREAD_ID		threadId;
	Threads			*threadBarn;
	Thread			*next;				// next thread in "thread barn"
	Thread			*prior;				// next thread in "thread barn"
	Thread			*que;				// next thread in wait que (see SyncObject)
	Thread			*srlQueue;			// serial log queue
	LockType		lockType;			// requested lock type (see SyncObject)
	LockType		wakeupType;			// used by SerialLog::flush
	
	volatile bool	lockGranted;
	volatile bool	licenseWakeup;
	volatile int32	activeLocks;
	Sync			*locks;
	Sync			*lockPending;
	SyncWait		*syncWait;
	bool			marked;
	int				pageMarks;
	int				eventNumber;		// for debugging
	const char		*description;
	const char		*where;
	const TimeZone	*defaultTimeZone;
	JavaThread		*javaThread;
	uint64			commitBlockNumber;

protected:
	static void setThread (Thread *thread);

	volatile INTERLOCK_TYPE		useCount;
};

#endif // !defined(AFX_THREAD_H__84FD1988_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
