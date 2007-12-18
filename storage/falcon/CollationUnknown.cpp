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

#include "Engine.h"
#include "CollationUnknown.h"
#include "CollationManager.h"
#include "SQLError.h"

CollationUnknown::CollationUnknown(CollationManager *collationManager, const char *collationName)
{
	name = collationName;
	manager = collationManager;
	collation = NULL;
}

CollationUnknown::~CollationUnknown(void)
{
	if (collation)
		collation->release();
}

int CollationUnknown::compare(Value *value1, Value *value2)
{
	if (!collation)
		getCollation();
	
	return collation->compare(value1, value2);
}

int CollationUnknown::makeKey(Value *value, IndexKey *key, int partialKey, int maxKeyLength)
{
	if (!collation)
		getCollation();
	
	return collation->makeKey(value, key, partialKey, maxKeyLength);
}

const char* CollationUnknown::getName()
{
	return name;
}

bool CollationUnknown::starting(const char *string1, const char *string2)
{
	if (!collation)
		getCollation();
	
	return collation->starting(string1, string2);
}

bool CollationUnknown::like(const char *string, const char *pattern)
{
	if (!collation)
		getCollation();
	
	return collation->like(string, pattern);
}

void CollationUnknown::getCollation(void)
{
	if (!collation)
		{
		Collation *col = manager->find(name);
		
		if (!col || col == this)
			throw SQLError (DDL_ERROR, "unknown collation type \"%s\"", (const char*) name);
		
		collation = col;
		}
}

char CollationUnknown::getPadChar(void)
{
	if (!collation)
		getCollation();
	
	return collation->getPadChar();
}

int CollationUnknown::truncate(Value *value, int partialLength)
{
	if (!collation)
		getCollation();
	
	return collation->truncate(value, partialLength);
}
