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

// SerialLogControl.cpp: implementation of the SerialLogControl class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SerialLogControl.h"
#include "SerialLogWindow.h"
#include "SerialLogTransaction.h"
#include "SQLError.h"
#include "Log.h"
#include "Dbb.h"

#define GET_BYTE	((input < inputEnd) ? *input++ : getByte())

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SerialLogControl::SerialLogControl(SerialLog *serialLog)
{
	log = serialLog;
	inputWindow = NULL;
	debug = false;
	singleBlock = false;
	lastCheckpoint = 0;
	records[0] = NULL;
	version = (serialLog->recovering) ? 0 : srlCurrentVersion;
	
	for (int n = 1; n < srlMax; ++n)
		{
		SerialLogRecord *manager = getRecordManager(n);
		manager->log = serialLog;
		manager->control = this;
		records[n] = manager;
		}
}

SerialLogControl::~SerialLogControl()
{
	if (inputWindow)
		inputWindow->deactivateWindow();
}

SerialLogRecord* SerialLogControl::getRecordManager(int which)
{
	switch (which)
		{
		case srlSwitchLog:
			return &switchLog;

		case srlCommit:
			return &commit;

		case srlRollback:
			return &rollback;

		case srlPrepare:
			return &prepare;

		case srlDataUpdate:
			return &dataUpdate;

		case srlDelete:
			return &deleteData;

		case srlIndexUpdate:
			return &indexUpdate;

		case srlWordUpdate:
			return &wordUpdate;

		case srlRecordStub:
			return &recordStub;

		case srlSequence:
			return &sequence;

		case srlCheckpoint:
			return &checkpoint;

		case srlBlobUpdate:
			return &blobUpdate;

		case srlDropTable:
			return &dropTable;

		case srlCreateSection:
			return &createSection;

		case srlSectionPage:
			return &sectionPage;

		case srlFreePage:
			return &freePage;

		case srlSectionIndex:
			return &recordLocator;

		case srlDataPage:
			return &dataPage;

		case srlIndexAdd:
			return &indexAdd;

		case srlIndexDelete:
			return &indexDelete;

		case srlIndexPage:
			return &indexPage;

		case srlInversionPage:
			return &inversionPage;

		case srlCreateIndex:
			return &createIndex;
			
		case srlDeleteIndex:
			return &deleteIndex;
			
		case srlVersion:
			return &logVersion;
		
		case srlUpdateRecords:
			return &updateRecords;
			
		case srlUpdateIndex:
			return &updateIndex;
		
		case srlSequencePage:
			return &sequencePage;
			
		case srlSectionPromotion:
			return &sectionPromotion;
			
		case srlSectionLine:
			return &sectionLine;
		
		case srlOverflowPages:
			return &overflowPages;
			
		case srlCreateTableSpace:
			return &createTableSpace;
			
		case srlDropTableSpace:
			return &dropTableSpace;
			
		case srlBlobDelete:
			return &blobDelete;
			
		case srlSession:
			return &session;
			
		case srlUpdateBlob:
			return &updateBlob;
			
		default:
			ASSERT(false);
		}
}

void SerialLogControl::setWindow(SerialLogWindow *window, SerialLogBlock *block, int offset)
{
	//ASSERT(window->validate(block));
	// SerialLogWindow *priorWindow = inputWindow;
	// SerialLogBlock *priorBlock = inputBlock;
	//ASSERT(!priorWindow || priorWindow->validate(priorBlock));

	if (inputWindow != window)
		{
		if (inputWindow)
			inputWindow->deactivateWindow();

		if ((inputWindow = window))
			inputWindow->activateWindow(true);
		}

	Sync sync(&log->syncWrite, "SerialLogControl::setWindow");
	sync.lock(Shared);
	//inputWindow->validate(block);
	//ASSERT(inputWindow->validate(block));
	inputBlock = block;
	input = inputBlock->data;
	inputEnd = (const UCHAR*) inputBlock + block->length;
	singleBlock = false;
	
	if (inputBlock == log->writeBlock && log->recordIncomplete)
		inputEnd = log->recordStart;
	
	//ASSERT(inputWindow->validate(inputEnd));	
	version = srlVersion0;

	if (input < inputEnd)
		{
		int type = getInt();
		
		if (type == srlVersion)
			version = getInt();
		}

	//ASSERT(version == srlCurrentVersion);
	input = inputBlock->data + offset;
}

int SerialLogControl::getInt()
{
	UCHAR c = GET_BYTE;
	int number = (c & 0x40) ? -1 : 0;

	for (;;)
		{
		number = (number << 7) | (c & 0x7f);

		if (c & LOW_BYTE_FLAG)
			break;

		c = GET_BYTE;
		}

	return number;
}

UCHAR SerialLogControl::getByte()
{
	if (input < inputEnd)
		return *input++;

	uint64 blockNumber = inputBlock->blockNumber + 1;
	SerialLogBlock *block = inputWindow->nextBlock(inputBlock);

	if (block)
		{
		ASSERT(block->blockNumber == blockNumber);
		//validate(inputWindow, block);
		setWindow(inputWindow, block, 0);
		}
	else
		{
		SerialLogWindow *window;
		
		if (inputWindow && inputWindow->next && inputWindow->next->firstBlockNumber == blockNumber)
			window = inputWindow->next;
		else
			window = log->findWindowGivenBlock(blockNumber);
		
		if (!window)
			{
			Log::debug("Can't find serial log block " I64FORMAT "\n", blockNumber);
			log->printWindows();
			
			throw SQLError(LOG_ERROR, "Serial log overrun");
			}

		window->activateWindow(true);
		//validate(window, window->firstBlock());
		setWindow(window, window->firstBlock(), 0);
		ASSERT(inputBlock->blockNumber == blockNumber);
		window->deactivateWindow();
		}
	
	if (log->recoveryPhase == 2 && blockNumber == lastCheckpoint && log->tracePage)
		Log::debug("*** Checkpoint block ***\n");
		
	if (debug)
		Log::debug("\nProcessing serial log block " I64FORMAT ", read block " I64FORMAT "\n\n", 
			   blockNumber, inputBlock->readBlockNumber);

	return *input++;
}

bool SerialLogControl::atEnd()
{
	if (input < inputEnd)
		return false;

	if (singleBlock)
		return true;

	SerialLogBlock *block = inputWindow->nextBlock(inputBlock);

	if (block)
		return false;

	SerialLogWindow *window = log->findWindowGivenBlock(inputBlock->blockNumber + 1);

	if (window)
		return false;

	return true;
}

SerialLogRecord* SerialLogControl::nextRecord()
{
	if (atEnd())
		return NULL;

	ASSERT(*input >= (LOW_BYTE_FLAG | srlEnd) && *input < (LOW_BYTE_FLAG | srlMax));
	recordStart = input;
	UCHAR type = getInt();

	while ((type == srlEnd) || (type == srlVersion))
		{
		if (debug)
			Log::debug("Recovery %s\n", (type == srlEnd) ? "end" : "version");
			
		if (atEnd())
			return NULL;

		// As of srlVersion1, each srl record starts with a srlVersion.

		if (type == srlVersion)
			version = getInt();
			
		recordStart = input;
		type = getInt();
		}

	ASSERT(version > 0);
	SerialLogRecord *record = records[type];
	record->read();
	
	if (debug)
		record->print();
		
	return record;
}

const UCHAR* SerialLogControl::getData(int length)
{
	ASSERT(length >= 0);

	if (input + length > inputEnd)
		throw SQLError(LOG_ERROR, "data overrun in serial log");

	const UCHAR *data = input;
	input += length;

	return data;
}

SerialLogTransaction* SerialLogControl::getTransaction(TransId transactionId)
{
	SerialLogTransaction *transaction = log->findTransaction(transactionId);

	if (transaction)
		return transaction;

	transaction = log->getTransaction(transactionId);
	transaction->setStart(recordStart, inputBlock, inputWindow);

	return transaction;
}

int SerialLogControl::getOffset()
{
	return (int) (input - (const UCHAR*) inputBlock);
}

uint64 SerialLogControl::getBlockNumber()
{
	return inputBlock->blockNumber;
}

void SerialLogControl::validate(SerialLogWindow *window, SerialLogBlock *block)
{
	setWindow(window, block, 0);
	
	for (;;)
		{
		if (atEnd())
			return;

		ASSERT(*input >= (LOW_BYTE_FLAG | srlEnd) && *input < (LOW_BYTE_FLAG | srlMax));
		recordStart = input;
		UCHAR type = getInt();

		while (type == srlEnd)
			{
			ASSERT((uint32) (input - (UCHAR*) inputBlock) == inputBlock->length);
			return;
			}

		SerialLogRecord *record = getRecordManager(type);
		record->read();
		}
}

void SerialLogControl::setVersion(int newVersion)
{
	version = newVersion;
}

void SerialLogControl::fini(void)
{
	if (inputWindow)
		{
		inputWindow->deactivateWindow();
		inputWindow = NULL;
		}
}

void SerialLogControl::printBlock(SerialLogBlock *block)
{
	setWindow(NULL, block, 0);
	Log::debug("Serial Log Block " I64FORMAT ", length %d, read block " I64FORMAT "\n", 
			inputBlock->blockNumber, inputBlock->length, inputBlock->readBlockNumber);
	singleBlock = true;
	
	for (SerialLogRecord *record; (record = nextRecord());)
		record->print();
}

void SerialLogControl::haveCheckpoint(int64 blockNumber)
{
	lastCheckpoint = (blockNumber) ? blockNumber : inputBlock->blockNumber;
}

bool SerialLogControl::isPostFlush(void)
{
	return inputBlock->blockNumber > lastCheckpoint;
}
