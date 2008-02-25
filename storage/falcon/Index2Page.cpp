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

// Index2Page.cpp: implementation of the Index2Page class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <string.h>
#include <stdio.h>
#include "Engine.h"
#include "Index2Page.h"
#include "Dbb.h"
#include "BDB.h"
#include "Validation.h"
#include "InversionPage.h"
#include "Index2Node.h"
#include "Log.h"
#include "LogLock.h"
#include "Debug.h"
#include "IndexKey.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "SQLError.h"
#include "RootPage.h"
#include "Index.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

AddNodeResult Index2Page::addNode(Dbb *dbb, IndexKey *indexKey, int32 recordNumber)
{
	if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_ADDED))
		Btn::printKey ("new key: ", indexKey, 0, false);
		
	if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_ADDED))
		printPage (this, 0, false);

	// Find the insertion point.  In the process, compute the prior key

	//validate(NULL);
	UCHAR *key = indexKey->key;
	UCHAR	nextKey [MAX_PHYSICAL_KEY_LENGTH];
	IndexKey priorKey;

	Index2Node node(findInsertionPoint(indexKey, recordNumber, &priorKey));

	if (node.getNumber() == recordNumber && 
		 indexKey->keyLength == node.offset + node.length &&
		 memcmp (priorKey.key, key, node.offset) == 0 &&
		 memcmp (node.key, key + node.offset, node.length) == 0)
		return Duplicate;

	// We got the prior key value.  Get key value for insertion point

	memcpy(nextKey, priorKey.key, priorKey.keyLength);
	int nextLength = node.offset + node.length;

	// Compute delta size of new node

	int offset1 = computePrefix (&priorKey, indexKey);
	int length1 = indexKey->keyLength - offset1;
	int delta = Index2Node::nodeLength(offset1, length1, recordNumber);
	int32 nextNumber = 0;
	int offset2;
	
	if ((UCHAR*) node.node == (UCHAR*) this + length)
		offset2 = -1;
	else
		{
		node.expandKey (nextKey);
		offset2 = computePrefix(indexKey->keyLength, indexKey->key, node.offset + node.length, nextKey);
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

		if (offset2 == -1 || keyCompare(node.keyLength(), nextKey, indexKey->keyLength, indexKey->key) >= 0)
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

	Index2Node newNode;
	newNode.insert (node.node, offset1, length1, key, recordNumber);

	// If necessary, rebuild next node

	if (offset2 >= 0)
		newNode.insert (newNode.nextNode, offset2, nextLength - offset2, nextKey, nextNumber);

	length += delta;
	//validate(NULL);

	if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_ADDED))
		printPage (this, 0, false);

	return NodeAdded;
}

Btn* Index2Page::findNode(IndexKey *indexKey, IndexKey *expandedKey)
{
	uint offset = 0;
	uint priorLength = 0;
	UCHAR *key = indexKey->key;
	UCHAR *keyEnd = key + indexKey->keyLength;
	Btn *bucketEnd = (Btn*) ((char*) this + length);

	if (indexKey->keyLength == 0)
		{
		if (expandedKey)
			expandedKey->keyLength = 0;

		return nodes;
		}

	Index2Node node (this);

	for (;; node.getNext())
		{
		if (node.node >= bucketEnd)
			{
			if (node.node != bucketEnd)
				FATAL ("Index2Page::findNode: trashed btree page");
				
			if (expandedKey)
				expandedKey->keyLength = priorLength;
				
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
		
		if (expandedKey)
			{
			node.expandKey (expandedKey);
			priorLength = node.offset + node.length;
			}
		}

	exit:

	if (expandedKey)
		expandedKey->keyLength = priorLength;

	return node.node;
}

int Index2Page::computePrefix(int l1, UCHAR * v1, int l2, UCHAR * v2)
{
	int		n, max;

	for (n = 0, max = MIN (l1, l2); n < max; ++n)
		if (*v1++ != *v2++)
			return n;

	return n;
}

int Index2Page::computePrefix(IndexKey *key1, IndexKey *key2)
{
	int max = MIN(key1->keyLength, key2->keyLength);
	int n;
	UCHAR *v1 = key1->key;
	UCHAR *v2 = key2->key;

	for (n = 0; n < max; ++n)
		if (*v1++ != *v2++)
			return n;

	return n;
}

Bdb* Index2Page::splitIndexPageMiddle(Dbb * dbb, Bdb *bdb, IndexKey *splitKey, TransId transId)
{
	if (dbb->debug & (DEBUG_PAGES | DEBUG_SPLIT_PAGE))
		printPage (this, 0, false);

	// Fudge page end to handle the all-duplicates problem

	UCHAR *key = splitKey->key;
	Btn *pageEnd = (Btn*) ((char*) this + dbb->pageSize - sizeof(Btn) - 256);
	Btn *midpoint = (Btn*) ((UCHAR*) nodes + 
					  (length - OFFSET (Index2Page*, nodes)) / 2);

	/* Find midpoint.  Don't worry about duplicates.
	   Use the node that crosses the midpoint of what is 
	   currently on the page.*/

	Index2Node node (this);
	Index2Node prevNode = node;
	Btn *chain = node.node;

	for (; node.node < pageEnd; node.getNext())
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
	Index2Page *split = (Index2Page*) splitBdb->buffer;

	/* Copy midpoint node to new page.  Compute length, then
	   copy tail of page to new page */

	int32 recordNumber = node.getNumber();
	Index2Node newNode;
	newNode.insert(split->nodes, 0, kLength, key, recordNumber);

	memcpy(newNode.nextNode, node.nextNode, tailLength);

	/* Now we get tricky.  We need to add a "end bucket" marker with the value
	 * of the next bucket.  So copy a little more (it's already compressed)
	 * and zap the node "number" to "END_BUCKET" */

	//node.setNumber (END_BUCKET);
	node.insert(node.node, node.offset, node.length, key, END_BUCKET);
	length = (int) ((UCHAR*) node.nextNode - (UCHAR*) this);

	//validate(split);
	split->length = (int) (((UCHAR*) newNode.nextNode + tailLength) - (UCHAR*) split);
	//split->validate(this);

	if (dbb->debug & (DEBUG_PAGES | DEBUG_SPLIT_PAGE))
		{
		printPage (this, 0, false);
		printPage (splitBdb, false);
		}

	logIndexPage(splitBdb, transId);


	return splitBdb;
}

/**
@brief		Return the node from a branch page that holds the indexKey.
**/
Btn* Index2Page::findNodeInBranch(IndexKey *indexKey)
{
	ASSERT(level > 0);
	uint offset = 0;
	UCHAR *keyEnd = indexKey->keyEnd();
	Btn *bucketEnd = (Btn*) ((UCHAR*) this + length);
	Btn *prior = nodes;
	UCHAR *key = indexKey->key;

	for (Index2Node node (this); node.node < bucketEnd; prior = node.node, node.getNext())	
		{
		UCHAR *q = node.key;

		if (node.offset < offset)
			return prior;

		if (node.getNumber() == END_LEVEL)
			return prior;

		if (node.offset > offset || node.length == 0)
			continue;

		UCHAR *p = key + node.offset;
		UCHAR *nodeEnd = q + node.length;

		for (;;)
			{
			if (p == keyEnd)
				return prior;	// searchKey <= node, return prior

			if (q == nodeEnd || *p > *q)
				break;			// searchKey > node, keep looking

			if (*p++ < *q++)
				return prior;	// searchKey <= node, return prior
			}

		offset = (int) (p - key);
		}

	return prior;
}

void Index2Page::validate(void *before)
{
	Btn *bucketEnd = (Btn*) ((char*) this + length);
	UCHAR key [MAX_PHYSICAL_KEY_LENGTH];
	int keyLength = 0;

	for (Index2Node node (this);; node.getNext())
		{
		if (node.node >= bucketEnd)
			{
			if (node.node != bucketEnd)
				{
				if (before)
					printPage ((Index2Page*) before, 0, false);
				printPage (this, 0, false);
				FATAL ("Index2Page::validate: trashed btree page");
				}
				
			break;
			}
			
		int l = MIN(keyLength, node.keyLength());

		for (UCHAR *p = node.key, *q = key + node.offset, *end = key + l; q < end;)
			if (*p++ > *q++)
				break;
			else if (*p++ < *q++)
				{
				if (before)
					printPage ((Index2Page*) before, 0, false);
				printPage(this, 0, false);
				FATAL("Mal-formed index page");
				}

		keyLength = node.expandKey(key);
		}
}

void Index2Page::printPage(Bdb * bdb, bool inversion)
{
	printPage ((Index2Page*) bdb->buffer, bdb->pageNumber, inversion);
}

Bdb* Index2Page::createNewLevel(Dbb * dbb, int level, int version, int32 page1, int32 page2, IndexKey* key2, TransId transId)
{
	Bdb *parentBdb = dbb->allocPage (PAGE_btree, transId);
	BDB_HISTORY(parentBdb);
	Index2Page *parentPage = (Index2Page*) parentBdb->buffer;
	parentPage->level = level;
	parentPage->version = version;
	IndexKey dummy(key2->index);
	dummy.keyLength = 0;
	parentPage->length = OFFSET (Index2Page*, nodes);
	parentPage->addNode (dbb, &dummy, END_LEVEL);
	parentPage->addNode (dbb, &dummy, page1);
	parentPage->addNode (dbb, key2, page2);
	//parentPage->validate(NULL);

	return parentBdb;
}

Bdb* Index2Page::findLevel(Dbb * dbb, Bdb *bdb, int level, IndexKey *indexKey, int32 recordNumber)
{
	Index2Page *page = (Index2Page*) bdb->buffer;
	
	while (page->level > level)
		{
		Index2Node node (page->findNodeInBranch(indexKey));
		bdb = dbb->handoffPage (bdb, node.getNumber(), PAGE_btree, Exclusive);
		BDB_HISTORY(bdb);
		page = (Index2Page*) bdb->buffer;
		}

	return bdb;
}

int Index2Page::deleteNode(Dbb * dbb, IndexKey *indexKey, int32 recordNumber)
{
	UCHAR *key = indexKey->key;
	uint keyLength = indexKey->keyLength;

	if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_DELETED))
		Btn::printKey ("delete key: ", indexKey, 0, false);
	if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_DELETED))
		printPage (this, 0, false);

	UCHAR	nextKey [MAX_PHYSICAL_KEY_LENGTH];
	IndexKey priorKey;
	Index2Node node (findNode (indexKey, &priorKey));
	Btn *bucketEnd = (Btn*) ((char*) this + length);
	int offset = (int) ((char*) node.node - (char*) this);

	// Make sure we've got an exact hit on key

	if (node.offset + node.length != keyLength || 
		memcmp (priorKey.key, key, node.offset) != 0 ||
		memcmp (node.key, key + node.offset, node.length))
		return -1;

	// We've got the start of the duplicate chain.  Find specific node

	for (bool first = true; node.node < bucketEnd; node.getNext(), first = false)
		{
		offset = (int) ((char*) node.node - (char*) this);
		
		if (!first && (node.offset != keyLength || node.length != 0))
			return -1;
			
		int32 number = node.getNumber();
		
		if (number == recordNumber)
			{
			Index2Node next (node.nextNode);

			if (next.node >= bucketEnd)
				length = (int) ((char*) node.node - (char*) this);
			else
				{
				// Reconstruct following key for recompression

				memcpy (nextKey, key, next.offset);
				memcpy (nextKey + next.offset, next.key, next.length);
				int nextLength = next.offset + next.length;

				Btn *tail = next.nextNode;
				int tailLength = (int) ((char*) bucketEnd - (char*) tail);

				// Compute new prefix length; rebuild node

				int prefix = computePrefix (priorKey.keyLength, priorKey.key, nextLength, nextKey);
				int32 num = next.getNumber();
				node.insert (node.node, prefix, nextLength - prefix, nextKey, num);

				// Compute length of remainder of bucket (no compression issues)

				Btn *newTail = node.nextNode;
				if (tailLength > 0)
					memcpy (newTail, tail, tailLength);

				length = (int) ((char*) newTail + tailLength - (char*) this);
				//validate(NULL);
				}
				
			if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_DELETED))
				printPage (this, 0, false);
				
			return 1;
			}
			
		node.expandKey (&priorKey);
		}

	return 0;
}

void Index2Page::printPage(Index2Page * page, int32 pageNumber, bool inversion)
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
	Index2Node node (page);

	for (; node.node < end; node.getNext())
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

void Index2Page::validate(Dbb *dbb, Validation *validation, Bitmap *pages, int32 parentPageNumber)
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
			Index2Node node(this);
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
					validation->inUse (pageNumber, "Index2Page");
					Index2Page *indexPage = (Index2Page*) bdb->buffer;
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
			validation->inUse (pageNumber, "Index2Page");
			Index2Page *indexPage = (Index2Page*) bdb->buffer;
			
			if (indexPage->level)
				indexPage->validateNodes(dbb, validation, &children, pageNumber);
				
			pageNumber = indexPage->nextPage;
			}
		else
			pageNumber = 0;
			
		bdb->release(REL_HISTORY);
		}
}


void Index2Page::validateNodes(Dbb *dbb, Validation *validation, Bitmap *children, int32 parentPageNumber)
{
	Btn *bucketEnd = (Btn*) ((char*) this + length);

	for (Index2Node node (this); node.node < bucketEnd; node.getNext())
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
			else if (validation->isPageType (bdb, PAGE_btree, "Index2Page"))
				{
				Index2Page *indexPage = (Index2Page*) bdb->buffer;
				
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
void Index2Page::validateInsertion(int keyLength, UCHAR *key, int32 recordNumber)
{
	Btn *bucketEnd = (Btn*) ((char*) this + length);
	Index2Node node (this);

	for (; node.node < bucketEnd; node.getNext())
		{
		int32 number = node.getNumber();
		if (number == recordNumber)
			return;
		if (number == END_LEVEL || number == END_BUCKET)
			break;
		}

	Log::log ("Index2Page::validateInsertion insert of record %d failed\n", recordNumber);
	Btn::printKey ("insert key: ", keyLength, key, 0, false);
	printPage (this, 0, false);
	//ASSERT (false);
}
***/

void Index2Page::analyze(int pageNumber)
{
	Btn *bucketEnd = (Btn*) ((char*) this + length);
	int count = 0;
	int prefixTotal = 0;
	int lengthTotal = 0;

	for (Index2Node node (this); node.node < bucketEnd; node.getNext())
		{
		++count;
		prefixTotal += node.length;
		lengthTotal += node.length + node.offset;
		}
	
	Log::debug ("Index page %d, level %d, %d nodes, %d byte compressed, %d bytes uncompressed\n",
				pageNumber, level, count, prefixTotal, lengthTotal);
}

bool Index2Page::isLastNode(Btn *indexNode)
{
	Index2Node node (indexNode);

	return (char*) node.nextNode == (char*) this + length;
}

Bdb* Index2Page::splitIndexPageEnd(Dbb *dbb, Bdb *bdb, TransId transId, IndexKey *insertKey, int recordNumber)
{
	if (dbb->debug & (DEBUG_KEYS | DEBUG_SPLIT_PAGE))
		Btn::printKey("insert key", insertKey, 0, false);
	if (dbb->debug & (DEBUG_PAGES | DEBUG_SPLIT_PAGE))
		printPage (this, 0, false);

	Btn *pageEnd = (Btn*) ((char*) this + length);
	Index2Node node (this);
	Btn *chain = node.node;
	uint priorLength = 0;
	IndexKey tempKey(insertKey->index);
	Btn *priorNode = NULL;

	// Lets start by finding a place to split the buck near the end

	for (; node.nextNode < pageEnd; node.getNext())
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
	Index2Page *split = (Index2Page*) splitBdb->buffer;

	// Build new page from what's left over from last page

	IndexKey rolloverKey(&tempKey);
	node.expandKey(&rolloverKey);
	int rolloverNumber = node.getNumber();
	int insertKeyLength = insertKey->keyLength;
	int offset = computePrefix(insertKeyLength, insertKey->key, tempKey.keyLength, tempKey.key);

	// nodeOverhead assumes END_BUCKET is the recordNumber.
	int nodeOverhead = 3 + (offset >= 128) + (insertKeyLength - offset >= 128);
	char *nextNextNode = (char*) node.nextNode - node.length + insertKeyLength - offset + nodeOverhead;

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
		node.insert(node.node, offset, insertKeyLength - offset, insertKey->key, END_BUCKET);
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

Btn* Index2Page::appendNode(int newLength, UCHAR *newKey, int32 recordNumber, int pageSize)
{
	UCHAR key [MAX_PHYSICAL_KEY_LENGTH];
	Btn *end = (Btn*)((char*) this + length);
	Index2Node node (nodes);
	int keyLength = 0;

	for (;node.node < end; node.getNext())
		keyLength = node.expandKey(key);

	int offset = computePrefix(newLength, newKey, keyLength, key);

	if (length + keyLength - offset >= pageSize)
		throw SQLError (INDEX_OVERFLOW, "Cannot split page; expanded key will not fit");

	node.insert(node.node, offset, newLength - offset, newKey, recordNumber);
	length = (int) ((char*)(node.nextNode) - (char*) this);

	return node.node;
}

Btn* Index2Page::appendNode(IndexKey *newKey, int32 recordNumber, int pageSize)
{
	IndexKey key(newKey->index);
	key.keyLength = 0;
	Btn *end = (Btn*)((char*) this + length);
	Index2Node node (nodes);

	for (;node.node < end; node.getNext())
		node.expandKey(&key);

	int offset = computePrefix(newKey, &key);

	if (length + (int) newKey->keyLength - offset >= pageSize)
		throw SQLError (INDEX_OVERFLOW, "Cannot split page; expanded key will not fit");

	node.insert(node.node, offset, newKey->keyLength - offset, newKey->key, recordNumber);
	length = (int) ((char*)(node.nextNode) - (char*) this);

	return node.node;
}

int Index2Page::keyCompare(int length1, UCHAR *key1, int length2, UCHAR *key2)
{
	// return -1 if key1 is greater, +1 if key2 is greater

	for (;length1 && length2; --length1, --length2)
		if (*key1++ != *key2++)
			return (key1 [-1] > key2 [-1]) ? -1 : 1;

	if (!length1 && !length2)
		return 0;

	return (length1) ? -1 : 1;
}

Bdb* Index2Page::splitPage(Dbb *dbb, Bdb *bdb, TransId transId)
{
	Bdb *splitBdb = dbb->allocPage (PAGE_btree, transId);
	BDB_HISTORY(splitBdb);
	Index2Page *split = (Index2Page*) splitBdb->buffer;

	split->level = level;
	split->version = version;
	split->priorPage = bdb->pageNumber;
	split->parentPage = parentPage;
	split->length = OFFSET(Index2Page*, nodes);
	
	// Link page into right sibling

	if ((split->nextPage = nextPage))
		{
		Bdb *nextBdb = dbb->fetchPage (split->nextPage, PAGE_btree, Exclusive);
		BDB_HISTORY(bdb);
		Index2Page *next = (Index2Page*) nextBdb->buffer;
		dbb->setPrecedence(bdb, splitBdb->pageNumber);
		nextBdb->mark(transId);
		next->priorPage = splitBdb->pageNumber;
		nextBdb->release(REL_HISTORY);
		}

	nextPage = splitBdb->pageNumber;

	return splitBdb;
}

Btn* Index2Page::findInsertionPoint(IndexKey *indexKey, int32 recordNumber, IndexKey *expandedKey)
{
	UCHAR *key = indexKey->key;
	const UCHAR *keyEnd = key + indexKey->keyLength;
	Btn *bucketEnd = (Btn*) ((char*) this + length);
	Index2Node node (this);
	uint offset = 0;
	uint priorLength = 0;

	for (;; node.getNext())
		{
		if (node.node >= bucketEnd)
			{
			if (node.node != bucketEnd)
				{
				Btn::printKey ("indexKey: ", indexKey, 0, false);
				Btn::printKey ("expandedKey: ", expandedKey, 0, false);
				printPage(this, 0, false);
				FATAL ("Index2Page::findInsertionPoint: trashed btree page");
				}
				
			expandedKey->keyLength = priorLength;
				
			return bucketEnd;
			}
			
		int32 number = node.getNumber();
		
		if (number == END_LEVEL || number == END_BUCKET)
			break;
			
		if (node.offset < offset)
			break;
		
		if (node.offset > offset)
			continue;
		
		const UCHAR *p = key + node.offset;
		const UCHAR *q = node.key;
		const UCHAR *nodeEnd = q + node.length;
		
		for (;;)
			{
			if (p == keyEnd)
				{
				if (level == 0 && recordNumber > number && (node.offset + node.length == indexKey->keyLength))
					break;

				goto exit;
				}
				
			if (q == nodeEnd || *p > *q)
				break;
				
			if (*p++ < *q++)
				goto exit;
			}
			
		offset = (int) (p - key);
		node.expandKey (expandedKey);
		priorLength = node.offset + node.length;
		}

	exit:

	expandedKey->keyLength = priorLength;

	return node.node;
}

int32 Index2Page::getRecordNumber(const UCHAR *ptr)
{
	int32 number = 0;
	
	for (int n = 0; n < 4; ++n)
		number = number << 8 | *ptr++;
	
	return number;
}

void Index2Page::logIndexPage(Bdb *bdb, TransId transId)
{
	Dbb *dbb = bdb->dbb;

	if (dbb->serialLog->recovering)
		return;

	ASSERT(bdb->useCount > 0);
	Index2Page *indexPage = (Index2Page*) bdb->buffer;
	dbb->serialLog->logControl->indexPage.append(dbb, transId, INDEX_VERSION_0, bdb->pageNumber, indexPage->level, 
												 indexPage->parentPage, indexPage->priorPage, indexPage->nextPage,  
												 indexPage->length - OFFSET (Index2Page*, nodes), 
												 (const UCHAR*) indexPage->nodes);
}

Btn* Index2Page::findInsertionPoint(int level, IndexKey* indexKey, int32 recordNumber, IndexKey* expandedKey, Btn* nodes, Btn* bucketEnd)
{
	UCHAR *key = indexKey->key;
	const UCHAR *keyEnd = key + indexKey->keyLength;
	Index2Node node(nodes);
	uint offset = 0;
	uint priorLength = 0;

	if (node.offset)
		{
		for (; offset < node.offset && indexKey->key[offset] == expandedKey->key[offset]; ++offset)
			;
		
		priorLength = expandedKey->keyLength;
		}

	for (;; node.getNext())
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
			
		if (node.offset < offset)
			break;
		
		if (node.offset > offset)
			continue;
		
		const UCHAR *p = key + node.offset;
		const UCHAR *q = node.key;
		const UCHAR *nodeEnd = q + node.length;
		
		for (;;)
			{
			if (p == keyEnd)
				{
				if (level == 0 && recordNumber > number && (node.offset + node.length == indexKey->keyLength))
					break;

				goto exit;
				}
				
			if (q == nodeEnd || *p > *q)
				break;
				
			if (*p++ < *q++)
				goto exit;
			}
			
		offset = (int) (p - key);
		node.expandKey (expandedKey);
		priorLength = node.offset + node.length;
		}

	exit:

	expandedKey->keyLength = priorLength;

	return node.node;
}
