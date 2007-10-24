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

// RecordLeaf.h: interface for the RecordLeaf class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RECORDLEAF_H__02AD6A56_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_RECORDLEAF_H__02AD6A56_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "RecordSection.h"
#include "SyncObject.h"


class RecordLeaf : public RecordSection  
{
public:
	virtual int countActiveRecords();
	virtual int retireRecords(Table *table, int id, RecordScavenge *recordScavenge);
	virtual bool retireSections(Table * table, int id);
	virtual bool inactive();
	virtual bool store(Record *record, Record *prior, int32 id,RecordSection **parentPtr);
	virtual Record* fetch(int32 id);
	RecordLeaf();
	virtual ~RecordLeaf();

	Record		*records [RECORD_SLOTS];
	SyncObject	syncObject;
	void inventoryRecords(RecordScavenge* recordScavenge);
};

#endif // !defined(AFX_RECORDLEAF_H__02AD6A56_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
