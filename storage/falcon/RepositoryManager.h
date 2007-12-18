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

// RepositoryManager.h: interface for the RepositoryManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REPOSITORYMANAGER_H__95325AB6_5CE2_4F3F_AE52_00E0923BF209__INCLUDED_)
#define AFX_REPOSITORYMANAGER_H__95325AB6_5CE2_4F3F_AE52_00E0923BF209__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Schedule.h"
#include "SyncObject.h"

#define REPOSITORY_HASH_SIZE	101

class Database;
class Repository;
class Sequence;

class RepositoryManager : public Schedule
{
public:
	void close();
	void reportStatistics();
	virtual void execute(Scheduler *scheduler);
	void deleteRepository (Repository *repository);
	Repository* createRepository (const char *name, const char *schema, Sequence *sequence, const char *fileName, int volume, const char *rolloverString);
	Repository* getRepository(const char *schema, const char *name);
	Repository* findRepository (const char *schema, const char *name);
	RepositoryManager(Database *db);
	virtual ~RepositoryManager();

	Database	*database;
	SyncObject	syncObject;
	Repository	*repositories [REPOSITORY_HASH_SIZE];
};

#endif // !defined(AFX_REPOSITORYMANAGER_H__95325AB6_5CE2_4F3F_AE52_00E0923BF209__INCLUDED_)
