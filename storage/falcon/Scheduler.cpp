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

// Scheduler.cpp: implementation of the Scheduler class.
//
//////////////////////////////////////////////////////////////////////

#include <time.h>
#include <stdio.h>
#include "Engine.h"
#include "Scheduler.h"
#include "Schedule.h"
#include "Thread.h"
#include "Threads.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Sync.h"
#include "SQLError.h"
#include "User.h"
#include "Log.h"

#ifndef STORAGE_ENGINE
#include "AppEvent.h"
#include "Application.h"
#endif

static const char *ddl [] =
	{
    "create table Schedule ("
			"application varchar (30) not null,"
			"eventName varchar (128) not null,"
			"userName varchar (128) not null,"
			"schedule varchar (128),"
			"primary key (application,eventName))",
	"grant select on Schedule to public",
	NULL
	};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Scheduler::Scheduler(Database *db)
{
	shutdownInProgress = false;
	database = db;	
	next = NULL;
	events = NULL;
	useCount = 1;
	thread = NULL;
}

Scheduler::~Scheduler()
{
	for (Schedule *scheduleEvent; (scheduleEvent = next);)
		{
		next = scheduleEvent->next;
		scheduleEvent->release();
		}

#ifndef STORAGE_ENGINE
	for (AppEvent *event; event = events;)
		{
		events = event->nextEvent;
		event->release();
		}
#endif
}

void Scheduler::shutdown(bool panic)
{
	shutdownInProgress = true;

	if (thread)
		{
		database->threads->shutdown(thread);
		thread = NULL;
		}
}

void Scheduler::schedule()
{
	addRef();
	Thread *thread = Thread::getThread("Scheduler::schedule");
	ASSERT (thread->activeLocks == 0);
	Sync sync (&syncObject, "Scheduler::schedule");

	for (;;)
		{
		if (shutdownInProgress)
			{
			release();
			return;
			}

		if (database->noSchedule)
			{
			thread->sleep (5 * 60 * 1000);
			continue;
			}

		time_t currentTime = time (NULL);
		sync.lock (Exclusive);

		// Fire anybody ready to be scheduled

		while (next && next->eventTime <= currentTime)
			{
			if (shutdownInProgress)
				{
				release();
				return;
				}
			Schedule *schedule = next;
			next = schedule->next;
			sync.unlock();
			if (!schedule->deleted)
				schedule->execute (this);
			if (thread->activeLocks)
				{
				Log::logBreak ("active lock exitting scheduled process\n");
				thread->print();
				ASSERT (thread->activeLocks == 0);
				}
			schedule->release();
			currentTime = time (NULL);
			sync.lock (Exclusive);
			}
		sync.unlock();
		if (next)
			thread->sleep ((int)(next->eventTime - currentTime) * 1000);
		else
			thread->sleep ();
		}
}

void Scheduler::schedule(void * lpParameter)
{
	Scheduler *scheduler = (Scheduler*) lpParameter;
	scheduler->schedule();
}

void Scheduler::addEvent(Schedule * schedule)
{
	schedule->addRef();
	Sync sync (&syncObject, "Scheduler::addEvent");
	sync.lock (Exclusive);
    Schedule **ptr;

	for (ptr = &next; *ptr && (*ptr)->eventTime < schedule->eventTime;
		 ptr = &(*ptr)->next)
		;

	schedule->next = *ptr;
	*ptr = schedule;

	if (thread && !shutdownInProgress)
		thread->wake();
}

void Scheduler::start()
{
	if (!thread)
		thread = database->threads->start ("Scheduler::start", &Scheduler::schedule, this);

	if (!database->findTable ("SYSTEM", "SCHEDULE"))
		initialize();

#ifndef STORAGE_ENGINE
	Sync sync (&database->syncSysConnection, "Scheduler::start");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"select application, eventName from system.schedule");
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		const char *appName = resultSet->getString (1);
		Application *application = database->getApplication (appName);
		if (!application)
			{
			Log::debug ("No application \"%s\" for schedule event \"%s\"", appName, resultSet->getString(2));
			continue;
			}
		}

	resultSet->close();
	statement->close();
#endif
}

void Scheduler::updateSchedule(const char *appName, const char *eventName, User *user, const char *schedule)
{
	Sync sync (&syncObject, "Scheduler::updateSchedule");
	sync.lock (Exclusive);

	// Ditch old event

#ifndef STORAGE_ENGINE
	AppEvent *event = removeEvent (appName, eventName);

	if (event)
		{
		event->deleted = true;
		event->release();
		}
#endif

	// If no schedule, we're done

	Sync syncConnection (&database->syncSysConnection, "Scheduler::updateSchedule(2)");
	syncConnection.lock (Exclusive);

	if (!schedule || !schedule[0])
		{
		PreparedStatement *statement = database->prepareStatement (
			"delete from system.schedule where application=? and eventName=?");
		statement->setString (1, appName);
		statement->setString (2, eventName);
		statement->executeUpdate();
		statement->close();
		syncConnection.unlock();
		sync.unlock();
		database->commitSystemTransaction();
		return;
		}

	PreparedStatement *statement = database->prepareStatement (
		"replace into system.schedule (application,eventName,userName,schedule) values (?,?,?,?)");
	int n = 1;
	statement->setString (n++, appName);
	statement->setString (n++, eventName);
	statement->setString (n++, user->name);
	statement->setString (n++, schedule);
	statement->executeUpdate();
	statement->close();
	syncConnection.unlock();
	sync.unlock();
	database->commitSystemTransaction();
	sync.lock (Exclusive);

#ifndef STORAGE_ENGINE
	Application *application = database->getApplication (appName);

	if (application)
		{
		event = new AppEvent (database, application, eventName, user, schedule);
		database->scheduler->addEvent (event);
		event->nextEvent = events;
		events = event;
		}
#endif
}

AppEvent* Scheduler::removeEvent(const char *appName, const char *eventName)
{
#ifndef STORAGE_ENGINE
	for (AppEvent **ptr = &events, *event; event = *ptr; ptr = &event->nextEvent)
		if (event->name == eventName && event->application->name == appName)
			{
			*ptr = event->nextEvent;
			for (Schedule **schdPtr = &next; *schdPtr; schdPtr = &(*schdPtr)->next)
				if (*schdPtr == event)
					{
					*schdPtr = event->next;
					break;
					}
			return event;
			}
#endif

	return NULL;
}


void Scheduler::loadEvents(Application *application)
{
	if (!database->findTable ("SYSTEM", "SCHEDULE"))
		initialize();

#ifndef STORAGE_ENGINE
	Sync sync (&database->syncSysConnection, "Scheduler::loadEvents");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"select * from system.schedule where application=?");
	statement->setString (1, application->name);
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		const char *appName = resultSet->getString ("APPLICATION");
		const char *eventName = resultSet->getString ("EVENTNAME");
		const char *userName = resultSet->getString ("USERNAME");
		User *user = database->findUser (userName);
		if (!user)
			{
			Log::debug ("No user \"%s\" for schedule event \"%s.%s\"", userName, appName, eventName);
			continue;
			}
		const char *schedule = resultSet->getString ("SCHEDULE");
		try
			{
			AppEvent *event = new AppEvent (database, application,eventName,user,schedule);
			database->scheduler->addEvent (event);
			event->nextEvent = events;
			events = event;
			}
		catch (SQLException &exception)
			{
			Log::debug ("could load event %s.%s: %s\n", appName, eventName,
					(const char*) exception.getText());
			}
		}

	resultSet->close();
	statement->close();
#endif
}

void Scheduler::initialize()
{
	Statement *statement = database->createStatement();

	for (const char **ptr = ddl; *ptr; ++ptr)
		try
			{
			statement->executeUpdate (*ptr);
			}
		catch (SQLException &exception)
			{
			Log::debug ("Scheduler::initialize: %s\n", exception.getText());
			throw;
			}

	statement->close();
}

void Scheduler::deleteEvents(Application *application)
{
#ifndef STORAGE_ENGINE
	Sync sync (&syncObject, "Scheduler::deleteEvents");
	sync.lock (Exclusive);

	for (AppEvent **ptr = &events, *event; event = *ptr;)
		if (event->application == application)
			{
			*ptr = event->nextEvent;
			for (Schedule **schdPtr = &next; *schdPtr; schdPtr = &(*schdPtr)->next)
				if (*schdPtr == event)
					{
					*schdPtr = event->next;
					event->release();
					break;
					}
			event->release();
			}
		else
			 ptr = &event->nextEvent;
#endif
}

void Scheduler::addRef()
{
	++useCount;
}

void Scheduler::release()
{
	if (--useCount == 0)
		delete this;
}
