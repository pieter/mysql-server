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

// FilterSetManager.cpp: implementation of the FilterSetManager class.
//
//////////////////////////////////////////////////////////////////////

#define HASH(address,size)				(int)(((UIPTR) address >> 2) % size)

#include <memory.h>
#include "Engine.h"
#include "FilterSetManager.h"
#include "FilterSet.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Sync.h"

static const char *ddl [] = {
	"create table filtersets ("
		"filtersetname varchar (128) not null,"
		"schema varchar (128) not null,"
		"text clob,"
		"primary key (filtersetname, schema))",
	"grant select on system.filtersets to public",
	0 };

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FilterSetManager::FilterSetManager(Database *db)
{
	database = db;
	memset (filterSets, 0, sizeof (filterSets));
}

FilterSetManager::~FilterSetManager()
{
	for (int n = 0; n < FILTERSETS_HASH_SIZE; ++n)
		for (FilterSet *filterSet; (filterSet = filterSets [n]);)
			{
			filterSets [n] = filterSet->collision;
			filterSet->release();
			}
}

void FilterSetManager::addFilterset(FilterSet *filterSet)
{
	int slot = HASH (filterSet->name, FILTERSETS_HASH_SIZE);
	filterSet->collision = filterSets [slot];
	filterSets [slot] = filterSet;	
}

FilterSet* FilterSetManager::findFilterSet(const char *schema, const char *name)
{
	if (!schema)
		return NULL;

	FilterSet *filterSet;
	int slot = HASH (name, FILTERSETS_HASH_SIZE);

	for (filterSet = filterSets [slot]; filterSet; filterSet = filterSet->collision)
		if (filterSet->name == name &&
		    filterSet->schema == schema)
			return filterSet;

	Sync sync (&database->syncSysConnection, "FilterSetManager::findFilterSet");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"select text from system.filtersets where filtersetname=? and schema=?");
	statement->setString (1, name);
	statement->setString (2, schema);
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		filterSet = new FilterSet (database, schema, name);
		filterSet->setText (resultSet->getString (1));
		addFilterset (filterSet);
		}

	resultSet->close();
	statement->close();

	return filterSet;
}

void FilterSetManager::initialize()
{
	if (!database->findTable ("SYSTEM", "FILTERSETS"))
		for (const char **p = ddl; *p; ++p)
			database->execute (*p);
}

void FilterSetManager::deleteFilterset(FilterSet *filterSet)
{
	int slot = HASH (filterSet->name, FILTERSETS_HASH_SIZE);

	for (FilterSet **ptr = filterSets + slot, *node; (node = *ptr); ptr = &node->collision)
		if ( (*ptr == filterSet) )
			{
			*ptr = node->collision;
			break;
			}

	Sync sync (&database->syncSysConnection, "FilterSetManager::deleteFilterset");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"delete from system.filtersets where filtersetname=? and schema=?");
	statement->setString (1, filterSet->name);
	statement->setString (2, filterSet->schema);
	statement->executeUpdate();
	statement->close();

	filterSet->release();
}
