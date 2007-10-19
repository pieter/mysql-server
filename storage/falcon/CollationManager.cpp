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

// CollationManager.cpp: implementation of the CollationManager class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "CollationManager.h"
#include "CollationCaseless.h"
#include "CollationUnknown.h"
#include "Field.h"
#include "SQLError.h"
#include "Sync.h"

static CollationCaseless collationCaseless;
static CollationManager collationManager;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CollationManager::CollationManager()
{
	memset(hashTable, 0, sizeof(hashTable));
	add(&collationCaseless);
}

CollationManager::~CollationManager()
{

}

Collation* CollationManager::getCollation(const char *name)
{
	if (!name || !name[0])
		return NULL;

	if (strcasecmp (name, "CASE_SENSITIVE") == 0)
		return NULL;

	Collation *collation = collationManager.findCollation(name);

	if (!collation)
		//addCollation(new CollationUnknown(&collationManager, name));
		return new CollationUnknown(&collationManager, name);
	
	return collation;
}

Collation* CollationManager::findCollation(const char* collationName)
{
	return collationManager.find(collationName);
}

void CollationManager::addCollation(Collation* collation)
{
	Sync sync(&collationManager.syncObject, "CollationManager::addCollation");
	sync.lock(Exclusive);
	collationManager.add(collation);
}

void CollationManager::add(Collation* collation)
{
	int slot = JString::hash(collation->getName(), COLLATION_HASH_SIZE);
	collation->collision = hashTable[slot];
	hashTable[slot] = collation;
}

Collation* CollationManager::find(const char* collationName)
{
	Sync sync(&syncObject, "CollationManager::addCollation");
	sync.lock(Shared);
	int slot = JString::hash(collationName, COLLATION_HASH_SIZE);
	
	for (Collation *collation = hashTable[slot]; collation; collation = collation->collision)
		if (strcmp (collation->getName(), collationName) == 0)
			{
			collation->addRef();
			
			return collation;
			}
	
	return NULL;
}

void CollationManager::flush(void)
{
	Sync sync(&syncObject, "CollationManager::flush");
	sync.lock(Exclusive);
	
	for (int n = 0; n < COLLATION_HASH_SIZE; ++n)
		for (Collation *collation; (collation = hashTable[n]);)
			{
			hashTable[n] = collation->collision;
			collation->release();
			}
}
