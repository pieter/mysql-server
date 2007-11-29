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

#ifndef _DEFERRED_INDEX_H_
#define _DEFERRED_INDEX_H_

#include "SyncObject.h"

static const uint DEFERRED_INDEX_FANOUT		= 32; // 4;
static const uint DEFERRED_INDEX_HUNK_SIZE	= 32768;
static const int DEFERRED_INDEX_MAX_LEVELS	= 16;

struct DIHunk {
	DIHunk	*next;
	UCHAR	space[DEFERRED_INDEX_HUNK_SIZE];
	};

struct DINode {
	int32	recordNumber;
	uint16	keyLength;
	UCHAR	key[1];
	};

struct DIUniqueNode {
	DIUniqueNode	*collision;
	DINode			node;
	};

#define UNIQUE_NODE(node) ((DIUniqueNode*) (((char*) node) - OFFSET(DIUniqueNode*, node)))

struct DILeaf {
	uint	count;
	DINode	*nodes[DEFERRED_INDEX_FANOUT];
	};

struct DIBucket;

struct DIRef {
	DINode		*node;
	DIBucket	*bucket;
	};

struct DIBucket {
	uint		count;
	DIRef		references[DEFERRED_INDEX_FANOUT];
	};

struct DIState {
	DIBucket	*bucket;
	uint		slot;
	};


class Transaction;
class Index;
class IndexKey;
class Bitmap;
class Dbb;

class DeferredIndex
{
public:
	DeferredIndex(Index *index, Transaction *trans);
	~DeferredIndex(void);

	void			freeHunks(void);
	void*			alloc(uint length);
	void			addNode(IndexKey* indexKey, int32 recordNumber);
	bool			deleteNode(IndexKey* key, int32 recordNumber);
	int				compare(IndexKey *node1, DINode *node2, bool partial);
	int				compare(DINode* node1, DINode* node2);
	int				compare(IndexKey* node1, int32 recordNumber, DINode* node2);
	void			detachIndex(void);
	void			detachTransaction(void);
	void			scanIndex (IndexKey *lowKey, IndexKey *highKey, int searchFlags, Bitmap *bitmap);
	void			chill(Dbb *dbb);

	int				checkTail(uint position, DINode* node);
	int				checkTail(uint position, IndexKey *indexKey);
	void			validate(void);
	static char*	format (uint indent, DINode *node, uint bufferLength, char *buffer);
	void			print();
	void			print(DIBucket* bucket);
	void			print(DILeaf* leaf);
	void			print(int indent, int level, DIBucket *bucket);
	void			print(const char *text, DINode *node);
	void			initializeSpace(void);
	DINode*			findMaxValue(void);
	DINode*			findMinValue(void);

	SyncObject		syncObject;	
	DeferredIndex	*next;
	DeferredIndex	*prior;
	DeferredIndex	*nextInTransaction;
	Index			*index;
	Transaction		*transaction;
	TransId			transactionId;
	DIHunk			*hunks;
	DINode			*minValue;
	DINode			*maxValue;
	UCHAR			initialSpace[500];
	UCHAR			*base;
	void			*root;
	uint			currentHunkOffset;
	uint			count;
	int				levels;
	bool			haveMinValue;
	bool			haveMaxValue;
	uint			sizeEstimate;
	uint64			virtualOffset;		// virtual offset into the serial log where this DI was flushed.
	uint64			virtualOffsetAtEnd;
};

#endif
