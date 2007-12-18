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

// Field.h: interface for the Field class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FIELD_H__02AD6A43_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_FIELD_H__02AD6A43_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Types.h"

#define NOT_NULL			1
#define SEARCHABLE			2
#define CASE_INSENSITIVE	4

class Table;
class ForeignKey;
class Transaction;
class Collation;
class Value;
class Repository;

enum JdbcType;

START_NAMESPACE
class Field
{
public:
	bool isString();
	const char* getSqlTypeName();
	void setRepository (Repository *repo);
	void setCollation (Collation *newCollation);
	void makeNotSearchable (Transaction *transaction, bool populate);
	static int getPrecision (Type type, int length);
	bool isSearchable();
	void drop();
	void save();
	JdbcType getSqlType();
	void setNotNull();
	void update();
	void makeSearchable(Transaction *transaction, bool populate);
	int getDisplaySize();
	int getPhysicalLength();
	static int boundaryRequirement (Type type);
	int boundaryRequirement();
	bool getNotNull();
	const char* getName();
	Field (Table *tbl, int fieldId, const char *fieldName, Type typ, int len, int precision, int scale, int flags);
	virtual ~Field();

	Table		*table;
	Collation	*collation;
	Repository	*repository;
	const char	*name;
	const char	*domainName;
	ForeignKey	*foreignKey;		// used only during DDL processing
	Type		type;
	Field		*next;				// next in table
	Value		*defaultValue;
	int			length;
	int			id;
	int			precision;
	int			scale;
	int			flags;
	char		indexPadByte;
};
END_NAMESPACE

#endif // !defined(AFX_FIELD_H__02AD6A43_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
