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

// ResultSet.h: interface for the ResultSet class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RESULTSET_H__02AD6A4F_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_RESULTSET_H__02AD6A4F_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Values.h"
#include "LinkedList.h"
#include "SyncObject.h"
#include "TimeStamp.h"
#include "DateTime.h"	// Added by ClassView


CLASS(Statement);
class NSelect;
class Value;
class ResultSetMetaData;
CLASS(Field);
class Blob;
class Clob;
class BinaryBlob;
class AsciiBlob;
class TemplateContext;
class Database;
class Table;
class CompiledStatement;
class Connection;


class ResultSet
{
protected:
	ResultSet();
	ResultSet (int count);
	virtual ~ResultSet();

public:
	void putTerQuad (int64 value, BinaryBlob *blob);
	Clob* getClob(const char *name);
	Clob* getClob (int index);
	void print (Stream *stream);
	void statementClosed();
	virtual Statement* getStatement();
	void clearConnection();
	void transactionEnded();
	const char* getSymbol (int index);
	void putTerLong (int32 value, BinaryBlob *blob);
	void connectionClosed();
	int getIndex (Field *field);
	virtual int getColumnIndex (const WCString *name);
	virtual int findColumn (const WCString *name);
	virtual int findColumn (const char *name);
	virtual bool wasNull();
	virtual Blob* getBlob (int index);
	virtual ResultSetMetaData* getMetaData();
	virtual double getDouble (int id);
	virtual void close();
	virtual int getInt (int id);
	virtual bool next();
	virtual const char* getString (int id);
	virtual int getColumnIndex (const char *name);
	virtual Value* getValue (int index);
	virtual DateTime getDate(int id);
	virtual DateTime getDate (const char *name);
	virtual int findColumnIndex (const char *name);
	virtual Blob* getRecord();
	virtual int getInt (const char *name);
	virtual int64 getLong (const char * name);
	virtual int64 getLong (int id);
	virtual void releaseJavaRef();
	virtual void addJavaRef();
	virtual double getDouble (const char * name);
	virtual float getFloat (const char *name);
	virtual float getFloat (int id);
	virtual TimeStamp getTimestamp (const char *name);
	virtual TimeStamp getTimestamp (int id);
	virtual short getShort (const char * name);
	virtual short getShort (int id);
	virtual Time getTime(const char *fieldName);
	virtual Time getTime(int id);
	Blob* getBlob (const char *name);

	void allocConversions();
	void init (int count);
	const char* getColumnName (int index);
	void clearStatement();
	void release();
	void addRef();
	Field* getField (const char *fieldName);
	bool isMember (Field *field);
	bool isMember (Table *table);
	Database* getDatabase();
	Value* getValue (const char *name);
	const char* getString (const char *name);
	void deleteBlobs();
	Field* getField (int index);
	void setValue (int column, Value* value);
	ResultSet(Statement *statement, NSelect *node, int numberColums);

	SyncObject		syncObject;
	Values			values;
	Statement		*statement;
	CompiledStatement *compiledStatement;
	Connection		*connection;
	Database		*database;
	ResultSet		*sibling;
	ResultSet		*connectionNext;
	int32			currentRow;
	int32			numberColumns;
	int32			numberRecords;
	NSelect			*select;
	char			**conversions;
	ResultSetMetaData	*metaData;
	LinkedList		blobs;
	LinkedList		clobs;
	short			valueWasNull;
	bool			active;
	volatile INTERLOCK_TYPE	useCount;
	volatile INTERLOCK_TYPE	javaCount;
	int32			javaStatementCount;
	int32			handle;
};

#endif // !defined(AFX_RESULTSET_H__02AD6A4F_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
