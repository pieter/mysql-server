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

// SequenceManager.cpp: implementation of the SequenceManager class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "SequenceManager.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Sequence.h"
#include "Sync.h"
#include "SQLError.h"

//#include "MemMgr.h"						// debugging only

#define HASH(address,size)				(int)(((UIPTR) address >> 2) % size)

static const char *ddl [] = {
	"create table system.sequences ("
		"schema varchar (128) not null,\n"
		"sequenceName varchar (128) not null,\n"
		"id int,\n"
		"primary key (schema, sequenceName))",
	"grant select on system.sequences to public",
	NULL
	};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SequenceManager::SequenceManager(Database *db)
{
	database = db;
	memset (sequences, 0, sizeof (sequences));
}

SequenceManager::~SequenceManager()
{
	for (int n = 0; n < SEQUENCE_HASH_SIZE; ++n)
		for (Sequence *sequence; (sequence = sequences [n]);)
			{
			sequences [n] = sequence->collision;
			delete sequence;
			}
}

void SequenceManager::initialize()
{
	if (!database->findTable ("SYSTEM", "SEQUENCES"))
		for (const char **p = ddl; *p; ++p)
			database->execute (*p);

	Sync sync (&database->syncSysConnection, "SequenceManager::initialize");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"select schema, sequenceName, id from system.sequences");
	ResultSet *resultSet = statement->executeQuery();
	
	while (resultSet->next())
		{
		const char *schema = resultSet->getSymbol (1);
		Sequence *sequence = new Sequence (database, schema, resultSet->getSymbol (2), resultSet->getInt (3));
		int slot = HASH (sequence->name, SEQUENCE_HASH_SIZE);
		ASSERT (slot >= 0 && slot < SEQUENCE_HASH_SIZE);
		sequence->collision = sequences [slot];
		sequences [slot] = sequence;
		}

	resultSet->close();
	statement->close();
}

Sequence* SequenceManager::findSequence(const char *schema, const char *name)
{
	if (!schema)
		return NULL;

	Sync sync (&syncObject, "SequenceManager::findSequence");
	sync.lock (Shared);
	ASSERT (database->isSymbol (schema));
	ASSERT (database->isSymbol (name));

	int slot = HASH (name, SEQUENCE_HASH_SIZE);

	for (Sequence *sequence = sequences [slot]; sequence; sequence = sequence->collision)
		if (sequence->name == name && sequence->schemaName == schema)
			return sequence;

	return NULL;
}

Sequence* SequenceManager::createSequence(const char *schema, const char *name, int64 initialValue)
{
	int id = database->createSequence (initialValue);
	Sequence *sequence = new Sequence (database, schema, name, id);
	Sync sync (&database->syncSysConnection, "SequenceManager::createSequence");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"insert into system.sequences (schema,sequenceName,id) values (?,?,?)");
	statement->setString (1, sequence->schemaName);
	statement->setString (2, sequence->name);
	statement->setLong(3, sequence->id);
	statement->executeUpdate();
	sync.unlock();
	statement->close();
	database->commitSystemTransaction();

	sync.setObject (&syncObject);
	sync.lock (Exclusive);
	int slot = HASH (sequence->name, SEQUENCE_HASH_SIZE);
	ASSERT (slot >= 0 && slot < SEQUENCE_HASH_SIZE);
	sequence->collision = sequences [slot];
	sequences [slot] = sequence;

	return sequence;
}

Sequence* SequenceManager::recreateSequence(Sequence *oldSequence)
{
	const char *schemaName = database->getSymbol(oldSequence->schemaName);
	const char *sequenceName = database->getSymbol(oldSequence->name);
	
	deleteSequence(schemaName, sequenceName);
	return createSequence(schemaName, sequenceName, 0);
}

void SequenceManager::deleteSequence(const char *schema, const char *name)
{
	Sync sync (&database->syncSysConnection, "SequenceManager::deleteSequence");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"delete from system.sequences where schema=? and sequenceName=?");
	statement->setString (1, schema);
	statement->setString (2, name);
	statement->executeUpdate();
	statement->close();
	sync.unlock();
	database->commitSystemTransaction();

	int slot = HASH (name, SEQUENCE_HASH_SIZE);
	sync.setObject (&syncObject);
	sync.lock (Exclusive);
	
	for (Sequence *sequence, **ptr = sequences + slot; (sequence = *ptr); ptr = &sequence->collision)
		if (sequence->schemaName == schema && sequence->name == name)
			{
			*ptr = sequence->collision;
			delete sequence;
			break;
			}
}

Sequence* SequenceManager::getSequence(const char *schema, const char *name)
{
	Sequence *sequence = findSequence (schema, name);

	if (!sequence)
		throw SQLEXCEPTION (DDL_ERROR, "can't find sequence %s.%s", schema, name);

	return sequence;
}

void SequenceManager::renameSequence(Sequence* sequence, const char* newName)
{
	Sync sync (&syncObject, "SequenceManager::renameSequence");
	sync.lock (Exclusive);
	int slot = HASH (sequence->name, SEQUENCE_HASH_SIZE);

	for (Sequence **ptr = sequences + slot; *ptr; ptr = &(*ptr)->collision)
		if (*ptr == sequence)
			{
			*ptr = sequence->collision;
			break;
			}
	
	sync.unlock();		

	PreparedStatement *statement = database->prepareStatement (
		"update system.sequences set sequenceName=? where schema=? and sequenceName=?");
	statement->setString (1, newName);
	statement->setString (2, sequence->schemaName);
	statement->setString (3, sequence->name);
	statement->executeUpdate();
	statement->close();

	sync.lock (Exclusive);
	sequence->name = database->getSymbol(newName);
	slot = HASH (sequence->name, SEQUENCE_HASH_SIZE);
	ASSERT (slot >= 0 && slot < SEQUENCE_HASH_SIZE);
	sequence->collision = sequences [slot];
	sequences [slot] = sequence;
}
