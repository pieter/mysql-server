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

// Schedule.h: interface for the Schedule class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCHEDULE_H__9E13C6D2_1F3E_11D3_AB74_0000C01D2301__INCLUDED_)
#define AFX_SCHEDULE_H__9E13C6D2_1F3E_11D3_AB74_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "JString.h"

class ScheduleElement;
class Scheduler;

#define SCHEDULE_ELEMENTS	6

class Schedule  
{
public:
	void release();
	void addRef();
	virtual void getNextEvent();
	virtual void execute(Scheduler *scheduler);
	Schedule(const char *string);

protected:
	virtual ~Schedule();

public:
	Schedule		*next;
	Schedule		*prior;
	bool			deleted;
	time_t			eventTime;

private:
	volatile INTERLOCK_TYPE	useCount;
	JString			string;
	ScheduleElement	*elements [SCHEDULE_ELEMENTS];
public:
	static int validateSchedule(const char* scheduleString);
};

#endif // !defined(AFX_SCHEDULE_H__9E13C6D2_1F3E_11D3_AB74_0000C01D2301__INCLUDED_)
