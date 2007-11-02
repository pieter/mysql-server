/* Copyright (C) 2007 MySQL AB

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

#include <memory.h>
#include "Engine.h"
#include "PriorityScheduler.h"
#include "Sync.h"
#include "Thread.h"

PriorityScheduler::PriorityScheduler(void)
{
	currentPriority = 0;
	count = 0;
	memset(waitingThreads, 0, sizeof(waitingThreads));
}

PriorityScheduler::~PriorityScheduler(void)
{
}

void PriorityScheduler::schedule(int priority)
{
	Sync sync(&mutex, "PriorityScheduler::schedule");
	sync.lock(Exclusive);
	
	if (priority == currentPriority)
		{
		++count;
		
		return;
		}
	
	if (priority > currentPriority)
		{
		currentPriority = priority;
		count = 1;
		
		return;
		}
	
	Thread *thread = Thread::getThread("PriorityScheduler::schedule");
	thread->que = waitingThreads[priority];
	waitingThreads[priority] = thread;
	thread->wakeupType = None;
	sync.unlock();

	while (thread->wakeupType == None)
		thread->sleep();
}

void PriorityScheduler::finished(int priority)
{
	Sync sync(&mutex, "PriorityScheduler::finished");
	sync.lock(Exclusive);
	
	// If this is below the current priority level, ignore it
	
	if (priority < currentPriority)
		return;
	
	// If there are other processes at this priority level, just decrement the count
	
	if (--count > 0)
		return;
	
	for (currentPriority = PRIORITY_MAX - 1; currentPriority > 0;  --currentPriority)
		if (waitingThreads[currentPriority])
			{
			count = 0;

			for (Thread *thread; (thread = waitingThreads[currentPriority]);)
				{
				++count;
				waitingThreads[currentPriority] = thread->que;
				thread->wakeupType = Exclusive;
				thread->wake();
				}
			
			break;
			}			
}
