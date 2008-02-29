/* Copyright (C) 2008 MySQL AB

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

#ifndef _BACKLOG_H_
#define _BACKLOG_H_

#define DEBUG_BACKLOG

class Database;
class Dbb;
class Section;
class RecordVersion;
class Bitmap;
class Transaction;

class BackLog
{
public:
	BackLog(Database *database, const char *fileNam);
	virtual ~BackLog(void);

	RecordVersion*	fetch(int32 backlogId);
	int32			save(RecordVersion* record);
	void			update(int32 backlogId, RecordVersion* record);
	void			deleteRecord(int32 backlogId);
	void			rollbackRecords(Bitmap* records, Transaction *transaction);
	
	Database	*database;
	Dbb			*dbb;
	Section		*section;
};

#endif
