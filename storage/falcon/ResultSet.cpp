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

// ResultSet.cpp: implementation of the ResultSet class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include <string.h>
#include "Engine.h"
#include "ResultSet.h"
#include "NSelect.h"
#include "Value.h"
#include "SQLError.h"
#include "Statement.h"
#include "CompiledStatement.h"
#include "ResultSetMetaData.h"
#include "BinaryBlob.h"
#include "AsciiBlob.h"
#include "Database.h"
#include "Stream.h"
//#include "TemplateContext.h"
#include "Field.h"
#include "Connection.h"
#include "Sync.h"
#include "Interlock.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ResultSet::ResultSet(Statement *stmt, NSelect *node, int count)
{
	init (count);
	statement = stmt;
	statement->addRef();
	connection = statement->connection;
	database = connection->database;
	compiledStatement = statement->statement;
	compiledStatement->addRef();
	select = node;

	if (connection)
		connection->addResultSet(this);
}

ResultSet::ResultSet(int count)
{
	init (count);
}

ResultSet::ResultSet()
{
	init (0);
}

void ResultSet::init(int count)
{
	statement = NULL;
	compiledStatement = NULL;
	select = NULL;
	numberColumns = count;
	numberRecords = 0;
	valueWasNull = -1;

	if (numberColumns)
		{
		values.alloc (numberColumns);
		allocConversions();
		}
	else
		conversions = NULL;

	metaData = NULL;
	active = false;
	useCount = 1;
	javaCount = 0;
	javaStatementCount = 0;
	connection = NULL;
	database = NULL;
}

ResultSet::~ResultSet()
{
	clearStatement();
	deleteBlobs();

	if (connection)
		clearConnection();

	if (metaData)
		metaData->resultSetClosed();
}

const char* ResultSet::getString(int id)
{
	Value *value = getValue(id);

	if (conversions [id - 1])
		return conversions [id - 1];

	return value->getString(conversions + id - 1);
}

bool ResultSet::next()
{
	if (!statement)
		throw SQLEXCEPTION (RUNTIME_ERROR, "ResultSet has been closed");

	deleteBlobs();
	values.clear();
	valueWasNull = -1;

	if (select)
		{
		if (active = select->next(statement, this))
			{
			++database->numberRecords;
			++numberRecords;
			++statement->stats.recordsReturned;
			}
			
		return active;
		}

	return false;
}

void ResultSet::setValue(int column, Value * value)
{
	if (!statement)
		throw SQLEXCEPTION (RUNTIME_ERROR, "ResultSet has been closed");

	values.values [column].setValue (value, true);

	if (conversions [column])
		{
		delete conversions[column];
		conversions[column] = NULL;
		}
}

int ResultSet::getInt(int id)
{
	return getValue(id)->getInt ();
}


int ResultSet::getInt(const char * name)
{
	int id = getColumnIndex (name);
	
	return getValue(id + 1)->getInt ();
}


short ResultSet::getShort(int id)
{
	return getValue(id)->getShort();
}

short ResultSet::getShort(const char *name)
{
	int id = getColumnIndex (name);
	
	return getValue(id + 1)->getShort();
}

int64 ResultSet::getLong(int id)
{
	return getValue(id)->getQuad ();
}

int64 ResultSet::getLong(const char *name)
{
	int id = getColumnIndex (name);
	
	return getValue(id + 1)->getQuad ();
}

void ResultSet::close()
{
	ASSERT (javaCount == 0);
	clearStatement();
	delete this;
}

double ResultSet::getDouble(int id)
{
	return getValue(id)->getDouble ();
}


double ResultSet::getDouble(const char *name)
{
	int id = getColumnIndex (name);
	
	return getDouble (id + 1);
}

float ResultSet::getFloat(int id)
{
	return (float) getValue(id)->getDouble ();
}

float ResultSet::getFloat(const char *name)
{
	int id = getColumnIndex (name);
	
	return (float) getDouble (id + 1);
}

ResultSetMetaData* ResultSet::getMetaData()
{
	/***
	if (!statement)
		throw SQLEXCEPTION (RUNTIME_ERROR, "ResultSet's statement has been closed");
	***/

	if (!metaData)
		metaData = new ResultSetMetaData (this);

	return metaData;
}

Field* ResultSet::getField(int index)
{
	if (!select)
		throw SQLEXCEPTION (RUNTIME_ERROR, "ResultSet is not active");

	if (index < 1 || index > numberColumns)
		throw SQLEXCEPTION (RUNTIME_ERROR, "invalid column index for result set");

	return select->getField (index - 1);
}

Blob* ResultSet::getBlob(int index)
{
	Blob *blob = getValue(index)->getBlob();
	blobs.append (blob);

	return blob;
}


Blob* ResultSet::getBlob(const char *name)
{
	Blob *blob = getValue(name)->getBlob();
	blobs.append (blob);

	return blob;
}


Clob* ResultSet::getClob(int index)
{
	Clob *clob = getValue(index)->getClob();
	clobs.append (clob);

	return clob;
}

Clob* ResultSet::getClob(const char *name)
{
	Clob *clob = getValue(name)->getClob();
	clobs.append (clob);

	return clob;
}

void ResultSet::deleteBlobs()
{
	FOR_OBJECTS (Blob*, blob, &blobs)
		blob->release();
	END_FOR;

	blobs.clear();

	FOR_OBJECTS (Clob*, clob, &clobs)
		clob->release();
	END_FOR;

	clobs.clear();
}

bool ResultSet::wasNull()
{
	if (valueWasNull < 0)
		throw SQLEXCEPTION (RUNTIME_ERROR, "illegal call to ResultSet.wasNull()");

	return (valueWasNull) ? true : false;
}

const char* ResultSet::getString(const char * name)
{
	int id = getColumnIndex (name);
	
	return getString (id + 1);
}

Value* ResultSet::getValue(int index)
{
	if (!active)
		throw SQLEXCEPTION (RUNTIME_ERROR, "no active row in result set ");

	if (index < 1 || index > numberColumns)
		throw SQLEXCEPTION (RUNTIME_ERROR, "invalid column index for result set");

	Value *value = values.values + index - 1;
	valueWasNull = value->isNull();

	return value;
}

Value* ResultSet::getValue(const char * name)
{
	if (!active)
		throw SQLEXCEPTION (RUNTIME_ERROR, "no active row in result set ");

	int index = getColumnIndex (name);

	if (index < 0)
		throw SQLEXCEPTION (RUNTIME_ERROR, "invalid column name (%s) for result set", name);

	Value *value = values.values + index;
	valueWasNull = value->isNull();

	return value;
}

Database* ResultSet::getDatabase()
{
	return database;
}

bool ResultSet::isMember(Table * table)
{
	return select->isMember (table);
}

bool ResultSet::isMember(Field * field)
{
	for (int n = 0; n < numberColumns; ++n)
		if (select->getField (n) == field)
			return true;

	return false;
}

Field* ResultSet::getField(const char * fieldName)
{
	return select->getField (fieldName);
}

void ResultSet::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void ResultSet::release()
{
	ASSERT (useCount > 0);

	if (useCount == 1 && connection)
		clearConnection();

	if (INTERLOCKED_DECREMENT (useCount) == 0)
		close();
}

void ResultSet::addJavaRef()
{
	INTERLOCKED_INCREMENT (javaCount);
	addRef();

	if (statement && javaStatementCount == 0)
		{
		++javaStatementCount;
		statement->addJavaRef();
		}
}

void ResultSet::releaseJavaRef()
{
	ASSERT (javaCount > 0);

	// If we're almost done with the object, break the connection

	if (javaCount == 1)
		{
		if (statement && javaStatementCount)
			{
			--javaStatementCount;
			statement->releaseJava();
			}
		if (connection)
			{
			if (useCount == javaCount)
				clearConnection();
			}
		else
			while (useCount > javaCount)
				release();
		}

	INTERLOCKED_DECREMENT (javaCount);
	release();
}

void ResultSet::clearStatement()
{
	Sync sync (&syncObject, "ResultSet::clearStatement");
	sync.lock (Exclusive);
	active = false;
	numberRecords = 0;
	valueWasNull = -1;

	if (conversions)
		{
		for (int n = 0; n < numberColumns; ++n)
			if (conversions [n])
				delete conversions [n];
		delete [] conversions;
		conversions = NULL;
		}

	if (compiledStatement)
		{
		compiledStatement->release();
		compiledStatement = NULL;
		}

	if (statement)
		{
		Statement *stmnt = statement;
		statement = NULL;
		stmnt->deleteResultSet (this);
		if (javaCount && javaStatementCount)
			{
			--javaStatementCount;
			stmnt->releaseJava();
			}
		stmnt->release();
		}
}

const char* ResultSet::getColumnName(int index)
{
	if (index < 1 || index > numberColumns)
		throw SQLEXCEPTION (RUNTIME_ERROR, "invalid column index for result set");

	return select->getColumnName (index - 1);
}

void ResultSet::allocConversions()
{
	conversions = new char* [numberColumns];
	memset (conversions, 0, sizeof (char*) * numberColumns);
}

int ResultSet::findColumn(const char * name)
{
	int id = getColumnIndex (name);

	if (id < 0)
		throw SQLEXCEPTION (RUNTIME_ERROR, "column %s not defined in ResultSet", name);

	return id + 1;
}

int ResultSet::findColumn(const WCString *name)
{
	int id = getColumnIndex (name);

	if (id < 0)
		{
		JString stuff (name);
		throw SQLEXCEPTION (RUNTIME_ERROR, "column %s not defined in ResultSet", (const char*) stuff);
		}

	return id + 1;
}

int ResultSet::findColumnIndex(const char * name)
{
	return select->getColumnIndex (name) + 1;
}

int ResultSet::getColumnIndex(const char * name)
{
	int n = select->getColumnIndex (name);

	if (n < 0)
		throw SQLEXCEPTION (RUNTIME_ERROR, 
								"invalid column name (\"%s\") for result set", name);

	return n;
}

int ResultSet::getColumnIndex(const WCString *name)
{
	const char *columnName = database->getSymbol (name);
	int n = select->getColumnIndex (columnName);

	if (n < 0)
		throw SQLEXCEPTION (RUNTIME_ERROR, 
				"invalid column name (\"%s\") for result set", columnName);

	return n;
}

int ResultSet::getIndex(Field * field)
{
	return select->getIndex (field) + 1;
}

void ResultSet::clearConnection()
{
	Sync sync (&database->syncResultSets, "ResultSet::clearConnection");
	sync.lock (Shared);

	if (connection)
		connection->deleteResultSet (this);

	connection = NULL;
}

void ResultSet::connectionClosed()
{
	if (statement)
		clearStatement();

	connection = NULL;

	if (javaCount == 0)
		for (int n = useCount; n > 0; --n)
			release();
}

DateTime ResultSet::getDate(int id)
{
	return getValue(id)->getDate ();
}

DateTime ResultSet::getDate(const char * name)
{
	int id = getColumnIndex (name);
	
	return getValue(id + 1)->getDate ();
}

TimeStamp ResultSet::getTimestamp(int id)
{
	return getValue(id)->getTimestamp ();
}

TimeStamp ResultSet::getTimestamp(const char *name)
{
	int id = getColumnIndex (name);
	
	return getValue(id + 1)->getTimestamp ();
}

Blob* ResultSet::getRecord()
{
	BinaryBlob *blob = new BinaryBlob (2000);
	blobs.append (blob);
	blob->putCharacter (TER_FORMAT);
	blob->putCharacter (TER_ENCODING_INTEL);
	putTerLong (numberColumns, blob);

	for (int n = 0; n < numberColumns; ++n)
		{
		Value *value = values.values + n;
		
		switch (value->getType())
			{
			case Null:
				blob->putCharacter (terNull);
				break;

			case String:
			case Char:
			case Varchar:
				{
				blob->putCharacter (terString);
				int len = value->getStringLength();
				putTerLong (len, blob);
				blob->putSegment (len, value->getString(), true);
				blob->putCharacter (0);
				}
				break;
				
			case Short:
				{
				int scale = value->getScale();
				
				if (scale)
					{
					blob->putCharacter(terScaledShort);
					blob->putCharacter(scale);
					}
				else
					blob->putCharacter(terShort);
				
				short n = value->getShort(scale);
				blob->putSegment (sizeof (short), (const char*) &n, true);
				}
				break;

			case Long:
				{
				int scale = value->getScale();
				
				if (scale)
					{
					blob->putCharacter (terScaledLong);
					blob->putCharacter (scale);
					}
				else
					blob->putCharacter (terLong);
					
				putTerLong (value->getInt(scale), blob);
				}
				break;

			case Quad:
				{
				int scale = value->getScale();
				
				if (scale)
					{
					blob->putCharacter (terScaledQuad);
					blob->putCharacter (scale);
					}
				else
					blob->putCharacter (terQuad);
				//blob->putSegment (sizeof (int64), (const char*) &value->data.quad, true);
				int64 quad = value->getQuad(scale);
				putTerQuad (quad, blob);
				}
				break;

			case Double:
				{
				blob->putCharacter (terDouble);
				double dbl = value->getDouble();
				blob->putSegment (sizeof (double), (const char*) &dbl, true);
				}
				break;

			case Date:
				{
				blob->putCharacter (terBigDate);
				int64 milliseconds = value->getMilliseconds();
				blob->putSegment (sizeof (milliseconds), (const char*) &milliseconds, true);
				}
				break;

			case TimeType:
				{
				blob->putCharacter (terTime);
				int64 milliseconds = value->getMilliseconds();
				blob->putSegment (sizeof (milliseconds), (const char*) &milliseconds, true);
				}
				break;

			case Timestamp:
				{
				blob->putCharacter (terBigTimestamp);
				int64 milliseconds = value->getMilliseconds();
				blob->putSegment (sizeof (milliseconds), (const char*) &milliseconds, true);
				putTerLong (value->getNanos(), blob);
				}
				break;

			case ClobPtr:
				{
				Clob *data = value->getClob();
				
				if (data->isBlobReference())
					{
					blob->putCharacter (terRepositoryClob);
					putTerLong (data->getReferenceLength(), blob);
					data->getReference (blob);
					int length = data->length();
					putTerLong (length, blob);
					
					if (length > 0)
						blob->putSegment (data);
					}
				else
					{
					blob->putCharacter (terString);
					putTerLong (data->length(), blob);
					blob->putSegment (data);
					blob->putCharacter (0);
					}
					
				data->release();
				}
				break;

			case BlobPtr:
				{
				Blob *data = value->getBlob();
				if (data->isBlobReference())
					{
					blob->putCharacter (terRepositoryBlob);
					putTerLong (data->getReferenceLength(), blob);
					data->getReference (blob);
					}
				else
					blob->putCharacter (terBinaryBlob);
				int length = data->length();
				putTerLong (length, blob);
				if (length > 0)
					blob->putSegment (data);
				data->release();
				}
				break;

			default:
				NOT_YET_IMPLEMENTED;
			}
		}

	return blob;
}

void ResultSet::putTerLong(int32 value, BinaryBlob * blob)
{
	ASSERT (sizeof (value) == 4);
	blob->putSegment (sizeof (int32), (const char*) &value, true);
}


void ResultSet::putTerQuad(int64 value, BinaryBlob *blob)
{
	ASSERT (sizeof (value) == 8);
	blob->putSegment (sizeof (int64), (const char*) &value, true);
}

const char* ResultSet::getSymbol(int index)
{
	return database->getSymbol (getString (index));
}

void ResultSet::transactionEnded()
{
	clearStatement();
}

Statement* ResultSet::getStatement()
{
	if (!statement)
		throw SQLError (RUNTIME_ERROR, "no statement currently associated with ResultSet");

	return statement;
}

void ResultSet::statementClosed()
{
	Statement *stmnt = statement;

	if (stmnt)
		{
		statement = NULL;
		if (javaCount && javaStatementCount)
			{
			--javaStatementCount;
			stmnt->releaseJava();
			}
		}
}

void ResultSet::print(Stream *stream)
{
	stream->format ("%8x ResultSet uc %d, jc %d, jsc %d, stat %x, cs %x, sib %x\n",
					this, useCount, javaCount, javaStatementCount, 
					statement, compiledStatement, sibling);
}

Time ResultSet::getTime(int id)
{
	return getValue(id)->getTime ();
}

Time ResultSet::getTime(const char *fieldName)
{
	int id = getColumnIndex (fieldName);
	
	return getValue(id + 1)->getTime ();
}
