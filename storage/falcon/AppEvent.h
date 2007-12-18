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

// AppEvent.h: interface for the AppEvent class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_APPEVENT_H__F04399E1_4B97_11D5_B8C9_00E0180AC49E__INCLUDED_)
#define AFX_APPEVENT_H__F04399E1_4B97_11D5_B8C9_00E0180AC49E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Schedule.h"

class Database;
class Application;
class User;

class AppEvent : public Schedule
{
public:
	virtual void execute(Scheduler * scheduler);
	AppEvent(Database *db, Application *app, const char *event, User *user, const char *schedule);

protected:
	virtual ~AppEvent();

public:
	AppEvent	*nextEvent;
	Database	*database;
	Application	*application;
	User		*user;
	JString		name;
};

#endif // !defined(AFX_APPEVENT_H__F04399E1_4B97_11D5_B8C9_00E0180AC49E__INCLUDED_)
