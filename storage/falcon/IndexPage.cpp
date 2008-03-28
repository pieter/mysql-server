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

// Do not make supernode, if there is too much loss for prefix compression
#define SUPERNODE_COMPRESSION_LOSS_LIMIT(pageSize) (pageSize)/(SUPERNODES+1)/2

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

AddNodeResult IndexPage::addNode(Dbb *dbb, IndexKey *indexKey, int32 recordNumber)
{


	// Find the insertion point.  In the process, compute the prior key
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
	return addNode(dbb, indexKey, recordNumber, &node, &priorKey, &nextKey);
}

// Add node (with already known insertion point)
AddNodeResult IndexPage::addNode (Dbb *dbb, IndexKey *indexKey, int32 recordNumber, IndexNode *node, IndexKey *priorKey, IndexKey *nextKey)
{

	UCHAR *key = indexKey->key;

	if (dbb->debug & (DEBUG_KEYS | DEBUG_NODES_ADDED | DEBUG_PAGE_LEVEL))
		{
		Log::debug("\n***** addNode(%d) - ", recordNumber);
		Btn::printKey ("New key", indexKey, 0, false);
		}
		
	if (dbb->debug & (DEBUG_PAGES | DEBUG_NODES_ADDED))
		printPage (this, 0, false);
	//validate(NULL);

	// Compute delta size of new node
	int offset1 = computePrefix (priorKey, indexKey);

	// Check whether inserted node shall be supernode. If yes, does not compress the prefix.
	bool makeNextSuper;
	bool makeSuper = checkAddSuperNode(dbb->pageSize, node, indexKey, recordNumber, offset1, &makeNextSuper);
	if (makeSuper)
		offset1 = 0;

	int length1 = indexKey->keyLength - offset1;
	int delta = IndexNode::nodeLength(offset1, length1, recordNumber);
	int32 nextNumber;
	int offset2;
	
	if ((UCHAR*) node->node == (UCHAR*) this + length)
		{
		offset2 = -1;
		nextNumber = 0;
		}
	else
		{
		node->expandKey (nextKey);
		offset2 = computePrefix(indexKey, nextKey);

		if (offset2 > SUPERNODE_COMPRESSION_LOSS_LIMIT(dbb->pageSize))
			makeNextSuper = false; // compresses well, no supenode
		else if (makeNextSuper)
			offset2 = 0; // we'll make supernode, so don't prefix compress

		int deltaOffset = offset2 - node->offset;
		
		if (node->length >= 128 && (node->length - deltaOffset) < 128)
			--delta;

		if (node->offset < 128 && (node->offset + deltaOffset) >= 128)
			++delta;
			
		delta -= deltaOffset;
		nextNumber = node->getNumber();
		}

	// If new node doesn't fit on page, split the page now

	if (length + delta > dbb->pageSize)
		{
		if ((char*) node->nextNode < (char*) this + length)
			return SplitMiddle;

		if (node->getNumber() == END_LEVEL)
			return SplitEnd;

		if (offset2 == -1 || nextKey->compare(indexKey) >= 0)
			return NextPage;

		return SplitEnd;
		}

	/***
	Log::debug ("insert index %d, record %d, page offset %d\n",
			index, recordNumber, (CHAR*) node - (CHAR*) page);
	***/

	// Delete supernode at the insertion point, if there was one
	// It might be reinstalled later
	deleteSupernode(node->node);

	// Slide tail of bucket forward to make room

	if (offset2 >= 0)
		{
		UCHAR *tail = (UCHAR*) node->nextNode;
		int tailLength = (int) ((UCHAR*) this + length - tail);
		ASSERT (tailLength >= 0);
		
		if (tailLength > 0)
			moveMemory(tail + delta, tail);
		}

	// Insert new node

	IndexNode newNode;

	if (makeSuper)
		addSupernode(node->node);

	newNode.insert (node->node, offset1, length1, key, recordNumber);

	// If necessary, rebuild next node
	if (makeNextSuper)
		addSupernode(newNode.nextNode);

	if (offset2 >= 0)
		newNode.insert (newNode.nextNode, offset2, nextKey->keyLength - offset2, nextKey->key, nextNumber);




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
	Btn *super;


	if (indexKey->keyLength == 0)
		{
		if (foundKey)
			foundKey->keyLength = 0;

		return nodes;
		}

	bool hitSupernode;
	super = findSupernode(level, key, keyEnd-key, 0, 0, &hitSupernode);

	IndexNode node(super);

	if (hitSupernode)
		goto exit;

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
		{
		if (hitSupernode) /* hit on left supernode, find prior key*/
			findPriorNodeForSupernode(node.node,foundKey);
		else
			foundKey->keyLength = priorLength;
		}

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
	
	int delta = (int)(node.nextNode - nodes);
	if (tailLength < 0)
		{
		node.parseNode(nodes);
		tailLength = (int) (((UCHAR*) this + length) - (UCHAR*) node.nextNode);
		delta = (int) (node.nextNode - nodes);
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
	int newNodeSize = IndexNode::nodeLength(0, kLength, recordNumber);

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
	


	// Split supernodes
	memset(split->superNodes,0, sizeof(split->superNodes));
	int i = 0;
	for (i = 0; i < SUPERNODES && superNodes[i]; i++) 
 		if(nodes + superNodes[i] >= node.node)
			break;

	int j = 0;
	for (; i < SUPERNODES && superNodes[i]; i++)
		{
		short newVal = superNodes[i] - delta + newNodeSize;
		ASSERT(newVal >=0);
		if(newVal == 0)
			continue;
		split->superNodes[j++] = newVal;
		superNodes[i] = 0;
		}

	//validate(NULL);
	//split->validate(NULL);

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
	UCHAR *key = searchKey->key;

	bool hitSupernode;
	Btn *super = findSupernode(level, key, keyEnd - key , searchRecordNumber, 0, &hitSupernode);

	if(hitSupernode)
		return super;

	Btn *prior = super;

	for (IndexNode node(super) ; node.node < bucketEnd ; prior = node.node, node.getNext(bucketEnd))
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
	int priorKeyLength = 0;
	int keyLength;
	int recordNumber;
	int priorRecordNumber=0;
	int superIdx = 0;
	int supernodeCount = 0;


	for (int i=0; i< SUPERNODES; i++)
	{
		if(superNodes[i] ==0)
			{
			for(;i<SUPERNODES;i++)
				if(superNodes[i] != 0)
					FATAL("Index validate: unexpected non-zero supernode index");
			break;
			}
		if (superNodes[i] < 0)
			FATAL ("Index validate: negative supernode");

		if (i> 0 && superNodes[i-1] >= superNodes[i])
			FATAL ("Index validate: entries in supernode array not increasing");

		if (nodes + superNodes[i] > bucketEnd)
			FATAL ("Index validate: superNodes behind the end of page");

		supernodeCount++;
	}


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

		bool isSuper = false;
		if (superIdx < SUPERNODES && superNodes[superIdx])
		{
			int delta = superNodes[superIdx] - (short) (node.node - nodes);
			if(delta == 0)
				{
				superIdx++;
				isSuper=true;
				if(node.offset != 0)
					FATAL ("Non-zero offset in supernode");
				}
			else if( delta < 0)
				FATAL ("Index validate: next supernode offset < current offset");
		}

		keyLength = node.keyLength();
		if (level > 0)
			keyLength -= node.getAppendedRecordNumberLength();

		if(isSuper && keyLength==0)
			FATAL("Supernode with 0 key length");

		int l = MIN(priorKeyLength, keyLength);
		if  (level == 0)
			recordNumber = node.getNumber();
		else
			recordNumber = node.getAppendedRecordNumber();

		if (node.number == END_BUCKET)
			{
			ASSERT(nextPage != 0);
			break;
			}

		if (node.number == END_LEVEL)
			break;

		if (node.node != nodes && l>0 && node.length > 0)
			{
			if(node.offset == 0 && node.key[0] == key[0] && !isSuper)
				FATAL("node not correctly prefix-compressed");
			}


		if(node.length == 0 && recordNumber < priorRecordNumber)
			FATAL ("Index validate:  record numbers out of order");


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

			node.expandKey(key);
			priorKeyLength = keyLength;
			priorRecordNumber = recordNumber;
		}
		
	if (superIdx != supernodeCount)
		FATAL("lost supernodes");
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
			
			deleteSupernode(node.node);

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
				
				// Check whether next node is a supernode.
				// If yes, delete it for now. Supernode will be reinstalled
				// at current position, if it makes sense.

				Btn *supernodePosition = 0;

				if(deleteSupernode(node.nextNode))
					{
					if (checkAddSuperNode(dbb->pageSize, &node, &nextKey, next.getNumber(), prefix, 0))
						{
						prefix = 0;
						supernodePosition = node.node;
						}
					}

				int32 num = next.getNumber();
				node.insert(node.node, prefix, nextKey.keyLength - prefix, nextKey.key, num);

				// Compute length of remainder of bucket (no compression issues)

				Btn *newTail = node.nextNode;
				
				if (tailLength > 0)
					moveMemory(newTail, tail);

				if (supernodePosition)
					addSupernode(supernodePosition);

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

	Log::debug("Supernodes:");
	for (int i=0; i < SUPERNODES && page->superNodes[i]; i++)
		Log::debug("%d ",page->superNodes[i]);
	Log::debug("\n");

	int superIdx = 0;

	for (; node.node < end; node.getNext(end))
		{
		bool isSuper = false;
		if (superIdx < SUPERNODES && (node.node - page->nodes == page->superNodes[superIdx]) )
				{
				superIdx++;
				isSuper = true;
				}
		Log::debug ("   %d%s. offset %d, length %d, %d: ",
			(char*) node.node - (char*) page, (isSuper ? "(super)" : ""),
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

	int length = (int)((UCHAR*) node.node - (UCHAR*) page);

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


	Btn *splitPos;
	if (nextNextNode >= (char*) this + dbb->pageSize)
		{
		splitPos = priorNode;
		node.parseNode(priorNode);
		int number = node.getNumber();
		//node.setNumber(END_BUCKET);
		node.insert(node.node, node.offset, node.length, tempKey.key, END_BUCKET);
		length = (int) ((char*) node.nextNode - (char*) this);
		split->appendNode(&tempKey, number, dbb->pageSize);
		}
	else
		{
		splitPos = node.node;
		int offset = computePrefix(&tempKey, insertKey);
		deleteSupernode(node.node);
		node.insert(node.node, offset, insertKey->keyLength - offset, insertKey->key, END_BUCKET);
		length = (int) ((char*)(node.nextNode) - (char*) this);
		}

	for(int i=0; i< SUPERNODES; i++)
		if(nodes + superNodes[i] >= splitPos)
			superNodes[i]=0;

	//validate(NULL);
	split->appendNode(insertKey, recordNumber, dbb->pageSize);
	split->appendNode(&rolloverKey, rolloverNumber, dbb->pageSize);
	//split->validate(NULL);
	
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
												 indexPage->length - OFFSET (IndexPage*, superNodes), 
												 (const UCHAR*) indexPage->superNodes);
}
 
Btn* IndexPage::findInsertionPoint(int level, IndexKey* indexKey, int32 recordNumber, IndexKey* expandedKey, Btn* from, Btn* bucketEnd)
{
	UCHAR *key = indexKey->key;
	const UCHAR *keyEnd = key + indexKey->keyLength;
	uint matchedOffset = 0;
	uint priorLength = 0;
	IndexNode node;
	bool hitSupernode;

	if (level && indexKey->recordNumber)
		keyEnd -= indexKey->getAppendedRecordNumberLength();

	Btn *super = findSupernode(level, key, keyEnd - key,
		(level > 0)?indexKey->recordNumber:recordNumber ,from, &hitSupernode);

	if (hitSupernode)
		{
		node.parseNode(super);
		goto exit;
		}

	if (super > from)
		node.parseNode(super);
	else
		node.parseNode(from);

	if (node.node != nodes)
		{
		node.expandKey (expandedKey);
		priorLength = expandedKey->keyLength;
		if (level)
			expandedKey->recordNumber = node.getAppendedRecordNumber();
		}

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

	if (expandedKey && hitSupernode)
		{
		/* hit on start supernode, find prior key */
		findPriorNodeForSupernode(node.node,expandedKey);
		}

	return node.node;
}

Btn* IndexPage::getEnd(void)
{
	return (Btn*) ((UCHAR*) this + length);
}


/* During node insertion, check whether supernode should be added at insertion point */
bool IndexPage::checkAddSuperNode(int pageSize, IndexNode* node, IndexKey *indexKey,
				int recordNumber, int offset, bool *makeNextSuper)
{


	Btn *insertionPoint = node->node;

	if (makeNextSuper)
		*makeNextSuper = false;

	if (insertionPoint == nodes) 
		{

		// Make supernode at the following node, if the
		// gap from the start of the page to the first supernode is too large.
		if (makeNextSuper && superNodes[0] > pageSize/(SUPERNODES+1) && !superNodes[SUPERNODES-1])
			*makeNextSuper = true;

		return false; // no supernode at the start of the page
		}

	if (offset >= SUPERNODE_COMPRESSION_LOSS_LIMIT(pageSize)) //compression is too good, no super
		return false;

	if (superNodes[SUPERNODES-1]) // supernode array is full
		return false;

	int keyLen = indexKey->keyLength;

	if (level)
		keyLen -= indexKey->getAppendedRecordNumberLength();

	if (keyLen == 0)
		return false;

	int position = (int)(insertionPoint - nodes);

	int i;
	// Find superNodes corresponding to insertion point
	for(i = 0; i< SUPERNODES;i++)
		{
		if (!superNodes[i])
			break;
		if (nodes + superNodes[i] >= insertionPoint)
			break;
		}

	// Avoid 2 superNodes adjacent to each other
	if (nodes + superNodes[i] == insertionPoint)
		{
		// Retain existing supernode
		if (makeNextSuper)
			*makeNextSuper = true;
		return false;
		}

	int nodeLen =
		IndexNode::nodeLength(offset, indexKey->keyLength - offset, recordNumber);
	
	/* Ideal distance between supernodes*/
	int idealDistance = pageSize/(SUPERNODES+1);

	
	int distLeft, distRight;

	distLeft = position;

	if (i > 0)
		distLeft -= superNodes[i-1];

	// last node, make supernode, if distance from prior >= idealDistance
	if(superNodes[i] == 0)
		{
		if (distLeft >= idealDistance)
			return true;
		return false;
		}

	// between two supernodes, make new one , if distance 
	// from both neighbours is at least idealSize*0.5

	distRight = 
		superNodes[i] + nodeLen - position;
	ASSERT(distRight >=0 );

	// Distance between existing supernodes
	int d= (i > 0)? superNodes[i] - superNodes[i-1] + nodeLen :superNodes[0] + nodeLen;

	if ( d >= idealDistance*3/2 && distLeft >= idealDistance/2 && distRight >= idealDistance/2 )
		return true;

	return false;
}

// Helper routine, used with findNodeInLeaf/Branch and findInsertionPoint
Btn* 	IndexPage::findPriorNodeForSupernode(Btn *where,IndexKey *priorKey)
{
	Btn *prior = nodes;
	bool found = false;


	// Scan supernode array for prior super
	for(int i=0; i < SUPERNODES && superNodes[i]; i++)
		{
		Btn* current = nodes + superNodes[i];
		if(current == where)
			{
			found = true;
			break;
			}
		prior = current;
		}

	ASSERT(found);

	IndexNode priorNode(prior);
	for (;; priorNode.getNext(where))
		{
		ASSERT(priorNode.nextNode <= where);
		priorNode.expandKey(priorKey);
		if (priorNode.nextNode == where)
			return priorNode.node;
		}

	ASSERT(false);
}


// Move nodes, adjust supernode array
void IndexPage::moveMemory(void *dst,void *src)
{
	UCHAR* end = (UCHAR *)getEnd();
	size_t size = (dst > src)?(end - (UCHAR *)src) : (end-(UCHAR *)dst);

	memmove(dst, src, size);

	short delta = (short)((UCHAR *)dst - (UCHAR *)src);

	for(int i=0; i< SUPERNODES && superNodes[i]; i++)
		if(nodes + superNodes[i] >= src)
			superNodes[i] += delta;
}

// Add entry to supernode array
void IndexPage::addSupernode(Btn *where)
{
	int i;
	short offset = (short)((UCHAR *)where - (UCHAR *)nodes);
	for (i=0; i< SUPERNODES && superNodes[i] ;i++)
		{
		ASSERT(superNodes[i] != offset);
		if(superNodes[i] > offset)
			{
			if(i != SUPERNODES -1)
				memmove(&superNodes[i+1],&superNodes[i], (SUPERNODES-i-1)*sizeof(superNodes[0]));
			break;
			}
		}
	ASSERT(i < SUPERNODES);
	superNodes[i] = (short)((UCHAR *)where - (UCHAR*)nodes);
}


// Delete entry from supernode array.
// Returns true if supernode was present at given position and false otherwise
bool IndexPage::deleteSupernode(Btn *where)
{
	short offset = (short)((UCHAR *)where - (UCHAR *)nodes);
	ASSERT(where > 0);
	for (int i=0; i< SUPERNODES && superNodes[i];i++)
		{
		if(superNodes[i] == offset)
			{
			if (i !=  SUPERNODES -1)
				memmove(&superNodes[i],&superNodes[i+1], (SUPERNODES-i-1)*sizeof(superNodes[0]));
			superNodes[SUPERNODES-1] = 0;
			return true;
			}
		}
	return false;
}


/* compare supernode to given key. Helper function, used for binary search in findSupernode*/

static int compareSuper(IndexNode *node, UCHAR *key, size_t keylen, int recordNumber, int level)
{
	int result;

	int len = node->keyLength();
	if (level)
		len -= node->getAppendedRecordNumberLength();
	ASSERT(len >= 0);

	result = memcmp(node->key, key, MIN((int)keylen,len));

	if (result == 0)
		result = len - (int)keylen;

	if (result == 0)
		{
		int number= (level == 0)? node->number : node->getAppendedRecordNumber();
		if ( number < 0 || recordNumber == 0)
			result = 1;
		else
			result = number - recordNumber;
		}
	return result;
}

// Given a key, find the maximum supernode smaller or equal to the search key
// Optional parameter "after" tells to skip supernodes smaller than its value
// Binary search is used for better speed
Btn * IndexPage::findSupernode(int level, UCHAR *key, size_t keylen, int32 recordNumber, Btn *after, bool *found)
{

	*found = false;
	if (superNodes[0] == 0)
		return nodes;

	int low = 0;
	int high = SUPERNODES;
	while (low < high)
		{
		int mid = (low + high)/2;

		int result;
		if (superNodes[mid] == 0)
			result = 1; // after last supernode
		else
			{
			Btn *pos = nodes +superNodes[mid];
			if (after && (pos < after))
				result = -1;
			else 
				{
				IndexNode node(pos);
				result = compareSuper(&node, key, keylen, recordNumber, level);
				}
			}

		if (result == 0)
			{
			low = mid;
			*found = true;
			break;
			}
		else if (result < 0)
			low = mid + 1; 
		else
			high = mid; 
		}

	if (low == SUPERNODES)
		return nodes + superNodes[SUPERNODES-1];

	if (!(*found))
		{	
		IndexNode node(nodes + superNodes[low]);
		*found = (compareSuper(&node, key, keylen, recordNumber, level) == 0);
		}

	if (*found)
		return nodes + superNodes[low];

	if (low == 0)
		return nodes;
	
	return nodes + superNodes[low-1];
}


