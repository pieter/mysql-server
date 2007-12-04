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

// RepositoryVolume.cpp: implementation of the RepositoryVolume class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "RepositoryVolume.h"
#include "Repository.h"
#include "Dbb.h"
#include "Database.h"
#include "Hdr.h"
#include "SQLError.h"
#include "BDB.h"
#include "BlobReference.h"
#include "Transaction.h"
#include "Bitmap.h"
#include "Stream.h"
#include "Sync.h"
#include "Index2RootPage.h"
#include "IndexPage.h"
#include "IndexNode.h"
#include "BDB.h"
#include "Stream.h"
#include "IndexKey.h"
#include "Index.h"
#include "TableSpaceManager.h"
#include "TableSpace.h"

#define TIMEOUT				(60 * 4)
#define VOLUME_INDEX_ID			0
#define VOLUME_INDEX_VERSION	0
#define VOLUME_SECTION_ID		0

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RepositoryVolume::RepositoryVolume(Repository *repo, int volume, JString file)
{
	repository = repo;
	volumeNumber = volume;
	fileName = file;
	database = repository->database;
	JString volumeName;
	volumeName.Format("%s.%s.%d", repository->schema, repository->name, volumeNumber);
	tableSpace = database->tableSpaceManager->findTableSpace(volumeName);
	
	if (!tableSpace)
		tableSpace = database->tableSpaceManager->createTableSpace(volumeName, fileName, 0, true);
	
	dbb = tableSpace->dbb;
	//dbb = new Dbb (database->dbb, -1);
	isOpen = false;
	isWritable = false;
	lastAccess = 0;
	rootPage = 0;
	section = NULL;
}

RepositoryVolume::~RepositoryVolume()
{
	close();
	delete dbb;
}

void RepositoryVolume::storeBlob(BlobReference *blob, Transaction *transaction)
{
	storeBlob (blob->blobId, blob->getStream(), transaction);
}

void RepositoryVolume::storeBlob(int64 blobId, Stream *stream, Transaction *transaction)
{
	Sync sync (&syncObject, "RepositoryVolume::getBlob");
	sync.lock (Shared);

	while (!isWritable)
		{
		sync.unlock();
		makeWritable();
		sync.lock(Shared);
		}

	if (!section)
		section = dbb->findSection(VOLUME_SECTION_ID);

	lastAccess = database->timestamp;
	IndexKey indexKey;
	//int keyLength = 
	makeKey(blobId, &indexKey);
	int recordNumber = getRecordNumber(&indexKey);

	if (recordNumber == 0)
		{
		//throw SQLError (DELETED_BLOB, "blobId previously deleted from repository file \"%s\"\n", (const char*) fileName);
		dbb->deleteIndexEntry (VOLUME_INDEX_ID, VOLUME_INDEX_VERSION, &indexKey, 0, transaction->transactionId);
		}

	if (recordNumber > 0)
		{
		Stream stream2;
		fetchRecord (recordNumber - 1, &stream2);

		if (compare (stream, &stream2) == 0)
			return;

		throw SQLError (INCONSISTENT_BLOB, "inconsistent values for repository blobId\n");
		}

	int recordId = dbb->insertStub (section, transaction);
	//dbb->logRecord (VOLUME_SECTION_ID, recordId, stream, transaction);
	dbb->updateRecord(section, recordId, stream, transaction, true);
	dbb->addIndexEntry (VOLUME_INDEX_ID, VOLUME_INDEX_VERSION, &indexKey, recordId + 1, transaction->transactionId);
}

void RepositoryVolume::open()
{
	if (isOpen)
		return;

	Sync sync (&syncObject, "RepositoryVolume::open");
	sync.lock (Exclusive);

	if (isOpen)
		return;

	Hdr	header;
	JString name;

	try
		{
		dbb->openFile (fileName, true);
		}
	catch (SQLException&)
		{
		if (dbb->doesFileExist(fileName))
			throw;

		create();
		return;
		}

	try
		{
		dbb->readHeader (&header);
		if (header.fileType != HdrRepositoryFile)
			throw SQLError (RUNTIME_ERROR, "repository file \"%s\" has wrong page type (expeced %d, got %d)\n", 
							(const char*) fileName, HdrRepositoryFile, header.fileType);

		if (header.pageSize != dbb->pageSize)
			throw SQLError (RUNTIME_ERROR, "repository file \"%s\" has wrong page size (expeced %d, got %d)\n", 
							(const char*) fileName, dbb->pageSize, header.pageSize);

		if (volumeNumber == 0)
			volumeNumber = header.volumeNumber;
		else if (header.volumeNumber != volumeNumber)
			throw SQLError (RUNTIME_ERROR, "repository file \"%s\" has wrong volume number (expected %d, got %d)\n", 
							(const char*) fileName, volumeNumber, header.volumeNumber);

		dbb->initRepository (&header);
		isOpen = true;
		isWritable = false;
		name = getName();

		if (name != "" && name != repository->name)
			throw SQLError (RUNTIME_ERROR, "repository file \"%s\" has wrong repository name (expected %s, got %s)\n", 
							(const char*) fileName, (const char*) repository->name, (const char*) name);
		}
	catch (...)
		{
		isOpen = false;
		isWritable = false;
		JString name = getName();
		dbb->closeFile();
		throw;
		}

	dbb->initRepository (&header);
	isOpen = true;
	isWritable = false;
	sync.unlock();

	if (name == "")
		setName (repository->name);
}

void RepositoryVolume::makeWritable()
{
	if (!isOpen)
		open();

	if (isWritable)
		return;

	Sync sync(&syncObject, "RepositoryVolume::getBlob");
	sync.lock(Exclusive);

	if (isWritable)
		return;

	isOpen = false;
	dbb->closeFile();
	dbb->openFile(fileName, false);
	isOpen = true;
	isWritable = true;
}

void RepositoryVolume::create()
{
	IO::createPath (fileName);
	dbb->create(fileName, dbb->pageSize, 0, HdrRepositoryFile, 0, NULL, 0);
	Sync syncSystem(&database->syncSysConnection, "RepositoryVolume::create");
	Transaction *transaction = database->getSystemTransaction();
	syncSystem.lock(Exclusive);
	int indexId = dbb->createIndex(transaction->transactionId, VOLUME_INDEX_VERSION);
	int sectionId = dbb->createSection(transaction->transactionId);
	syncSystem.unlock();
	database->commitSystemTransaction();
	
	Bdb *bdb = dbb->fetchPage(HEADER_PAGE, PAGE_header, Exclusive);
	BDB_HISTORY(bdb);
	bdb->mark (0);
	Hdr *header = (Hdr*) bdb->buffer;
	header->volumeNumber = volumeNumber;
	header->putHeaderVariable(dbb, hdrRepositoryName, (int) strlen (repository->name), repository->name);
	bdb->release(REL_HISTORY);
	dbb->flush();
	isWritable = true;
	isOpen = true;
}

void RepositoryVolume::close()
{
	Sync sync(&syncObject, "RepositoryVolume::close");
	sync.lock(Exclusive);

	if (isOpen)
		{
		dbb->close();
		isOpen = false;
		isWritable = false;
		}
}

int RepositoryVolume::makeKey(int64 value, IndexKey *indexKey)
{
	union {
		double	dbl;
		int64	quad;
		UCHAR	chars [8];
		} stuff;

	stuff.quad = value ^ QUAD_CONSTANT(0x8000000000000000);
	UCHAR *key = indexKey->key;
	UCHAR *p = key;

	for (UCHAR *q = stuff.chars + 8; q > stuff.chars; )
		*p++ = *--q;

	while (p > key && p [-1] == 0)
		--p;

	return indexKey->keyLength = (int) (p - key);
}


int64 RepositoryVolume::reverseKey(UCHAR *key)
{
	union {
		double	dbl;
		int64	quad;
		UCHAR	chars [8];
		} stuff;

	for (UCHAR *q =stuff.chars + 8, *p = key; q > stuff.chars;)
		*--q = *p++;

	stuff.quad ^= QUAD_CONSTANT (0x8000000000000000);

	return stuff.quad;
}

void RepositoryVolume::getBlob(BlobReference *blob)
{
	if (!isOpen)
		open();

	Sync sync (&syncObject, "RepositoryVolume::getBlob");
	sync.lock (Shared);
	lastAccess = database->timestamp;

	int recordNumber = getRecordNumber (blob->blobId);

	if (recordNumber < 0)
		throw SQLError (LOST_BLOB, "blob not found in repository file \"%s\"\n", (const char*) fileName);

	if (recordNumber == 0)
		throw SQLError (DELETED_BLOB, "blob previously deleted from repository file \"%s\"\n", (const char*) fileName);

	fetchRecord (recordNumber - 1, blob->getStream());
}

int RepositoryVolume::getRecordNumber(int64 blobId)
{
	IndexKey indexKey;
	makeKey (blobId, &indexKey);

	return getRecordNumber (&indexKey);
}

int RepositoryVolume::getRecordNumber(IndexKey *indexKey)
{
	if (!rootPage)
		rootPage = Index2RootPage::getIndexRoot(dbb, VOLUME_INDEX_ID);

	Bitmap bitmap;
	//dbb->scanIndex (VOLUME_INDEX_ID, VOLUME_INDEX_VERSION, indexKey, indexKey, false, bitmap);
	Index2RootPage::scanIndex(dbb, VOLUME_INDEX_ID, rootPage, indexKey, indexKey, false, NO_TRANSACTION, &bitmap);
	int recordNumber = bitmap.nextSet (0);

	return recordNumber;
}

void RepositoryVolume::fetchRecord(int recordNumber, Stream *stream)
{
	if (!section)
		section = dbb->findSection(VOLUME_SECTION_ID);
		
	dbb->fetchRecord (section, recordNumber, stream);
}

int RepositoryVolume::compare(Stream *stream1, Stream *stream2)
{
	for (int offset = 0;;)
		{
		int length1 = stream1->getSegmentLength(offset);
		int length2 = stream2->getSegmentLength(offset);

		if (length1 == 0)
			if (length2)
				return -1;
			else
				return 0;

		if (length2 == 0)
			return 1;	
						
		int length = MIN (length1, length2);
		const char *p1 = (const char*) stream1->getSegment (offset);
		const char *p2 = (const char*) stream2->getSegment (offset);

		for (const char *end = p1 + length; p1 < end;)
			{
			int n = *p1++ - *p2++;
			if (n)
				return n;
			}
		offset += length;
		}
}

int64 RepositoryVolume::getRepositorySize()
{
	return (int64) dbb->highPage * dbb->pageSize;
}

void RepositoryVolume::deleteBlob(int64 blobId, Transaction *transaction)
{
	Sync sync (&syncObject, "RepositoryVolume::getBlob");
	sync.lock (Shared);

	while (!isWritable)
		{
		sync.unlock();
		makeWritable();
		sync.lock(Shared);
		}

	lastAccess = database->timestamp;
	int recordNumber = getRecordNumber (blobId);

	if (recordNumber < 0)
		throw SQLError (LOST_BLOB, "blob not found in repository file \"%s\"\n", (const char*) fileName);

	if (recordNumber == 0)
		throw SQLError (DELETED_BLOB, "blobId already deleted repository file \"%s\"\n", (const char*) fileName);

	IndexKey indexKey;
	makeKey (blobId, &indexKey);
	dbb->addIndexEntry (VOLUME_INDEX_ID, VOLUME_INDEX_VERSION, &indexKey, 0, transaction->transactionId);
	dbb->deleteIndexEntry (VOLUME_INDEX_ID, VOLUME_INDEX_VERSION, &indexKey, recordNumber + 1, transaction->transactionId);
}

JString RepositoryVolume::getName()
{
	Bdb *bdb = dbb->fetchPage (HEADER_PAGE, PAGE_header, Shared);
	BDB_HISTORY(bdb);
	Hdr *header = (Hdr*) bdb->buffer;
	char name [128];
	
	if (header->getHeaderVariable (dbb, hdrRepositoryName, sizeof (name), name) < 0)
		name [0] = 0;

	bdb->release(REL_HISTORY);

	return name;
}

void RepositoryVolume::setName(const char *name)
{
	Sync sync (&syncObject, "RepositoryVolume::setName");
	sync.lock (Shared);

	while (!isWritable)
		{
		sync.unlock();
		makeWritable();
		sync.lock(Shared);
		}

	Bdb *bdb = dbb->fetchPage(HEADER_PAGE, PAGE_header, Exclusive);
	BDB_HISTORY(bdb);
	bdb->mark (0);
	Hdr *header = (Hdr*) bdb->buffer;
	header->putHeaderVariable(dbb, hdrRepositoryName, (int) strlen(name), name);
	bdb->release(REL_HISTORY);
}

void RepositoryVolume::scavenge()
{
	if (!isOpen || lastAccess + TIMEOUT > database->timestamp)
		return;

	Sync sync (&syncObject, "RepositoryVolume::getBlob");
	sync.lock (Exclusive);

	if (!isOpen || lastAccess + TIMEOUT > database->timestamp)
		return;

	if (dbb->hasDirtyPages())
		return;

	close();	
}

void RepositoryVolume::synchronize(Transaction *transaction)
{
	open();
	RepositoryVolume *target = repository->getVolume (volumeNumber);
	Bdb *bdb = NULL;
	Stream stream;

	try
		{
		IndexKey indexKey;
		indexKey.keyLength = 0;
		bdb = Index2RootPage::findLeaf (dbb, VOLUME_INDEX_ID, 0, &indexKey, Shared, transaction->transactionId);

		for (;;)
			{
			IndexPage *page = (IndexPage*) bdb->buffer;
			Btn *bucketEnd = (Btn*) ((char*) page + page->length);

			for (IndexNode node (page); node.node < bucketEnd; node.getNext(bucketEnd))
				{
				memcpy (indexKey.key + node.offset, node.key, node.length);
				int32 recordNumber = node.getNumber();
				int64 id = reverseKey (indexKey.key);

				if (recordNumber > 0)
					{
					stream.clear();
					fetchRecord (recordNumber - 1, &stream);
					target->synchronize (id, &stream, transaction);
					}
				else if (recordNumber == 0)
					target->synchronize (id, NULL, transaction);
				else
					break;
				}

			if (!page->nextPage)
				break;

			bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Shared);
			BDB_HISTORY(bdb);
			}
		bdb->release(REL_HISTORY);
		}
	catch (...)
		{
		if (bdb)
			bdb->release(REL_HISTORY);
		throw;
		}
}

void RepositoryVolume::synchronize(int64 id, Stream *stream, Transaction *transaction)
{
	if (!isOpen)
		open();

	Sync sync (&syncObject, "RepositoryVolume::synchronize");
	sync.lock (Shared);
	IndexKey indexKey;
	makeKey (id, &indexKey);
	int recordNumber = getRecordNumber (&indexKey);

	// If the blob has been deleted, make sure it's deleted here, too.

	if (!stream)
		{
		sync.unlock();

		if (recordNumber > 0)
			deleteBlob (id, transaction);

		return;
		}

	// If the local blob has been deleted, that's ok.

	if (recordNumber == 0)
		return;

	// If the local blob does not exist, store it now

	if (recordNumber < 0)
		{
		sync.unlock();
		storeBlob (id, stream, transaction);
		return;
		}

	// Verify blob is identical

	Stream stream2;
	fetchRecord (recordNumber - 1, &stream2);

	if (stream2.compare (stream))
		throw SQLError (INCONSISTENT_BLOB, "inconsistent blob %ld in repository \"%s.%s\" volume %d\n",
						id, repository->schema, repository->name, volumeNumber);
}

void RepositoryVolume::reportStatistics()
{
	dbb->reportStatistics();
}
