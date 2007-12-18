/* Copyright (C) 2007 MySQL AB

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

#include <memory.h>
#include "Engine.h"
#include "RecordScavenge.h"
#include "Database.h"
#include "Record.h"
#include "Log.h"
#include "MemMgr.h"


RecordScavenge::RecordScavenge(Database *db, TransId oldestTransaction)
{
	database = db;
	transactionId = oldestTransaction;
	baseGeneration = database->currentGeneration;
	memset(ageGroups, 0, sizeof(ageGroups));
	recordsReclaimed = 0;
	recordsRemaining = 0;
	versionsRemaining = 0;
	spaceReclaimed = 0;
	spaceRemaining = 0;
	overflowSpace = 0;
	numberRecords = 0;
	recordSpace = 0;
}

RecordScavenge::~RecordScavenge(void)
{
}

void RecordScavenge::inventoryRecord(Record* record)
{
	for (Record *rec = record; rec; rec = rec->getPriorVersion())
		{
		++numberRecords;
		recordSpace += record->size;
		int age = baseGeneration - record->generation;
		int size = record->size + sizeof(MemBigHeader);
		
		if (record->hasRecord() || (record->state == recChilled))
			size += sizeof(MemBigHeader);
			
		if (age >= 0 && age < AGE_GROUPS)
			ageGroups[age] += size;
		else if (age >= AGE_GROUPS)
			overflowSpace += size;
		else
			ageGroups[0] = size;
		}
}

int RecordScavenge::computeThreshold(uint64 target)
{
	totalSpace = 0;
	scavengeGeneration = 0;
	
	for (int n = 0; n < AGE_GROUPS; ++n)
		{
		totalSpace += ageGroups[n];
		
		if (totalSpace >= target && scavengeGeneration == 0)
			scavengeGeneration = baseGeneration - n;
		}

	totalSpace += overflowSpace;

	if (scavengeGeneration == 0 && totalSpace > target)
		scavengeGeneration = baseGeneration + AGE_GROUPS;
	
	return scavengeGeneration;
}

void RecordScavenge::printRecordMemory(void)
{
	Log::debug ("Record Memory usage for %s:\n", (const char*) database->name);
	int max;

	for (max = AGE_GROUPS - 1; max > 0; --max)
		if (ageGroups[max])
			break;

	for (int n = 0; n <= max; ++n)
		if (ageGroups [n])
			Log::debug ("  %d. %d\n", baseGeneration - n, ageGroups[n]);

	Log::log(LogScavenge, " total: " I64FORMAT ", threshold %d%s\n", totalSpace, scavengeGeneration,
				(scavengeGeneration > 0) ? " -- scavenge" : "");
}
