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

// Synchronize.h: interface for the Synchronize class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SYNCHRONIZE_H__9E13C6D8_1F3E_11D3_AB74_0000C01D2301__INCLUDED_)
#define AFX_SYNCHRONIZE_H__9E13C6D8_1F3E_11D3_AB74_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifdef _PTHREADS
#include <pthread.h>
#endif

//#define SYNCHRONIZE_FREEZE

#ifdef SYNCHRONIZE_FREEZE
#define DEBUG_FREEZE	if (synchronizeFreeze) Synchronize::freeze();
extern INTERLOCK_TYPE synchronizeFreeze;
#else
#define DEBUG_FREEZE
#endif

#ifdef ENGINE
#define LOG_DEBUG	Log::debug
#define DEBUG_BREAK	Log::debugBreak
#else
#define LOG_DEBUG	printf
#define DEBUG_BREAK	printf
#endif

class Synchronize  
{
public:
	virtual void shutdown();
	virtual void wake();
	virtual bool sleep (int milliseconds);
	virtual void sleep();
	Synchronize();
	virtual ~Synchronize();

	bool			shutdownInProgress;
	bool			sleeping;
	volatile bool	wakeup;
	int64			waitTime;

#ifdef _WIN32
	void	*event;
#endif

#ifdef _PTHREADS
	pthread_cond_t	condition;
	pthread_mutex_t	mutex;
#endif
	static void freeze(void);
	static void freezeSystem(void);
};

#endif // !defined(AFX_SYNCHRONIZE_H__9E13C6D8_1F3E_11D3_AB74_0000C01D2301__INCLUDED_)
