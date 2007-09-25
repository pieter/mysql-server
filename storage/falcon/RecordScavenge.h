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

#ifndef _RECORD_SCAVENGE_H_
#define _RECORD_SCAVENGE_H_

static const int AGE_GROUPS = 20;

class Database;
class Record;

class RecordScavenge
{
public:
	Database	*database;
	TransId		transactionId;
	int			scavengeGeneration;
	int			baseGeneration;
	uint		recordsReclaimed;
	uint		recordsRemaining;
	uint		numberRecords;
	uint		versionsRemaining;
	uint64		spaceReclaimed;
	uint64		spaceRemaining;
	uint64		ageGroups[AGE_GROUPS];
	uint64		overflowSpace;
	uint64		totalSpace;
	uint64		recordSpace;
	
	RecordScavenge(Database *db, TransId oldestTransaction);
	~RecordScavenge(void);

	void		inventoryRecord(Record* record);
	int computeThreshold(uint64 target);
	void printRecordMemory(void);
};

#endif
