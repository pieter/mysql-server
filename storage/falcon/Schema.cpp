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

// Schema.cpp: implementation of the Schema class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Schema.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Sync.h"
#include "SQLError.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Schema::Schema(Database *db, const char *schemaName)
{
	name = schemaName;
	database = db;
	sequenceInterval = 0;
	systemId = 0;
	refresh();
}

Schema::~Schema()
{

}

void Schema::update()
{
	Sync sync (&database->syncSysConnection, "Schema::update");
	sync.lock (Shared);
	PreparedStatement *statement = database->prepareStatement (
		"replace system.schemas (schema,sequence_interval,system_id) values (?,?,?)");
	int n = 1;
	statement->setString (n++, name);
	statement->setInt (n++, sequenceInterval);
	statement->setInt (n++, systemId);
	statement->executeUpdate();
	sync.unlock();
	database->commitSystemTransaction();
}

void Schema::setInterval(int newInterval)
{
	if (newInterval <= sequenceInterval)
		return;

	sequenceInterval = newInterval;
	update();
}

void Schema::setSystemId(int newId)
{
	if (newId >= sequenceInterval)
		throw SQLError (DDL_ERROR, "schema system id cannot equal or exceed sequence interval\n");

	systemId = newId;
	update();
}

void Schema::refresh()
{
	Sync sync (&database->syncSysConnection, "Schema::refresh");
	sync.lock (Shared);
	PreparedStatement *statement = database->prepareStatement (
		"select sequence_interval, system_id from system.schemas where schema=?");
	statement->setString (1, name);
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		sequenceInterval = resultSet->getInt (1);
		systemId = resultSet->getInt (2);
		}

	resultSet->close();
	statement->close();
}
