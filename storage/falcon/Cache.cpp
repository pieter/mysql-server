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

// Cache.cpp: implementation of the Cache class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "Cache.h"
#include "BDB.h"
#include "Dbb.h"
#include "Page.h"
#include "IndexPage.h"
#include "PageInventoryPage.h"
#include "Sync.h"
#include "Log.h"
#include "LogLock.h"
#include "Stream.h"
#include "PagePrecedence.h"
#include "PageWriter.h"
#include "SQLError.h"
#include "Thread.h"
#include "Threads.h"
#include "DatabaseCopy.h"
#include "Database.h"
#include "Bitmap.h"

extern uint falcon_io_threads;

#define FLUSH_INTERWRITE_WAIT			0									// in milliseconds

//#define STOP_PAGE		64

static const uint64 cacheHunkSize		= 1024 * 1024 * 128;
static const int	ASYNC_BUFFER_SIZE	= 1024000;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Cache::Cache(Database *db, int pageSz, int hashSz, int numBuffers)
{
	database = db;
	panicShutdown = false;
	pageSize = pageSz;
	hashSize = hashSz;
	numberBuffers = numBuffers;
	upperFraction = numberBuffers / 4;
	bufferAge = 0;
	firstDirty = NULL;
	lastDirty = NULL;
	numberDirtyPages = 0;
	pageWriter = NULL;
	freePrecedence = NULL;
	hashTable = new Bdb* [hashSz];
	memset (hashTable, 0, sizeof (Bdb*) * hashSize);
	uint64 n = ((uint64) pageSize * numberBuffers + cacheHunkSize - 1) / cacheHunkSize;
	numberHunks = (int) n;
	bufferHunks = new char* [numberHunks];
	memset(bufferHunks, 0, numberHunks * sizeof(char*));
	syncObject.setName("Cache::syncObject");
	syncDirty.setName("Cache::syncDirty");
	syncFlush.setName("Cache::syncFlush");
	flushBitmap = new Bitmap;
	numberIoThreads = falcon_io_threads;
	ioThreads = new Thread*[numberIoThreads];
	memset(ioThreads, 0, numberIoThreads * sizeof(ioThreads[0]));
	flushing = false;
	
	try
		{	
		bdbs = new Bdb [numberBuffers];
		endBdbs = bdbs + numberBuffers;
		int remaining = 0;
		int hunk = 0;
		int allocated = 0;
		char *stuff = NULL;
		
		for (Bdb *bdb = bdbs; bdb < endBdbs; ++bdb, --remaining)
			{
			if (remaining == 0)
				{
				remaining = MIN(numberBuffers - allocated, (int) (cacheHunkSize / pageSize));
				stuff = bufferHunks[hunk++] = new char [pageSize * (remaining + 1)];
				stuff = (char*) (((UIPTR) stuff + pageSize - 1) / pageSize * pageSize);
				allocated += remaining;
				}
				
			bdb->cache = this;
			bufferQueue.append(bdb);
			bdb->buffer = (Page*) stuff;
			stuff += pageSize;
			}
		}
	catch(...)
		{
		delete [] bdbs;
		
		for (int n = 0; n < numberHunks; ++n)
			delete [] bufferHunks[n];
		
		delete [] bufferHunks;
		
		throw;
		}
	
	validateCache();

	for (int n = 0; n < numberIoThreads; ++n)
		ioThreads[n] = database->threads->start("Cache::Cache", &Cache::ioThread, this);
}

Cache::~Cache()
{
	delete [] hashTable;
	delete [] bdbs;
	delete [] ioThreads;
	delete flushBitmap;
	
	if (bufferHunks)
		{
		for (int n = 0; n < numberHunks; ++n)
			delete [] bufferHunks[n];
		
		delete[] bufferHunks;
		}
		
	for (PagePrecedence *precedence; (precedence = freePrecedence);)
		{
		freePrecedence = precedence->nextHigh;
		delete precedence;
		}
}

Bdb* Cache::probePage(Dbb *dbb, int32 pageNumber)
{
	ASSERT (pageNumber >= 0);
	Sync sync (&syncObject, "Cache::probePage");
	sync.lock (Shared);
	Bdb *bdb = findBdb(dbb, pageNumber);
	
	if (bdb)
		{
		bdb->incrementUseCount(ADD_HISTORY);
		sync.unlock();

		if (bdb->buffer->pageType == PAGE_free)
			{
			bdb->decrementUseCount(REL_HISTORY);
			
			return NULL;
			}

		bdb->addRef(Shared  COMMA_ADD_HISTORY);
		bdb->decrementUseCount(REL_HISTORY);

		return bdb;
		}

	return NULL;
}

Bdb* Cache::findBdb(Dbb* dbb, int32 pageNumber)
{
	for (Bdb *bdb = hashTable [pageNumber % hashSize]; bdb; bdb = bdb->hash)
		if (bdb->pageNumber == pageNumber && bdb->dbb == dbb)
			return bdb;

	return NULL;
}

Bdb* Cache::fetchPage(Dbb *dbb, int32 pageNumber, PageType pageType, LockType lockType)
{
	if (panicShutdown)
		{
		Thread *thread = Thread::getThread("Cache::fetchPage");
		
		if (thread->pageMarks == 0)
			throw SQLError(RUNTIME_ERROR, "Emergency shut is underway");
		}

#ifdef STOP_PAGE			
		if (pageNumber == STOP_PAGE)
 			Log::debug("fetching page %d/%d\n", pageNumber, dbb->tableSpaceId);
#endif

	ASSERT (pageNumber >= 0);
	int slot = pageNumber % hashSize;
	LockType actual = lockType;
	Sync sync (&syncObject, "Cache::fetchPage");
	sync.lock (Shared);
	int hit = 0;

	/* If we already have a buffer for this go, we're done */

	Bdb *bdb;

	for (bdb = hashTable [slot]; bdb; bdb = bdb->hash)
		if (bdb->pageNumber == pageNumber && bdb->dbb == dbb)
			{
			//syncObject.validateShared("Cache::fetchPage");
			bdb->incrementUseCount(ADD_HISTORY);
			sync.unlock();
			bdb->addRef(lockType  COMMA_ADD_HISTORY);
			bdb->decrementUseCount(REL_HISTORY);
			hit = 1;
			break;
			}

	if (!bdb)
		{
		sync.unlock();
		actual = Exclusive;
		sync.lock(Exclusive, "Cache::fetchPage 2");

		for (bdb = hashTable [slot]; bdb; bdb = bdb->hash)
			if (bdb->pageNumber == pageNumber && bdb->dbb == dbb)
				{
				//syncObject.validateExclusive("Cache::fetchPage (retry)");
				bdb->incrementUseCount(ADD_HISTORY);
				sync.unlock();
				bdb->addRef(lockType  COMMA_ADD_HISTORY);
				bdb->decrementUseCount(REL_HISTORY);
				hit = 2;
				break;
				}

		if (!bdb)
			{
			bdb = findBuffer(dbb, pageNumber, actual);
			moveToHead(bdb);
			sync.unlock();

#ifdef STOP_PAGE			
			if (bdb->pageNumber == STOP_PAGE)
				Log::debug("reading page %d/%d\n", bdb->pageNumber, dbb->tableSpaceId);
#endif
				
			dbb->readPage(bdb);
			
			if (actual != lockType)
				bdb->downGrade(lockType);
			}
		}

	Page *page = bdb->buffer;

	if (pageType && page->pageType != pageType)
		{
		/*** future code
		bdb->release();
		throw SQLError (DATABASE_CORRUPTION, "page %d wrong page type, expected %d got %d\n",
						pageNumber, pageType, page->pageType);
		***/
		FATAL ("page %d wrong page type, expected %d got %d\n",
				 bdb->pageNumber, pageType, page->pageType);
		}

	// If buffer has moved out of the upper "fraction" of the LRU queue, move it back up
	
	if (bdb->age < bufferAge - upperFraction)
		{
		sync.lock (Exclusive, "Cache::fetchPage 3");
		moveToHead (bdb);
		}
		
	ASSERT (bdb->pageNumber == pageNumber);
	ASSERT (bdb->dbb == dbb);
	ASSERT (bdb->useCount > 0);

	return bdb;
}

Bdb* Cache::fakePage(Dbb *dbb, int32 pageNumber, PageType type, TransId transId)
{
	Sync sync(&syncObject, "Cache::fakePage");
	sync.lock(Exclusive);
	int	slot = pageNumber % hashSize;

#ifdef STOP_PAGE			
	if (pageNumber == STOP_PAGE)
		Log::debug("faking page %d/%d\n",pageNumber, dbb->tableSpaceId);
#endif

	/* If we already have a buffer for this, we're done */

	Bdb *bdb;

	for (bdb = hashTable [slot]; bdb; bdb = bdb->hash)
		if (bdb->pageNumber == pageNumber && bdb->dbb == dbb)
			{
			if (bdb->syncObject.isLocked())
				{
				// The pageWriter may still be cleaning up this freed page with a shared lock
				ASSERT(bdb->buffer->pageType == PAGE_free);
				ASSERT(bdb->syncObject.getState() >= 0);
				}
				
			bdb->addRef(Exclusive  COMMA_ADD_HISTORY);
			
			break;
			}

	if (!bdb)
		bdb = findBuffer(dbb, pageNumber, Exclusive);

	bdb->mark(transId);
	bdb->flags |= BDB_new;
	memset(bdb->buffer, 0, pageSize);
	bdb->buffer->pageType = type;
	moveToHead(bdb);

	return bdb;
}

void Cache::flush(int64 arg)
{
	Sync flushLock(&syncFlush, "Cache::ioThread");
	Sync sync(&syncDirty, "Cache::ioThread");
	flushLock.lock(Exclusive);
	sync.lock(Shared);
	flushArg = arg;
	flushPages = 0;
	physicalWrites = 0;
	
	for (Bdb *bdb = firstDirty; bdb; bdb = bdb->nextDirty)
		{
		bdb->flushIt = true;
		flushBitmap->set(bdb->pageNumber);
		++flushPages;
		}

	/***
	int start = 0;
	int last = 0;
	int runs = 0;
	int runPages = 0;
	
	for (int n = 0; (n = flushBitmap->nextSet(n)) >= 0; ++n)
		if (n == last + 1)
			++last;
		else
			{
			if (last - start > 1)
				{
				//Log::debug(" Flush run of %d pages starting at %d\n", last - start, start);
				++runs;
				runPages += last - start;
				}
				
			last = start = n;
			}
	
	if (runs > 0)
		Log::debug("Flush: %d runs of %d pages out of %d\n", runs, runPages, flushPages);			
	***/
			
	flushStart = database->timestamp;
	flushing = true;
	sync.unlock();
	flushLock.unlock();
	
	for (int n = 0; n < numberIoThreads; ++n)
		if (ioThreads[n])
			ioThreads[n]->wake();
	
	/***
	Sync sync(&syncDirty, "Cache::flush");
	sync.lock(Exclusive);
	Bdb *bdb;
	Thread *thread = Thread::getThread("Cache::flush");
	
#if TRACE_PAGE	
	Log::debug("Starting page cace flush\n");
#endif

	for (bdb = firstDirty; bdb; bdb = bdb->nextDirty)
		bdb->flushIt = true;
		
	while ((bdb = firstDirty))
		{
		while (bdb && !bdb->flushIt)
			bdb = bdb->nextDirty;

		if (!bdb)
			break;
		
		while (bdb->higher)
			bdb = bdb->higher->higher;

		if (!(bdb->flags & BDB_dirty))
			{
			markClean(bdb);
			continue;
			}
		
		bdb->incrementUseCount(ADD_HISTORY);
		sync.unlock();
		bdb->addRef(Shared  COMMA_ADD_HISTORY);
		bdb->decrementUseCount(REL_HISTORY);
		writePage(bdb, WRITE_TYPE_FLUSH);
		Dbb *dbb = bdb->dbb;
		bdb->release(REL_HISTORY);
		
		if (FLUSH_FSYNC_THRESHOLD && dbb->writesSinceSync > FLUSH_FSYNC_THRESHOLD)
			syncFile(dbb, "flush");

		if (FLUSH_INTERWRITE_WAIT)
			thread->sleep(FLUSH_INTERWRITE_WAIT);
		
		sync.lock(Exclusive);
		}
		
#if TRACE_PAGE	
	Log::debug("Ending page cace flush\n");
#endif
	***/
}

void Cache::moveToHead(Bdb * bdb)
{
	bdb->age = bufferAge++;
	bufferQueue.remove(bdb);
	bufferQueue.prepend(bdb);
	//validateUnique (bdb);
}

Bdb* Cache::findBuffer(Dbb *dbb, int pageNumber, LockType lockType)
{
	//syncObject.validateExclusive("Cache::findBuffer");
	int	slot = pageNumber % hashSize;
	Sync sync(&syncDirty, "Cache::findBuffer");
	
	/* Find least recently used, not-in-use buffer */

	Bdb *bdb;

	// Find a candidate BDB.  If there are higher precedence pages, they must be written first
	
	for (;;)
		{
		for (bdb = bufferQueue.last; bdb; bdb = bdb->prior)
			{
			if (bdb->higher)
				{
				sync.lock(Shared);
				
				if (bdb->higher)
					{
					Bdb *candidate;
					
					for (candidate = bdb; candidate->higher; candidate = candidate->higher->higher)
						;
										
					if (candidate->useCount == 0)
						{
						bdb = candidate;
						sync.unlock();
						
						break;
						}
					}
				
				sync.unlock();
				}
			else if (bdb->useCount == 0)
				break;
			}

		if (!bdb)
			throw SQLError(RUNTIME_ERROR, "buffer pool is exhausted\n");
			/*** the following is for debugging, if necessary
			{
			for (bdb = bufferQueue.last; bdb; bdb = bdb->prior)
				{
				if (bdb->higher)
					{
					sync.lock(Shared);
					
					if (bdb->higher)
						{
						Bdb *candidate;
						
						for (candidate = bdb; candidate->higher; candidate = candidate->higher->higher)
							;
											
						if (candidate->useCount == 0)
							{
							bdb = candidate;
							sync.unlock();
							
							break;
							}
						
						sync.unlock();
						}
					}
				else if (bdb->useCount == 0)
					break;
				}
				
			throw SQLError(RUNTIME_ERROR, "buffer pool is exhausted\n");
			}
			***/
			
		if (!(bdb->flags & BDB_dirty))
			break;
			
		writePage (bdb, WRITE_TYPE_REUSE);
		}

	ASSERT(bdb->higher == NULL);

	for (PagePrecedence *precedence; (precedence = bdb->lower);)
		clearPrecedence (bdb->lower);

	/* Unlink its old incarnation from the page/hash table */

	if (bdb->pageNumber >= 0)
		for (Bdb **ptr = hashTable + bdb->pageNumber % hashSize;; ptr = &(*ptr)->hash)
			if (*ptr == bdb)
				{
				*ptr = bdb->hash;
				break;
				}
			else
				ASSERT (*ptr);

	bdb->addRef (lockType  COMMA_ADD_HISTORY);

	/* Set new page number and relink into hash table */

	bdb->hash = hashTable [slot];
	hashTable [slot] = bdb;
	bdb->pageNumber = pageNumber;
	bdb->dbb = dbb;

#ifdef COLLECT_BDB_HISTORY
	bdb->initHistory();
#endif

	return bdb;
}

void Cache::validate()
{
	for (Bdb *bdb = bufferQueue.last; bdb; bdb = bdb->prior)
		{
		//IndexPage *page = (IndexPage*) bdb->buffer;
		ASSERT (bdb->useCount == 0);
		}
}

void Cache::markDirty(Bdb *bdb)
{
	Sync sync (&syncDirty, "Cache::markDirty");
	sync.lock (Exclusive);
	bdb->nextDirty = NULL;
	bdb->priorDirty = lastDirty;

	if (lastDirty)
		lastDirty->nextDirty = bdb;
	else
		firstDirty = bdb;

	lastDirty = bdb;
	++numberDirtyPages;
	//validateUnique (bdb);
}

void Cache::markClean(Bdb *bdb)
{
	Sync sync (&syncDirty, "Cache::markClean");
	sync.lock (Exclusive);
	bdb->flushIt = false;
	--numberDirtyPages;
	
	if (bdb == lastDirty)
		lastDirty = bdb->priorDirty;

	if (bdb->priorDirty)
		bdb->priorDirty->nextDirty = bdb->nextDirty;

	if (bdb->nextDirty)
		bdb->nextDirty->priorDirty = bdb->priorDirty;

	if (bdb == firstDirty)
		firstDirty = bdb->nextDirty;

	bdb->nextDirty = NULL;
	bdb->priorDirty = NULL;
	PagePrecedence *precedence;

	while ( (precedence = bdb->lower) )
		clearPrecedence (bdb->lower);

	while ( (precedence = bdb->higher) )
		clearPrecedence (bdb->higher);
}

void Cache::writePage(Bdb *bdb, int type)
{
	Sync writer(&bdb->syncWrite, "Cache::writePage");
	writer.lock(Exclusive);
	
	if (!(bdb->flags & BDB_dirty))
		{
		//Log::debug("Cache::writePage: page %d not dirty\n", bdb->pageNumber);
		markClean (bdb);
		
		return;
		}
		
	ASSERT(!(bdb->flags & BDB_write_pending));
	Dbb *dbb = bdb->dbb;
	ASSERT(database);
	markClean (bdb);
	time_t start = database->timestamp;
	dbb->writePage(bdb, type);
	time_t delta = database->timestamp - start;
	
	/***
	if (delta > 1)
		Log::debug("Page %d took %d seconds to write\n", bdb->pageNumber, delta);
	***/
	
#ifdef STOP_PAGE			
	if (bdb->pageNumber == STOP_PAGE)
		Log::debug("writing page %d/%d\n", bdb->pageNumber, dbb->tableSpaceId);
#endif
		
	bdb->flags &= ~(BDB_dirty | BDB_new);

	if (pageWriter && (bdb->flags & BDB_writer))
		{
		bdb->flags &= ~(BDB_writer | BDB_register);
		pageWriter->pageWritten(bdb->dbb, bdb->pageNumber);
		}

	if (dbb->shadows)
		{
		Sync sync (&dbb->cloneSyncObject, "Cache::writePage");
		sync.lock (Shared);

		for (DatabaseCopy *shadow = dbb->shadows; shadow; shadow = shadow->next)
			shadow->rewritePage(bdb);
		}
}

void Cache::analyze(Stream *stream)
{
	Sync sync (&syncDirty, "Cache::analyze");
	sync.lock (Shared);
	int inUse = 0;
	int dirty = 0;
	int dirtyList = 0;
	int total = 0;
	int maxChain = 0;
	int totalChain = 0;
	int freeCount = 0;
	Bdb *bdb;

	for (bdb = bdbs; bdb < endBdbs; ++bdb)
		{
		++total;
		
		if (bdb->flags & BDB_dirty)
			++dirty;
			
		if (bdb->useCount)
			++inUse;
		
		int chain = 0;
		
		for (PagePrecedence *precedence = bdb->higher; precedence; precedence = precedence->nextHigh)
			++chain;
		
		totalChain += chain;
		maxChain = MAX(chain, maxChain);
		}

	for (bdb = firstDirty; bdb; bdb = bdb->nextDirty)
		++dirtyList;

	for (PagePrecedence *precedence = freePrecedence; precedence; precedence = precedence->nextHigh)
		++freeCount;
		
	stream->format ("Cache: %d pages, %d in use, %d dirty, %d in dirty chain\nMax chain %d, total chain %d, free %d\n",
					total, inUse, dirty, dirtyList, maxChain, totalChain, freeCount);
}

void Cache::validateUnique(Bdb *target)
{
	int	slot = target->pageNumber % hashSize;

	for (Bdb *bdb = hashTable [slot]; bdb; bdb = bdb->hash)
		ASSERT (bdb == target || !(bdb->pageNumber == target->pageNumber && bdb->dbb == target->dbb));
}

void Cache::setPrecedence(Bdb *lower, int32 highPageNumber)
{
	Sync sync (&syncDirty, "Cache::setPrecedence");
	sync.lock (Shared);
	int	slot = highPageNumber % hashSize;
	Bdb *higher;
	PagePrecedence *precedence;
	int count = 0;
	
	for (precedence = lower->higher; precedence; precedence = precedence->nextHigh, ++count)
		if (precedence->higher->pageNumber == highPageNumber)
			return;
	
	/***
	if (count == 100)
		{
		LogLock logLock;
		Log::debug("Long precedence chain:\n");
		dbb->printPage(lower);
		
		for (precedence = lower->higher; precedence; precedence = precedence->nextHigh, ++count)
			dbb->printPage(precedence->higher->pageNumber);
		}
	***/

	for (higher = hashTable [slot]; higher; higher = higher->hash)
		if (higher->pageNumber == highPageNumber && higher->dbb == lower->dbb)
			break;

	if (!higher)
		return;

	sync.unlock();
	sync.lock(Exclusive);
	
	for (higher = hashTable [slot]; higher; higher = higher->hash)
		if (higher->pageNumber == highPageNumber && higher->dbb == lower->dbb)
			break;

	if (!higher || !(higher->flags & BDB_dirty))
		return;
	
	// Make sure we're not creating a cycle.  If so, write some pages now!

	while (higher->isHigher(lower))
		{
		Bdb *bdb = lower;
		
		while (bdb->higher)
			bdb = bdb->higher->higher;
			
		writePage (bdb, WRITE_TYPE_PRECEDENCE);
		}

	if ( (precedence = freePrecedence) )
		{
		freePrecedence = precedence->nextHigh;
		precedence->setPrecedence(lower, higher);
		}
	else
		new PagePrecedence(lower, higher);
}

void Cache::clearPrecedence(PagePrecedence *precedence)
{
	Sync sync (&syncDirty, "Cache::setPrecedence");
	sync.lock (Exclusive);
	PagePrecedence **ptr;
	Bdb *bdb = precedence->higher;
	int hits = 0;

	for (ptr = &bdb->lower; *ptr; ptr = &(*ptr)->nextLow)
		if (*ptr == precedence)
			{
			++hits;
			*ptr = precedence->nextLow;
			break;
			}

	ASSERT (hits == 1);
	bdb = precedence->lower;

	for (ptr = &bdb->higher; *ptr; ptr = &(*ptr)->nextHigh)
		if (*ptr == precedence)
			{
			++hits;
			*ptr = precedence->nextHigh;
			break;
			}

	ASSERT (hits == 2);
	//delete precedence;
	precedence->nextHigh = freePrecedence;
	freePrecedence = precedence;
}

void Cache::freePage(Dbb *dbb, int32 pageNumber)
{
	Sync sync (&syncObject, "Cache::freePage");
	sync.lock (Shared);
	int	slot = pageNumber % hashSize;

	// If page exists in cache (usual case), clean it up

	for (Bdb *bdb = hashTable [slot]; bdb; bdb = bdb->hash)
		if (bdb->pageNumber == pageNumber && bdb->dbb == dbb)
			{
			if (bdb->flags & BDB_dirty)
				{
				sync.unlock();
				markClean (bdb);
				}
				
			bdb->flags &= ~(BDB_dirty | BDB_new);
			break;
			}
}

void Cache::flush(Dbb *dbb)
{
	Sync sync (&syncDirty, "Cache::flush(Dbb)");
	sync.lock (Exclusive);

	for (Bdb *bdb = bdbs; bdb < endBdbs; ++bdb)
		if (bdb->dbb == dbb)
			{
			if (bdb->flags & (BDB_dirty | BDB_new))
				writePage(bdb, WRITE_TYPE_FLUSH);

			bdb->dbb = NULL;
			}
}

bool Cache::hasDirtyPages(Dbb *dbb)
{
	Sync sync (&syncDirty, "Cache::hasDirtyPages");
	sync.lock (Shared);

	for (Bdb *bdb = firstDirty; bdb; bdb = bdb->nextDirty)
		if (bdb->dbb == dbb)
			return true;

	return false;
}


void Cache::setPageWriter(PageWriter *writer)
{
	pageWriter = writer;
}

void Cache::shutdownNow(void)
{
	panicShutdown = true;
	Sync sync (&syncDirty, "Cache::shutdownNow");
	sync.lock (Exclusive);

	for (Bdb *bdb = firstDirty; bdb; bdb = bdb->nextDirty)
		{
		Dbb *database = bdb->dbb;
		database->writePage (bdb, WRITE_TYPE_SHUTDOWN);
		}


}

void Cache::validateCache(void)
{
	//MemMgrValidate(bufferSpace);
}

Bdb* Cache::trialFetch(Dbb* dbb, int32 pageNumber, LockType lockType)
{
	if (panicShutdown)
		{
		Thread *thread = Thread::getThread("Cache::trialFetch");
		
		if (thread->pageMarks == 0)
			throw SQLError(RUNTIME_ERROR, "Emergency shut is underway");
		}

	ASSERT (pageNumber >= 0);
	int	slot = pageNumber % hashSize;
	Sync sync (&syncObject, "Cache::trialFetch");
	sync.lock (Shared);
	int hit = 0;

	/* If we already have a buffer for this go, we're done */

	Bdb *bdb;

	for (bdb = hashTable [slot]; bdb; bdb = bdb->hash)
		if (bdb->pageNumber == pageNumber && bdb->dbb == dbb)
			{
			//syncObject.validateShared("Cache::trialFetch");
			bdb->incrementUseCount(ADD_HISTORY);
			sync.unlock();
			bdb->addRef(lockType  COMMA_ADD_HISTORY);
			bdb->decrementUseCount(REL_HISTORY);
			hit = 1;
			break;
			}

	return bdb;
}

void Cache::syncFile(Dbb *dbb, const char *text)
{
	const char *fileName = dbb->fileName;
	int writes = dbb->writesSinceSync;
	time_t start = database->timestamp;
	dbb->sync();
	time_t delta = database->timestamp - start;
	
	if (delta > 1)
		Log::log(LogInfo, "%d: %s %s sync: %d page in %d seconds\n", database->deltaTime, fileName, text, writes, delta);
}

void Cache::ioThread(void* arg)
{
	((Cache*) arg)->ioThread();
}

void Cache::ioThread(void)
{
	Sync flushLock(&syncFlush, "Cache::ioThread");
	Sync sync(&syncFlush, "Cache::ioThread");
	Thread *thread = Thread::getThread("Cache::ioThread");
	flushLock.lock(Exclusive);
	UCHAR *rawBuffer = new UCHAR[ASYNC_BUFFER_SIZE];
	UCHAR *buffer = (UCHAR*) (((UIPTR) rawBuffer + pageSize - 1) / pageSize * pageSize);
	UCHAR *end = (UCHAR*) ((UIPTR) (rawBuffer + ASYNC_BUFFER_SIZE) / pageSize * pageSize);
	
	while (!thread->shutdownInProgress)
		{
		int32 pageNumber = flushBitmap->nextSet(0);
		
		if (pageNumber >= 0)
			{
			sync.lock(Shared);
			int	slot = pageNumber % hashSize;
			bool hit = false;
			Bdb *bdbList = NULL;
			UCHAR *p = buffer;
			
			// Look for a page to flush
			
			for (Bdb *bdb = hashTable [slot]; bdb; bdb = bdb->hash)
				if (bdb->pageNumber == pageNumber && bdb->flushIt && (bdb->flags & BDB_dirty))
					{
					hit = true;
					int32 pageNumber = bdb->pageNumber;
					Dbb *dbb = bdb->dbb;
					
					if (!bdb->hash)
						flushBitmap->clear(pageNumber);
					
					while (p < end)
						{
						bdb->syncWrite.lock(NULL, Exclusive);
						bdb->ioThreadNext = bdbList;
						bdbList = bdb;
						bdb->incrementUseCount(ADD_HISTORY);
						bdb->addRef(Shared  COMMA_ADD_HISTORY);
						bdb->flags |= BDB_write_pending;
						memcpy(p, bdb->buffer, pageSize);
						p += pageSize;
						bdb->release(REL_HISTORY);
						markClean(bdb);
						bdb->flags &= ~(BDB_dirty | BDB_new);
						bdb = findBdb(dbb, bdb->pageNumber + 1);
						
						if (!bdb || !bdb->flushIt || !(bdb->flags & BDB_dirty))
							break;
						}
					
					sync.unlock();
					flushLock.unlock();
					int length = p - buffer;
					
					/***
					if (length > pageSize)
						Log::debug("Writing %d pages starting with %d to %s\n", 
									length / pageSize, pageNumber, (const char*) dbb->fileName);
					***/
					
					dbb->writePages(pageNumber, length, buffer, WRITE_TYPE_FLUSH);
					
					while (bdbList)
						{
						bdbList->flags &= ~BDB_write_pending;
						bdbList->decrementUseCount(REL_HISTORY);
						bdbList->syncWrite.unlock();
						bdbList = bdbList->ioThreadNext;
						}
						
					flushLock.lock(Exclusive);
					++physicalWrites;
					break;
					}
			
			if (!hit)
				{
				sync.unlock();
				flushBitmap->clear(pageNumber);
				}
			}
		else 
			{
			if (flushing)
				{
				flushing = false;
				flushLock.unlock();
				int delta = database->timestamp - flushStart;
				
				if (delta > 1)
					Log::log(LogInfo, "%d: Cache flush: %d pages, %d writes in %d seconds (%d pps)\n",
								database->deltaTime, flushPages, physicalWrites, delta, flushPages / delta);

				database->pageCacheFlushed(flushArg);
				}
			else
				flushLock.unlock();
			
			thread->sleep();
			flushLock.lock(Exclusive);
			}
		}
	
	delete [] rawBuffer;			
}

void Cache::shutdown(void)
{
	for (int n = 0; n < numberIoThreads; ++n)
		{
		ioThreads[n]->shutdown();
		ioThreads[n] = 0;
		}
		
	Sync sync (&syncDirty, "Cache::shutdown");
	sync.lock (Exclusive);

	for (Bdb *bdb = firstDirty; bdb; bdb = bdb->nextDirty)
		bdb->dbb->writePage(bdb, WRITE_TYPE_SHUTDOWN);
}
