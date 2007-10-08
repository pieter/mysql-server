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

// RecordGroup.cpp: implementation of the RecordGroup class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "RecordGroup.h"
#include "RecordLeaf.h"
#include "Record.h"
#include "Interlock.h"
#include "Table.h"
#include "Sync.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RecordGroup::RecordGroup(int32 recordBase)
{
	base = recordBase;
	memset (records, 0, sizeof (records));
}

RecordGroup::~RecordGroup()
{
	for (int n = 0; n < RECORD_SLOTS; ++n)
		if (records [n])
			delete records [n];
}

RecordGroup::RecordGroup(int32 recordBase, int32 id, RecordSection * section)
{
	base = recordBase;
	memset(records, 0, sizeof (records));
	records[0] = section;
}

Record* RecordGroup::fetch(int32 id)
{
	int slot = id / base;

	if (slot >= RECORD_SLOTS)
		return NULL;

	RecordSection *section = records[slot];

	if (!section)
		return NULL;

	return section->fetch(id % base);
}


bool RecordGroup::store(Record * record, Record *prior, int32 id, RecordSection **parentPtr)
{
	int slot = id / base;

	// If record doesn't fit in this group, create another level,
	// then store record in new group.
		
	if (slot >= RECORD_SLOTS)
		{
		RecordGroup *section = new RecordGroup(base * RECORD_SLOTS, id, this);
		
		if (COMPARE_EXCHANGE_POINTER(parentPtr, this, section))
			return section->store(record, prior, id, parentPtr);
		
		section->records[0] = NULL;
		delete section;
		
		return (*parentPtr)->store(record, prior, id, parentPtr);
		}

	RecordSection **ptr = records + slot;
	RecordSection *section = *ptr;

	if (!section)
		{
		if (base == RECORD_SLOTS)
			section = new RecordLeaf();
		else
			section = new RecordGroup(base / RECORD_SLOTS);

		if (!COMPARE_EXCHANGE_POINTER(ptr, NULL, section))
			section = *ptr;
		}

	return section->store(record, prior, id % base, NULL);
}

int RecordGroup::retireRecords(Table *table, int base, RecordScavenge *recordScavenge)
{
	int count = 0;
	int recordNumber = base * RECORD_SLOTS;

	for (RecordSection **ptr = records, **end = records + RECORD_SLOTS; ptr < end; ++ptr, ++recordNumber)
		{
		RecordSection *section = *ptr;
		
		if (section)
			{
			int n = section->retireRecords(table, recordNumber, recordScavenge);
			count += n;
			
			/***
			if (n)
				count += n;
			else
				{
				delete section;
				*ptr = NULL;
				}
			***/
			}
		}

	return count;
}

int RecordGroup::countActiveRecords()
{
	int count = 0;

	for (RecordSection **ptr = records, **end = records + RECORD_SLOTS; ptr < end; ++ptr)
		{
		RecordSection *section = *ptr;
		
		if (section)
			count += section->countActiveRecords();
		}

	return count;
}

bool RecordGroup::inactive()
{
	for (int slot = 0; slot < RECORD_SLOTS; slot++)
		if (records[slot])
			return false;

	return true;
}

bool RecordGroup::retireSections(Table * table, int id)
{
	int slot = id / base;

	if (slot < RECORD_SLOTS)
		{
		RecordSection *section = records[slot];
		
		if (section)
			{
			int nextId = id % base;
		
			// Delete inactive child sections
			
			if (section->retireSections(table, nextId))
				{
				delete records[slot];
				records[slot] = NULL;
				}
			}
		}
		
	return inactive();
}

void RecordGroup::inventoryRecords(RecordScavenge* recordScavenge)
{
	for (RecordSection **section = records, **end = records + RECORD_SLOTS; section < end; ++section)
		if (*section)
			(*section)->inventoryRecords(recordScavenge);
}
