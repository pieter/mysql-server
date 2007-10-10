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

// FilterSet.h: interface for the FilterSet class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILTERSET_H__1F63A280_9414_11D5_899A_CC4599000000__INCLUDED_)
#define AFX_FILTERSET_H__1F63A280_9414_11D5_899A_CC4599000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define FILTERSET_HASH_SIZE	101

class TableFilter;
class Table;
class SQLParse;
class Syntax;
class Database;

class FilterSet  
{
public:
	void release();
	void addRef();
	void save();
	void setText (const char *string);
	TableFilter* findFilter (Table *table);
	void addFilter (TableFilter *filter);
	FilterSet(Database *db, const char *filterSchema, const char *filterName);
	JString stripSQL (const char *source);

protected:
	virtual ~FilterSet();

public:
	void clear();
	int			useCount;
	Database	*database;
	const char	*name;
	const char	*schema;
	JString		sql;
	Syntax		*syntax;
	SQLParse	*parse;
	FilterSet	*collision;
	TableFilter	*filters [FILTERSET_HASH_SIZE];
};

#endif // !defined(AFX_FILTERSET_H__1F63A280_9414_11D5_899A_CC4599000000__INCLUDED_)
