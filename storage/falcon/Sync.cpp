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

// Sync.cpp: implementation of the Sync class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "Sync.h"
#include "SynchronizationObject.h"
#include "Synchronize.h"
#include "Log.h"

#ifndef ASSERT
#define ASSERT(bool)
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Sync::Sync(SynchronizationObject *obj, const char *fromWhere)
{
	ASSERT (obj);
	state = None;
	syncObject = obj;
	where = fromWhere;
	prior = NULL;
	marked = NULL;
}

Sync::~Sync()
{
	if (marked)
		Log::log("Sync::~Sync: %s\n", marked);
		
	if (syncObject && state != None)
		syncObject->unlock(this, state);
}

void Sync::lock(LockType type)
{
	ASSERT(state == None);
	request = type;
	syncObject->lock(this, type, 0);
	state = type;
}

void Sync::lock(LockType type, int timeout)
{
	ASSERT(state == None);
	request = type;
	syncObject->lock(this, type, timeout);
	state = type;
}

/***
void Sync::lock(LockType type, const char *fromWhere)
{
	where = fromWhere;
	lock(type);
}
***/
void Sync::unlock()
{
	ASSERT (state != None);
	syncObject->unlock(this, state);
	state = None;
}

void Sync::setObject(SynchronizationObject * obj)
{
	if (syncObject && state != None)
		syncObject->unlock(this, state);

	state = None;
	syncObject = obj;
}

/***
void Sync::print(int level)
{
	LOG_DEBUG ("%*s%s (%x) state %d (%d)\n", level * 2, "", where, this, state, request);

	if (syncObject)
		syncObject->print(level + 1);

}
***/

void Sync::findLocks(LinkedList &threads, LinkedList &syncObjects)
{
	if (syncObject)
		syncObject->findLocks(threads, syncObjects);
}

void Sync::print(const char *label)
{
	LOG_DEBUG ("%s %s state %d (%d) syncObject %p\n", 
			   label, where, state, request, syncObject);
}

void Sync::mark(const char* text)
{
	marked = text;
	
	Log::debug("Sync::mark %s\n", marked);
}
