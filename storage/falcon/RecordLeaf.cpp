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

// RecordLeaf.cpp: implementation of the RecordLeaf class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "RecordLeaf.h"
#include "RecordGroup.h"
#include "RecordVersion.h"
#include "Table.h"
#include "Sync.h"
#include "Interlock.h"
#include "Bitmap.h"
#include "RecordScavenge.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RecordLeaf::RecordLeaf()
{
	base = 0;
	memset (records, 0, sizeof (records));
	syncObject.setName("RecordLeaf::syncObject");
}

RecordLeaf::~RecordLeaf()
{
	for (int n = 0; n < RECORD_SLOTS; ++n)
		if (records[n])
			{
#ifdef CHECK_RECORD_ACTIVITY
			for (Record *rec = records[n]; rec; rec = rec->getPriorVersion())
				rec->active = false;
#endif
				
			records[n]->release();
			}
}

Record* RecordLeaf::fetch(int32 id)
{
	if (id >= RECORD_SLOTS)
		return NULL;

	Sync sync(&syncObject, "RecordLeaf::fetch");
	sync.lock(Shared);

	Record *record = records[id];

	if (record)
		record->addRef();

	return record;
}


bool RecordLeaf::store(Record *record, Record *prior, int32 id, RecordSection **parentPtr)
{
	// If this doesn't fit, create a new level above use, then store
	// record in new record group.

	if (id >= RECORD_SLOTS)
		{
		RecordGroup* group = new RecordGroup (RECORD_SLOTS, 0, this);
		
		if (COMPARE_EXCHANGE_POINTER(parentPtr, this, group))
			 return group->store(record, prior, id, parentPtr);

		group->records[0] = NULL;
		delete group;
		
		return (*parentPtr)->store(record, prior, id, parentPtr);
		}

	// If we're adding a new version, don't bother with a lock.  Otherwise we need to lock out
	// simultaneous fetches to avoid a potential race between addRef() and release().

	if (record && record->getPriorVersion() == prior)
		{
		if (!COMPARE_EXCHANGE_POINTER(records + id, prior, record))
			return false;
		}
	else
		{
		Sync sync(&syncObject, "RecordLeaf::store");
		sync.lock(Exclusive);

		if (!COMPARE_EXCHANGE_POINTER(records + id, prior, record))
			return false;
		}

	return true;
}

int RecordLeaf::retireRecords (Table *table, int base, RecordScavenge *recordScavenge)
{
	int count = 0;
	Record **ptr, **end;
	Sync sync(&syncObject, "RecordLeaf::retireRecords");
	sync.lock(Shared);
	
	// Get a shared lock to find at least one record to scavenge
	
	for (ptr = records, end = records + RECORD_SLOTS; ptr < end; ++ptr)
		{
		Record *record = *ptr;
		
		if (record)
			{
			if (record->isVersion())
				{
				if ((record->scavenge(recordScavenge)) &&
				    ((!record->hasRecord()) || ((record->useCount == 1) && (record->generation <= recordScavenge->scavengeGeneration))))
				    break;
				else
					++count;
				}
			else if (record->generation <= recordScavenge->scavengeGeneration && record->useCount == 1)
				break;
			else
				++count;
			}
		}

	if (ptr >= end)
		return count;
	
	// Get an exclusive lock and do the actual scavenging
	
	sync.unlock();
	sync.lock(Exclusive);
	count = 0;
	
	for (ptr = records; ptr < end; ++ptr)
		{
		Record *record = *ptr;
		
		if (record)
			{
			if (record->isVersion())
				{
				if ((record->scavenge(recordScavenge)) &&
				    ((!record->hasRecord()) || ((record->useCount == 1) && (record->generation <= recordScavenge->scavengeGeneration))))
					{
					*ptr = NULL;
					recordScavenge->spaceReclaimed += record->size;
					++recordScavenge->recordsReclaimed;
#ifdef CHECK_RECORD_ACTIVITY
					record->active = false;
#endif
					record->release();
					}
				else
					{
					++recordScavenge->recordsRemaining;
					recordScavenge->spaceRemaining += record->size;
					++count;
					}
				}
			else if (record->generation <= recordScavenge->scavengeGeneration && record->useCount == 1)
				{
				*ptr = NULL;
				recordScavenge->spaceReclaimed += record->size;
				++recordScavenge->recordsReclaimed;
#ifdef CHECK_RECORD_ACTIVITY
				record->active = false;
#endif
				record->release();
				}
			else
				{
				++recordScavenge->recordsRemaining;
				recordScavenge->spaceRemaining += record->size;
				++count;
				
				for (Record *prior = record->getPriorVersion(); prior; prior = prior->getPriorVersion())
					{
					++recordScavenge->versionsRemaining;
					recordScavenge->spaceRemaining += prior->size;
					}
				}
			}
		}
		
	// If this node is empty, store the base record number for use as an
	// identifier when the leaf node is scavenged later.
	
	if (!count && table->emptySections)
		table->emptySections->set(base * RECORD_SLOTS);

	return count;
}

bool RecordLeaf::retireSections(Table * table, int id)
{
	return inactive();
}

bool RecordLeaf::inactive()
{
	return countActiveRecords() == 0;
}

int RecordLeaf::countActiveRecords()
{
	int count = 0;

	for (Record **ptr = records, **end = records + RECORD_SLOTS; ptr < end; ++ptr)
		if (*ptr)
			++count;

	return count;
}

void RecordLeaf::inventoryRecords(RecordScavenge* recordScavenge)
{
	Sync sync(&syncObject, "RecordLeaf::inventoryRecords");
	sync.lock(Shared);

	for (Record **ptr = records, **end = records + RECORD_SLOTS; ptr < end; ++ptr)
		if (*ptr)
			recordScavenge->inventoryRecord(*ptr);
}
