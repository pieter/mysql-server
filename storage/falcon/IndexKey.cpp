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

// IndexKey.cpp: implementation of the IndexKey class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "IndexKey.h"
#include "Index.h"

#define SHIFT(n)	((uint64) 1 << n)

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IndexKey::IndexKey(Index *idx)
{
	index = idx;
	keyLength = 0;
	recordNumber = 0;
}

IndexKey::IndexKey(IndexKey *indexKey)
{
	index = indexKey->index;
	keyLength = indexKey->keyLength;
	recordNumber = indexKey->recordNumber;
	memcpy (key, indexKey->key, keyLength);
}

IndexKey::IndexKey()
{
	index = NULL;
	keyLength = 0;
	recordNumber = 0;
}

IndexKey::IndexKey(int length, const unsigned char *indexKey)
{
	index = NULL;
	keyLength = length;
	memcpy(key, indexKey, length);
	recordNumber = 0;
}

IndexKey::~IndexKey()
{

}

void IndexKey::setKey(IndexKey *indexKey)
{
	index = indexKey->index;
	keyLength = indexKey->keyLength;
	memcpy (key, indexKey->key, keyLength);
}


void IndexKey::setKey(int length, const unsigned char* keyPtr)
{
	keyLength = length;
	memcpy(key, keyPtr, keyLength);
}

bool IndexKey::isEqual(IndexKey *indexKey)
{
	if (indexKey->keyLength != keyLength)
		return false;

	return memcmp(key, indexKey->key, keyLength) == 0;
}


/*	This deceptively simple little method transforms a
	double precision number from its natural state to
	a format that compares correctly bytewise.  It
	starts with a flexible union of a double, quad,
	and char[8] and assigns the incoming key value
	to that structure and sets up a pointer to the
	end of the current key.
*/

void IndexKey::appendNumber(double number)
{
	union {
		double	dbl;
		int64	quad;
		unsigned char	chars [8];
		} stuff;

	stuff.dbl = number;
	unsigned char *p = key + keyLength;

	// These lines set a sign bit at the beginning of positive
	// values making all of them bigger than any negative value
	// and inverts negative values so they sort correctly as well.

	if (stuff.quad >= 0)
		stuff.quad ^= QUAD_CONSTANT(0x8000000000000000);
	else
		stuff.quad ^= QUAD_CONSTANT(0xffffffffffffffff);

	// This loop inverts the mantissa and exponent on little-endian
	// platforms.  Note that this won’t work on big-endian machines!

	for (unsigned char *q = stuff.chars + 8; q > stuff.chars; )
		*p++ = *--q;

	// This loop truncates trailing zeros in the mantissa.

	while (p > key && p [-1] == 0)
		--p;

	// Finally, readjust the length of the key being built.

	keyLength = (int) (p - key);
}

void IndexKey::appendRecordNumber(int32 recordNum)
{
	recordNumber = recordNum;

	int len = (recordNumber < (int32) SHIFT(8))  ? 2 :
	          (recordNumber < (int32) SHIFT(16)) ? 3 :
	          (recordNumber < (int32) SHIFT(24)) ? 4 : 5;

	int shift = len * 8 - 12;	
	key[keyLength++] = (len << 4) | ((recordNumber >>shift) & 0x0f);
	
	for (shift -= 8; shift > 0; shift -= 8)
		key[keyLength++] = recordNumber >> shift;
	
	key[keyLength++] = (recordNumber << 4) | len;

#ifdef EXTRA_DEBUG_KEYS
	ASSERT(getNumberLength() == len);
	const UCHAR *p = key + keyLength - len;
	int32 value = *p++ & 0x0f;
	
	for (int length = len; length > 2; --length)
		value = (value << 8) | *p++;
	
	value = (value << 4) | (*p >> 4);
	ASSERT(value == recordNumber);
#endif
}

int IndexKey::checkTail(const unsigned char* endPoint)
{
	for (const UCHAR *p = endPoint, *end = key + keyLength; p < end; ++p)
		if (*p)
			return 1;
	
	return -1;
}

void IndexKey::setKey(int offset, int length, const UCHAR *data)
{
	memcpy(key + offset, data, length);
	length = offset + length;
}

// Return whether key sent in is >, ==, < than this node.

int IndexKey::compareValues(IndexKey* indexKey)
{
	bool isPartial = false;
	int len2 = indexKey->keyLength;
	if (indexKey->recordNumber)
		len2 -= indexKey->getAppendedRecordNumberLength();
	return compareValues(indexKey->key, len2, isPartial);
}

int IndexKey::compareValues(UCHAR *key2, uint len2, bool isPartial)
{
	// First, compare value length
	uint len1 = keyLength;
	if (recordNumber)
		len1 -= getAppendedRecordNumberLength();

	uint len = MIN(len1, len2);

	int ret = memcmp(key2, key, len);
	if (ret == 0 && !isPartial)
		ret = len2 - len1;

	return ret;

}

int IndexKey::compare(IndexKey* indexKey)
{
	int ret = compareValues(indexKey);
	if (ret)
		return ret;

	// values are equal, compare record numbers.

	return indexKey->getRecordNumber() - recordNumber;
}

int32 IndexKey::getRecordNumber()
{
	return recordNumber;
}
