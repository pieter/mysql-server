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

// CompiledStatement.h: interface for the CompiledStatement class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COMPILEDSTATEMENT_H__02AD6A47_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_COMPILEDSTATEMENT_H__02AD6A47_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Stack.h"
#include "LinkedList.h"
#include "SyncObject.h"
#include "Types.h"

class Database;
class Syntax;
class SQLParse;
class NNode;
class NSelect;
CLASS(Field);
class Table;
class Context;
class NField;
class Fsb;
class LinkedList;
class Index;
class Connection;
CLASS(Statement);
class View;
class Sequence;
class FilterSet;
class FilterSetManager;
class TableFilter;
class Row;

class CompiledStatement  
{
public:
	void		renameTables (Syntax *syntax);
	int			getRowSlot();
	int			getValueSetSlot();
	void		invalidate();
	Type		getType (Syntax *syntax);
	void		decomposeAdjuncts (Syntax *syntax, LinkedList &adjuncts);
	int			getBitmapSlot();
	int			countInstances();
	bool		addFilter (TableFilter *filter);
	FilterSet*	findFilterset (Syntax *syntax);
	void		checkAccess (Connection *connection);
	Sequence*	findSequence (Syntax *syntax, bool search);
	bool		isInvertible (Context *context, NNode *node);
	void*		markContextStack();
	void		popContexts (void *mark);
	View*		getView (const char *sql);
	void		deleteInstance (Statement *instance);
	void		addInstance (Statement *instance);
	void		addRef();
	void		compile (JString sqlStr);
	NNode*		compile (Syntax *syntax);
	NNode*		compileField (Syntax *syntax);
	NNode*		compileField (Field *field, Context *context);
	NNode*		compileFunction (Syntax *syntax);
	NNode*		compileSelect(Syntax *syntax, NNode *inExpr);
	Fsb*		compileStream (Context *context, LinkedList &conjuncts, Row *row);
	bool		contextComputable (int contextId);
	Table*		findTable (Syntax *syntax, bool search);
	NNode*		genInversion (Context *context, Index *index, LinkedList& booleans);
	NNode*		mergeInversion (NNode *node1, NNode *node2);
	NNode*		isDirectReference (Context *context, NNode *node);
	NNode*		genInversion (Context *context, NNode *node, LinkedList *booleans);
	int			getValueSlot();
	int			getGeneralSlot();
	int			getSortSlot();
	Table*		getTable (Syntax *syntax);
	bool		references (Table *table);
	void		noteUnqualifiedTable(Table *table);
	Context*	compileContext (Syntax *syntax, int32 privMask);
	Context*	makeContext (Table *table, int32 privMask);
	Context*	popContext();
	Context* getContext (int contextId);
	void		pushContext (Context *context);
	void		release();
	bool		validate (Connection *connection);
	JString		 getNameString (Syntax *syntax);

	static Table*	getTable (Connection *connection, Syntax *syntax);
	static Field* findField (Table *table, Syntax *syntax);

	CompiledStatement(Connection *connection);
	virtual ~CompiledStatement();

	SyncObject	syncObject;
	JString		sqlString;
	Database	*database;
	volatile INTERLOCK_TYPE	useCount;
	SQLParse	*parse;
	Syntax		*syntax;
	NNode		*node;
	NNode		*nodeList;
	NSelect		*select;
	LinkedList	contexts;
	Stack		filters;
	int			numberParameters;
	int			numberValues;
	int			numberContexts;
	int			numberSlots;
	int			numberSorts;
	int			numberBitmaps;
	int			numberValueSets;
	int			numberRowSlots;
	Stack		contextStack;
	Stack		unqualifiedTables;
	Stack		unqualifiedSequences;
	Stack		filteredTables;
	bool		ddl;
	bool		useable;
	time_t		lastUse;

	Connection			*connection;		// valid only during compile
	Statement			*firstInstance;
	Statement			*lastInstance;
	CompiledStatement	*next;				// next in database
};

#endif // !defined(AFX_COMPILEDSTATEMENT_H__02AD6A47_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
