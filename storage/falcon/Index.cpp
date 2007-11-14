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

// Index.cpp: implementation of the Index class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "Index.h"
#include "PreparedStatement.h"
#include "Table.h"
#include "Field.h"
#include "Configuration.h"
#include "Database.h"
#include "Value.h"
#include "Record.h"
#include "ResultSet.h"
#include "Collation.h"
#include "Sync.h"
#include "SQLError.h"
#include "IndexKey.h"
#include "DeferredIndex.h"
#include "DeferredIndexWalker.h"
#include "Transaction.h"
#include "Connection.h"
#include "Bitmap.h"
#include "Dbb.h"
#include "IndexRootPage.h"
#include "Index2RootPage.h"
#include "PStatement.h"
#include "RSet.h"

#define SEGMENT_BYTE(segment,count)		((indexVersion >= INDEX_VERSION_1) ? count - segment : segment)
#define PAD_BYTE(field)					((indexVersion >= INDEX_VERSION_1) ? field->indexPadByte : 0)

static const char *tables[] = {
	"Indexes",
	"IndexFields",
	NULL };

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Index::Index(Table *tbl, const char *indexName, int count, int indexType)
{
	init(tbl, indexName, indexType, count);
	indexId = -1;
	savePending = true;
	indexVersion = dbb->defaultIndexVersion;
}

Index::Index(Table * tbl, const char * indexName, int indexType, int id, int numFields)
{
	ASSERT (id != -1);
	init(tbl, indexName, indexType, numFields);
	indexId = INDEX_ID(id);
	indexVersion = INDEX_VERSION(id);
	loadFields();
	savePending = false;
}


void Index::init(Table *tbl, const char *indexName, int indexType, int count)
{
	table = tbl;
	database = table->database;
	dbb = table->dbb;
	name = indexName;
	type = indexType & IndexTypeMask;
	numberFields = count;
	damaged = false;
	rebuild = false;
	fields = new Field* [numberFields];
	partialLengths = NULL;
	recordsPerSegment = NULL;
	rootPage = 0;
	recordsPerSegment = new uint64[numberFields];
	memset(recordsPerSegment, 0, sizeof(uint64) * numberFields);
	deferredIndexes.syncObject.setName("Index::deferredIndexes");
//* These kind of commented lines implement multiple DI hash sizes.
//*	curHashTable = 0;
//*	memset(DIHashTables, 0, sizeof(DIHashTables));
//*	memset(DIHashTableCounts, 0, sizeof(DIHashTableCounts));
//*	memset(DIHashTableSlotsUsed, 0, sizeof(DIHashTableSlotsUsed));
	DIHashTable = NULL;
	DIHashTableCounts =  0;
	DIHashTableSlotsUsed =  0;
}

Index::~Index()
{
	if (deferredIndexes.first)
		{
		Sync sync(&deferredIndexes.syncObject, "Index::~Index");
		sync.lock(Exclusive);

		for (DeferredIndex *deferredIndex = deferredIndexes.first; deferredIndex;deferredIndex = deferredIndex->next)
			{
			ASSERT(deferredIndex->index == this);
			deferredIndex->detachIndex();
			}
		}

//*	for (int h = 0; h < MAX_DI_HASH_TABLES; h++)
//*		if (DIHashTables[h] != NULL)
//*			delete[] DIHashTables[h];
		if (DIHashTable != NULL)
			delete[] DIHashTable;

	delete[] fields;
	delete[] partialLengths;
	delete[] recordsPerSegment;
}

void Index::addField(Field * fld, int position)
{
	ASSERT (position >= 0 && position < numberFields);
	fields[position] = fld;

	if (type == PrimaryKey)
		fld->setNotNull();
}

int Index::matchField(Field * fld)
{
	for (int n = 0; n < numberFields; ++n)
		if (fields[n] == fld)
			return n;

	return -1;
}


void Index::save()
{
	ASSERT (indexId != -1);
	Sync sync (&database->syncSysConnection, "Index::save");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"insert Indexes (indexName,schema,tableName,indexType,fieldCount,indexId)values(?,?,?,?,?,?);");
	statement->setString (1, name);
	statement->setString (2, table->schemaName);
	statement->setString (3, table->name);
	statement->setInt (4, (fields) ? type : (type | StorageEngineIndex));
	statement->setInt (5, numberFields);
	statement->setInt (6, INDEX_COMPOSITE (indexId, indexVersion));
	statement->executeUpdate();
	statement->close();

	const char *sql = (database->fieldExtensions) ?
		"insert IndexFields (field,schema,indexName,position,tableName, partial)values(?,?,?,?,?,?)" :
		"insert IndexFields (field,schema,indexName,position,tableName)values(?,?,?,?,?)";
		
	statement = database->prepareStatement (sql);
	statement->setString (2, table->schemaName);
	statement->setString (3, name);
	statement->setString (5, table->name);

	for (int n = 0; n < numberFields; ++n)
		{
		statement->setString (1, fields[n]->getName());
		statement->setInt (4, n);
		
		if (database->fieldExtensions)
			statement->setInt(6, getPartialLength(n));
			
		statement->executeUpdate();
		}
	
	statement->close();
	savePending = false;
}

void Index::create(Transaction *transaction)
{
	ASSERT (indexId == -1);
	indexId = dbb->createIndex(TRANSACTION_ID(transaction));
	indexVersion = INDEX_CURRENT_VERSION;
}

void Index::insert(Record * record, Transaction *transaction)
{
	IndexKey key(this);
	makeKey (record, &key);
	insert(&key, record->recordNumber, transaction);
}

void Index::insert(IndexKey* key, int32 recordNumber, Transaction *transaction)
{
	DeferredIndex *deferredIndex = getDeferredIndex(transaction);
	deferredIndex->addNode(key, recordNumber);

#ifdef CHECK_DEFERRED_INDEXES
	bool ret = database->addIndexEntry (indexId, &key, record->recordNumber, transaction);
#endif
}

// Find or create a deferredIndex associated with this Index and transaction
// for the purpose of writing to it.

DeferredIndex *Index::getDeferredIndex(Transaction *transaction)
{
	DeferredIndex *deferredIndex;

	// Use the shortest linked list. Index::deferredIndexes or transaction->deferredIndexes

	if (deferredIndexes.count < transaction->deferredIndexCount)
		{
		Sync sync(&deferredIndexes.syncObject, "Index::insert");
		sync.lock(Shared);
		for (deferredIndex = deferredIndexes.first; 
			 deferredIndex; 
			 deferredIndex = deferredIndex->next)
			{
			if ((deferredIndex->transaction == transaction) && (deferredIndex->virtualOffset == 0))
				break;
			}
		}
	else
		{
		for (deferredIndex = transaction->deferredIndexes; 
			 deferredIndex; 
			 deferredIndex = deferredIndex->nextInTransaction)
			{
			if ((deferredIndex->index == this) && (deferredIndex->virtualOffset == 0))
				break;
			}
		}

	if ((deferredIndex) && 
		(type != PrimaryKey) && 
		(type != UniqueIndex) && 
		(transaction->scanIndexCount == 0))
		{
		if (deferredIndex->sizeEstimate > database->configuration->indexChillThreshold)
			{
			// Scavenge (or chill) this DeferredIndex and get a new one
			deferredIndex->chill(dbb);
			ASSERT(deferredIndex->virtualOffset);
			deferredIndex = NULL;
			}
		}

	if (deferredIndex)
		{
		ASSERT((deferredIndex->index == this) && (deferredIndex->transaction == transaction));
		return deferredIndex;
		}

	// Make a new one and attach to Index and Transaction.

	Sync sync(&deferredIndexes.syncObject, "Index::insert");
	deferredIndex = new DeferredIndex(this, transaction);
	sync.lock(Exclusive);
	deferredIndexes.append(deferredIndex);
	sync.unlock();
	transaction->add(deferredIndex);

	return deferredIndex;
}


void Index::makeKey(Field *field, Value *value, int segment, IndexKey *indexKey)
{
	if (damaged)
		damageCheck();

	indexKey->keyLength = 0;

	if (!value)
		return;

	switch (field->type)
		{
		case String:
		case Varchar:
		case Char:
			{
			int partialLength = getPartialLength(segment);

			if (field->collation)
				{
				field->collation->makeKey(value, indexKey, partialLength);
				
				return;
				}

			UCHAR *key = indexKey->key;
			int l = value->getString(sizeof(indexKey->key), (char*) indexKey->key);
			
			if (partialLength && partialLength < l)
				l = partialLength;

			UCHAR *q = key + l;

			while (q > key && q[-1] == ' ')
				--q;

			indexKey->keyLength = (int) (q - key);

			return ;
			}

		case Timestamp:
		case Date:
			indexKey->appendNumber(value->getDate().getDouble());
			break;

		case TimeType:
			indexKey->appendNumber(value->getTime().getDouble());
			break;

		default:
			indexKey->appendNumber(value->getDouble());
		}
}

void Index::makeKey(int count, Value **values, IndexKey *indexKey)
{
	if (damaged)
		damageCheck();

// This causes a different keylength than other makeKey()s 
// when the key is null.  This section is not needed.
//	if (!count)
//		{
//		indexKey->keyLength = 0;
//		
//		return;
//		}

	if (numberFields == 1)
		{
		makeKey(fields[0], values[0], 0, indexKey);
		
		return;
		}

	uint p = 0, q = 0;
	int n;
	UCHAR *key = indexKey->key;

	for (n = 0; (n < count) && values[n]; ++n)
		{
		Field *field = fields[n];
		char padByte = PAD_BYTE(field);

		while (p % RUN != 0)
			key[p++] = padByte;
		
		IndexKey tempKey(this);
		makeKey(field, values[n], n, &tempKey);
		int length = tempKey.keyLength;
		UCHAR *t = tempKey.key;
		
		// All segments before the last one are padded to the nearest RUN length.

		if (n < count - 1)
			q = (length + RUN - 1) / (RUN - 1) * RUN;
		else
			q = (length * RUN / (RUN - 1)) + (length % (RUN -1));
		
		if (p + q > MAX_INDEX_KEY_RUN_LENGTH)
			throw SQLError (INDEX_OVERFLOW, "maximum index key length exceeded");
			
		for (int i = 0; i < length; ++i)
			{
			if (p % RUN == 0)
				key[p++] = SEGMENT_BYTE(n, numberFields);

			key[p++] = t[i];
			}
		}

	indexKey->keyLength = p;
}


void Index::deleteIndex(Transaction *transaction)
{
	if (!damaged && indexId != -1)
		{
		dbb->deleteIndex(indexId, indexVersion, TRANSACTION_ID(transaction));
		indexId = -1;
		}
}

Bitmap* Index::scanIndex(IndexKey* lowKey, IndexKey* highKey, int searchFlags, Transaction *transaction, Bitmap *bitmap)
{
	if (damaged)
		damageCheck();

	ASSERT (indexId != -1);
	//Bitmap *bitmap = new Bitmap;

	if (bitmap)
		bitmap->clear();
	else
		bitmap = new Bitmap;

	// Use the DIHash if we can.

	if (   (database->configuration->useDeferredIndexHash)
		&& (lowKey) && (lowKey == highKey)
		&& INDEX_IS_UNIQUE(type)
		&& (DIHashTableCounts))
		{
		scanDIHash(lowKey, searchFlags, bitmap);
		}

	else if (deferredIndexes.first)
		{
		Sync sync(&deferredIndexes.syncObject, "Index::scanIndex");
		sync.lock(Shared);
		
		if (transaction)
			{
			for (DeferredIndex *deferredIndex = deferredIndexes.first; deferredIndex; deferredIndex = deferredIndex->next)
				{
				if (transaction->visible(deferredIndex->transaction, deferredIndex->transactionId, FOR_WRITING))
					{
					deferredIndex->scanIndex(lowKey, highKey, searchFlags, bitmap);
					
					if (transaction->database->dbb->debug & (DEBUG_KEYS & DEBUG_DEFERRED_INDEX))
						deferredIndex->print();
					}
				}
			}
		else
			for (DeferredIndex *deferredIndex = deferredIndexes.first; deferredIndex; deferredIndex = deferredIndex->next)
				deferredIndex->scanIndex(lowKey, highKey, searchFlags, bitmap);
		}
	
	if (partialLengths)
		searchFlags |= Partial;
		
	//database->scanIndex (indexId, indexVersion, lowKey, highKey, searchFlags, bitmap);
	
	if (rootPage == 0)
		getRootPage();
		
	switch (indexVersion)
		{
		case INDEX_VERSION_0:
			Index2RootPage::scanIndex (dbb, indexId, rootPage, lowKey, highKey, searchFlags, NO_TRANSACTION, bitmap);
			break;
		
		case INDEX_VERSION_1:
			IndexRootPage::scanIndex (dbb, indexId, rootPage, lowKey, highKey, searchFlags, NO_TRANSACTION, bitmap);
			break;
		
		default:
			ASSERT(false);
		}
	
	if (transaction)
		transaction->scanIndexCount++;
	return bitmap;
}

void Index::setIndex(int32 id)
{
	indexId = id;
}


void Index::loadFields()
{
	Sync sync (&database->syncSysConnection, "Index::loadFields");
	sync.lock (Shared);

	memset (fields, 0, sizeof (Field*) * numberFields);
	const char *sql = (database->fieldExtensions) ?
		"select field,position,partial,records_per_value from IndexFields where indexName=? and schema=? and tableName=?" :
		"select field,position,0,0 from IndexFields where indexName=? and schema=? and tableName=?";
		
	PreparedStatement *statement = database->prepareStatement(sql);
	statement->setString (1, name);
	statement->setString (2, table->schemaName);
	statement->setString (3, table->getName());
	ResultSet *set = statement->executeQuery();

	while (set->next())
		{
		int position = set->getInt (2);
		fields[position] = table->findField (set->getSymbol(1));
		recordsPerSegment[position] = set->getInt(4);
		setPartialLength(position, set->getInt(3));
		}

	set->close();
	statement->close();
	
	if (INDEX_IS_UNIQUE(type))
		recordsPerSegment[numberFields - 1] = 1;

	for (int n = 0; n < numberFields; ++n)
		{
		if (!fields[n])
			throw SQLError (RUNTIME_ERROR, "Can't find field %d of index %s of table %s.%s\n",
							n, (const char*) name, table->schemaName, table->name);
						
		if (type == PrimaryKey)
			fields[n]->setNotNull();
		}
}

void Index::update(Record * oldRecord, Record * record, Transaction *transaction)
{
	// Get key value
				
	IndexKey key(this);
	makeKey (record, &key);

	// If there is a duplicate in the old version chain, don't both with another

	if (duplicateKey (&key, oldRecord))
		return;

	// Update index

	//bool ret = database->addIndexEntry (indexId, &key, record->recordNumber, transaction);
	insert(&key, record->recordNumber, transaction);
}

void Index::makeKey(Record * record, IndexKey *key)
{
	if (damaged)
		damageCheck();

	ASSERT (indexId != -1);
	Value *values[MAX_KEY_SEGMENTS];
	Value vals[MAX_KEY_SEGMENTS];

	for (int n = 0; n < numberFields; ++n)
		{
		Field *field = fields[n];
		Value *value = values[n] = vals + n;
		record->getValue (field->id, value);
		}
		
	makeKey (numberFields, values, key);
}

void Index::garbageCollect(Record * leaving, Record * staying, Transaction *transaction, bool quiet)
{
	int n = 0;
	
	for (Record *record = leaving; record && record != staying; record = record->getPriorVersion(), ++n)
		if (record->hasRecord() && record->recordNumber >= 0)
			{
			IndexKey key(this);
			makeKey (record, &key);

			if (!duplicateKey(&key, record->getPriorVersion()) && !duplicateKey (&key, staying))
				{
				bool hit = false;
				
				if (deferredIndexes.first)
					{
					Sync sync(&deferredIndexes.syncObject, "Index::garbageCollect");
					sync.lock(Shared);
					
					for (DeferredIndex *deferredIndex = deferredIndexes.first; deferredIndex; deferredIndex = deferredIndex->next)
						if (deferredIndex->deleteNode(&key, record->recordNumber))
							hit = true;
					}
				
				if (dbb->deleteIndexEntry(indexId, indexVersion, &key, record->recordNumber, TRANSACTION_ID(transaction)))
					hit = true;
				
				if (!hit && !quiet)
					{
					/***
					Log::log("Index deletion failed for record %d.%d of %s.%s.%s\n", 
							 record->recordNumber, n, table->schemaName, table->name, (const char*) name);
					***/
					//int prevDebug = dbb->debug
					//dbb->debug = DEBUG_PAGES | DEBUG_KEYS;
					dbb->deleteIndexEntry(indexId, indexVersion, &key, record->recordNumber, TRANSACTION_ID(transaction));
					//dbb->debug = prevDebug ;
					}
				}
			}

}

bool Index::duplicateKey(IndexKey *key, Record * record)
{
	for (Record *oldie = record; oldie; oldie = oldie->getPriorVersion())
		if (oldie->hasRecord())
			{
			IndexKey oldKey(this);
			makeKey (oldie, &oldKey);

			if (oldKey.isEqual(key))
				return true;
			}

	return false;
}

JString Index::getTableName(Database *database, const char *schema, const char *indexName)
{
	Sync sync (&database->syncSysConnection, "Index::getTableName");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"select tableName from system.indexes where schema=? and indexName=?");
	int n = 1;
	statement->setString (n++, schema);
	statement->setString (n++, indexName);
	ResultSet *resultSet = statement->executeQuery();
	JString tableName;

	while (resultSet->next())
		tableName = resultSet->getSymbol (1);

	resultSet->close();
	statement->close();

	return tableName;
}

void Index::deleteIndex(Database *database, const char *schema, const char *indexName)
{
	Sync sync (&database->syncSysConnection, "Index::deleteIndex");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"delete from system.indexes where schema=? and indexName=?");
	int n = 1;
	statement->setString (n++, schema);
	statement->setString (n++, indexName);
	statement->executeUpdate();
	statement->close();

	statement = database->prepareStatement (
		"delete from system.indexfields where schema=? and indexName=?");
	n = 1;
	statement->setString (n++, schema);
	statement->setString (n++, indexName);
	statement->executeUpdate();
	statement->close();
}

bool Index::changed(Record *record1, Record *record2)
{
	if (record2->state == recLock)
		//return false;
		record2 = record2->getPriorVersion();

	Value value1, value2;
	
	for (int n = 0; n < numberFields; ++n)
		{
		Field *field = fields[n];
		record1->getValue (field->id, &value1);
		record2->getValue (field->id, &value2);
		
		if (value1.compare (&value2) != 0)
			return true;
		}

	return false;
}

void Index::rebuildIndex(Transaction *transaction)
{
	if (damaged)
		damageCheck();

	Sync sync (&database->syncSysConnection, "Index::rebuildIndex");
	sync.lock (Shared);
	int oldId = indexId;
	indexId = dbb->createIndex(TRANSACTION_ID(transaction));

	PreparedStatement *statement = database->prepareStatement (
		"update system.indexes set indexId=? where indexName=? and schema=? and tableName=?");
	int n = 1;
	statement->setInt (n++, INDEX_COMPOSITE (indexId, indexVersion));
	statement->setString (n++, name);
	statement->setString (n++, table->schemaName);
	statement->setString (n++, table->name);
	n = statement->executeUpdate();
	statement->close();

	if (n != 1)
		throw SQLEXCEPTION (DDL_ERROR, "couldn't update system.indexs for %s", (const char*) name);

	if (oldId != indexId)
		dbb->deleteIndex(oldId, indexVersion, TRANSACTION_ID(transaction));
}

void Index::setDamaged()
{
	savePending = false;
	damaged = true;
}

void Index::damageCheck()
{
	if (damaged)
		throw SQLEXCEPTION (RUNTIME_ERROR, "Index %s on table %s.%s is damaged",
							(const char*) name, table->schemaName, table->name);
}

bool Index::isMember(Field *field)
{
	for (int n = 0; n < numberFields; ++n)
		if (fields[n] == field)
			return true;

	return false;
}

void Index::rename(const char* newName)
{
	for (const char **tableName = tables; *tableName; ++tableName)
		{
		char sql[256];
		snprintf(sql, sizeof(sql), "update system.%s set indexName=? where schema=? and indexName=?", *tableName);
		PreparedStatement *statement = database->prepareStatement(sql);
		statement->setString(1, newName);
		statement->setString(2, table->schemaName);
		statement->setString(3, name);
		statement->executeUpdate();
		statement->close();
		}
	
	name = newName;	
}

const char* Index::getIndexName()
{
	return name;
}

const char* Index::getSchemaName()
{
	return table->schemaName;
}

int Index::getPartialLength(int segment)
{
	if (!partialLengths)
		return 0;
	
	return partialLengths[segment];
}

void Index::setPartialLength(int segment, uint partialLength)
{
	if (partialLength > MAX_INDEX_KEY_LENGTH)
		partialLength = MAX_INDEX_KEY_LENGTH;

	if (!partialLengths)
		{
		if (partialLength == 0)
			return;

		partialLengths = new int[numberFields];
		memset(partialLengths, 0, numberFields * sizeof(partialLengths[0]));
		}
	
	partialLengths[segment] = partialLength;
}

UCHAR Index::getPadByte(int index)
{
	return fields[index]->indexPadByte;
}

UCHAR Index::getPadByte(void)
{
	return fields[numberFields - 1]->indexPadByte;
}

void Index::detachDeferredIndex(DeferredIndex *deferredIndex)
{
	Sync sync(&deferredIndexes.syncObject, "Index::detachDeferredIndex");
	sync.lock(Exclusive);
	deferredIndexes.remove(deferredIndex);
	sync.unlock();

	if (   (database->configuration->useDeferredIndexHash)
		&& (INDEX_IS_UNIQUE(type)))
		{
		Sync syncHash(&syncDIHash, "Index::detachDeferredIndex");
		syncHash.lock(Exclusive);
		Sync syncDI(&deferredIndex->syncObject, "Index::detachDeferredIndex");
		syncDI.lock(Exclusive);

		DeferredIndexWalker walker(deferredIndex, NULL);

		for (DINode *node; (node = walker.next());)
			{
			DIUniqueNode *uniqueNode = UNIQUE_NODE(node);
			removeFromDIHash(uniqueNode);
			}
		}

}

int Index::getRootPage(void)
{
	rootPage = IndexRootPage::getIndexRoot(dbb, indexId);
	
	return rootPage;
}

void Index::checkMaxKeyLength(void)
{
	Field *fld;
	int sumKeyLen = 0;
	int len;
	int maxKeyLen = database->getMaxKeyLength() * RUN / (RUN - 1);

	// All but the last field will be padded to the nearest RUN length
	
	for (int s = 0; s < numberFields; s++)
		{
		fld = fields[s];
		
		if ((fld->type == String) || (fld->type == Varchar) || (fld->type == Char))
			{
			len = getPartialLength(s);
			
			if (len == 0)
				len = fld->length;
			}
		else
			len = MAX(fld->length,8); // numbers can take up to 8 bytes encoded by IndexKey::appendNumber().

		if (s == numberFields - 1)
			sumKeyLen += (len  * RUN) / (RUN - 1) + (len % (RUN -1));
		else
			sumKeyLen += (len + RUN - 1) / (RUN - 1) * RUN;
		}

	if (sumKeyLen > maxKeyLen)
		{
		Log::log("Maximum key length can be exceeded on index");

		throw SQLEXCEPTION (INDEX_OVERFLOW, "Maximum key length can be exceeded on index %s on table %s.%s",
							(const char*) name, table->schemaName, table->name);
		}
}

void Index::optimize(uint64 cardinality, Connection *connection)
{
	PStatement update = database->prepareStatement(
		"update indexfields set records_per_value=? where schema=? and indexname=? and field=?");
	update->setString(2, table->schemaName);
	update->setString(3, name);
	JString keys;
	Statement *statement = connection->createStatement();
	
	for (int n = 0; n < numberFields; ++n)
		{
		Field *field = fields[n];
		uint64 count = 0;
		
		if ((n == numberFields - 1) && (type & (PrimaryKey | UniqueIndex)))
			count = 1;
		else
			{
			if (n > 0)
				keys += ",";
			
			keys += field->name;
			JString sql;
			sql.Format("select count(*) from (select distinct %s from %s.%s)", 
					   (const char*) keys, (const char*) table->schemaName, (const char*) table->name);
			RSet resultSet = statement->executeQuery(sql);
			
			if (resultSet->next())
				count = resultSet->getLong(1);
			}
		
		uint64 records = count ? cardinality / count : 0;
		recordsPerSegment[n] = records;
		update->setLong(1, records);
		update->setString(4, field->name);
		update->executeUpdate();
		}
	
	statement->close();	
}

// The table is borrowed from a zlib crc table.

static const int32 somenums[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

// Using prime numbers for hash sizes.
//*  static const int hashTableSizes[MAX_DI_HASH_TABLES] = {11, 101, 1009, 10007, 100003};
static const int hashTableSize = 1009;

//*  uint32 Index::hash(UCHAR *buf, int len, uint hashTable)
uint32 Index::hash(UCHAR *buf, int len)
{
	uint32 somenum = 0;
	UCHAR *bufEnd = buf + len;

	while (buf < bufEnd)
		somenum = somenums[((int) somenum ^ (*buf++)) & 0xff] ^ (somenum >> 8);

//*	ASSERT (hashTable <= curHashTable);
//*	return (somenum % hashTableSizes[hashTable]);
	return (somenum % hashTableSize);
}

void Index::addToDIHash(struct DIUniqueNode *uniqueNode)
{
//	if (DIHashTables[curHashTable] == NULL)
	if (DIHashTable == NULL)
		{
//*		uint hashSize = hashTableSizes[curHashTable];
//*		DIHashTables[curHashTable] = new DIUniqueNode* [hashSize];
//*		memset(DIHashTables[curHashTable], 0, sizeof(DIUniqueNode*) * hashSize);
		DIHashTable = new DIUniqueNode* [hashTableSize];
		memset(DIHashTable, 0, sizeof(DIUniqueNode*) * hashTableSize);
		}

//*	DIUniqueNode **DIHashTable = DIHashTables[curHashTable];
	DINode *node = &uniqueNode->node;
//*	int slot = hash(node->key, node->keyLength, curHashTable);
	int slot = hash(node->key, node->keyLength);
	uniqueNode->collision = DIHashTable[slot];
	DIHashTable[slot] = uniqueNode;

//*	DIHashTableCounts[curHashTable]++;
//*	if (uniqueNode->collision == NULL)
//*		DIHashTableSlotsUsed[curHashTable]++;
	DIHashTableCounts++;
	if (uniqueNode->collision == NULL)
		DIHashTableSlotsUsed++;

//* Make a larger hash table when this one is 'full'.
//*	if ((DIHashTableCounts[curHashTable] > hashTableSizes[curHashTable] * 2) &&
//*		(curHashTable < MAX_DI_HASH_TABLES - 1))
//*		curHashTable++;

}

void Index::removeFromDIHash(struct DIUniqueNode *uniqueNode)
{
	// Assume caller got an exclusive lock on syncDIHash

	if (DIHashTable == NULL)
		return;

	DINode *node = &uniqueNode->node;
	
//*	for (int h = curHashTable; h >= 0; h--)
		{
//*		DIUniqueNode **DIHashTable = DIHashTables[h];
//*		if (DIHashTable == NULL)
//*			continue;

//*		int slot = hash(node->key, node->keyLength, h);
		int slot = hash(node->key, node->keyLength);

		for (DIUniqueNode **uNode = &DIHashTable[slot];
			 uNode; 
			 uNode = &(*uNode)->collision)
			{
			if (*uNode == uniqueNode)
				{
				*uNode = uniqueNode->collision;
				uniqueNode->collision = NULL;

//*				DIHashTableCounts[h]--;
//*				if (DIHashTable[slot] == NULL)
//*					DIHashTableSlotsUsed[h]--;
				DIHashTableCounts--;
				if (DIHashTable[slot] == NULL)
					DIHashTableSlotsUsed--;

				// Delete empty smaller hash tables.
//*				if ((DIHashTableCounts[h] <= 0) &&
//*					(h < MAX_DI_HASH_TABLES - 1))
//*					{
//*					for (slot = 0; slot < hashTableSizes[h]; slot++)
//*						if (DIHashTable[slot] != NULL)
//*							ASSERT(DIHashTable[slot] == NULL);

//*					delete[] DIHashTables[h];
//*					DIHashTables[h] = NULL;
//*					}

				return;
				}
			}
		}

	ASSERT(false);   // Did not find it in any hash table
}

void Index::scanDIHash(IndexKey* scanKey, int searchFlags, Bitmap *bitmap)
{
	Sync sync(&syncDIHash, "Index::scanDIHash");
	sync.lock(Shared);
	bool isPartial = (searchFlags & Partial) == Partial;

//*	for (int h = curHashTable; h >= 0; h--)
		{
//*		DIUniqueNode **hashTable = DIHashTables[h];
		DIUniqueNode **hashTable = DIHashTable;
		if (hashTable)
			{
			uint slot = hash(scanKey->key, scanKey->keyLength /*, h*/);

			for (DIUniqueNode *uNode = hashTable[slot]; uNode; uNode = uNode->collision)
				{
				DINode *node = &uNode->node;
				if (scanKey->compareValues(node->key, node->keyLength, isPartial) == 0)
					{
					bitmap->set(node->recordNumber);
					return;
					}
				}
			}
		}
}
//* These commented lines implement multiple DI hash sizes.
