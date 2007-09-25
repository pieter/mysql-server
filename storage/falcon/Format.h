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

// Format.h: interface for the Format class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FORMAT_H__02AD6A4E_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_FORMAT_H__02AD6A4E_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class Table;
class ResultSet;

struct FieldFormat {
	short	type;
	short	nullPosition;
	short	scale;
	short	index;
    int32	offset;
	int32	length;
	};

class Format  
{
public:
	void save (Table *table);
	Format (Table *tbl, ResultSet *resultSet);
	bool validate (Table *table);
	Format(Table *table, int newVersion);
	virtual ~Format();

	int32		version;
	int32		maxId;
	int32		length;
	int			count;
	FieldFormat	*format;
	Table		*table;
	Format		*hash;
	bool		saved;
};

#endif // !defined(AFX_FORMAT_H__02AD6A4E_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
