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

// Trigger.h: interface for the Trigger class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TRIGGER_H__EF5109C1_B340_11D4_98FA_0000C01D2301__INCLUDED_)
#define AFX_TRIGGER_H__EF5109C1_B340_11D4_98FA_0000C01D2301__INCLUDED_

#include "LinkedList.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Table;
class Database;
class Record;
class RecordVersion;
class Transaction;
class JavaNative;
class Java;
class TriggerRecord;
CLASS(Field);
class Connection;

class _jobject;
class _jclass;
struct JNIEnv_;

typedef JNIEnv_ JNIEnv;

struct _jmethodID;

class Trigger  
{
public:
	void release();
	void addRef();
	bool isEnabled (Connection *connection);
	void addTriggerClass (const char *symbol);
	Field* getField (int id);
	Field* getField (const WCString *fieldName);
	_jobject* wrapTriggerRecord (JavaNative *javaNative, TriggerRecord *record);
	static void deleteTrigger (Database *database, const char *schema, const char *name);
	static JString getTableName (Database *database, const char *schema, const char *name);
	void deleteTrigger();
	void fireTrigger(Transaction *transaction, int operation, Record *before, RecordVersion *after);
	static void getTableTriggers (Table *table);
	void save();
	void loadClass();
	void zapLinkages();
	static void initialize (Database *database);
	Trigger(JString triggerName, Table *tbl, int typeMask, int pos, bool act, JString cls, JString method);

protected:
	virtual ~Trigger();

public:
	JString		name;
	JString		className;
	JString		methodName;
	int			useCount;
	Table		*table;
	Database	*database;
	Java		*java;
	int			mask;
	int			position;
	bool		active;
	Trigger		*next;
	LinkedList	triggerClasses;
	_jclass		*triggerClass;
	_jclass		*recordClass;
	_jmethodID	*recordInit;
	_jmethodID	*triggerExecute;
};

#endif // !defined(AFX_TRIGGER_H__EF5109C1_B340_11D4_98FA_0000C01D2301__INCLUDED_)
