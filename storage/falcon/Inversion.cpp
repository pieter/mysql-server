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

// Inversion.cpp: implementation of the Inversion class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include <string.h>
#include "Engine.h"
#include "Inversion.h"
#include "Dbb.h"
#include "InversionFilter.h"
#include "InversionWord.h"
#include "InversionPage.h"
#include "Index2Page.h"
#include "BDB.h"
#include "Hdr.h"
#include "ResultList.h"
#include "Validation.h"
#include "Sync.h"
#include "Log.h"
#include "IndexKey.h"
#include "Index2Node.h"
#include "RootPage.h"

#define STACK_SIZE	512

#define STUFF(number,ptr,shift)	if (n = number >> shift) *ptr++ = n | 0x80

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Inversion::Inversion(Dbb *db)
{
	dbb = db;
	size = 20000;
	sortBuffer = new UCHAR* [size];
	records = 0;
	lastRecord = (UCHAR*) (sortBuffer + size);
	wordsAdded = 0;
	wordsRemoved = 0;
	runs = 0;
	inserting = true;
	state = 0;
}

Inversion::~Inversion()
{
	delete [] sortBuffer;
}

int32 Inversion::addInversion(InversionFilter *filter, TransId transId, bool insertFlag)
{
	Sync sync (&syncObject, "Inversion::addInversion");
	sync.lock (Exclusive);

	if (inserting != insertFlag)
		{
		flush (transId);
		inserting = insertFlag;
		}

	filter->start();
	UCHAR key [MAX_INV_KEY];
	int32 count = 0;

	for (InversionWord *word; (word = filter->getWord());)
		if (word->wordLength < 50)
			{
			int keyLength = word->makeKey (key);
			//Log::debug ("Inversion:addInversion: %s\n", word);
			UCHAR *record = lastRecord - (keyLength + 1);
			if (record < (UCHAR*) (sortBuffer + records + 1))
				{
				flush (transId);
				record = lastRecord - (keyLength + 1);
				}
			lastRecord = record;
			sortBuffer [records++] = lastRecord;
			lastRecord [0] = keyLength;
			memcpy (lastRecord + 1, key, keyLength);
			++count;
			}

	return count;
}

void Inversion::createInversion(TransId transId)
{
	Bdb *bdb = dbb->allocPage (PAGE_inversion, transId);
	BDB_HISTORY(bdb);
	InversionPage *page = (InversionPage*) bdb->buffer;
	page->length = OFFSET (InversionPage*, nodes);
	updateInversionRoot (bdb->pageNumber, transId);
	bdb->release(REL_HISTORY);
}


void Inversion::addWord(int keyLength, UCHAR *key, TransId transId)
{
	/*
	 * Add try to add node to inversion leaf page.  If it doesn't fit, split the
	 * page, either create a new level or proprogate the split upwards, and try
	 * again.  Don't try too hard, howerver.
	 */

	++wordsAdded;
	IndexKey indexKey(keyLength, key);

	for (int iteration = 0;; ++iteration)
		{
		Bdb *bdb = findInversion (&indexKey, Exclusive);

		if (!bdb)
			return;

		InversionPage *page = (InversionPage*) bdb->buffer;

		if (page->pageType != PAGE_inversion)
			FATAL ("page %d wrong type, expected %d got %d\n",
					 bdb->pageNumber, PAGE_btree, page->pageType);

		bdb->mark(transId);

		try
			{
			if (page->addNode (dbb, &indexKey))
				{
				bdb->release(REL_HISTORY);
				return;
				}
			}
		catch (...)
			{
			bdb->release(REL_HISTORY);
			throw;
			}

		if (iteration > 0)
			ASSERT (false);

		/* Node didn't fit.  Split the page page and propogate the
		   split upward.  Sooner or laster we'll go back and re-try
		   the original insertion */

		IndexKey splitKey;
		Bdb *splitBdb = page->splitInversionPage (dbb, bdb, &splitKey, transId);

		if (page->parentPage)
			propogateSplit (1, &splitKey, splitBdb->pageNumber, transId);
		else
			{
			Bdb *parentBdb = Index2Page::createNewLevel (dbb, 1, INVERSION_VERSION_NUMBER, bdb->pageNumber, splitBdb->pageNumber,
														&splitKey, transId);
			Index2Page::logIndexPage(parentBdb, transId);
			int32 pageNumber = parentBdb->pageNumber;
			Index2Page *splitPage = (Index2Page*) splitBdb->buffer;
			page->parentPage = pageNumber;
			splitPage->parentPage = pageNumber;
			updateInversionRoot (pageNumber, transId);
			parentBdb->release(REL_HISTORY);
			}

		InversionPage::logPage(bdb);
		InversionPage::logPage(splitBdb);
		bdb->release(REL_HISTORY);
		splitBdb->release(REL_HISTORY);
		}

}

Bdb* Inversion::findRoot(LockType lockType)
{
	Bdb *bdb = dbb->fetchPage (HEADER_PAGE, PAGE_header, Shared);
	BDB_HISTORY(bdb);
	Hdr *header = (Hdr*) bdb->buffer;

	if (!header->inversion)
		return NULL;

	bdb = dbb->handoffPage (bdb, header->inversion, PAGE_any, lockType);
	BDB_HISTORY(bdb);

	return bdb;
}

void Inversion::updateInversionRoot(int32 pageNumber, TransId transId)
{
	Bdb *bdb = dbb->fetchPage (HEADER_PAGE, PAGE_header, Exclusive);
	BDB_HISTORY(bdb);
	bdb->mark(transId);
	Hdr *header = (Hdr*) bdb->buffer;
	header->inversion = pageNumber;
	bdb->release(REL_HISTORY);
}

void Inversion::propogateSplit(int level, IndexKey *indexKey, int32 pageNumber, TransId transId)
{
	for (int iteration = 0;; ++iteration)
		{
		Bdb *bdb = findRoot (Exclusive);
		BDB_HISTORY(bdb);
		Index2Page *page = (Index2Page*) bdb->buffer;

		bdb = Index2Page::findLevel (dbb, bdb, level, indexKey, 0);
		BDB_HISTORY(bdb);
		bdb->mark(transId);
		page = (Index2Page*) bdb->buffer;

		// If we can add the node, everything is simple, and we're done

		AddNodeResult result = page->addNode (dbb, indexKey, pageNumber);

		if (result == NodeAdded || result == Duplicate)
			{
			bdb->release(REL_HISTORY);
			return;
			}

		ASSERT (iteration == 0);

		// We've got to split this level.  Sigh

		IndexKey splitKey;
		Bdb *splitBdb = page->splitIndexPageMiddle (dbb, bdb, &splitKey, transId);

		if (page->parentPage)
			{
			bdb->release(REL_HISTORY);
			splitBdb->release(REL_HISTORY);
			propogateSplit (level + 1, &splitKey, splitBdb->pageNumber, transId);
			}
		else
			{
			Bdb *parentBdb = Index2Page::createNewLevel (
					dbb, level + 1, INVERSION_VERSION_NUMBER, bdb->pageNumber, splitBdb->pageNumber,
					&splitKey, transId);
			Index2Page::logIndexPage(parentBdb, transId);
			int32 pageNumber = parentBdb->pageNumber;
			page->parentPage = pageNumber;
			Index2Page *splitPage = (Index2Page*) splitBdb->buffer;
			splitPage->parentPage = pageNumber;

			Index2Page::logIndexPage(bdb, transId);
			bdb->release(REL_HISTORY);
			Index2Page::logIndexPage(splitBdb, transId);
			splitBdb->release(REL_HISTORY);
			updateInversionRoot (pageNumber, transId);
			Index2Page::logIndexPage(parentBdb, transId);
			parentBdb->release(REL_HISTORY);
			}
		}
}

Bdb* Inversion::findInversion(IndexKey *indexKey, LockType lockType)
{
	Bdb *bdb = findRoot (Shared);
	BDB_HISTORY(bdb);

	if (!bdb)
		return NULL;
	
	if (bdb->buffer->pageType == PAGE_btree)
		{
		Index2Page *page = (Index2Page*) bdb->buffer;

		if (dbb->debug & (DEBUG_PAGES | DEBUG_INVERSION))
			page->printPage (bdb, true);

		int32 pageNumber;

		for (;;)
			{
			Index2Node node(page->findNodeInBranch(indexKey));
			pageNumber = node.getNumber();
			
			if (page->level == 1)
				break;

			bdb = dbb->handoffPage (bdb, pageNumber, PAGE_btree, Shared);
			BDB_HISTORY(bdb);
			page = (Index2Page*) bdb->buffer;

			if (dbb->debug & (DEBUG_PAGES | DEBUG_INVERSION))
				page->printPage (bdb, true);
			}

		bdb = dbb->handoffPage (bdb, pageNumber, PAGE_inversion, lockType);
		BDB_HISTORY(bdb);

		return bdb;
		}

	if (lockType != Shared)
		{
		bdb->release(REL_HISTORY);
		bdb = findRoot (lockType);
		BDB_HISTORY(bdb);
		ASSERT (bdb->buffer->pageType == PAGE_inversion);
		}

	return bdb;
}

void Inversion::deleteInversion(TransId transId)
{
	Bdb *bdb = findRoot(Exclusive);
	BDB_HISTORY(bdb);

	if (!bdb)
		return;

	updateInversionRoot (0, transId);

	// Delete index levels

	while (bdb->buffer->pageType == PAGE_btree)
		{
		Index2Page *indexPage = (Index2Page*) bdb->buffer;
		Index2Node node(indexPage);
		//int32 pageNumber = indexPage->nodes->getPageNumber();
		int32 pageNumber = node.getNumber();
		int32 next = indexPage->nextPage;
		dbb->freePage (bdb, transId);
		
		while (next)
			{
			bdb = dbb->fetchPage(next, PAGE_btree, Exclusive);
			BDB_HISTORY(bdb);
			indexPage = (Index2Page*) bdb->buffer;
			next = indexPage->nextPage;
			dbb->freePage(bdb, transId);
			}
			
		bdb = dbb->fetchPage (pageNumber, PAGE_any, Exclusive);
		BDB_HISTORY(bdb);
		}

	// Delete leaf level

	InversionPage *page = (InversionPage*) bdb->buffer;
	ASSERT (page->pageType == PAGE_inversion);
	int32 next = page->nextPage;
	dbb->freePage (bdb, transId);

	while (next)
		{
		bdb = dbb->fetchPage (next, PAGE_inversion, Exclusive);
		BDB_HISTORY(bdb);
		page = (InversionPage*) bdb->buffer;
		next = page->nextPage;
		dbb->freePage (bdb, transId);
		}
}

void Inversion::summary()
{
	int32 pages = 0;
	int32 words = 0;
	int32 uniqueWords = 0;
	double fill = 0;
	int32 keyLength = 0;
	int32 compressedKeyLength = 0;

	IndexKey indexKey;
	indexKey.keyLength = 0;
	indexKey.key [0] = 0;
	UCHAR *key = indexKey.key;
	InversionPage *page;
	Bdb *bdb;

	for (bdb = findInversion (&indexKey, Shared);; 
		 bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_inversion, Shared))
		{
		BDB_HISTORY(bdb);
		++pages;
		page = (InversionPage*) bdb->buffer;
		fill += (double) (page->length * 100) / dbb->pageSize;
		Inv *end = (Inv*) ((char*) page + page->length);
		
		for (Inv *node = page->nodes; node < end; node = NEXT_INV (node))
			{
			++words;
			keyLength += node->offset + node->length;
			compressedKeyLength += node->length;
			
			for (int n = 0;; ++n)
				if (n < node->offset)
					{
					if (!key [n])
						break;
					}
				else 
					{
					if (key [n] != node->key [n - node->offset])
						{
						memcpy (key + node->offset, node->key, node->length);
						Log::debug ("%s\n", key);						
						++uniqueWords;
						break;
						}
						
					if (!key [n])
						break;
					}
					
			memcpy (key + node->offset, node->key, node->length);
			}

		getchar();
		
		if (!page->nextPage)
			break;
		}

	bdb->release(REL_HISTORY);
	FILE *file = fopen ("indexSummary.txt", "w");

	fprintf (file, "Pages:\t\t%d\n", pages);
	fprintf (file, "Words:\t\t%d\n", words);
	fprintf (file, " unique:\t%d\n", uniqueWords);
	fprintf (file, "Fill:\t\t%f\n", fill / pages);
	fprintf (file, "Avg key:\t%f\n", (double) keyLength / words);
	fprintf (file, " comprsd:\t%f\n", (double) compressedKeyLength / words);
	fclose (file);
}

void Inversion::sort (UCHAR **records, int size)
{
	//int iteration = 0;
	int i, j, r, stack = 0;
	int low [STACK_SIZE];
	int high [STACK_SIZE];
	UCHAR *temp;

	if (size > 2)
		{
		low [stack] = 0;
		high [stack++] = size - 1;
		}

	while (stack > 0)
		{
		UCHAR *key;
		/***
		if (++iteration > 10)
			{
			Debug.print ("punting");
			return;
			}
		***/
		r = low [--stack];
		j = high [stack];
		//Debug.print ("sifting " + r + " thru " + j + ": " + range (r, j));
		key = records [r];
		//Debug.print (" key", key);
		int limit = j;
		i = r + 1;
		for (;;)
			{
			while (compare (records [i], key) <= 0 && i < limit)
				++i;
			while (compare (records [j], key) >= 0 && j > r)
				--j;
			//Debug.print ("  i " + i, records [i]);
			//Debug.print ("  j " + j, records [j]);
			if (i >= j)
				break;
			temp = records [i];
			records [i] = records [j];
			records [j] = temp;
			}
		i = high [stack];
		records [r] = records [j];
		records [j] = key;
		//Debug.print (" midpoint " + j + ": " +  range (r, i));
		if ((j - 1) - r >= 2)
			{
			low [stack] = r;
			ASSERT (stack < STACK_SIZE);
			high [stack++] = j - 1;
			}
		if (i - (j + 1) >= 2)
			{
			low [stack] = j + 1;
			ASSERT (stack < STACK_SIZE);
			high [stack++] = i;
			}
		}

	for (int n = 1; n < size; ++n)
		if (compare (records [n - 1], records [n]) > 0)
			{
			//Debug.print ("Flipping");
			temp = records [n - 1];
			records [n - 1] = records [n];
			records [n] = temp;
			}
}

int Inversion::compare(UCHAR * key1, UCHAR * key2)
{
	int l1 = *key1++;
	int l2 = *key2++;
    UCHAR *end;

	for (end = key1 + MIN (l1, l2); key1 < end;)
		{
		int n = *key1++ - *key2++;
		if (n)
			return n;
		}

	if (l1 == l2)
		return 0;

	return (key1 == end) ? -1 : 1;
}

void Inversion::flush(TransId transId)
{
	if (!records)
		return;
		
	Sync sync (&syncObject, "Inversion::flush");
	sync.lock (Exclusive);

	if (!records)
		return;

	//Log::debug ("Inversion::flush\n");
	sort (sortBuffer, records);

	for (int n = 0; n < records; ++n)
		{
		UCHAR *record = sortBuffer [n];

		if (inserting)
			addWord (record [0], record + 1, transId);
		else
			removeWord (record [0], record + 1, transId);
		}

	records = 0;
	lastRecord = (UCHAR*) (sortBuffer + size);
	++runs;
}

void Inversion::validate(Validation *validation)
{
	Bdb *bdb = findRoot (Shared);
	BDB_HISTORY(bdb);
	Bitmap children;

	if (bdb->buffer->pageType == PAGE_inversion)
		{
		validation->inUse (bdb, "inversion root");
		InversionPage *page = (InversionPage*) bdb->buffer;
		page->validate (dbb, validation, &children);
		}
	else if (validation->isPageType (bdb, PAGE_btree, "Index2Page"))
		{
		validation->inUse (bdb, "inversion index root");
		Index2Page *page = (Index2Page*) bdb->buffer;
		page->validate (dbb, validation, &children, bdb->pageNumber);
		}

	bdb->release(REL_HISTORY);
}

void Inversion::removeWord(int keyLength, UCHAR *key, TransId transId)
{
	++wordsRemoved;
	IndexKey indexKey(keyLength, key);
	Bdb *bdb = findInversion (&indexKey, Exclusive);

	if (!bdb)
		return;

	try
		{
		InversionPage *page = (InversionPage*) bdb->buffer;

		if (page->pageType != PAGE_inversion)
			FATAL ("page %d wrong type, expected %d got %d\n",
					 bdb->pageNumber, PAGE_btree, page->pageType);

		bdb->mark(transId);
		//page->printPage (bdb);
		page->removeNode (dbb, keyLength, key);
		//page->printPage (bdb);
		}
	catch (...)
		{
		bdb->release(REL_HISTORY);
		throw;
		}

	bdb->release(REL_HISTORY);
}
