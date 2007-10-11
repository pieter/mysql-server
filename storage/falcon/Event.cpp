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

// Event.cpp: implementation of the Event class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Event.h"
#include "Sync.h"
#include "Thread.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Event::Event()
{
	eventCount = 0;
	waiters = NULL;
}

Event::~Event()
{

}

bool Event::wait(int count)
{
	if (isComplete(count))
		return true;

	Thread *thread = Thread::getThread("Event::wait");
	Sync sync(&mutex, "Event::wait");
	sync.lock(Exclusive);
	thread->que = (Thread*) waiters;
	waiters = thread;
	sync.unlock();

	for (;;)
		{
		if (thread->shutdownInProgress || isComplete(count))
			{
			removeWaiter(thread);
			return !thread->shutdownInProgress;
			}

		thread->sleep();
		}

}

void Event::post()
{
	INTERLOCKED_INCREMENT(eventCount);

	if (waiters)
		{
		Sync sync(&mutex, "Event::post");
		sync.lock(Exclusive);
		for (Thread *waiter = (Thread*) waiters; waiter; waiter = waiter->que)
			waiter->wake();
		}
}

bool Event::isComplete(int count)
{
	if (eventCount >= count)
		return true;

	if ((count - eventCount) > MAX_EVENT_COUNT / 2)
		return true;

	return false;
}

void Event::removeWaiter(Thread *thread)
{
	if (!waiters)
		return;
	
	Sync sync(&mutex, "Event::removeWaiter");
	sync.lock(Exclusive);

	for (Thread **ptr = (Thread**) &waiters; *ptr; ptr = &(*ptr)->que)
		if (*ptr == thread)
			{
			*ptr = thread->que;
			return;
			}
}
