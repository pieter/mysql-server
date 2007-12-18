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

// ResultSetMetaData.h: interface for the ResultSetMetaData class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RESULTSETMETADATA_H__84FD1961_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_RESULTSETMETADATA_H__84FD1961_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Types.h"

static const int columnNoNulls = 0;
static const int columnNullable = 1;
static const int columnNullableUnknown = 2;

class ResultSet;
CLASS(Statement);

class ResultSetMetaData  
{
public:
	const char* getRepositoryName (int index);
	const char* getColumnTypeName (int index);
	void checkResultSet();
	void resultSetClosed();
	void releaseJavaRef();
	void addJavaRef();
	virtual JdbcType getColumnType(int index);
	virtual const char* getTableName (int index);
	virtual const char* getColumnName (int index);
	virtual int getColumnCount();
	virtual const char* getSchemaName (int index);
	virtual const char* getQuery();
	virtual int getScale (int index);
	virtual int getPrecision (int index);
	virtual int getColumnDisplaySize (int column);
	virtual const char* getCatalogName (int index);
	virtual bool isSearchable (int index);
	virtual bool isNullable (int index);
	virtual const char* getCollationSequence (int index);
	virtual const char* getDefaultValue(int index);

	ResultSetMetaData(ResultSet *results);

protected:
	virtual ~ResultSetMetaData();
	void	release();
	void	addRef();

	ResultSet		*resultSet;
	JString			sqlString;
	volatile INTERLOCK_TYPE	useCount;
};

#endif // !defined(AFX_RESULTSETMETADATA_H__84FD1961_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
