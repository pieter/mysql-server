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

// RecordSection.h: interface for the RecordSection class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RECORDSECTION_H__02AD6A54_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_RECORDSECTION_H__02AD6A54_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class Record;
class Table;
class RecordScavenge;


#define RECORD_SLOTS		100

class RecordSection  
{
public:
	virtual bool retireSections(Table * table, int id) = 0;
	virtual bool inactive() = 0;
	virtual		~RecordSection();
	
	virtual	Record*	fetch (int32 id) = 0;
	virtual	bool	store (Record *record, Record *prior, int32 id, RecordSection **parentPtr) = 0;
	virtual	int		retireRecords(Table *table, int base, RecordScavenge *recordScavenge) = 0;
	virtual	void	inventoryRecords(RecordScavenge* recordScavenge) = 0;
	virtual	int		countActiveRecords() = 0;

	int32			base;
};

#endif // !defined(AFX_RECORDSECTION_H__02AD6A54_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
