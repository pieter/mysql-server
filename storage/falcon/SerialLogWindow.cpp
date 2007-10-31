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

// SerialLogWindow.cpp: implementation of the SerialLogWindow class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SerialLogWindow.h"
#include "SerialLogFile.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "SQLError.h"
#include "Log.h"
#include "Sync.h"

#define NEXT_BLOCK(prior)	(SerialLogBlock*) ((UCHAR*) prior + ROUNDUP(prior->length, sectorSize))


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SerialLogWindow::SerialLogWindow(SerialLog *serialLog, SerialLogFile *logFile, int64 logOrigin)
{
	log = serialLog;
	bufferLength = SRL_WINDOW_SIZE;
	buffer = NULL;
	setPosition(logFile, logOrigin);
	next = NULL;
	lastBlock = NULL;
	inUse = 0;
	useCount = 0;
	currentLength = 0;
	virtualOffset = 0;
}

SerialLogWindow::~SerialLogWindow()
{
}

void SerialLogWindow::setPosition(SerialLogFile *logFile, int64 logOrigin)
{
	file = logFile;
	sectorSize = file->sectorSize;
	origin = logOrigin;
}

SerialLogBlock* SerialLogWindow::readFirstBlock()
{
	uint32 length;
	
	try
		{
		length = file->read(origin, sectorSize, buffer);
		}
	catch (SQLException&)
		{
		return NULL;
		}

	if (length != sectorSize)
		return NULL;
		
	SerialLogBlock *block = (SerialLogBlock*) buffer;

	if (block->creationTime != (uint32) log->creationTime || 
		 block->length == 0)
		return NULL;

	currentLength = sectorSize;
	length = block->length - sectorSize;
	length = ROUNDUP(length, sectorSize);

	if (length)
		{
		uint32 len = file->read(origin + sectorSize, block->length - sectorSize, buffer + sectorSize);

		if (len != length)
			throw SQLError(IO_ERROR, "truncated log file \"%s\"", (const char*) file->fileName);

		currentLength += length;
		}

	const UCHAR *end = (const UCHAR*) block + block->length;

	if (end[-1] != (srlEnd | LOW_BYTE_FLAG))
		return NULL;

	firstBlockNumber = block->blockNumber;
	
	return block;
}

void SerialLogWindow::write(SerialLogBlock *block)
{
	uint32 length = ROUNDUP(block->length, sectorSize);
	uint32 offset = (int) (origin + ((UCHAR*) block - buffer));
	ASSERT(length <= bufferLength);
	file->write(offset, length, block);
	++log->windowWrites;
}


SerialLogBlock* SerialLogWindow::findLastBlock(SerialLogBlock *first)
{
	int length = bufferLength - currentLength;

	if (length)
		{
		length = file->read(origin + currentLength, length, buffer + currentLength);
		length = length / sectorSize * sectorSize;
		currentLength += length;
		}

	uint64 blockNumber = first->blockNumber;
	
	for (SerialLogBlock *prior = first, *end = (SerialLogBlock*) (buffer + currentLength), *block;; prior = block)
		{
		block = NEXT_BLOCK(prior);
		UCHAR *endBlock = (UCHAR*) block + block->length;

		if (endBlock < (UCHAR*) end &&
			 block->creationTime == (uint32) log->creationTime &&
			 block->blockNumber != blockNumber + 1)
			{
			printf("Serial Log possible gap: " I64FORMAT " - " I64FORMAT "\n", blockNumber + 1, block->blockNumber);
			//SerialLogControl control(log);
			//control.printBlock(prior);
			}
		
		if (endBlock > (UCHAR*) end || 
			 block->creationTime != (uint32) log->creationTime || 
			 block->blockNumber != ++blockNumber)
			{
			currentLength = (int) (((const UCHAR*) block) - buffer);
			lastBlockNumber = prior->blockNumber;

			return lastBlock = prior;
			}
		
		if (endBlock[-1] != (srlEnd | LOW_BYTE_FLAG))
			{
			Log::log("damaged serial log block " I64FORMAT "\n", block->blockNumber);
			currentLength = (int) ((const UCHAR*) block - buffer);
			lastBlockNumber = prior->blockNumber;

			return lastBlock = prior;
			}
		
		//SerialLogControl validation(log);
		//validation.validate(this, block);
		}

	return NULL;
}

SerialLogBlock* SerialLogWindow::findBlock(uint64 blockNumber)
{
	for (SerialLogBlock *block = firstBlock(); block < (SerialLogBlock*) (buffer + currentLength);
		 block = NEXT_BLOCK(block))
		if (block->blockNumber == blockNumber)
			return block;

	NOT_YET_IMPLEMENTED;

	return NULL;
}

SerialLogBlock* SerialLogWindow::nextBlock(SerialLogBlock *block)
{
	SerialLogBlock *nextBlk = NEXT_BLOCK(block);

	if (nextBlk < (SerialLogBlock*) (buffer + currentLength) &&
		 nextBlk->creationTime == (uint32) log->creationTime &&
		 nextBlk->blockNumber == block->blockNumber + 1)
		{
		//ASSERT(validate(nextBlk));
		return nextBlk;
		}

	return NULL;
}

SerialLogBlock* SerialLogWindow::nextAvailableBlock(SerialLogBlock *block)
{
	SerialLogBlock *nextBlk = NEXT_BLOCK(block);

	if ((UCHAR*) nextBlk >= bufferEnd)
		return NULL;

	lastBlock = nextBlk;

	return nextBlk;
}

int64 SerialLogWindow::getNextFileOffset()
{
	SerialLogBlock *end = NEXT_BLOCK(lastBlock);

	return origin + (UCHAR*) end - buffer;
}

int64 SerialLogWindow::getNextVirtualOffset()
{
	return virtualOffset + currentLength;
}


void SerialLogWindow::setBuffer(UCHAR *newBuffer)
{
	if ( (buffer = newBuffer) )
		{
		bufferEnd = buffer + bufferLength;
		warningTrack = bufferEnd - 1;
		}
	else
		bufferEnd = warningTrack = buffer;
}

void SerialLogWindow::activateWindow(bool read)
{
	Sync sync(&log->syncWrite, "SerialLogWindow::activateWindow");
	sync.lock(Exclusive);
	++inUse;

	if (buffer)
		{
		ASSERT(firstBlock()->blockNumber == firstBlockNumber);
		return;
		}

	setBuffer(log->allocBuffer());

	if (read)
		{
		ASSERT(currentLength > 0 && currentLength <= (uint32) SRL_WINDOW_SIZE);
		file->read(origin, currentLength, buffer);
		++log->windowReads;
		lastBlock = (SerialLogBlock*) (buffer + lastBlockOffset);
		ASSERT(firstBlock()->blockNumber == firstBlockNumber);
		}
}

void SerialLogWindow::deactivateWindow()
{
	Sync sync(&log->syncWrite, "SerialLogWindow::deactivateWindow");
	sync.lock(Exclusive);
	ASSERT(inUse > 0);
	--inUse;
}

void SerialLogWindow::addRef(void)
{
	++useCount;
}

void SerialLogWindow::release(void)
{
	--useCount;
}

void SerialLogWindow::setLastBlock(SerialLogBlock* block)
{
	ASSERT(block->blockNumber >= firstBlock()->blockNumber);
	ASSERT(block->blockNumber <= log->nextBlockNumber);
	lastBlock = block;
	lastBlockOffset = (int) ((UCHAR*) block - buffer);
	lastBlockNumber = block->blockNumber;
	currentLength = lastBlockOffset + block->length;
}

uint64 SerialLogWindow::getVirtualOffset()
{
	return (virtualOffset);
}


void SerialLogWindow::print(void)
{
	Log::debug("  Window#" I64FORMAT "- blocks " I64FORMAT ":" I64FORMAT " len %d, in-use %d, useCount %d, buffer %p, virtoff " I64FORMAT "\n",
	           virtualOffset / SRL_WINDOW_SIZE, firstBlockNumber, lastBlockNumber, 
	           currentLength, inUse, useCount, buffer, virtualOffset);
}

bool SerialLogWindow::validate(SerialLogBlock* block)
{
	ASSERT((UCHAR*) block >= buffer && (UCHAR*) block < buffer + bufferLength);
	ASSERT((UCHAR*) block + block->length < buffer + bufferLength);
    ASSERT(block->blockNumber >= firstBlockNumber);
	ASSERT(block->blockNumber <= lastBlockNumber);

	return true;
}

bool SerialLogWindow::validate(const UCHAR* pointer)
{
	ASSERT(pointer >= buffer && pointer < buffer + bufferLength);
	
	return true;
}
