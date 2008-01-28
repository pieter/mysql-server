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

// Record.cpp: implementation of the Record class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "Record.h"
#include "RecordVersion.h"
#include "Value.h"
#include "Transaction.h"
#include "Format.h"
#include "Table.h"
#include "Stream.h"
#include "SQLError.h"
#include "AsciiBlob.h"
#include "BinaryBlob.h"
#include "Database.h"
#include "Log.h"
#include "Interlock.h"
#include "EncodedRecord.h"
#include "Field.h"

#undef new

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#define NULL_BYTE(ff)		(2 + ff->nullPosition / 8)
#define NULL_BIT(ff)		(1 << ff->nullPosition % 8)


/***
static int numberRecords;
static int compressedSize;
static int decompressedSize;
static int encodedSize;
static int offsetVectorSize;
***/

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Record::Record(Table *table, int32 recordNum, Stream *stream)
{
	//ASSERT (tbl);
	useCount = 1;
	//table = tbl;
	format = table->getCurrentFormat();
	state = recData;
	recordNumber = recordNum;
	const short *p = (short*) stream->segments->address;
	size = sizeof (*this);
	generation = table->database->currentGeneration;
#ifdef CHECK_RECORD_ACTIVITY
	active = false;
#endif

	if (*p > 0)
		{
		encoding = traditional;
		data.record = stream->decompress(table->tableId, recordNumber);
		format = table->getFormat (*(short*) data.record);
		size += format->length;
		//setAgeGroup();
		
		if (stream->decompressedLength != format->length)
			throw SQLEXCEPTION (RUNTIME_ERROR,
					"wrong length record (got %d, expected %d), table %s.%s, section %d, record %d",
					stream->decompressedLength,
					format->length,
					table->getSchema(),
					table->getName(),
					table->dataSection,
					recordNumber);
		}
	else
		{
		format = table->getFormat (-*p);
		data.record = NULL;
		setEncodedRecord(stream, false);
		}
}

// This constructor is called from the constructor for RecordVersion

Record::Record(Table * tbl, Format *fmt)
{
	ASSERT (tbl);
	useCount = 1;
	
	if ( !(format = fmt) )
		format = tbl->getCurrentFormat();
		
	//table = tbl;
	size = sizeof (RecordVersion);
	encoding = noEncoding;
	state = recData;
	generation = format->table->database->currentGeneration;
	
#ifdef CHECK_RECORD_ACTIVITY
	active = false;
#endif
	
	/*
		Do not allocate a data.record buffer here since every time a 
		Recordversion is created, the record buffer is allocated in setEncodedRecord()
		setAgeGroup() will be called later when data.record is allocated.
	*/
	
	data.record = NULL;
}


Record::~Record()
{
#ifdef CHECK_RECORD_ACTIVITY
	ASSERT(!active);
#endif
	/***
	if (table)
		unsetAgeGroup();
	***/
	
	deleteData();
}

void Record::setValue(Transaction * transaction, int id, Value * value, bool cloneFlag, bool copyFlag)
{
	ASSERT (id < format->maxId);

	switch (encoding)
		{
		case noEncoding:
			if (data.record != NULL)
				DELETE_RECORD (data.record);

			data.record = (char*) NEW Value[format->count];
			encoding = valueVector;
		case valueVector:
			((Value*) data.record)[format->format[id].index].setValue(value, copyFlag);
			return;

		case traditional:
			break;

		default:
			ASSERT(false);
		}

	FieldFormat *ff = format->format + id;
	char *ptr = data.record + ff->offset;

	if (value->isNull ((Type) ff->type))
		{
		data.record [NULL_BYTE (ff)] &= ~NULL_BIT (ff);
		memset(ptr, 0, ff->length);
		
		return;
		}

	switch (ff->type)
		{
		case String:
		case Char:
			{
			int length = value->getString(ff->length, ptr);
			
			if (length < ff->length)
				memset (ptr + length, 0, ff->length - length);
			}
			break;

		case Varchar:
			*(short*) ptr = value->getString (ff->length - 2, ptr + 2);
			break;

		case Short:
			*(short*) ptr = value->getShort(ff->scale);
			break;

		case Long:
			*(int*) ptr = value->getInt(ff->scale);
			break;

		case Quad:
			*(int64*) ptr = value->getQuad(ff->scale);
			break;

		case Double:
			*(double*) ptr = value->getDouble();
			break;

		case Asciiblob:
		case Binaryblob:
			*(int32*) ptr = format->table->getBlobId (value, *(int32*) ptr, cloneFlag, transaction);
			break;

		case Date:
			{
			DateTime date = value->getDate();
			
			if (ff->length == 4)
				*(int32*) ptr = (int32) date.getSeconds();
			else
				*(int64*) ptr = date.getMilliseconds();
			}
			break;

		case TimeType:
			{
			Time date = value->getTime();
			
			if (ff->length == 4)
				*(int32*) ptr = (int32) date.getSeconds();
			else
				*(int64*) ptr = date.getMilliseconds();
			}
			break;

		case Timestamp:
			{
			TimeStamp timestamp = value->getTimestamp();
			
			if (ff->length == 4)
				*(int32*) ptr = (int32) timestamp.getSeconds();
			else if (ff->length == 8)
				{
				*(int32*) ptr = (int32) timestamp.getSeconds();
				*(int32*) (ptr + sizeof (int32)) = timestamp.getNanos();
				}
			else if (ff->length == 12)
				{
				*(int64*) ptr = timestamp.getMilliseconds();
				*(int32*) (ptr + sizeof (int64)) = timestamp.getNanos();
				}
			else
				ASSERT (false);
			}
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}

	data.record [NULL_BYTE (ff)] |= NULL_BIT (ff);
}

int Record::getFormatVersion()
{
	switch (encoding)
		{
		case traditional:
			return *(short*) data.record;

		case shortVector:
			return -*(short*) (data.record + ((USHORT*) data.record)[0] - sizeof(short));

		default:
			NOT_YET_IMPLEMENTED;
		}

	return 0;
}

Record* Record::fetchVersion(Transaction * transaction)
{
	return this;
}

void Record::getValue(int fieldId, Value * value)
{
	getRawValue(fieldId, value);

	switch (value->getType())
		{
		case Asciiblob:
			{
			AsciiBlob *blob = format->table->getAsciiBlob(value->getBlobId());
			value->setValue (blob);
			blob->release();
			}
			break;

		case Binaryblob:
			{
			BinaryBlob *blob = format->table->getBinaryBlob(value->getBlobId());
			value->setValue (blob);
			blob->release();
			}
			break;

		default:
			break;
		}
}

void Record::getRawValue(int fieldId, Value * value)
{
	//ASSERT (table);
	ASSERT (format);
	value->clear();

	if (!this)
		return;

	//ASSERT (fieldId <= format->maxId);

	if (fieldId >= format->maxId)
		return;

	// If chilled, restore the record data from the serial log
	
	if (state == recChilled)
		thaw();
	
	// If this is an encoded record, parse through the fields

	switch (encoding)
		{
		case byteVector:
		case shortVector:
		case longVector:
			getEncodedValue(fieldId, value);
			return;

		case valueVector:
			value->setValue(((Value*) data.record) + format->format[fieldId].index, false);
			return;

		case traditional:
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}

	FieldFormat *ff = format->format + fieldId;

	if (!(data.record [NULL_BYTE (ff)] & NULL_BIT (ff)))
		return;

	char *ptr = data.record + ff->offset;

	switch (ff->type)
		{
		case String:
			value->setString(ptr, false);
			break;

		case Char:
			value->setString(ff->length, ptr, false);
			break;

		case Varchar:
			value->setString(*(short*) ptr, ptr + 2, false);
			break;

		case Short:
			value->setValue(*(short*) ptr, ff->scale);
			break;

		case Long:
			value->setValue(*(int*) ptr, ff->scale);
			break;

		case Quad:
			value->setValue(*(int64*) ptr, ff->scale);
			break;

		case Timestamp:
			{
			TimeStamp timestamp;
			
			if (ff->length == 4)
				{
				timestamp.setSeconds (*(int32*) ptr);
				timestamp.setNanos (0);
				}
			else if (ff->length == 8)
				{
				timestamp.setSeconds (*(int32*) ptr);
				timestamp.setNanos (*(int32*) (ptr + sizeof (int32)));
				}
			else
				{
				timestamp.setMilliseconds (*(int64*) ptr);
				timestamp.setNanos (*(int32*) (ptr + sizeof (int64)));
				}
				
			value->setValue (timestamp);
			}
			break;

		case Date:
			{
			DateTime date;
			
			if (ff->length == sizeof (int32))
				date.setSeconds (*(int32*) ptr);
			else
				date.setMilliseconds (*(int64*) ptr);
				
			value->setValue (date);
			}
			break;

		case TimeType:
			{
			Time date;
			
			if (ff->length == sizeof (int32))
				date.setSeconds (*(int32*) ptr);
			else
				date.setMilliseconds (*(int64*) ptr);
				
			value->setValue (date);
			}
			break;

		case Double:
			value->setValue (*(double*) ptr);
			break;

		case Asciiblob:
			{
			//AsciiBlob *blob = format->table->getAsciiBlob (*(int32*) ptr);
			//value->setValue (blob);
			//blob->release();
			value->setAsciiBlob(*(int32*) ptr);
			}
			break;

		case Binaryblob:
			{
			//BinaryBlob *blob = format->table->getBinaryBlob (*(int32*) ptr);
			//value->setValue (blob);
			//blob->release();
			value->setBinaryBlob(*(int32*) ptr);
			}
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}
}

bool Record::isVersion()
{
	return false;
}


bool Record::scavenge(RecordScavenge *recordScavenge)
{
	return true;
}

void Record::scavenge(TransId targetTransactionId, int oldestActiveSavePointId)
{

}

Record* Record::getPriorVersion()
{
	return NULL;
}

void Record::setSuperceded(bool flag)
{

}

Transaction* Record::getTransaction()
{
	return NULL;
}

bool Record::isSuperceded()
{
	return false;
}

void Record::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void Record::release()
{
	ASSERT (useCount > 0);

	if (INTERLOCKED_DECREMENT (useCount) == 0)
		{
		/*** debugging only
		int cnt = useCount;
		Table *t = table;
		const char *tableName = table->name;
		***/
		delete this;
		}
}

int Record::getBlobId(int fieldId)
{
	if (fieldId >= format->maxId)
		return -1;

	switch (encoding)
		{
		case traditional:
			{
			FieldFormat *ff = format->format + fieldId;

			if (!(data.record [NULL_BYTE (ff)] & NULL_BIT (ff)))
				return -1;

			if (ff->type != Asciiblob && ff->type != Binaryblob)
				return -1;

			return *(int32*) (data.record + ff->offset);
			}

		case byteVector:
		case shortVector:
		case longVector:
			{
			Value value;
			getEncodedValue(fieldId, &value);

			if (value.getType() == Asciiblob || value.getType() == Binaryblob)
				return value.getBlobId();

			return -1;
			}
		
		default:
			NOT_YET_IMPLEMENTED;
		}

	return -1;
}


TransId Record::getTransactionId()
{
	return 0;
}

int Record::getSavePointId()
{
	return 0;
}

void Record::getRecord(Stream *stream)
{
	switch (encoding)
		{
		case traditional:
			stream->compress (format->length, data.record);
			break;

		case shortVector:
			stream->putSegment(getEncodedSize(),
							   data.record + ((USHORT*) data.record)[0] - sizeof(short), false);
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}
}

int Record::getEncodedSize()
{
	switch (encoding)
		{
		case traditional:
			return format->length;

		case shortVector:
			return size - format->count * sizeof(USHORT) - ((isVersion()) ? sizeof(RecordVersion) : sizeof(Record));

		case noEncoding:
			if (!data.record)
				return 0;
		default:
			NOT_YET_IMPLEMENTED;
			
			return 0;
		}
}

void Record::getEncodedValue(int fieldId, Value *value)
{
	// If chilled, restore the record data from the serial log
	
	if (state == recChilled)
		thaw();

	switch (encoding)
		{
		case shortVector:
			{
			int index = format->format[fieldId].index;
			USHORT *vector = (USHORT*) data.record;

			if (highWater < index)
				{
				const UCHAR *p = (UCHAR*) data.record + vector[highWater];

				while (highWater < index)
					{
					const UCHAR *q = EncodedDataStream::skip(p);
					vector[++highWater] = (USHORT) (q - (UCHAR*) data.record);
					p = q;
					}
				}

			ASSERT(vector[index] < size);
			const UCHAR *q = EncodedDataStream::decode((UCHAR*) data.record + vector[index], value, false);

			if (++index < format->count && highWater < index)
				vector[++highWater] = (USHORT) (q - (UCHAR*) data.record);

			return;
			}

		default:
			NOT_YET_IMPLEMENTED;
		}
}

void Record::finalize(Transaction *transaction)
{
	ASSERT(encoding == valueVector);
	Stream stream;
	EncodedRecord encodedStream(format->table, transaction, &stream);
	short version = (short) - format->version;
	stream.putSegment(sizeof(version), (char*) &version, true);
	Value *values = (Value*) data.record;
	data.record = NULL;

	for (int n = 0; n < format->maxId; ++n)
		{
		FieldFormat *fld = format->format + n;

		if (fld->offset == 0)
			continue;

		Value *value = values + fld->index;
		encodedStream.encode(fld->type, value);
		}

	setEncodedRecord(&stream, false);
	delete [] values;
}

int Record::setEncodedRecord(Stream *stream, bool interlocked)
{
	if (data.record)
		{
		if (encoding == valueVector)
			delete [] (Value*) data.record;
		else
			DELETE_RECORD (data.record);
		
		data.record = NULL;
		}

	encoding = shortVector;
	int vectorLength = format->count * sizeof(short);
	int totalLength = vectorLength + stream->totalLength;
	char *dataBuffer = allocRecordData(totalLength);
	memset(dataBuffer, 0, vectorLength);
	stream->getSegment(0, stream->totalLength, dataBuffer + vectorLength);
	((USHORT*)dataBuffer)[0] = (USHORT) (vectorLength + sizeof(short));
	
	highWater = 0;
	size +=  vectorLength + stream->totalLength;
	//setAgeGroup();
	generation = format->table->database->currentGeneration;

	if (interlocked)
		{
		char **ptr = &data.record;
		
		// If data.record has changed since allocating the new buffer, then free the new buffer
		
		if (!COMPARE_EXCHANGE_POINTER(ptr, NULL, dataBuffer))
			{
			DELETE_RECORD(dataBuffer);
			totalLength = 0;
			}
		}
	else
		data.record = dataBuffer;

	return totalLength;
}

const char* Record::getEncodedRecord()
{
	// If chilled, restore the record data from the serial log
	
	if (state == recChilled)
		thaw();

	ASSERT(encoding == shortVector);

	return data.record + ((USHORT*) data.record)[0];
}

const UCHAR* Record::getEncoding(int index)
{
	switch (encoding)
		{
		case shortVector:
			{
			USHORT *vector = (USHORT*) data.record;

			if (highWater < index)
				{
				const UCHAR *p = (UCHAR*) data.record + vector[highWater];

				while (highWater < index)
					{
					const UCHAR *q = EncodedDataStream::skip(p);
					vector[++highWater] = (USHORT) (q - (UCHAR*) data.record);
					p = q;
					}
				}

			return (UCHAR*) data.record + vector[index];
			}

		default:
			NOT_YET_IMPLEMENTED;
			return NULL;
		}
}

bool Record::isNull(int fieldId)
{
	if (fieldId >= format->maxId)
		return true;

	// If this is an encoded record, parse through the fields

	switch (encoding)
		{
		case shortVector:
			{
			int index = format->format[fieldId].index;
			USHORT *vector = (USHORT*) data.record;

			if (highWater < index)
				{
				const UCHAR *p = (UCHAR*) data.record + vector[highWater];

				while (highWater < index)
					{
					const UCHAR *q = EncodedDataStream::skip(p);
					vector[++highWater] = (USHORT) (q - (UCHAR*) data.record);
					p = q;
					}
				}

			const UCHAR *q = (UCHAR*) data.record + vector[index];

			return *q == edsNull;
			}

		case valueVector:
			return ((Value**) data.record)[format->format[fieldId].index]->isNull();

		case traditional:
			{
			FieldFormat *ff = format->format + fieldId;

			return (data.record [NULL_BYTE (ff)] & NULL_BIT (ff)) ? true : false;
			}
			
		case byteVector:
		case longVector:
		default:
			NOT_YET_IMPLEMENTED;
		}
	
	return true;
}

// Set age group and add size to database

/***
void Record::setAgeGroup()
{
	Database *database = table->database;
	generation = database->currentGeneration;
	//database->ageGroupSizes [0] += size;
	INTERLOCKED_ADD(database->ageGroupSizes + 0, size);
}

void Record::unsetAgeGroup(void)
{
	if (table)
		{
		Database *database = table->database;
		int n = database->currentGeneration - generation;
		
		if (n >= AGE_GROUPS)
			INTERLOCKED_ADD(&database->overflowSize, -size);
		else
			INTERLOCKED_ADD(database->ageGroupSizes + n, -size);
		}

}
***/

void Record::poke()
{
	int gen = format->table->database->currentGeneration;
	
	if (generation != gen)
		generation = gen;
	
	/***
	Database *database = table->database;
	int32 currentGeneration = database->currentGeneration;
	int32 n = currentGeneration - generation;

	if (n == 0)
		return;

	ASSERT (n > 0);
	generation = currentGeneration;

	if (n >= AGE_GROUPS)
		INTERLOCKED_ADD(&database->overflowSize, -size);
	else
		INTERLOCKED_ADD(database->ageGroupSizes + n, -size);

	INTERLOCKED_ADD(database->ageGroupSizes + 0, size);
	***/
}

Record* Record::releaseNonRecursive(void)
{
	release();
	
	return NULL;
}

void Record::setPriorVersion(Record* record)
{
	ASSERT(false);
}

int Record::thaw()
{
	return 0;
}

int Record::setRecordData(const UCHAR * dataIn, int dataLength)
{
	encoding = shortVector;
	int vectorLength = format->count * sizeof(short);
	int totalLength = vectorLength + dataLength;
	//char *dataBuffer = ALLOCATE_RECORD(totalLength);
	char *dataBuffer = allocRecordData(totalLength);
	
	memset(dataBuffer, 0, vectorLength);
	memcpy(dataBuffer + vectorLength, dataIn, dataLength);
	
	((USHORT*) dataBuffer)[0] = (USHORT) (vectorLength + sizeof(short));
	
	char **ptr = &data.record;
	
	// If data.record has changed since allocating the new buffer, then free the new buffer
	
	if (!COMPARE_EXCHANGE_POINTER(ptr, NULL, dataBuffer))
		{
		DELETE_RECORD(dataBuffer);
		totalLength = 0;
		}
		
	return (totalLength);
}


void Record::deleteData(void)
{
	if (data.record)
		{
		switch (encoding)
			{
			case valueVector:
				delete [] (Value*) data.record;
				break;

			default:
				DELETE_RECORD (data.record);
			}
		
		data.record = NULL;
		}
}

void Record::print(void)
{
	printf("  %p\tId %d, enc %d, state %d, use %d, grp %d\n",
			this, recordNumber, encoding, state, useCount, generation);
}

void Record::printRecord(const char* header)
{
	Log::debug("%s:\n", header);
	print();
}

void Record::validateData(void)
{
	ASSERT(data.record);
}

char* Record::allocRecordData(int length)
{
	for (int n = 0;; ++n)
		try
			{
			return POOL_NEW(format->table->database->recordDataPool) char[length];
			}
		catch (SQLException& exception)
			{
			if (n > 2 || exception.getSqlcode() != OUT_OF_RECORD_MEMORY_ERROR)
				throw;
			
			format->table->database->forceRecordScavenge();
			}
	
	return NULL;
}

Record* Record::getGCPriorVersion(void)
{
	return NULL;
}
