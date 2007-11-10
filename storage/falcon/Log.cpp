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

// Log.cpp: implementation of the Log class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <memory.h>

#ifdef _WIN32
#include <windows.h>
#undef ERROR
#undef ASSERT
#endif

#include "Engine.h"
#include "Log.h"
#include "Thread.h"
#include "SQLError.h"
#include "SymbolManager.h"
#include "Sync.h"

#ifdef _PTHREADS
#include <pthread.h>
#endif

#ifdef _WIN32
#define ENTER_CRITICAL_SECTION	EnterCriticalSection (&criticalSection)
#define LEAVE_CRITICAL_SECTION	LeaveCriticalSection (&criticalSection)
static CRITICAL_SECTION criticalSection;
#endif

#ifdef _PTHREADS
#define ENTER_CRITICAL_SECTION	pthread_mutex_lock (&mutex)
#define LEAVE_CRITICAL_SECTION	pthread_mutex_unlock (&mutex)
static pthread_mutex_t	mutex;
#endif


#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif

#ifndef ASSERT
#define ASSERT
#endif

static LogListener		*listeners;
static SymbolManager	*symbols;
static int				activeMask;
volatile int			Log::exclusive;
volatile Thread*		Log::exclusiveThread;

static bool		initialize();
static bool		initialized = initialize ();

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

bool initialize()
{
#ifdef _WIN32
	InitializeCriticalSection (&criticalSection);
#endif

#ifdef _PTHREADS
	//int ret = 
	pthread_mutex_init (&mutex, NULL);
#endif

	return false;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


void Log::log(const char *txt, ...)
{
	if (!(activeMask & LogLog))
		return;
		
	va_list		args;
	va_start	(args, txt);
	log (LogLog, txt, args);
}

void Log::logBreak(const char *txt, ...)
{
	if (!(activeMask & LogLog))
		return;

	va_list		args;
	va_start	(args, txt);
	log (LogLog, txt, args);
}

void Log::debug(const char *txt, ...)
{
	if (!(activeMask & LogDebug))
		return;

	va_list		args;
	va_start	(args, txt);
	log (LogDebug, txt, args);
}

void Log::debugBreak(const char *txt, ...)
{
	if (!(activeMask & LogDebug))
		return;

	va_list		args;
	va_start	(args, txt);
	log (LogDebug, txt, args);
}

void Log::log(int mask, const char *txt, ...)
{
	if (!(activeMask & mask))
		return;

	va_list		args;
	va_start	(args, txt);
	log (mask, txt, args);
}

void Log::print(int mask, const char *text, void *arg)
{
	printf ("%s", text);
}

void Log::addListener(int mask, Listener *fn, void *arg)
{
	ENTER_CRITICAL_SECTION;
	LogListener *listener;
	activeMask |= mask;
	
	for (listener = listeners; listener; listener = listener->next)
		if (listener->listener == fn && listener->arg == arg)
			{
			listener->mask = mask;
			break;
			}

	if (!listener)
		{
		listener = new LogListener;
		listener->listener = fn;
		listener->mask = mask;
		listener->arg = arg;
		listener->next = listeners;
		listeners = listener;
		}

	LEAVE_CRITICAL_SECTION;
}

void Log::deleteListener(Listener *fn, void *arg)
{
	ENTER_CRITICAL_SECTION;

	for (LogListener **ptr = &listeners, *listener; (listener = *ptr); ptr = &listener->next)
		if (listener->listener == fn && listener->arg == arg)
			{
			*ptr = listener->next;
			delete listener;
			break;
			}

	activeMask = 0;
	
	for (LogListener *lissn = listeners; lissn; lissn = lissn->next)
		activeMask |= listener->mask;
		
	LEAVE_CRITICAL_SECTION;
}

int Log::init()
{
	//addListener (-1, &print, NULL);

	return 0;
}

void Log::log(int mask, const char *text, va_list args)
{
	if (!(activeMask & mask))
		return;

	char		temp [1024];

	if (vsnprintf (temp, sizeof (temp) - 1, text, args) < 0)
		temp [sizeof (temp) - 1] = 0;

	logMessage (mask, temp);
}

void Log::logMessage(int mask, const char *text)
{
	if (!(activeMask & mask))
		return;

	char temp [1024], *scrubbed = temp;
	bool inCS = false;

	if ((mask & LogScrub) && symbols)
		{
		int l = (int) strlen (text) + 30;
		
		if (l > (int) sizeof (temp))
			scrubbed = new char [l];
			
		char *q = scrubbed;
		const char *parameter = NULL;
		
		for (const char *p = text;;)
			{
			char c = *p++;
			*q++ = c;
			
			if (c == '&' || c == ' ')
				parameter = p;
			else if (parameter && (c == '='))
				{
				char word [128];
				l = (int) (p - parameter) - 1;
				
				if (l < (int) sizeof (word) - 1)
					{
					memcpy (word, parameter, l);
					word [l] = 0;
					
					if (symbols->findString (word))
						{
						*q++ = '*';
						*q++ = '*';
						*q++ = '*';
						while (*p && *p != '&')
							++p;
						}
					}
				parameter = NULL;
				}
				
			if (c == 0)
				break;
			}
		*q = 0;
		text = scrubbed;
		}

	if (!exclusive)
		{
		ENTER_CRITICAL_SECTION;
		inCS = true;
		}

	try
		{
		if (!initialized)
			initialized = true;

		for (LogListener **ptr = &listeners, *listener; (listener = *ptr);)
			{
			if (listener->mask & mask)
				try
					{
					(listener->listener)(mask, text, listener->arg);
					}
				catch (...)
					{
					*ptr = listener->next;
					continue;
					}
					
			ptr = &listener->next;
			}
		}
	catch (SQLException& exception)
		{
		printf ("Log::logMessage failed: %s\n", exception.getText());
		}
	catch (...)
		{
		printf ("Log::logMessage failed for unknown reason\n");
		}
	

	if (inCS)
		LEAVE_CRITICAL_SECTION;
	
	if (scrubbed != temp)
		delete [] scrubbed;
}

void Log::fini()
{
	for (LogListener *listener; (listener = listeners);)
		{
		listeners = listener->next;
		delete listener;
		}

	if (symbols)
		{
		delete symbols;
		symbols = NULL;
		}
}

void Log::scrubWords(const char *words)
{
	Log::log ("Log scrub words: %s\n", words);

	if (!symbols)
		{
		symbols = new SymbolManager;
		symbols->getString ("_fld_PASSWORD");
		}

	char word [128], *q = word;

	for (const char *p = words;;)
		{
		char c = *p++;
		if (c == 0 || c == ',' || c == ' ')
			{
			if (q > word)
				{
				*q = 0;
				symbols->getString (word);
				q = word;
				}
				
			if (!c)
				break;
			}
		else
			*q++ = c;
		}
}

void Log::setExclusive(void)
{
	Thread *thread = Thread::getThread("Log::setExclusive");
	
	if (thread != exclusiveThread)
		{
		ENTER_CRITICAL_SECTION;
		exclusiveThread = thread;
		}
	
	++exclusive;
}

void Log::releaseExclusive(void)
{
	ASSERT(exclusive);
	
	if (--exclusive == 0)
		{
		exclusiveThread = NULL;
		LEAVE_CRITICAL_SECTION;
		}
}

bool Log::isActive(int mask)
{
	return (mask & activeMask) != 0;
}
