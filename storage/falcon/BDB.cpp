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

// Bdb.cpp: implementation of the Bdb class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "BDB.h"
#include "Cache.h"
#include "Interlock.h"
#include "PagePrecedence.h"
#include "PageWriter.h"
#include "Thread.h"
#include "SQLError.h"
#include "Dbb.h"
#include "Log.h"
#include "Database.h"
#include "Sync.h"
#include "Page.h"

//#define TRACE_PAGE 130049

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Bdb::Bdb()
{
	cache = NULL;
	next = NULL;
	prior = NULL;
	hash = NULL;
	buffer = NULL;
	flags = 0;
	pageNumber = -1;
	useCount = 0;
	age = 0;
	higher = lower = NULL;
	markingThread = NULL;
	priorDirty = nextDirty = NULL;
	flushIt = false;
	dbb = NULL;

#ifdef COLLECT_BDB_HISTORY
	lockType = None;
	initCount = 0;
	historyCount = 0;
	memset(history, 0, sizeof(history));
#endif
}

Bdb::~Bdb()
{
	PagePrecedence *precedence;

	while ( (precedence = higher) )
		cache->clearPrecedence (precedence);

	while ( (precedence = lower) )
		cache->clearPrecedence (precedence);

}


void Bdb::mark(TransId transId)
{
	ASSERT (useCount > 0);
	ASSERT (lockType == Exclusive);
	transactionId = transId;
	//cache->validateCache();
	lastMark = cache->database->timestamp;

#ifdef TRACE_PAGE
	if (pageNumber == TRACE_PAGE)
		Log::debug("Marking page %d/%d\n", pageNumber, dbb->tableSpaceId);
#endif

	if (!markingThread)
		{
		markingThread = syncObject.getExclusiveThread();
		++markingThread->pageMarks;
		}

	if (!(flags & BDB_dirty))
		{
		flags |= BDB_dirty;
		cache->markDirty (this);
		}
}

void Bdb::addRef(LockType lType)
{
	incrementUseCount();
	syncObject.lock (NULL, lType);
	lockType = lType;
}

void Bdb::release()
{
#ifdef HAVE_PAGE_NUMBER
	ASSERT(buffer->pageNumber == pageNumber);
#endif

	ASSERT (useCount > 0);
	decrementUseCount();

	if (markingThread)
		{
		//cache->validateCache();
		--markingThread->pageMarks;
		markingThread = NULL;
		}

	if (flags & BDB_register)
		{
		cache->pageWriter->writePage(dbb, pageNumber, transactionId);
		flags &= ~BDB_register;
		}

	syncObject.unlock (NULL, lockType);

	if (cache->panicShutdown)
		{
		Thread *thread = Thread::getThread("Cache::fetchPage");
		
		if (thread->pageMarks == 0)
			throw SQLError(RUNTIME_ERROR, "Emergency shut is underway");
		}

}

void Bdb::setPageHeader(short type)
{
	buffer->pageType = type;

#ifdef HAVE_PAGE_NUMBER
	buffer->pageNumber = pageNumber;
#endif
}

void Bdb::downGrade(LockType lType)
{
	ASSERT (lockType == Exclusive);
	lockType = lType;
	syncObject.downGrade (lType);
}

void Bdb::incrementUseCount()
{
	INTERLOCKED_INCREMENT (useCount);
}

void Bdb::decrementUseCount()
{
	ASSERT (useCount > 0);
	INTERLOCKED_DECREMENT (useCount);
}

/***
void Bdb::setPrecedence(int32 priorPage)
{
	cache->setPrecedence (this, priorPage);
}
***/

bool Bdb::isHigher(Bdb *bdb)
{
	if (this == bdb)
		return true;

	for (PagePrecedence *prec = higher; prec; prec = prec->nextHigh)
		if (prec->higher->isHigher (bdb))
			return true;

	return false;
}

void Bdb::setWriter()
{
	flags |= BDB_writer | BDB_register;
}

#ifdef COLLECT_BDB_HISTORY
void Bdb::addRef(LockType lType, int category, const char *file, int line)
{
	addRef(lType);
	addHistory(category, file, line);
}

void Bdb::release(int category, const char *file, int line)
{
	release();
	addHistory(category, file, line);
}

void Bdb::incrementUseCount(int category, const char *file, int line)
{
	INTERLOCKED_INCREMENT (useCount);
	addHistory(category, file, line);
}

void Bdb::decrementUseCount(int category, const char *file, int line)
{
	ASSERT (useCount > 0);
	INTERLOCKED_DECREMENT (useCount);
	addHistory(category, file, line);
}

void Bdb::initHistory()
{
	initCount++;
	historyCount = 0;
	memset(history, 0, sizeof(history));
}

void Bdb::addHistory(int delta, const char *file, int line)
{
	Sync sync (&historySyncObject, "Bdb::addHistory");
	sync.lock (Exclusive);
	unsigned int historyOffset = historyCount++ % MAX_BDB_HISTORY;

	#ifdef _WIN32
	history[historyOffset].threadId = (unsigned long) GetCurrentThreadId();
	#endif
	#ifdef _PTHREADS
	history[historyOffset].threadId = (unsigned long) pthread_self();
	#endif

	// lockType is never set to None from Shared since a shared lock 
	// can be added concurrently.   So let's try to catch a lock of None here.
	history[historyOffset].lockType = lockType;
	if (syncObject.getState() == 0)
		history[historyOffset].lockType = None;

	history[historyOffset].useCount = useCount;
	history[historyOffset].delta  = delta;
	strncpy(history[historyOffset].file, file, BDB_HISTORY_FILE_LEN - 1);
	history[historyOffset].line = line;
}
#endif


