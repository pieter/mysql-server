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

// InversionPage.cpp: implementation of the InversionPage class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "InversionPage.h"
#include "InversionWord.h"
#include "IndexPage.h"
#include "Dbb.h"
#include "BDB.h"
#include "Validation.h"
#include "Bitmap.h"
#include "Log.h"
#include "SerialLog.h"
#include "SerialLogControl.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


bool InversionPage::addNode(Dbb * dbb, IndexKey *indexKey)
{
	int keyLength = indexKey->keyLength;
	UCHAR *key = indexKey->key;
	UCHAR	nextKey [MAX_PHYSICAL_KEY_LENGTH], priorKey [MAX_PHYSICAL_KEY_LENGTH];
	int		priorLength;
	Inv *node = findNode (keyLength, key, priorKey, &priorLength);

	/* We got the prior key value.  Get key value for insertion point */

	memcpy (nextKey, priorKey, priorLength);
	int nextLength = node->offset + node->length;

	if ((char*) node < (char*) this + length)
		memcpy (nextKey + node->offset, node->key, node->length);
	else
		nextLength = 0;

	/* Compute delta size of new node */

	int l1 = computePrefix (priorLength, priorKey, keyLength, key);

	/* If this node is identical to the previous, don't do nothin! */

	if (l1 == keyLength)
		{
		ASSERT (false);
		return true;
		}

	int l2;
	int delta = INV_SIZE + keyLength - l1;
	ASSERT (delta > 0);

	if ((UCHAR*) node == (UCHAR*) this + length)
		l2 = -1;
	else
		{
		l2 = computePrefix (keyLength, key, node->offset + node->length, nextKey);
		ASSERT (MIN (l1, l2) <= node->offset);
		delta -= l2 - node->offset;
		}
	
	if (l2 == keyLength && l2 == (node->offset + node->length))
		return true;
				
	/* If new node doesn't fit on page, split the page now */

	if (length + delta > dbb->pageSize)
		return false;

	/* Slide tail of bucket forward to make room */

	if (l2 >= 0)
		{
		UCHAR *tail = (UCHAR*) NEXT_INV (node);
		int tailLength = (int) ((UCHAR*) this + length - tail);
		ASSERT (tailLength >= 0);

		if (tailLength > 0)
			memmove (tail + delta, tail, tailLength);
		}

	/* Insert new node */

	node->offset = l1;
	node->length = keyLength - l1;
	memcpy (node->key, key + l1, node->length);

	/* If necessary, rebuilt next node */

	if (l2 >= 0)
		{
		node = NEXT_INV (node);
		node->offset = l2;
		node->length = nextLength - l2;
		memcpy (node->key, nextKey + l2, nextLength - l2);
		}

	length += delta;
	//validate();

	return true;
}

int InversionPage::computePrefix(int l1, UCHAR * v1, int l2, UCHAR * v2)
{
	int		n, max;

	for (n = 0, max = MIN (l1, l2); n < max; ++n)
		if (*v1++ != *v2++)
			return n;

	return n;
}

Inv* InversionPage::findNode(int keyLength, UCHAR * key, UCHAR * expandedKey, int * expandedLength)
{
	int offset = 0;
	int priorLength = 0;
	UCHAR *keyEnd = key + keyLength;
	Inv *bucketEnd = (Inv*) ((char*) this + length);
	Inv *node;

	for (node = nodes;; node = NEXT_INV (node))
		{
		if (node >= bucketEnd)
			{
			if (node != bucketEnd)
				{
				printPage (0);
				FATAL ("InversionPage findNode: trashed inversion page");
				}

			if (expandedLength)
				*expandedLength = priorLength;

			return bucketEnd;
			}

		int l = node->length;

		if (node->offset < offset)
			break;

		if (node->offset > offset || l == 0)
			continue;

		UCHAR *p = key + node->offset;
		UCHAR *q = node->key;
		UCHAR *nodeEnd = q + l;

		for (;;)
			{
			if (p == keyEnd)
				goto exit;

			if (q == nodeEnd || *p > *q)
				break;

			if (*p++ < *q++)
				goto exit;
			}

		offset = (int) (p - key);

		if (expandedKey && l)
			{
			memcpy (expandedKey + node->offset, node->key, l);
			priorLength = l + node->offset;
			}
		}

	exit:

	if (expandedLength)
		*expandedLength = priorLength;

	return node;
}

Bdb* InversionPage::splitInversionPage(Dbb * dbb, Bdb * bdb, IndexKey *indexKey, TransId transId)
{
	Inv *midpoint = (Inv*) ((UCHAR*) nodes + 
					  (dbb->pageSize - OFFSET (InversionPage*, nodes)) / 2);

	/* Find midpoint */

	Inv *node;
	UCHAR *key = indexKey->key;

	for (node = nodes;; node = NEXT_INV (node))
		{
		ASSERT (node->offset + node->length <= MAX_INV_KEY);
		memcpy (key + node->offset, node->key, node->length);

		if (node > midpoint && (node->offset || node->length))
			break;
		}

	int kLength = indexKey->keyLength = node->offset + node->length;

	/* Allocate and format new page.  Link forward and backward to
	   siblings. */

	Bdb *splitBdb = dbb->allocPage (PAGE_inversion, transId);
	BDB_HISTORY(splitBdb);
	InversionPage *split = (InversionPage*) splitBdb->buffer;
	split->priorPage = bdb->pageNumber;
	split->parentPage = parentPage;

	if ((split->nextPage = nextPage))
		{
		Bdb *nextBdb = dbb->fetchPage (split->nextPage, PAGE_inversion, Exclusive);
		BDB_HISTORY(bdb);
		InversionPage *next = (InversionPage*) nextBdb->buffer;
		nextBdb->mark(transId);
		next->priorPage = splitBdb->pageNumber;
		nextBdb->release(REL_HISTORY);
		}

	nextPage = splitBdb->pageNumber;
	//dbb->serialLog->logControl->inversionPage.append(splitBdb->pageNumber, parentPage, split->priorPage, nextPage);

	/* Copy midpoint node to new page.  Compute length, then
	   copy tail of page to new page */

	Inv *newNode = split->nodes;
	newNode->offset = 0;
	newNode->length = (UCHAR) kLength;
	memcpy (newNode->key, key, kLength);
	int tailLength = (int) (((UCHAR*) this + length) - (UCHAR*) NEXT_INV (node));
	memcpy (NEXT_INV (newNode), NEXT_INV (node), tailLength);

	length = (int) ((UCHAR*) node - (UCHAR*) this);
	//validate();
	split->length = (int) ((newNode->key + newNode->length + tailLength) - (UCHAR*) split);
	//split->validate();

	return splitBdb;
}

void InversionPage::printPage(Bdb *bdb)
{
	UCHAR	key [MAX_PHYSICAL_KEY_LENGTH];

	Log::debug ("Inversion Page %d length %d, prior %d, next %d, parent %d\n", 
			((bdb) ? bdb->pageNumber : 0),
			length,
			priorPage,
			nextPage,
			parentPage);

	Inv *end = (Inv*) ((UCHAR*) this + length);
    Inv *node;

	for (node = nodes; node < end; node = NEXT_INV (node))
		{
		Log::debug ("   %x. offset %d, length %d:\t",
				(char*) node - (char*) this, 
				node->offset, node->length);
		if (node->offset + node->length >= sizeof (key))
			{
			Log::debug ("**** bad inversion node ****\n");
			return;
			}
		memcpy (key + node->offset, node->key, node->length);
		node->printKey (key);
		Log::debug ("\n");
		}

	int len = (int) ((UCHAR*) node - (UCHAR*) this);

	if (len != length)
		{
		Log::debug ("**** back bucket length -- should be %d ***\n", length);
		}
}

void Inv::printKey(UCHAR *expandedKey)
{
	int l = offset + length;
	bool null = false;
    int n;

	for (n = 0; n < l; ++n)
		{
		UCHAR c;
		if (n == offset)
			Log::debug ("*");
		if (n < offset)
			c = expandedKey [n];
		else
			c = key [n - offset];
		if (!c)
			null = true;
		if (!null && (ISLOWER (c) || 
						c == '@' || 
						c == '/' || 
						c == ':' || 
						ISDIGIT (c)))
			Log::debug ("%c", c);
		else
			Log::debug ("\\%x", c);
		}

	UCHAR *p = expandedKey;
	UCHAR *end = p + l;
	int32 numbers [4];

	while (*p++)
		;

	for (n = 0; n < 4; ++n)
		numbers [n] = Inv::decode (&p);

	Log::debug (" (%d,%d,%d,%d)", numbers [0], numbers [1], numbers [2], numbers [3]);

	while (p < end)
		Log::debug (" 0x%x", *p++);
}

void InversionPage::validate()
{
	Inv *bucketEnd = (Inv*) ((char*) this + length);
	Inv *prior = NULL;
	UCHAR expandedKey [128];
	int expandedLength = 0;

	for (Inv *node = nodes;; node = NEXT_INV (node))
		{
		if (node >= bucketEnd)
			{
			if (node != bucketEnd)
				{
				printPage (0);
				FATAL ("Index validate: trashed inversion page");
				}
			break;
			}
		if (node->offset + node->length >= sizeof (expandedKey))
			{
			printPage (0);
			FATAL ("Index validate: trashed inversion node");
			}
		int length = node->length + node->offset;
		if (length <= expandedLength)
			{
			if (node->key [0] == expandedKey [node->offset])
				{
				printPage (0);
				ASSERT (false);
				}
			}
		expandedLength = length;
		memcpy (expandedKey + node->offset,  node->key, node->length);

		ASSERT (node >= nodes && node < bucketEnd);
		prior = node;
		}
}

int32 Inv::decode(UCHAR **ptr)
{
	UCHAR *p = *ptr;
	UCHAR c = *p++;
	int32 number;

	if (c & 0x80)
		{
		number = c &0x1f;
		int count = c >> 5;
		for (int n = 3; n < count; ++n)
			number = number << 8 | *p++;
		}
	else
		number = c & 0x7f;

	*ptr = p;

	return number;
}

void Inv::encode(ULONG number, UCHAR **ptr)
{
	UCHAR *p = *ptr;

	if (number >      0x1fffffff)
		{
		*p++ = (7 << 5);
		*p++ = (UCHAR) (number >> 24);
		*p++ = (UCHAR) (number >> 16);
		*p++ = (UCHAR) (number >> 8);
		*p++ = (UCHAR) number;
		}
	else if (number > 0x1fffff)
		{
		*p++ = (UCHAR) ((6 << 5) | (number >> 24));
		*p++ = (UCHAR) (number >> 16);
		*p++ = (UCHAR) (number >> 8);
		*p++ = (UCHAR) number;
		}
	else if (number > 0x1fff)
		{
		*p++ = (UCHAR) ((5 << 5) | (number >> 16));
		*p++ = (UCHAR) (number >> 8);
		*p++ = (UCHAR) number;
		}
	else if (number > 0x7f)
		{
		*p++ = (UCHAR)((4 << 5) | (number >> 8));
		*p++ = (UCHAR) number;
		}
	else
		{
		*p++ = (UCHAR) number;
		}

	*ptr = p;
}

void InversionPage::validate(Dbb *dbb, Validation *validation, Bitmap *pages)
{
	InversionPage *priorPage = this;
	int32 priorPageNumber = 0;

	for (int32 pageNumber = nextPage; pageNumber;)
		{
		Bdb *bdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
		BDB_HISTORY(bdb);
		if (validation->isPageType (bdb, (PageType) pageType, "inversion page"))
			{
			pages->set (pageNumber);
			validation->inUse (pageNumber, "InversionPage");
			InversionPage *inversionPage = (InversionPage*) bdb->buffer;
			priorPage = inversionPage;
			priorPageNumber = pageNumber;
			pageNumber = inversionPage->nextPage;
			}
		else
			pageNumber = 0;
		bdb->release(REL_HISTORY);
		}
}

void InversionPage::removeNode(Dbb *dbb, int keyLength, UCHAR *key)
{
	//validate();
	//Inv::printKey (keyLength, key);
	UCHAR	nextKey [MAX_PHYSICAL_KEY_LENGTH], priorKey [MAX_PHYSICAL_KEY_LENGTH];
	int		priorLength;
	Inv *node = findNode (keyLength, key, priorKey, &priorLength);
	//Inv::printKey (priorLength, priorKey);

	// Make sure we found the right guy

	if (node->offset + node->length != keyLength)
		return;

	memcpy (nextKey, priorKey, priorLength);
	memcpy (nextKey + node->offset, node->key, node->length);
	//Inv::printKey (keyLength, nextKey);

	if (memcmp (nextKey, key, keyLength) != 0)
		return;

	// Find next node,  If there isn't one, adjust page length, and call it a day

	Inv *next = NEXT_INV (node);
	char *pageEnd = (char*) this + length;

	if ((char*) next >= pageEnd)
		{
		length = (int) ((char*) node - (char*) this);
		//validate();
		return;
		}

	// Compute full key of next remaining node, then new prefix length

	memcpy (nextKey + next->offset, next->key, next->length);
	int nextLength = next->offset + next->length;
	int l1 = computePrefix (priorLength, priorKey, nextLength, nextKey);
	char *tail = (char*) NEXT_INV (next);

	// Rebuild "next" node

	node->offset = l1;
	node->length = nextLength - l1;
	memcpy (node->key, nextKey + l1, nextLength - l1);

	// Slide down everything else

	char *newEnd = (char*) NEXT_INV (node);
	int tailLength = (int) (pageEnd - tail);

	if (tailLength > 0)
		{
		memmove (newEnd, tail, tailLength);
		newEnd += tailLength;
		}

	length = (int) (newEnd - (char*) this);
}

void Inv::printKey(int length, UCHAR *key)
{
    int n;

	for (n = 0; n < length; ++n)
		{
		UCHAR c = key [n];
		
		if (c && (ISLOWER (c) || 
					c == '@' || 
					c == '/' || 
					c == ':' || 
					ISDIGIT (c)))
			Log::debug ("%c", c);
		else
			Log::debug ("\\%x", c);
		}

	UCHAR *p = key;
	int32 numbers [4];

	while (*p++)
		;

	for (n = 0; n < 4; ++n)
		numbers [n] = Inv::decode (&p);

	Log::debug (" (%d,%d,%d,%d)\n", numbers [0], numbers [1], numbers [2], numbers [3]);
}

bool Inv::validate(Inv *node, int keyLength, UCHAR *expandedKey)
{
	UCHAR *p = expandedKey;

	while (*p++)
		;

	for (int n = 0; n < 4; ++n)
		decode (&p);

	if (p - expandedKey > node->offset + node->length)
		{
		Log::debug ("\n*** bad node: ");
		printKey (keyLength, expandedKey);
		Log::debug ("\n");
		return false;
		}

	return true;
}

void InversionPage::analyze(int pageNumber)
{
	int count = 0;
	int prefixTotal = 0;
	int lengthTotal = 0;

	Inv *bucketEnd = (Inv*) ((char*) this + length);

	for (Inv *node = nodes; node < bucketEnd; node = NEXT_INV (node))
		{
		++count;
		prefixTotal += node->length;
		lengthTotal += node->length + node->offset;
		}
	
	Log::debug ("Inversion page %d, %d nodes, %d compressed, %d uncompressed\n",
				pageNumber, count, prefixTotal, lengthTotal);
}

void InversionPage::logPage(Bdb *bdb)
{
	Dbb *dbb = bdb->dbb;

	if (dbb->serialLog->recovering)
		return;

	InversionPage *page = (InversionPage*) bdb->buffer;
	dbb->serialLog->logControl->inversionPage.append(dbb, bdb->pageNumber,
												 page->parentPage, page->priorPage, page->nextPage,  
												 page->length - OFFSET (IndexPage*, nodes), 
												 (const UCHAR*) page->nodes);
}
