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

// IndexPage.cpp: implementation of the IndexPage class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <string.h>
#include <stdio.h>
#include "Engine.h"
#include "IndexPage.h"
#include "Dbb.h"
#include "BDB.h"
#include "Validation.h"
#include "InversionPage.h"
#include "IndexNode.h"
#include "Log.h"
#include "LogLock.h"
#include "Debug.h"
#include "IndexKey.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "SQLError.h"
#include "Index.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

AddNodeResult IndexPage::addNode(Dbb *dbb, IndexKey *indexKey, int32 recordNumber)
{
	if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_ADDED | DEBUG_PAGE_LEVEL))
		{
		Log::debug("\n***** addNode(%d) - ", recordNumber);
		Btn::printKey ("New key", indexKey, 0, false);
		}
		
	if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_ADDED))
		printPage (this, 0, false);

	// Find the insertion point.  In the process, compute the prior key

	//validate(NULL);
	UCHAR *key = indexKey->key;
	IndexKey priorKey;

	IndexNode node(findInsertionPoint(indexKey, recordNumber, &priorKey));

	if (node.getNumber() == recordNumber && 
		 indexKey->keyLength == node.offset + node.length &&
		 memcmp (priorKey.key, key, node.offset) == 0 &&
		 memcmp (node.key, key + node.offset, node.length) == 0)
		return Duplicate;

	// We got the prior key value.  Get key value for insertion point

	IndexKey nextKey (priorKey);

	// Compute delta size of new node

	int offset1 = computePrefix (&priorKey, indexKey);
	int length1 = indexKey->keyLength - offset1;
	int delta = IndexNode::nodeLength(offset1, length1, recordNumber);
	int32 nextNumber;
	int offset2;
	
	if ((UCHAR*) node.node == (UCHAR*) this + length)
		{
		offset2 = -1;
		nextNumber = 0;
		}
	else
		{
		node.expandKey (&nextKey);
		offset2 = computePrefix(indexKey, &nextKey);
		int deltaOffset = offset2 - node.offset;
		
		if (node.length >= 128 && (node.length - deltaOffset) < 128)
			--delta;

		if (node.offset < 128 && (node.offset + deltaOffset) >= 128)
			++delta;
			
		delta -= deltaOffset;
		nextNumber = node.getNumber();
		}

	// If new node doesn't fit on page, split the page now

	if (length + delta > dbb->pageSize)
		{
		if ((char*) node.nextNode < (char*) this + length)
			return SplitMiddle;

		if (node.getNumber() == END_LEVEL)
			return SplitEnd;

		if (offset2 == -1 || nextKey.compare(indexKey) >= 0)
			return NextPage;

		return SplitEnd;
		}

	/***
	Log::debug ("insert index %d, record %d, page offset %d\n",
			index, recordNumber, (CHAR*) node - (CHAR*) page);
	***/

	// Slide tail of bucket forward to make room

	if (offset2 >= 0)
		{
		UCHAR *tail = (UCHAR*) node.nextNode;
		int tailLength = (int) ((UCHAR*) this + length - tail);
		ASSERT (tailLength >= 0);
		
		if (tailLength > 0)
			memmove (tail + delta, tail, tailLength);
		}

	// Insert new node

	IndexNode newNode;
	newNode.insert (node.node, offset1, length1, key, recordNumber);

	// If necessary, rebuild next node

	if (offset2 >= 0)
		newNode.insert (newNode.nextNode, offset2, nextKey.keyLength - offset2, nextKey.key, nextNumber);

	length += delta;
	//validate(NULL);

	if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_ADDED))
		printPage (this, 0, false);

	return NodeAdded;
}

/**
@brief		Find the first duplicate of an indexKey in a leaf node.
**/
Btn* IndexPage::findNodeInLeaf(IndexKey *indexKey, IndexKey *foundKey)
{
	ASSERT(level == 0);
	ASSERT(indexKey->recordNumber == 0);

	uint offset = 0;
	uint priorLength = 0;
	UCHAR *key = indexKey->key;
	UCHAR *keyEnd = key + indexKey->keyLength;
	Btn *bucketEnd = getEnd();

	if (indexKey->keyLength == 0)
		{
		if (foundKey)
			foundKey->keyLength = 0;

		return nodes;
		}

	IndexNode node (this);

	for (;; node.getNext(bucketEnd))
		{
		if (node.node >= bucketEnd)
			{
			if (node.node != bucketEnd)
				FATAL ("Index findNodeInLeaf: trashed btree page");
				
			if (foundKey)
				foundKey->keyLength = priorLength;
				
			return bucketEnd;
			}
		
		int32 number = node.getNumber();
		
		if (number == END_LEVEL)
			break;
			
		if (node.offset < offset)
			break;
			
		if (node.offset > offset || node.length == 0)
			continue;
			
		UCHAR *p = key + node.offset;
		UCHAR *q = node.key;
		UCHAR *nodeEnd = q + node.length;
		
		for (;;)
			{
			if (p == keyEnd)
				goto exit;
				
			if (q == nodeEnd)
				{
				if (indexKey->checkTail(p) < 0)
					goto exit;
				break;
				}
				
			if (*p > *q)
				break;
				
			if (*p++ < *q++)
				goto exit;
			}
			
		offset = (int) (p - key);
		
		if (foundKey)
			{
			node.expandKey (foundKey);
			priorLength = node.offset + node.length;
			}
		}

	exit:

	if (foundKey)
		foundKey->keyLength = priorLength;

	return node.node;
}

int IndexPage::computePrefix(IndexKey *key1, IndexKey *key2)
{
	int len1 = key1->keyLength;
	if (key1->recordNumber)
		len1 -= key1->getAppendedRecordNumberLength();

	int len2 = key2->keyLength;
	if (key2->recordNumber)
		len2 -= key2->getAppendedRecordNumberLength();

	int max = MIN(len1, len2);
	int n;
	UCHAR *v1 = key1->key;
	UCHAR *v2 = key2->key;

	for (n = 0; n < max; ++n)
		if (*v1++ != *v2++)
			return n;

	return n;
}

Bdb* IndexPage::splitIndexPageMiddle(Dbb * dbb, Bdb *bdb, IndexKey *splitKey, TransId transId)
{
	if (dbb->debug & (DEBUG_PAGES | DEBUG_SPLIT_PAGE))
		printPage (this, 0, false);

	// Fudge page end to handle the all-duplicates problem

	UCHAR *key = splitKey->key;
	Btn *pageEnd = (Btn*) ((char*) this + dbb->pageSize - sizeof(Btn) - 256);
	Btn *midpoint = (Btn*) ((UCHAR*) nodes + 
					  (length - OFFSET (IndexPage*, nodes)) / 2);

	/* Find midpoint.  Don't worry about duplicates.
	   Use the node that crosses the midpoint of what is 
	   currently on the page.*/

	IndexNode node (this);
	IndexNode prevNode = node;
	Btn *chain = node.node;
	Btn *bucketEnd = getEnd();
	
	for (; node.node < pageEnd; node.getNext(bucketEnd))
		{
		//int l = 
		node.expandKey(key);

		if (node.offset || node.length)
			{
			if (node.nextNode > midpoint)
				break;

			chain = node.node;
			}
		}

	// The split node should never be the last node on the page
	
	if ((UCHAR*) node.nextNode > (UCHAR*) this + length)
		node = prevNode;

	int tailLength = (int) (((UCHAR*) this + length) - (UCHAR*) node.nextNode);

	if (tailLength < 0)
		{
		node.parseNode(nodes);
		tailLength = (int) (((UCHAR*) this + length) - (UCHAR*) node.nextNode);
		}

	// If we didn't find a break, use the last one.  This may be the first node

	int kLength = splitKey->keyLength = node.offset + node.length;

	// Allocate and format new page.  Link forward and backward to siblings.

	Bdb *splitBdb = splitPage(dbb, bdb, transId);
	IndexPage *split = (IndexPage*) splitBdb->buffer;

	/* Copy midpoint node to new page.  Compute length, then
	   copy tail of page to new page */

	int32 recordNumber = node.getNumber();
	IndexNode newNode;
	newNode.insert(split->nodes, 0, kLength, key, recordNumber);

	memcpy(newNode.nextNode, node.nextNode, tailLength);

	/* Now we get tricky.  We need to add a "end bucket" marker with the value
	 * of the next bucket.  So copy a little more (it's already compressed)
	 * and zap the node "number" to "END_BUCKET" */

	node.insert(node.node, node.offset, node.length, key, END_BUCKET);
	length = (int) ((UCHAR*) node.nextNode - (UCHAR*) this);

	//validate(split);
	split->length = (int) (((UCHAR*) newNode.nextNode + tailLength) - (UCHAR*) split);
	//split->validate(this);

	if (level == 0)
		splitKey->appendRecordNumber(recordNumber);
		
	if (dbb->debug & (DEBUG_PAGES | DEBUG_SPLIT_PAGE))
		{
		printPage (this, 0, false);
		printPage (splitBdb, false);
		}

	logIndexPage(splitBdb, transId);

	return splitBdb;
}

Btn* IndexPage::findNodeInBranch(IndexKey *searchKey, int32 searchRecordNumber)
{
	ASSERT(level > 0);

	uint matchedOffset = 0;
	UCHAR *keyEnd = searchKey->keyEnd();
	if (searchKey->recordNumber)
		keyEnd -= searchKey->getAppendedRecordNumberLength();

	Btn *bucketEnd = (Btn*) ((UCHAR*) this + length);
	Btn *prior = nodes;
	UCHAR *key = searchKey->key;

	for (IndexNode node (this); node.node < bucketEnd; prior = node.node, node.getNext(bucketEnd))
		{
		if (node.offset < matchedOffset)
			return prior;

		if (node.getNumber() == END_LEVEL)
			return prior;

		if (node.offset > matchedOffset || node.length == 0)
			continue;

		UCHAR *q = node.key;
		UCHAR *p = key + node.offset;
		UCHAR *nodeEnd = q + node.length - node.getAppendedRecordNumberLength();

		// Compare the main part of the key
		for (;;)
			{
			if (p >= keyEnd)
				{
				// return the prior node if this node is longer or
				// we are not comparing the record number.

				if ((q < nodeEnd) || (searchRecordNumber == 0) || (node.length == 0))
					return prior;

				// Compare the record number.

				if (searchRecordNumber < node.getAppendedRecordNumber(nodeEnd))
					return prior;
				}

			if (q >= nodeEnd || *p > *q)
				break;						// searchKey > node, keep looking

			if (*p++ < *q++)
				return prior;
			}

		matchedOffset = (int) (p - key);
		}

	return prior;
}

void IndexPage::validate(void *before)
{
	Btn *bucketEnd = getEnd();
	UCHAR key [MAX_PHYSICAL_KEY_LENGTH];
	int keyLength = 0;

	for (IndexNode node (this);; node.getNext(bucketEnd))
		{
		if (node.node >= bucketEnd)
			{
			if (node.node != bucketEnd)
				{
				if (before)
					printPage ((IndexPage*) before, 0, false);
					
				printPage (this, 0, false);
				FATAL ("Index validate: trashed btree page");
				}
				
			break;
			}
			
		int l = MIN(keyLength, node.keyLength());
		int recordNumber = node.getNumber();
		
		if (recordNumber == END_BUCKET)
			ASSERT(nextPage != 0);

		for (UCHAR *p = node.key, *q = key + node.offset, *end = key + l; q < end; p++, q++)
			{
			if (*p > *q)		// current > previous, good
				break;
			else if (*p == *q)	// current == previous, good so far
				continue;     
			else if (*p < *q)	// current < previous, bad
				{
				if (before)
					printPage ((IndexPage*) before, 0, false);
					
				printPage(this, 0, false);
				FATAL("Mal-formed index page");
				}
			}

		keyLength = node.expandKey(key);
		}
}

void IndexPage::printPage(Bdb * bdb, bool inversion)
{
	printPage ((IndexPage*) bdb->buffer, bdb->pageNumber, inversion);
}

Bdb* IndexPage::createNewLevel(Dbb* dbb, int level, int version, TransId transId)
{
	Bdb *bdb = dbb->allocPage (PAGE_btree, transId);
	BDB_HISTORY(bdb);
	IndexPage *page = (IndexPage*) bdb->buffer;
	page->level = level;
	page->version = version;
	IndexKey dummy;
	page->length = OFFSET (IndexPage*, nodes);
	//page->addNode (dbb, &dummy, END_LEVEL);
	IndexNode node;
	node.insert(page->nodes, 0, 0, dummy.key, END_LEVEL);
	page->length += IndexNode::nodeLength(0, 0, END_LEVEL);
	
	return bdb;
}

Bdb* IndexPage::createNewLevel(Dbb * dbb, int level, int version, int32 page1, int32 page2, IndexKey* key2, TransId transId)
{
	Bdb *parentBdb = createNewLevel(dbb, level, version, transId);
	BDB_HISTORY(parentBdb);
	IndexPage *parentPage = (IndexPage*) parentBdb->buffer;
	IndexKey dummy(key2->index);
	parentPage->addNode (dbb, &dummy, page1);
	parentPage->addNode (dbb, key2, page2);
	//parentPage->validate(NULL);

	return parentBdb;
}

Bdb* IndexPage::findLevel(Dbb * dbb, int32 indexId, Bdb *bdb, int level, IndexKey *indexKey, int32 recordNumber)
{
	IndexPage *page = (IndexPage*) bdb->buffer;
	
	while (page->level > level)
		{
		IndexNode node (page->findNodeInBranch(indexKey, recordNumber));

		int32 pageNumber = node.getNumber();
		int32 parentPageNumber = bdb->pageNumber;
		
		if (pageNumber == END_BUCKET)
			{
			if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEVEL))
				{
				Log::debug("\n***** findLevel(%d, %d) - node.number = %d\n", recordNumber, indexId, pageNumber);
				page->printPage(page, 0, true, false);
				}
			pageNumber = page->nextPage;
			}
			
		if (pageNumber == 0)
			{
			page->printPage (bdb, false);
			//node.parseNode(page->findNodeInBranch (indexKey, recordNumber));	// try again for debugging
			bdb->release(REL_HISTORY);
			throw SQLError (DATABASE_CORRUPTION, "index %d damaged", indexId);
			}
		
		bdb = dbb->handoffPage (bdb, pageNumber, PAGE_btree, Exclusive);
		BDB_HISTORY(bdb);
		page = (IndexPage*) bdb->buffer;

		if (dbb->debug & (DEBUG_PAGES | DEBUG_FIND_LEVEL))
			page->printPage (bdb, false);
		
		page->parentPage = parentPageNumber;
		}

	return bdb;
}

int IndexPage::deleteNode(Dbb * dbb, IndexKey *indexKey, int32 recordNumber)
{
	ASSERT(level == 0);

	UCHAR *key = indexKey->key;
	uint keyLength = indexKey->keyLength;

	if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_DELETED))
		Btn::printKey ("delete key: ", indexKey, 0, false);
	if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_DELETED))
		printPage (this, 0, false);

	IndexKey nextKey;
	IndexKey priorKey;
	Btn *bucketEnd = getEnd();
	IndexNode node (findNodeInLeaf (indexKey, &priorKey), bucketEnd);
	int offset = (int) ((char*) node.node - (char*) this);

	// Make sure we've got an exact hit on key

	if (node.offset + node.length != keyLength || 
		memcmp (priorKey.key, key, node.offset) != 0 ||
		memcmp (node.key, key + node.offset, node.length))
		return -1;

	// We've got the start of the duplicate chain.  Find specific node

	for (bool first = true; node.node < bucketEnd; node.getNext(bucketEnd), first = false)
		{
		offset = (int) ((char*) node.node - (char*) this);
		
		if (!first && (node.offset != keyLength || node.length != 0))
			return -1;
			
		int32 number = node.getNumber();
		
		if (number == recordNumber)
			{
			IndexNode next (node.nextNode);
			
			if (next.node >= bucketEnd)
				length = (int) ((char*) node.node - (char*) this);
			else
				{
				// Reconstruct following key for recompression

				memcpy(nextKey.key, key, next.offset);
				memcpy(nextKey.key + next.offset, next.key, next.length);
				nextKey.keyLength = next.offset + next.length;

				Btn *tail = next.nextNode;
				int tailLength = (int) ((char*) bucketEnd - (char*) tail);

				// Compute new prefix length; rebuild node

				int prefix = computePrefix(&priorKey, &nextKey);
				int32 num = next.getNumber();
				node.insert(node.node, prefix, nextKey.keyLength - prefix, nextKey.key, num);

				// Compute length of remainder of bucket (no compression issues)

				Btn *newTail = node.nextNode;
				
				if (tailLength > 0)
					memmove(newTail, tail, tailLength);

				length = (int) ((char*) newTail + tailLength - (char*) this);
				//validate(NULL);
				}
				
			if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_DELETED))
				printPage(this, 0, false);
				
			return 1;
			}
			
		node.expandKey(&priorKey);
		}

	return 0;
}

void IndexPage::printPage(IndexPage * page, int32 pageNumber, bool inversion)
{
	LogLock logLock;
	UCHAR	key [MAX_PHYSICAL_KEY_LENGTH];
	int		length;

	Log::debug ("Btree Page %d, level %d, length %d, prior %d, next %d, parent %d\n", 
			pageNumber,
			page->level,
			page->length,
			page->priorPage,
			page->nextPage,
			page->parentPage);

	Btn *end = (Btn*) ((UCHAR*) page + page->length);
	IndexNode node (page);

	for (; node.node < end; node.getNext(end))
		{
		Log::debug ("   %d. offset %d, length %d, %d: ",
				(char*) node.node - (char*) page, 
				node.offset, node.length, node.getNumber());
		node.expandKey (key);
		node.printKey ("", key, inversion);
		}

	length = (int) ((UCHAR*) node.node - (UCHAR*) page);

	if (length != page->length)
		Log::debug ("**** bad bucket length -- should be %d ***\n", length);
}

void IndexPage::printPage(IndexPage *page, int32 pageNum, bool printDetail, bool inversion)
{
	LogLock logLock;
#ifdef DEBUG_INDEX_PAGE
	int32	pageNumber = page->pageNumber;
#else
	int32	pageNumber = pageNum;
#endif	

	Log::debug ("Page: %d  level: %d  length: %d  prior: %d  next: %d  parent: %d BEGIN\n", 
			pageNumber,
			page->level,
			page->length,
			page->priorPage,
			page->nextPage,
			page->parentPage);

	Btn *end = (Btn*) ((UCHAR*) page + page->length);
	IndexNode node (page);
	IndexNode lastNode1;
	IndexNode lastNode2;

	for (int i = 0; node.node < end; node.getNext(end), i++)
		{
		if (i < 3 || printDetail)
			printNode(i, page, pageNumber, node, inversion);
		else if (node.getNumber() < 0)
			{
			if (i > 3)
				Log::debug("       ...\n");
			if (i > 4)
				printNode(i - 2, page, pageNumber, lastNode2, inversion);
			if (i > 3)
				printNode(i - 1, page, pageNumber, lastNode1, inversion);
			printNode(i, page, pageNumber, node, inversion);
			}
			
		lastNode2 = lastNode1;
		lastNode1 = node;
		}

//	if (printDetail)
		Log::debug ("Page: %d  level: %d  length: %d  prior: %d  next: %d  parent: %d END\n", 
				pageNumber,
				page->level,
				page->length,
				page->priorPage,
				page->nextPage,
				page->parentPage);

	int length = (UCHAR*) node.node - (UCHAR*) page;

	if (length != page->length)
		Log::debug ("**** bad bucket length -- should be %d ***\n", length);
}

void IndexPage::printNode(int i, IndexPage * page, int32 pageNumber, IndexNode & node, bool inversion)
{
	UCHAR key[MAX_PHYSICAL_KEY_LENGTH];

	//LogLock logLock;
	node.expandKey(key);
	
	if (page->level == 0)
		Log::debug("%4d:  record: %6d,  ofs: %4d,  len: %4d,  pgofs: %4d | %6d ",
				i,						     	   // node count
				node.getNumber(),                  // record number
				node.offset,                       // key offset
				node.length,                       // stored length of key
				(char*) node.node - (char*) page,  // physical offset into page
				pageNumber);
	else
		Log::debug("%4d:  page:   %6d,  ofs: %4d,  len: %4d,  pgofs: %4d, record: %6d | %6d ",
				i,	                               // node count
				node.getNumber(),                  // page number
				node.offset,                       // key offset
				node.length,                       // stored length of key
				(char*) node.node - (char*) page,  // offset into page
				node.getAppendedRecordNumber(),    // appended record number
				pageNumber);
				
	node.printKey("", key, inversion);
}

void IndexPage::printNode(IndexPage *page, int32 pageNumber, Btn *node, bool inversion)
{
	Btn *end = (Btn*) ((UCHAR*) page + page->length);
	IndexNode tmp(page);
	int i;
	
	for (i = 0; tmp.node < end; tmp.getNext(end), i++)
		{
		if (tmp.node == node)
			break;
		}
		
	{
	//LogLock logLock;
	Log::debug("Page: %d  level: %d  length: %d  prior: %d  next: %d  parent: %d\n", 
			pageNumber,
			page->level,
			page->length,
			page->priorPage,
			page->nextPage,
			page->parentPage);
		
	printNode(i, page, pageNumber, tmp, inversion);
	}
}

void IndexPage::validate(Dbb *dbb, Validation *validation, Bitmap *pages, int32 parentPageNumber)
{
	if (pageType == PAGE_inversion)
		{
		((InversionPage*) this)->validate (dbb, validation, pages);
		return;
		}

	// Take care of earlier level first

	Bitmap children;

	if (level)
		{
		Btn *end = (Btn*) ((UCHAR*) this + length);
		
		if (nodes < end)
			{
			IndexNode node(this);
			int32 pageNumber = node.getNumber();
			
			if (pageNumber > 0)
				{
				Bdb *bdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
				BDB_HISTORY(bdb);
				
				if (level == 1 && bdb->buffer->pageType == PAGE_inversion)
					{
					children.set (pageNumber);
					validation->inUse (pageNumber, "InversionPage");
					InversionPage *inversionPage = (InversionPage*) bdb->buffer;
					inversionPage->validate (dbb, validation, &children);
					}
				else if (validation->isPageType (bdb, PAGE_btree, "intermediate index page a, index id %d", validation->indexId))
					{
					children.set (pageNumber);
					validation->inUse (pageNumber, "IndexPage");
					IndexPage *indexPage = (IndexPage*) bdb->buffer;
					indexPage->validate (dbb, validation, &children, pageNumber);
					}
					
				bdb->release(REL_HISTORY);
				}
			}
		}

	if (level)
		validateNodes (dbb, validation, &children, parentPageNumber);

	// Next, loop through siblings

	for (int32 pageNumber = nextPage; pageNumber;)
		{
		Bdb *bdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
		BDB_HISTORY(bdb);
		
		if (validation->isPageType (bdb, PAGE_btree, "intermediate index page b, index id %d", validation->indexId))
			{
			pages->set (pageNumber);
			validation->inUse (pageNumber, "IndexPage");
			IndexPage *indexPage = (IndexPage*) bdb->buffer;
			
			if (indexPage->level)
				indexPage->validateNodes(dbb, validation, &children, pageNumber);
				
			pageNumber = indexPage->nextPage;
			}
		else
			pageNumber = 0;
			
		bdb->release(REL_HISTORY);
		}
}


void IndexPage::validateNodes(Dbb *dbb, Validation *validation, Bitmap *children, int32 parentPageNumber)
{
	Btn *bucketEnd = getEnd();

	for (IndexNode node (this); node.node < bucketEnd; node.getNext(bucketEnd))
		{
		int32 pageNumber = node.getNumber();
		
		if (pageNumber == END_LEVEL || pageNumber == END_BUCKET)
			{
			if (node.nextNode != bucketEnd)
				validation->error ("bucket end not last node");
				
			break;
			}
			
		if (!children->isSet (pageNumber))
			{
			validation->error ("lost index child page %d, index id %d", pageNumber, validation->indexId);
			printPage (this, parentPageNumber, false);
			Bdb *bdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
			BDB_HISTORY(bdb);
			
			if (level == 1 && bdb->buffer->pageType == PAGE_inversion)
				{
				InversionPage *inversionPage = (InversionPage*) bdb->buffer;
				inversionPage->validate (dbb, validation, children);
				}
			else if (validation->isPageType (bdb, PAGE_btree, "IndexPage"))
				{
				IndexPage *indexPage = (IndexPage*) bdb->buffer;
				
				if (indexPage->parentPage == parentPageNumber)
					indexPage->validate (dbb, validation, children, bdb->pageNumber);
				else
					validation->error ("lost index child page %d, index id %d has wrong parent", pageNumber, validation->indexId);
				}
				
			bdb->release(REL_HISTORY);
			}
		}
}

/***
void IndexPage::validateInsertion(int keyLength, UCHAR *key, int32 recordNumber)
{
	Btn *bucketEnd = getEnd();
	IndexNode node (this);

	for (; node.node < bucketEnd; node.getNext())
		{
		int32 number = node.getNumber();
		if (number == recordNumber)
			return;
		if (number == END_LEVEL || number == END_BUCKET)
			break;
		}

	Log::log ("IndexPage::validateInsertion insert of record %d failed\n", recordNumber);
	Btn::printKey ("insert key: ", keyLength, key, 0, false);
	printPage (this, 0, false);
	//ASSERT (false);
}
***/

void IndexPage::analyze(int pageNumber)
{
	Btn *bucketEnd = getEnd();
	int count = 0;
	int prefixTotal = 0;
	int lengthTotal = 0;

	for (IndexNode node (this); node.node < bucketEnd; node.getNext(bucketEnd))
		{
		++count;
		prefixTotal += node.length;
		lengthTotal += node.length + node.offset;
		}
	
	Log::debug ("Index page %d, level %d, %d nodes, %d byte compressed, %d bytes uncompressed\n",
				pageNumber, level, count, prefixTotal, lengthTotal);
}

bool IndexPage::isLastNode(Btn *indexNode)
{
	IndexNode node (indexNode);

	return (char*) node.nextNode == (char*) this + length;
}

Bdb* IndexPage::splitIndexPageEnd(Dbb *dbb, Bdb *bdb, TransId transId, IndexKey *insertKey, int recordNumber)
{
	if (dbb->debug & (DEBUG_KEYS | DEBUG_SPLIT_PAGE))
		Btn::printKey("insert key", insertKey, 0, false);
	if (dbb->debug & (DEBUG_PAGES | DEBUG_SPLIT_PAGE))
		printPage (this, 0, false);

	Btn *pageEnd = getEnd();
	IndexNode node (this);
	Btn *chain = node.node;
	uint priorLength = 0;
	IndexKey tempKey(insertKey->index);
	Btn *priorNode = NULL;

	// Lets start by finding a place to split the buck near the end

	for (; node.nextNode < pageEnd; node.getNext(pageEnd))
		{
		priorNode = node.node;
		node.expandKey (&tempKey);

		if (node.offset != priorLength || node.length)
			{
			chain = node.node;
			priorLength = node.length;
			}
		}

	// Allocate and format new page.  Link forward and backward to siblings.

	Bdb *splitBdb = splitPage(dbb, bdb, transId);
	IndexPage *split = (IndexPage*) splitBdb->buffer;

	// Build new page from what's left over from last page

	IndexKey rolloverKey(&tempKey);
	node.expandKey(&rolloverKey);
	int rolloverNumber = node.getNumber();
	int offset = computePrefix(insertKey, &tempKey);

	// nodeOverhead assumes END_BUCKET is the recordNumber.
	int nodeOverhead = 3 + (offset >= 128) + (insertKey->keyLength - offset >= 128);
	char *nextNextNode = (char*) node.nextNode - node.length + insertKey->keyLength - offset + nodeOverhead;

	if (nextNextNode >= (char*) this + dbb->pageSize)
		{
		node.parseNode(priorNode);
		int number = node.getNumber();
		//node.setNumber(END_BUCKET);
		node.insert(node.node, node.offset, node.length, tempKey.key, END_BUCKET);
		length = (int) ((char*) node.nextNode - (char*) this);
		split->appendNode(&tempKey, number, dbb->pageSize);
		}
	else
		{
		int offset = computePrefix(&tempKey, insertKey);
		node.insert(node.node, offset, insertKey->keyLength - offset, insertKey->key, END_BUCKET);
		length = (int) ((char*)(node.nextNode) - (char*) this);
		}

	split->appendNode(insertKey, recordNumber, dbb->pageSize);
	split->appendNode(&rolloverKey, rolloverNumber, dbb->pageSize);
	//printf ("splitIndexPage: level %d, %d -> %d\n", level, bdb->pageNumber, nextPage);
	//validate( splitBdb->buffer);

	if (dbb->debug & (DEBUG_PAGES | DEBUG_SPLIT_PAGE))
		{
		printPage (this, 0, false);
		printPage (splitBdb, false);
		}

	logIndexPage(splitBdb, transId);

	return splitBdb;
}

Btn* IndexPage::appendNode(IndexKey *newKey, int32 recordNumber, int pageSize)
{
	IndexKey key(newKey->index);
	key.keyLength = 0;
	Btn *end = (Btn*)((char*) this + length);
	IndexNode node(this);

	for (; node.node < end; node.getNext(end))
		node.expandKey(&key);
		
	int offset = computePrefix(newKey, &key);

	if (length + (int) newKey->keyLength - offset >= pageSize)
		throw SQLError (INDEX_OVERFLOW, "Cannot split page; expanded key will not fit");

	node.insert(node.node, offset, newKey->keyLength - offset, newKey->key, recordNumber);
	length = (int) ((char*)(node.nextNode) - (char*) this);

	return node.node;
}

Bdb* IndexPage::splitPage(Dbb *dbb, Bdb *bdb, TransId transId)
{
	Bdb *splitBdb = dbb->allocPage (PAGE_btree, transId);
	BDB_HISTORY(splitBdb);
	IndexPage *split = (IndexPage*) splitBdb->buffer;

	split->level = level;
	split->version = version;
	split->priorPage = bdb->pageNumber;
	split->parentPage = parentPage;
	split->length = OFFSET(IndexPage*, nodes);
	
	// Link page into right sibling

	if ((split->nextPage = nextPage))
		{
		Bdb *nextBdb = dbb->fetchPage (split->nextPage, PAGE_btree, Exclusive);
		BDB_HISTORY(bdb);
		IndexPage *next = (IndexPage*) nextBdb->buffer;
		dbb->setPrecedence(bdb, splitBdb->pageNumber);
		nextBdb->mark(transId);
		next->priorPage = splitBdb->pageNumber;
		nextBdb->release(REL_HISTORY);
		}

	nextPage = splitBdb->pageNumber;

	return splitBdb;
}

Btn* IndexPage::findInsertionPoint(IndexKey *indexKey, int32 recordNumber, IndexKey *expandedKey)
{
	Btn *bucketEnd = getEnd();

	return findInsertionPoint(level, indexKey, recordNumber, expandedKey, nodes, bucketEnd);
}

void IndexPage::logIndexPage(Bdb *bdb, TransId transId)
{
	Dbb *dbb = bdb->dbb;

	if (dbb->serialLog->recovering)
		return;

	ASSERT(bdb->useCount > 0);
	IndexPage *indexPage = (IndexPage*) bdb->buffer;
	
	/**** Debugging only -- can deadlock
	if (indexPage->parentPage)
		{
		Bdb *parentBdb = dbb->fetchPage(indexPage->parentPage, PAGE_btree, Shared);
		BDB_HISTORY(bdb);
		IndexPage *parentPage = (IndexPage*) parentBdb->buffer;
		ASSERT(parentPage->level == indexPage->level + 1);
		parentBdb->release();
		}
	***/
	
	dbb->serialLog->logControl->indexPage.append(dbb, transId, INDEX_VERSION_1, bdb->pageNumber, indexPage->level, 
												 indexPage->parentPage, indexPage->priorPage, indexPage->nextPage,  
												 indexPage->length - OFFSET (IndexPage*, nodes), 
												 (const UCHAR*) indexPage->nodes);
}

Btn* IndexPage::findInsertionPoint(int level, IndexKey* indexKey, int32 recordNumber, IndexKey* expandedKey, Btn* nodes, Btn* bucketEnd)
{
	UCHAR *key = indexKey->key;
	const UCHAR *keyEnd = key + indexKey->keyLength;
	IndexNode node(nodes);
	uint matchedOffset = 0;
	uint priorLength = 0;

	if (level && indexKey->recordNumber)
		keyEnd -= indexKey->getAppendedRecordNumberLength();

	if (node.offset)
		{
		while ((matchedOffset < node.offset) && 
		       (indexKey->key[matchedOffset] == expandedKey->key[matchedOffset]))
			{
			matchedOffset++;
			}
		
		priorLength = expandedKey->keyLength;
		}

	for (;; node.getNext(bucketEnd))
		{
		if (node.node >= bucketEnd)
			{
			if (node.node != bucketEnd)
				{
				Btn::printKey ("indexKey: ", indexKey, 0, false);
				Btn::printKey ("expandedKey: ", expandedKey, 0, false);
				FATAL ("Index findInsertionPoint: trashed btree page");
				}
				
			expandedKey->keyLength = priorLength;
				
			return bucketEnd;
			}
			
		int32 number = node.getNumber();
		
		if (number == END_LEVEL || number == END_BUCKET)
			break;
			
		if (node.offset < matchedOffset)
			break;
		
		if (node.offset > matchedOffset)
			continue;
		
		const UCHAR *p = key + node.offset;
		const UCHAR *q = node.key;
		const UCHAR *nodeEnd = q + node.length;
		if (level)
			nodeEnd -= node.getAppendedRecordNumberLength();
		
		for (;;)
			{
			if (p >= keyEnd)
				{
				if (q < nodeEnd)
					goto exit;		// key < node, use this node.

				// Key values are equal, so test the record numbers.
				// Record numbers are embedded for level 0, appended for level > 0.

				if (level)
					{
					if (node.length == 0)
						goto exit;  // node length of 0 is valid for null keys on level 0
					else if (indexKey->recordNumber > node.getAppendedRecordNumber())
						break;		// keep looking
					}
				else
					{
					if (recordNumber > node.number)
						break;		// keep looking.
					}

				goto exit;			// key record number <= node record number, use this node.
				}
				
			if (q >= nodeEnd || *p > *q)
				break;			// key > node, keep looking.
				
			if (*p++ < *q++)
				goto exit;		// key < node, use this node.
			}
			
		matchedOffset = (int) (p - key);
		node.expandKey (expandedKey);

		if (level)
			expandedKey->recordNumber = node.getAppendedRecordNumber();

		priorLength = node.offset + node.length;
		}

	exit:

	expandedKey->keyLength = priorLength;

	return node.node;
}

Btn* IndexPage::getEnd(void)
{
	return (Btn*) ((UCHAR*) this + length);
}
