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
#include "StorageDatabase.h"
#include "StorageConnection.h"
#include "SyncObject.h"
#include "Sync.h"
#include "SQLError.h"
#include "Threads.h"
#include "StorageHandler.h"
#include "StorageTable.h"
#include "StorageTableShare.h"
#include "TableSpaceManager.h"
#include "Sync.h"
#include "Threads.h"
#include "Configuration.h"
#include "Connection.h"
#include "Database.h"
#include "Table.h"
#include "Field.h"
#include "User.h"
#include "RoleModel.h"
#include "Value.h"
#include "RecordVersion.h"
#include "Transaction.h"
#include "Statement.h"
#include "Bitmap.h"
#include "PStatement.h"
#include "RSet.h"
#include "Sequence.h"
#include "StorageConnection.h"
#include "MySqlEnums.h"
#include "ScaledBinary.h"
#include "Dbb.h"
#include "CmdGen.h"
//#include "SyncTest.h"

#define ACCOUNT				"mysql"
#define PASSWORD			"mysql"

static Threads			*threads;
static Configuration	*configuration;

static const char *ddl [] = {
	"upgrade sequence mystorage.indexes",
	
	NULL
	};

#ifdef TRACE_TRANSACTIONS
static const char *traceTable = 
	"upgrade table falcon.transactions ("
	"     transaction_id int not null primary key,"
	"     committed int,"
	"     blocked_by int,"
	"     statements clob)";
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


StorageDatabase::StorageDatabase(StorageHandler *handler, const char *dbName, const char* path)
{
	name = dbName;
	filename = path;
	storageHandler = handler;
	masterConnection = NULL;
	user = NULL;
	lookupIndexAlias = NULL;
	useCount = 1;
	insertTrace	= NULL;
	
	if (!threads)
		{
		threads = new Threads(NULL);
		//SyncTest syncTest;
		//syncTest.test();
		}
}

StorageDatabase::~StorageDatabase(void)
{
	if (lookupIndexAlias)
		lookupIndexAlias->release();

	if (masterConnection)
		masterConnection->release();
	
	if (user)
		user->release();
}

Connection* StorageDatabase::getConnection()
{
	if (!configuration)
		configuration = new Configuration(NULL);
		
	return new Connection(configuration);
}

Connection* StorageDatabase::getOpenConnection(void)
{
	try
		{
		if (!masterConnection)
			masterConnection = getConnection();
		
		if (!masterConnection->database)
			{
			masterConnection->openDatabase(name, filename, ACCOUNT, PASSWORD, NULL, threads);
			clearTransactions();
			}
		}
	catch (...)
		{
		if (masterConnection)
			{
			masterConnection->close();
			masterConnection = NULL;
			}
		
		throw;
		}
	
	return masterConnection->clone();
}

Connection* StorageDatabase::createDatabase(void)
{
	try
		{
		masterConnection = getConnection();
		IO::createPath(filename);
		masterConnection->createDatabase(name, filename, ACCOUNT, PASSWORD, threads);
		Statement *statement = masterConnection->createStatement();
		
		for (const char **sql = ddl; *sql; ++sql)
			statement->execute(*sql);
		
		statement->release();
		}
	catch (...)
		{
		if (masterConnection)
			{
			masterConnection->close();
			masterConnection = NULL;
			}
		
		throw;
		}
	
	return masterConnection->clone();
}


Table* StorageDatabase::createTable(StorageConnection *storageConnection, const char* tableName, const char *schemaName, const char* sql, int64 autoIncrementValue)
{
	Database *database = masterConnection->database;
	
	if (!user)
		if ((user = database->findUser(ACCOUNT)))
			user->addRef();		
	
	Statement *statement = masterConnection->createStatement();
	
	try
		{
		Table *table = database->findTable(schemaName, tableName);
		
		if (table)
			database->dropTable(table, masterConnection->getTransaction());
			
		statement->execute(sql);
		
		if (autoIncrementValue)
			{
			char buffer[1024];
			snprintf(buffer, sizeof(buffer), "create sequence  %s.\"%s\" start with " I64FORMAT, schemaName, tableName, autoIncrementValue - 1);
			statement->execute(buffer);
			}
			
		statement->release();
		}
	catch (SQLException& exception)
		{
		statement->release();
		storageConnection->setErrorText(&exception);
		
		return NULL;
		}
		
	return findTable(tableName, schemaName);
}

Table* StorageDatabase::findTable(const char* tableName, const char *schemaName)
{
	return masterConnection->database->findTable(schemaName, tableName);
}

int StorageDatabase::insert(Connection* connection, Table* table, Stream* stream)
{
	return table->insert(connection->getTransaction(), stream);
}

int StorageDatabase::nextRow(StorageTable* storageTable, int recordNumber, bool lockForUpdate)
{
	StorageConnection *storageConnection = storageTable->storageConnection;
	Connection *connection = storageConnection->connection;
	Table *table = storageTable->share->table;
	Transaction *transaction = connection->getTransaction();
	Record *candidate;
	Record *record;
	
	try
		{
		for (;; ++recordNumber)
			{
			record = NULL;
			candidate = NULL;
			candidate = table->fetchNext(recordNumber);
			
			if (!candidate)
				return StorageErrorRecordNotFound;

			record = (lockForUpdate)
			               ? table->fetchForUpdate(transaction, candidate, false)
			               : candidate->fetchVersion(transaction);
			
			if (!record)
				{
				if (!lockForUpdate)
					candidate->release();
					
				continue;
				}
			
			if (!lockForUpdate && candidate != record)
				{
				record->addRef();
				candidate->release();
				}
			
			recordNumber = record->recordNumber;
			storageTable->setRecord(record, lockForUpdate);
			
			return recordNumber;
			}
		}
	catch (SQLException& exception)
		{
		if (record && record != candidate)
			record->release();

		if (candidate && !lockForUpdate)
			candidate->release();
			
		int sqlcode = storageConnection->setErrorText(&exception);
		
		switch (sqlcode)
			{
			case UPDATE_CONFLICT:
				return StorageErrorUpdateConflict;
			case OUT_OF_MEMORY_ERROR:
				return StorageErrorOutOfMemory;
			case OUT_OF_RECORD_MEMORY_ERROR:
				return StorageErrorOutOfRecordMemory;
			case DEADLOCK:
				return StorageErrorDeadlock;
			}

		return StorageErrorRecordNotFound;
		}
}

int StorageDatabase::fetch(StorageConnection *storageConnection, StorageTable* storageTable, int recordNumber, bool lockForUpdate)
{
	Table *table = storageTable->share->table;
	Connection *connection = storageConnection->connection;
	Transaction *transaction = connection->getTransaction();
	Record *candidate = NULL;;
	
	try
		{
		candidate = table->fetch(recordNumber);
		
		if (!candidate)
			return StorageErrorRecordNotFound;

		Record *record = (lockForUpdate)
		               ? table->fetchForUpdate(transaction, candidate, false)
		               : candidate->fetchVersion(transaction);
		
		if (!record)
			{
			if (!lockForUpdate)
				candidate->release();
			
			return StorageErrorRecordNotFound;
			}
		
		if (!lockForUpdate && record != candidate)
			{
			record->addRef();
			candidate->release();
			}
		
		storageTable->setRecord(record, lockForUpdate);
				
		return 0;
		}
	catch (SQLException& exception)
		{
		if (candidate && !lockForUpdate)
			candidate->release();
			
		int sqlcode = storageConnection->setErrorText(&exception);
		
		switch (sqlcode)
			{
			case UPDATE_CONFLICT:
				return StorageErrorUpdateConflict;
			case OUT_OF_MEMORY_ERROR:
				return StorageErrorOutOfMemory;
			case OUT_OF_RECORD_MEMORY_ERROR:
				return StorageErrorOutOfRecordMemory;
			case DEADLOCK:
				return StorageErrorDeadlock;
			}

		return StorageErrorRecordNotFound;
		}
}


int StorageDatabase::nextIndexed(StorageTable *storageTable, void* recordBitmap, int recordNumber, bool lockForUpdate)
{
	if (!recordBitmap)
		return StorageErrorRecordNotFound;

	StorageConnection *storageConnection = storageTable->storageConnection;
	Connection *connection = storageConnection->connection;
	Table *table = storageTable->share->table;
	Transaction *transaction = connection->getTransaction();
	Record *candidate = NULL;
	
	try
		{
		Bitmap *bitmap = (Bitmap*) recordBitmap;
		
		for (;;)
			{
			recordNumber = bitmap->nextSet(recordNumber);
			
			if (recordNumber < 0)
				return StorageErrorRecordNotFound;
				
			candidate = table->fetch(recordNumber);
			++recordNumber;
			
			if (candidate)
				{
				Record *record = (lockForUpdate) 
				               ? table->fetchForUpdate(transaction, candidate, true)
				               : candidate->fetchVersion(transaction);
				
				if (record)
					{
					recordNumber = record->recordNumber;

					if (!lockForUpdate && candidate != record)
						{
						record->addRef();
						candidate->release();
						}
					
					storageTable->setRecord(record, lockForUpdate);
					
					return recordNumber;
					}
				
				if (!lockForUpdate)
					candidate->release();
				}
			}
		}
	catch (SQLException& exception)
		{
		if (candidate && !lockForUpdate)
			candidate->release();

		storageConnection->setErrorText(&exception);

		int errorCode = exception.getSqlcode();
		switch (errorCode)
			{
			case UPDATE_CONFLICT:
				return StorageErrorUpdateConflict;
			case OUT_OF_MEMORY_ERROR:
				return StorageErrorOutOfMemory;
			case OUT_OF_RECORD_MEMORY_ERROR:
				return StorageErrorOutOfRecordMemory;
			case DEADLOCK:
				return StorageErrorDeadlock;
			}

		return StorageErrorRecordNotFound;
		}
}

RecordVersion* StorageDatabase::lockRecord(StorageConnection* storageConnection, Table *table, Record* record)
{
	try
		{
		return table->lockRecord(record, storageConnection->connection->getTransaction());
		}
	catch (SQLException& exception)
		{
		storageConnection->setErrorText(&exception);
		
		return NULL;
		}
}

int StorageDatabase::savepointSet(Connection* connection)
{
	Transaction *transaction = connection->getTransaction();
	
	return transaction->createSavepoint();
}

int StorageDatabase::savepointRollback(Connection* connection, int savePoint)
{
	Transaction *transaction = connection->getTransaction();
	transaction->rollbackSavepoint(savePoint);
	
	return 0;
}

int StorageDatabase::savepointRelease(Connection* connection, int savePoint)
{
	Transaction *transaction = connection->getTransaction();
	transaction->releaseSavepoint(savePoint);
	
	return 0;
}

int StorageDatabase::deleteTable(StorageConnection* storageConnection, StorageTableShare *tableShare)
{
	const char *schemaName = tableShare->schemaName;
	const char *tableName = tableShare->name;
	Connection *connection = storageConnection->connection;
	CmdGen gen;
	gen.gen("drop table %s.\"%s\"", schemaName, tableName);
	Statement *statement = connection->createStatement();
	
	try
		{
		statement->execute(gen.getString());
		}
	catch (SQLException& exception)
		{
		int errorCode = exception.getSqlcode();
		storageConnection->setErrorText(&exception);
		
		switch (errorCode)
			{
			case NO_SUCH_TABLE:
				return StorageErrorTableNotFound;
				
			case UNCOMMITTED_UPDATES:
				return StorageErrorUncommittedUpdates;
			}
			
		return 200 - errorCode;
		}

	// Drop sequence, if any.  If none, this will throw an exception.  Ignore it
	
	int res = 0;
	
	if (connection->findSequence(schemaName, tableName))
		try
			{
			gen.reset();
			gen.gen("drop sequence %s.\"%s\"", schemaName, tableName);
			statement->execute(gen.getString());
			}
		catch (SQLException& exception)
			{
			storageConnection->setErrorText(&exception);
			res = 200 - exception.getSqlcode();
			}
	
	statement->release();
	
	return res;
}

int StorageDatabase::truncateTable(StorageConnection* storageConnection, StorageTableShare *tableShare)
{
	Connection *connection = storageConnection->connection;
	Transaction *transaction = connection->transaction;
	Database *database = connection->database;
	Sequence *sequence = tableShare->sequence;
	
	int res = 0;
	
	try
		{
		database->truncateTable(tableShare->table, transaction);
	
		if (sequence)
			sequence = sequence->recreate();
		}
	catch (SQLException& exception)
		{
		int errorCode = exception.getSqlcode();
		storageConnection->setErrorText(&exception);
		
		switch (errorCode)
			{
			case NO_SUCH_TABLE:
				return StorageErrorTableNotFound;
				
			case UNCOMMITTED_UPDATES:
				return StorageErrorUncommittedUpdates;
			}
			
		res = 200 - exception.getSqlcode();
		}
	
	return res;
}

int StorageDatabase::deleteRow(StorageConnection *storageConnection, Table* table, int recordNumber)
{
	Connection *connection = storageConnection->connection;
	Transaction *transaction = connection->transaction;
	Record *candidate = NULL, *record = NULL;
	
	try
		{
		candidate = table->fetch(recordNumber);
		
		if (!candidate)
			return StorageErrorRecordNotFound;
		
		if (candidate->state == recLock)
			record = candidate->getPriorVersion();
		else if (candidate->getTransaction() == transaction)
			record = candidate;
		else
			record = candidate->fetchVersion(transaction);

		if (record != candidate)
			{
			record->addRef();
			candidate->release();
			}
		
		table->deleteRecord(transaction, record);
		record->release();
		
		return 0;
		}
	catch (SQLException& exception)
		{
		int code;
		int sqlCode = exception.getSqlcode();

		if (record)
			record->release();
		else if (candidate)
			candidate->release();

		switch (sqlCode)
			{
			case DEADLOCK:
				code = StorageErrorDeadlock;
				break;

			case UPDATE_CONFLICT:
				code = StorageErrorUpdateConflict;
				break;
				
			case OUT_OF_MEMORY_ERROR:
				code = StorageErrorOutOfMemory;
				break;

			default:
				code = StorageErrorRecordNotFound;
			}

		storageConnection->setErrorText(&exception);

		return code;
		}
}

int StorageDatabase::updateRow(StorageConnection* storageConnection, Table* table, Record *oldRecord, Stream* stream)
{
	Connection *connection = storageConnection->connection;
	table->update (connection->getTransaction(), oldRecord, stream);
	
	return 0;
}

int StorageDatabase::createIndex(StorageConnection *storageConnection, Table* table, const char* indexName, const char* sql)
{
	Connection *connection = storageConnection->connection;
	Statement *statement = connection->createStatement();
	
	try
		{
		statement->execute(sql);
		}
	catch (SQLException& exception)
		{
		storageConnection->setErrorText(&exception);
		statement->release();
		
		if (exception.getSqlcode() == INDEX_OVERFLOW)
			return StorageErrorIndexOverflow;
		
		return StorageErrorNoIndex;
		}
	
	statement->release();
	
	return 0;
}

int StorageDatabase::renameTable(StorageConnection* storageConnection, Table* table, const char* tableName, const char *schemaName)
{
	Connection *connection = storageConnection->connection;

	try
		{
		Database *database = connection->database;
		Sequence *sequence = connection->findSequence(schemaName, table->name);
		int numberIndexes = 0;
		int firstIndex = 0;
		Index *index;

		for (index = table->indexes; index; index = index->next)
			{
			if (index->type == PrimaryKey)
				firstIndex = 1;

			++numberIndexes;
			}

		Sync sync(&database->syncSysConnection, "StorageDatabase::renameTable");
		sync.lock(Exclusive);

		for (int n = firstIndex; n < numberIndexes; ++n)
			{
			char indexName[256];
			sprintf(indexName, "%s$%d", (const char*) table->name, n);
			Index *index = table->findIndex(indexName);

			if (index)
				{
				sprintf(indexName, "%s$%d", tableName, n);
				index->rename(indexName);
				}
			}

		table->rename(schemaName, tableName);

		if (sequence)
			sequence->rename(tableName);

		sync.unlock();
		database->commitSystemTransaction();

		return 0;
		}
	catch (SQLException& exception)
		{
		storageConnection->setErrorText(&exception);
		
		return StorageErrorDupKey;
		}
}

Index* StorageDatabase::findIndex(Table* table, const char* indexName)
{
	return table->findIndex(indexName);
}

Bitmap* StorageDatabase::indexScan(Index* index, StorageKey *lower, StorageKey *upper, int searchFlags, StorageConnection* storageConnection, Bitmap *bitmap)
{
	if (!index)
		return NULL;

	if (lower)
		lower->indexKey.index = index;
		
	if (upper)
		upper->indexKey.index = index;
		
	return index->scanIndex((lower) ? &lower->indexKey : NULL,
							(upper) ? &upper->indexKey : NULL, searchFlags, 
							storageConnection->connection->getTransaction(), bitmap);
}

int StorageDatabase::makeKey(StorageIndexDesc* indexDesc, const UCHAR* key, int keyLength, StorageKey* storageKey)
{
	int segmentNumber = 0;
	Value vals [MAX_KEY_SEGMENTS];
	Value *values[MAX_KEY_SEGMENTS];
	Index *index = indexDesc->index;
	
	if (!index)
		return StorageErrorBadKey;
	
	try
		{
		for (const UCHAR *p = key, *end = key + keyLength; p < end && segmentNumber < indexDesc->numberSegments; ++segmentNumber)
			{
			StorageSegment *segment = indexDesc->segments + segmentNumber;
			int nullFlag = (segment->nullBit) ? *p++ : 0;
			values[segmentNumber] = vals + segmentNumber;
			int len = getSegmentValue(segment, p, values[segmentNumber], index->fields[segmentNumber]);
			
			if (nullFlag)
				{
				values[segmentNumber]->setNull();
				break;
				}

			p += len;
			}

		index->makeKey(segmentNumber, values, &storageKey->indexKey);
		storageKey->numberSegments = segmentNumber;
		
		return 0;
		}
	catch (SQLError&)
		{
		return StorageErrorBadKey;
		}
}


int StorageDatabase::isKeyNull(StorageIndexDesc* indexDesc, const UCHAR* key, int keyLength)
{
	int segmentNumber = 0;
	
	for (const UCHAR *p = key, *end = key + keyLength; p < end && segmentNumber < indexDesc->numberSegments; ++segmentNumber)
		{
		StorageSegment *segment = indexDesc->segments + segmentNumber;
		int nullFlag = (segment->nullBit) ? *p++ : 0;
	
		if (!nullFlag)
			return false;
		
		switch (segment->type)
			{
			case HA_KEYTYPE_VARBINARY1:
			case HA_KEYTYPE_VARBINARY2:
			case HA_KEYTYPE_VARTEXT1:
			case HA_KEYTYPE_VARTEXT2:
				p += segment->length + 2;
				break;
			
			default:
				p += segment->length;
			}
		}
	
	return true;
}

int StorageDatabase::storeBlob(Connection* connection, Table* table, StorageBlob *blob)
{
	return table->storeBlob(connection->getTransaction(), blob->length, blob->data);
}

void StorageDatabase::getBlob(Table* table, int recordNumber, StorageBlob* blob)
{
	Stream stream;
	table->getBlob(recordNumber, &stream);
	blob->length = stream.totalLength;
	blob->data = new UCHAR[blob->length];
	stream.getSegment(0, blob->length, blob->data);
}

Sequence* StorageDatabase::findSequence(const char* name, const char *schemaName)
{
	return masterConnection->findSequence(schemaName, name);
}

void StorageDatabase::addRef(void)
{
	++useCount;
}

void StorageDatabase::release(void)
{
	if (--useCount == 0)
		delete this;
}

void StorageDatabase::close(void)
{
	if (masterConnection)
		{
		if (user)
			{
			user->release();
			user = NULL;
			}
			
		masterConnection->shutdownDatabase();
		masterConnection = NULL;
		}
}

void StorageDatabase::dropDatabase(void)
{
	if (user)
		{
		user->release();
		user = NULL;
		}
	
	if (!masterConnection)
		{
		Connection *connection = getOpenConnection();
		connection->release();
		}
	
	masterConnection->dropDatabase();
	masterConnection->release();
	masterConnection = NULL;
}

void StorageDatabase::freeBlob(StorageBlob *blob)
{
	delete [] blob->data;
	blob->data = NULL;
}

void StorageDatabase::validateCache(void)
{
	if (masterConnection)
		masterConnection->database->validateCache();
}

int StorageDatabase::getSegmentValue(StorageSegment* segment, const UCHAR* ptr, Value* value, Field *field)
{
	int length = segment->length;
	
	switch (segment->type)
		{
		case HA_KEYTYPE_LONG_INT:
			{
			int32 temp;
			memcpy(&temp, ptr, sizeof(temp));
			value->setValue(temp);
			}
			break;

		case HA_KEYTYPE_SHORT_INT:
			{
			short temp;
			memcpy(&temp, ptr, sizeof(temp));
			value->setValue(temp);
			}
			break;

		case HA_KEYTYPE_ULONGLONG:
		case HA_KEYTYPE_LONGLONG:
			{
			int64 temp;
			memcpy(&temp, ptr, sizeof(temp));
			value->setValue(temp);
			}
			break;
				
		case HA_KEYTYPE_FLOAT:
			{
			float temp;
			memcpy(&temp, ptr, sizeof(temp));
			value->setValue(temp);
			}
			break;
		
		case HA_KEYTYPE_DOUBLE:
			{
			double temp;
			memcpy(&temp, ptr, sizeof(temp));
			value->setValue(temp);
			}
			break;
		
		case HA_KEYTYPE_VARBINARY1:
		case HA_KEYTYPE_VARBINARY2:
		case HA_KEYTYPE_VARTEXT1:
		case HA_KEYTYPE_VARTEXT2:
			{
			unsigned short len;
			memcpy(&len, ptr, sizeof(len));
			value->setString(len, (const char*) ptr + 2, false);
			length += 2;
			}
			break;
		
		case HA_KEYTYPE_BINARY:
			if (field->isString())
				value->setString(length, (const char*) ptr, false);
			else if (segment->isUnsigned)
				{
				int64 number = 0;
				
				for (int n = 0; n < length; ++n)
					number = number << 8 | *ptr++;
					
				value->setValue(number);
				}
			else if (field->precision < 19 && field->scale == 0)
				{
				int64 number = (signed char) (*ptr++ ^ 0x80);
				
				for (int n = 1; n < length; ++n)
					number = (number << 8) | *ptr++;
				
				if (number < 0)
					++number;
					
				value->setValue(number);
				}
			else
				{
				BigInt bigInt;
				ScaledBinary::getBigIntFromBinaryDecimal((const char*) ptr, field->precision, field->scale, &bigInt);
				value->setValue(&bigInt);
				}

			break;
			
		case HA_KEYTYPE_TEXT:
			value->setString(length, (const char*) ptr, false);
			break;
		
		case HA_KEYTYPE_ULONG_INT:
			{
			uint32 temp;
			memcpy(&temp, ptr, sizeof(temp));
			
			if (field && field->type == Timestamp)
				value->setValue((int64) temp * 1000);
			else
				value->setValue((int64) temp);
			}
			break;
		
		case HA_KEYTYPE_INT8:
			value->setValue(*(signed char*) ptr);
			break;
		
		case HA_KEYTYPE_USHORT_INT:
			{
			unsigned short temp;
			memcpy(&temp, ptr, sizeof(temp));
			value->setValue(*ptr);
			}
			break;
			
		case HA_KEYTYPE_UINT24:
			{
			uint32 temp;
			memcpy(&temp, ptr, 3);
			value->setValue((int) (temp & 0xffffff));
			}
			break;
			
		case HA_KEYTYPE_INT24:
			{
			int32 temp;
			memcpy(&temp, ptr, 3);
			value->setValue((temp << 8) >> 8);
			}
			break;
		
		default:
			NOT_YET_IMPLEMENTED;
		}
	
	return length;
}

void StorageDatabase::save(void)
{
	Sync sync(&storageHandler->dictionarySyncObject, "StorageTableShare::save");
	sync.lock(Exclusive);
	Connection *connection = storageHandler->getDictionaryConnection();
	PreparedStatement *statement = connection->prepareStatement(
		"replace falcon.tablespaces (name, pathname) values (?,?)");
	int n = 1;
	statement->setString(n++, name);
	statement->setString(n++, filename);
	statement->executeUpdate();	
	statement->close();	
	connection->commit();
}

void StorageDatabase::load(void)
{
	Sync sync(&storageHandler->dictionarySyncObject, "StorageDatabase::load");
	sync.lock(Exclusive);
	Connection *connection = storageHandler->getDictionaryConnection();
	
	if (connection)
		{
		PreparedStatement *statement = connection->prepareStatement(
			"select pathname from falcon.tablespaces where name=?");
		statement->setString(1, name);
		ResultSet *resultSet = statement->executeQuery();
		
		if (resultSet->next())
			filename = resultSet->getString(1);
		
		resultSet->close();
		statement->close();
		connection->commit();
		}
}

void StorageDatabase::clearTransactions(void)
{
#ifdef TRACE_TRANSACTIONS

	Sync sync(&traceSyncObject, "StorageDatabase::clearTransactions");
	sync.lock(Exclusive);
	Statement *statement = masterConnection->createStatement();
	statement->execute(traceTable);
	statement->release();

	PreparedStatement *preparedStatement = masterConnection->prepareStatement(
		"delete from falcon.transactions");
	preparedStatement->executeUpdate();
	preparedStatement->close();
	masterConnection->commit();
#endif
}

void StorageDatabase::traceTransaction(int transId, int committed, int blockedBy, Stream *stream)
{
	try
		{
		Sync sync(&traceSyncObject, "StorageDatabase::traceTransaction");
		sync.lock(Exclusive);
		char buffer [10000];
		int length = stream->getSegment(0, sizeof(buffer) - 1, buffer);
		buffer[length] = 0;
		
		if (!insertTrace)
			insertTrace = masterConnection->prepareStatement(
				"insert into falcon.transactions (transaction_id,committed,blocked_by,statements) values (?,?,?,?)");
		
		int n = 1;
		insertTrace->setInt(n++, transId);
		insertTrace->setInt(n++, committed);
		insertTrace->setInt(n++, blockedBy);
		insertTrace->setString(n++, buffer);
		insertTrace->executeUpdate();
		masterConnection->commit();
		}
	catch (SQLException&)
		{
		}
}

void StorageDatabase::getIOInfo(InfoTable* infoTable)
{
	if (masterConnection && masterConnection->database)
		{
		masterConnection->database->getIOInfo(infoTable);
		masterConnection->database->tableSpaceManager->getIOInfo(infoTable);
		}
}

void StorageDatabase::getTransactionInfo(InfoTable* infoTable)
{
	if (masterConnection && masterConnection->database)
		masterConnection->database->getTransactionInfo(infoTable);
}

void StorageDatabase::getSerialLogInfo(InfoTable* infoTable)
{
	if (masterConnection && masterConnection->database)
		masterConnection->database->getSerialLogInfo(infoTable);
}

void StorageDatabase::getTransactionSummaryInfo(InfoTable* infoTable)
{
	if (masterConnection && masterConnection->database)
		masterConnection->database->getTransactionSummaryInfo(infoTable);
}
