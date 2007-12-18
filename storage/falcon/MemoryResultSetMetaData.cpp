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

// MemoryResultSetMetaData.cpp: implementation of the MemoryResultSetMetaData class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "MemoryResultSetMetaData.h"
#include "MemoryResultSet.h"
#include "MemoryResultSetColumn.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

MemoryResultSetMetaData::MemoryResultSetMetaData(ResultSet *resultSet) : ResultSetMetaData (resultSet)
{

}

MemoryResultSetMetaData::~MemoryResultSetMetaData()
{

}

const char* MemoryResultSetMetaData::getTableName(int index)
{
	return "";
}

const char* MemoryResultSetMetaData::getColumnName(int index)
{
	MemoryResultSetColumn *column = ((MemoryResultSet*) resultSet)->getColumn (index);

	return column->name;
}

const char* MemoryResultSetMetaData::getSchemaName(int index)
{
	return "";
}

const char* MemoryResultSetMetaData::getQuery()
{
	return "";
}

JdbcType MemoryResultSetMetaData::getColumnType(int index)
{
	//MemoryResultSetColumn *column = ((MemoryResultSet*) resultSet)->getColumn (index);

	return VARCHAR;
}
