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

// Validation.cpp: implementation of the Validation class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include "Engine.h"
#include "Validation.h"
#include "BDB.h"
#include "Log.h"
#include "Connection.h"

//#define STOP_PAGE		2896

#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Validation::Validation(Dbb *db, int validationOptions)
{
	dbb = db;
	options = validationOptions;
	phase = 0;
	dups = false;
	highPage = 0;
	memset (pageCounts, 0, sizeof (pageCounts));
	stopPage = 0;
}

Validation::~Validation()
{

}

bool Validation::inUse(int32 pageNumber, const char *description)
{
	if (stopPage && pageNumber == stopPage)
		Log::debug("Stop page %d in validation\n", pageNumber);
		
	if (phase == 1)
		{
		if (duplicates.isSet (pageNumber))
			Log::debug ("  Page %d referenced as %s\n", pageNumber, description);
			
		return true;
		}

	bool allocated = pages.isSet (pageNumber);

	if (allocated)
		{
		//Log::debug ("  Page %d is multiply allocated as %s\n", pageNumber, description);
		dups = true;
		duplicates.set (pageNumber);
		}
	else
		pages.set (pageNumber);

	return allocated;
}

void Validation::sectionInUse(int sectionId)
{
	sections.set (sectionId);
}

bool Validation::error(const char *text, ...)
{
	va_list		args;
	va_start	(args, text);
	char temp [1024];

	if (vsnprintf (temp, sizeof (temp) - 1, text, args) < 0)
		temp [sizeof (temp) - 1] = 0;

	Log::debug ("  %s\n", temp);
	++errors;
	
	if (options & validateOrBreak)
		Error::error("  %s\n", temp);
		
	return false;
}


bool Validation::warning(const char* text, ...)
{
	va_list		args;
	va_start	(args, text);
	char temp [1024];

	if (vsnprintf (temp, sizeof (temp) - 1, text, args) < 0)
		temp [sizeof (temp) - 1] = 0;

	Log::debug ("  %s\n", temp);
	++errors;
		
	return false;
}

bool Validation::isPageType(Bdb *bdb, PageType type, const char *text, ...)
{
#ifdef STOP_PAGE
	if (bdb->pageNumber == STOP_PAGE)
		Log::debug (" validating page %d\n", bdb->pageNumber);
#endif

	if (bdb->pageNumber > highPage)
		highPage = bdb->pageNumber;
		
	if (bdb->buffer->pageType == type)
		{
		++pageCounts [(int) type];
		return true;
		}

	dups = true;
	duplicates.set (bdb->pageNumber);

	va_list		args;
	va_start	(args, text);
	char temp [1024];

	if (vsnprintf (temp, sizeof (temp) - 1, text, args) < 0)
		temp [sizeof (temp) - 1] = 0;

	if (phase == 0)
		error ("Page %d (%s) wrong type; expected %d, got %d", 
				   bdb->pageNumber, temp, type, bdb->buffer->pageType);

	return false;
}

bool Validation::inUse(Bdb *bdb, const char *description)
{
	return inUse (bdb->pageNumber, description);
}

bool Validation::isRepair()
{
	// Check for backwards compatibility

	if (options == -1)
		return false;

	return (options & validateRepair) != 0;
}

bool Validation::minutia()
{
	return (options & validateMinutia) != 0;
}
