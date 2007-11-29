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

#include "Engine.h"
#include "StorageTable.h"
#include "StorageTableShare.h"
#include "StorageConnection.h"
#include "StorageDatabase.h"
#include "Sync.h"
#include "Bitmap.h"
#include "Index.h"
#include "SQLException.h"
#include "Record.h"
#include "Table.h"
#include "Field.h"
#include "Value.h"
#include "SQLException.h"
#include "MySQLCollation.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

StorageTable::StorageTable(StorageConnection *connection, StorageTableShare *tableShare)
{
	storageConnection = connection;
	
	if (storageConnection)
		storageConnection->addRef();
		
	storageDatabase = (storageConnection) ? storageConnection->storageDatabase : NULL;
	share = tableShare;
	name = share->name;
	currentIndex = NULL;
	bitmap = NULL;
	upperBound = lowerBound = NULL;
	record = NULL;
	recordLocked = false;
	haveTruncateLock = false;
	syncTruncate = new SyncObject;
	syncTruncate->setName("StorageTable::syncTruncate");
}

StorageTable::~StorageTable(void)
{
	clearTruncateLock();
	delete syncTruncate;
	
	if (bitmap)
		((Bitmap*) bitmap)->release();

	if (record)
		clearRecord();
		
	storageConnection->remove(this);
}

int StorageTable::create(const char* sql, int64 autoIncrementValue)
{
	return share->create(storageConnection, sql, autoIncrementValue);
}

int StorageTable::open(void)
{
	return share->open();
}

int StorageTable::deleteTable(void)
{
	clearTruncateLock();
	
	int ret = share->deleteTable(storageConnection);
	
	if (ret == 0)
		share = NULL;
		
	return ret;
}

int StorageTable::truncateTable(void)
{
	Sync sync(syncTruncate, "StorageTable::truncateTable");
	sync.lock(Exclusive);
	
	clearRecord();
	int ret = share->truncateTable(storageConnection);
	
	return ret;
}

void StorageTable::clearTruncateLock(void)
{
	if (haveTruncateLock)
		{
		syncTruncate->unlock();
		haveTruncateLock = false;
		}
}

void StorageTable::setTruncateLock()
{
	if (!haveTruncateLock)
		{
		syncTruncate->lock(NULL, Shared);
		haveTruncateLock = true;
		}
}

int StorageTable::insert(void)
{
	try
		{
		int ret = storageDatabase->insert(storageConnection->connection, share->table, &insertStream);
		insertStream.clear();
		
		return ret;
		}
	catch (SQLException& exception)
		{
		insertStream.clear();
		
		return translateError(&exception, StorageErrorDupKey);
		}
}

int StorageTable::deleteRow(int recordNumber)
{
	return storageDatabase->deleteRow(storageConnection, share->table, recordNumber);
}

int StorageTable::updateRow(int recordNumber)
{
	try
		{
		ASSERT(record->useCount >= 2);
		storageDatabase->updateRow(storageConnection, share->table, record, &insertStream);
		}
	catch (SQLException& exception)
		{
		insertStream.clear();
		
		return translateError(&exception, StorageErrorRecordNotFound);
		}
	
	return 0;
}

int StorageTable::next(int recordNumber, bool lockForUpdate)
{
	recordLocked = false;

	int ret = storageDatabase->nextRow(this, recordNumber, lockForUpdate);

	return ret;
}

int StorageTable::nextIndexed(int recordNumber, bool lockForUpdate)
{
	recordLocked = false;

	int ret = storageDatabase->nextIndexed(this, bitmap, recordNumber, lockForUpdate);

	return ret;
}

int StorageTable::fetch(int recordNumber, bool lockForUpdate)
{
	recordLocked = false;

	int ret = storageDatabase->fetch(storageConnection, this, recordNumber, lockForUpdate);

	return ret;
}

void StorageTable::transactionEnded(void)
{
}

int StorageTable::setIndex(int numberIndexes, int indexId, StorageIndexDesc* storageIndex)
{
	if (!(currentIndex = share->getIndex(numberIndexes, indexId, storageIndex)))
		return StorageErrorNoIndex;

	upperBound = lowerBound = NULL;
	searchFlags = 0;
	
	return 0;
}

int StorageTable::indexScan()
{
	if (!currentIndex)
		return StorageErrorNoIndex;
	
	int numberSegments = (upperBound) ?  upperBound->numberSegments : (lowerBound) ? lowerBound->numberSegments : 0;
	
	if (numberSegments != currentIndex->index->numberFields)
		searchFlags |= Partial;
		
	bitmap = storageDatabase->indexScan(currentIndex->index, lowerBound, upperBound, searchFlags, storageConnection, (Bitmap*) bitmap);
	
	return 0;
}


void StorageTable::clearBitmap(void)
{
	if (bitmap)
		((Bitmap*) bitmap)->clear();
}

int StorageTable::setIndex(int indexId)
{
	if (!(currentIndex = share->getIndex(indexId)))
		return StorageErrorNoIndex;
	
	upperBound = lowerBound = NULL;
	searchFlags = 0;
	
	return 0;
}

void StorageTable::indexEnd(void)
{
	if (bitmap)
		clearBitmap();
}

int StorageTable::setIndexBound(const unsigned char* key, int keyLength, int which)
{
	if (!currentIndex)
		return StorageErrorNoIndex;

	if (which & LowerBound)
		{
		lowerBound = &lowerKey;
		int ret = storageDatabase->makeKey(currentIndex, key, keyLength, lowerBound);
		
		if (ret)
			return ret;
			
		if (which & UpperBound)
			upperBound = lowerBound;
		}
	else if (which & UpperBound)
		{
		upperBound = &upperKey;
		int ret = storageDatabase->makeKey(currentIndex, key, keyLength, upperBound);

		if (ret)
			return ret;
		}
		
	return 0;
}


int StorageTable::isKeyNull(const unsigned char* key, int keyLength)
{
	return storageDatabase->isKeyNull(currentIndex, key, keyLength);
}

int StorageTable::storeBlob(StorageBlob *blob)
{
	return storageDatabase->storeBlob(storageConnection->connection, share->table, blob);
}

void StorageTable::getBlob(int recordNumber, StorageBlob* blob)
{
	return storageDatabase->getBlob(share->table, recordNumber, blob);
}

void StorageTable::release(StorageBlob* blob)
{
	delete blob->data;
}

void StorageTable::deleteStorageTable(void)
{
	delete this;
}

void StorageTable::freeBlob(StorageBlob* blob)
{
	storageDatabase->freeBlob(blob);
}

const char* StorageTable::getName(void)
{
	return name;
}

void StorageTable::setRecord(Record* newRecord, bool locked)
{
	if (record)
		record->release();
	
	record = newRecord;
	recordLocked = locked;
	dataStream.setData((const UCHAR*) record->getEncodedRecord());
}

void StorageTable::clearRecord(void)
{
	if (record)
		{
		record->release();
		record = NULL;
		}
}

void StorageTable::preInsert(void)
{
	insertStream.clear();
	short version = -share->table->getFormatVersion();
	insertStream.putSegment(sizeof(version), (char*) &version, true);
	dataStream.setStream(&insertStream);
}

const UCHAR* StorageTable::getEncoding(int fieldIndex)
{
	return record->getEncoding(fieldIndex);
}

const char* StorageTable::getSchemaName(void)
{
	return share->schemaName;
}

void StorageTable::setConnection(StorageConnection* newStorageConn)
{
	if (bitmap)
		clearBitmap();

	if (record)
		clearRecord();

	if (storageConnection == newStorageConn)
		return;

	if (newStorageConn)
		newStorageConn->addRef();

	if (storageConnection)
		storageConnection->release();

	storageConnection = newStorageConn;
}

void StorageTable::clearIndexBounds(void)
{
	if (bitmap)
		clearBitmap();

	upperBound = lowerBound = NULL;
}

int StorageTable::compareKey(const unsigned char* key, int keyLength)
{
	int segmentNumber = 0;
	StorageIndexDesc *indexDesc = currentIndex;
	Index *index = indexDesc->index;

	if (!index)
		return StorageErrorBadKey;
	
	// Compare key, collating nulls as low
	
	try
		{
		for (const UCHAR *p = key, *end = key + keyLength; p < end && segmentNumber < indexDesc->numberSegments; ++segmentNumber)
			{
			StorageSegment *segment = indexDesc->segments + segmentNumber;
			int nullFlag = (segment->nullBit) ? *p++ : 0;
			Value keyValue;
			int len = storageDatabase->getSegmentValue(segment, p, &keyValue, index->fields[segmentNumber]);
			Field *field = index->fields[segmentNumber];

			if (nullFlag)
				{
				if (!record->isNull(field->id))
					return 1;
				}
			else if (record->isNull(field->id))
				return -1;
			else
				{
				Value fldValue;
				record->getValue(field->id, &fldValue);

				// Why should this matter? Because the server only sent this many characters.
				uint partialLength = index->getPartialLength(segmentNumber);
				int cmp;
				
				if (segment->mysql_charset)
					{
					void *cs = segment->mysql_charset;
					const char *fldString = fldValue.getString();
					const char *keyString = keyValue.getString();
					uint fldLen = fldValue.getStringLength();
					uint keyLen = keyValue.getStringLength();

					if (falcon_cs_is_binary(cs))
						{
						if (partialLength)
							{
							fldLen = MIN(fldLen, partialLength);
							keyLen = MIN(keyLen, partialLength);
							}
						}
					else
						{
/* This is not necessary when comparing with the record using strnncollsp()
						char padChar = falcon_get_pad_char(cs);
						char minSortChar = falcon_get_min_sort_char(cs);
						fldLen = MySQLCollation::computeKeyLength(fldLen, fldString, padChar, minSortChar);
						keyLen = MySQLCollation::computeKeyLength(keyLen, keyString, padChar, minSortChar);
*/
						// Trim it down to the number of bytes that represent
						// the number of characters indicated by partialLength.

						if (partialLength)
							{
							fldLen = falcon_strntrunc(cs, partialLength, fldString, fldLen);
							keyLen = falcon_strntrunc(cs, partialLength, keyString, keyLen);
							}
						}

					cmp = falcon_strnncollsp(cs, fldString, fldLen, 
					                       keyString, keyLen, true);

					//if ((cmp == 0) && (keyLen < fldLen))
					//	cmp = 1;  // The field begins with the string searched for; fldString > keyString. #23962
					}
				else
					{
					if (partialLength)
						{
						fldValue.truncateString(partialLength);
						keyValue.truncateString(partialLength);
						}
					cmp = fldValue.compare(&keyValue);
					}

				if (cmp)
					return cmp;
				}
			
			p += len;
			}

		return 0;
		}
	catch (SQLException& exception)
		{
		return translateError(&exception, StorageErrorDupKey);
		}
}
int StorageTable::translateError(SQLException *exception, int defaultStorageError)
{
		int errorCode;
		int indexId = -1;
		int sqlCode = exception->getSqlcode();

		switch (sqlCode)
			{
			case UNIQUE_DUPLICATE:
				indexId = share->getIndexId(exception->getObjectSchema(), exception->getObjectName());
				errorCode = StorageErrorDupKey;
				break;

			case DEADLOCK:
				indexId = share->getIndexId(exception->getObjectSchema(), exception->getObjectName());
				errorCode = StorageErrorDeadlock;
				break;

			case UPDATE_CONFLICT:
				errorCode = StorageErrorUpdateConflict;
				break;

			case TRUNCATION_ERROR:
				errorCode = StorageErrorTruncation;
				break;

			case INDEX_OVERFLOW:
				errorCode = StorageErrorIndexOverflow;
				break;
				
			case OUT_OF_MEMORY_ERROR:
				errorCode = StorageErrorOutOfMemory;
				break;

			case OUT_OF_RECORD_MEMORY_ERROR:
				errorCode = StorageErrorOutOfRecordMemory;
				break;

			case LOCK_TIMEOUT:
				errorCode = StorageErrorLockTimeout;
				break;

			case DEVICE_FULL:
				errorCode = StorageErrorDeviceFull;
				break;

			default:
				errorCode = defaultStorageError;
			}
			
		storageConnection->setErrorText(exception);

		return (errorCode << StoreErrorIndexShift) | (indexId + 1);
}

void StorageTable::setPartialKey(void)
{
	searchFlags |= Partial;
}

void StorageTable::setReadAfterKey(void)
{
	searchFlags |= AfterLowKey;
}

void StorageTable::setReadBeforeKey(void)
{
	searchFlags |= BeforeHighKey;
}

void StorageTable::clearAlter(void)
{
	share->table->clearAlter();
}

bool StorageTable::setAlter(void)
{
	return share->table->setAlter();
}

int StorageTable::alterCheck(void)
{
	if (share->table->hasUncommittedRecords(NULL))
		{
		storageConnection->setErrorText("table has uncommitted updates");
		
		return StorageErrorUncommittedRecords;
		}

	// This causes a bunch of failures, back out for now
	if (!setAlter())
		return StorageErrorUncommittedUpdates;

	return 0;
}

void StorageTable::unlockRow(void)
{
	if (recordLocked)
		{
		share->table->unlockRecord(record->recordNumber);
		recordLocked = false;
		}
}

int StorageTable::optimize(void)
{
	share->table->optimize(storageConnection->connection);
	
	return 0;
}

void StorageTable::setLocalTable(StorageInterface* handler)
{
	localTable = handler;
}
