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

// RepositoryVolume.h: interface for the RepositoryVolume class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REPOSITORYVOLUME_H__E8CCDC9E_C665_44DE_AADA_3EC20EAB366D__INCLUDED_)
#define AFX_REPOSITORYVOLUME_H__E8CCDC9E_C665_44DE_AADA_3EC20EAB366D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SyncObject.h"

class Repository;
class Dbb;
class Database;
class Transaction;
class BlobReference;
class IndexKey;

class RepositoryVolume  
{
public:
	void setName (const char *name);
	JString getName();
	void deleteBlob (int64 blobId, Transaction *transaction);
	void getBlob (BlobReference *reference);
	void create();
	int64 getRepositorySize();
	void storeBlob (BlobReference *blob, Transaction *transaction);
	RepositoryVolume(Repository *repo, int volume, JString file);
	virtual ~RepositoryVolume();
	void close();

protected:
	int compare (Stream *stream1, Stream *stream2);
	void fetchRecord (int recordNumber, Stream *stream);
	int getRecordNumber (IndexKey *indexKey);
	int getRecordNumber (int64 blobId);
	void makeWritable();
	void open();
	int makeKey (int64 value, IndexKey *indexKey);

public:
	void reportStatistics();
	void storeBlob (int64 blobId, Stream *stream, Transaction *transaction);
	void synchronize (int64 id, Stream *stream, Transaction *transaction);
	int64 reverseKey (UCHAR *key);
	void synchronize(Transaction *transaction);
	void scavenge();
	int					volumeNumber;
	Repository			*repository;
	RepositoryVolume	*collision;
	SyncObject			syncObject;
	JString				fileName;
	Dbb					*dbb;
	Database			*database;
	time_t				lastAccess;
	bool				isOpen;
	bool				isWritable;
};

#endif // !defined(AFX_REPOSITORYVOLUME_H__E8CCDC9E_C665_44DE_AADA_3EC20EAB366D__INCLUDED_)
