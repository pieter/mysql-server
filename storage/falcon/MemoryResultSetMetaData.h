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

// MemoryResultSetMetaData.h: interface for the MemoryResultSetMetaData class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MEMORYRESULTSETMETADATA_H__E1F54F74_8A1E_11D6_B911_00E0180AC49E__INCLUDED_)
#define AFX_MEMORYRESULTSETMETADATA_H__E1F54F74_8A1E_11D6_B911_00E0180AC49E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResultSetMetaData.h"
//#include "Types.h"	// Added by ClassView

class MemoryResultSetMetaData : public ResultSetMetaData  
{
public:
	virtual JdbcType getColumnType(int index);
	virtual const char* getQuery();
	virtual const char* getSchemaName (int index);
	virtual const char* getColumnName (int index);
	virtual const char* getTableName (int index);
	MemoryResultSetMetaData(ResultSet *resultSet);
	virtual ~MemoryResultSetMetaData();

};

#endif // !defined(AFX_MEMORYRESULTSETMETADATA_H__E1F54F74_8A1E_11D6_B911_00E0180AC49E__INCLUDED_)
