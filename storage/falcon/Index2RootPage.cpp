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

// Index2RootPage.cpp: implementation of the Index2RootPage class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "Index2RootPage.h"
#include "Index2Page.h"
#include "Dbb.h"
#include "BDB.h"
#include "Section.h"
#include "SectionPage.h"
#include "Bitmap.h"
#include "Index2Node.h"
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

void Index2RootPage::create(Dbb * dbb, TransId transId)
{
	Bdb *bdb = dbb->fakePage (INDEX_ROOT, PAGE_sections, transId);
	BDB_HISTORY(bdb);
	Index2RootPage *sections = (Index2RootPage*) bdb->buffer;
	sections->section = -1;
	sections->level = 0;
	sections->sequence = 0;
	bdb->release(REL_HISTORY);
}

int32 Index2RootPage::createIndex(Dbb * dbb, TransId transId)
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
				sections->pages [slot] = indexBdb->pageNumber;
				Index2Page::logIndexPage(indexBdb, transId);
				indexBdb->release(REL_HISTORY);
				sectionsBdb->release(REL_HISTORY);
				dbb->nextIndex = (skipped) ? skipped : id + 1;

				return id;
				}
			}
		}
}

bool Index2RootPage::addIndexEntry(Dbb * dbb, int32 indexId, IndexKey *key, int32 recordNumber, TransId transId)
{
	IndexKey searchKey(key);
	
	if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_ADDED))
		{
		LogLock logLock;
		Btn::printKey ("insertion key: ", key, 0, false);
		}
		
	for (int n = 0; n < 3; ++n)
		{
		/* Find insert page and position on page */

		Bdb *bdb = findInsertionLeaf(dbb, indexId, &searchKey, recordNumber, transId);

		if (!bdb)
			return false;

		Index2Page *page;

		for (;;)
			{
			IndexKey priorKey;
			page = (Index2Page*) bdb->buffer;
			Btn *node = page->findNode(key, &priorKey);
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

			bdb = dbb->handoffPage(bdb, page->nextPage, PAGE_btree, Exclusive);
			BDB_HISTORY(bdb);
			bdb->mark(transId);
			page = (Index2Page*) bdb->buffer;
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
			int prevDebug = dbb->debug;
			dbb->debug = DEBUG_PAGES | DEBUG_KEYS;
			Btn::printKey ("Key: ", key, 0, false);
			Btn::printKey ("SearchKey: ", &searchKey, 0, false);
			Bdb *bdb = findInsertionLeaf (dbb, indexId, &searchKey, recordNumber, transId);
			Index2Page *page = (Index2Page*) bdb->buffer;

			while (page->nextPage)
				{
				bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Exclusive);
				BDB_HISTORY(bdb);
				page = (Index2Page*) bdb->buffer;
				page->printPage(bdb, false);
				}

			bdb->release(REL_HISTORY);
			dbb->debug = prevDebug;
			}
#endif
		}


	ASSERT (false);
	FATAL ("index split failed");

	return false;
}

Bdb* Index2RootPage::findLeaf(Dbb *dbb, int32 indexId, int32 rootPage, IndexKey *indexKey, LockType lockType, TransId transId)
{
	Bdb *bdb = findRoot(dbb, indexId, rootPage, lockType, transId);
	BDB_HISTORY(bdb);

	if (!bdb)
		return NULL;

	Index2Page *page = (Index2Page*) bdb->buffer;

	if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF))
		page->printPage (bdb, false);

	while (page->level > 0)
		{
		Index2Node node (page->findNodeInBranch (indexKey));
		int32 pageNumber = node.getNumber();

		if (pageNumber == END_BUCKET)
			pageNumber = page->nextPage;

		if (pageNumber == 0)
			{
			page->printPage (bdb, false);
			bdb->release(REL_HISTORY);
			throw SQLError (DATABASE_CORRUPTION, "index %d damaged", indexId);
			}

		bdb = dbb->handoffPage(bdb, pageNumber, PAGE_btree,
								(page->level > 1) ? Shared : lockType);
		BDB_HISTORY(bdb);
		page = (Index2Page*) bdb->buffer;

		if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF))
			page->printPage (bdb, false);
		}

	return bdb;
}
Bdb* Index2RootPage::findInsertionLeaf(Dbb *dbb, int32 indexId, IndexKey *indexKey, int32 recordNumber, TransId transId)
{
	Bdb *bdb = findRoot(dbb, indexId, 0, Shared, transId);
	BDB_HISTORY(bdb);

	if (!bdb)
		return NULL;

	Index2Page *page = (Index2Page*) bdb->buffer;
	
	if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF))
		page->printPage (bdb, false);

	if (page->level == 0)
		{
		bdb->release(REL_HISTORY);
		
		if ( !(bdb = findRoot(dbb, indexId, 0, Exclusive, transId)) )
			return NULL;

		BDB_HISTORY(bdb);
		page = (Index2Page*) bdb->buffer;
		
		if (page->level == 0)
			return bdb;
		}

	while (page->level > 0)
		{
		Index2Node node (page->findNodeInBranch(indexKey));
		int32 pageNumber = node.getNumber();

		if (pageNumber == END_BUCKET)
			pageNumber = page->nextPage;

		if (pageNumber == 0)
			{
			page->printPage (bdb, false);
			bdb->release(REL_HISTORY);
			throw SQLError (DATABASE_CORRUPTION, "index %d damaged", indexId);
			}

		bdb = dbb->handoffPage(bdb, pageNumber, PAGE_btree, 
								(page->level > 1) ? Shared : Exclusive);
		BDB_HISTORY(bdb);
		page = (Index2Page*) bdb->buffer;

		if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEAF))
			page->printPage (bdb, false);
		}

	return bdb;
}

Bdb* Index2RootPage::findRoot(Dbb *dbb, int32 indexId, int32 rootPage, LockType lockType, TransId transId)
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

void Index2RootPage::scanIndex (Dbb *dbb,
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

	Index2Page *page = (Index2Page*) bdb->buffer;
	Index2Node node (page->findNode (lowKey, &key));
	Btn *end = (Btn*) ((UCHAR*) page + page->length);
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
		page = (Index2Page*) bdb->buffer;
		node.parseNode (page->findNode (lowKey, &key));
		end = (Btn*) ((UCHAR*) page + page->length);
		}

	if (highKey && node.node < end)
		{
		ASSERT (node.offset + node.length < sizeof(lowKey->key));
		node.expandKey (&key);
		offset = page->computePrefix (node.offset + node.length, key.key, highKey->keyLength, highKey->key);
		}

	/* Scan index setting bits */

	for (;;)
		{
		for (; node.node < end; node.getNext())
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
		page = (Index2Page*) bdb->buffer;
		node.parseNode (page->nodes);
		end = (Btn*) ((UCHAR*) page + page->length);
		offset = 0;
		
		if (dbb->debug & (DEBUG_PAGES | DEBUG_SCAN_INDEX))
			page->printPage (bdb, false);
		}
}

bool Index2RootPage::splitIndexPage(Dbb * dbb, int32 indexId, Bdb * bdb, TransId transId,
								   AddNodeResult addResult, IndexKey *indexKey, int recordNumber)
{
	// Start by splitting page (allocating new page)

	Index2Page *page = (Index2Page*) bdb->buffer;
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

	Index2Page *splitPage = (Index2Page*) splitBdb->buffer;
	Index2Node splitNode(splitPage);
	splitKey.setKey(splitNode.keyLength(), splitNode.key);
	int32 splitRecordNumber = 0;
	
	// If there isn't a parent page, we need to create a new level.  Do so.

	if (!page->parentPage)
		{
		// Allocate and copy a new left-most index page
		
		Bdb *leftBdb = dbb->allocPage(PAGE_btree, transId);
		BDB_HISTORY(leftBdb);
		Index2Page *leftPage = (Index2Page*) leftBdb->buffer;
		memcpy(leftPage, page, page->length);
		splitPage->priorPage = leftBdb->pageNumber;
		
		// Reinitialize the parent page
		
		/***
		Bdb *parentBdb = Index2Page::createNewLevel (dbb, 
				splitPage->level + 1, page->version, bdb->pageNumber, splitBdb->pageNumber,
				&splitKey, transId);
		***/
		
		page->level = splitPage->level + 1;
		page->version = splitPage->version;
		IndexKey dummy(indexKey->index);
		dummy.keyLength = 0;
		page->length = OFFSET (Index2Page*, nodes);
		page->addNode (dbb, &dummy, END_LEVEL);
		page->addNode (dbb, &dummy, leftBdb->pageNumber);
		page->addNode (dbb, &splitKey, splitBdb->pageNumber);
		
		
		leftPage->parentPage = bdb->pageNumber;
		splitPage->parentPage = bdb->pageNumber;
		Index2Page::logIndexPage(bdb, transId);
		Index2Page::logIndexPage(splitBdb, transId);
		Index2Page::logIndexPage(leftBdb, transId);

		/***
		bdb->release();
		dbb->setPrecedence(bdb, splitBdb->pageNumber);
		dbb->setPrecedence(parentBdb, bdb->pageNumber);
		splitBdb->release();
		parentBdb->release();
		setIndexRoot(dbb, indexId, pageNumber, transId);
		***/
		
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
	Index2Page::logIndexPage(bdb, transId);
	bdb->release(REL_HISTORY);
	int splitPageNumber = splitBdb->pageNumber;
	splitBdb->release(REL_HISTORY);
		
	for (;;)
		{
		bdb = findRoot (dbb, indexId, 0, Exclusive, transId);
		BDB_HISTORY(bdb);
		page = (Index2Page*) bdb->buffer;

		bdb = Index2Page::findLevel (dbb, bdb, level, &splitKey, splitRecordNumber);
		BDB_HISTORY(bdb);
		bdb->mark(transId);
		page = (Index2Page*) bdb->buffer;

		// If we can add the node, we're happy

		AddNodeResult result = page->addNode (dbb, &splitKey, splitPageNumber);

		if (result == NodeAdded || result == Duplicate)
			{
			dbb->setPrecedence(bdb, splitPageNumber);
			splitBdb = dbb->fetchPage (splitPageNumber, PAGE_btree, Exclusive);
			BDB_HISTORY(splitBdb);
			splitBdb->mark (transId);
			splitPage = (Index2Page*) splitBdb->buffer;
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

bool Index2RootPage::deleteIndexEntry(Dbb * dbb, int32 indexId, IndexKey *key, int32 recordNumber, TransId transId)
{
	Index2Page *page;
	IndexKey searchKey(key);

	if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_DELETED))
		{
		Btn::printKey ("deletion key: ", key, 0, false);
		}
		
	// Try to delete node.  If necessary, chase to next page.

	for (Bdb *bdb = findInsertionLeaf (dbb, indexId, &searchKey, recordNumber, transId); bdb;
		 bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Exclusive))
		{
		BDB_HISTORY(bdb);
		page = (Index2Page*) bdb->buffer;
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

void Index2RootPage::deleteIndex(Dbb *dbb, int32 indexId, TransId transId)
{
	Bdb *bdb = Section::getSectionPage (dbb, INDEX_ROOT, indexId / dbb->pagesPerSection, Exclusive, transId);
	BDB_HISTORY(bdb);

	if (!bdb)
		return;

	bdb->mark (transId);
	SectionPage *sections = (SectionPage*) bdb->buffer;
	int32 nextLevel = sections->pages [indexId % dbb->pagesPerSection];
	sections->pages [indexId % dbb->pagesPerSection] = 0;
	dbb->nextIndex = MIN(dbb->nextIndex, indexId);
	bdb->release(REL_HISTORY);

	while (nextLevel)
		{
		bdb = dbb->fetchPage (nextLevel, PAGE_any, Exclusive);
		BDB_HISTORY(bdb);
		Index2Page *page = (Index2Page*) bdb->buffer;
		
		if (page->pageType != PAGE_btree)
			{
			Log::logBreak ("Drop index %d: bad level page %d\n", indexId, bdb->pageNumber);
			bdb->release(REL_HISTORY);
			
			break;
			}
			
		Index2Node node (page->nodes);
		nextLevel = (page->level) ? node.getNumber() : 0;
		
		for (;;)
			{
			int32 nextPage = page->nextPage;
			dbb->freePage (bdb, transId);
			
			if (!nextPage)
				break;
				
			bdb = dbb->fetchPage (nextPage, PAGE_any, Exclusive);
			BDB_HISTORY(bdb);
			page = (Index2Page*) bdb->buffer;
			
			if (page->pageType != PAGE_btree)
				{
				Log::logBreak ("Drop index %d: bad index page %d\n", indexId, bdb->pageNumber);
				bdb->release(REL_HISTORY);
				break;
				}
			}
		}
}


void Index2RootPage::debugBucket(Dbb *dbb, int indexId, int recordNumber, TransId transactionId)
{
	Bdb *bdb = findRoot (dbb, indexId, 0, Exclusive, transactionId);
	BDB_HISTORY(bdb);
	Index2Page *page;
	Index2Node node;
	int debug = 1;

	// Find leaf

	for (;;)
		{
		page = (Index2Page*) bdb->buffer;
		
		if (debug)
			page->printPage (bdb, false);
			
		node.parseNode (page->nodes);
		
		if (page->level == 0)
			break;
			
		bdb = dbb->handoffPage (bdb, node.getNumber(), PAGE_btree, Exclusive);
		BDB_HISTORY(bdb);
		}

	// Scan index

	Btn *end = (Btn*) ((UCHAR*) page + page->length);
	int pages = 0;
	int nodes = 0;

	/* If we didn't find it here, try the next page */

	for (;;)
		{
		++pages;
		
		for (; node.node < end; node.getNext())
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
		page = (Index2Page*) bdb->buffer;
		
		if (debug)
			page->printPage (bdb, false);
			
		node.parseNode (page->nodes);
		end = (Btn*) ((UCHAR*) page + page->length);
		}

	bdb->release(REL_HISTORY);
}

void Index2RootPage::redoIndexPage(Dbb* dbb, int32 pageNumber, int32 parentPage, int level, int32 priorPage, int32 nextPage, int length, const UCHAR *data)
{
	//Log::debug("redoIndexPage %d -> %d -> %d level %d, parent %d)\n", priorPage, pageNumber, nextPage, level, parentPage);
	Bdb *bdb = dbb->fakePage(pageNumber, PAGE_any, 0);
	BDB_HISTORY(bdb);
	Index2Page *indexPage = (Index2Page*) bdb->buffer;

	//  Try to read page.  If it looks OK, keep it.  Otherwise, rebuild it

	if (!dbb->trialRead(bdb) ||
		 indexPage->pageType != PAGE_btree ||
		 indexPage->parentPage != parentPage ||
		 indexPage->level == level)
		{
		memset(indexPage, 0, dbb->pageSize);
		indexPage->setType(PAGE_btree, bdb->pageNumber);
		//indexPage->pageType = PAGE_btree;
		}

	indexPage->level = level;
	indexPage->parentPage = parentPage;
	indexPage->nextPage = nextPage;
	indexPage->priorPage = priorPage;
	indexPage->length = length + (int32) OFFSET (Index2Page*, nodes);
	memcpy(indexPage->nodes, data, length);
	
	// If we have a parent page, propogate the first node upward

	if (parentPage)
		{
		Index2Node node(indexPage);
		int number = node.getNumber();

		if (number >= 0)
			{
			IndexKey indexKey(node.keyLength(), node.key);
			Bdb *parentBdb = dbb->fetchPage(parentPage, PAGE_btree, Exclusive);
			BDB_HISTORY(parentBdb);
			Index2Page *parent = (Index2Page*) parentBdb->buffer;
		
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
			indexPage = (Index2Page*) bdb->buffer;
			
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
			indexPage = (Index2Page*) bdb->buffer;
			
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

void Index2RootPage::setIndexRoot(Dbb* dbb, int indexId, int32 pageNumber, TransId transId)
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

void Index2RootPage::redoIndexDelete(Dbb* dbb, int indexId)
{
	setIndexRoot(dbb, indexId, 0, NO_TRANSACTION);
}

void Index2RootPage::indexMerge(Dbb *dbb, int indexId, SRLUpdateIndex *logRecord, TransId transId)
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
			
		if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_DELETED))
			{
			LogLock logLock;
			Btn::printKey ("insertion key: ", &key, 0, false);
			}
		
		Bdb *bdb = findInsertionLeaf(dbb, indexId, &searchKey, recordNumber, transId);
		ASSERT(bdb);
		Index2Page *page = (Index2Page*) bdb->buffer;
		Btn *bucketEnd = (Btn*) ((char*) page + page->length);
		IndexKey priorKey;
		Index2Node node(page->findInsertionPoint(&key, recordNumber, &priorKey));
		IndexKey nextKey;
		nextKey.setKey(0, node.offset, priorKey.key);
		node.expandKey(&nextKey);
		int number = node.getNumber();
		
		// If we need to go to the next page, do it now
		
		while (number == END_BUCKET && nextKey.compareValues(&key) > 0)
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
				
			nextKey.compareValues(&key);
			ASSERT (bdb->pageNumber != page->nextPage);
			bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_btree, Exclusive);
			BDB_HISTORY(bdb);
			page = (Index2Page*) bdb->buffer;
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
				int offset1 = Index2Page::computePrefix (&priorKey, &key);
				int length1 = key.keyLength - offset1;
				int delta = Index2Node::nodeLength(offset1, length1, recordNumber);
				int32 nextNumber = 0;
				int offset2 = 0;
				
				if ((UCHAR*) node.node == (UCHAR*) page + page->length)
					offset2 = -1;
				else
					{
					node.expandKey(&nextKey);
					offset2 = Index2Page::computePrefix(key.keyLength, key.key, node.offset + node.length, nextKey.key);
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
				Index2Node newNode;
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
				//break;
				ASSERT(false);
			
			// Find the next insertion point, compute the next key, etc.
			
			bucketEnd = (Btn*) ((char*) page + page->length);
			node.parseNode(Index2Page::findInsertionPoint(0, &key, recordNumber, &priorKey, node.node, bucketEnd));
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

void Index2RootPage::redoCreateIndex(Dbb* dbb, int indexId)
{
	Bdb *bdb = Section::getSectionPage (dbb, INDEX_ROOT, indexId / dbb->pagesPerSection, Exclusive, NO_TRANSACTION);
	ASSERT(bdb);
	BDB_HISTORY(bdb);
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

Bdb* Index2RootPage::createIndexRoot(Dbb* dbb, TransId transId)
{
	Bdb *bdb = dbb->allocPage (PAGE_btree, transId);
	BDB_HISTORY(bdb);
	Index2Page *btree = (Index2Page*) bdb->buffer;
	btree->level = 0;
	btree->version = INDEX_CURRENT_VERSION;
	btree->length = OFFSET (Index2Page*, nodes);
	IndexKey key;
	key.keyLength = 0;
	btree->addNode (dbb, &key, END_LEVEL);

	return bdb;
}


void Index2RootPage::analyzeIndex(Dbb* dbb, int indexId, IndexAnalysis *indexAnalysis)
{
	Bdb *bdb = findRoot(dbb, indexId, 0, Shared, NO_TRANSACTION);
	BDB_HISTORY(bdb);

	if (!bdb)
		return;

	Index2Page *page = (Index2Page*) bdb->buffer;
	indexAnalysis->levels = page->level + 1;
	int32 nextLevel;
	bool first = true;
	
	for (;;)
		{
		if (first)
			{
			Index2Node node(page);
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
		
		page = (Index2Page*) bdb->buffer;
		}
}

int32 Index2RootPage::getIndexRoot(Dbb* dbb, int indexId)
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
