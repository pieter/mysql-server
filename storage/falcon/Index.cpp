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

	if (!count)
		{
		indexKey->keyLength = 0;
		
		return;
		}

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
		
	if (deferredIndexes.first)
		{
		Sync sync(&deferredIndexes.syncObject, "Index::scanIndex");
		sync.lock(Shared);
		
		if (transaction)
			{
			for (DeferredIndex *deferredIndex = deferredIndexes.first; deferredIndex; deferredIndex = deferredIndex->next)
				{
				if (transaction->visible(deferredIndex->transaction, deferredIndex->transactionId))
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
	
	if (type & (PrimaryKey | UniqueIndex))
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
