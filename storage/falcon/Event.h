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

// Event.h: interface for the Event class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_EVENT_H__0CBDFCD1_2A29_4ACE_B0E7_EF2397F57F7B__INCLUDED_)
#define AFX_EVENT_H__0CBDFCD1_2A29_4ACE_B0E7_EF2397F57F7B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

static const int MAX_EVENT_COUNT	= 100000000;

#include "Mutex.h"
#include "Interlock.h"

class Thread;

class Event  
{
public:
	void removeWaiter(Thread *thread);
	bool isComplete (int count);
	void post();
	bool wait (int count);
	Event();
	virtual ~Event();

	inline int getNext()
		{
		volatile int count = eventCount;
		return (count == MAX_EVENT_COUNT) ? 0 : count + 1;
		}

	Mutex					mutex;
	volatile INTERLOCK_TYPE	eventCount;
	volatile Thread			*waiters;
};

#endif // !defined(AFX_EVENT_H__0CBDFCD1_2A29_4ACE_B0E7_EF2397F57F7B__INCLUDED_)
