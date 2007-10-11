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

// Timestamp.h: interface for the Timestamp class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey


#if !defined(AFX_TIMESTAMP_H__35227BA2_2C14_11D4_98E0_0000C01D2301__INCLUDED_)
#define AFX_TIMESTAMP_H__35227BA2_2C14_11D4_98E0_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "DateTime.h"

class TimeStamp : public DateTime
{
public:
	//DateTime getDate();
	int compare (TimeStamp when);
	void setNanos(int nanoseconds);
	int getNanos();
	int getString(int length, char * buffer);
	TimeStamp& operator = (int32 value);
	TimeStamp& operator = (DateTime value);

protected:
	int32	nanos;					// nano seconds
};

#endif // !defined(AFX_TIMESTAMP_H__35227BA2_2C14_11D4_98E0_0000C01D2301__INCLUDED_)
