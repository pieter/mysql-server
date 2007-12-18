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

// TableFilter.h: interface for the TableFilter class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TABLEFILTER_H__1F63A281_9414_11D5_899A_CC4599000000__INCLUDED_)
#define AFX_TABLEFILTER_H__1F63A281_9414_11D5_899A_CC4599000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Table;
class Syntax;
class NNode;

class TableFilter  
{
public:
	void release();
	void addRef();
	TableFilter(const char *table, const char *schema, const char *aliasName, Syntax *syntax);

protected:
	virtual ~TableFilter();

public:
	const char	*tableName;
	const char	*schemaName;
	const char	*alias;
	Syntax		*syntax;
	TableFilter	*collision;
	int			useCount;
};

#endif // !defined(AFX_TABLEFILTER_H__1F63A281_9414_11D5_899A_CC4599000000__INCLUDED_)
