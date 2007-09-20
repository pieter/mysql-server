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

// Schedule.cpp: implementation of the Schedule class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <time.h>
#include <stdio.h>
#include "Engine.h"
#include "Schedule.h"
#include "Scheduler.h"
#include "ScheduleElement.h"
#include "SQLError.h"
#include "Interlock.h"

#define SECOND		0
#define MINUTE		1
#define HOUR		2
#define DAY			3
#define MONTH		4
#define WEEKDAY		5

#define YEAR		5

static const int maxValues [SCHEDULE_ELEMENTS+1] = { 60, 60, 24, 32, 12, 2100 };

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Schedule::Schedule(const char *scheduleString)
{
	string = scheduleString;
	memset(elements, 0, sizeof (elements));
	const char *p = string;
	deleted = false;
	useCount = 1;

	for (int n = 0; *p && n < SCHEDULE_ELEMENTS; ++n)
		{
		while (*p == ' ' || *p == '\t' || *p == '"')
			++p;
		
		if (!*p)
			break;

		if (*p == '*')
			++p;
		else if (ISDIGIT (*p))
			{
			ASSERT (n >= 0 && n < SCHEDULE_ELEMENTS);
			elements [n] = new ScheduleElement (&p);
			int max = maxValues [n];
			
			if (n == WEEKDAY)
				max = 7;
				
			for (ScheduleElement *element = elements [n]; element; element = element->next)
				if (element->from >= max || element->to >= max)
					throw SQLEXCEPTION (RUNTIME_ERROR, "invalid schedule string \"%s\"", (const char*) string);
			}
		else
			throw SQLEXCEPTION (RUNTIME_ERROR, "invalid schedule string \"%s\"", (const char*) string);
		}

	getNextEvent();
}

Schedule::~Schedule()
{
	for (int n = 0; n < SCHEDULE_ELEMENTS; ++n)
		if (elements [n])
			delete elements [n];
}

void Schedule::execute(Scheduler *scheduler)
{
	getNextEvent();
	scheduler->addEvent (this);
}

void Schedule::getNextEvent()
{
	time_t currentTime = time (NULL);
	struct tm *local = localtime (&currentTime);
	struct tm mk;
	mk = *local;
	int values [SCHEDULE_ELEMENTS];
	values [YEAR] = local->tm_year + 1900;
	values [MONTH] = local->tm_mon;
	values [DAY] = local->tm_mday;
	values [HOUR] = local->tm_hour;
	values [MINUTE] = local->tm_min;
	values [SECOND] = local->tm_sec;
	int bump = -1;

	for (int cycles = 0;; ++cycles)
		{

		// Bump any values that need bumping

		if (bump >= 0)
			{
			ASSERT (bump >= 0 && bump < SCHEDULE_ELEMENTS);
			
			if (++values [bump] >= maxValues [bump])
				{
				bump = bump + 1;
				continue;
				}
				
			for (int n = 0; n < bump; ++n)
				values [n] = 0;
				
			bump = -1;
			}

		// Make sure all values are in valid range, otherwise bump

		int n;

		for (n = 0; n < YEAR; ++n)
			if (values [n] >= maxValues [n])
				{
				bump = n + 1;
				break;
				}

		if (bump >= 0)
			continue;

		// Apply any constraints (except weekdays)

		for (n = MONTH; n >= SECOND; --n)
			if (elements [n])
				{
				int next = elements [n]->getNext (values [n]);
				
				if (next < 0 || next >= maxValues [n])
					{
					bump = n + 1;
					break;
					}
					
				if (next > values [n])
					{
					values [n] = next;
					
					for (int m = 0; m < n; ++m)
						values [m] = 0;
					}
				}

		if (bump >= 0)
			continue;

		// Compute the time of the next event

		mk.tm_year = values [YEAR] - 1900;
		mk.tm_mon = values [MONTH];
		mk.tm_mday = values [DAY];
		mk.tm_hour = values [HOUR];
		mk.tm_min = values [MINUTE];
		mk.tm_sec = values [SECOND];
		eventTime = mktime (&mk);

		// If this is a lousy date, bump the day and get on with life

		if (eventTime < 0)
			{
			bump = DAY;
			continue;
			}

		// See if we've drifted into the next month (damn calender)

		struct tm *lt = localtime (&eventTime);
		
		if (mk.tm_mon != lt->tm_mon)
			{
			bump = DAY;
			continue;
			}

		// If we're being picky about weekday, handle it here

		if (elements [WEEKDAY])
			{
			int weekday = lt->tm_wday;
			int next = elements [WEEKDAY]->getNext (weekday);
			
			if (next > weekday)
				{
				values [DAY] += next - weekday - 1;
				bump = DAY;
				continue;
				}
				
			if (next < 0)
				{
				values [DAY] += 7 - weekday - 1;
				bump = DAY;
				continue;
				}
			}

		time_t delta = eventTime - currentTime;
		//printf ("delta: %d\n", delta);

		if (delta > 0)
			return;

		for (n = 0; n < YEAR; ++n)
			if (elements [n])
				{
				bump = n;
				break;
				}
		}
}


void Schedule::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void Schedule::release()
{
	if (INTERLOCKED_DECREMENT (useCount) == 0)
		delete this;
}

int Schedule::validateSchedule(const char* scheduleString)
{
	try
		{
		Schedule schedule(scheduleString);
		
		return true;
		}
	catch (...)
		{
		return false;
		}
}
