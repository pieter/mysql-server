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

// MetaDataResultSet.h: interface for the MetaDataResultSet class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_METADATARESULTSET_H__C3220BA2_0199_11D3_AB6F_0000C01D2301__INCLUDED_)
#define AFX_METADATARESULTSET_H__C3220BA2_0199_11D3_AB6F_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "ResultSet.h"

class MetaDataResultSet : public ResultSet  
{
public:
	virtual Statement* getStatement();
	virtual void close();
	virtual bool wasNull();
	virtual bool next();
	virtual Value* getValue(int index);
	virtual Value* getValue(const char * name);
	MetaDataResultSet(ResultSet *source);
	virtual ~MetaDataResultSet();

	ResultSet *resultSet;
};

#endif // !defined(AFX_METADATARESULTSET_H__C3220BA2_0199_11D3_AB6F_0000C01D2301__INCLUDED_)
