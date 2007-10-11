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

// Scheduler.h: interface for the Scheduler class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCHEDULER_H__9E13C6D1_1F3E_11D3_AB74_0000C01D2301__INCLUDED_)
#define AFX_SCHEDULER_H__9E13C6D1_1F3E_11D3_AB74_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SyncObject.h"

class Database;
class Thread;
class Schedule;
class AppEvent;
class User;
//class PreparedStatement;
class Application;

class Scheduler  
{
public:
	void release();
	void addRef();
	void deleteEvents (Application *application);
	void initialize();
	void loadEvents (Application *application);
	AppEvent* removeEvent (const char *appName, const char *eventName);
	void updateSchedule (const char *appName, const char *eventName, User *user, const char *schedule);
	void start();
	void addEvent (Schedule *schedule);
	static void schedule (void * lpParameter);
	void schedule();
	void shutdown (bool panic);
	Scheduler(Database *db);

protected:
	virtual ~Scheduler();

public:
	Database	*database;
	Thread		*thread;
	bool		shutdownInProgress;
	Schedule	*next;
	SyncObject	syncObject;
	AppEvent	*events;
	int			useCount;
};

#endif // !defined(AFX_SCHEDULER_H__9E13C6D1_1F3E_11D3_AB74_0000C01D2301__INCLUDED_)
