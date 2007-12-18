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

#ifndef _STORAGE_DATABASE_H_
#define _STORAGE_DATABASE_H_

#include "SyncObject.h"

//#define TRACE_TRANSACTIONS

static const int shareHashSize = 101;

class StorageConnection;
class StorageTableShare;
class StorageTable;
class StorageHandler;
class Connection;
class Table;
class User;
class Record;
class RecordVersion;
class Index;
class PreparedStatement;
class Sequence;
class Value;
class Bitmap;

CLASS(Field);

struct StorageIndexDesc;
struct StorageKey;
struct StorageBlob;
struct StorageSegment;

class StorageDatabase
{
public:
	StorageDatabase(StorageHandler *handler, const char *dbName, const char* path);
	~StorageDatabase(void);
	
	Connection*			getConnection();
	Connection*			getOpenConnection(void);
	Connection*			createDatabase(void);
	Table*				createTable(StorageConnection *storageConnection, const char* tableName, const char *schemaName, const char* sql, int64 autoIncrementValue);
	int					savepointSet(Connection* connection);
	int					savepointRelease(Connection* connection, int savePoint);
	int					savepointRollback(Connection* connection, int savePoint);
	int					deleteTable(StorageConnection* storageConnection,StorageTableShare *tableShare);
	int					truncateTable(StorageConnection* storageConnection, StorageTableShare *tableShare);
	int					createIndex(StorageConnection *storageConnection, Table* table, StorageIndexDesc* indexDesc);
	int					renameTable(StorageConnection* storageConnection, Table* table, const char* newName, const char *schemaName);
	Bitmap*				indexScan(Index* index, StorageKey *lower, StorageKey *upper, int searchFlags, StorageConnection* storageConnection, Bitmap *bitmap);
	int					makeKey(StorageIndexDesc* index, const UCHAR* key, int keyLength, StorageKey* storageKey);
	int					storeBlob(Connection* connection, Table* table, StorageBlob* blob);
	void				getBlob(Table* table, int recordNumber, StorageBlob* blob);
	void				addRef(void);
	void				release(void);
	void				dropDatabase(void);
	void				freeBlob(StorageBlob *blob);
	void				close(void);
	void				validateCache(void);
	int					createIndex(StorageConnection* storageConnection, Table* table, const char* indexName, const char* sql);
	int					insert(Connection* connection, Table* table, Stream* stream);
	
	int					nextRow(StorageTable* storageTable, int recordNumber, bool lockForUpdate);
	int					nextIndexed(StorageTable *storageTable, void* recordBitmap, int recordNumber, bool lockForUpdate);
	int					fetch(StorageConnection* storageConnection, StorageTable* storageTable, int recordNumber, bool lockForUpdate);
	//bool				lockRecord(StorageTable* storageTable, Record* record);
	RecordVersion*		lockRecord(StorageConnection* storageConnection, Table *table, Record* record);
	
	int					updateRow(StorageConnection* storageConnection, Table* table, Record *oldRecord, Stream* stream);
	int					getSegmentValue(StorageSegment* segment, const UCHAR* ptr, Value* value, Field *field);
	int					deleteRow(StorageConnection* storageConnection, Table* table, int recordNumber);
	Table*				findTable(const char* tableName, const char *schemaName);
	Index*				findIndex(Table* table, const char* indexName);
	Sequence*			findSequence(const char* name, const char *schemaName);
	int					isKeyNull(StorageIndexDesc* indexDesc, const UCHAR* key, int keyLength);
	void				save(void);
	void				load(void);

	void				clearTransactions(void);
	void				traceTransaction(int transId, int committed, int blockedBy, Stream *stream);

	Connection			*masterConnection;
	JString				name;
	JString				filename;
	StorageDatabase		*collision;
	StorageDatabase		*next;
	StorageHandler		*storageHandler;
	SyncObject			syncObject;
	SyncObject			traceSyncObject;
	User				*user;
	PreparedStatement	*lookupIndexAlias;
	PreparedStatement	*insertTrace;
	int					useCount;
	void getIOInfo(InfoTable* infoTable);
	void getTransactionInfo(InfoTable* infoTable);
	void getSerialLogInfo(InfoTable* infoTable);
	void getTransactionSummaryInfo(InfoTable* infoTable);
};

#endif
