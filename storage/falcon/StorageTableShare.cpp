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

#include <memory.h>
#include <string.h>
#include <stdio.h>
#include "Engine.h"
#include "StorageTableShare.h"
#include "StorageDatabase.h"
#include "StorageHandler.h"
#include "SyncObject.h"
#include "Sync.h"
#include "Sequence.h"
#include "Index.h"
#include "Table.h"
#include "SyncObject.h"
#include "CollationManager.h"
#include "MySQLCollation.h"
#include "Connection.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "SQLException.h"

static const char *FALCON_TEMPORARY		= "/falcon_temporary";
static const char *DB_ROOT				= ".fts";

#if defined(_WIN32) && MYSQL_VERSION_ID < 0x50100
#define IS_SLASH(c)	(c == '/' || c == '\\')
#else
#define IS_SLASH(c)	(c == '/')
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StorageTableShare::StorageTableShare(StorageHandler *handler, const char * path, const char *tableSpaceName, int lockSize, bool tempTbl)
{
	storageHandler = handler;
	storageDatabase = NULL;
	impure = new UCHAR[lockSize];
	initialized = false;
	table = NULL;
	indexes = NULL;
	syncObject = new SyncObject;
	syncObject->setName("StorageTableShare::syncObject");
	sequence = NULL;
	tempTable = tempTbl;
	setPath(path);
	syncTruncate = new SyncObject;
	syncTruncate->setName("StorageTableShare::syncTruncate");
	
	if (tempTable)
		tableSpace = TEMPORARY_TABLESPACE;
	else if (tableSpaceName && tableSpaceName[0])
		tableSpace = JString::upcase(tableSpaceName);
	else
		tableSpace = schemaName;
}

StorageTableShare::~StorageTableShare(void)
{
	delete syncObject;
	delete syncObject;
	delete [] impure;
	
	if (storageDatabase)
		storageDatabase->release();
		
	if (indexes)
		{
		for (int n = 0; n < numberIndexes; ++n)
			delete indexes[n];
			
		delete [] indexes;
		indexes = NULL;
		}
}

void StorageTableShare::lock(bool exclusiveLock)
{
	syncObject->lock(NULL, (exclusiveLock) ? Exclusive : Shared);
}

void StorageTableShare::unlock(void)
{
	syncObject->unlock();
}

int StorageTableShare::open(void)
{
	if (!table)
		{
		table = storageDatabase->findTable(name, schemaName);
		sequence = storageDatabase->findSequence(name, schemaName);
		}
	
	if (!table)
		return StorageErrorTableNotFound;
	
	return 0;
}

int StorageTableShare::create(StorageConnection *storageConnection, const char* sql, int64 autoIncrementValue)
{
	if (!(table = storageDatabase->createTable(storageConnection, name, schemaName, sql, autoIncrementValue)))
		return StorageErrorTableExits;
	
	if (autoIncrementValue)
		sequence = storageDatabase->findSequence(name, schemaName);
		
	return 0;
}

int StorageTableShare::deleteTable(StorageConnection *storageConnection)
{
	int res = storageDatabase->deleteTable(storageConnection, this);
	
	if (res == 0 || res == StorageErrorTableNotFound)
		{
		unRegisterTable();
		
		if (res == 0)
			storageHandler->removeTable(this);
			
		delete this;
		}

	return res;
}

int StorageTableShare::truncateTable(StorageConnection *storageConnection)
{
	int res = storageDatabase->truncateTable(storageConnection, this);
	
	return res;
}

void StorageTableShare::cleanupFieldName(const char* name, char* buffer, int bufferLength)
{
	char *q = buffer;
	char *end = buffer + bufferLength - 1;
	const char *p = name;
	
	for (; *p && q < end; ++p)
		*q++ = UPPER(*p);
	
	*q = 0;
}

const char* StorageTableShare::cleanupTableName(const char* name, char* buffer, int bufferLength, char *schema, int schemaLength)
{
	char *q = buffer;
	const char *p = name;
	
	while (*p == '.')
		++p;

	for (; *p; ++p)
		if (*p == '/' || *p == '\\')
			{
			*q = 0;
			strcpy(schema, buffer);
			q = buffer;
			}
		else
			*q++ = *p; //UPPER(*p);
	
	*q = 0;
	
	if ( (q = strchr(buffer, '.')) )
		*q = 0;
	
	return buffer;
}

int StorageTableShare::createIndex(StorageConnection *storageConnection, const char* name, const char* sql)
{
	if (!table)
		open();

	return storageDatabase->createIndex(storageConnection, table, name, sql);
}

int StorageTableShare::renameTable(StorageConnection *storageConnection, const char* newName)
{
	char tableName[256];
	char schemaName[256];
	cleanupTableName(newName, tableName, sizeof(tableName), schemaName, sizeof(schemaName));
	int ret = storageDatabase->renameTable(storageConnection, table, JString::upcase(tableName), JString::upcase(schemaName));

	if (ret)
		return ret;
	
	unRegisterTable();
	storageHandler->removeTable(this);
	setPath(newName);
	registerTable();
	storageHandler->addTable(this);
	
	return ret;
}

StorageIndexDesc* StorageTableShare::getIndex(int indexCount, int indexId, StorageIndexDesc* indexDesc)
{
	if (!indexes)
		{
		indexes = new StorageIndexDesc*[indexCount];
		memset(indexes, 0, indexCount * sizeof(StorageIndexDesc*));
		numberIndexes = indexCount;
		}
	
	if (indexId >= numberIndexes)
		return NULL;
	
	StorageIndexDesc *index = indexes[indexId];
	
	if (index)
		return index;

	indexes[indexId] = index = new StorageIndexDesc;
	*index = *indexDesc;
	
	if (indexDesc->primaryKey)
		index->index = table->primaryKey;
	else
		{
		char indexName[150];
		sprintf(indexName, "%s$%d", (const char*) name, indexId);
		index->index = storageDatabase->findIndex(table, indexName);
		}

	if (index->index)
		index->segmentRecordCounts = index->index->recordsPerSegment;
	else
		index = NULL;
	
	return index;
}

StorageIndexDesc* StorageTableShare::getIndex(int indexId)
{
	if (!indexes || indexId >= numberIndexes)
		return NULL;
	
	return indexes[indexId];
}

INT64 StorageTableShare::getSequenceValue(int delta)
{
	if (!sequence)
		return 0;

	return sequence->update(delta, NULL);
}

int StorageTableShare::setSequenceValue(INT64 value)
{
	if (!sequence)
		return StorageErrorNoSequence;
		
	Sync sync(syncObject, "StorageTableShare::setSequenceValue");
	sync.lock(Exclusive);
	INT64 current = sequence->update(0, NULL);
	
	if (value > current)
		sequence->update(value - current, NULL);

	return 0;
}

int StorageTableShare::getIndexId(const char* schemaName, const char* indexName)
{
	if (indexes)
		for (int n = 0; n < numberIndexes; ++n)
			{
			Index *index = indexes[n]->index;
			
			if (strcmp(index->getIndexName(), indexName) == 0 &&
				strcmp(index->getSchemaName(), schemaName) == 0)
				return n;
			}
		
	return -1;
}

int StorageTableShare::haveIndexes(void)
{
	if (indexes == NULL)
		return false;
	
	for (int n = 0; n < numberIndexes; ++n)
		if (indexes[n]== NULL)
			return false;
	
	return true;
}

void StorageTableShare::setTablePath(const char* path, bool tmp)
{
	if (pathName.IsEmpty())
		pathName = path;
	
	tempTable = tmp;
}

void StorageTableShare::registerCollation(const char* collationName, void* arg)
{
	JString name = JString::upcase(collationName);
	Collation *collation = CollationManager::findCollation(name);
	
	if (collation)
		{
		collation->release();
		
		return;
		}
	
	collation = new MySQLCollation(name, arg);
	CollationManager::addCollation(collation);
}

void StorageTableShare::load(void)
{
	Sync sync(&storageHandler->dictionarySyncObject, "StorageTableShare::load");
	sync.lock(Exclusive);
	Connection *connection = storageHandler->getDictionaryConnection();
	PreparedStatement *statement = connection->prepareStatement(
		"select given_schema_name,given_table_name,effective_schema_name,effective_table_name,tablespace_name "
		"from falcon.tables where pathname=?");
	statement->setString(1, pathName);
	ResultSet *resultSet = statement->executeQuery();
	
	if (resultSet->next())
		{
		int n = 1;
		givenSchema = resultSet->getString(n++);
		givenName = resultSet->getString(n++);
		schemaName = resultSet->getString(n++);
		name = resultSet->getString(n++);
		tableSpace = resultSet->getString(n++);
		}
	
	resultSet->close();
	statement->close();	
	connection->commit();
}

void StorageTableShare::registerTable(void)
{
	Connection *connection = NULL;
	PreparedStatement *statement = NULL;
	
	try
		{
		Sync sync(&storageHandler->dictionarySyncObject, "StorageTableShare::save");
		sync.lock(Exclusive);
		connection = storageHandler->getDictionaryConnection();
		statement = connection->prepareStatement(
			"replace falcon.tables "
			"(given_schema_name,given_table_name,effective_schema_name,effective_table_name,tablespace_name, pathname)"
			" values (?,?,?,?,?,?)");
		int n = 1;
		statement->setString(n++, givenSchema);
		statement->setString(n++, givenName);
		statement->setString(n++, schemaName);
		statement->setString(n++, name);
		statement->setString(n++, tableSpace);
		statement->setString(n++, pathName);
		statement->executeUpdate();	
		statement->close();	
		connection->commit();
		}
	catch (SQLException&)
		{
		if (statement)
			statement->close();
			
		if (connection)
			connection->commit();
		}
}


void StorageTableShare::unRegisterTable(void)
{
	Sync sync(&storageHandler->dictionarySyncObject, "StorageTableShare::unsave");
	sync.lock(Exclusive);
	Connection *connection = storageHandler->getDictionaryConnection();
	PreparedStatement *statement = connection->prepareStatement(
		"delete from falcon.tables where pathname=?");
	statement->setString(1, pathName);
	statement->executeUpdate();	
	statement->close();	
	connection->commit();
}

void StorageTableShare::getDefaultPath(char *buffer)
{
	const char *slash = NULL;
	const char *p;

	for (p = pathName; *p; p++)
		if (IS_SLASH(*p))
			slash = p;

	if (!slash)
		slash = p;

	IPTR len = slash - pathName + 1;
	
	if (tempTable)
		len += sizeof(FALCON_TEMPORARY);
		
	char *q = buffer;

	for (p = pathName; p < slash; )
		{
		char c = *p++;
		*q++ = (IS_SLASH(c)) ? '/' : c;
		}

	if (tempTable)
		for (p = FALCON_TEMPORARY; *p;)
			*q++ = *p++;
			
	strcpy(q, DB_ROOT);
}

void StorageTableShare::setPath(const char* path)
{
	pathName = path;
	char tableName[256];
	char schema[256];
	cleanupTableName(path, tableName, sizeof(tableName), schema, sizeof(schema));
	givenName = tableName;
	givenSchema = schema;
	tableSpace = JString::upcase(givenSchema);
	name = JString::upcase(tableName);
	schemaName = JString::upcase(schema);
}

void StorageTableShare::findDatabase(void)
{
	load();
	const char *dbName = (tableSpace.IsEmpty()) ? MASTER_NAME : tableSpace;
	storageDatabase = storageHandler->findDatabase(dbName);
}

const char* StorageTableShare::getDefaultRoot(void)
{
	return DB_ROOT;
}

void StorageTableShare::setDatabase(StorageDatabase* db)
{
	if (storageDatabase)
		storageDatabase->release();
	
	if ( (storageDatabase = db) )
		storageDatabase->addRef();
}

uint64 StorageTableShare::estimateCardinality(void)
{
	return table->estimateCardinality();
}

bool StorageTableShare::tableExists(void)
{
	JString path = lookupPathName();
	
	return !path.IsEmpty();
}

JString StorageTableShare::lookupPathName(void)
{
	Sync sync(&storageHandler->dictionarySyncObject, "StorageTableShare::lookupPathName");
	sync.lock(Exclusive);
	Connection *connection = storageHandler->getDictionaryConnection();
	PreparedStatement *statement = connection->prepareStatement(
		"select pathname from falcon.tables where effective_schema_name=? and effective_table_name=?");
	int n = 1;
	statement->setString(n++, schemaName);
	statement->setString(n++, name);
	ResultSet *resultSet = statement->executeQuery();
	JString path;
		
	if (resultSet->next())
		path = resultSet->getString(1);
	
	statement->close();
	connection->commit();
	
	return path;
}
