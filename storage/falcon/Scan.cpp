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

// Scan.cpp: implementation of the Scan class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include <string.h>
#include "Engine.h"
#include "Inversion.h"
#include "Dbb.h"
#include "BDB.h"
#include "InversionPage.h"
#include "Scan.h"
#include "Database.h"
#include "Log.h"
#include "SearchWords.h"
#include "IndexKey.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


Scan::Scan(SearchWords *words)
{
	bdb = NULL;
	next = NULL;
	type = Simple;
	eof = false;
	frequentWord = false;
	frequentTail = false;
	hits = 0;
	searchWords = words;
}

Scan::Scan(ScanType typ, const char * word, SearchWords *words)
{
	searchWords = words;
	bdb = NULL;
	next = NULL;
	type = typ;
	frequentWord = false;
	frequentTail = false;
	setKey (word);
	eof = false;
	hits = 0;
}

Scan::~Scan()
{
	fini();

	if (next)
		delete next;
}

bool Scan::start(Database *database)
{
	if (next && next->start (database))
		{
		fini();
		return eof;
		}

	dbb = database->dbb;
	inversion = dbb->inversion;
	memset (numbers, 0, sizeof (numbers));

	if (frequentWord)
		{
		if (next)
			{
			memcpy (numbers, next->numbers, sizeof (numbers));
			--numbers [3];
			eof = next->eof;
			frequentTail = next->frequentTail;
			}
		else
			frequentTail = true;

		return eof;
		}

	IndexKey indexKey(keyLength, key);

	if (!(bdb = inversion->findInversion (&indexKey, Shared)))
		{
		eof = true;
		return eof;
		}

	page = (InversionPage*) bdb->buffer;

	if (dbb->debug & (DEBUG_PAGES | DEBUG_SCAN_INDEX))
		page->printPage (bdb);

	node = page->findNode (keyLength, key, expandedKey, &expandedLength);

	if (fetch ())
		{
		if (next && !next->frequentTail)
			while (next->compare (4, numbers) <= 0)
				if (!next->getNext ())
					break;
		if (!hit())
			getNext ();
		}

	return eof;
}

void Scan::fini()
{
	eof = true;

	if (bdb)
		{
		bdb->release(REL_HISTORY);
		bdb = NULL;
		}

	if (next)
		next->fini();
}

int32* Scan::getNext()
{
	if (frequentWord)
		{
		if (next && !next->frequentTail)
			{
			int32* ret = next->getNext();
			if (!ret)
				{
				eof = true;
				return ret;
				}
			memcpy (numbers, ret, sizeof (numbers));
			--numbers [3];
			}
		return numbers;
		}

	for (;;)
		{
		node = NEXT_INV (node);
		int32 *result = fetch();
		if (!result)
			break;
		if (next && !next->frequentTail)
			{
			if (next->eof)
				return NULL;
			while (next->compare (4, numbers) <= 0)
				if (!next->getNext ())
					{
					fini();
					return NULL;
					}
			}
		if (hit())
			return result;
		}

	return NULL;
}


int32* Scan::fetch ()
{
	if (!bdb)
		return NULL;

	// If advance is indicated, find next node, advancing to next
	// page if necessary

	while (node >= (Inv*) ((char*) page + page->length))
		{
		if (!page->nextPage)
			{
			fini();
			return NULL;
			}
		bdb = dbb->handoffPage (bdb, page->nextPage, PAGE_inversion, Shared);
		BDB_HISTORY(bdb);
		page = (InversionPage*) bdb->buffer;
		node = page->nodes;
		}

	// Expand compressed key

	expandedLength = node->offset + node->length;
	ASSERT (expandedLength <= MAX_INV_KEY);
	memcpy (expandedKey + node->offset, node->key, node->length);

	// Validate that we're on the same word

	UCHAR *p = expandedKey;
	UCHAR *q = key;

	if (rootEnd)
		{
		while (q < rootEnd && (*p || *q))
			if (*p++ != *q++)
				{
				fini();
				return NULL;
				}
		while (*p)
			++p;
		}
	else
		{
		while (*p || *q)
			if (*p++ != *q++)
				{
				fini();
				return NULL;
				}
		}

	++p;
	++hits;

	// Parse recordId, fieldId, recordNumber, and word number

	for (int n = 0; n < 4; ++n)
		numbers [n] = Inv::decode (&p);

	// Pick up case mask

	UCHAR *end = expandedKey + expandedLength;

	if (p < end)
		caseMask = *p++;
	else
		caseMask = 0;

	return numbers;
}

bool Scan::hit(int32 wordNumber)
{
	if (!hit())
		return false;

	if (numbers [WORD_NUMBER] != wordNumber)
		return false;

	if (next)
		return next->hit (wordNumber + 1);

	return true;
}

void Scan::print()
{
	Log::debug ("%s tbl %d, rec %d, fld %d, word %d, mask %x, hit %d\n",
			key, numbers [0], numbers [1], numbers [2], numbers [3],
			caseMask, hit());
}

bool Scan::hit()
{
	if (!frequentWord)
		{
		if (!bdb)
			return false;

		if (searchMask & ~caseMask)
			return false;
		}

	if (next && !next->frequentTail)
		{
		if (next->compare (3, numbers) != 0)
			return false;
		return next->hit (numbers [WORD_NUMBER] + 1);
		}

	return true;
}

int Scan::compare(int count, int32 * nums)
{
	if (eof)
		return false;

	for (int n = 0; n < count; ++n)
		if (nums [n] != numbers [n])
			return (numbers [n] < nums [n]) ? -1 : 1;

	return 0;
}

void Scan::setKey(const char * word)
{
	UCHAR *p = key;
	char c;
	searchMask = 0;
	rootEnd = NULL;

	for (int n = 0; (c = word [n]); ++n)
		{
		char c = word [n];

		if (c == '*')
			{
			rootEnd = p;
			break;
			}

		if (ISUPPER (c))
			{
			searchMask |= 1 << n;
			c -= 'A' - 'a';
			}

		*p++ = c;
		}

	*p = 0;
	keyLength = (int) (p - key);
	frequentWord = searchWords->isStopWord ((const char*) key);		
}


void Scan::addWord(const char *word)
{
	if (next)
		next->addWord (word);
	else
		next = new Scan (Tail, word, searchWords);
}

int32* Scan::getNumbers()
{
	if (!frequentWord)
		return numbers;

	if (next)
		return next->getNumbers();

	NOT_YET_IMPLEMENTED;

	return NULL;
}

bool Scan::validate(Scan *prior)
{
	if (!frequentWord)
		{
		if (next)
			return next->validate (this);
		return true;
		}

	if (prior && frequentTail)
		{
		memcpy (numbers, prior->numbers, sizeof (numbers));
		++numbers [3];
		}

	// If there is a prior node, pick up numbers

	UCHAR *p = key + keyLength;
	*p++ = 0;

	for (int n = 0; n < 4; ++n)
		Inv::encode (numbers [n], &p);

	int length = (int) (p - key);
	IndexKey indexKey(length, key);
	Bdb *bdb = inversion->findInversion (&indexKey, Shared);

	if (!bdb)
		return false;

	InversionPage *page = (InversionPage*) bdb->buffer;

	if (dbb->debug & (DEBUG_PAGES | DEBUG_SCAN_INDEX))
		page->printPage (bdb);

	node = page->findNode (length, key, expandedKey, &expandedLength);
	bool eob = (char*) node >= (char*) page + page->length;
	bdb->release(REL_HISTORY);

	if (eob)
		return false;

	expandedLength = node->offset + node->length;
	ASSERT (expandedLength <= MAX_INV_KEY);
	memcpy (expandedKey + node->offset, node->key, node->length);

	if (length != expandedLength)
		return false;

	if (memcmp (key, expandedKey, length))
		return false;

	if (next)
		return next->validate (this);

	return true;
}

int32* Scan::getNextValid()
{
	for (;;)
		{
		int32 *ret = getNext();
		if (!ret)
			return ret;
		if (validate (NULL))
			return ret;
		}
}

bool Scan::validateWord(int32 * wordNumbers)
{
	// Build a search key

	UCHAR *p = key + keyLength;
	*p++ = 0;

	for (int n = 0; n < 3; ++n)
		Inv::encode (wordNumbers [n], &p);

	int length = (int) (p - key);
	IndexKey indexKey(length, key);

	if (!(bdb = inversion->findInversion (&indexKey, Shared)))
		return false;

	page = (InversionPage*) bdb->buffer;

	if (dbb->debug & (DEBUG_PAGES | DEBUG_SCAN_INDEX))
		page->printPage (bdb);

	node = page->findNode (length, key, expandedKey, &expandedLength);

	if (!fetch())
		return false;

	bdb->release(REL_HISTORY);
	bdb = NULL;

	if (compare (3, wordNumbers))
		return false;

	if (next)
		return next->validateWord (wordNumbers);

	return true;
}
