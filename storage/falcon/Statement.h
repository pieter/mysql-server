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

// Statement.h: interface for the Statement class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STATEMENT_H__02AD6A41_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_STATEMENT_H__02AD6A41_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Syntax.h"
#include "Values.h"
#include "SyncObject.h"

struct StatementStats {
	int	indexHits;
	int	indexFetches;
	int	exhaustiveFetches;
	int	recordsFetched;
	int recordsReturned;
	int	inserts;
	int	updates;
	int	deletions;
	int	replaces;
	int records;
	int recordsSorted;
	};

class Database;
class Syntax;
class Table;
class Connection;
class CompiledStatement;
class Value;
class Transaction;
class ResultSet;
class Context;
class NNode;
class NSelect;
class Record;
class ResultList;
CLASS(Field);
class ForeignKey;
class Sort;
class Index;
class Bitmap;
class LinkedList;
class PrivilegeObject;
class ValueSet;
class Row;

enum SyntaxType;
struct FieldType;

START_NAMESPACE

class Statement
{
public:
	void renameTables (Syntax *syntax);
	ValueSet* getValueSet (int slot);
	ResultList* findResultList (int32 handle);
	void syncRepository (Syntax *syntax);
	void dropRepository (Syntax *syntax);
	void upgradeSchema (Syntax *syntax, bool upgrade);
	void createDomain (Syntax *syntax, bool upgrade);
	void createRepository (Syntax *syntax, bool update);
	void setTableFilter(const WCString *schemaName, const WCString *tableName);
	void setTableFilter (const char *schemaName, const char *tableName);
	void analyze (Stream *stream, const char *name, int count);
	void print (Stream *stream);
	void checkAlterPriv (PrivilegeObject *object);
	void keyChangeError (Table *table);
	void invalidate();
	void dropView (Syntax *syntax);
	bool execute (const char *sqlString, bool isQuery);
	void dropUser (Syntax *syntax);
	virtual bool isPreparedResultSet();
	void reIndex (Syntax *syntax);
	void clearConnection();
	char* getString (Syntax *node, char *buffer, int length);
	void dropCoterie (Syntax *syntax);
	void createCoterie (Syntax *syntax, bool upgrade);
	void disableTriggerClass(Syntax *syntax);
	void enableTriggerClass (Syntax *syntax);
	void disableFilterSet (Syntax *syntax);
	void enableFilterSet (Syntax *syntax);
	void dropFilterSet (Syntax *syntax);
	void createFilterSet (Syntax *syntax, bool upgrade);
	void alterTrigger (Syntax *syntax);
	void revokePriv (Syntax *syntax);
	void dropIndex (Index *index);
	void dropIndex (Syntax *syntax);
	void dropTrigger (Syntax *syntax);
	void createTrigger (Syntax *syntax, bool update);
	void dropSequence (Syntax *syntax);
	void createSequence (Syntax *syntax, bool upgrade);
	void createView (Syntax *syntax, bool upgrade);
	void dropRole (Syntax *syntax);
	void revokeRole (Syntax *syntax);
	void alterUser (Syntax *syntax);
	ResultSet* getResultSet (int sequence);
	void releaseJava();
	void addJavaRef();
	void connectionClosed();
	const char* getName (Syntax *syntax);
	const char* getSchemaName (Syntax *syntax);
	void grantRole (Syntax *syntax);
	int32 getPrivilegeMask (SyntaxType type);
	void createRole (Syntax *syntax, bool upgrade);
	void grantPriv (Syntax *syntax);
	void createUser (Syntax *syntax);
	void clearResults(bool forceRelease);
	Index* createPrimaryKey (Table *table, Field *field);
	Index* createPrimaryKey (Table *table, Syntax *item);
	bool upgradeField (Table *table, Field *field, Syntax *item, LinkedList &newIndexes);
	void getFieldType (Syntax *node, FieldType *type);
	void reset();
	void upgradeTable (Syntax *syntax);
	void release();
	void addRef();
	virtual void setCursorName (const char *name);
	virtual ResultSet* executeQuery (const char *sqlString);
	virtual int executeUpdate (const char *sqlString);
	virtual ResultSet* getResultSet();
	virtual ResultList* search (const char *string);
	virtual void close();
	virtual ResultSet* executeQuery();
	virtual void clearParameters();
	virtual int getParameterCount();
	virtual bool execute (const char *sqlString);
	virtual int getUpdateCount();
	virtual void setTableFilter (const char *tableName);
	virtual bool getMoreResults();
	virtual void setTableFilter (const WCString *tableName);
	virtual JString analyze(int mask);

	ForeignKey	*addForeignKey (Table *foreignTable, Field *column, Syntax *syntax);
	Field		*addField (Table *table, Syntax* item);
	void		alterField (Table *table, Syntax *syntax);
	void		alterTable (Syntax *syntax);
	void		deleteResultList (ResultList *resultList);
	void		deleteSort (int slot);
	void		dropTable (Syntax *syntax);
	void		executeDDL();
	Context		*getUpdateContext();
	void		deleteResultSet(ResultSet *resultSet);
	ResultSet	*createResultSet (NSelect *node, int numberColumns);
	Value		*getValue (int slot);
	Context		*getContext (int contextId);
	void		start (NNode *start);
	void		prepareStatement();
	Value		*getParameter (int index);
	void		allocParameters(int count);

	Statement(Connection *connection, Database *db);

	Database			*database;
	Statement			*parent;
	Connection			*connection;
	CompiledStatement	*statement;
	Values				parameters;
	Values				values;
	int32				*slots;
	Sort				**sorts;
	Bitmap				**bitmaps;
	ValueSet			**valueSets;
	Row					**rowSlots;
	int32				handle;
	int					numberContexts;
	int					numberSorts;
	int					numberBitmaps;
	int					numberValueSets;
	int					numberRowSlots;
	Transaction			*transaction;
	ResultSet			*resultSets;
	ResultList			*resultLists;
	const char			*cursorName;
	int					currentResultSet;
	int					recordsUpdated;
	Bitmap				*tableFilter;
	Statement			*next;				// sibling relative to CompiledStatement
	Statement			*prior;				// sibling relative to CompiledStatement
	Statement			*connectionNext;	// sibling relative to Connection
	bool				eof;
	bool				active;
	bool				updateStatements;
	bool				special;
	StatementStats		stats;

protected:
	virtual ~Statement();
	void createIndex (Syntax *syntax, bool upgrade);
	void createTable (Syntax *syntax);

	Context				*contexts;
	volatile INTERLOCK_TYPE		useCount;
	volatile INTERLOCK_TYPE		javaCount;
	SyncObject			syncObject;
public:
	void createTableSpace (Syntax *syntax);
	void transactionEnded(void);
	void dropTableSpace(Syntax* syntax);
};

END_NAMESPACE

#endif // !defined(AFX_STATEMENT_H__02AD6A41_A433_11D2_AB5B_0000C01D2301__INCLUDED_)

