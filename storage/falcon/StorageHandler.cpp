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

#include <string.h>
#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "StorageHandler.h"
#include "StorageConnection.h"
#include "Connection.h"
#include "SyncObject.h"
#include "Sync.h"
#include "SQLError.h"
#include "StorageDatabase.h"
#include "StorageTableShare.h"
#include "Stream.h"
#include "Configuration.h"
#include "Threads.h"
#include "Connection.h"
#include "PStatement.h"
#include "RSet.h"
#include "InfoTable.h"
#include "CmdGen.h"
#include "Dbb.h"

#define DICTIONARY_ACCOUNT		"mysql"
#define DICTIONARY_PW			"mysql"
#define FALCON_USER				DEFAULT_TABLESPACE_PATH
#define FALCON_TEMPORARY		TEMPORARY_PATH

#define HASH(address,size)				(int)(((UIPTR) address >> 2) % size)

struct StorageSavepoint {
	StorageSavepoint*	next;
	StorageConnection*	storageConnection;
	int					savepoint;
	};

extern uint64		falcon_initial_allocation;

static const char *createTempSpace = "upgrade tablespace " TEMPORARY_TABLESPACE " filename '" FALCON_TEMPORARY "'";
//static const char *dropTempSpace = "drop tablespace " TEMPORARY_TABLESPACE;

static const char *falconSchema [] = {
	//"create tablespace " DEFAULT_TABLESPACE " filename '" FALCON_USER "' allocation 2000000000",
	createTempSpace,
	
	"upgrade table falcon.tablespaces ("
	"    name varchar(128) not null primary	key,"
	"    pathname varchar(1024) not null)",
	
	"upgrade table falcon.tables ("
	"    given_schema_name varchar(128) not null,"
	"    effective_schema_name varchar(128) not null,"
	"    given_table_name varchar(128) not null,"
	"    effective_table_name varchar(128) not null,"
	"    tablespace_name varchar(128) not null,"
	"    pathname varchar(1024) not null primary key)",
	
	"upgrade unique index effective on falcon.tables (effective_schema_name, effective_table_name)",

	NULL };
				
class Server;
extern Server*	startServer(int port, const char *configFile);

static StorageHandler *storageHandler;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

StorageHandler*	getFalconStorageHandler(int lockSize)
{
	if (!storageHandler)
		storageHandler = new StorageHandler(lockSize);
	
	return storageHandler;
}


StorageHandler::StorageHandler(int lockSize)
{
	mySqlLockSize = lockSize;
	memset(connections, 0, sizeof(connections));
	memset(storageDatabases, 0, sizeof(storageDatabases));
	memset(tables, 0, sizeof(tables));
	dictionaryConnection = NULL;
	databaseList = NULL;
	defaultDatabase = NULL;
}

StorageHandler::~StorageHandler(void)
{
	for (int n = 0; n < databaseHashSize; ++n)
		for (StorageDatabase *storageDatabase; (storageDatabase = storageDatabases[n]);)
			{
			storageDatabases[n] = storageDatabase->collision;
			delete storageDatabase;
			}
	
	for (int n = 0; n < tableHashSize; ++n)
		for (StorageTableShare *table; (table = tables[n]);)
			{
			tables[n] = table->collision;
			delete table;
			}
}

void StorageHandler::startNfsServer(void)
{
	try
		{
		startServer(0, NULL);
		}
	catch (SQLException& exception)
		{
		Log::log("Can't start debug server: %s\n", exception.getText());
		}
}

void StorageHandler::addNfsLogger(int mask, Logger listener, void* arg)
{
	addLogListener(mask, listener, arg);
}


void StorageHandler::shutdownHandler(void)
{
	if (dictionaryConnection)
		{
		dictionaryConnection->commit();
		dictionaryConnection->close();
		dictionaryConnection = NULL;
		}
	
	for (int n = 0; n < databaseHashSize; ++n)
		for (StorageDatabase *storageDatabase = storageDatabases[n]; storageDatabase; storageDatabase = storageDatabase->collision)
			storageDatabase->close();
	
	/***
	Configuration configuration(NULL);
	Connection *connection = new Connection(&configuration);
	connection->shutdown();
	connection->close();
	***/
}

void StorageHandler::databaseDropped(StorageDatabase *storageDatabase, StorageConnection* storageConnection)
{
	if (!storageDatabase && storageConnection)
		storageDatabase = storageConnection->storageDatabase;
		
	if (storageDatabase)
		{
		Sync syncHash(&hashSyncObject, "StorageHandler::dropDatabase");
		int slot = JString::hash(storageDatabase->name, databaseHashSize);
		syncHash.lock(Exclusive);
		StorageDatabase **ptr;
		
		for (ptr = storageDatabases + slot; *ptr; ptr = &(*ptr)->collision)
			if (*ptr == storageDatabase)
				{
				*ptr = storageDatabase->collision;
				break;
				}

		for (ptr = &databaseList; *ptr; ptr = &(*ptr)->next)
			if (*ptr == storageDatabase)
				{
				*ptr = storageDatabase->next;
				break;
				}

		syncHash.unlock();
		storageDatabase->release();
		}

	Sync sync(&syncObject, "StorageHandler::~dropDatabase");
	sync.lock(Exclusive);

	for (int n = 0; n < connectionHashSize; ++n)
		for (StorageConnection *cnct = connections[n]; cnct; cnct = cnct->collision)
			if (cnct != storageConnection)
				cnct->databaseDropped(storageDatabase);
			
	sync.unlock();
}

void StorageHandler::remove(StorageConnection* storageConnection)
{
	Sync sync(&syncObject, "StorageHandler::remove");
	sync.lock(Exclusive);
	removeConnection(storageConnection);			
}

int StorageHandler::commit(THD* mySqlThread)
{
	Sync sync(&syncObject, "StorageHandler::commit");
	sync.lock(Shared);
	int slot = HASH(mySqlThread, connectionHashSize);
	
	for (StorageConnection *connection = connections[slot]; connection; connection = connection->collision)
		if (connection->mySqlThread == mySqlThread)
			{
			int ret =connection->commit();
			
			if (ret)
				return ret;
			}
	
	return 0;
}

int StorageHandler::prepare(THD* mySqlThread, int xidSize, const UCHAR *xid)
{
	Sync sync(&syncObject, "StorageHandler::prepare");
	sync.lock(Shared);
	int slot = HASH(mySqlThread, connectionHashSize);
	
	for (StorageConnection *connection = connections[slot]; connection; connection = connection->collision)
		if (connection->mySqlThread == mySqlThread)
			{
			int ret = connection->prepare(xidSize, xid);
			
			if (ret)
				return ret;
			}
	
	return 0;
}

int StorageHandler::rollback(THD* mySqlThread)
{
	Sync sync(&syncObject, "StorageHandler::rollback");
	sync.lock(Shared);
	int slot = HASH(mySqlThread, connectionHashSize);
	
	for (StorageConnection *connection = connections[slot]; connection; connection = connection->collision)
		if (connection->mySqlThread == mySqlThread)
			{
			int ret = connection->rollback();
			
			if (ret)
				return ret;
			}
	
	return 0;
}

int StorageHandler::releaseVerb(THD* mySqlThread)
{
	Sync sync(&syncObject, "StorageHandler::releaseVerb");
	sync.lock(Shared);
	int slot = HASH(mySqlThread, connectionHashSize);
	
	for (StorageConnection *connection = connections[slot]; connection; connection = connection->collision)
		if (connection->mySqlThread == mySqlThread)
			connection->releaseVerb();
	
	return 0;
}

int StorageHandler::rollbackVerb(THD* mySqlThread)
{
	Sync sync(&syncObject, "StorageHandler::rollbackVerb");
	sync.lock(Shared);
	int slot = HASH(mySqlThread, connectionHashSize);
	
	for (StorageConnection *connection = connections[slot]; connection; connection = connection->collision)
		if (connection->mySqlThread == mySqlThread)
			connection->rollbackVerb();
	
	return 0;
}

int StorageHandler::savepointSet(THD* mySqlThread, void* savePoint)
{
	Sync sync(&syncObject, "StorageHandler::savepointSet");
	sync.lock(Shared);
	int slot = HASH(mySqlThread, connectionHashSize);
	StorageSavepoint *savepoints = NULL;
	
	for (StorageConnection *connection = connections[slot]; connection; connection = connection->collision)
		if (connection->mySqlThread == mySqlThread)
			{
			StorageSavepoint *savepoint = new StorageSavepoint;
			savepoint->next = savepoints;
			savepoints = savepoint;
			savepoint->storageConnection = connection;
			savepoint->savepoint = connection->savepointSet();
			}
	
	*((void**) savePoint) = savepoints;
	
	return 0;
}

int StorageHandler::savepointRelease(THD* mySqlThread, void* savePoint)
{
	Sync sync(&syncObject, "StorageHandler::savepointRelease");
	sync.lock(Shared);
	
	for (StorageSavepoint *savepoints = *(StorageSavepoint**) savePoint, *savepoint; 
		  (savepoint = savepoints);)
		{
		savepoint->storageConnection->savepointRelease(savepoint->savepoint);
		savepoints = savepoint->next;
		delete savepoint;
		}
		
	*((void**) savePoint) = NULL;

	return 0;
}

int StorageHandler::savepointRollback(THD* mySqlThread, void* savePoint)
{
	Sync sync(&syncObject, "StorageHandler::savepointRollback");
	sync.lock(Shared);
	
	for (StorageSavepoint *savepoints = *(StorageSavepoint**) savePoint, *savepoint; 
		   (savepoint = savepoints);)
		{
		savepoint->storageConnection->savepointRollback(savepoint->savepoint);
		savepoints = savepoint->next;
		delete savepoint;
		}
	
	*((void**) savePoint) = NULL;

	return 0;
}

StorageDatabase* StorageHandler::getStorageDatabase(const char* dbName, const char* path)
{
	Sync sync(&hashSyncObject, "StorageHandler::getStorageDatabase");
	int slot = JString::hash(dbName, databaseHashSize);
	StorageDatabase *storageDatabase;
	
	if (storageDatabases[slot])
		{
		sync.lock(Shared);
		
		if ( (storageDatabase = findDatabase(dbName)) )
			return storageDatabase;
			
		sync.unlock();
		}
		
	sync.lock(Exclusive);

	if ( (storageDatabase = findDatabase(dbName)) )
		return storageDatabase;
	
	storageDatabase = new StorageDatabase(this, dbName, path);
	storageDatabase->load();
	storageDatabase->collision = storageDatabases[slot];
	storageDatabases[slot] = storageDatabase;
	storageDatabase->addRef();
	storageDatabase->next = databaseList;
	databaseList = storageDatabase;
	
	return storageDatabase;
}

void StorageHandler::closeDatabase(const char* path)
{
	Sync sync(&hashSyncObject, "StorageHandler::getStorageDatabase");
	int slot = JString::hash(path, databaseHashSize);
	sync.lock(Exclusive);
	
	for (StorageDatabase *storageDatabase, **ptr = storageDatabases + slot; (storageDatabase = *ptr); ptr = &storageDatabase->collision)
		if (storageDatabase->filename == path)
			{
			*ptr = storageDatabase->collision;
			storageDatabase->close();
			storageDatabase->release();
			break;
			}
}

void StorageHandler::releaseText(const char* text)
{
	delete [] text;
}

int StorageHandler::commitByXID(int xidLength, const UCHAR* xid)
{
	return 0;
}

int StorageHandler::rollbackByXID(int xidLength, const UCHAR* xis)
{
	return 0;
}

Connection* StorageHandler::getDictionaryConnection(void)
{
	return dictionaryConnection;
}

int StorageHandler::createTablespace(const char* tableSpaceName, const char* filename)
{
	if (!defaultDatabase)
		initialize();

	//StorageDatabase *storageDatabase = NULL;
	JString tableSpace = JString::upcase(tableSpaceName);
	
	try
		{
		CmdGen gen;
		gen.gen("create tablespace %s filename '%s'", tableSpaceName, filename);
		Sync sync(&dictionarySyncObject, "StorageHandler::createTablespace");
		sync.lock(Exclusive);
		Statement *statement = dictionaryConnection->createStatement();
		statement->executeUpdate(gen.getString());
		statement->close();
		}
	catch (SQLException& exception)
		{
                if (exception.getSqlcode() == DDL_TABLESPACE_EXIST_ERROR)
			return StorageErrorTableSpaceExist;
		return StorageErrorTablesSpaceOperationFailed;
		}
	
	return 0;
}

int StorageHandler::deleteTablespace(const char* tableSpaceName)
{
	if (!defaultDatabase)
		initialize();

	if (   !strcasecmp(tableSpaceName, MASTER_NAME)
		|| !strcasecmp(tableSpaceName, DEFAULT_TABLESPACE)
		|| !strcasecmp(tableSpaceName, TEMPORARY_TABLESPACE))
		{
			return StorageErrorTablesSpaceOperationFailed;
		}
		
	try
		{
		CmdGen gen;
		gen.gen("drop tablespace %s", tableSpaceName);
		Sync sync(&dictionarySyncObject, "StorageHandler::createTablespace");
		sync.lock(Exclusive);
		Statement *statement = dictionaryConnection->createStatement();
		statement->executeUpdate(gen.getString());
		statement->close();
		}
	catch (SQLException&)
		{
		return StorageErrorTablesSpaceOperationFailed;
		}
	
	return 0;
}

StorageTableShare* StorageHandler::findTable(const char* pathname)
{
	char filename [1024];
	cleanFileName(pathname, filename, sizeof(filename));
	Sync sync(&hashSyncObject, "StorageHandler::findTable");
	int slot = JString::hash(filename, tableHashSize);
	StorageTableShare *tableShare;

	if (tables[slot])
		{
		sync.lock(Shared);
		
		for (tableShare = tables[slot]; tableShare; tableShare = tableShare->collision)
			if (tableShare->pathName == filename)
				return tableShare;
	
		sync.unlock();
		}

	sync.lock(Exclusive);
	
	for (tableShare = tables[slot]; tableShare; tableShare = tableShare->collision)
		if (tableShare->pathName == filename)
			return tableShare;
	
	tableShare = new StorageTableShare(this, filename, NULL, mySqlLockSize, false);
	tableShare->collision = tables[slot];
	tables[slot] = tableShare;
	
	return tableShare;
}

StorageTableShare* StorageHandler::preDeleteTable(const char* pathname)
{
	if (!defaultDatabase)
		initialize();

	char filename [1024];
	cleanFileName(pathname, filename, sizeof(filename));
	int slot = JString::hash(filename, tableHashSize);
	StorageTableShare *tableShare;

	if (tables[slot])
		{
		Sync sync(&hashSyncObject, "StorageHandler::preDeleteTable");
		sync.lock(Shared);
		
		for (tableShare = tables[slot]; tableShare; tableShare = tableShare->collision)
			if (tableShare->pathName == filename)
				return tableShare;
		}

	tableShare = new StorageTableShare(this, filename, NULL, mySqlLockSize, false);
	JString path = tableShare->lookupPathName();
	delete tableShare;
	
	if (path == pathname)
		return findTable(pathname);
	
	return NULL;
}

StorageTableShare* StorageHandler::createTable(const char* pathname, const char *tableSpaceName, bool tempTable)
{
	if (!defaultDatabase)
		initialize();

	StorageTableShare *tableShare = new StorageTableShare(this, pathname, tableSpaceName, mySqlLockSize, tempTable);
	
	if (tableShare->tableExists())
		{
		delete tableShare;
		
		return NULL;
		}

	addTable(tableShare);
	tableShare->registerTable();
	
	return tableShare;
}

void StorageHandler::addTable(StorageTableShare* table)
{
	int slot = JString::hash(table->pathName, tableHashSize);
	Sync sync(&hashSyncObject, "StorageHandler::add");
	sync.lock(Exclusive);
	table->collision = tables[slot];
	tables[slot] = table;
}

void StorageHandler::removeTable(StorageTableShare* table)
{
	Sync sync(&hashSyncObject, "StorageHandler::deleteTable");
	sync.lock(Exclusive);
	int slot = JString::hash(table->pathName, tableHashSize);
	
	for (StorageTableShare **ptr = tables + slot; *ptr; ptr = &(*ptr)->collision)
		if (*ptr == table)
			{
			*ptr = table->collision;
			break;
			}
}

StorageConnection* StorageHandler::getStorageConnection(StorageTableShare* tableShare, THD* mySqlThread, int mySqlThdId, OpenOption createFlag)
{
	Sync sync(&syncObject, "StorageConnection::getStorageConnection");
	
	if (!defaultDatabase)
		initialize();

	if (!tableShare->storageDatabase)
		tableShare->findDatabase();

	StorageDatabase *storageDatabase = defaultDatabase;
	int slot = HASH(mySqlThread, connectionHashSize);
	StorageConnection *storageConnection;
	
	if (connections[slot])
		{
		for (storageConnection = connections[slot]; storageConnection; storageConnection = storageConnection->collision)
			if (storageConnection->mySqlThread == mySqlThread) // && storageConnection->storageDatabase == tableShare->storageDatabase)
				{
				storageConnection->addRef();
				
				if (!tableShare->storageDatabase)
					tableShare->setDatabase(storageDatabase);
					
				return storageConnection;
				}

		sync.lock(Shared);
		sync.unlock();
		}
		
	sync.lock(Exclusive);
	
	for (storageConnection = connections[slot]; storageConnection; storageConnection = storageConnection->collision)
		if (storageConnection->mySqlThread == mySqlThread) // && storageConnection->storageDatabase == tableShare->storageDatabase)
			{
			storageConnection->addRef();
				
			if (!tableShare->storageDatabase)
				tableShare->setDatabase(storageDatabase);
					
			return storageConnection;
			}
	
	
	storageConnection = new StorageConnection(this, storageDatabase, mySqlThread, mySqlThdId);
	bool success = false;
	
	if (createFlag != CreateDatabase) // && createFlag != OpenTemporaryDatabase)
		try
			{
			storageConnection->connect();
			success = true;
			}
		catch (SQLException& exception)
			{
			//fprintf(stderr, "database open failed: %s\n", exception.getText());
			storageConnection->setErrorText(exception.getText());
			
			if (createFlag == OpenDatabase)
				{
				delete storageConnection;
				
				return NULL;
				}
			}
	
	if (!success && createFlag != OpenDatabase)
		try
			{
			storageConnection->create();
			}
		catch (SQLException&)
			{
			delete storageConnection;
			
			return NULL;
			}
	
	tableShare->setDatabase(storageDatabase);
	storageConnection->collision = connections[slot];
	connections[slot] = storageConnection;
	
	return storageConnection;
}

StorageDatabase* StorageHandler::findDatabase(const char* dbName)
{
	int slot = JString::hash(dbName, databaseHashSize);
	
	for (StorageDatabase *storageDatabase = storageDatabases[slot]; storageDatabase; storageDatabase = storageDatabase->collision)
		if (storageDatabase->name == dbName)
			{
			storageDatabase->addRef();
			
			return storageDatabase;
			}
			
	return NULL;
}

void StorageHandler::changeMySqlThread(StorageConnection* storageConnection, THD* newThread)
{
	Sync sync(&syncObject, "StorageHandler::changeMySqlThread");
	sync.lock(Exclusive);
	removeConnection(storageConnection);
	storageConnection->mySqlThread = newThread;		
	int slot = HASH(storageConnection->mySqlThread, connectionHashSize);
	storageConnection->collision = connections[slot];
	connections[slot] = storageConnection;	
}

void StorageHandler::removeConnection(StorageConnection* storageConnection)
{
	int slot = HASH(storageConnection->mySqlThread, connectionHashSize);
	
	for (StorageConnection **ptr = connections + slot; *ptr; ptr = &(*ptr)->collision)
		if (*ptr == storageConnection)
			{
			*ptr = storageConnection->collision;
			break;
			}
}

int StorageHandler::closeConnections(THD* thd)
{
	int slot = HASH(thd, connectionHashSize);
	Sync sync(&syncObject, "StorageHandler::closeConnections");
	
	for (StorageConnection *storageConnection = connections[slot], *next; storageConnection; storageConnection = next)
		{
		next = storageConnection->collision;
		
		if (storageConnection->mySqlThread == thd)
			{
			storageConnection->close();
		
			if (storageConnection->mySqlThread)
				storageConnection->release();	// This is for thd->ha_data[falcon_hton->slot]
			
			storageConnection->release();	// This is for storageConn
			}
		}
	
	return 0;
}

int StorageHandler::dropDatabase(const char* path)
{
	/***
	char pathname[FILENAME_MAX];
	const char *SEPARATOR = pathname;
	char *q = pathname;
	
	for (const char *p = path; *p;)
		{
		char c = *p++;
		
		if (c == '/')
			{
			if (*p == 0)
				break;
				
			SEPARATOR = q + 1;
			}
		
		*q++ = c;
		}
	
	*q = 0;
	JString dbName = JString::upcase(SEPARATOR);
	strcpy(q, StorageTableShare::getDefaultRoot());	
	StorageDatabase *storageDatabase = getStorageDatabase(dbName, pathname);
	databaseDropped(storageDatabase, NULL);

	try
		{
		storageDatabase->dropDatabase();
		}
	catch (SQLException&)
		{
		}

	storageDatabase->release();
	***/
	
	return 0;
}

void StorageHandler::getIOInfo(InfoTable* infoTable)
{
	Sync sync(&hashSyncObject, "StorageHandler::getIOInfo");
	sync.lock(Shared);
	
	for (StorageDatabase *storageDatabase = databaseList; storageDatabase; storageDatabase = storageDatabase->next)
		storageDatabase->getIOInfo(infoTable);
}

void StorageHandler::getMemoryDetailInfo(InfoTable* infoTable)
{
	MemMgrAnalyze(MemMgrSystemDetail, infoTable);
}

void StorageHandler::getMemorySummaryInfo(InfoTable* infoTable)
{
	MemMgrAnalyze(MemMgrSystemSummary, infoTable);
}

void StorageHandler::getRecordCacheDetailInfo(InfoTable* infoTable)
{
	MemMgrAnalyze(MemMgrRecordDetail, infoTable);
}

void StorageHandler::getRecordCacheSummaryInfo(InfoTable* infoTable)
{
	MemMgrAnalyze(MemMgrRecordSummary, infoTable);
}

void StorageHandler::getTransactionInfo(InfoTable* infoTable)
{
	Sync sync(&hashSyncObject, "StorageHandler::getTransactionInfo");
	sync.lock(Shared);
	
	for (StorageDatabase *storageDatabase = databaseList; storageDatabase; storageDatabase = storageDatabase->next)
		storageDatabase->getTransactionInfo(infoTable);
}

void StorageHandler::getSerialLogInfo(InfoTable* infoTable)
{
	Sync sync(&hashSyncObject, "StorageHandler::getTransactionInfo");
	sync.lock(Shared);
	
	for (StorageDatabase *storageDatabase = databaseList; storageDatabase; storageDatabase = storageDatabase->next)
		storageDatabase->getSerialLogInfo(infoTable);
}

void StorageHandler::getSyncInfo(InfoTable* infoTable)
{
	SyncObject::getSyncInfo(infoTable);
}

void StorageHandler::getTransactionSummaryInfo(InfoTable* infoTable)
{
	Sync sync(&hashSyncObject, "StorageHandler::getTransactionSummaryInfo");
	sync.lock(Shared);
	
	for (StorageDatabase *storageDatabase = databaseList; storageDatabase; storageDatabase = storageDatabase->next)
		storageDatabase->getTransactionSummaryInfo(infoTable);
}

void StorageHandler::initialize(void)
{
	if (defaultDatabase)
		return;
	
	Sync sync(&syncObject, "StorageConnection::initialize");
	sync.lock(Exclusive);
	
	if (defaultDatabase)
		return;
		
	defaultDatabase = getStorageDatabase(MASTER_NAME, MASTER_PATH);
	
	try
		{
		defaultDatabase->getOpenConnection();
		dictionaryConnection = defaultDatabase->getOpenConnection();
		dropTempTables();
		}
	catch (...)
		{
		try
			{
			defaultDatabase->createDatabase();
			IO::deleteFile(FALCON_USER);
			IO::deleteFile(FALCON_TEMPORARY);
			dictionaryConnection = defaultDatabase->getOpenConnection();
			Statement *statement = dictionaryConnection->createStatement();

			JString createTableSpace;
			createTableSpace.Format(
					"create tablespace " DEFAULT_TABLESPACE " filename '" FALCON_USER "' allocation " I64FORMAT,
					falcon_initial_allocation);
			statement->executeUpdate(createTableSpace);
			
			for (const char **ddl = falconSchema; *ddl; ++ddl)
				statement->executeUpdate(*ddl);
			
			statement->close();
			dictionaryConnection->commit();
			}
		catch(...)
			{
			return;
			}
		}
}

void StorageHandler::dropTempTables(void)
{
	Statement *statement = dictionaryConnection->createStatement();
	
	try
		{
		PStatement select = dictionaryConnection->prepareStatement(
			"select schema,tablename from system.tables where tablespace='" TEMPORARY_TABLESPACE "'");
		RSet resultSet = select->executeQuery();
		bool hit = false;
		
		while (resultSet->next())
			{
			CmdGen gen;
			gen.gen("drop table \"%s\".\"%s\"", resultSet->getString(1), resultSet->getString(2));
			statement->executeUpdate(gen.getString());
			hit = true;
			}
		
		//if (hit)
			//statement->executeUpdate(dropTempSpace);
		}
	catch(...)
		{
		}
	
	/***
	try
		{
		statement->executeUpdate(dropTempSpace);
		}
	catch(SQLException& exception)
		{
		Log::log("Can't delete temporary tablespace: %s\n", exception.getText());
		}

	IO::deleteFile(FALCON_TEMPORARY);
	***/
	
	try
		{
		statement->executeUpdate(createTempSpace);
		}
	catch(SQLException& exception)
		{
		Log::log("Can't create temporary tablespace: %s\n", exception.getText());
		}
	
	statement->close();
}

void StorageHandler::getTablesInfo(InfoTable* infoTable)
{
	if (!defaultDatabase)
		initialize();
	
	try
		{
		PStatement statement = dictionaryConnection->prepareStatement(
			"select schema,tablename,tablespace from system.tables where tablespace <> ''");
		RSet resultSet = statement->executeQuery();
		
		while (resultSet->next())
			{

			// Parse table and partition name
			
			const char *pStr = resultSet->getString(2);
			char *pTable = NULL;
			char *pPart = NULL;
			
			if (pStr)
				{
				const int max_buf = 1024;
				char buffer[max_buf+1];
				
				pTable = buffer;
				*pTable = 0;
				strncpy(buffer, pStr, (size_t)max_buf);
				
				char *pBuf = strchr(buffer, '#');
				
				if (pBuf)
					{
					*pBuf = 0;
					if ((pPart = strrchr(++pBuf, '#')) != NULL)
						pPart++;
					}
				}
					
			infoTable->putString(0, resultSet->getString(1));	// database
			infoTable->putString(1, (pTable ? pTable : pStr));	// table
			infoTable->putString(2, (pPart ? pPart : ""));		// partition
			infoTable->putString(3, resultSet->getString(3));	// tablespace
			infoTable->putString(4, resultSet->getString(2));	// internal name
		
			//for (int n = 0; n < 3; ++n)
			//	infoTable->putString(n, resultSet->getString(n + 1));
			
			infoTable->putRecord();
			}
		
		dictionaryConnection->commit();
		}
	catch(...)
		{
		}
}

void StorageHandler::setRecordMemoryMax(uint64 value)
{
	if (dictionaryConnection)
		dictionaryConnection->setRecordMemoryMax(value);
}

void StorageHandler::setRecordScavengeThreshold(int value)
{
	if (dictionaryConnection)
		dictionaryConnection->setRecordScavengeThreshold(value);
}

void StorageHandler::setRecordScavengeFloor(int value)
{
	if (dictionaryConnection)
		dictionaryConnection->setRecordScavengeFloor(value);
}

void StorageHandler::cleanFileName(const char* pathname, char* filename, int filenameLength)
{
	char c, prior = 0;
	char *q = filename;
	char *end = filename + filenameLength - 1;
	filename[0] = 0;
	
	for (const char *p = pathname; q < end && (c = *p++); prior = c)
		if (c != SEPARATOR || c != prior)
			*q++ = c;

	*q = 0;
}
