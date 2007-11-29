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

// IndexNode.h: interface for the IndexNode class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INDEXNODE_H__F25B47A4_C0C8_11D4_98FC_0000C01D2301__INCLUDED_)
#define AFX_INDEXNODE_H__F25B47A4_C0C8_11D4_98FC_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <memory.h>
#include "Btn.h"
#include "RootPage.h"

#define SHIFT(n)	(1 << n)

#define RECORD_NUMBER_LENGTH(recordNumber) ((recordNumber < 0) ? 1 :	\
			(recordNumber < SHIFT(6)) ? 1 :		\
			(recordNumber < SHIFT(13)) ? 2 :	\
			(recordNumber < SHIFT(20)) ? 3 :	\
			(recordNumber < SHIFT(27)) ? 4 : 5)
			
class IndexPage;

class IndexNode
{
public:
	inline void expandKey (IndexKey *indexKey);
	IndexNode();
	IndexNode(IndexPage *page);
	IndexNode (Btn *node);

	Btn*	insert (Btn *where, int offst, int len, UCHAR *fullKey, int32 nmbr);
	void	printKey(const char * msg, UCHAR * key, bool inversion);
	int		expandKey (UCHAR *keyPtr);
	Btn*	getNext(void *end);
	int32	getNumber();
	Btn*	parseNode (Btn *indexNode);
	int32	getAppendedRecordNumber(const UCHAR *ptr);
	int32	getAppendedRecordNumber();
	bool	isEnd(void);

	inline int keyLength()
		{
		return offset + length;
		}

	inline static int nodeLength (int offset, int length, int32 number)
		{
		return ((offset >= 128) ? 2 : 1) + ((length >= 128) ? 2 : 1) + length + RECORD_NUMBER_LENGTH(number);
		}
	
	inline int getAppendedRecordNumberLength()
		{
		return (length ? key[length - 1] & 0x0f : 0);
		}
		
	Btn		*node;
	Btn		*nextNode;
	uint	offset;
	uint	length;
	UCHAR	*key;
	int32	number;
	IndexNode(Btn* indexNode, void* endBucket);
	Btn* parseNode(Btn* indexNode, void* bucketEnd);
};

inline Btn* IndexNode::parseNode(Btn *indexNode)
{
	node = indexNode;
	key = (UCHAR*) indexNode;
	
	if ( (offset = *key++) >= 128)
		offset = ((offset & 0x7f) << 8) | *key++;
	
	if ( (length = *key++) >= 128)
		length = ((length & 0x7f) << 8) | *key++;
	
	number = 0;
	
	if ((*key & 0xC0) == 0xC0)
		number = -(*key++ & 0x2f);
	else
		for (;;)
			{
			UCHAR c = *key++;
			number = (number << 7) | (c & 0x7F);
			
			if (c & 0x80)
				break;
			}
	
	ASSERT(key - (UCHAR*) indexNode < 14);	
	nextNode = (Btn*) (key + length);

	return nextNode;
}

inline int32 IndexNode::getNumber()
{
	return number;
}

inline Btn* IndexNode::getNext(void *end)
{
	if (nextNode == end)
		{
		node = nextNode;
		number = -1;
		offset = 0;
		length = 0;
		
		return node;
		}		
		
	return parseNode(nextNode);
}

inline int IndexNode::expandKey(UCHAR *keyPtr)
{
	ASSERT (offset + length <= MAX_PHYSICAL_KEY_LENGTH);

	if (length)
		memcpy (keyPtr + offset, key, length);

	return offset + length;
}


inline void IndexNode::expandKey(IndexKey *indexKey)
{
	ASSERT (offset + length <= MAX_PHYSICAL_KEY_LENGTH);

	if (length)
		memcpy (indexKey->key + offset, key, length);

	indexKey->keyLength = offset + length;
}


inline bool IndexNode::isEnd(void)
{
	return nextNode == NULL || number == END_BUCKET || number == END_LEVEL;
}

#endif // !defined(AFX_INDEXNODE_H__F25B47A4_C0C8_11D4_98FC_0000C01D2301__INCLUDED_)
