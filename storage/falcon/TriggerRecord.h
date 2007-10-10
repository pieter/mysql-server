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

// TriggerRecord.h: interface for the TriggerRecord class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TRIGGERRECORD_H__EF5109C4_B340_11D4_98FA_0000C01D2301__INCLUDED_)
#define AFX_TRIGGERRECORD_H__EF5109C4_B340_11D4_98FA_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Types.h"
#include "Value.h"
//#include "DateTime.h"	// Added by ClassView

class Trigger;
class Record;
class Transaction;
class Value;
CLASS(Field);

class TriggerRecord  
{
public:
	void setValue (Field *field, Value *value);
	void setValue(int id, Clob *clob);
	void setValue(int id, Blob *value);
	void setValue(const WCString *fieldName, Blob *value);
	void setValue(const WCString *fieldName, Clob *clob);
	Clob* getClob (const WCString *fieldName);
	Clob* getClob (int id);
	Blob* getBlob (const WCString *fieldName);
	Blob* getBlob (int id);
	void setValue (int id, Time value);
	void setValue(const WCString *fieldName, Time value);
	Time getTime(const WCString *fieldName);
	Time getTime (int id);
	void setNull(const WCString *fieldName);
	void setNull (int id);
	void setValue (const WCString *fieldName, DateTime value);
	void setValue (const WCString *fieldName, Value *value);
	void setValue (int id, Value *value);
	void setValue (const WCString *fieldName, TimeStamp value);
	void setValue (const WCString *fieldName, double value);
	void setValue (const WCString *fieldName, float value);
	void setValue (const WCString *fieldName, int64 value);
	void setValue (const WCString *fieldName, int value);
	void setValue (const WCString *fieldName, short value);
	void setValue (const WCString *fieldName, char value);
	void setValue (const WCString *fieldName, const char *value);
	void setValue (const WCString *fieldName, int length, const char *bytes);
	void setValue (int id, TimeStamp value);
	void setValue (int id, DateTime date);
	void setValue (int id, double value);
	void setValue (int id, float value);
	void setValue (int id, int64 value);
	void setValue (int id, int value);
	void setValue (int id, short value);
	void setValue (int id, char value);
	void setValue (int id, const char *value);
	void setValue (int index, int length, const char *bytes);
	int nextPrimaryKeyColumn (int previous);
	void getValue (int columnId);
	bool isChanged (int columnId, TriggerRecord *record);
	const char* getString (int id, char **temp);
	int nextColumnId (int prior);
	int getColumnId (const WCString *fieldName);
	int getColumnType (int columnId);
	TimeStamp getTimestamp  (const WCString *fieldName);
	TimeStamp getTimestamp (int id);
	DateTime getDate  (const WCString *fieldName);
	DateTime getDate (int id);
	double	getDouble (const WCString *fieldName);
	double	getDouble (int id);
	float	getFloat  (const WCString *fieldName);
	float	getFloat (int id);
	int64	getLong (int id);
	int		getInt (const WCString *fieldName);
	int		getInt (int id);
	short getShort (const WCString *fieldName);
	short getShort (int id);
	bool getBoolean (const WCString *fieldName);
	bool getBoolean (int id);
	char getByte (const WCString *fieldName);
	char getByte (int id);
	int getScale (int columnId);
	int getPrecision (int columnId);
	bool wasNull();
	const char* getSchemaName();
	const char* getColumnName (int columnId);
	const char* getTableName();
	/***
	void setValue (const WCString *fieldName, const char *value);
	void setValue (const WCString *fieldName, long int32);
	***/
	int64 getLong (const WCString *fieldName);
	void getValue (const WCString *fieldName);
	const char* getString (const WCString *fieldName, char **temp);
	TriggerRecord(Trigger *trg, Transaction *trans, Record *rec, int operation, bool updatable);
	virtual ~TriggerRecord();

	Trigger		*trigger;
	Record		*record;
	Transaction	*transaction;
	int			operation;
	bool		updatable;
	char		*temp;
	Value		value;
};

#endif // !defined(AFX_TRIGGERRECORD_H__EF5109C4_B340_11D4_98FA_0000C01D2301__INCLUDED_)
