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

// Threads.h: interface for the Threads class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_THREADS_H__84FD1987_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_THREADS_H__84FD1987_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SyncObject.h"
#include "Synchronize.h"
#include "ThreadQueue.h"

class Thread;

struct ThreadPending
{
	void			(*fn)(void*);
	void			*arg;
	const char*		description;
	ThreadPending	*next;
	ThreadPending	*prior;
};

class Threads : public Synchronize
{
public:
	Threads(Threads *parentThreads, int maximumThreads=0);
	virtual bool	shutdown (Thread *thread);
	void			checkInactive (Thread* thread);
	void			printThreads();
	void			waitForAll();
	void			clear();
	void			shutdownAll();
	void			exitting (Thread *thread);
	Thread*			start (const char *desc, void (fn)(void *), void *arg);
	Thread*			start (const char *desc, void (fn)(void *), void *arg, Threads *threadBarn);
	Thread*			startWhenever(const char *desc, void (fn)(void *), void *arg);
	Thread*			startWhenever(const char *desc, void (fn)(void *), void *arg, Threads *threadBarn);
	void			release();
	void			addRef();
	void			print();
	void			leave (Thread *thread);
	void			enter (Thread *thread);
	void			inactive (Thread *thread);

protected:
	virtual ~Threads();
	SyncObject		syncObject;
	ThreadQueue		activeThreads;
	ThreadQueue		inactiveThreads;
	Threads			*parent;
	volatile INTERLOCK_TYPE	useCount;
	ThreadPending	*firstPending;
	ThreadPending	*lastPending;
	int				maxThreads;
	int				threadsActive;

};

#endif // !defined(AFX_THREADS_H__84FD1987_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
