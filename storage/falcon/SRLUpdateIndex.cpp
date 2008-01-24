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

#include <stdio.h>
#include "Engine.h"
#include "SRLUpdateIndex.h"
#include "DeferredIndex.h"
#include "Index.h"
#include "IndexRootPage.h"
#include "Index2RootPage.h"
#include "IndexKey.h"
#include "DeferredIndexWalker.h"
#include "Transaction.h"
#include "SerialLogTransaction.h"
#include "SerialLogControl.h"
#include "SerialLogWindow.h"
#include "Dbb.h"
#include "Log.h"

SRLUpdateIndex::SRLUpdateIndex(void)
{
}

SRLUpdateIndex::~SRLUpdateIndex(void)
{
}

void SRLUpdateIndex::append(DeferredIndex* deferredIndex)
{
	Sync syncIndexes(&log->syncIndexes, "SRLUpdateIndex::append");
//	syncIndexes.lock(Exclusive);
	syncIndexes.lock(Shared);

	Transaction *transaction = deferredIndex->transaction;
	DeferredIndexWalker walker(deferredIndex, NULL);
	uint indexId = deferredIndex->index->indexId;
	int idxVersion = deferredIndex->index->indexVersion;
	int tableSpaceId = deferredIndex->index->dbb->tableSpaceId;
	uint64 virtualOffset = 0;
	uint64 virtualOffsetAtEnd = 0;

	// Remember where this is logged
	
	virtualOffset = log->writeWindow->getNextVirtualOffset();

	for (DINode *node = walker.next(); node;)
		{
		START_RECORD(srlUpdateIndex, "SRLUpdateIndex::append");
		log->updateIndexUseVector(indexId, tableSpaceId, 1);
		SerialLogTransaction *srlTrans = log->getTransaction(transaction->transactionId);
		srlTrans->setTransaction(transaction);
		ASSERT(transaction->writePending);
		putInt(tableSpaceId);
		putInt(transaction->transactionId);
		putInt(indexId);
		putInt(idxVersion);
		UCHAR *lengthPtr = putFixedInt(0);
		UCHAR *start = log->writePtr;
		UCHAR *end = log->writeWarningTrack;

		for (; node; node = walker.next())
			{
			if (log->writePtr + byteCount(node->recordNumber) +
				byteCount(node->keyLength) + node->keyLength >= end)
				break;
			
			putInt(node->recordNumber);
			putInt(node->keyLength);
			log->putData(node->keyLength, node->key);
			}
		
		int len = (int) (log->writePtr - start);
		//printf("SRLUpdateIndex::append tid %d, index %d, length %d, ptr %x (%x)\n",  transaction->transactionId, indexId, len, lengthPtr, org);
		ASSERT(len >= 0);
		putFixedInt(len, lengthPtr);
		const UCHAR *p = lengthPtr;
		ASSERT(getInt(&p) == len);
		virtualOffsetAtEnd = log->writeWindow->getNextVirtualOffset();
		log->endRecord();
		
		if (node)
			log->flush(true, 0, &sync);
		else
			sync.unlock();
		}
		
	// Update the virtual offset only if one more nodes were written
	
	if (virtualOffsetAtEnd > 0)
		{
		deferredIndex->virtualOffset = virtualOffset;
		deferredIndex->virtualOffsetAtEnd = virtualOffsetAtEnd;
		}
}

void SRLUpdateIndex::read(void)
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	transactionId = getInt();
	indexId = getInt();
	
	if (control->version >= srlVersion6)
		indexVersion = getInt();
	else
		indexVersion = INDEX_VERSION_1;

	dataLength = getInt();
	data = getData(dataLength);
}

void SRLUpdateIndex::print(void)
{
	logPrint("UpdateIndex: transaction %d, length %d\n",
			transactionId, dataLength);

	for (const UCHAR *p = data, *end = data + dataLength; p < end;)
		{
		int recordNumber = getInt(&p);
		int length = getInt(&p);
		char temp[40];
		Log::debug("   rec %d to index %d %s\n", recordNumber, indexId, format(length, p, sizeof(temp), temp));
		p += length;
		}
}

void SRLUpdateIndex::pass1(void)
{
	control->getTransaction(transactionId);
}

void SRLUpdateIndex::redo(void)
{
	execute();
}

void SRLUpdateIndex::commit(void)
{
	Sync sync(&log->syncIndexes, "SRLUpdateIndex::commit");
	sync.lock(Shared);
	log->updateIndexUseVector(indexId, tableSpaceId, -1);
	execute();
	log->setPhysicalBlock(transactionId);
}

void SRLUpdateIndex::execute(void)
{
	if (!log->isIndexActive(indexId, tableSpaceId))
		return;

	//SerialLogTransaction *transaction = 
	control->getTransaction(transactionId);
	ptr = data;
	end = ptr + dataLength;
	Dbb *dbb = log->getDbb(tableSpaceId);
	
	switch (indexVersion)
		{
		case INDEX_VERSION_0:
			Index2RootPage::indexMerge(dbb, indexId, this, NO_TRANSACTION);
			break;
		
		case INDEX_VERSION_1:
			IndexRootPage::indexMerge(dbb, indexId, this, NO_TRANSACTION);
			break;
		
		default:
			ASSERT(false);
		}
		
	/***
	IndexKey indexKey;
	
	for (int recordNumber; (recordNumber = nextKey(&indexKey)) != -1;)
		log->dbb->addIndexEntry(indexId, &indexKey, recordNumber, NO_TRANSACTION);
	***/
}

int SRLUpdateIndex::nextKey(IndexKey *indexKey)
{
	if (ptr >= end)
		return -1;
		
	int recordNumber = getInt(&ptr);
	int length = getInt(&ptr);
	
	indexKey->setKey(length, ptr);
	ptr += length;
	
	return recordNumber;
}

void SRLUpdateIndex::thaw(DeferredIndex* deferredIndex)
{
	Sync sync(&log->syncWrite, "SRLUpdateIndex::thaw");
	sync.lock(Exclusive);
	uint64 virtualOffset = deferredIndex->virtualOffset;
	int recordNumber = 0;  // a valid record number to get into the loop.
	ASSERT(deferredIndex->virtualOffset);
	Transaction *trans = deferredIndex->transaction;
	TransId transId = trans->transactionId;
	indexId = deferredIndex->index->indexId;
		
	Log::debug("Def Index Thaw:    trxId=%-5ld indexId=%-7ld  bytes=%8ld  addr=%p  vofs=%llx\n",
				transId, indexId, deferredIndex->sizeEstimate, this, virtualOffset);
				
	// Find the window where the DeferredIndex is stored using the virtualOffset,
	// then activate the window, reading from disk if necessary.

	SerialLogWindow *window = log->findWindowGivenOffset(deferredIndex->virtualOffset);
	
	if (window == NULL)
		{
		Log::log("A window for DeferredIndex::virtualOffset=" I64FORMAT " could not be found.\n",
		         deferredIndex->virtualOffset);
		log->printWindows();
		
		return;
		}
		
	// Find the correct block within the window and set the offset using that block.

	SerialLogBlock *block = window->firstBlock();
	uint32 blockOffset = 0;
	ASSERT( (UCHAR *) block == window->buffer);
	uint32 windowOffset = (uint32) (virtualOffset - window->virtualOffset);

	while (windowOffset >= blockOffset + block->length)
		{
		SerialLogBlock *prevBlock = block;
		block = window->nextBlock(block);
		uint32 thisBlockOffset = (uint32) ((UCHAR*) block - (UCHAR *) prevBlock);
		blockOffset += thisBlockOffset;
		}

	uint32 offsetWithinBlock = (windowOffset - blockOffset - OFFSET(SerialLogBlock*, data));
	control->setWindow(window, block, offsetWithinBlock);
	ASSERT(control->input == window->buffer + windowOffset);
	ASSERT(control->inputEnd <= window->bufferEnd);

	// Read the SerialLogRecord type and header

	UCHAR type = getInt();
	ASSERT(type == srlUpdateIndex);
	read();		// this read() is also in control->nextRecord() below.

	while (virtualOffset < deferredIndex->virtualOffsetAtEnd)
		{
		sync.unlock();

		// Read the header of the deferredIndex and validate.

		ASSERT(transactionId == transId);
		ASSERT(indexId == deferredIndex->index->indexId);
		ASSERT(indexVersion == deferredIndex->index->indexVersion);

		Log::debug("Def Index Thaw:    trxId=%-5ld indexId=%-7ld  bytes=%8ld  addr=%p  version=%ld\n",
					transactionId, indexId, dataLength, this, indexVersion);
					
		IndexKey indexKey(deferredIndex->index);

		// Read each IndexKey and add it to the deferredIndex.   set ptr and end for nextKey()

		ptr = data;
		end = ptr + dataLength;

		for (recordNumber = nextKey(&indexKey); recordNumber >= 0; recordNumber = nextKey(&indexKey))
			deferredIndex->addNode(&indexKey, recordNumber);

		sync.lock(Exclusive);

		for (;;)
			{
			// Quit if there are no more SerialLogRecords for this DeferredIndex.

			SerialLogWindow *inputWindow = control->inputWindow;
			virtualOffset = inputWindow->virtualOffset + (control->input - inputWindow->buffer);

			if (virtualOffset >= deferredIndex->virtualOffsetAtEnd)
				break;		// All done.

			// Find the next SerialLogRecord of this deferredIndex.

			SerialLogRecord *record = control->nextRecord();

			if ((record == this) && (transactionId == transId) && (indexId == deferredIndex->index->indexId))
				break;
			}
		}

	control->fini();
	window->deactivateWindow();
}
