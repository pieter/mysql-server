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

// Context.h: interface for the Context class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CONTEXT_H__02AD6A58_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_CONTEXT_H__02AD6A58_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

enum ContextType {
   CtxInnerJoin,
   CtxOuterJoin,
   CtxInsert,
   };

class Table;
CLASS(Statement);
class Record;
class Bitmap;
class Sort;
class LinkedList;

class Context  
{
public:
	void checkRecordLimits (Statement *statement);
	void setRecord(Record *record);
	void close();
	void open();
	void getTableContexts (LinkedList& list, ContextType contextType);
	bool isContextName (const char *name);
	bool fetchIndexed (Statement *statement);
	void setBitmap (Bitmap *map);
	bool fetchNext (Statement *statement);
	void initialize (Context *context);
	 Context();
	bool setComputable (bool flag);
	Context(int id, Table *tbl, int32 privMask);
	virtual ~Context();

	Table		*table;
	Context		**viewContexts;
	int			contextId;
	bool		computable;
	bool		eof;
	bool		select;
	const char	*alias;
	int32		recordNumber;
	int32		privilegeMask;
	Record		*record;
	Bitmap		*bitmap;
	Sort		*sort;
	ContextType	type;
};

#endif // !defined(AFX_CONTEXT_H__02AD6A58_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
