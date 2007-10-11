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

// Scavenger.cpp: implementation of the Scavenger class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "Scavenger.h"
#include "Database.h"
#include "Scheduler.h"

#ifndef STORAGE_ENGINE
#include "JavaVM.h"
#include "Java.h"
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Scavenger::Scavenger (Database *db, ScavengeType scavengeType, const char *schedule) : Schedule (schedule)
{
	database = db;
	type = scavengeType;
}

Scavenger::~Scavenger()
{

}


void Scavenger::scavenge()
{
	switch (type)
		{
#ifndef STORAGE_ENGINE
		case scvJava:
			database->java->garbageCollect();
			break;
#endif

		case scvRecords:
			database->scavenge();
			break;
		
		default:
			break;
		}
}

void Scavenger::execute(Scheduler * scheduler)
{
	scavenge();
	getNextEvent();
	scheduler->addEvent (this);
}
