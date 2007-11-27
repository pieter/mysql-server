/* Copyright (C) 2007 MySQL AB

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


// TableSpaceManager.cpp: implementation of the TableSpaceManager class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "TableSpaceManager.h"
#include "TableSpace.h"
#include "Sync.h"
#include "SQLError.h"
#include "PStatement.h"
#include "RSet.h"
#include "Database.h"
#include "SequenceManager.h"
#include "Sequence.h"
#include "Stream.h"
#include "EncodedDataStream.h"
#include "Value.h"
#include "Section.h"
#include "Dbb.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "SRLCreateTableSpace.h"
#include "SRLDropTableSpace.h"
#include "Log.h"
#include "InfoTable.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TableSpaceManager::TableSpaceManager(Database *db)
{
	database = db;
	memset(nameHash, 0, sizeof(nameHash));
	memset(idHash, 0, sizeof(nameHash));
	tableSpaces = NULL;
}

TableSpaceManager::~TableSpaceManager()
{
	for (TableSpace *tableSpace; (tableSpace = tableSpaces);)
		{
		tableSpaces = tableSpace->next;
		delete tableSpace;
		}
}

void TableSpaceManager::add(TableSpace *tableSpace)
{
	Sync sync(&syncObject, "TableSpaceManager::add");
	sync.lock(Exclusive);
	int slot = tableSpace->name.hash(TS_HASH_SIZE);
	tableSpace->nameCollision = nameHash[slot];
	nameHash[slot] = tableSpace;
	slot = tableSpace->tableSpaceId % TS_HASH_SIZE;
	tableSpace->idCollision = idHash[slot];
	idHash[slot] = tableSpace;
	tableSpace->next = tableSpaces;
	tableSpaces = tableSpace;
}

TableSpace* TableSpaceManager::findTableSpace(const char *name)
{
	Sync sync(&syncObject, "TableSpaceManager::findTableSpace");
	sync.lock(Shared);
	TableSpace *tableSpace;

	for (tableSpace = nameHash[JString::hash(name, TS_HASH_SIZE)]; tableSpace; tableSpace = tableSpace->nameCollision)
		if (tableSpace->name == name)
			return tableSpace;

	sync.unlock();
	sync.lock(Exclusive);

	for (tableSpace = nameHash[JString::hash(name, TS_HASH_SIZE)]; tableSpace; tableSpace = tableSpace->nameCollision)
		if (tableSpace->name == name)
			return tableSpace;


	Sync syncSystem (&database->syncSysConnection, "TableSpaceManager::findTableSpace");
	syncSystem.lock (Shared);
	PStatement statement = database->prepareStatement(
		"select tablespace_id, filename from system.tablespaces where tablespace=?");
	statement->setString(1, name);
	RSet resultSet = statement->executeQuery();

	if (resultSet->next())
		{
		int id = resultSet->getInt(1);
		const char *fileName = resultSet->getString(2);
		tableSpace = new TableSpace(database, name, id, fileName, 0);

		try
			{
			tableSpace->open();
			}
		catch (...)
			{
			delete tableSpace;

			throw;
			}

		add(tableSpace);
		}

	return tableSpace;
}

TableSpace* TableSpaceManager::getTableSpace(const char *name)
{
	TableSpace *tableSpace = findTableSpace(name);

	if (!tableSpace)
		throw SQLError(DDL_ERROR, "can't find table space \"%s\"", name);

	if (!tableSpace->active)
		throw SQLError(RUNTIME_ERROR, "table space \"%s\" is not active", (const char*) tableSpace->name);

	return tableSpace;
}

TableSpace* TableSpaceManager::createTableSpace(const char *name, const char *fileName, uint64 initialAllocation)
{
	Sync syncSystem(&database->syncSysConnection, "TableSpaceManager::createTableSpace");
	syncSystem.lock(Shared);
	Sequence *sequence = database->sequenceManager->getSequence(database->getSymbol("SYSTEM"), database->getSymbol("TABLESPACE_IDS"));
	int id = (int) sequence->update(1, database->getSystemTransaction());
	TableSpace *tableSpace = new TableSpace(database, name, id, fileName, initialAllocation);
	
	if (tableSpace->dbb->doesFileExist(fileName))
		{
		delete tableSpace;
		throw SQLError(TABLESPACE_EXIST_ERROR, "table space file name \"%s\" already exists\n", fileName);
		}
		
	try
		{
		tableSpace->save();
		tableSpace->create();
		syncSystem.unlock();
		database->commitSystemTransaction();
		add(tableSpace);
		}
	catch (...)
		{
		delete tableSpace;

		throw;
		}

	database->serialLog->logControl->createTableSpace.append(tableSpace);
	
	return tableSpace;
}

void TableSpaceManager::bootstrap(int sectionId)
{
	Dbb *dbb = database->dbb;
	Section *section = dbb->findSection(sectionId);
	Stream stream;
	UCHAR buffer[1024];

	for (int n = 0; (n = dbb->findNextRecord(section, n, &stream)) >= 0; ++n)
		{
		stream.getSegment(0, sizeof(buffer), buffer);
		const UCHAR *p = buffer + 2;
		Value name;
		Value id;
		Value fileName;
		p = EncodedDataStream::decode(p, &name, true);
		p = EncodedDataStream::decode(p, &id, true);
		p = EncodedDataStream::decode(p, &fileName, true);
		TableSpace *tableSpace = new TableSpace(database, name.getString(), id.getInt(), fileName.getString(), 0);
		
		try
			{
			tableSpace->open();
			}
		catch(SQLException& exception)
			{
			Log::log("Couldn't open table space file \"%s\" for tablespace \"%s\": %s\n", 
					 fileName.getString(), name.getString(), exception.getText());
			}
			
		add(tableSpace);
		stream.clear();
		}
}

TableSpace* TableSpaceManager::findTableSpace(int tableSpaceId)
{
	for (TableSpace *tableSpace = idHash[tableSpaceId % TS_HASH_SIZE]; tableSpace; tableSpace = tableSpace->idCollision)
		if (tableSpace->tableSpaceId == tableSpaceId)
			return tableSpace;

	return NULL;
}

TableSpace* TableSpaceManager::getTableSpace(int id)
{
	TableSpace *tableSpace = findTableSpace(id);

	if (!tableSpace)
		throw SQLError(COMPILE_ERROR, "can't find table space %d", id);

	if (!tableSpace->active)
		throw SQLError(RUNTIME_ERROR, "table space \"%s\" is not active", (const char*) tableSpace->name);
		
	return tableSpace;
}


void TableSpaceManager::shutdown(TransId transId)
{
	for (TableSpace *tableSpace = tableSpaces; tableSpace; tableSpace = tableSpace->next)
		tableSpace->shutdown(transId);
}

void TableSpaceManager::dropDatabase(void)
{
	for (TableSpace *tableSpace = tableSpaces; tableSpace; tableSpace = tableSpace->next)
		tableSpace->dropTableSpace();
}

void TableSpaceManager::dropTableSpace(TableSpace* tableSpace)
{
	Sync sync(&syncObject, "TableSpaceManager::dropTableSpace");
	sync.lock(Exclusive);
	Sync syncSystem(&database->syncSysConnection, "TableSpaceManager::dropTableSpace");
	syncSystem.lock(Shared);
	PStatement statement = database->prepareStatement(
		"delete from system.tablespaces where tablespace=?");
	statement->setString(1, tableSpace->name);
	statement->executeUpdate();
	Transaction *transaction = database->getSystemTransaction();
	transaction->hasUpdates = true;
	database->serialLog->logControl->dropTableSpace.append(tableSpace, transaction);
	syncSystem.unlock();
	database->commitSystemTransaction();
	int slot = tableSpace->name.hash(TS_HASH_SIZE);

	for (TableSpace **ptr = nameHash + slot; *ptr; ptr = &(*ptr)->nameCollision)
		if (*ptr == tableSpace)
			{
			*ptr = tableSpace->nameCollision;
			
			break;
			}
			
	sync.unlock();
	tableSpace->active = false;
}

void TableSpaceManager::reportStatistics(void)
{
	Sync sync(&syncObject, "TableSpaceManager::reportStatistics");
	sync.lock(Shared);

	for (TableSpace *tableSpace = tableSpaces; tableSpace; tableSpace = tableSpace->next)
		tableSpace->dbb->reportStatistics();
}

void TableSpaceManager::getIOInfo(InfoTable* infoTable)
{
	Sync sync(&syncObject, "TableSpaceManager::getIOInfo");
	sync.lock(Shared);

	for (TableSpace *tableSpace = tableSpaces; tableSpace; tableSpace = tableSpace->next)
		tableSpace->getIOInfo(infoTable);
}

void TableSpaceManager::validate(int optionMask)
{
	Sync sync(&syncObject, "TableSpaceManager::validate");
	sync.lock(Shared);

	for (TableSpace *tableSpace = tableSpaces; tableSpace; tableSpace = tableSpace->next)
		tableSpace->dbb->validate(optionMask);
}

void TableSpaceManager::sync()
{
	Sync sync(&syncObject, "TableSpaceManager::sync");
	sync.lock(Shared);

	for (TableSpace *tableSpace = tableSpaces; tableSpace; tableSpace = tableSpace->next)
		tableSpace->sync();
}

void TableSpaceManager::expungeTableSpace(int tableSpaceId)
{
	Sync sync(&syncObject, "TableSpaceManager::expungeTableSpace");
	sync.lock(Exclusive);
	TableSpace *tableSpace = findTableSpace(tableSpaceId);
	
	if (!tableSpace)
		return;

	TableSpace **ptr;
	int slot = tableSpace->name.hash(TS_HASH_SIZE);

	for (ptr = nameHash + slot; *ptr; ptr = &(*ptr)->nameCollision)
		if (*ptr == tableSpace)
			{
			*ptr = tableSpace->nameCollision;
			break;
			}

	slot = tableSpace->tableSpaceId % TS_HASH_SIZE;

	for (ptr = idHash + slot; *ptr; ptr = &(*ptr)->idCollision)
		if (*ptr == tableSpace)
			{
			*ptr = tableSpace->idCollision;
			break;
			}
	
	for (ptr = &tableSpaces; *ptr; ptr = &(*ptr)->next)
		if (*ptr == tableSpace)
			{
			*ptr = tableSpace->next;
			break;
			}

	sync.unlock();
	tableSpace->dropTableSpace();
	delete tableSpace;
}

void TableSpaceManager::reportWrites(void)
{
	Sync sync(&syncObject, "TableSpaceManager::reportWrites");
	sync.lock(Shared);

	for (TableSpace *tableSpace = tableSpaces; tableSpace; tableSpace = tableSpace->next)
		tableSpace->dbb->reportWrites();
}

void TableSpaceManager::redoCreateTableSpace(int id, int nameLength, const char* name, int fileNameLength, const char* fileName)
{
	Sync sync(&syncObject, "TableSpaceManager::redoCreateTableSpace");
	sync.lock(Exclusive);
	TableSpace *tableSpace;

	for (tableSpace = idHash[id % TS_HASH_SIZE]; tableSpace; tableSpace = tableSpace->idCollision)
		if (tableSpace->tableSpaceId == id)
			return;

	char buffer[1024];
	memcpy(buffer, name, nameLength);
	buffer[nameLength] = 0;
	char *file = buffer + nameLength + 1;
	memcpy(file, fileName, fileNameLength);
	file[fileNameLength] = 0;
	tableSpace = new TableSpace(database, buffer, id, file, 0);
	tableSpace->needSave = true;
	add(tableSpace);	

	try
		{
		tableSpace->open();
		}
	catch(SQLException& exception)
		{
		Log::log("Couldn't open table space file \"%s\" for tablespace \"%s\": %s\n", 
					file, buffer, exception.getText());
		}
}

void TableSpaceManager::initialize(void)
{
	Sync sync(&syncObject, "TableSpaceManager::initialize");
	sync.lock(Shared);

	for (TableSpace *tableSpace = tableSpaces; tableSpace; tableSpace = tableSpace->next)
		if (tableSpace->needSave)
			{
			Sync syncSystem(&database->syncSysConnection, "TableSpaceManager::dropTableSpace");
			syncSystem.lock(Shared);
			tableSpace->save();
			syncSystem.unlock();
			database->commitSystemTransaction();
			}
}
