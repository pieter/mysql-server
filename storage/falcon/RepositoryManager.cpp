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

// RepositoryManager.cpp: implementation of the RepositoryManager class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "RepositoryManager.h"
#include "SequenceManager.h"
#include "Repository.h"
#include "Database.h"
#include "Connection.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "SQLError.h"
#include "Sync.h"

#define HASH(address,size)				(int)(((UIPTR) address >> 2) % size)

static const char *schedule = "7 1,6,11,16,21,26,31,36,41,46,51,56";

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RepositoryManager::RepositoryManager(Database *db) : Schedule (schedule)
{
	database = db;
	memset (repositories, 0, sizeof (repositories));
}

RepositoryManager::~RepositoryManager()
{
	for (int n = 0; n < REPOSITORY_HASH_SIZE; ++n)
		for (Repository *repository; (repository = repositories [n]);)
			{
			repositories [n] = repository->collision;
			delete repository;
			}
}

Repository* RepositoryManager::findRepository(const char *schema, const char *name)
{
	Repository *repository;
	int slot = HASH (schema, REPOSITORY_HASH_SIZE);

	for (repository = repositories [slot]; repository; repository = repository->collision)
		if (repository->name == name)
			return repository;

	PreparedStatement *statement = database->prepareStatement (
		"select sequenceName,filename,rollovers,currentVolume from system.repositories where schema=? and repositoryName=?");
	statement->setString (1, schema);
	statement->setString (2, name);
	ResultSet *resultSet = statement->executeQuery();

	if (resultSet->next())
		try
			{
			int i = 1;
			const char *sequenceName = resultSet->getSymbol (i++);
			const char *fileName = resultSet->getString (i++);
			const char *rollovers = resultSet->getString (i++);
			int volume = resultSet->getInt (i++);
			Sequence *sequence = database->sequenceManager->getSequence (schema, sequenceName);
			repository = new Repository (name, schema, database, sequence, fileName, rollovers, volume);
			repository->collision = repositories [slot];
			repositories [slot] = repository;
			}
		catch (...)
			{
			resultSet->close();
			statement->close();
			throw;
			}

	resultSet->close();
	statement->close();

	return repository;
}

Repository* RepositoryManager::getRepository(const char *schema, const char *name)
{
	Sync sync (&syncObject, "RepositoryManager::getRepository");
	sync.lock (Shared);
	Repository *repository = findRepository (schema, database->getSymbol (name));

	if (!repository)
		throw SQLEXCEPTION (DDL_ERROR, "can't find repository %s.%s", schema, name);

	return repository;
}

Repository* RepositoryManager::createRepository(const char *name, const char *schema, Sequence *sequence, const char *fileName, int volume, const char *rolloverString)
{
	Sync sync (&syncObject, "RepositoryManager::getRepository");
	sync.lock (Exclusive);
	Repository *repository = findRepository (schema, database->getSymbol (name));

	if (repository)
		throw SQLEXCEPTION (DDL_ERROR, "repository %s.%s already exists", schema, name);

	if (!fileName)
		fileName = "%d/repositories/%s-%r-%v.nfr";

	repository = new Repository (name, schema, database, sequence, fileName, rolloverString, (volume) ? volume : 1);
	int slot = HASH (schema, REPOSITORY_HASH_SIZE);
	repository->collision = repositories [slot];
	repositories [slot] = repository;
	repository->save();

	return repository;
}

void RepositoryManager::deleteRepository(Repository *repository)
{
	repository->drop();
	Sync sync (&syncObject, "RepositoryManager::getRepository");
	sync.lock (Exclusive);
	int slot = HASH (repository->schema, REPOSITORY_HASH_SIZE);

	for (Repository **ptr = repositories + slot; *ptr; ptr = &(*ptr)->collision)
		if (*ptr == repository)
			{
			*ptr = repository->collision;
			break;
			}

	//repository->close();
	delete repository;
}

void RepositoryManager::execute(Scheduler *scheduler)
{
	for (int n = 0; n < REPOSITORY_HASH_SIZE; ++n)
		for (Repository *repository = repositories [n]; repository; repository = repository->collision)
			repository->scavenge();

	Schedule::execute (scheduler);
}

void RepositoryManager::reportStatistics()
{
	for (int n = 0; n < REPOSITORY_HASH_SIZE; ++n)
		for (Repository *repository = repositories [n]; repository; repository = repository->collision)
			repository->reportStatistics();
}

void RepositoryManager::close()
{
	for (int n = 0; n < REPOSITORY_HASH_SIZE; ++n)
		for (Repository *repository = repositories [n]; repository; repository = repository->collision)
			repository->close();
}
