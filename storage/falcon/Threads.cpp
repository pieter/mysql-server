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

// Threads.cpp: implementation of the Threads class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "Threads.h"
#include "Thread.h"
#include "Sync.h"
#include "Interlock.h"
#include "Log.h"

#ifndef ASSERT
#define ASSERT(arg)
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Threads::Threads(Threads *parentThreads, int maximumThreads)
{
	useCount = 1;
	parent = parentThreads;
	maxThreads = maximumThreads;
	threadsActive = 0;
	firstPending = NULL;
	lastPending = NULL;
}

Threads::~Threads()
{
	for (Thread *thread = activeThreads.first; thread; thread = thread->next)
		thread->setThreadBarn (NULL);
}

Thread* Threads::start(const char *desc, void (fn)(void*), void * arg)
{
	return start (desc, fn, arg, this);
}

Thread* Threads::start(const char *desc, void (fn)(void*), void * arg, Threads *threadBarn)
{
	if (parent)
		return parent->start (desc, fn, arg, threadBarn);

	Sync sync (&syncObject, "Threads::start");
	sync.lock (Exclusive);

	Thread *thread = inactiveThreads.first;

	if (thread)
		{
		if (thread->threadBarn)
			{
			printThreads();
			ASSERT (!thread->threadBarn);
			}
			
		thread->addRef();
		inactiveThreads.remove (thread);
		sync.unlock();
		thread->setThreadBarn (threadBarn);
		thread->release();
		++threadBarn->threadsActive;
		
		try
			{
			thread->start (desc, fn, arg);
			}
		catch (...)
			{
			throw;
			}

		release();
		
		return thread;
		}

	
	sync.unlock();
	++threadBarn->threadsActive;
	//thread = new Thread (desc, this, fn, arg, threadBarn);
	thread = new Thread (desc, threadBarn);
	thread->createThread(fn, arg);
	
	return thread;		
}

void Threads::exitting(Thread * thread)
{
	Sync sync (&syncObject, "Threads::exiting");
	sync.lock (Exclusive);
	bool rel = false;

	if (activeThreads.isMember (thread))
		{
		activeThreads.remove (thread);
		rel = true;
		}
	else if (inactiveThreads.isMember (thread))
		{
		inactiveThreads.remove (thread);
		rel = true;
		}

	sync.unlock();

	if (rel)
		release();

	wake();
}

void Threads::shutdownAll()
{
	//Thread *thisThread = 
	Thread::getThread("Threads::shutdownAll");
	Sync sync (&syncObject, "Threads::shutdownAll");
	sync.lock (Exclusive);
	Thread *thread;

	for (thread = activeThreads.first; thread; thread = thread->next)
		thread->shutdown();

	while ( (thread = inactiveThreads.first) )
		{
		inactiveThreads.remove (thread);
		sync.unlock();
		thread->setThreadBarn (this);
		thread->shutdown();
		sync.lock (Exclusive);
		}

	sync.unlock();
}

void Threads::clear()
{
	for (Thread *thread = activeThreads.first; thread; thread = thread->next)
		thread->marked = false;
}

void Threads::waitForAll()
{
	Thread *thisThread = Thread::getThread("Threads::waitForAll");
	Sync sync (&syncObject, "Threads::waitForAll");

	for (;;)
		{
		sync.lock (Exclusive);
		bool done = true;
		
		for (Thread *thread = activeThreads.first; thread; thread = thread->next)
			if (thread->threadId != thisThread->threadId)
				done = false;
				
		if (done)
			break;
			
		sync.unlock();
		sleep(1000);
		}
}

void Threads::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void Threads::release()
{
	if (INTERLOCKED_DECREMENT (useCount) == 0)
		delete this;
}

void Threads::inactive(Thread *thread)
{
	if (parent)
		{
		parent->inactive (thread);
		return;
		}

	thread->setThreadBarn (NULL);
	Sync sync (&syncObject, "Threads::inactive");
	sync.lock (Exclusive);

	if (firstPending)
		{
		ThreadPending *pending = firstPending;
		
		if ( (firstPending = pending->next) )
			firstPending->prior = NULL;
		else
			lastPending = NULL;

		thread->setThreadBarn (this);
		thread->start(pending->description, pending->fn, pending->arg);
		delete pending;
		
		return;
		}

	--threadsActive;
	inactiveThreads.insert (thread);
	addRef();
}

void Threads::enter(Thread *thread)
{
	Sync sync (&syncObject, "Threads::enter");
	sync.lock (Exclusive);
	addRef();
	activeThreads.insert (thread);
}

void Threads::leave(Thread *thread)
{
	Sync sync (&syncObject, "Threads::leave");
	sync.lock (Exclusive);
	activeThreads.remove (thread);
	release();
}

void Threads::print()
{
	Sync sync (&syncObject, "Threads::print");
	sync.lock (Shared);
	printThreads();
}

void Threads::printThreads()
{
	LOG_DEBUG ("Threads %x, parent %x\n", this, parent);
	
	if (activeThreads.first)
		{
		LOG_DEBUG (" Active threads:\n");
		int n = 0;
		for (Thread *thread = activeThreads.first; thread && n < 100; thread = thread->next, ++n)
			thread->print();
		}

	if (inactiveThreads.first)
		{
		LOG_DEBUG (" Inactive threads:\n");
		int n = 0;
		for (Thread *thread = inactiveThreads.first; thread && n < 100; thread = thread->next, ++n)
			thread->print();
		}

	if (parent)
		parent->print();
}

void Threads::checkInactive(Thread *thread)
{
	if (parent)
		parent->checkInactive (thread);
	else
		ASSERT (!inactiveThreads.isMember (thread));
}

bool Threads::shutdown(Thread *thread)
{
	Sync sync (&syncObject, "Threads::shutdown");
	sync.lock (Exclusive);

	if (inactiveThreads.isMember(thread))
		{
		inactiveThreads.remove(thread);
		release();
		}
	else if (!activeThreads.isMember(thread) && parent)
		return parent->shutdown(thread);

	thread->shutdown();

	return true;
}

Thread* Threads::startWhenever(const char *desc, void (fn)(void *), void *arg)
{
	return startWhenever(desc, fn, arg, this);
}

Thread* Threads::startWhenever(const char *desc, void (fn)(void *), void *arg, Threads *threadBarn)
{
	if (parent)
		return parent->startWhenever (desc, fn, arg, threadBarn);

	if (maxThreads == 0 || (threadsActive < maxThreads && !firstPending))
		return start(desc, fn, arg, threadBarn);

	Sync sync (&syncObject, "Threads::startWhenever");
	sync.lock (Exclusive);

	if (maxThreads == 0 || (threadsActive < maxThreads && !firstPending))
		return start(desc, fn, arg, threadBarn);

	ThreadPending *pending = new ThreadPending;
	pending->fn = fn;
	pending->arg = arg;
	pending->description = desc;
	pending->next = NULL;

	if ( (pending->prior = lastPending) )
		lastPending->next = pending;
	else
		firstPending = pending;

	lastPending = pending;

	return NULL;
}

