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

// SortRecord.h: interface for the SortRecord class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SORTRECORD_H__84FD1962_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_SORTRECORD_H__84FD1962_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Row.h"

class Value;
class Record;
class Collation;
CLASS(Statement);
//class Row;

struct SortParameters {
    bool	direction;
	Collation	*collation;
	};


class SortRecord : public Row
{
public:
	virtual Value* getValue (Statement *statement, int index);
	int compare (SortParameters *sortParameters, SortRecord *record);
	SortRecord(Statement *statement, Row *row, int keyCount, int recordCount, Record **obj);
	virtual ~SortRecord();

	short	numberKeys;
	short	numberRecords;
	Value	*keys;
	Record	**object;
	Row		*row;
};

#endif // !defined(AFX_SORTRECORD_H__84FD1962_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
