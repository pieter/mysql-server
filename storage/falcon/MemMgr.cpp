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


#ifndef NO_MEM_INLINE
#define NO_MEM_INLINE
#endif

#include <stdio.h>
#include <string.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <memory.h>
#include "Engine.h"
#include "MemMgr.h"
#include "Sync.h"
#include "Mutex.h"
#include "SQLError.h"
#include "Interlock.h"
#include "Stream.h"
#include "InfoTable.h"
#include "MemControl.h"

#ifdef HAVE_purify
#include <valgrind/memcheck.h>
#endif

#ifndef VALGRIND_MAKE_MEM_UNDEFINED
#define VALGRIND_MAKE_MEM_DEFINED(address, length)
#define VALGRIND_MAKE_MEM_UNDEFINED(address, length)
#endif

#ifdef ENGINE
#include "Log.h"
#include "LogStream.h"
#endif

static const int guardBytes = sizeof(long); // * 2048;

#ifndef ASSERT
#define ASSERT
#endif

#define INIT_BYTE			0xCC
#define GUARD_BYTE			0xDD
#define DELETE_BYTE			0xEE

#define HEAP_SIZE			2097152
#define FREE_OBJECTS_SIZE	1024
#define FREE_OBJECTS_SHIFT	2
#define MEMORY_UNIT			4
#define BLOCK_LENGTH(size)	ROUNDUP(size,MEMORY_UNIT)
#define DUMMY_LINE			100000
#define CLIENT_HASH_SIZE	101
#define MAX_CLIENTS			1024

const int validateMinutia	= 16;

// Nominal memory limits at startup--final values set during initialization

static MemMgr		memoryManager(defaultRounding, FREE_OBJECTS_SIZE, HEAP_SIZE);
static MemMgr		recordManager(defaultRounding, 2, HEAP_SIZE);
//static MemMgr		recordObjectManager (defaultRounding, sizeof(RecordVersion) + 100, HEAP_SIZE);
static MemControl	memControl;

#ifdef _DEBUG
static void		*stopAddress;
static FILE		*traceFile;

#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#define LEAK_VECTOR		100

struct MemLeak {
	const char	*fileName;
	int			line;
	int			count;
	};

struct Client {
	const char	*fileName;
	Client		*collision;
	short		line;
	//short		blocks;
	long		objectsInUse;
	long		objectsDeleted;
	long		spaceInUse;
	long		spaceDeleted;
	};

#ifdef _DEBUG
	void* MemMgrPoolAllocateDebug (MemMgr *pool, unsigned int s, const char *file, int line)
		{
		void *object = pool->allocateDebug(s, file, line);

		if (object == stopAddress)
			printf("MemMgrAllocateDebug at %p\n", stopAddress);

		if (traceFile)
			fprintf(traceFile, "a %d %p\n", s, object);

		return object;
		}

	void* MemMgrAllocateDebug (unsigned int s, const char *file, int line)
		{
		void *object = memoryManager.allocateDebug(s, file, line);

		if (object == stopAddress)
			printf("MemMgrAllocateDebug at %p\n", stopAddress);

		if (traceFile)
			fprintf(traceFile, "a %d %p\n", s, object);

		return object;
		}

	void MemMgrRelease (void *object)
		{
		/***
		if (object == stopAddress)
			printf ("MemMgrRelease at %p\n", stopAddress);
		***/

		if (traceFile)
			fprintf (traceFile, "r %p\n", object);

		memoryManager.releaseDebug (object);
		}

	void* MemMgrRecordAllocate (int size, const char *file, int line)
		{
		return recordManager.allocateDebug (size, file, line);
		}

	void MemMgrRecordDelete (char *record)
		{
		recordManager.releaseDebug (record);
		}
#else
	void* MemMgrPoolAllocate (MemMgr *pool, unsigned int s)
		{
		return pool->allocate (s);
		}

	void* MemMgrAllocate (unsigned int s)
		{
		return memoryManager.allocate (s);
		}

	void MemMgrRelease (void *object)
		{
		memoryManager.release (object);
		}

	void* MemMgrRecordAllocate (int size, const char *file, int line)
		{
		return recordManager.allocate (size);
		}

	void MemMgrRecordDelete (char *record)
		{
		recordManager.release (record);
		}

#endif

void MemMgrValidate ()
	{
	memoryManager.validate();
	recordManager.validate();
	//recordObjectManager.validate();
	}

void MemMgrValidate (void *object)
	{
	if (object)
		MemMgr::validate(object);
	}

void MemMgrAnalyze(int mask, Stream *stream)
	{
	//ENTER_CRITICAL_SECTION;
	stream->putSegment ("Memory\n");
	memoryManager.analyze (mask, stream, NULL, NULL);
	stream->putSegment ("Records\n");
	recordManager.analyze (mask, stream, NULL, NULL);
	//LEAVE_CRITICAL_SECTION;
	}

void MemMgrAnalyze(MemMgrWhat what, InfoTable *infoTable)
	{
	switch (what)
		{
		case MemMgrSystemSummary:
			memoryManager.analyze(0, NULL, infoTable, NULL);
			break;

		case MemMgrSystemDetail:
			memoryManager.analyze(0, NULL, NULL, infoTable);
			break;

		case MemMgrRecordSummary:
			recordManager.analyze(0, NULL, infoTable, NULL);
			//recordObjectManager.analyze(0, NULL, infoTable, NULL);
			break;

		case MemMgrRecordDetail:
			recordManager.analyze(0, NULL, NULL, infoTable);
			//recordObjectManager.analyze(0, NULL, NULL, infoTable);
			break;
		}
	}

void MemMgrSetMaxRecordMember (long long size)
	{
	if (!recordManager.memControl)
		{
		memControl.setMaxSize(size);
		memControl.addPool(&recordManager);
		//memControl.addPool(&recordObjectManager);
		}
	}

MemMgr*	MemMgrGetFixedPool (int id)
	{
	switch (id)
		{
		case MemMgrPoolGeneral:
			return &memoryManager;

		case MemMgrPoolRecordData:
			return &recordManager;

		/***
		case MemMgrPoolRecordObject:
			return &recordObjectManager;
		***/

		default:
			return NULL;
		}
	}


void MemMgrLogDump()
	{
#ifdef ENGINE
	LogStream stream;
	MemMgrAnalyze (0, &stream);
#endif
	}


MemMgr::MemMgr(int rounding, int cutoff, int minAlloc)
{
	signature = defaultSignature;
	roundingSize = rounding;
	threshold = cutoff;
	minAllocation = minAlloc;
	currentMemory		= 0;
	blocksAllocated		= 0;
	blocksActive		= 0;
	activeMemory		= 0;
	numberBigHunks		= 0;
	numberSmallHunks	= 0;
	bigHunks			= NULL;
	smallHunks			= NULL;
	memControl			= NULL;

	int vecSize = (cutoff + rounding) / rounding;
	int l = vecSize * sizeof (void*);

	freeObjects = (MemBlock**) allocRaw (l);
	memset (freeObjects, 0, l);
	//freeBlocks.nextLarger = freeBlocks.priorSmaller = &freeBlocks;
	//freeBlockTree = NULL;
	junk.larger = junk.smaller = &junk;
}


MemMgr::MemMgr(void* arg1, void* arg2)
{
	MemMgr();
}

MemMgr::~MemMgr(void)
{
	releaseRaw (freeObjects);
	freeObjects = NULL;

	for (MemSmallHunk *hunk; (hunk = smallHunks);)
		{
		smallHunks = hunk->nextHunk;
		releaseRaw (hunk);
		}
		
	for (MemBigHunk *bigHunk; (bigHunk = bigHunks);)
		{
		bigHunks = bigHunk->nextHunk;
		releaseRaw (bigHunk);
		}
}

MemBlock* MemMgr::alloc(int length)
{
	if (length <= 0)
		throw SQLError (RUNTIME_ERROR, "illegal memory allocate for %d bytes", length);
		
	Sync sync (&mutex, "MemMgr::alloc");
	sync.lock(Exclusive);

	// If this is a small block, look for it there
	
	if (length <= threshold)
		{
		int slot = length / roundingSize;
		MemBlock *block;
		
		while ( (block = freeObjects [slot]) )
			{
			void *next = block->pool;
			
			if (COMPARE_EXCHANGE_POINTER(freeObjects + slot, block, next))
				return block;
			}
		
		// See if some other hunk has unallocated space to use
		
		MemSmallHunk *hunk;
		
		for (hunk = smallHunks; hunk; hunk = hunk->nextHunk)
			if (length <= hunk->spaceRemaining)
				{
				MemBlock *block = (MemBlock*) hunk->memory;
				hunk->memory += length;
				hunk->spaceRemaining -= length;
				block->length = -length;
				
				return block;
				}
		
		// No good so far.  Time for a new hunk
		
		hunk = (MemSmallHunk*) allocRaw (minAllocation);
		hunk->length = minAllocation;
		hunk->nextHunk = smallHunks;
		smallHunks = hunk;
		
		int l = ROUNDUP(sizeof (MemSmallHunk), sizeof (double));
		block = (MemBlock*) ((UCHAR*) hunk + l);
		hunk->spaceRemaining = minAllocation - length - l;
		hunk->memory = (UCHAR*) block + length;
		block->length = -length;
		++numberSmallHunks;

		return block;		
		}
	
	/*
	 *  OK, we've got a "big block" on on hands.  To maximize confusing, the indicated
	 *  length of a free big block is the length of MemHeader plus body, explicitly
	 *  excluding the MemFreeBlock and MemBigHeader fields.
	
                         [MemHeader::length]	
                        	                                  
		                <---- MemBlock ---->
		                
		*--------------*----------*---------*
		| MemBigHeader | MemHeader |  Body  |
		*--------------*----------*---------*
		
		 <---- MemBigObject ----->
		
		*--------------*----------*---------------*
		| MemBigHeader | MemHeader | MemFreeBlock |
		*--------------*----------*---------------*
		
		 <--------------- MemFreeBlock ---------->
	 */
	
	

	if (length < (int) (OFFSET(MemBlock*, body) + sizeof (MemFreeBlock) - sizeof (MemBigObject)))
		length = (int) (OFFSET(MemBlock*, body) + sizeof (MemFreeBlock) - sizeof (MemBigObject));	
	
	MemFreeBlock *freeBlock = freeBlockTree.findNextLargest(length);
	
	if (!freeBlock && freeBlockTree.larger)
		freeBlock = freeBlockTree.findNextLargest(length);	
			
	if (freeBlock)
		{
		//freeBlockTree.validate();
		MemBlock *block = (MemBlock*) &freeBlock->memHeader;
		
		// Compute length (MemHeader + body) for new free block
		
		int tail = block->length - length;
		
		// If there isn't room to split off a new free block, allocate the whole thing
		
		if (tail < (int) sizeof (MemFreeBlock))
			{
			block->pool = this;
			activeMemory += block->length;
			
			return block;
			}
		
		// Otherwise, chop up the block
		
		MemBigObject *newBlock = freeBlock;
		freeBlock = (MemFreeBlock*) ((UCHAR*) block + length);
		freeBlock->memHeader.length = tail - sizeof (MemBigHeader); 
		block->length = length;
		block->pool = this;
		activeMemory += length;
		
		if ( (freeBlock->next = newBlock->next) )
			freeBlock->next->prior = freeBlock;
		
		newBlock->next = freeBlock;
		freeBlock->prior = newBlock;
		freeBlock->memHeader.pool = NULL;		// indicate block is free
		insert (freeBlock);
		//validateFreeList();
		
		return block;
		}

			 
	// Didn't find existing space -- allocate new hunk
	
	int hunkLength = sizeof (MemBigHunk) + sizeof(MemBigHeader) + length;
	int freeSpace = 0;
	
	// If the hunk size is sufficient below minAllocation, allocate extra space
	
	if (hunkLength + (int) sizeof(MemBigObject) + threshold < minAllocation)
		{
		hunkLength = minAllocation;
		freeSpace = hunkLength - sizeof(MemBigHunk) - 2 * sizeof(MemBigHeader) - length;
		}
	
	// Allocate the new hunk
	
	MemBigHunk *hunk = (MemBigHunk*) allocRaw(hunkLength);
	hunk->nextHunk = bigHunks;
	bigHunks = hunk;
	hunk->length = hunkLength;
	++numberBigHunks;

	// Create the new block
	
	MemBigObject *newBlock = (MemBigObject*) &hunk->blocks;
	newBlock->prior = NULL;
	newBlock->next = NULL;
	
	MemBlock *block = (MemBlock*) &newBlock->memHeader;
	block->pool = this;
	block->length = length;
	activeMemory += length;
	
	// If there is space left over, create a free block
	
	if (freeSpace)
		{
		freeBlock = (MemFreeBlock*) ((UCHAR*) block + length);
		freeBlock->memHeader.length = freeSpace;
		freeBlock->memHeader.pool = NULL;
		freeBlock->next = NULL;
		freeBlock->prior = newBlock;
		newBlock->next = freeBlock;
		insert (freeBlock);
		}
	
	//validateFreeList();

	return block;		
}

void* MemMgr::allocate(int size)
{
	int length = ROUNDUP(size, roundingSize) + OFFSET(MemBlock*, body) + guardBytes;
	MemBlock *memory;
	
	if (signature)
		{
		memory = alloc (length);
		memory->pool = this;
		}
	else
		{
		length = ROUNDUP(size, defaultRounding) + OFFSET(MemBlock*, body) + sizeof(long);
		memory = (MemBlock*) allocRaw(length);
		memory->pool = NULL;
		memory->length = length;
		}
	
#ifdef MEM_DEBUG
	memset (&memory->body, INIT_BYTE, size);
	memset (&memory->body + size, GUARD_BYTE, ABS(memory->length) - size - OFFSET(MemBlock*,body));
	memory->fileName = NULL;
	memory->lineNumber = 0;
#endif

	++blocksAllocated;
	++blocksActive;
	VALGRIND_MAKE_MEM_UNDEFINED(&memory->body, size);
	
	return &memory->body;
}

void* MemMgr::allocateDebug(int size, const char* fileName, int line)
{
	int length = ROUNDUP(size, roundingSize) + OFFSET(MemBlock*, body) + guardBytes;
	MemBlock *memory;

	if (signature)
		{
		memory = alloc (length);
		memory->pool = this;
		}
	else
		{
		length = ROUNDUP(size, defaultRounding) + OFFSET(MemBlock*, body) + sizeof(long);
		memory = (MemBlock*) allocRaw(length);
		memory->pool = NULL;
		}
	
#ifdef MEM_DEBUG
	memory->fileName = fileName;
	memory->lineNumber = line;
#endif

	memset (&memory->body, INIT_BYTE, size);
	int l = ABS(memory->length) - size - OFFSET(MemBlock*,body);
	ASSERT(l >= guardBytes && l < length - size + guardBytes + (int) sizeof (MemFreeBlock));
	memset (&memory->body + size, GUARD_BYTE, l);
	++blocksAllocated;
	++blocksActive;
	//validateBlock(&memory->body);
	VALGRIND_MAKE_MEM_UNDEFINED(&memory->body, size);

	return &memory->body;
}


void MemMgr::release(void* object)
{
	if (object)
		{
		MemBlock *block = (MemBlock*) ((UCHAR*) object - OFFSET(MemBlock*, body));

		if (block->pool == NULL)
			{
			free(block);	// releaseRaw(block);

			return;
			}

		block->pool->releaseBlock(block);
		}
}

void MemMgr::releaseBlock(MemBlock *block)
{
	if (!freeObjects)
		return;

	if (block->pool->signature != defaultSignature)
		corrupt("bad block released");

#ifdef MEM_DEBUG
	for (const UCHAR *end = (UCHAR*) block + ABS(block->length), *p = end - guardBytes; p < end;)
		if (*p++ != GUARD_BYTE)
			corrupt ("guard bytes overwritten");
#endif

	--blocksActive;
	int length = block->length;

	// If length is negative, this is a small block

	if (length < 0)
		{
		VALGRIND_MAKE_MEM_DEFINED(block, -length);
#ifdef MEM_DEBUG
		block->lineNumber = -block->lineNumber;
		memset (&block->body, DELETE_BYTE, -length - OFFSET(MemBlock*, body));
#endif

		for (int slot = -length / roundingSize;;)
			{
			void *next = freeObjects [slot];
			block->pool = (MemMgr*) next;

			if (COMPARE_EXCHANGE_POINTER(freeObjects + slot, next, block))
				return;
			}
		}

	// OK, this is a large block.  Try recombining with neighbors

	VALGRIND_MAKE_MEM_DEFINED(block, length);

#ifdef MEM_DEBUG
	memset (&block->body, DELETE_BYTE, length - OFFSET(MemBlock*, body));
#endif

	MemFreeBlock *freeBlock = (MemFreeBlock*) ((UCHAR*) block - sizeof (MemBigHeader));
	Sync sync (&mutex, "MemMgr::release");
	sync.lock(Exclusive);
	//validateFreeList();
	block->pool = NULL;
	ASSERT(length <= (int) activeMemory);
	activeMemory -= length;

	if (freeBlock->next && !freeBlock->next->memHeader.pool)
		{
		MemFreeBlock *next = (MemFreeBlock*) (freeBlock->next);
		remove (next);
		freeBlock->memHeader.length += next->memHeader.length + sizeof (MemBigHeader);

		if ( (freeBlock->next = next->next) )
			freeBlock->next->prior = freeBlock;

		//validateFreeList();
		}


	if (freeBlock->prior && !freeBlock->prior->memHeader.pool)
		{
		MemFreeBlock *prior = (MemFreeBlock*) (freeBlock->prior);
		remove (prior);
		prior->memHeader.length += freeBlock->memHeader.length + sizeof (MemBigHeader);

		if ( (prior->next = freeBlock->next) )
			prior->next->prior = prior;

		freeBlock = prior;
		//validateFreeList();
		}

	if (freeBlock->prior == NULL && freeBlock->next == NULL)
		{
		for (MemBigHunk **ptr = &bigHunks, *hunk; (hunk = *ptr); ptr = &hunk->nextHunk)
			if (&hunk->blocks == freeBlock)
				{
				*ptr = hunk->nextHunk;
				releaseRaw(hunk);
				return;
				}

		corrupt("can't find big hunk");
		}

	insert (freeBlock);
	//validateFreeList();
}

void MemMgr::corrupt(const char* text)
{
#ifdef ENGINE
	Log::logBreak ("Memory pool corrupted: %s\n", text);
#endif

	MemMgrLogDump();
	throw SQLError (BUG_CHECK, "memory is corrupt: %s", text);
}

void* MemMgr::memoryIsExhausted(void)
{
#ifdef ENGINE
	Log::logBreak ("Memory is exhausted\n");
#endif

	MemMgrLogDump();
	throw SQLError (OUT_OF_MEMORY_ERROR, "memory is exhausted");
}

void MemMgr::remove(MemFreeBlock* block)
{
	//freeBlockTree.validate();
	//int count = freeBlockTree.count();
	
	block->remove();
	
	/***
	freeBlockTree.validate();
	int count2 = freeBlockTree.count();
	
	if (count - 1 != count2)
		corrupt("bad count");
	***/
	
	/***
	// If this is junk, chop it out and be done with it
	
	if (block->memHeader.length < threshold)
		return;
		
	// If we're a twin, take out of the twin list
	
	if (!block->nextLarger)
		{
		block->nextTwin->priorTwin = block->priorTwin;
		block->priorTwin->nextTwin = block->nextTwin;
		//validateFreeList();
		
		return;
		}
	
	// We're in the primary list.  If we have twin, move him in
	
	MemFreeBlock *twin = block->nextTwin;

	if (twin != block)
		{
		block->priorTwin->nextTwin = twin;
		twin->priorTwin = block->priorTwin;
		twin->priorSmaller = block->priorSmaller;
		twin->nextLarger = block->nextLarger;
		twin->priorSmaller->nextLarger = twin;
		twin->nextLarger->priorSmaller = twin;
		//validateFreeList();
		
		return;
		}
	
	// No twins.  Just take the guy out of the list
	
	block->priorSmaller->nextLarger = block->nextLarger;
	block->nextLarger->priorSmaller = block->priorSmaller;
	//validateFreeList();
	***/
}

void MemMgr::insert(MemFreeBlock* freeBlock)
{
	//freeBlockTree.validate();
	//int count = freeBlockTree.count();
	
	freeBlockTree.insert(freeBlock);
	
	/***
	freeBlockTree.validate();
	int count2 = freeBlockTree.count();
	
	if (count + 1 != count2)
		corrupt("bad count");
	***/
	
	/***
	// If this is junk (too small for pool), stick it in junk
	
	if (freeBlock->memHeader.length < threshold)
		return;
		
	// Start by finding insertion point

	MemFreeBlock *block;
	
	for (block = freeBlocks.nextLarger; 
		 block != &freeBlocks && freeBlock->memHeader.length >= block->memHeader.length;
		 block = block->nextLarger)
		if (block->memHeader.length == freeBlock->memHeader.length)
			{
			// If this is a "twin" (same size block), hang off block
			freeBlock->nextTwin = block->nextTwin;
			freeBlock->nextTwin->priorTwin = freeBlock;
			freeBlock->priorTwin = block;
			block->nextTwin = freeBlock;
			freeBlock->nextLarger = NULL;
			//validateFreeList();
			return;
			}
	
	// OK, then, link in after insertion point
	
	freeBlock->nextLarger = block;
	freeBlock->priorSmaller = block->priorSmaller;
	block->priorSmaller->nextLarger = freeBlock;
	block->priorSmaller = freeBlock;
	
	freeBlock->nextTwin = freeBlock->priorTwin = freeBlock;
	//validateFreeList();
	***/
}

void* MemMgr::allocRaw(int length)
{
	if (memControl && !memControl->poolExtensionCheck(length))
		throw SQLError(OUT_OF_RECORD_MEMORY_ERROR, "record memory is exhausted");

	void *memory = malloc(length);
	
	if (memory)
		currentMemory += length;
	else
		memoryIsExhausted();
	
	return memory;
}

void MemMgr::debugStop(void)
{
}

void MemMgr::validateFreeList(void)
{
	int len = 0;
	int count = 0;
	MemFreeBlock *block;
	
	for (block = freeBlockTree.getFirst(); block; block = block->getNext())
		{
		if (block->memHeader.length <= len)
			corrupt ("bad free list\n");
			
		len = block->memHeader.length;
		++count;
		int twins = 0;
		MemFreeBlock *twin;
		MemFreeBlock *priorTwin = block;
		
		for (twin = block->nextTwin; twin != block; priorTwin = twin, twin = twin->nextTwin)
			{
			if (twin->priorTwin != priorTwin)
				corrupt("bad priorTwin pointer");
				
			++twins;
			}
			
		for (twin = block->priorTwin; twin != block; twin = twin->priorTwin)
			--twins;
			
		if (twins)
			corrupt("bad twin list");
		}
	
	len += 1;
	
	for (block = freeBlockTree.getFirst(); block; block = block->getNext())
		{
		if (block->memHeader.length >= len)
			corrupt ("bad free list\n");
			
		len = block->memHeader.length;
		}

}

void MemMgr::validateBigBlock(MemBigObject* block)
{
	MemBigObject *neighbor;
	
	if ( (neighbor = block->prior) )
		if ((UCHAR*) &neighbor->memHeader + neighbor->memHeader.length != (UCHAR*) block)
			corrupt ("bad neighbors");
	
	if ( (neighbor = block->next) )
		if ((UCHAR*) &block->memHeader + block->memHeader.length != (UCHAR*) neighbor)
			corrupt ("bad neighbors");
}

void MemMgr::releaseRaw(MemBlock **block) // tbd: clean these up
{
	if (block)
		{
		if (*block)
			currentMemory -= (*block)->length;
			
		free (block);
		}
}

void MemMgr::releaseRaw(MemSmallHunk *block)
{
	if (block)
		{
		currentMemory -= block->length;
		free (block);
		}
}

void MemMgr::releaseRaw(MemBigHunk *block)
{
	if (block)
		{
		currentMemory -= block->length;
		free (block);
		}
}

void MemMgr::releaseDebug(void *object)
{
	release (object);
}

void MemMgr::validate()
{

}

void MemMgr::analyze(int mask, Stream *stream, InfoTable *summaryTable, InfoTable *detailTable)
{
#ifdef MEM_DEBUG
	Sync sync (&mutex, "MemMgr::analyze");
	
	if (summaryTable || detailTable)
		sync.lock(Exclusive);
		
	Client *hashTable [CLIENT_HASH_SIZE];
	memset (hashTable, 0, sizeof (hashTable));
	UCHAR *memory = (UCHAR*) malloc (sizeof(Client) * MAX_CLIENTS);
	Client *clients = (Client*) memory;
	Client *endClients = clients + MAX_CLIENTS;
	int l = ROUNDUP(sizeof (MemSmallHunk), sizeof (double));
	Client *client;
	Client totals;

	for (MemSmallHunk *smallHunk = smallHunks; smallHunk; smallHunk = smallHunk->nextHunk)
		{
		for (UCHAR *p = (UCHAR*) smallHunk + l; p < smallHunk->memory;)
			{
			MemHeader *object = (MemHeader*) p;
			int length = object->length;

			if (length < 0)
				length = -length;

			const char *file = object->fileName;

			if (!file)
				file = "*unknown*";

			int line = object->lineNumber;

			if (line < 0)
				line = -line;

			int slot = line % CLIENT_HASH_SIZE;

			for (client = hashTable [slot]; client; client = client->collision)
				if (client->fileName == file && client->line == line)
					break;

			if (!client)
				{
				client = clients++;

				if (!client || client >= endClients)
					{
#ifdef ENGINE
					Log::debugBreak ("MemMgr::analyze: analysis space exhausted");
#endif
					free (memory);
					return;
					}

				memset (client, 0, sizeof (Client));
				client->fileName = file;
				client->line = line;
				client->collision = hashTable [slot];
				hashTable [slot] = client;
				}

			if (object->lineNumber > 0)
				{
				++client->objectsInUse;
				client->spaceInUse += length;
				}
			else
				{
				++client->objectsDeleted;
				client->spaceDeleted += length;
				}

			MemObject *next = (MemObject*) (p + length);
			p = (UCHAR*) next;
			}
		}

	int64 freeSpace = 0;
	int numberFree = 0;
	int littleSpace = 0;

	for (MemBigHunk *bigHunk = bigHunks; bigHunk; bigHunk = bigHunk->nextHunk)
		for (MemBigObject *bigObject = (MemBigObject*) &bigHunk->blocks; bigObject; bigObject = bigObject->next)
			{
			int length = bigObject->memHeader.length;
			if (bigObject->memHeader.pool)
				{
				const char *file = bigObject->memHeader.fileName;

				if (!file)
					file = "*unknown*";

				int line = bigObject->memHeader.lineNumber;
				ASSERT (line >= 0);
				int slot = line % CLIENT_HASH_SIZE;

				for (client = hashTable [slot]; client; client = client->collision)
					if (client->fileName == file && client->line == line)
						break;

				if (!client)
					{
					client = clients++;

					if (client >= endClients)
						{
#ifdef ENGINE
						Log::debugBreak ("MemMgr::analyze: analysis space exhausted");
#endif
						free (memory);
						return;
						}

					memset (client, 0, sizeof (Client));
					client->fileName = file;
					client->line = line;
					client->collision = hashTable [slot];
					hashTable [slot] = client;
					}

				++client->objectsInUse;
				client->spaceInUse += length;
				}
			else
				{
				++numberFree;

				if (length < threshold)
					littleSpace += length;
				else
					freeSpace += length;
				}
			}

	if (stream)
		{
		stream->putSegment ("\nModule\tLine\tIn Use\tSpace in Use\tDeleted\tSpace deleted\n");
		memset (&totals, 0, sizeof(totals));
		
		for (client = (Client*) memory; client < clients; ++client)
			{
			stream->format ("%s\t%d\t%d\t%d\t%d\t%d\n", client->fileName, client->line,
						client->objectsInUse, client->spaceInUse,
						client->objectsDeleted, client->spaceDeleted);
			totals.objectsInUse += client->objectsInUse;
			totals.spaceInUse += client->spaceInUse;
			totals.objectsDeleted += client->objectsDeleted;
			totals.spaceDeleted += client->spaceDeleted;
			}

		stream->format ("Total\t\t%d\t%d\t%d\t%d\n", 
						totals.objectsInUse, totals.spaceInUse,
						totals.objectsDeleted, totals.spaceDeleted);
		stream->format ("Number small hunks:\t%d\n", numberSmallHunks);
		stream->format ("Number big hunks:\t%d\n", numberBigHunks);

		if (mask & validateMinutia)
			stream->format ("Free blocks:\t");

		int batch = 0;
		int64 orderedSpace = 0;
		int sizes = 0;
		
		//for (MemFreeBlock *blk = freeBlocks.nextLarger; blk != &freeBlocks; blk = blk->nextLarger)
		for (MemFreeBlock *blk = freeBlockTree.getFirst(); blk; blk = blk->getNext())
			{
			++sizes;
			int count = 1;

			for (MemFreeBlock *twin = blk->nextTwin; twin != blk; twin = twin->nextTwin)
				++count;

			int length = blk->memHeader.length;
			orderedSpace += length * count;

			if (mask & validateMinutia)
				{
				stream->format ("%d (%d), ", length, count);

				if (++batch > 10)
					{
					stream->putSegment ("\n\t\t");
					batch = 0;
					}
				}
			}

		if (batch)
			stream->putCharacter ('\n');
		
		stream->format ("Unique sizes: %d\n", sizes);
		stream->format ("Free segments:\t%d\n", numberFree);
		stream->format ("Free space:\t" I64FORMAT "\n", freeSpace);

		if (orderedSpace != freeSpace)
			stream->format ("Memory leak: " I64FORMAT "\n", freeSpace - orderedSpace);
		}
	
	if (summaryTable)
		{
		int sizes = 0;

		
		for (MemFreeBlock *blk = freeBlockTree.getFirst(); blk; blk = blk->getNext())
		//for (MemFreeBlock *blk = freeBlocks.nextLarger; blk != &freeBlocks; blk = blk->nextLarger)
			{
			++sizes;
			int count = 1;

			for (MemFreeBlock *twin = blk->nextTwin; twin != blk; twin = twin->nextTwin)
				++count;
			}
		
		int n = 0;
		summaryTable->putInt64(n++, currentMemory);
		summaryTable->putInt64(n++, freeSpace);
		summaryTable->putInt(n++, numberFree);
		summaryTable->putInt(n++, numberBigHunks);
		summaryTable->putInt(n++, numberSmallHunks);
		summaryTable->putInt(n++, sizes);
		summaryTable->putRecord();
		}
	
	if (detailTable)
		{
		for (client = (Client*) memory; client < clients; ++client)
			{
			const char *p = strrchr(client->fileName, SEPARATOR);
			int n = 0;
			detailTable->putString(n++, (p) ? p + 1 : client->fileName);
			detailTable->putInt(n++, client->line);
			detailTable->putInt(n++, client->objectsInUse);
			detailTable->putInt(n++, client->spaceInUse);
			detailTable->putInt(n++, client->objectsDeleted);
			detailTable->putInt(n++, client->spaceDeleted);
			detailTable->putRecord();
			}
		}

	free (memory);
#endif
}

void MemMgr::validateBlock(void *object)
{
#ifdef MEM_DEBUG
	MemBlock *block = (MemBlock*) ((UCHAR*) object - OFFSET(MemBlock*, body));

	for (const UCHAR *end = (UCHAR*) block + ABS(block->length), *p = end - guardBytes; p < end;)
		if (*p++ != GUARD_BYTE)
			corrupt ("guard bytes overwritten");
#endif
}

void MemMgr::validate(void *object)
{
	if (object)
		{
		MemBlock *block = (MemBlock*) ((UCHAR*) object - OFFSET(MemBlock*, body));
		block->pool->validateBlock(block);
		}
}

void MemMgr::validateBlock(MemBlock *block)
{
	if (block->pool->signature != defaultSignature)
		corrupt("bad block released");
		
#ifdef MEM_DEBUG
	for (const UCHAR *end = (UCHAR*) block + ABS(block->length), *p = end - guardBytes; p < end;)
		if (*p++ != GUARD_BYTE)
			corrupt ("guard bytes overwritten");
#endif
}
