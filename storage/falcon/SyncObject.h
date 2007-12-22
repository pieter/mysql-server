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

// SyncObject.h: interface for the SyncObject class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SYNCOBJECT_H__59333A53_BC53_11D2_AB5E_0000C01D2301__INCLUDED_)
#define AFX_SYNCOBJECT_H__59333A53_BC53_11D2_AB5E_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SynchronizationObject.h"

#if !defined(INTERLOCK_TYPE) && defined(_WIN32)
#define INTERLOCK_TYPE	long
#elif !defined(INTERLOCK_TYPE)
#define INTERLOCK_TYPE	int
#endif

#ifdef POSIX_THREADS
#include <pthread.h>
#endif

#ifdef SOLARIS_MT
#include <sys/mutex.h>
#include <thread.h>
#endif

#include "Mutex.h"

#define TRACE_SYNC_OBJECTS

class SyncObject;
class Sync;
class Thread;
class LinkedList;
class Stream;
class InfoTable;

class SyncObject : public SynchronizationObject
{
public:
	SyncObject();
	virtual ~SyncObject();

	void		print();
	void		stalled(Thread *thread);
	void		printEvents(int level);
	void		postEvent (Thread *thread, const char *what, Thread *granting);
	void		downGrade (LockType type);
	bool		isLocked();
	void		sysServiceFailed(const char* server, int code);
	void		bumpWaiters(int delta);
	void		grantLocks(void);
	//void		assertionFailed(void);
	int			getState(void);
	void		validate(LockType lockType);
	void		unlock(void);
	bool		ourExclusiveLock(void);
	void		frequentStaller(Thread *thread, Sync *sync);
	void		setName(const char* name);

	virtual void	unlock (Sync *sync, LockType type);
	virtual void	lock (Sync *sync, LockType type, int timeout = 0);
	virtual void	findLocks (LinkedList &threads, LinkedList& syncObjects);

	static void		analyze(Stream* stream);
	static void		getSyncInfo(InfoTable* infoTable);
	static void		dump(void);

	inline Thread* getExclusiveThread()
		{ return exclusiveThread; };

protected:
	void wait(LockType type, Thread *thread, Sync *sync, int timeout);

	int32				monitorCount;
	Mutex				mutex;
	Thread				*volatile que;
	Thread				*volatile exclusiveThread;
	volatile INTERLOCK_TYPE		waiters;
	volatile INTERLOCK_TYPE		lockState;
	int					stalls;

#ifdef TRACE_SYNC_OBJECTS
	int					objectId;
	INTERLOCK_TYPE		sharedCount;
	int					exclusiveCount;
	int					waitCount;
	int					queueLength;
	const char*			where;
	const char*			name;
#endif
};

#endif // !defined(AFX_SYNCOBJECT_H__59333A53_BC53_11D2_AB5E_0000C01D2301__INCLUDED_)
