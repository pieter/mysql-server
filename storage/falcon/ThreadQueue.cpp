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

// ThreadQueue.cpp: implementation of the ThreadQueue class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "ThreadQueue.h"
#include "Thread.h"

#ifndef ASSERT
#define ASSERT(arg)
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ThreadQueue::ThreadQueue()
{
	first = NULL;
	last = NULL;
}

ThreadQueue::~ThreadQueue()
{

}

void ThreadQueue::insert(Thread *thread)
{
	thread->addRef();

	if ( (thread->prior = last) )
		last->next = thread;
	else
		{
		ASSERT (!first);
		first = thread;
		}

	thread->next = NULL;
	last = thread;
}

void ThreadQueue::remove(Thread *thread)
{
	
	if (thread->prior)
		thread->prior->next = thread->next;
	else
		{
		ASSERT (first == thread);
		first = thread->next;
		}

	if (thread->next)
		thread->next->prior = thread->prior;
	else
		{
		ASSERT (last == thread);
		last = thread->prior;
		}

	thread->next = NULL;
	thread->prior = NULL;
	thread->release();
}

bool ThreadQueue::isMember(Thread *candidate)
{
	for (Thread *thread = first; thread; thread = thread->next)
		if (thread == candidate)
			return true;

	return false;
}
