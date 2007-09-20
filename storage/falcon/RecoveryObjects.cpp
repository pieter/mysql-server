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

// RecoveryObjects.cpp: implementation of the RecoveryObjects class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "RecoveryObjects.h"
#include "RecoveryPage.h"
#include "SerialLog.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RecoveryObjects::RecoveryObjects(SerialLog *log)
{
	serialLog = log;
	memset(recoveryObjects, 0, sizeof(recoveryObjects));
}

RecoveryObjects::~RecoveryObjects()
{
	clear();
}

void RecoveryObjects::clear()
{
	for (int n = 0; n < RPG_HASH_SIZE; ++n)
		for (RecoveryPage *object; (object = recoveryObjects[n]);)
			{
			recoveryObjects[n] = object->collision;
			delete object;
			}
}

bool RecoveryObjects::bumpIncarnation(int objectNumber, int tableSpaceId, int state, bool pass1)
{
	RecoveryPage *object = getRecoveryObject(objectNumber, tableSpaceId);

	if (object->state != state)
		{
		if (pass1)
			++object->pass1Count;
		else
			++object->currentCount;
		
		object->state = state;
		}

	if (pass1)
		return object->pass1Count == 1;

	return object->pass1Count == object->currentCount;
}

void RecoveryObjects::reset()
{
	for (int n = 0; n < RPG_HASH_SIZE; ++n)
		for (RecoveryPage *object = recoveryObjects[n]; object; object = object->collision)
			{
			object->currentCount = 0;
			object->state = 0;
			}
}

bool RecoveryObjects::isObjectActive(int objectNumber, int tableSpaceId)
{
	RecoveryPage *object = findRecoveryObject(objectNumber, tableSpaceId);

	if (!object)
		return true;

	return object->pass1Count == object->currentCount;
}

RecoveryPage* RecoveryObjects::findRecoveryObject(int objectNumber, int tableSpaceId)
{
	int slot = objectNumber % RPG_HASH_SIZE;

	for (RecoveryPage *object = recoveryObjects[slot]; object; object = object->collision)
		if (object->objectNumber == objectNumber && object->tableSpaceId == tableSpaceId)
			return object;

	return NULL;
}

void RecoveryObjects::setActive(int objectNumber, int tableSpaceId)
{
	RecoveryPage *object = findRecoveryObject(objectNumber, tableSpaceId);
	
	if (!object)
		return;
	
	object->pass1Count = 0;
	object->currentCount = 0;
}

void RecoveryObjects::setInactive(int objectNumber, int tableSpaceId)
{
	RecoveryPage *object = getRecoveryObject(objectNumber, tableSpaceId);
	object->pass1Count = 0;
	object->currentCount = 1;
}

RecoveryPage* RecoveryObjects::getRecoveryObject(int objectNumber, int tableSpaceId)
{
	int slot = objectNumber % RPG_HASH_SIZE;
	RecoveryPage *object;

	for (object = recoveryObjects[slot]; object; object = object->collision)
		if (object->objectNumber == objectNumber && object->tableSpaceId == tableSpaceId)
			return object;

	object = new RecoveryPage(objectNumber, tableSpaceId);
	object->collision = recoveryObjects[slot];
	recoveryObjects[slot] = object;
	
	return object;
}

void RecoveryObjects::deleteObject(int objectNumber, int tableSpaceId)
{
	int slot = objectNumber % RPG_HASH_SIZE;
	
	for (RecoveryPage **ptr = recoveryObjects + slot, *object; (object = *ptr); ptr = &object->collision)
		if (object->objectNumber == objectNumber && object->tableSpaceId == tableSpaceId)
			{
			*ptr = object->collision;
			delete object;
			
			return;
			}

}

int RecoveryObjects::getCurrentState(int objectNumber, int tableSpaceId)
{
	RecoveryPage *object = findRecoveryObject(objectNumber, tableSpaceId);
	
	return (object) ? object->state : objUnknown;
}
