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

// DateTime.h: interface for the DateTime class.
//
//////////////////////////////////////////////////////////////////////

/*
 * copyright (c) 1999 - 2000 by James A. Starkey
 */


#if !defined(AFX_DATETIME_H__84FD1970_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_DATETIME_H__84FD1970_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

//#include "Engine.h"	// Added by ClassView
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

struct tm;
struct ZoneRule;
struct TimeZone;

class DateTime  
{
public:
	static bool checkDayLightSavings (tm *time, const TimeZone *timeZone);
	static void checkConversion (time_t t, tm *tm, const TimeZone *timeZone);
	static void getToday (tm *time);
	static int timeRule (tm *time, int64 first, const ZoneRule *rule);
	static bool isDayLightSavings(tm *time, const TimeZone *timeZone);
	static const TimeZone* getDefaultTimeZone();
	static const TimeZone* findTimeZone (const char *string);
	void add (int64 seconds);
	static int getDelta (const char **ptr);
	static void getNow (tm *time);
	static const char* getTimeZone();
	int compare (DateTime when);
	bool isNull();
	void setNull();
	bool equals (DateTime when);
	bool after (DateTime when);
	bool before (DateTime when);
	void setNow();
	void setMilliseconds (int64 milliseconds);
	void setSeconds (int64 seconds);
	int64 getMilliseconds();
	int64 getSeconds();
	void getLocalTime (tm *time);
	static bool isDayLightSavings (tm *time);
	static DateTime relativeDay (int deltaDay);
	double getDouble();
	static time_t getNow();
	int getString (int length, char *buffer);
	static DateTime conversionError();
	static bool match (const char *str1, const char *str2);
	static int lookup (const char *string, const char **table);
	static DateTime convert (const char *string, int length);
	static DateTime convert (const char *string);
	static void getYMD (int64 date, tm *time);

protected:
	static void getLocalTime (int64 milliseconds, tm *time);
	static int64 getSeconds (tm *time);
	static int64 getSeconds (tm *time, const TimeZone *timeZone);
	static int64 getDate (short year, short month, short day);

	int64	date;			// Milliseconds since Jan 1, 1970
};

class Time : public DateTime  
{
public:
	int getString(int length, char * buffer);
	//void setTime (int32 time);
	static Time convert (const char *string, int length);
};

#endif // !defined(AFX_DATETIME_H__84FD1970_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
