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

// AppEvent.cpp: implementation of the AppEvent class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "AppEvent.h"
#include "Database.h"
#include "Application.h"
#include "User.h"
#include "Scheduler.h"
#include "Connection.h"
#include "SessionManager.h"
#include "SQLException.h"
#include "Log.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

AppEvent::AppEvent(Database *db, Application *app, const char *event, User *usr, const char *schedule) : Schedule (schedule)
{
	database = db;
	name = event;
	application = app;
	user = usr;
	//database->scheduler->addEvent (this);
}

AppEvent::~AppEvent()
{

}

void AppEvent::execute(Scheduler *scheduler)
{
	Connection *connection = NULL;

	try
		{
		connection = new Connection(database, user);
		connection->setLicenseNotRequired (true);
		application->pushNameSpace (connection);
		database->sessionManager->scheduled (connection, application, name);
		getNextEvent();
		database->scheduler->addEvent (this);
		connection->release();
		}
	catch (SQLException& exception)
		{
		if (connection)
			connection->release();
		Log::debug ("Event %s.%d failed with %s\n",
					(const char*) application->name,
					(const char*) name,
					(const char*) exception.getText());
		}
}
