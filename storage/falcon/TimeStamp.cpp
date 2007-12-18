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

// Timestamp.cpp: implementation of the Timestamp class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey


#include <stdio.h>
#include <time.h>
#include <string.h>
#include "Engine.h"
#include "TimeStamp.h"
#include "SQLError.h"

#ifdef _WIN32
#define snprintf	_snprintf
#endif

#define MS_PER_DAY		(24 * 60 * 60 * 1000)

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


TimeStamp& TimeStamp::operator =(DateTime value)
{
	date = value.getMilliseconds();
	nanos = 0;

	return *this;
}

TimeStamp& TimeStamp::operator =(int32 value)
{
	date = value;
	nanos = 0;

	return *this;
}

int TimeStamp::getString(int length, char * buffer)
{
	//return DateTime::getString ("%Y-%m-%d %H:%M", length, buffer);
	tm time;
	getLocalTime (&time);
	snprintf (buffer, length, "%d-%.2d-%.2d %.2d:%.2d:%.2d",
			  time.tm_year + 1900, 
			  time.tm_mon + 1, 
			  time.tm_mday,
			  time.tm_hour, 
			  time.tm_min,
			  time.tm_sec);

	return (int) strlen (buffer);
}

int TimeStamp::getNanos()
{
	return nanos;
}

void TimeStamp::setNanos(int nanoseconds)
{
	/***
	if (nanoseconds > 999999999 || nanoseconds < 0)
		throw SQLError (RUNTIME_ERROR, "illegal value for nanoseconds in Timestamp");
	***/

	nanos = nanoseconds;
}

int TimeStamp::compare(TimeStamp when)
{
	if (date > when.date)
		return 1;
	else if (date < when.date)
		return -1;

	return nanos - when.nanos;
}

/***
DateTime TimeStamp::getDate()
{
	tm time;
	getLocalTime (&time);
	time.tm_sec = 0;
	time.tm_min = 0;
	time.tm_hour = 0;
	time.tm_isdst = -1;
	DateTime date;
	date.setSeconds (getSeconds (&time));

	return date;
}
***/

