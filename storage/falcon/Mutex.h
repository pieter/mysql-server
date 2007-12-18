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


// Mutex.h: interface for the Mutex class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MUTEX_H__F3F1D3A7_4083_11D4_98E8_0000C01D2301__INCLUDED_)
#define AFX_MUTEX_H__F3F1D3A7_4083_11D4_98E8_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SynchronizationObject.h"

#ifdef _WIN32
#include <windows.h>
#undef ERROR
#endif

#ifdef _PTHREADS
#include <pthread.h>
#endif

#ifdef SOLARIS_MT
#include <sys/mutex.h>
#include <thread.h>
#endif

class Mutex : public SynchronizationObject
{
public:
	void unlock();
	void release();
	void lock();
	Mutex();
	~Mutex();
	Sync	*holder;
	
#ifdef _WIN32
	//void*	mutex;
	CRITICAL_SECTION criticalSection;
#endif

#ifdef _PTHREADS
	pthread_mutex_t	mutex;
#endif

#ifdef SOLARIS_MT
	cond_t			condition;
	mutex_t			mutex;
#endif

	virtual void unlock(Sync* sync, LockType type);
	virtual void lock(Sync* sync, LockType type, int timeout);
	virtual void findLocks(LinkedList& threads, LinkedList& syncObjects);
};


#endif // !defined(AFX_MUTEX_H__F3F1D3A7_4083_11D4_98E8_0000C01D2301__INCLUDED_)
