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

// IndexRootPage.cpp: implementation of the IndexRootPage class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include <string.h>
#include "Engine.h"
#include "IndexRootPage.h"
#include "IndexPage.h"
#include "Dbb.h"
#include "BDB.h"
#include "Section.h"
#include "SectionPage.h"
#include "Bitmap.h"
#include "IndexNode.h"
#include "SQLError.h"
#include "Log.h"
#include "LogLock.h"
#include "IndexKey.h"
#include "SerialLogControl.h"
#include "Transaction.h"
#include "Index.h"
#include "SRLUpdateIndex.h"

static IndexKey dummyKey;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void IndexRootPage::create(Dbb * dbb, TransId transId)
{
	Bdb *bdb = dbb->fakePage (INDEX_ROOT, PAGE_sections, transId);
	BDB_HISTORY(bdb);
	IndexRootPage *sections = (IndexRootPage*) bdb->buffer;
	sections->section = -1;
	sections->level = 0;
	sections->sequence = 0;
	bdb->release(REL_HISTORY);
}

int32 IndexRootPage::createIndex(Dbb * dbb, TransId transId)
{
	int sequence = -1;
	Bdb *sectionsBdb = NULL;
	SectionPage	*sections = NULL;
	int skipped = 0;
	
	for (int32 id = dbb->nextIndex;; ++id)
		{
		int n = id / dbb->pagesPerSection;

		if (n != sequence)
			{
			sequence = n;
			
			if (sectionsBdb)
				sectionsBdb->release(REL_HISTORY);
				
			sectionsBdb = Section::getSectionPage(dbb, INDEX_ROOT, n, Exclusive, transId);
			BDB_HISTORY(sectionsBdb);
			sections = (SectionPage*) sectionsBdb->buffer;
			sequence = n;
			}

		int slot = id % dbb->pagesPerSection;

		if (sections->pages [slot] == 0)
			{
			if (dbb->indexInUse(id))
				{
				if (!skipped)
					skipped = id;
				}
			else
				{
				sectionsBdb->mark(transId);
				Bdb *indexBdb = createIndexRoot(dbb, transId);
				BDB_HISTORY(indexBdb);
				sections->pages [slot] = indexBdb->pageNumber;
				IndexPage::logIndexPage(indexBdb, transId);
				indexBdb->release(REL_HISTORY);
				sectionsBdb->release(REL_HISTORY);
				dbb->nextIndex = (skipped) ? skipped : id + 1;

				return id;
				}
			}
		}
}

bool IndexRootPage::addIndexEntry(Dbb * dbb, int32 indexId, IndexKey *key, int32 recordNumber, TransId transId)
{
	IndexKey searchKey(key);
	searchKey.appendRecordNumber(recordNumber);
	
	if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_ADDED | DEBUG_PAGE_LEVEL))
		{
		LogLock logLock;
		Log::debug("\n***** addIndexEntry(%d, %d)\n", recordNumber, indexId);
		Btn::printKey ("insertion key", key, 0, false);
		Btn::printKey (" appended key", &searchKey, 0, false);
		}
		
	// Multiple threads may attempt to update the same index. If necessary, make several attempts.
	
	for (int n = 0; n < 10; ++n)
		{
		/* Find insert page and position on page */

		Bdb *bdb = findInsertionLeaf(dbb, indexId, &searchKey, recordNumber, transId);

		if (!bdb)
			return false;

		IndexPage *page;

		for (;;)
			{
			IndexKey priorKey;
			page = (IndexPage*) bdb->buffer;
			Btn *node = page->findNodeInLeaf(key, &priorKey);
			Btn *bucketEnd = (Btn*) ((char*) page + page->length);

			if (node < bucketEnd || page->nextPage == 0)
				break;

			ASSERT (bdb->pageNumber != page->nextPage);
			bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Exclusive);
			BDB_HISTORY(bdb);
			}

		bdb->mark(transId);

		/* If the node fits on page, we're done */

		AddNodeResult result;

		for (;;)
			{
			result = page->addNode(dbb, key, recordNumber);

			if (result != NextPage)
				break;

			int32 parentPageNumber = page->parentPage;
			bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Exclusive);
			BDB_HISTORY(bdb);
			bdb->mark(transId);
			page = (IndexPage*) bdb->buffer;
			page->parentPage = parentPageNumber;
			}

		if (result == NodeAdded || result == Duplicate)
			{
			if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_ADDED))
				page->printPage (bdb, false);
				
			//page->validateInsertion (length, key, recordNumber);
			bdb->release(REL_HISTORY);

			return true;
			}

		/* Node didn't fit.  Split the page and propogate the
		   split upward.  Sooner or laster we'll go back and re-try
		   the original insertion */

		if (splitIndexPage (dbb, indexId, bdb, transId, result, key, recordNumber))
			return true;

#ifdef _DEBUG
		if (n)
			{
			++dbb->debug;
			Btn::printKey ("Key", key, 0, false);
			Btn::printKey ("SearchKey", &searchKey, 0, false);
			Bdb *bdb = findInsertionLeaf (dbb, indexId, &searchKey, recordNumber, transId);
			IndexPage *page = (IndexPage*) bdb->buffer;

			while (page->nextPage)
				{
				bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Exclusive);
				BDB_HISTORY(bdb);
				page = (IndexPage*) bdb->buffer;
				page->printPage(bdb, false);
				}

			bdb->release(REL_HISTORY);
			--dbb->debug;
			}
#endif
		}


	ASSERT (false);
	FATAL ("index split failed");

	return false;
}

Bdb* IndexRootPage::findLeaf(Dbb *dbb, int32 indexId, int32 rootPage, IndexKey *indexKey, LockType lockType, TransId transId)
{
	Bdb *bdb = findRoot(dbb, indexId, rootPage, lockType, transId);
	BDB_HISTORY(bdb);

	if (!bdb)
		return NULL;

	IndexPage *page = (IndexPage*) bdb->buffer;

	if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF))
		page->printPage (bdb, false);

	while (page->level > 0)
		{
		IndexNode node (page->findNodeInBranch (indexKey, 0));
		int32 pageNumber = node.getNumber();

		if (pageNumber == END_BUCKET)
			pageNumber = page->nextPage;

		if (pageNumber == 0)
			{
			page->printPage (bdb, false);
			//node.parseNode(page->findNodeInBranch (indexKey, 0));
			bdb->release(REL_HISTORY);
			throw SQLError (DATABASE_CORRUPTION, "index %d damaged", indexId);
			}

		bdb = dbb->handoffPage(bdb, pageNumber, PAGE_btree,
								(page->level > 1) ? Shared : lockType);
		BDB_HISTORY(bdb);
		page = (IndexPage*) bdb->buffer;

		if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF))
			page->printPage (bdb, false);
		}

	return bdb;
}
Bdb* IndexRootPage::findInsertionLeaf(Dbb *dbb, int32 indexId, IndexKey *indexKey, int32 recordNumber, TransId transId)
{
	Bdb *bdb = findRoot(dbb, indexId, 0, Shared, transId);
	BDB_HISTORY(bdb);

	if (!bdb)
		return NULL;

	IndexPage *page = (IndexPage*) bdb->buffer;
	
	if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF))
		page->printPage (bdb, false);

	if (page->level == 0)
		{
		bdb->release(REL_HISTORY);
		
		bdb = findRoot(dbb, indexId, 0, Exclusive, transId);
		BDB_HISTORY(bdb);
		if (!bdb)
			return NULL;
			
		page = (IndexPage*) bdb->buffer;
		
		if (page->level == 0)
			return bdb;
		}
		
	while (page->level > 0)
		{
		IndexNode node (page->findNodeInBranch(indexKey, recordNumber));
		int32 pageNumber = node.getNumber();

		if (pageNumber == END_BUCKET)
			pageNumber = page->nextPage;

		if (pageNumber == 0)
			{
			page->printPage (bdb, false);
			//node.parseNode(page->findNodeInBranch (indexKey, recordNumber));	// try again for debugging
			bdb->release(REL_HISTORY);
			throw SQLError (DATABASE_CORRUPTION, "index %d damaged", indexId);
			}

		int level = page->level;
		int32 nextPage = page->nextPage;

		int32 parentPage = bdb->pageNumber;
		bdb = dbb->handoffPage(bdb, pageNumber, PAGE_btree, 
								(page->level > 1) ? Shared : Exclusive);
		BDB_HISTORY(bdb);
		page = (IndexPage*) bdb->buffer;

		// Verify that the new page is either a child page or the next page on
		// the same level.
		
		ASSERT((page->level == level - 1) || (pageNumber == nextPage));
		ASSERT(page->level > 0 || bdb->lockType == Exclusive);

		if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF))
			page->printPage (bdb, false);
		
		page->parentPage = parentPage;
		}

	return bdb;
}

Bdb* IndexRootPage::findRoot(Dbb *dbb, int32 indexId, int32 rootPage, LockType lockType, TransId transId)
{
	if (rootPage)
		return dbb->fetchPage(rootPage, PAGE_btree, lockType);
		
	if (indexId < 0)
		return NULL;

	Bdb *bdb = Section::getSectionPage (dbb, INDEX_ROOT, indexId / dbb->pagesPerSection, Shared, transId);
	BDB_HISTORY(bdb);

	if (!bdb)
		return NULL;

	SectionPage *sections = (SectionPage*) bdb->buffer;
	int32 pageNumber = sections->pages [indexId % dbb->pagesPerSection];

	if (pageNumber == 0)
		{
		bdb->release(REL_HISTORY);
		return NULL;
		}

	bdb = dbb->handoffPage (bdb, pageNumber, PAGE_btree, lockType);
	BDB_HISTORY(bdb);
	return bdb;
}

void IndexRootPage::scanIndex  (Dbb *dbb,
								int32 indexId,
								int32 rootPage,
								IndexKey *lowKey,
								IndexKey *highKey,
								int searchFlags,
								TransId transId,
								Bitmap *bitmap)
{
	IndexKey key;
	uint offset = 0;

	if (dbb->debug & (DEBUG_KEYS | DEBUG_SCAN_INDEX))
		{
		LogLock logLock;
		Btn::printKey ("lower: ", lowKey, 0, false);
		Btn::printKey ("upper: ", highKey, 0, false);
		}

	if (!lowKey)
		lowKey = &dummyKey;
		
	/* Find leaf page and position on page */

	Bdb *bdb = findLeaf (dbb, indexId, rootPage, lowKey, Shared, transId);

	if (!bdb)
		throw SQLError (RUNTIME_ERROR, "can't find index %d", indexId);

	IndexPage *page = (IndexPage*) bdb->buffer;
	
	if (dbb->debug & (DEBUG_PAGES | DEBUG_SCAN_INDEX))
		page->printPage (bdb, false);
		
	Btn *end = page->getEnd();
	IndexNode node(page->findNodeInLeaf(lowKey, &key), end);
	UCHAR *endKey = (highKey) ? highKey->key + highKey->keyLength : 0;

	/* If we didn't find it here, try the next page */

	while (node.node >= end)
		{
		if (!page->nextPage)
			{
			bdb->release(REL_HISTORY);
			
			return;
			}
			
		bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Shared);
		BDB_HISTORY(bdb);
		page = (IndexPage*) bdb->buffer;
		end = page->getEnd();
		node.parseNode(page->findNodeInLeaf(lowKey, &key), end);
		}

	if (highKey && node.node < end)
		{
		ASSERT (node.offset + node.length < sizeof(lowKey->key));
		node.expandKey (&key);
		offset = page->computePrefix (&key, highKey);
		}

	/* Scan index setting bits */

	for (;;)
		{
		for (; node.node < end; node.getNext(end))
			{
			if (highKey)
				{
				if (node.offset <= offset)
					{
					UCHAR *p = highKey->key + node.offset;
					UCHAR *q = node.key;
					
					for (int length = node.length; length; --length)
						{
						if (p >= endKey)
							{
							if (searchFlags & Partial)		// this is highly suspect
								break;
								
							bdb->release(REL_HISTORY);
							
							return;
							}
							
						if (*p < *q)
							{
							bdb->release(REL_HISTORY);
							
							return;
							}
							
						if (*p++ > *q++)
							break;
							
						offset = (int) (p - highKey->key);
						}
					}
				}
				
			int number = node.getNumber();
			
			if (number < 0)
				break;
				
			bitmap->set (number);
			}

		if (!page->nextPage)
			{
			bdb->release(REL_HISTORY);
			
			return;
			}
			
		bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Shared);
		BDB_HISTORY(bdb);
		page = (IndexPage*) bdb->buffer;
		node.parseNode(page->nodes);
		end = page->getEnd();
		offset = 0;
		
		if (dbb->debug & (DEBUG_PAGES | DEBUG_SCAN_INDEX))
			page->printPage (bdb, false);
		}
}

bool IndexRootPage::splitIndexPage(Dbb * dbb, int32 indexId, Bdb * bdb, TransId transId,
								   AddNodeResult addResult, IndexKey *indexKey, int recordNumber)
{
	// Start by splitting page (allocating new page)

	IndexPage *page = (IndexPage*) bdb->buffer;
	IndexKey splitKey(indexKey->index);
	bool inserted = false;
	Bdb *splitBdb;
	
	if (addResult == SplitEnd)
		{
		// int prevDebug = dbb->debug;
		// dbb->debug = DEBUG_PAGES;
		splitBdb = page->splitIndexPageEnd (dbb, bdb, transId, indexKey, recordNumber);
		//dbb->debug = prevDebug;
		inserted = true;
		}
	else
		splitBdb = page->splitIndexPageMiddle (dbb, bdb, &splitKey, transId);

	IndexPage *splitPage = (IndexPage*) splitBdb->buffer;
	IndexNode splitNode(splitPage);
	splitKey.setKey(splitNode.keyLength(), splitNode.key);
	int32 splitRecordNumber = 0;
	
	if (splitPage->level == 0)
		{
		splitRecordNumber = splitNode.getNumber();
		splitKey.appendRecordNumber(splitRecordNumber);
		}

	// If there isn't a parent page, we need to create a new level.  Do so.

	if (!page->parentPage)
		{
		// Allocate and copy a new left-most index page
		
		Bdb *leftBdb = dbb->allocPage(PAGE_btree, transId);
		BDB_HISTORY(leftBdb);
		IndexPage *leftPage = (IndexPage*) leftBdb->buffer;
		memcpy(leftPage, page, page->length);
		leftBdb->setPageHeader(leftPage->pageType);
//		leftPage->pageNumber = leftBdb->pageNumber;
		splitPage->priorPage = leftBdb->pageNumber;
		
		// Create a node referencing the leftmost page. Assign to it a null key
		// and record number 0 to ensure that all nodes are inserted after it.
		
		IndexKey leftKey(indexKey->index);

		if (leftPage->level == 0)
			leftKey.appendRecordNumber(0);
			
		// Reinitialize the parent page
		
		page->level = splitPage->level + 1;
		page->version = splitPage->version;
		page->nextPage = 0;
		IndexKey dummy(indexKey->index);
		dummy.keyLength = 0;
		page->length = OFFSET (IndexPage*, nodes);
		page->addNode(dbb, &dummy, END_LEVEL);
		page->addNode(dbb, &leftKey, leftBdb->pageNumber);
		page->addNode(dbb, &splitKey, splitBdb->pageNumber);
		
		leftPage->parentPage = bdb->pageNumber;
		splitPage->parentPage = bdb->pageNumber;
		IndexPage::logIndexPage(bdb, transId);
		IndexPage::logIndexPage(splitBdb, transId);
		IndexPage::logIndexPage(leftBdb, transId);
		
		/***
		IndexPage::printPage(bdb, false);
		IndexPage::printPage(leftBdb, false);
		IndexPage::printPage(splitBdb, false);
		***/
		
		if (dbb->debug & DEBUG_PAGE_LEVEL)
			{
			Log::debug("\n***** splitIndexPage(%d, %d) - NEW LEVEL: Parent page = %d\n", recordNumber, indexId, bdb->pageNumber);
			page->printPage(page, 0, false, false);
			Log::debug("\n***** splitIndexPage(%d, %d) - NEW LEVEL: Left page = %d \n" , recordNumber, indexId, leftBdb->pageNumber);
			page->printPage(leftPage, 0, true, false);
			Log::debug("\n***** splitIndexPage(%d, %d) - NEW LEVEL: Split page = %d \n", recordNumber, indexId, splitBdb->pageNumber);
			page->printPage(splitPage, 0, false, false);
			}
		
		dbb->setPrecedence(leftBdb, splitBdb->pageNumber);
		dbb->setPrecedence(bdb, leftBdb->pageNumber);
		splitBdb->release(REL_HISTORY);
		leftBdb->release(REL_HISTORY);
		bdb->release(REL_HISTORY);
		
		return false;
		}

	// We need to propogate the split upward.  Start over from the top
	// to find the insertion point.  Try to insert.  If successful, be happy

	int level = splitPage->level + 1;
	IndexPage::logIndexPage(bdb, transId);
	bdb->release(REL_HISTORY);
	int splitPageNumber = splitBdb->pageNumber;
	splitBdb->release(REL_HISTORY);
		
	for (;;)
		{
		bdb = findRoot(dbb, indexId, 0, Exclusive, transId);
		BDB_HISTORY(bdb);
		page = (IndexPage*) bdb->buffer;
		bdb = IndexPage::findLevel(dbb, indexId, bdb, level, &splitKey, splitRecordNumber);
		BDB_HISTORY(bdb);
		bdb->mark(transId);
		page = (IndexPage*) bdb->buffer;

		// If we can add the node, we're happy
		
		AddNodeResult result = page->addNode(dbb, &splitKey, splitPageNumber);

		if (result == NodeAdded || result == Duplicate)
			{
			dbb->setPrecedence(bdb, splitPageNumber);
			splitBdb = dbb->fetchPage (splitPageNumber, PAGE_btree, Exclusive);
			BDB_HISTORY(splitBdb);
			splitBdb->mark (transId);
			splitPage = (IndexPage*) splitBdb->buffer;
			splitPage->parentPage = bdb->pageNumber;
			bdb->release(REL_HISTORY);
			splitBdb->release(REL_HISTORY);

			return false;
			}

		// That page needs to be split.  Recurse

		if (splitIndexPage (dbb, indexId, bdb, transId, 
							result, &splitKey, splitPageNumber))
			return true;
		}
}

bool IndexRootPage::deleteIndexEntry(Dbb * dbb, int32 indexId, IndexKey *key, int32 recordNumber, TransId transId)
{
	IndexPage *page;
	IndexKey searchKey(key);
	searchKey.appendRecordNumber(recordNumber);

	if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_DELETED))
		{
		Btn::printKey ("deletion key", key, 0, false);
		Btn::printKey ("  search key", &searchKey, 0, false);
		}
		
	// Try to delete node.  If necessary, chase to next page.

	for (Bdb *bdb = findInsertionLeaf (dbb, indexId, &searchKey, recordNumber, transId); bdb;
		 bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Exclusive))
		{
		BDB_HISTORY(bdb);
		page = (IndexPage*) bdb->buffer;
		bdb->mark(transId);
		int result = page->deleteNode (dbb, key, recordNumber);

		if (result || page->nextPage == 0)
			{
			bdb->release(REL_HISTORY);

			return result > 0;
			}
		}	

	return false;
}

void IndexRootPage::deleteIndex(Dbb *dbb, int32 indexId, TransId transId)
{
	Bdb *bdb = Section::getSectionPage (dbb, INDEX_ROOT, indexId / dbb->pagesPerSection, Exclusive, transId);
	BDB_HISTORY(bdb);
	if (!bdb)
		return;

	bdb->mark (transId);
	SectionPage *sections = (SectionPage*) bdb->buffer;
	int32 firstPageNumAtNextLevel = sections->pages [indexId % dbb->pagesPerSection];
	sections->pages [indexId % dbb->pagesPerSection] = 0;
	dbb->nextIndex = MIN(dbb->nextIndex, indexId);
	bdb->release(REL_HISTORY);

	while (firstPageNumAtNextLevel)
		{
		bdb = dbb->fetchPage (firstPageNumAtNextLevel, PAGE_any, Exclusive);
		BDB_HISTORY(bdb);
		IndexPage *page = (IndexPage*) bdb->buffer;
		
		if (page->pageType != PAGE_btree)
			{
			Log::logBreak ("Drop index %d: bad level page %d\n", indexId, bdb->pageNumber);
			bdb->release(REL_HISTORY);
			
			break;
			}
			
		IndexNode node (page->nodes);
		firstPageNumAtNextLevel = (page->level) ? node.getNumber() : 0;
		
		for (;;)
			{
			int32 nextPage = page->nextPage;
			dbb->freePage (bdb, transId);
			
			if (!nextPage)
				break;
				
			bdb = dbb->fetchPage (nextPage, PAGE_any, Exclusive);
			BDB_HISTORY(bdb);
			page = (IndexPage*) bdb->buffer;
			
			if (page->pageType != PAGE_btree)
				{
				Log::logBreak ("Drop index %d: bad index page %d\n", indexId, bdb->pageNumber);
				bdb->release(REL_HISTORY);
				break;
				}
			}
		}
}


void IndexRootPage::debugBucket(Dbb *dbb, int indexId, int recordNumber, TransId transactionId)
{
	Bdb *bdb = findRoot (dbb, indexId, 0, Exclusive, transactionId);
	BDB_HISTORY(bdb);
	IndexPage *page;
	IndexNode node;
	int debug = 1;

	// Find leaf

	for (;;)
		{
		page = (IndexPage*) bdb->buffer;
		
		if (debug)
			page->printPage (bdb, false);
			
		node.parseNode (page->nodes);
		
		if (page->level == 0)
			break;
			
		bdb = dbb->handoffPage (bdb, node.getNumber(), PAGE_btree, Exclusive);
		BDB_HISTORY(bdb);
		}

	// Scan index

	Btn *end = page->getEnd();
	int pages = 0;
	int nodes = 0;

	/* If we didn't find it here, try the next page */

	for (;;)
		{
		++pages;
		
		for (; node.node < end; node.getNext(end))
			{
			++nodes;
			int number = node.getNumber();
			
			if (number < 0)
				break;
				
			if (number == recordNumber)
				page->printPage (bdb, false);
			}
			
		if (!page->nextPage)
			break;
			
		bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Exclusive);
		BDB_HISTORY(bdb);
		page = (IndexPage*) bdb->buffer;
		
		if (debug)
			page->printPage (bdb, false);
			
		node.parseNode (page->nodes);
		end = page->getEnd();
		}

	bdb->release(REL_HISTORY);
}

void IndexRootPage::redoIndexPage(Dbb* dbb, int32 pageNumber, int32 parentPage, int level, int32 priorPage, int32 nextPage, int length, const UCHAR *data)
{
	//Log::debug("redoIndexPage %d -> %d -> %d level %d, parent %d)\n", priorPage, pageNumber, nextPage, level, parentPage);
	Bdb *bdb = dbb->fakePage(pageNumber, PAGE_any, 0);
	BDB_HISTORY(bdb);
	IndexPage *indexPage = (IndexPage*) bdb->buffer;

	//  Try to read page.  If it looks OK, keep it.  Otherwise, rebuild it

	if (!dbb->trialRead(bdb) ||
		 indexPage->pageType != PAGE_btree ||
		 indexPage->parentPage != parentPage ||
		 indexPage->level == level)
		{
		memset(indexPage, 0, dbb->pageSize);
		//indexPage->pageType = PAGE_btree;
		bdb->setPageHeader(PAGE_btree);
		}

	indexPage->level = level;
	indexPage->parentPage = parentPage;
	indexPage->nextPage = nextPage;
	indexPage->priorPage = priorPage;
	indexPage->length = length + (int32) OFFSET (IndexPage*, nodes);
	memcpy(indexPage->nodes, data, length);
	
	// If we have a parent page, propogate the first node upward

	if (parentPage && indexPage->priorPage != 0)
		{
		IndexNode node(indexPage);
		int number = node.getNumber();

		if (number >= 0)
			{
			IndexKey indexKey(node.keyLength(), node.key);
			Bdb *parentBdb = dbb->fetchPage(parentPage, PAGE_btree, Exclusive);
			BDB_HISTORY(parentBdb);
			IndexPage *parent = (IndexPage*) parentBdb->buffer;
			
			// Assertion disabled--for debug only.
			// The parent page pointer is redundant and not always reliable.
			// During an index split, lower level pages may be given a new
			// parent page, but the parent pointers are not adjusted.
			
			//ASSERT(parent->level == indexPage->level + 1);
			
			if (level == 0)
				indexKey.appendRecordNumber(number);
				
			parentBdb->mark(NO_TRANSACTION);
			//AddNodeResult result = 
			parent->addNode(dbb, &indexKey, pageNumber);
			parentBdb->release(REL_HISTORY);
			}
		}

	bdb->release(REL_HISTORY);
	
	if (nextPage)
		{
		bdb = dbb->trialFetch(nextPage, PAGE_any, Exclusive);
		BDB_HISTORY(bdb);
		
		if (bdb)
			{
			indexPage = (IndexPage*) bdb->buffer;
			
			if (indexPage->pageType == PAGE_btree &&
				indexPage->parentPage == parentPage &&
				indexPage->level == level)
				{
				if (indexPage->priorPage != pageNumber)
					{
					bdb->mark(NO_TRANSACTION);
					indexPage->priorPage = pageNumber;
					}
				}
			
			bdb->release(REL_HISTORY);
			}
		}
		
	if (priorPage)
		{
		bdb = dbb->trialFetch(priorPage, PAGE_any, Exclusive);
		BDB_HISTORY(bdb);
		
		if (bdb)
			{
			indexPage = (IndexPage*) bdb->buffer;
			
			if (indexPage->pageType == PAGE_btree &&
				indexPage->parentPage == parentPage &&
				indexPage->level == level)
				{
				if (indexPage->nextPage != pageNumber)
					{
					bdb->mark(NO_TRANSACTION);
					indexPage->nextPage = pageNumber;
					}
				}
				
			bdb->release(REL_HISTORY);
			}
		}
}

void IndexRootPage::setIndexRoot(Dbb* dbb, int indexId, int32 pageNumber, TransId transId)
{
	int sequence = indexId / dbb->pagesPerSection;
	int slot = indexId % dbb->pagesPerSection;
	Bdb *bdb = Section::getSectionPage (dbb, INDEX_ROOT, sequence, Exclusive, transId);
	BDB_HISTORY(bdb);
	dbb->setPrecedence(bdb, pageNumber);
	bdb->mark(transId);
	SectionPage *sections = (SectionPage*) bdb->buffer;
	sections->pages[slot] = pageNumber;
	
	if (!dbb->serialLog->recovering)
		dbb->serialLog->logControl->sectionPage.append(dbb, transId, bdb->pageNumber, pageNumber, slot, INDEX_ROOT, sequence, 0);

	bdb->release(REL_HISTORY);
}

void IndexRootPage::redoIndexDelete(Dbb* dbb, int indexId)
{
	setIndexRoot(dbb, indexId, 0, NO_TRANSACTION);
}

void IndexRootPage::indexMerge(Dbb *dbb, int indexId, SRLUpdateIndex *logRecord, TransId transId)
{
	IndexKey key;
	int recordNumber = logRecord->nextKey(&key);
	int duplicates = 0;
	int insertions = 0;
	int punts = 0;
	int rollovers = 0;
	
	for (; recordNumber != -1;)
		{
		// Position to insert first key (clone of addIndexEntry)
		
		IndexKey searchKey(key);
		searchKey.appendRecordNumber(recordNumber);
			
		if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_DELETED | DEBUG_PAGE_LEVEL))
			{
			LogLock logLock;
			Log::debug("\n***** indexMerge(%d, %d) - Key\n", recordNumber, indexId);
			Btn::printKey ("insertion key", &key, 0, false);
			Btn::printKey (" appended key", &searchKey, 0, false);
			}
		
		Bdb *bdb = findInsertionLeaf(dbb, indexId, &searchKey, recordNumber, transId);
		ASSERT(bdb);
		IndexPage *page = (IndexPage*) bdb->buffer;
		Btn *bucketEnd = (Btn*) ((char*) page + page->length);
		IndexKey priorKey;
		IndexNode node(page->findInsertionPoint(&key, recordNumber, &priorKey));
		IndexKey nextKey;
		nextKey.setKey(0, node.offset, priorKey.key);
		node.expandKey(&nextKey);
		int number = node.getNumber();
		
		// If we need to go to the next page, do it now
		
		while (number == END_BUCKET && nextKey.compare(&key) > 0)
			{
			if (!page->nextPage)
				{
				bdb->release(REL_HISTORY);
				bdb = NULL;
				++punts;
				addIndexEntry(dbb, indexId, &key, recordNumber, transId);
				
				if ( (recordNumber = logRecord->nextKey(&key)) == -1)
					//return;
					goto exit;
				
				break;
				}
				
			nextKey.compare(&key);
			ASSERT (bdb->pageNumber != page->nextPage);
			bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Exclusive);
			BDB_HISTORY(bdb);
			page = (IndexPage*) bdb->buffer;
			node.parseNode(page->findInsertionPoint(&key, recordNumber, &priorKey));
			nextKey.setKey(0, node.offset, priorKey.key);
			node.expandKey(&nextKey);
			number = node.getNumber();
			++rollovers;
			}
		
		if (!bdb)
			continue;

		bdb->mark(transId);

		for (;;)
			{
			if (number == recordNumber && nextKey.compare(&key) == 0)
				++duplicates;
			else
				{
				int offset1 = IndexPage::computePrefix (&priorKey, &key);
				int length1 = key.keyLength - offset1;
				int delta = IndexNode::nodeLength(offset1, length1, recordNumber);
				int32 nextNumber = 0;
				int offset2;
				
				if ((UCHAR*) node.node == (UCHAR*) page + page->length)
					offset2 = -1;
				else
					{
					node.expandKey(&nextKey);
					offset2 = IndexPage::computePrefix(&key, &nextKey);
					int deltaOffset = offset2 - node.offset;
					
					if (node.length >= 128 && (node.length - deltaOffset) < 128)
						--delta;

					if (node.offset < 128 && (node.offset + deltaOffset) >= 128)
						++delta;
						
					delta -= deltaOffset;
					nextNumber = node.getNumber();
					}

				// If node doesn't fit, punt and let someone else do it
				
				if (page->length + delta > dbb->pageSize)
					{
					bdb->release(REL_HISTORY);
					bdb = NULL;
					++punts;
					addIndexEntry(dbb, indexId, &key, recordNumber, transId);
					
					if ( (recordNumber = logRecord->nextKey(&key)) == -1)
						//return;
						goto exit;
					
					break;
					}
				
				// Add insert node into page
				
				if (offset2 >= 0)
					{
					UCHAR *tail = (UCHAR*) node.nextNode;
					int tailLength = (int) ((UCHAR*) page + page->length - tail);
					ASSERT (tailLength >= 0);
					
					if (tailLength > 0)
						memmove (tail + delta, tail, tailLength);
					}

				// Insert new node

				++insertions;
				IndexNode newNode;
				newNode.insert(node.node, offset1, length1, key.key, recordNumber);

				// If necessary, rebuild next node

				if (offset2 >= 0)
					newNode.insert(newNode.nextNode, offset2, nextKey.keyLength - offset2, nextKey.key, nextNumber);

				page->length += delta;
				//page->validate(NULL);

				if (dbb->debug & (DEBUG_PAGES | DEBUG_INDEX_MERGE))
					page->printPage(bdb, false);
				}
				
			priorKey.setKey(&key);
			
			// Get next key
			
			if ( (recordNumber = logRecord->nextKey(&key)) == -1)
				break;

			// If the key is out of order, somebody screwed up.  Punt out of here
			
			if (key.compare(&priorKey) > 0)
				//ASSERT(false);
				Log::log("IndexRootPage::addIndexEntry: Unexpected out of order index");

			// Find the next insertion point, compute the next key, etc.
			
			bucketEnd = (Btn*) ((char*) page + page->length);
			node.parseNode(IndexPage::findInsertionPoint(0, &key, recordNumber, &priorKey, node.node, bucketEnd));
			nextKey.setKey(0, node.offset, priorKey.key);
			node.expandKey(&nextKey);
			number = node.getNumber();
			
			if (number != END_LEVEL && nextKey.compare(&key) > 0)
				break;
			}
		
		if (bdb)
			bdb->release(REL_HISTORY);
		}	

	exit:;
	
	//Log::debug("indexMerge: index %d, %d insertions, %d punts, %d duplicates, %d rollovers\n",  indexId, insertions, punts, duplicates, rollovers);
}

void IndexRootPage::redoCreateIndex(Dbb* dbb, int indexId)
{
	Bdb *bdb = Section::getSectionPage (dbb, INDEX_ROOT, indexId / dbb->pagesPerSection, Exclusive, NO_TRANSACTION);
	BDB_HISTORY(bdb);
	ASSERT(bdb);
	SectionPage *sections = (SectionPage*) bdb->buffer;
	int slot = indexId % dbb->pagesPerSection;

	if (!sections->pages [slot])
		{
		bdb->mark(NO_TRANSACTION);
		Bdb *pageBdb = createIndexRoot(dbb, NO_TRANSACTION);
		sections->pages [slot] = pageBdb->pageNumber;
		pageBdb->release(REL_HISTORY);
		}

	bdb->release(REL_HISTORY);
}

Bdb* IndexRootPage::createIndexRoot(Dbb* dbb, TransId transId)
{
	Bdb *bdb = IndexPage::createNewLevel(dbb, 0, INDEX_CURRENT_VERSION, transId);

	return bdb;
}

void IndexRootPage::analyzeIndex(Dbb* dbb, int indexId, IndexAnalysis *indexAnalysis)
{
	Bdb *bdb = findRoot(dbb, indexId, 0, Shared, NO_TRANSACTION);
	BDB_HISTORY(bdb);

	if (!bdb)
		return;

	IndexPage *page = (IndexPage*) bdb->buffer;
	indexAnalysis->levels = page->level + 1;
	int32 nextLevel;
	bool first = true;
	
	for (;;)
		{
		if (first)
			{
			IndexNode node(page);
			nextLevel = node.getNumber();
			}

		if (page->level == 0)
			{
			++indexAnalysis->leafPages;
			indexAnalysis->leafSpaceUsed += page->length;
			}
		else
			++indexAnalysis->upperLevelPages;
	
		if (page->nextPage)
			{
			bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Shared);
			BDB_HISTORY(bdb);
			}
		else
			{
			bdb->release(REL_HISTORY);

			if (page->level == 0)
				break;
				
			bdb = dbb->fetchPage(nextLevel, PAGE_btree, Shared);
			BDB_HISTORY(bdb);
			first = true;
			}
		
		page = (IndexPage*) bdb->buffer;
		}
}

int32 IndexRootPage::getIndexRoot(Dbb* dbb, int indexId)
{
	Bdb *bdb = Section::getSectionPage (dbb, INDEX_ROOT, indexId / dbb->pagesPerSection, Shared, NO_TRANSACTION);
	BDB_HISTORY(bdb);
	if (!bdb)
		return 0;

	SectionPage *sections = (SectionPage*) bdb->buffer;
	int32 pageNumber = sections->pages [indexId % dbb->pagesPerSection];
	bdb->release(REL_HISTORY);
	
	return pageNumber;
}
