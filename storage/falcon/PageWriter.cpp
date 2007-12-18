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

// PageWriter.cpp: implementation of the PageWriter class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "PageWriter.h"
#include "Sync.h"
#include "Thread.h"
#include "Threads.h"
#include "Database.h"
#include "BDB.h"
#include "Cache.h"
#include "Transaction.h"
#include "IOx.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PageWriter::PageWriter(Database *db)
{
	database = db;
	cache = database->cache;
	dirtyLinen = NULL;
	firstPage = NULL;
	lastPage = NULL;
	freePages = NULL;
	freeTransactions = NULL;
	dirtySocks = NULL;
	thread = NULL;
	memset(pageHash, 0, sizeof(pageHash));
	memset(transactions, 0, sizeof(transactions));
	shuttingDown = false;
}

PageWriter::~PageWriter()
{
	for (DirtyLinen *linen; (linen = dirtyLinen);)
		{
		dirtyLinen = linen->next;
		delete linen;
		}

	for (DirtySocks *sock; (sock = dirtySocks);)
		{
		dirtySocks = sock->next;
		delete sock;
		}
}

void PageWriter::writePage(Dbb *dbb, int32 pageNumber, TransId transactionId)
{
	Sync sync(&syncObject, "PageWriter::writePage");
	sync.lock(Exclusive);

	DirtyTrans *transaction = getDirtyTransaction(transactionId);
	DirtyPage *element;
	int slot = pageNumber % dirtyHashSize;
	
	// If we already know about this page, there's nothing more to be done

	for (element = pageHash[slot]; element; element = element->pageCollision)
		if (element->pageNumber == pageNumber && 
			 element->dbb == dbb && 
			 element->transaction == transaction)
			return;

	// Get a dirty page block and link it into the linked list and page and transaction hash tables

	element = getElement();
	element->dbb = dbb;
	element->pageNumber = pageNumber;
	element->transaction = transaction;

	// Link into page queue

	if ( (element->priorPage = lastPage) )
		lastPage->nextPage = element;
	else
		firstPage = element;

	element->nextPage = NULL;
	lastPage = element;

	// Link into page hash table

	ASSERT(pageHash[slot] != element);
	element->pageCollision = pageHash[slot];
	pageHash[slot] = element;

	// Link into transaction

	transaction->addPage(element);
	sync.unlock();
	thread->wake();
}

DirtyPage* PageWriter::getElement()
{
	// Caller must have an exclusive lock on syncObject.

	DirtyPage *element = freePages;

	if (element)
		{
		freePages = element->nextPage;
		return element;
		}

	DirtyLinen *stuff = new DirtyLinen;
	stuff->next = dirtyLinen;
	dirtyLinen = stuff;

	for (int n = 1; n < pagesPerLinen; ++n)
		{
		element = dirtyLinen->dirtyPages + n;
		element->nextPage = freePages;
		freePages = element;
		}

	return dirtyLinen->dirtyPages;
}

void PageWriter::release(DirtyPage *element)
{
	// Caller must have an exclusive lock on syncObject.

	int slot = element->pageNumber % dirtyHashSize;

	for (DirtyPage *el = pageHash[slot]; el; el = el->pageCollision)
		ASSERT(el != element);

	element->nextPage = freePages;
	freePages = element;
}

void PageWriter::writer(void *arg)
{
	((PageWriter*) arg)->writer();
}

void PageWriter::writer()
{
	thread = Thread::getThread("PageWriter::writer");
	Sync sync(&syncObject, "PageWriter::writer");

	while (!thread->shutdownInProgress)
		{
		if (!firstPage)
			thread->sleep();

		sync.lock(Exclusive);
		DirtyPage *element = firstPage;

		if (element)
			{
			removeElement(element);
			sync.unlock();
			Bdb *bdb = cache->probePage(element->dbb, element->pageNumber);
			BDB_HISTORY(bdb);

			if (bdb)
				{
				if (bdb->flags & BDB_dirty)
					cache->writePage(bdb, WRITE_TYPE_PAGE_WRITER);

				bdb->release(REL_HISTORY);
				}

			sync.lock(Exclusive);
			release(element);
			}

		sync.unlock();
		}
}

void PageWriter::start()
{
	database->threads->start("PageWriter::start", writer, this);
}

void PageWriter::pageWritten(Dbb *dbb, int32 pageNumber)
{
	Sync sync(&syncObject, "PageWriter::pageWritten");
	sync.lock(Exclusive);
	int slot = pageNumber % dirtyHashSize;

	for (DirtyPage **ptr = pageHash + slot, *element; (element = *ptr);)
		{
		if (element->pageNumber == pageNumber && element->dbb == dbb)
			{
			removeElement(element);
			release(element);
			return;
			}
		else
			ptr = &element->pageCollision;
		}
}

void PageWriter::removeElement(DirtyPage *element)
{
	// Caller must have an exclusive lock on syncObject.

	int slot = element->pageNumber % dirtyHashSize;
	DirtyPage **ptr;

	// Remove from page hash table

	bool hit = false;

	for (ptr = pageHash + slot; *ptr; ptr = &(*ptr)->pageCollision)
		if (*ptr == element)
			{
			*ptr = element->pageCollision;
			hit = true;
			break;
			}

	ASSERT(hit);

	// Remove from write queue

	if (element->nextPage)
		element->nextPage->priorPage = element->priorPage;
	else
		lastPage = element->priorPage;

	if (element->priorPage)
		element->priorPage->nextPage = element->nextPage;
	else
		firstPage = element->nextPage;

	// Remove from transaction

	DirtyTrans *transaction = element->transaction;
	transaction->removePage(element);

	if (!transaction->firstPage)
		release(transaction);
}

void PageWriter::waitForWrites(Transaction *transaction)
{
	Sync sync(&syncObject, "PageWriter::waitForWrites");
	Thread *thread = NULL;

	for (;;)
		{
		sync.lock(Exclusive);
		DirtyTrans *dirtyTrans = findDirtyTransaction(transaction->transactionId);

		if (!dirtyTrans)
			return;

		if (!thread)
			thread = Thread::getThread("PageWriter::waitForWrites");

		dirtyTrans->thread = thread;
		//transaction->printBlockage();
		sync.unlock();
		thread->sleep();
		}
}

DirtyTrans* PageWriter::findDirtyTransaction(TransId transactionId)
{
	// Caller must have at least a shared lock on syncObject.

	for (DirtyTrans *trans = transactions[transactionId % dirtyHashSize]; trans; trans = trans->collision)
		if (trans->transactionId == transactionId)
			return trans;

	return NULL;
}

DirtyTrans* PageWriter::getDirtyTransaction(TransId transactionId)
{
	// Caller must have an exclusive lock on syncObject.

	DirtyTrans *transaction = findDirtyTransaction(transactionId);

	if (transaction)
		return transaction;

	if ( (transaction = freeTransactions) )
		freeTransactions = transaction->next;
	else
		{
		DirtySocks *stuff = new DirtySocks;
		stuff->next = dirtySocks;
		dirtySocks = stuff;

		for (int n = 1; n < pagesPerLinen; ++n)
			{
			DirtyTrans *trans = stuff->transactions + n;
			trans->next = freeTransactions;
			freeTransactions = trans;
			}

		transaction = stuff->transactions;
		}

	transaction->clear();
	transaction->transactionId = transactionId;
	int slot = transactionId % dirtyHashSize;
	transaction->collision = transactions[slot];
	transactions[slot] = transaction;

	return transaction;
}

void PageWriter::release(DirtyTrans *transaction)
{
	// Caller must have an exclusive lock on syncObject.

	int slot = transaction->transactionId % dirtyHashSize;

	for (DirtyTrans **ptr = transactions + slot; *ptr; ptr = &(*ptr)->collision)
		if (*ptr == transaction)
			{
			*ptr = transaction->collision;
			break;
			}

	if (transaction->thread)
		transaction->thread->wake();

	transaction->next = freeTransactions;
	freeTransactions = transaction;
}

void DirtyTrans::addPage(DirtyPage *page)
{
	// Caller must have an exclusive lock on syncObject.

	if ( (page->transPrior = lastPage) )
		lastPage->transNext = page;
	else
		firstPage = page;

	page->transNext = NULL;
	lastPage = page;
}

void DirtyTrans::clear()
{
	firstPage = NULL;
	lastPage = NULL;
	thread = NULL;
}

void DirtyTrans::removePage(DirtyPage *page)
{
	// Caller must have an exclusive lock on syncObject.

	if (page->transNext)
		page->transNext->transPrior = page->transPrior;
	else
		lastPage = page->transPrior;

	if (page->transPrior)
		page->transPrior->transNext = page->transNext;
	else
		firstPage = page->transNext;

	page->transaction = NULL;
}


void PageWriter::shutdown(bool panic)
{
	if (shuttingDown)
		return;
	
	shuttingDown = true;
	
	if (thread)
		thread->shutdown();
}

void PageWriter::validate()
{
	Sync sync(&syncObject, "PageWriter::validate");
	sync.lock(Shared);
	DirtyPage *element;

	for (element = firstPage; element; element = element->nextPage)
		{
		ASSERT(element == firstPage || element == element->priorPage->nextPage);
		ASSERT(element == lastPage || element == element->nextPage->priorPage);
		int slot = element->pageNumber % dirtyHashSize;
		int hit = false;

		for (DirtyPage *dup = pageHash [slot]; dup; dup = dup->pageCollision)
			if (dup == element)
				{
				hit = true;
				break;
				}

		ASSERT(hit);
		}
}
