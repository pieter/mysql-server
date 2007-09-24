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

// DateTime.cpp: implementation of the DateTime class.
//
//////////////////////////////////////////////////////////////////////

/*
 * copyright (c) 1999 - 2003 by James A. Starkey
 */


#include <time.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "Engine.h"
#include "DateTime.h"
#include "SQLError.h"
#include "Log.h"

#ifdef ENGINE
#include "Thread.h"
#endif

#define MILLISECONDS(seconds)	((int64) seconds * 1000)
#define TODAY					"today"
#define TOMORROW				"tomorrow"
#define YESTERDAY				"yesterday"
#define NOW						"now"
#define THIS_MONTH				"thismonth"
#define THIS_DAY				"thisday"
#define THIS_YEAR				"thisyear"
#define SECONDS_PER_DAY			(60 * 60 * 24)
#define	HASH_SIZE				101

#define BASE_DATE				719469			// for Jan 1, 1970
#define WEEKDAY					4

const char *months [] = {
    "January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December",
	0
	};

const char *weekDays [] = {
    "Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday",
	0
	};

const char *meridians [] = {
	"am",
	"pm",
	0
	};

struct ZoneRule {
	short		month;
	short		day;
	short		dayOfWeek;
	int			time;
	};

struct TimeZoneRule {
	int			year;
	ZoneRule	startRule;
	ZoneRule	endRule;
	};

struct TimeZone {
    const char	*abbr;
    const char	*name;
	int			offset;
	TimeZoneRule *timeZoneRule;
	int			dst;
	TimeZone	*collision;
	};


static TimeZoneRule noDST[]			= { 0,    0,0,0,0,0,0,0,0 };
static TimeZoneRule USDST[]			= { 2007, 2,8,-1,7200000,	10,1,-1,7200000,
									    0,    3,1,-1,7200000,	9,-1,1,7200000 };
									    
static TimeZoneRule ParisDST[]		= { 0,    2,-1,1,7200000,	9,-1,1,7200000 };
static TimeZoneRule LondonDST[]		= { 0,    2,-1,1,3600000,	9,-1,1,3600000 };
static TimeZoneRule WarsawDST[]		= { 0,    2,-1,1,3600000,	9,-1,1,7200000 };

static TimeZoneRule BrazilDST[]		= { 0,    9,1,-1,0,			1,11,-1,0 };
static TimeZoneRule AzoresDST[]		= { 0,    2,-1,1,0,			9,-1,1,0 };
static TimeZoneRule InstanbulDST[]	= { 0,    2,-1,1,10800000,	9,-1,1,10800000 };
static TimeZoneRule AustralianDST[]	= { 0,    9,-1,1,7200000,	2,-1,1,10800000 };
static TimeZoneRule CairoDST[]		= { 0,    3,-1,6,0,			8,-1,6,0 };
static TimeZoneRule AucklandDST[]	= { 0,    9,1,-1,7200000,	2,15,-1,10800000 };
static TimeZoneRule RussianDST[]	= { 0,    2,-1,1,7200000,	9,-1,1,10800000 };
static TimeZoneRule RarotongaDST[]	= { 0,    9,-1,1,0,			2,1,-1,0 };
static TimeZoneRule TehranDST[]		= { 0,    2,4,0,0,			8,4,0,0 };
static TimeZoneRule ChileDST[]		= { 0,    9,9,-1,0,			2,9,-1,0 };
static TimeZoneRule CubaDST[]		= { 0,    3,1,-1,0,			9,8,-1,3600000 };
static TimeZoneRule HaitiDST[]		= { 0,    3,1,-1,3600000,	9,-1,1,7200000 };
static TimeZoneRule GrandTurkDST[]	= { 0,    3,1,-1,0,			9,-1,1,0 };
static TimeZoneRule GodthabDST[]	= { 0,    2,-1,1,-7200000,	9,-1,1,-7200000 };
static TimeZoneRule TripoliDST[]	= { 0,    2,-1,5,7200000,	9,1,-5,10800000  };
static TimeZoneRule AmmanDST[]		= { 0,    3,1,-6,0,			8,15,-6,3600000 };
static TimeZoneRule RigaDST[]		= { 0,    2,-1,1,7200000,	8,-1,1,10800000 };
static TimeZoneRule BeruitDST[]		= { 0,    2,-1,1,0,			8,-1,1,0 };
static TimeZoneRule WindhoekDST[]	= { 0,    8,1,-1,7200000,	3,1,-1,7200000 };
static TimeZoneRule DamascusDST[]	= { 0,    3,1,0,0,			9,1,0,0 };
static TimeZoneRule JerusalamDST[]	= { 0,    2,15,-6,0,		8,15,-1,0 };
static TimeZoneRule BagdadDST[]		= { 0,    3,1,0,10800000,	9,1,0,14400000 };
static TimeZoneRule BakuDST[]		= { 0,    2,-1,1,18000000,	9,-1,1,18000000 };
static TimeZoneRule BishkekDST[]	= { 0,    3,7,-1,0,			8,-1,1,0 };
static TimeZoneRule HobartDST[]		= { 0,    9,1,-1,7200000,	2,-1,1,10800000 };
static TimeZoneRule NoumeaDST[]		= { 0,    10,-1,1,7200000,	2,1,-1,10800000 };
static TimeZoneRule ChathamDST[]	= { 0,    9,1,-1,9900000,	2,15,-1,13500000 };
static TimeZoneRule StandleyDST[]	= { 0,    8,8,-1,0,			3,16,-1,0 };
static TimeZoneRule AsuncionDST[]	= { 0,    9,1,0,0,			2,1,0,0 };
//static TimeZoneRule DST		= {  };

#define TIMEZONE(abbr,name,offset,timeZoneRule,dst)\
	abbr,name,offset/1000,timeZoneRule,dst/1000,NULL,
	
	//abbr,name,offset/1000,startMonth,startDay,startDayOfWeek,StartTime/1000,endMonth,endDay,endDayOfWeek,endTime/1000,dst/1000,NULL,

static TimeZone timeZoneData [] = {
	TIMEZONE ("MIT","Pacific/Apia",-39600000,noDST,3600000)
	TIMEZONE ("HST","Pacific/Honolulu",-36000000,noDST,3600000)
	TIMEZONE ("AST","America/Anchorage",-32400000,USDST,3600000)
	TIMEZONE ("PST","America/Los_Angeles",-28800000,USDST,3600000)
	TIMEZONE ("MST","America/Phoenix",-25200000,noDST,3600000)
	TIMEZONE ("MST","America/Denver",-25200000,USDST,3600000)
	TIMEZONE ("CST","America/Chicago",-21600000,USDST,3600000)
	TIMEZONE ("IET","America/Indianapolis",-18000000,noDST,3600000)
	TIMEZONE ("EST","America/New_York",-18000000,USDST,3600000)
	TIMEZONE ("PRT","America/Caracas",-14400000,noDST,3600000)
	TIMEZONE ("AST","America/Halifax",-14400000,USDST,3600000)
	TIMEZONE ("CNT","America/St_Johns",-12600000,USDST,3600000)
	TIMEZONE ("AGT","America/Buenos_Aires",-10800000,noDST,3600000)
	TIMEZONE ("BET","America/Sao_Paulo",-10800000,BrazilDST,3600000)
	TIMEZONE ("CAT","Atlantic/Cape_Verde",-3600000,noDST,3600000)
	TIMEZONE ("CAT","Atlantic/Azores",-3600000,AzoresDST,3600000)
	TIMEZONE ("GMT","Africa/Casablanca",0,noDST,3600000)
	TIMEZONE ("ECT","Europe/Paris",3600000,ParisDST,3600000)
	TIMEZONE ("EET","Europe/Istanbul",7200000,InstanbulDST,3600000)
	TIMEZONE ("ART","Africa/Cairo",7200000,CairoDST,3600000)
	TIMEZONE ("EAT","Asia/Riyadh",10800000,noDST,3600000)
	TIMEZONE ("MET","Asia/Tehran",12600000,TehranDST,3600000)
	TIMEZONE ("NET","Asia/Yerevan",14400000,noDST,3600000)
	TIMEZONE ("PLT","Asia/Karachi",18000000,noDST,3600000)
	TIMEZONE ("IST","Asia/Calcutta",19800000,noDST,3600000)
	TIMEZONE ("BST","Asia/Dacca",21600000,noDST,3600000)
	TIMEZONE ("VST","Asia/Bangkok",25200000,noDST,3600000)
	TIMEZONE ("CTT","Asia/Shanghai",28800000,noDST,3600000)
	TIMEZONE ("JST","Asia/Tokyo",32400000,noDST,3600000)
	TIMEZONE ("ACT","Australia/Darwin",34200000,noDST,3600000)
	TIMEZONE ("AS", "Australia/Adelaide",34200000,AustralianDST,3600000)
	TIMEZONE ("AET","Australia/Sydney",36000000,AustralianDST,3600000)
	TIMEZONE ("SST","Pacific/Guadalcanal",39600000,noDST,3600000)
	TIMEZONE ("NST","Pacific/Fiji",43200000,noDST,3600000)
	TIMEZONE ("NZT","Pacific/Auckland",43200000,AucklandDST,3600000)
	TIMEZONE ("xxx","America/Adak",-36000000,USDST,3600000)
	TIMEZONE ("xxx","Pacific/Rarotonga",-36000000,RarotongaDST,1800000)
	TIMEZONE ("xxx","Pacific/Marquesas",-34200000,noDST,0)
	TIMEZONE ("xxx","Pacific/Gambier",-32400000,noDST,0)
	TIMEZONE ("xxx","Pacific/Pitcairn",-30600000,noDST,0)
	TIMEZONE ("xxx","America/Costa_Rica",-21600000,noDST,0)
	TIMEZONE ("xxx","Pacific/Easter",-21600000,ChileDST,3600000)
	TIMEZONE ("xxx","America/Havana",-18000000,CubaDST,3600000)
	TIMEZONE ("xxx","America/Port-au-Prince",-18000000,HaitiDST,3600000)
	TIMEZONE ("xxx","America/Grand_Turk",-18000000,GrandTurkDST,3600000)
	TIMEZONE ("xxx","America/Cuiaba",-14400000,BrazilDST,3600000)
	TIMEZONE ("xxx","America/Halifax",-14400000,USDST,3600000)
	TIMEZONE ("xxx","America/Santiago",-14400000,ChileDST,3600000)
	TIMEZONE ("xxx","Atlantic/Stanley",-14400000,StandleyDST,3600000)
	TIMEZONE ("xxx","America/Asuncion",-14400000,AsuncionDST,3600000)
	TIMEZONE ("xxx","America/Miquelon",-10800000,USDST,3600000)
	TIMEZONE ("xxx","America/Godthab",-10800000,GodthabDST,3600000)
	TIMEZONE ("xxx","Atlantic/South_Georgia",-7200000,noDST,0)
	TIMEZONE ("GMT","Europe/London",0,LondonDST,3600000)
	TIMEZONE ("xxx","Africa/Lagos",3600000,noDST,0)
	TIMEZONE ("xxx","Africa/Tripoli",3600000,TripoliDST,3600000)
	TIMEZONE ("xxx","Europe/Warsaw",3600000,WarsawDST,3600000)
	TIMEZONE ("xxx","Africa/Johannesburg",7200000,noDST,0)
	TIMEZONE ("xxx","Europe/Bucharest",7200000,AzoresDST,3600000)
	TIMEZONE ("xxx","Asia/Amman",7200000,AmmanDST,3600000)
	TIMEZONE ("xxx","Europe/Riga",7200000,RigaDST,3600000)
	TIMEZONE ("xxx","Asia/Beirut",7200000,BeruitDST,3600000)
	TIMEZONE ("xxx","Africa/Windhoek",7200000,WindhoekDST,3600000)
	TIMEZONE ("xxx","Europe/Minsk",7200000,RussianDST,3600000)
	TIMEZONE ("xxx","Asia/Damascus",7200000,DamascusDST,3600000)
	TIMEZONE ("xxx","Asia/Jerusalem",7200000,JerusalamDST,3600000)
	TIMEZONE ("xxx","Europe/Simferopol",10800000,InstanbulDST,3600000)
	TIMEZONE ("xxx","Asia/Baghdad",10800000,BagdadDST,3600000)
	TIMEZONE ("xxx","Europe/Moscow",10800000,RussianDST,3600000)
	TIMEZONE ("xxx","Asia/Aqtau",14400000,AzoresDST,3600000)
	TIMEZONE ("xxx","Asia/Baku",14400000,BakuDST,3600000)
	TIMEZONE ("xxx","Europe/Samara",14400000,RussianDST,3600000)
	TIMEZONE ("xxx","Asia/Kabul",16200000,noDST,0)
	TIMEZONE ("xxx","Asia/Aqtobe",18000000,AzoresDST,3600000)
	TIMEZONE ("xxx","Asia/Bishkek",18000000,BishkekDST,3600000)
	TIMEZONE ("xxx","Asia/Yekaterinburg",18000000,RussianDST,3600000)
	TIMEZONE ("xxx","Asia/Katmandu",20700000,noDST,0)
	TIMEZONE ("xxx","Asia/Alma-Ata",21600000,AzoresDST,3600000)
	TIMEZONE ("xxx","Asia/Novosibirsk",21600000,RussianDST,3600000)
	TIMEZONE ("xxx","Asia/Rangoon",23400000,noDST,0)
	TIMEZONE ("xxx","Asia/Krasnoyarsk",25200000,RussianDST,3600000)
	TIMEZONE ("xxx","Asia/Ulan_Bator",28800000,BeruitDST,3600000)
	TIMEZONE ("xxx","Asia/Irkutsk",28800000,RussianDST,3600000)
	TIMEZONE ("xxx","Asia/Yakutsk",32400000,RussianDST,3600000)
	TIMEZONE ("xxx","Australia/Brisbane",36000000,noDST,0)
	TIMEZONE ("xxx","Australia/Hobart",36000000,HobartDST,3600000)
	TIMEZONE ("xxx","Asia/Vladivostok",36000000,RussianDST,3600000)
	TIMEZONE ("xxx","Australia/Lord_Howe",37800000,AustralianDST,1800000)
	TIMEZONE ("xxx","Pacific/Noumea",39600000,NoumeaDST,3600000)
	TIMEZONE ("xxx","Asia/Magadan",39600000,RussianDST,3600000)
	TIMEZONE ("xxx","Pacific/Norfolk",41400000,noDST,0)
	TIMEZONE ("xxx","Asia/Kamchatka",43200000,RussianDST,3600000)
	TIMEZONE ("xxx","Pacific/Chatham",45900000,ChathamDST,3600000)
	TIMEZONE ("xxx","Pacific/Tongatapu",46800000,noDST,0)
	TIMEZONE ("xxx","Asia/Anadyr",46800000,RussianDST,3600000)
	TIMEZONE ("xxx","Pacific/Kiritimati",50400000,noDST,0)
	NULL,NULL,0,noDST,0,NULL
	};

struct Alias {
	const char		*alias;
	const char		*name;
	Alias			*collision;
	const TimeZone	*timeZone;
	};

#define ALIAS(alias,name)	alias,name,NULL,NULL,

static Alias aliasData [] = {
	ALIAS ("ACT","Australia/Darwin")
	ALIAS ("AET","Australia/Sydney")
	ALIAS ("AGT","America/Buenos_Aires")
	ALIAS ("ART","Africa/Cairo")
	ALIAS ("AS", "Australia/Adelaide")
	//ALIAS ("AST","America/Anchorage")
	ALIAS ("AST","America/Halifax")
	ALIAS ("BET","America/Sao_Paulo")
	ALIAS ("BST","Asia/Dacca")
	//ALIAS ("CAT","Atlantic/Cape_Verde")
	ALIAS ("CAT","Atlantic/Azores")
	ALIAS ("CTT","Asia/Shanghai")
	ALIAS ("CNT","America/St_Johns")
	ALIAS ("CST","America/Chicago")
	ALIAS ("EAT","Asia/Riyadh")
	ALIAS ("ECT","Europe/Paris")
	ALIAS ("CET","Europe/Paris")
	ALIAS ("EET","Europe/Istanbul")
	ALIAS ("EST","America/New_York")
	ALIAS ("GMT","Africa/Casablanca")
	ALIAS ("HST","Pacific/Honolulu")
	ALIAS ("IET","America/Indianapolis")
	ALIAS ("IST","Asia/Calcutta")
	ALIAS ("JST","Asia/Tokyo")
	ALIAS ("MET","Asia/Tehran")
	ALIAS ("MIT","Pacific/Apia")
	//ALIAS ("MST","America/Phoenix")
	ALIAS ("MST","America/Denver")
	ALIAS ("PRT","America/Caracas")
	ALIAS ("PST","America/Los_Angeles")
	ALIAS ("NET","Asia/Yerevan")
	ALIAS ("NST","Pacific/Fiji")
	ALIAS ("NZT","Pacific/Auckland")
	ALIAS ("PLT","Asia/Karachi")
	ALIAS ("VST","Asia/Bangkok")
	ALIAS ("SST","Pacific/Guadalcanal")
	ALIAS ("Greenwich Mean Time",			"Africa/Casablanca")
	ALIAS ("Greenwich Standard Time",		"Africa/Casablanca")
    ALIAS ("GMT Standard Time",				"Europe/London")
    ALIAS ("Romance Standard Time",			"Europe/Paris")
    ALIAS ("W. Europe Standard Time",		"Europe/Paris")
    ALIAS ("Central Europe Standard Time",	"Europe/Paris")
    ALIAS ("Egypt Standard Time",			"Africa/Cairo")
    ALIAS ("Saudi Arabia Standard Time",	"Asia/Riyadh")
    ALIAS ("Iran Standard Time",			"Asia/Tehran")
    ALIAS ("Arabian Standard Time",			"Asia/Yerevan")
    ALIAS ("West Asia Standard Time",		"Asia/Karachi")
    ALIAS ("India Standard Time",			"Asia/Calcutta")
    ALIAS ("Central Asia Standard Time",	"Asia/Dacca")
    ALIAS ("Bangkok Standard Time",			"Asia/Bangkok")
    ALIAS ("China Standard Time",			"Asia/Shanghai")
    ALIAS ("Tokyo Standard Time",			"Asia/Tokyo")
    ALIAS ("Cen. Australia Standard Time",	"Australia/Darwin")
    ALIAS ("Sydney Standard Time",			"Australia/Sydney")
    ALIAS ("Central Pacific Standard Time",	"Pacific/Guadalcanal")
    ALIAS ("New Zealand Standard Time",		"Pacific/Fiji")
    ALIAS ("Samoa Standard Time",			"Pacific/Apia")
    ALIAS ("Hawaiian Standard Time",		"Pacific/Honolulu")
    ALIAS ("Alaskan Standard Time",			"America/Anchorage")
    ALIAS ("Pacific Standard Time",			"America/Los_Angeles")
    ALIAS ("US Mountain Standard Time",		"America/Denver")
    ALIAS ("Central Standard Time",			"America/Chicago")
    ALIAS ("Eastern Standard Time",			"America/New_York")
    ALIAS ("Atlantic Standard Time",		"America/Caracas")
    ALIAS ("Newfoundland Standard Time",	"America/St_Johns")
    ALIAS ("SA Eastern Standard Time",		"America/Buenos_Aires")
    ALIAS ("E. South America Standard Time","America/Sao_Paulo")
    ALIAS ("Azores Standard Time",			"Atlantic/Cape_Verde")
	NULL, NULL, NULL, NULL };

static const short	monthLengths [] = {31,28,31,30,31,30,31,31,30,31,30,31};
static int			init();
static TimeZone		*timeZones [HASH_SIZE];
static Alias		*aliases [HASH_SIZE];
static int			isTzset = init();
static const TimeZone		*defaultTimeZone;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

int init()
{
	for (TimeZone *timeZone = timeZoneData; timeZone->abbr; ++timeZone)
		{
		int slot = JString::hash (timeZone->name, HASH_SIZE);
		timeZone->collision = timeZones [slot];
		timeZones [slot] = timeZone;
		}

	for (Alias *alias = aliasData; alias->alias; ++alias)
		{
		alias->timeZone = DateTime::findTimeZone (alias->name);
#ifdef ENGINE
		ASSERT (alias->timeZone);
#endif
		int slot = JString::hash (alias->alias, HASH_SIZE);
		alias->collision = aliases [slot];
		aliases [slot] = alias;
		}

	tzset();
	DateTime::getDefaultTimeZone();

	return 0;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


DateTime DateTime::convert(const char *dateString, int length)
{
	DateTime date;
	const char *end = dateString + length;
	char	string [100], *q = string;
	bool	numeric = true;
	int		year = -1;
	int		month = 0;
	int		day = 0;
	int		hour = 0;
	int		second = 0;
	int		minute = 0;
	int		timezone = 0;
	int		n;
	int		state = 0;			// 1 = hour, 2 = minute, 3 = second
	bool	relativeDate = false;
	bool	euroDate = false;
	bool	hardMonth = false;
	tm		today;
	const char *p = dateString;
	const TimeZone	*zone = NULL;

	if (match (NOW, dateString))
		{
		date.setNow();
		return date;
		}

	if (match (TODAY, dateString))
		return relativeDay (0);

	if (match (TOMORROW, dateString))
		return relativeDay (1);

	if (match (YESTERDAY, dateString))
		return relativeDay (-1);

	for (char c = 1; c;)
		{
		if (p < end)
			c = *p++;
		else
			c = 0;
		switch (c)
			{
			case '-':
			case '+':
				if (q > string)
					--p;
			case ' ':
			case ',':
			case '/':
			case ':':
			case ')':
			case '.':
			case 0:
				if (c == '.')
					euroDate = true;
				if (q > string)
					{
					*q = 0;
					if (numeric)
						{
						n = atoi (string);
						
						if (n > 100 && year < 0)
							year = n;
						else if (month == 0)
							month = n;
						else if (day == 0)
							day = n;
						else if (year < 0)
							year = n;
						else
							switch (state++)
								{
								case 0:
									hour = n;
									break;

								case 1:
									minute = n;
									break;

								case 2:
									second = n;
									break;

								case 3:
									timezone = n / 100 * 60 + n % 100;
									break;

								default:
									return conversionError();
								}
						}
					else if ((n = lookup (string, months)) >= 0)
						{
						if (month && !day)
							day = month;
							
						month = n + 1;
						hardMonth = true;
						}
					else if ((n = lookup (string, weekDays)) >= 0)
						{
						}
					else if ( (zone = findTimeZone (string)) )
						timezone = zone->offset;
					else if ((n = lookup (string, meridians)) >= 0)
						{
						if (hour > 23)
							throw SQLEXCEPTION (CONVERSION_ERROR, 
								"error converting time from %s", dateString);
								
						if (n && hour < 12)
							hour += 12;
						}
					else if (match (THIS_MONTH, string))
						{
						if (month && !day)
							day = month;
							
						if (!relativeDate)
							{
							getNow (&today);
							relativeDate = true;
							}
							
						month = today.tm_mon + 1 + getDelta (&p);
						hardMonth = true;
						}
					else if (match (THIS_DAY, string))
						{
						if (!relativeDate)
							{
							getNow (&today);
							relativeDate = true;
							}
							
						day = today.tm_mday + getDelta (&p);
						}
					else if (match (THIS_YEAR, string))
						{
						if (!relativeDate)
							{
							getNow (&today);
							relativeDate = true;
							}
							
						year = today.tm_year + 1900 + getDelta (&p);
						}
					else
						{
						//n = lookup (string, timezones);
						//return conversionError();
						}
					}
				q = string;
				numeric = true;
				break;

			case '(':
				break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				*q++ = c;
				break;

			default:
				*q++ = c;
				if (p < end && *p == '.')
					*q++ = *p++;
				numeric = false;			
			}
		}

	// Assume dates with embedded periods are European style

	if (!hardMonth && euroDate)
		{
		n = month;
		month = day;
		day = n;
		}

	// Tweak the time if given in military style (1730)

	if (hour > 100)
		{
		minute = hour % 100;
		hour = hour / 100;
		}

	if (year < 0)
		{
		struct tm time;
		getNow (&time);
		year = time.tm_year + 1900;
		}
	else if (year < 100)
		{
		if (year > 70)
			year += 1900;
		else
			year += 2000;
		}

	tm	time;
	memset (&time, 0, sizeof (time));
	time.tm_sec = second;
	time.tm_min = minute;
	time.tm_hour = hour;
	time.tm_mday = day;
	time.tm_mon = month - 1;
	time.tm_year = year - 1900;
	time.tm_isdst = -1;
    date.date = MILLISECONDS (getSeconds (&time));
	tm time2;
	getLocalTime (date.date, &time2);

	if (!relativeDate &&
	    (date.date == -1 || time2.tm_mon != month - 1 || time2.tm_mday != day))
		{
		getLocalTime (date.date, &time2);
		throw SQLEXCEPTION (CONVERSION_ERROR, 
								"error converting to date from %s", dateString);
		}

	return date;
}

int DateTime::lookup(const char *string, const char **table)
{
	char temp [128], *q = temp, c;

	for (const char *p = string; (c = *p++);)
		if (c != '.')
			*q++ = c;

	*q = 0;

	for (const char **tbl = table; *tbl; ++tbl)
		if (match (temp, *tbl))
			return tbl - table;

	return -1;
}

bool DateTime::match(const char * str1, const char * str2)
{
	for (; *str1 && *str2; ++str1, ++str2)
		if (UPPER (*str1) != UPPER (*str2))
			{
			
			return false;
			}

	return *str1 == 0;
}

DateTime DateTime::conversionError()
{
	DateTime date;
	date.date = 01;

	return date;
}

int DateTime::getString(int length, char * buffer)
{
	tm time;
	getLocalTime (&time);
	snprintf (buffer, length, "%d-%.2d-%.2d",
			  time.tm_year + 1900, 
			  time.tm_mon + 1, 
			  time.tm_mday);

	return strlen (buffer);
}


time_t DateTime::getNow()
{
	time_t t;
	time (&t);

	return t;
}

double DateTime::getDouble()
{
	return (double) date;
}	

Time Time::convert(const char *timeString, int length)
{
	const char	*p = timeString;
	const char	*end = p + length;
	char		string [100], *q = string;
	char		*endString = string + sizeof (string) - 1;
	bool		numeric = true;
	int			state = 0;
	int			n;
	int			values [3];
	values [0] = values [1] = values [2] = 0;

	if (match (NOW, timeString))
		{
		tm	time;
		getNow (&time);
		time.tm_mday = 1;
		time.tm_mon = 0;
		time.tm_year = 1970 - 1900;
		time.tm_isdst = -1;
		Time date;
		date.date = MILLISECONDS (getSeconds (&time));
		
		return date;
		}

	for (char c = 1; c;)
		{
		if (q >= endString)
			throw SQLEXCEPTION (CONVERSION_ERROR, 
				"error converting time from %*s", length, timeString);

		if (p < end)
			c = *p++;
		else
			c = 0;
			
		switch (c)
			{
			case '-':
			case '+':
				if (q > string)
					--p;
			case ' ':
			case ',':
			case '/':
			case ':':
			case ')':
			case '.':
			case 0:
				if (q > string)
					{
					*q = 0;
					if (numeric)
						{
						n = atoi (string);
						
						if (state >= 3)
							throw SQLEXCEPTION (CONVERSION_ERROR, 
								"error converting time from %*s", length, timeString);
								
						values [state++] = n;
						}
					else
						{
						if ((n = lookup (string, meridians)) >= 0)
							{
							if (values [0] > 23)
								throw SQLEXCEPTION (CONVERSION_ERROR, 
									"error converting time from %s", timeString);
									
							if (n && values [0] < 12)
								values [0] += 12;
							}
						else
							throw SQLEXCEPTION (CONVERSION_ERROR, 
								"error converting time from %*s", length, timeString);
						}
					}
				q = string;
				numeric = true;
				break;

			case '(':
				break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				*q++ = c;
				break;

			default:
				if (q > string && numeric)
					{
					*q = 0;
					n = atoi (string);
					
					if (state >= 3)
						throw SQLEXCEPTION (CONVERSION_ERROR, 
							"error converting time from %*s", length, timeString);
							
					values [state++] = n;
					q = string;
					}
				*q++ = c;
				
				if (p < end && *p == '.')
					*q++ = *p++;
					
				numeric = false;
				break;
			}
		}

	// Tweak the time if given in military style (1730)

	if (values [0] > 100)
		{
		values [1] = values [0] % 100;
		values [0] /= 100;
		}

	Time date;
	tm	time;
	memset (&time, 0, sizeof (time));
	time.tm_sec = values [2];
	time.tm_min = values [1];
	time.tm_hour = values [0];
	time.tm_mday = 1;
	time.tm_mon = 0;
	time.tm_year = 1970 - 1900;
	time.tm_isdst = -1;
    date.date = MILLISECONDS (getSeconds (&time));

	return date;
}


DateTime DateTime::relativeDay(int deltaDay)
{
	struct tm	time;
	getToday (&time);
	time.tm_mday += deltaDay;
	DateTime date;
	date.date = MILLISECONDS (getSeconds (&time));

	return date;
}

int64 DateTime::getDate(short year, short month, short day)
{
	month += 1;

	if (month > 2)
		month -= 3;
	else
		{
		month += 9;
		year -= 1;
		}

	int32 century = year / 100;
	int32 ya = year - 100 * century;

	return (int32) (((int64) 146097 * century) / 4 + 
		(1461 * ya) / 4 + 
		(153 * month + 2) / 5 + 
		day - BASE_DATE);
}

void DateTime::getYMD(int64 secs, tm *time)
{
	int64 date = secs / SECONDS_PER_DAY;
	int32 seconds = (int32) (secs % SECONDS_PER_DAY);

	if (seconds < 0)
		{
		seconds += SECONDS_PER_DAY;
		--date;
		}

	int64 nday = date  + BASE_DATE;
	int32 century = (int32) ((4 * nday - 1) / 146097);
	nday = 4 * nday - 1 - 146097 * century;
	int64 day = (nday / 4);

	nday = (4 * day + 3) / 1461;
	day =  4 * day + 3 - 1461 * nday;
	day = (day + 4) / 4;

	int32 month = (int32) ((5 * day - 3) / 153);
	day = 5 * day - 3 - 153 * month;
	day = (day + 5) / 5;

	int32 year = 100 * century + (int32) nday;

	if (month < 10)
		month += 3;
	else
		{
		month -= 9;
		year += 1;
		}

	time->tm_mday = (short) day;
	time->tm_mon = month - 1;
	time->tm_year = (short) year - 1900;
	time->tm_sec = (int) (seconds % 60);
	time->tm_min = (int) ((seconds / 60) % 60);
	time->tm_hour = (int) ((seconds / (60 * 60)) % 24);
	time->tm_wday = (int) (secs / SECONDS_PER_DAY + WEEKDAY) % 7;

	if (time->tm_wday < 0)
		time->tm_wday += 7;
}

int64 DateTime::getSeconds(tm *time)
{
	const TimeZone *timeZone = getDefaultTimeZone();

	return getSeconds (time, timeZone);
}


int64 DateTime::getSeconds(tm *time, const TimeZone *timeZone)
{
	int64 day = getDate (time->tm_year + 1900, time->tm_mon, time->tm_mday);
	int64 seconds = day * SECONDS_PER_DAY;
	seconds += time->tm_sec + 60 * (time->tm_min + 60 * time->tm_hour);
	seconds -= timeZone->offset;

	if (checkDayLightSavings (time, timeZone))
		seconds -= 60 * 60;

	return seconds;
}

bool DateTime::isDayLightSavings(tm *time)
{
	if (!time->tm_isdst)
		return false;

	// Jan to March and November and December are definitely no

	if (time->tm_mon < 3 || time->tm_mon > 9)
		return false;

	// May through September are definitely yes

	if (time->tm_mon > 3 && time->tm_mon < 9)
		return true;

	int64 first = getDate (time->tm_year + 1900, time->tm_mon, 1);
	int64 weekday = (first + WEEKDAY) % 7;
	int64 sunday = (weekday) ? 8 - weekday : 1;
	bool before = false;

	if (time->tm_mon == 9)
		{
		before = true;

		while (sunday + 7 <= 31)
			sunday += 7;
		}

	if (time->tm_mday < sunday)
		return before;

	if (time->tm_mday > sunday)
		return !before;

	if (time->tm_hour < 2)
		return before;

	return !before;
}

void DateTime::getLocalTime(tm *time)
{
	getLocalTime (date, time);
}

void DateTime::getLocalTime(int64 milliseconds, tm *time)
{
	const TimeZone *timeZone = getDefaultTimeZone();
	int64 seconds = (milliseconds / 1000 + timeZone->offset);
	getYMD (seconds, time);

	if (checkDayLightSavings (time, timeZone))
		{
		seconds += timeZone->dst;
		getYMD (seconds, time);
		}

	int t = (int) (milliseconds/1000);
	if (t >= 0 && milliseconds >= 0)
		checkConversion (t, time, timeZone);
}

int64 DateTime::getSeconds()
{
	return date / 1000;
}

int64 DateTime::getMilliseconds()
{
	return date;
}

void DateTime::setSeconds(int64 seconds)
{
	date = MILLISECONDS (seconds);
}

void DateTime::setMilliseconds(int64 milliseconds)
{
	date = milliseconds;
}

/***
void Time::setTime(int32 time)
{

}
***/

void DateTime::setNow()
{
	//time (&date);
	setSeconds (time (NULL));
}

bool DateTime::before(DateTime when)
{
	return date < when.date;
}

bool DateTime::after(DateTime when)
{
	return date > when.date;
}

bool DateTime::equals(DateTime when)
{
	return date == when.date;
}

void DateTime::setNull()
{
	date = 0;
}

bool DateTime::isNull()
{
	return date == 0;
}

int DateTime::compare(DateTime when)
{
	if (date > when.date)
		return 1;
		
	if (date < when.date)
		return -1;

	return 0;
	//return date - when.date;
}

DateTime DateTime::convert(const char *string)
{
	return convert (string, strlen (string));
}

const char* DateTime::getTimeZone()
{
	/***
	//printf ("%s;%s\n", tzname [0], (tzname[1]) ? tzname[1] : "");
	if (!defaultTimeZone)
		return getDefaultTimeZone()->abbr;

	return defaultTimeZone->abbr;
	***/
	return getDefaultTimeZone()->abbr;
}

void DateTime::getNow(tm *tm)
{
	time_t t = time (NULL);
	const TimeZone *timeZone = getDefaultTimeZone();
	time_t local = t + timeZone->offset;
	getYMD (local, tm);

	if (checkDayLightSavings (tm, timeZone))
		getYMD (local + timeZone->dst, tm);

	tm->tm_isdst = -1;
	checkConversion (t, tm, timeZone);
}


void DateTime::getToday(tm *time)
{
	getNow (time);
	time->tm_sec = 0;
	time->tm_min = 0;
	time->tm_hour = 0;
	time->tm_isdst = -1;
}

int DateTime::getDelta(const char **ptr)
{
	const char *p = *ptr;

	// skip over whitespace

	while (*p == ' ')
		++p;

	char c = *p++;

	// if we're not a plus minus, bad the entire exercise

	if (c != '-' && c != '+')
		return 0;

	// skip more whitespace

	while (*p == ' ')
		++p;

	// gobble a number

	int n = 0;

	while (*p >= '0' && *p <= '9')
		n = n * 10 + *p++ - '0';

	*ptr = p;

	return (c == '+') ? n : -n;
}

void DateTime::add(int64 seconds)
{
	date += MILLISECONDS (seconds);
}

int Time::getString(int length, char *buffer)
{
	tm time;
	getLocalTime (&time);
	//snprintf (buffer, length, "%d-%.2d-%.2d",
	snprintf (buffer, length, "%.2d:%.2d:%.2d",
			  time.tm_hour, 
			  time.tm_min, 
			  time.tm_sec);

	return strlen (buffer);
}

const TimeZone* DateTime::findTimeZone(const char *string)
{
	int slot = JString::hash (string, HASH_SIZE);

	for (TimeZone *timeZone = timeZones [slot]; timeZone; timeZone = timeZone->collision)
		if (strcasecmp (string, timeZone->name) == 0)
			return timeZone;

	for (Alias *alias = aliases [slot]; alias; alias = alias->collision)
		if (strcasecmp (string, alias->alias) == 0)
			return alias->timeZone;

	return NULL;
}

const TimeZone* DateTime::getDefaultTimeZone()
{
#ifdef ENGINE
	Thread *thread = Thread::findThread();

	if (thread)
		{
		const TimeZone *timeZone = thread->defaultTimeZone;
		
		if (timeZone)
			return timeZone;
		}
#endif

	if (defaultTimeZone)
		return defaultTimeZone;

	if ( (defaultTimeZone = DateTime::findTimeZone (tzname [0])) )
		return defaultTimeZone;

	defaultTimeZone = DateTime::findTimeZone ("PST");

	return defaultTimeZone;
}

bool DateTime::isDayLightSavings(tm *time, const TimeZone *timeZone)
{
	if (!timeZone->dst)
		return false;

	TimeZoneRule *timeZoneRule = timeZone->timeZoneRule;
	int year = time->tm_year + 1900;
	
	while (year < timeZoneRule->year)
		++timeZoneRule;
		
	// Jan to March (now Feb.) and (formerly November and) December are definitely no

	if (time->tm_mon < timeZoneRule->startRule.month || time->tm_mon > timeZoneRule->endRule.month)
		return false;

	// May through September are definitely yes

	if (time->tm_mon > timeZoneRule->startRule.month && time->tm_mon < timeZoneRule->endRule.month)
		return true;

	int64 first = getDate(year + 1900, time->tm_mon, 1);

	if (time->tm_mon == timeZoneRule->startRule.month)
		{
		int ret = timeRule(time, first, &timeZoneRule->startRule);
		
		if (ret < 0)
			return false;
			
		if (ret > 0)
			return true;
		}
	else
		{
		int ret = timeRule(time, first, &timeZoneRule->endRule);
		
		if (ret > 0)
			return false;
			
		if (ret < 0)
			return true;
		}

	return true;
}

// Returns -1 if day before rule day, +1 if after, 0 if exactly equal

int DateTime::timeRule(tm *time, int64 first, const ZoneRule *rule)
{
	int weekday = (int) (first + WEEKDAY) % 7;
	int day = ABS(rule->day);
	int dayOfWeek = ABS(rule->dayOfWeek) - 1;
	int changeDay = 1 + dayOfWeek - weekday;

	// Positive day means first weekday in month
	// Negative day means last weekday in month

	if (rule->day > 0)
		{
		while (changeDay < day)
			changeDay += 7;
		}
	else if (rule->day < 0)
		{
		int max = monthLengths[time->tm_mon];
		
		while (changeDay + 7 < max)
			changeDay += 7;
		}

	if (time->tm_mday < changeDay)
		return -1;
		
	if (time->tm_mday > changeDay)
		return 1;

	int seconds = time->tm_hour * 60 * 60 + time->tm_min * 60 + time->tm_sec;

	if (seconds < rule->time)
		return -1;
		
	if (seconds > rule->time)
		return 1;

	return 0;	
}

void DateTime::checkConversion(time_t t, tm *tm, const TimeZone *timeZone)
{
	/***
	struct tm tm2 = *localtime (&t);

	if (tm2.tm_mday != tm->tm_mday ||
	    tm2.tm_hour != tm->tm_hour)
		{
		struct tm tm3;
		getYMD (t, &tm3);
		printf ("bad conversion from %d %s(%d): %d:%d %d/%d/%d <> %d:%d %d/%d/%d \n", 
				t, timeZone->abbr, timeZone->offset,
				tm->tm_hour, tm->tm_min, tm->tm_mon, tm->tm_mday, tm->tm_year,
				tm2.tm_hour, tm2.tm_min, tm2.tm_mon, tm2.tm_mday, tm2.tm_year);
		}
	***/
}

bool DateTime::checkDayLightSavings(tm *time, const TimeZone *timeZone)
{
	bool ret = isDayLightSavings (time, timeZone);

	if (ret != isDayLightSavings (time))
		{
		isDayLightSavings (time);
		isDayLightSavings (time, timeZone);
		}

	return ret;
}
