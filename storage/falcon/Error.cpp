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

// Error.cpp: implementation of the Error class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include "Engine.h"
#include "Error.h"
#include "SQLError.h"
#include "Log.h"
//#include "MemMgr.h"

#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif

//#define CHECK_HEAP

#ifdef CHECK_HEAP
#include <crtdbg.h>
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


void Error::error(const char * string, ...)
{
	char buffer [256];
	va_list	args;
	va_start (args, string);

	if (vsnprintf (buffer, sizeof (buffer) - 1, string, args) < 0)
		buffer [sizeof (buffer) - 1] = 0;

#ifdef ENGINE
	Log::logBreak ("Bugcheck: %s\n", buffer);
	//MemMgrLogDump();
#endif

	debugBreak();

	throw SQLEXCEPTION (BUG_CHECK, buffer);
}

void Error::assertionFailed(const char * fileName, int line)
{
	error ("assertion failed at line %d in file %s\n", line, fileName);
}

void Error::validateHeap(const char *where)
{
#ifdef CHECK_HEAP
	if (!_CrtCheckMemory())
		Log::debug ("***> memory corrupted at %s!!!\n", where);
#endif
}

void Error::debugBreak()
{
#ifdef _WIN32
	DebugBreak();
#else
	raise (SIGILL);
#endif
}

void Error::notYetImplemented(const char *fileName, int line)
{
#ifdef ENGINE
	Log::logBreak ("feature not yet implemented at line %d in file %s\n", line, fileName);
#endif

	throw SQLEXCEPTION (FEATURE_NOT_YET_IMPLEMENTED, "feature not yet implemented at line %d in file %s\n", line, fileName);
}
