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

// IndexNode.cpp: implementation of the IndexNode class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "IndexNode.h"
#include "IndexPage.h"
#include "IndexKey.h"
#include "Btn.h"


#define RECORD_NUMBER_SHIFT(recordNumber) (		\
			(recordNumber < SHIFT(7)) ? 0 :		\
			(recordNumber < SHIFT(14)) ? 7 :	\
			(recordNumber < SHIFT(21)) ? 14 :	\
			(recordNumber < SHIFT(28)) ? 21 : 28)


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IndexNode::IndexNode()
{

}

IndexNode::IndexNode(IndexPage *page)
{
	if (page->length == OFFSET(IndexPage*, nodes))
		{
		node = page->nodes;
		offset = 0;
		length = 0;
		number = 0;
		nextNode = NULL;
		}
	else
		parseNode (page->nodes);
}


IndexNode::IndexNode(Btn *node)
{
	parseNode(node);
}


IndexNode::IndexNode(Btn* indexNode, void* endBucket)
{
	if (indexNode == endBucket)
		{
		node = indexNode;
		number = -1;
		offset = 0;
		length = 0;
		nextNode = NULL;
		}
	else
		parseNode(indexNode);
}

void IndexNode::printKey(const char *msg, UCHAR *key, bool inversion)
{
	node->printKey ("", offset + length, key, offset, inversion);
}

Btn* IndexNode::insert(Btn *where, int offst, int len, UCHAR *fullKey, int32 recordNumber)
{
	node = where;
	key = (UCHAR*) node;
	
	if ( (offset = offst) >= 128)
		*key++ = (UCHAR) ((offset >> 8) | 0x80);
		
	*key++ = (UCHAR) offset;
	
	if ( (length = len) >= 128)
		*key++ = (UCHAR) ((length >> 8) | 0x80);

	*key++ = (UCHAR) length;
	
	if (recordNumber > 0)
		{
		int len = RECORD_NUMBER_LENGTH(recordNumber);
		int shift = (len - 1) * 7;
		
		for (; shift > 0; shift -= 7)
			*key++ = (recordNumber >> shift) & 0x7f;
		
		*key++ = (UCHAR) (recordNumber | 0x80);
		}
	else
		*key++ = (UCHAR) (-recordNumber | 0xC0);
	
	memcpy(key, fullKey + offset, length);
	nextNode = (Btn*) (key + length);
	
	return nextNode;
}

/**
@brief		Return the value of an appended record number starting at the byte sent in.
**/
int32 IndexNode::getAppendedRecordNumber(const UCHAR *ptr)
{
	int len1 = (*ptr & 0xf0) >> 4;
	int32 number = *ptr++ & 0x0f;

	for (int length = len1; length > 2; --length)
		number = (number << 8) | *ptr++;

	number = (number << 4) | (*ptr >> 4);

#ifdef DEBUG_EXTRA_INDEXES
	int len2 = *ptr & 0x0f;
	ASSERT(len1 == len2);
#endif

	return number;
}

/**
@brief		Return the value of an appended record number.
**/
int32 IndexNode::getAppendedRecordNumber()
{
	int32 number = 0;
	int len2 = getAppendedRecordNumberLength();
	if (len2)
		{
		UCHAR *ptr = key + length - len2;

#ifdef DEBUG_EXTRA_INDEXES
		int len1 = (ptr & 0xf0) >> 4;
		ASSERT(len1 == len2);
#endif

		number = *ptr++ & 0x0f;

		for (int length = len2; length > 2; --length)
			number = (number << 8) | *ptr++;

		number = (number << 4) | (*ptr >> 4);
		}

	return number;
}

Btn* IndexNode::parseNode(Btn* indexNode, void* endBucket)
{
	if (indexNode == endBucket)
		{
		node = indexNode;
		number = -1;
		offset = 0;
		length = 0;
		nextNode = NULL;
		return node;
		}
		
	return parseNode(indexNode);
}
