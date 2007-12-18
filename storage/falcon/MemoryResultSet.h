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

// MemoryResultSet.h: interface for the MemoryResultSet class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MEMORYRESULTSET_H__E4CF0403_0ECC_11D3_AB71_0000C01D2301__INCLUDED_)
#define AFX_MEMORYRESULTSET_H__E4CF0403_0ECC_11D3_AB71_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "ResultSet.h"
#include "LinkedList.h"

class MemoryResultSetColumn;

class MemoryResultSet : public ResultSet
{
public:
	void setDouble(int index, double value);
	MemoryResultSetColumn* getColumn (int index);
	virtual ResultSetMetaData* getMetaData();
	virtual int findColumn(const WCString *name);
	virtual int getColumnIndex(const WCString *name);
	virtual int findColumnIndex (const char *name);
	virtual Value* getValue (int index);
	virtual int getColumnIndex (const char *name);
	virtual const char* getString (int index);
	virtual bool next();
	virtual void setInt (int index, int value);
	virtual void setString (int index, const char *string);
	virtual void addRow();
	virtual void addColumn (const char *name, const char *label, int type, int displaySize, int precision, int scale);
	MemoryResultSet();
	virtual ~MemoryResultSet();

	Values		*row;
	LinkedList	rows;
	LinkedList	columns;
	int			numberRows;
};

#endif // !defined(AFX_MEMORYRESULTSET_H__E4CF0403_0ECC_11D3_AB71_0000C01D2301__INCLUDED_)
