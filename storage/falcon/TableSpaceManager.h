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


// TableSpaceManager.h: interface for the TableSpaceManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TABLESPACEMANAGER_H__BD1D39F6_2201_4136_899C_7CB106E99B8C__INCLUDED_)
#define AFX_TABLESPACEMANAGER_H__BD1D39F6_2201_4136_899C_7CB106E99B8C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SyncObject.h"

static const int TS_HASH_SIZE = 101;

class TableSpace;
class Database;
class Transaction;

class TableSpaceManager  
{
public:
	TableSpaceManager(Database *db);
	virtual ~TableSpaceManager();

	TableSpace*		getTableSpace (int id);
	TableSpace*		findTableSpace(int id);
	void			bootstrap (int sectionId);
	TableSpace*		createTableSpace (const char *name, const char *fileName, uint64 initialAllocation);
	TableSpace*		getTableSpace (const char *name);
	TableSpace*		findTableSpace(const char *name);
	void			add (TableSpace *tableSpace);
	void			shutdown(TransId transId);
	void			dropDatabase(void);
	void			dropTableSpace(TableSpace* tableSpace);
	void			reportStatistics(void);
	void			validate(int optionMask);
	void			sync(uint threshold);
	void			expungeTableSpace(int tableSpaceId);
	void			reportWrites(void);
	void			redoCreateTableSpace(int id, int nameLength, const char* name, int fileNameLength, const char* fileName);
	void			initialize(void);

	Database	*database;
	TableSpace	*tableSpaces;
	TableSpace	*nameHash[TS_HASH_SIZE];
	TableSpace	*idHash[TS_HASH_SIZE];
	SyncObject	syncObject;
};

#endif // !defined(AFX_TABLESPACEMANAGER_H__BD1D39F6_2201_4136_899C_7CB106E99B8C__INCLUDED_)
