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

// ResultList.h: interface for the ResultList class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RESULTLIST_H__84FD1977_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_RESULTLIST_H__84FD1977_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SearchHit.h"

class Database;
class ResultSet;
class PreparedStatement;
CLASS(Statement);
class Connection;
class Bitmap;
CLASS(Field);

class ResultList
{
public:
	const char* getSchemaName();
	void init(Connection *cnct);
	ResultList (Connection *cnct, Field *field);
	void clearStatement();
	virtual const char* getTableName();
	double getScore();
	virtual int getCount();
	virtual void close();
	virtual ResultSet* fetchRecord();
	virtual bool next();
	virtual void print();
	void release();
	void addRef();
	void sort();
	SearchHit* add (int32 tableId, int32 recordNumber, int32 fieldId, double score);
	ResultList(Statement *stmt);

	int				count;
	int32			handle;
	volatile INTERLOCK_TYPE	useCount;
	Connection		*connection;
	Database		*database;
	SearchHit		*nextHit;
	Statement		*parent;
	PreparedStatement	*statement;
	SearchHit		hits;
	Bitmap			*tableFilter;
	ResultList		*sibling;
	Field			*fieldFilter;

//protected:			// allocated on stack, so destructor must be accessible	
	virtual ~ResultList();
};

#endif // !defined(AFX_RESULTLIST_H__84FD1977_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
