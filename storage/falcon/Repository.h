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

// Repository.h: interface for the Repository class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REPOSITORY_H__79C36725_74D4_495A_B1B7_1F3AED7A749A__INCLUDED_)
#define AFX_REPOSITORY_H__79C36725_74D4_495A_B1B7_1F3AED7A749A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SyncObject.h"
#include "Engine.h"	// Added by ClassView

static const int VOLUME_HASH_SIZE =	25;

enum RolloverPeriod {
    RolloverNone = 0,
	RolloverWeekly,
	RolloverMonthly,
	};

class Database;
class Sequence;
class Value;
CLASS(Field);
class Transaction;
class BlobReference;
class RepositoryVolume;

class Repository  
{
public:
	void close();
	void reportStatistics();
	void synchronize (const char *fileName, Transaction *transaction);
	void scavenge();
	void deleteBlob (int volumeNumber, int64 blobId, Transaction *transaction);
	void drop();
	void setRollover (const char *string);
	static RolloverPeriod getRolloverPeriod (const char *token);
	static int64 getFileSize (const char *token);
	static bool getToken (const char **ptr, int sizeToken, char *token);
	static void validateRollovers (const char *string);
	void setVolume (int volume);
	void getBlob (BlobReference *reference);
	void setFilePattern (const char *pattern);
	void setSequence (Sequence *seq);
	JString genFileName (int volume);
	RepositoryVolume* findVolume (int volumeNumber);
	RepositoryVolume* getVolume (int volumeNumber);
	void storeBlob (BlobReference *blob, Transaction *transaction);
	Value* defaultRepository (Field *field, Value *value, Value *alt);
	void save();
	Repository(const char *repositoryName, const char *repositorySchema, Database *db, Sequence *seq, const char *fileName, const char *rollovers, int volume);
	virtual ~Repository();

	const char		*name;
	const char		*schema;
	JString			filePattern;
	JString			rolloverString;
	Repository		*collision;
	Database		*database;
	Sequence		*sequence;
	int				currentVolume;
	int64			maxSize;
	RolloverPeriod	rolloverPeriod;
	SyncObject		syncObject;
	RepositoryVolume	*volumes [VOLUME_HASH_SIZE];
};

#endif // !defined(AFX_REPOSITORY_H__79C36725_74D4_495A_B1B7_1F3AED7A749A__INCLUDED_)
