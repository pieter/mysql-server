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

// ForeignKey.cpp: implementation of the ForeignKey class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "ForeignKey.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Connection.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "Transaction.h"
#include "Table.h"
#include "Field.h"
#include "Log.h"
#include "Sync.h"
#include "SQLError.h"
#include "Value.h"
#include "Record.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ForeignKey::ForeignKey()
{
	numberFields = 0;
	primaryTable = foreignTable = NULL;
	primaryTableId = foreignTableId = -1;
	primaryFields = foreignFields = NULL;
	primaryFieldIds = foreignFieldIds = NULL;
	deleteRule = importedKeyNoAction;
}

ForeignKey::ForeignKey(int cnt, Table *primary, Table *foreign)
{
	numberFields = cnt;
	primaryTable = primary;
	foreignTable = foreign;
	primaryTableId = foreignTableId = -1;
	primaryFields = new Field* [numberFields];
	foreignFields = new Field* [numberFields];
	primaryFieldIds = foreignFieldIds = NULL;
	deleteRule = importedKeyNoAction;
}

ForeignKey::ForeignKey(ForeignKey * key)
{
	numberFields = key->numberFields;
	primaryTable = key->primaryTable;
	foreignTable = key->foreignTable;
	primaryFieldIds = foreignFieldIds = NULL;
	deleteRule = key->deleteRule;

	primaryFields = new Field* [numberFields];
	foreignFields = new Field* [numberFields];

	for (int n = 0; n < numberFields; ++n)
		{
		primaryFields [n] = key->primaryFields [n];
		foreignFields [n] = key->foreignFields [n];
		}
}

ForeignKey::~ForeignKey()
{
	/***
	if (primaryTable)
		ASSERT (!primaryTable->foreignKeyMember (this));

	if (foreignTable)
		ASSERT (!foreignTable->foreignKeyMember (this));
	***/

	if (primaryFields)
		delete [] primaryFields;

	if (foreignFields)
		delete [] foreignFields;

	if (primaryFieldIds)
		delete [] primaryFieldIds;

	if (foreignFieldIds)
		delete [] foreignFieldIds;
}

void ForeignKey::save(Database * database)
{
	Sync sync (&database->syncSysConnection, "ForeignKey::save");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
			"replace into ForeignKeys\
				(primaryTableId, primaryFieldId, foreignTableId, foreignFieldId,\
				 numberKeys, position,deleteRule)\
				 values (?,?,?,?,?,?,?)");
	statement->setInt (1, primaryTable->tableId);
	statement->setInt (3, foreignTable->tableId);
	statement->setInt (5, numberFields);
	statement->setInt (7, deleteRule);
	
	for (int n = 0; n < numberFields; ++n)
		{
		statement->setInt (2, primaryFields [n]->id);	
		statement->setInt (4, foreignFields [n]->id);
		statement->setInt (6, n);
		statement->executeUpdate();
		}
	
	statement->close();	
}

void ForeignKey::loadRow(Database * database, ResultSet * resultSet)
{
	if (numberFields == 0)
		{
		numberFields = resultSet->getInt (5);
		primaryFieldIds = new int [numberFields];
		foreignFieldIds = new int [numberFields];
		primaryTableId = resultSet->getInt (1);
		foreignTableId = resultSet->getInt (3);
		}

	int position = resultSet->getInt (6);
	ASSERT (position < numberFields);
	primaryFieldIds [position] = resultSet->getInt (2);
	foreignFieldIds [position] = resultSet->getInt (4);
	deleteRule = resultSet->getInt (7);

	if (resultSet->wasNull())
		deleteRule = importedKeyNoAction;
}

void ForeignKey::loadPrimaryKeys(Database *database, Table *table)
{
	Sync sync (&database->syncSysConnection, "ForeignKey::loadPrimaryKeys");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
			"select\
				primaryTableId, primaryFieldId, foreignTableId, foreignFieldId,\
				numberKeys, position,deleteRule\
				from ForeignKeys\
				where foreignTableId = ? order by primaryTableId, position");
	statement->setInt (1, table->tableId);
	ForeignKey *key = NULL;
	int primaryTableId = -1;
	int lastPosition = -1;
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		int tableId = resultSet->getInt (1);
		int position = resultSet->getInt (6);
		if (!key || tableId != primaryTableId || position <= lastPosition)
			{
			if (key)
				table->addForeignKey (key);
			key = new ForeignKey;
			primaryTableId = tableId;
			}
		key->loadRow (database, resultSet);
		lastPosition = position;
		}

	if (key)
		table->addForeignKey (key);

	resultSet->close();
	statement->close();
}

void ForeignKey::loadForeignKeys(Database *database, Table *table)
{
	Sync sync (&database->syncSysConnection, "ForeignKey::loadForeignKeys");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
			"select\
				primaryTableId, primaryFieldId, foreignTableId, foreignFieldId,\
				 numberKeys, position,deleteRule\
				 from ForeignKeys\
				 where primaryTableId = ? order by foreignTableId, position");
	statement->setInt (1, table->tableId);
	ForeignKey *key = NULL;
	int foreignTableId = -1;
	int lastPosition = -1;
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		int tableId = resultSet->getInt (3);
		int position = resultSet->getInt (6);
		if (!key || tableId != foreignTableId || position <= lastPosition)
			{
			if (key)
				table->addForeignKey (key);
			key = new ForeignKey;
			foreignTableId = tableId;
			}
		key->loadRow (database, resultSet);
		lastPosition = position;
		}

	if (key)
		table->addForeignKey (key);

	resultSet->close();
	statement->close();
}

void ForeignKey::bind(Database * database)
{
	/***
	if (primaryTable && foreignTable)
		return;
	***/

	if (primaryFields)
		return;

	ASSERT (primaryFieldIds);

	if (!primaryTable && !(primaryTable = database->getTable (primaryTableId)))
		throw SQLError (RUNTIME_ERROR, "can't find primary table for foreign key");

	if (!foreignTable && !(foreignTable = database->getTable (foreignTableId)))
		throw SQLError (RUNTIME_ERROR, "can't find foreign table for foreign key");

	/***
	if (primaryFields)
		delete [] primaryFields;
	***/

	primaryFields = new Field* [numberFields];
	foreignFields = new Field* [numberFields];

	for (int n = 0; n < numberFields; ++n)
		{
		primaryFields [n] = primaryTable->findField (primaryFieldIds [n]);
		foreignFields [n] = foreignTable->findField (foreignFieldIds [n]);
		}

	delete [] primaryFieldIds;
	delete [] foreignFieldIds;
	primaryFieldIds = foreignFieldIds = NULL;
}


bool ForeignKey::isMember(Field * field, bool foreign)
{
	if (foreign)
		{
		for (int n = 0; n < numberFields; ++n)
			if (foreignFields [n] == field)
				return true;
		}
	else
		for (int n = 0; n < numberFields; ++n)
			if (primaryFields [n] == field)
				return true;

	return false;
}

bool ForeignKey::matches(ForeignKey * key, Database *database)
{
	if (numberFields != key->numberFields)
		return false;

	bind(database);

	/*** handle at a different level
	if (deleteRule != key->deleteRule)
		return false;
	***/

	if (foreignTable != key->foreignTable ||
	    primaryTable != key->primaryTable)
		return false;

	for (int n = 0; n < numberFields; ++n)
		{
		if (primaryFields [n] != key->primaryFields [n])
			return false;
		if (foreignFields [n] != key->foreignFields [n])
			return false;
		}

	return true;		
}

void ForeignKey::drop()
{
	Database *database = primaryTable->database;
	Sync sync (&database->syncSysConnection, "ForeignKey::drop");
	sync.lock (Shared);
	PreparedStatement *statement;

	if (foreignTable)
		{
		statement = database->prepareStatement (
			"delete from ForeignKeys where primaryTableId=? and foreignTableId=? and foreignFieldId=?");
		statement->setInt (1, primaryTable->tableId);
		statement->setInt (2, foreignTable->tableId);
		for (int n = 0; n < numberFields; ++n)
			if (foreignFields[n])
				{
				statement->setInt (3, foreignFields[n]->id);
				if (statement->executeUpdate() == 0)
					Log::log ("ForeignKey::drop: foreign key segment misplaced\n");
				}
		}
	else
		{
		statement = database->prepareStatement (
			"delete from ForeignKeys where primaryTableId=? and foreignTableId=?");
		statement->setInt (1, primaryTable->tableId);
		statement->setInt (2, foreignTableId);
		statement->executeUpdate();
		}

	statement->close();
}

void ForeignKey::create()
{
	//foreignTable->addIndex (this);
	foreignTable->addForeignKey (this);

	if (foreignTable != primaryTable)
		primaryTable->addForeignKey (new ForeignKey (this));
}

void ForeignKey::deleteForeignKey()
{
	// Get rid of foreign key in system tables

	drop();

	// Get rid of peer, if loaded

	if (primaryTable)
		{
		ForeignKey *key = primaryTable->dropForeignKey (this);
		if (key && key != this)
			delete key;
		}
				
	if (foreignTable)
		{
		ForeignKey *key = foreignTable->dropForeignKey (this);
		if (key && key != this)
			delete key;
		}
				
	delete this;
}

void ForeignKey::bindTable(Table *table)
{
	if (!primaryTable && primaryTableId == table->tableId)
		primaryTable = table;
	else if (!foreignTable && foreignTableId == table->tableId)
		foreignTable = table;
}

void ForeignKey::setDeleteRule(int rule)
{
	deleteRule = rule;
}

void ForeignKey::cascadeDelete(Transaction *transaction, Record *oldRecord)
{
	Connection *connection = transaction->connection;
	PreparedStatement *statement = connection->findRegisteredStatement (statementDeleteCascade, this);

	if (!statement)
		{
		if (deleteStatement.IsEmpty())
			{
			deleteStatement.Format ("delete from %s.%s where ", foreignTable->schemaName, foreignTable->name);
			const char *sep = "";
			for (int n = 0; n < numberFields; ++n)
				{
				Field *field = foreignFields [n];
				deleteStatement += sep;
				deleteStatement += field->name;
				deleteStatement += "=? ";
				sep = " and ";
				}
			}
		statement = connection->prepareStatement (deleteStatement);
		connection->registerStatement (statementDeleteCascade, this, statement);
		}

	bool active = statement->active;

	if (active)
		statement = connection->prepareStatement (deleteStatement);

	for (int n = 0; n < numberFields; ++n)
		{
		Field *field = primaryFields [n];
		Value value;
		oldRecord->getValue (field->id, &value);
		statement->setValue (n + 1, &value);
		}

	try
		{
		statement->executeUpdate();
		statement->active = false;
		if (active)
			statement->release();
		else
			statement->active = false;
		}
	catch (...)
		{
		if (active)
			statement->release();
		else
			statement->active = false;
		}
}
