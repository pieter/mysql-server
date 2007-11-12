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

#ifndef _STORAGE_HANDLER_H_
#define _STORAGE_HANDLER_H_

#include "SyncObject.h"

#define MASTER_NAME				"FALCON_MASTER"
#define MASTER_PATH				"falcon_master.fts"
#define DEFAULT_TABLESPACE		"FALCON_USER"
#define DEFAULT_TABLESPACE_PATH "falcon_user.fts"
#define TEMPORARY_TABLESPACE	"FALCON_TEMPORARY"
#define TEMPORARY_PATH			"falcon_temporary.fts"

static const int connectionHashSize = 101;

enum OpenOption {
	CreateDatabase,
	OpenDatabase,
	OpenOrCreateDatabase,
	OpenTemporaryDatabase
	};
	
static const int TABLESPACE_INTERNAL		= 0;
static const int TABLESPACE_SCHEMA			= 1;

typedef void (Logger) (int, const char*, void *arg);

struct IOAnalysis;
class StorageConnection;
class StorageHandler;
class StorageDatabase;
class StorageTableShare;
class SyncObject;
class Connection;
class InfoTable;
class PreparedStatement;
class THD;

extern "C" 
{
StorageHandler*	getFalconStorageHandler(int lockSize);
}

static const int databaseHashSize = 101;
static const int tableHashSize = 101;

class StorageHandler
{
public:
	StorageHandler(int lockSize);
	virtual ~StorageHandler(void);
	virtual void		startNfsServer(void);
	virtual void		addNfsLogger(int mask, Logger listener, void* arg);
	virtual void		deleteNfsLogger(Logger listener, void* arg);

	virtual void		shutdownHandler(void);
	virtual void		databaseDropped(StorageDatabase *storageDatabase, StorageConnection* storageConnection);
	virtual int			startTransaction(THD* mySqlThread, int isolationLevel);
	virtual int			commit(THD* mySqlThread);
	virtual int			rollback(THD* mySqlThread);
	virtual int			releaseVerb(THD* mySqlThread);
	virtual int			rollbackVerb(THD* mySqlThread);
	virtual int			savepointSet(THD* mySqlThread, void* savePoint);
	virtual int			savepointRelease(THD* mySqlThread, void* savePoint);
	virtual int			savepointRollback(THD* mySqlThread, void* savePoint);
	virtual void		releaseText(const char* text);
	virtual int			commitByXID(int xidLength, const unsigned char* xid);
	virtual int			rollbackByXID(int xidLength, const unsigned char* xis);
	virtual Connection*	getDictionaryConnection(void);
	virtual int			createTablespace(const char* tableSpaceName, const char* filename);
	virtual int			deleteTablespace(const char* tableSpaceName);

	virtual StorageTableShare* findTable(const char* pathname);
	virtual StorageTableShare* createTable(const char* pathname, const char *tableSpaceName, bool tempTable);
	virtual StorageConnection* getStorageConnection(StorageTableShare* tableShare, THD* mySqlThread, int mySqlThdId, OpenOption createFlag);

	virtual void		getIOInfo(InfoTable* infoTable);
	virtual void		getMemoryDetailInfo(InfoTable* infoTable);
	virtual void		getMemorySummaryInfo(InfoTable* infoTable);
	virtual void		getRecordCacheDetailInfo(InfoTable* infoTable);
	virtual void		getRecordCacheSummaryInfo(InfoTable* infoTable);
	virtual void		getTransactionInfo(InfoTable* infoTable);
	virtual void		getSerialLogInfo(InfoTable* infoTable);
	virtual void		getSyncInfo(InfoTable* infoTable);
	virtual void		getTransactionSummaryInfo(InfoTable* infoTable);
	virtual void		getTablesInfo(InfoTable* infoTable);

	virtual void		setRecordMemoryMax(uint64 size);
	virtual void		setRecordScavengeThreshold(int value);
	virtual void		setRecordScavengeFloor(int value);
	virtual	StorageTableShare* preDeleteTable(const char* pathname);

	StorageDatabase*	getStorageDatabase(const char* dbName, const char* path);
	void				remove(StorageConnection* storageConnection);
	void				closeDatabase(const char* path);
	int					prepare(THD* mySqlThread, int xidSize, const unsigned char *xid);
	StorageDatabase*	findTablespace(const char* name);
	void				removeTable(StorageTableShare* table);
	void				addTable(StorageTableShare* table);
	StorageDatabase*	findDatabase(const char* dbName);
	void				changeMySqlThread(StorageConnection* storageConnection, THD* newThread);
	void				removeConnection(StorageConnection* storageConnection);
	int					closeConnections(THD* thd);
	int					dropDatabase(const char* path);
	void				initialize(void);
	void				dropTempTables(void);
	void				cleanFileName(const char* pathname, char* filename, int filenameLength);
	
	StorageConnection	*connections[connectionHashSize];
	StorageDatabase		*defaultDatabase;
	SyncObject			syncObject;
	SyncObject			hashSyncObject;
	SyncObject			dictionarySyncObject;
	StorageDatabase		*storageDatabases[databaseHashSize];
	StorageDatabase		*databaseList;
	StorageTableShare	*tables[tableHashSize];
	Connection			*dictionaryConnection;
	int					mySqlLockSize;
};

#endif
