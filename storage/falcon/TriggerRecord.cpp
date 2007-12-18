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

// TriggerRecord.cpp: implementation of the TriggerRecord class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "TriggerRecord.h"
#include "Trigger.h"
#include "Table.h"
#include "RecordVersion.h"
#include "Transaction.h"
#include "Value.h"
#include "Field.h"
#include "SQLError.h"
#include "BinaryBlob.h"
#include "AsciiBlob.h"
#include "Repository.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TriggerRecord::TriggerRecord(Trigger *trg, Transaction *trans, Record *rec, int op, bool update)
{
	trigger =trg;
	record = rec;
	transaction = trans;
	operation = op;
	updatable = update;
}

TriggerRecord::~TriggerRecord()
{
}

const char* TriggerRecord::getString(const WCString *fieldName, char **temp)
{
	//Value value;
	getValue (fieldName);

	return value.getString (temp);
}

const char* TriggerRecord::getString(int id, char **temp)
{
	getValue (id);

	return value.getString (temp);
}

void TriggerRecord::getValue(const WCString *fieldName)
{
	Field *field = trigger->getField (fieldName);
	record->getValue (field->id, &value);
}

void TriggerRecord::getValue(int columnId)
{
	Field *field = trigger->getField (columnId);
	record->getValue (field->id, &value);
}

void TriggerRecord::setValue(const WCString *fieldName, Value *value)
{
	setValue (trigger->getField (fieldName), value);
}

void TriggerRecord::setValue(int id, Value *value)
{
	setValue (trigger->getField (id), value);
}


void TriggerRecord::setValue(Field *field, Value *value)
{
	if (!updatable)
		throw SQLEXCEPTION (RUNTIME_ERROR, "trigger operation doesn't allow record update");

	Value temp;

	if (field->repository)
		value = field->repository->defaultRepository (field, value, &temp);

	record->setValue (transaction, field->id, value, false, true);
}

int64 TriggerRecord::getLong(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getQuad();
}

void TriggerRecord::setValue(const WCString *fieldName, const char *value)
{
	Value val;
	val.setString (value, false);
	setValue (fieldName, &val);
}

const char* TriggerRecord::getTableName()
{
	return trigger->table->getName();
}

const char* TriggerRecord::getColumnName(int columnId)
{
	return trigger->getField (columnId)->getName();
}

const char* TriggerRecord::getSchemaName()
{
	return trigger->table->getSchema();
}

bool TriggerRecord::wasNull()
{
	return value.isNull();	
}

int TriggerRecord::getPrecision(int columnId)
{
	return trigger->getField (columnId)->length;
}

int TriggerRecord::getScale(int columnId)
{
	return trigger->getField (columnId)->scale;
}

char TriggerRecord::getByte(int id)
{
	getValue (id);

	return value.getByte();
}

char TriggerRecord::getByte(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getByte();
}

bool TriggerRecord::getBoolean(int id)
{
	getValue (id);

	return value.getInt() != 0;
}

bool TriggerRecord::getBoolean(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getInt() != 0;
}

short TriggerRecord::getShort(int id)
{
	getValue (id);

	return value.getShort();
}

short TriggerRecord::getShort(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getShort();
}

int TriggerRecord::getInt(int id)
{
	getValue (id);

	return value.getInt();
}

int TriggerRecord::getInt(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getInt();
}

int64 TriggerRecord::getLong(int id)
{
	getValue (id);

	return value.getQuad();
}

float TriggerRecord::getFloat(int id)
{
	getValue (id);

	return (float) value.getDouble();
}

float TriggerRecord::getFloat(const WCString *fieldName)
{
	getValue (fieldName);

	return (float) value.getDouble();
}

double TriggerRecord::getDouble(int id)
{
	getValue (id);

	return value.getDouble();
}

double TriggerRecord::getDouble(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getDouble();
}

DateTime TriggerRecord::getDate(int id)
{
	getValue (id);

	return value.getDate();
}

DateTime TriggerRecord::getDate(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getDate();
}

TimeStamp TriggerRecord::getTimestamp(int id)
{
	getValue (id);

	return value.getTimestamp();
}

TimeStamp TriggerRecord::getTimestamp(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getTimestamp();
}

int TriggerRecord::getColumnType(int columnId)
{
	return trigger->getField (columnId)->getSqlType();
}

int TriggerRecord::getColumnId(const WCString *fieldName)
{
	return trigger->getField (fieldName)->id;
}

int TriggerRecord::nextColumnId(int prior)
{
	return trigger->table->nextColumnId (prior);
}

bool TriggerRecord::isChanged(int columnId, TriggerRecord *record)
{
	try
		{
		getValue (columnId);
		record->getValue (columnId);
		return value.compare (&record->value) != 0;
		}
	catch (SQLException &exception)
		{
		exception;
		}

	return true;
}

int TriggerRecord::nextPrimaryKeyColumn(int previous)
{
	return trigger->table->nextPrimaryKeyColumn (previous);
}

void TriggerRecord::setValue(int id, char value)
{
	Value val;
	val.setValue ((short) value);
	setValue (id, &val);
}

void TriggerRecord::setValue(int id, short value)
{
	Value val;
	val.setValue (value);
	setValue (id, &val);
}

void TriggerRecord::setValue(int id, float value)
{
	Value val;
	val.setValue (value);
	setValue (id, &val);
}

void TriggerRecord::setValue(int id, double value)
{
	Value val;
	val.setValue (value);
	setValue (id, &val);
}

void TriggerRecord::setValue(int id, int value)
{
	Value val;
	val.setValue (value);
	setValue (id, &val);
}

void TriggerRecord::setValue(int id, int64 value)
{
	Value val;
	val.setValue (value);
	setValue (id, &val);
}

void TriggerRecord::setValue(const WCString *fieldName, char value)
{
	Value val;
	val.setValue ((short) value);
	setValue (fieldName, &val);
}

void TriggerRecord::setValue(const WCString *fieldName, short value)
{
	Value val;
	val.setValue (value);
	setValue (fieldName, &val);
}

void TriggerRecord::setValue(const WCString *fieldName, int value)
{
	Value val;
	val.setValue (value);
	setValue (fieldName, &val);
}

void TriggerRecord::setValue(const WCString *fieldName, int64 value)
{
	Value val;
	val.setValue (value);
	setValue (fieldName, &val);
}

void TriggerRecord::setValue(const WCString *fieldName, float value)
{
	Value val;
	val.setValue (value);
	setValue (fieldName, &val);
}

void TriggerRecord::setValue(const WCString *fieldName, double value)
{
	Value val;
	val.setValue (value);
	setValue (fieldName, &val);
}

void TriggerRecord::setValue(int id, const char *value)
{
	Value val;
	val.setString (value, true);
	setValue (id, &val);
}

void TriggerRecord::setValue(int id, DateTime value)
{
	Value val;
	val.setValue (value);
	setValue (id, &val);
}

void TriggerRecord::setValue(int id, TimeStamp value)
{
	Value val;
	val.setValue (value);
	setValue (id, &val);
}

void TriggerRecord::setValue(const WCString *fieldName, TimeStamp value)
{
	Value val;
	val.setValue (value);
	setValue (fieldName, &val);
}

void TriggerRecord::setValue(const WCString *fieldName, DateTime value)
{
	Value val;
	val.setValue (value);
	setValue (fieldName, &val);
}

void TriggerRecord::setNull(int id)
{
	Value val;
	setValue (id, &val);
}

void TriggerRecord::setNull(const WCString *fieldName)
{
	Value val;
	setValue (fieldName, &val);
}

Time TriggerRecord::getTime(int id)
{
	getValue (id);

	return value.getTime();
}

Time TriggerRecord::getTime(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getTime();
}

void TriggerRecord::setValue(const WCString *fieldName, Time value)
{
	Value val;
	val.setValue (value);
	setValue (fieldName, &val);
}

void TriggerRecord::setValue(int id, Time value)
{
	Value val;
	val.setValue (value);
	setValue (id, &val);
}

void TriggerRecord::setValue(int index, int length, const char *bytes)
{
	BinaryBlob *blob = new BinaryBlob();
	blob->putSegment (length, bytes, true);
	Value val;
	val.setValue (blob);
	setValue (index, &val);
	blob->release();
}

void TriggerRecord::setValue(const WCString *fieldName, int length, const char *bytes)
{
	BinaryBlob *blob = new BinaryBlob();
	blob->putSegment (length, bytes, true);
	Value val;
	val.setValue (blob);
	setValue (fieldName, &val);
	blob->release();
}

Blob* TriggerRecord::getBlob(int id)
{
	getValue (id);

	return value.getBlob();
}

Blob* TriggerRecord::getBlob(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getBlob();
}

Clob* TriggerRecord::getClob(int id)
{
	getValue (id);

	return value.getClob();
}

Clob* TriggerRecord::getClob(const WCString *fieldName)
{
	getValue (fieldName);

	return value.getClob();
}

void TriggerRecord::setValue(const WCString *fieldName, Clob *value)
{
	Value val;
	val.setValue (value);
	setValue (fieldName, &val);
}

void TriggerRecord::setValue(const WCString *fieldName, Blob *value)
{
	Value val;
	val.setValue (value);
	setValue (fieldName, &val);
}

void TriggerRecord::setValue(int id, Blob *value)
{
	Value val;
	val.setValue (value);
	setValue (id, &val);
}

void TriggerRecord::setValue(int id, Clob *value)
{
	Value val;
	val.setValue (value);
	setValue (id, &val);
}
