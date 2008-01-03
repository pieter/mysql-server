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

// SerialLog.cpp: implementation of the SerialLog class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "SerialLog.h"
#include "Sync.h"
#include "SQLError.h"
#include "Thread.h"
#include "SerialLogFile.h"
#include "SerialLogControl.h"
#include "SerialLogWindow.h"
#include "SerialLogTransaction.h"
#include "Threads.h"
#include "Thread.h"
#include "Database.h"
#include "Dbb.h"
#include "Scheduler.h"
#include "Bitmap.h"
#include "Log.h"
#include "RecoveryPage.h"
#include "RecoveryObjects.h"
#include "SRLVersion.h"
#include "InfoTable.h"
#include "Configuration.h"
#include "TableSpaceManager.h"
#include "TableSpace.h"
#include "Gopher.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

static const int TRACE_PAGE = 0;

extern uint falcon_gopher_threads;

//static const int windowBuffers = 10;
static bool debug;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SerialLog::SerialLog(Database *db, JString schedule, int maxTransactionBacklog) : Schedule (schedule)
{
	database = db;
	defaultDbb = database->dbb;
	tableSpaceManager = database->tableSpaceManager;
	creationTime = database->creationTime;
	maxTransactions = maxTransactionBacklog;
	file1 = new SerialLogFile(database);
	file2 = new SerialLogFile(database);
	logControl = new SerialLogControl(this);
	active = false;
	nextBlockNumber = 0;
	firstWindow = NULL;
	lastWindow = NULL;
	freeWindows = NULL;
	writeWindow = NULL;
	writeBlock = NULL;
	memset(transactions, 0, sizeof(transactions));
	earliest = latest = NULL;
	lastFlushBlock = 1;
	lastReadBlock = 0;
	eventNumber = 0;
	endSrlQueue = NULL;
	writer = NULL;
	finishing = false;
	logicalFlushes = 0;
	physicalFlushes = 0;
	highWaterBlock = 0;
	nextLimboTransaction = 0;
	bufferSpace = NULL;
	recoveryPages = NULL;
	recoveryIndexes = NULL;
	recoverySections = NULL;
	recoveryPhase = 0;
	tracePage = TRACE_PAGE;
	chilledRecords = 0;
	chilledBytes = 0;
	windowReads = 0;
	priorWindowWrites = 0;
	windowWrites = 0;
	priorWindowReads = 0;
	maxWindows = 0;
	commitsComplete = 0;
	backlogStalls = 0;
	priorBacklogStalls = 0;
	priorCount = 0;
	priorDelta = 0;
	priorCommitsComplete = 0;
	priorWrites = 0;
	lastBlockWritten = 0;
	recoveryBlockNumber = 0;
	recovering = false;
	blocking = false;
	writeError = false;
	windowBuffers = MAX(database->configuration->serialLogWindows, 10);
	tableSpaceInfo = NULL;
	memset(tableSpaces, 0, sizeof(tableSpaces));
	syncWrite.setName("SerialLog::syncWrite");
	syncSections.setName("SerialLog::syncSections");
	syncIndexes.setName("SerialLog::syncIndexes");
	syncGopher.setName("SerialLog::syncGopher");
	syncUpdateStall.setName("SerialLog::syncUpdateStall");
	pending.syncObject.setName("SerialLog::pending transactions");
	gophers = NULL;
	
	for (uint n = 0; n < falcon_gopher_threads; ++n)
		{
		Gopher *gopher = new Gopher(this);
		gopher->next = gophers;
		gophers = gopher;
		}
}

SerialLog::~SerialLog()
{
	delete file1;
	delete file2;
	delete logControl;
	delete recoveryIndexes;
	delete recoverySections;
	SerialLogWindow *window;

	while ( (window = firstWindow) )
		{
		firstWindow = window->next;
		delete window;
		}
		
	while (( window = freeWindows) )
		{
		freeWindows = window->next;
		delete window;
		}

	while (!buffers.isEmpty())
		buffers.pop();
		
	delete [] bufferSpace;
	
	while (tableSpaceInfo)
		{
		TableSpaceInfo *info = tableSpaceInfo;
		tableSpaceInfo = info->next;
		delete info;
		}
	
	for (Gopher *gopher; (gopher = gophers);)
		{
		gophers = gopher->next;
		delete gopher;
		}
}

void SerialLog::open(JString fileRoot, bool createFlag)
{
	file1->open(fileRoot + ".fl1", createFlag);
	file2->open(fileRoot + ".fl2", createFlag);

	int sectorSize = file1->sectorSize;
	int len = windowBuffers * SRL_WINDOW_SIZE + sectorSize;
	bufferSpace = new UCHAR[len];

#ifdef HAVE_purify
	bzero(bufferSpace, len);
#endif

	UCHAR *space = (UCHAR*) (((UIPTR) bufferSpace + sectorSize - 1) / sectorSize * sectorSize);

	for (int n = 0; n < windowBuffers; ++n, space += SRL_WINDOW_SIZE)
		buffers.push(space);

	if (createFlag)
		initializeLog(1);
}

void SerialLog::start()
{
	logControl->session.append(recoveryBlockNumber, 0);
	
	for (Gopher *gopher = gophers; gopher; gopher = gopher->next)
		gopher->start();
}

void SerialLog::recover()
{
	Log::log("Recovering database %s ...\n", (const char*) defaultDbb->fileName);
	Sync sync(&syncWrite, "SerialLog::recover");
	sync.lock(Exclusive);
	recovering = true;
	recoveryPhase = 0;
	
	// See if either or both files have valid blocks

	SerialLogWindow *window1 = allocWindow(file1, 0);
	SerialLogWindow *window2 = allocWindow(file2, 0);
	SerialLogBlock *block1 = window1->readFirstBlock();
	SerialLogBlock *block2 = window2->readFirstBlock();

	if (!block1)
		{
		window1->deactivateWindow();
		release(window1);
		window1 = NULL;
		}

	if (!block2)
		{
		window2->deactivateWindow();
		release(window2);
		window2 = NULL;
		}

	// Pick a window to start the search for the last block

	SerialLogWindow *recoveryWindow = NULL;
	SerialLogWindow *otherWindow = NULL;

	if (block1)
		{
		if (block2)
			{
			if (block1->blockNumber > block2->blockNumber)
				{
				recoveryWindow = window1;
				otherWindow = window2;
				window1->next = NULL;
				lastWindow = window2->next = window1;
				firstWindow = window1->prior = window2;
				window2->prior = NULL;
				}
			else
				{
				recoveryWindow = window2;
				otherWindow = window1;
				}
			}
		else
			recoveryWindow = window1;
		}
	else if (block2)
		recoveryWindow = window2;
	else
		{
		Log::log("No recovery block found\n");
		initializeLog(1);
		
		return;
		}

	// Look through windows looking for the very last block

	SerialLogBlock *recoveryBlock = recoveryWindow->firstBlock();
	recoveryBlockNumber = recoveryBlock->blockNumber;
	SerialLogBlock *lastBlock = findLastBlock(recoveryWindow);
	Log::log("first recovery block is " I64FORMAT "\n", 
			(otherWindow) ? otherWindow->firstBlock()->blockNumber : recoveryBlockNumber);
	Log::log("last recovery block is " I64FORMAT "\n", lastBlock->blockNumber);

	if (otherWindow)
		{
		SerialLogBlock *block = findLastBlock(otherWindow);

		if (block && block->blockNumber != (recoveryBlockNumber - 1))
			throw SQLError(LOG_ERROR, "corrupted serial log");
		
		SerialLogWindow *window = findWindowGivenBlock(block->blockNumber);
		window->deactivateWindow();
		}

	// Find read block

	uint64 readBlockNumber = lastBlock->readBlockNumber;
	Log::log("recovery read block is " I64FORMAT "\n", readBlockNumber);
	nextBlockNumber = lastBlock->blockNumber + 1;
	lastWindow->deactivateWindow();
	SerialLogWindow *window = findWindowGivenBlock(readBlockNumber);
	
	if (!window)
		throw SQLError(LOG_ERROR, "can't find recovery block %d\n", (int) readBlockNumber);
		
	window->activateWindow(true);
	SerialLogBlock *block = window->findBlock(readBlockNumber);
	SerialLogControl control(this);
	control.debug = debug;
	writeWindow = NULL;
	writeBlock = NULL;
	control.setWindow(window, block, 0);
	SerialLogRecord *record;
	pass1 = true;
	recoveryPages = new RecoveryObjects(this);
	recoverySections = new RecoveryObjects(this);
	recoveryIndexes = new RecoveryObjects(this);
	recoveryPhase = 1;
	
	// Make a first pass finding records, transactions, etc.

	while ( (record = control.nextRecord()) )
		record->pass1();

	//control.debug = false;
	pass1 = false;
	control.setWindow(window, block, 0);

	recoveryPages->reset();
	recoveryIndexes->reset();
	recoverySections->reset();
	recoveryPhase = 2;

	// Next, make a second pass to reallocate any necessary pages

	while ( (record = control.nextRecord()) )
		record->pass2();

	recoveryPages->reset();
	recoveryIndexes->reset();
	recoverySections->reset();

	// Now mark any transactions still pending as rolled back

	control.setWindow(window, block, 0);
	SerialLogTransaction *transaction;
	recoveryPhase = 3;

	for (transaction = running.first; transaction; transaction = transaction->next)
		transaction->preRecovery();

	// Make a third pass doing things

	while ( (record = control.nextRecord()) )
		record->redo();
		
	for (SerialLogTransaction *action, **ptr = &running.first; (action = *ptr);)
		if (action->completedRecovery())
			{
			running.remove(action);
			delete action;
			}
		else
			ptr = &action->next;

	control.fini();
	window->deactivateWindow();
	window = findWindowGivenBlock(lastBlock->blockNumber);
	window->activateWindow(true);
	
	if ( (writeBlock = window->nextAvailableBlock(lastBlock)) )
		{
		writeWindow = window;
		initializeWriteBlock(writeBlock);
		}
	else
		{
		SerialLogFile *file = (window->file == file1) ? file2 : file1;
		writeWindow = allocWindow(file, 0);
		writeBlock = writeWindow->firstBlock();
		window->deactivateWindow();
		initializeWriteBlock(writeBlock);
		writeWindow->firstBlockNumber = writeBlock->blockNumber;
		}

	//preFlushBlock = writeBlock->blockNumber;
	delete recoveryPages;
	delete recoverySections;
	delete recoveryIndexes;
	recoveryPages = NULL;
	recoveryIndexes = NULL;
	recoverySections = NULL;
	
	for (window = firstWindow; window; window = window->next)
		if (!(window->inUse == 0 || window == writeWindow))
			ASSERT(false);
		
	recovering = false;
	lastFlushBlock = writeBlock->blockNumber;
	checkpoint(true);
	
	for (TableSpaceInfo *info = tableSpaceInfo; info; info = info->next)
		{
		info->sectionUseVector.zap();
		info->indexUseVector.zap();
		}
		
	Log::log("Recovery complete\n");
	recoveryPhase = 0;
}

void SerialLog::overflowFlush(void)
{
	++eventNumber;		
	++logicalFlushes;

	// OK, we're going to do some writing.  Start by locking the serial log

	*writePtr++ = srlEnd | LOW_BYTE_FLAG;
	writeBlock->length = (int) (writePtr - (UCHAR*) writeBlock);
	writeWindow->setLastBlock(writeBlock);
	lastReadBlock = writeBlock->readBlockNumber = getReadBlock();
	//ASSERT(writeWindow->validate(writeBlock));
	
	// Keep track of what needs to be written

	SerialLogWindow *flushWindow = writeWindow;
	SerialLogBlock *flushBlock = writeBlock;
	createNewWindow();

	mutex.lock();
	
	try
		{
		flushWindow->write(flushBlock);
		lastBlockWritten = flushBlock->blockNumber;
		}
	catch (...)
		{
		writeError = true;
		mutex.unlock();
		throw;
		}	
		
	//ASSERT(flushWindow->validate(flushBlock));
	++physicalFlushes;
	mutex.unlock();		
	
	highWaterBlock = flushBlock->blockNumber;
	ASSERT(writer || !srlQueue);
}

uint64 SerialLog::flush(bool forceNewWindow, uint64 commitBlockNumber, Sync *clientSync)
{
	Sync sync(&syncWrite, "SerialLog::flush");
	Sync *syncPtr = clientSync;
	
	if (!syncPtr)
		{
		sync.lock(Exclusive);
		syncPtr = &sync;
		}

	++eventNumber;
	Thread *thread = Thread::getThread("SerialLog::flush");
	++logicalFlushes;
	thread->commitBlockNumber = commitBlockNumber;

	// Add ourselves to the queue to preserve our place in order

	thread->srlQueue = NULL;

	if (endSrlQueue)
		endSrlQueue->srlQueue = thread;
	else
		srlQueue = thread;

	endSrlQueue = thread;
	thread->wakeupType = None;

	// If there's a writer and it's not use, go to sleep
	
	if (writer && writer != thread)
		{
		syncPtr->unlock();

		for (;;)
			{
			thread->sleep();
			
			if (thread->wakeupType != None)
				break;
			}

		syncPtr->lock(Exclusive);

		if (commitBlockNumber <= highWaterBlock)
			{
			if (writer == thread || (!writer && srlQueue))
				wakeupFlushQueue(thread);

			ASSERT(writer || !srlQueue);
			syncPtr->unlock();
			
			return nextBlockNumber;
			}
		}

	// OK, we're going to do some writing.

	ASSERT(writer == NULL || writer == thread);
	writer = thread;
	*writePtr++ = srlEnd | LOW_BYTE_FLAG;
	writeBlock->length = (int) (writePtr - (UCHAR*) writeBlock);
	lastReadBlock = writeBlock->readBlockNumber = getReadBlock();

	// Keep track of what needs to be written

	SerialLogWindow *flushWindow = writeWindow;
	SerialLogBlock *flushBlock = writeBlock;

	// Prepare the next write block for use while we're writing

	if (!forceNewWindow && (writeBlock = writeWindow->nextAvailableBlock(writeBlock)))
		{
		initializeWriteBlock(writeBlock);
		writeBlock->readBlockNumber = lastReadBlock;
		}
	else
		{
		flushBlock->length = (int) (writePtr - (UCHAR*) flushBlock);
		createNewWindow();
		}

	// Everything is ready to go.  Release the locks and start writing

	mutex.lock();
	syncPtr->unlock();
	//Log::debug("Flushing log block %d, read block %d\n", (int) flushBlock->blockNumber, (int) flushBlock->readBlockNumber);
	
	try
		{
		flushWindow->write(flushBlock);
		lastBlockWritten = flushBlock->blockNumber;
		}
	catch (...)
		{
		writeError = true;
		mutex.unlock();
		throw;
		}
			
	//ASSERT(flushWindow->validate(flushBlock));
	++physicalFlushes;
	mutex.unlock();
	
	// We're done.  Wake up anyone who got flushed and pick the next writer

	syncPtr->lock(Exclusive);
	highWaterBlock = MAX(highWaterBlock, flushBlock->blockNumber);
	wakeupFlushQueue(thread);
	ASSERT(writer != thread);
	ASSERT(writer || !srlQueue);
	//ASSERT(writeWindow->validate(writeBlock));
	syncPtr->unlock();

	return nextBlockNumber;
}

void SerialLog::createNewWindow(void)
{
	// We need a new window.  Start by purging any unused windows

	while (   (firstWindow->lastBlockNumber < lastReadBlock)
	       && (firstWindow->inUse == 0)
	       && (firstWindow->useCount == 0))
		{
		release(firstWindow);
		}

	// Then figure out which file to use.  If the other isn't in use, take it

	SerialLogFile *file = (writeWindow->file == file1) ? file2 : file1;
	int64 fileOffset = 0;
	
	for (SerialLogWindow *window = firstWindow; window; window = window->next)
		if (window->file == file)
			{
			file = writeWindow->file;
			fileOffset = writeWindow->getNextFileOffset();
			break;
			}

	if (fileOffset == 0 && Log::isActive(LogInfo))
		Log::log(LogInfo, "%d: Switching log files (%d used)\n", database->deltaTime, file->highWater);

	writeWindow->deactivateWindow();
	writeWindow = allocWindow(file, fileOffset);
	writeWindow->firstBlockNumber = nextBlockNumber;
	initializeWriteBlock(writeWindow->firstBlock());
	ASSERT(writeWindow->firstBlockNumber == writeBlock->blockNumber);
	//ASSERT(writeWindow->validate(writeBlock));
}

void SerialLog::shutdown()
{
	finishing = true;
	
	for (Gopher *gopher = gophers; gopher; gopher = gopher->next)
		gopher->wakeup();

	// Wait for all gopher threads to exit
	
	Sync sync(&syncGopher, "SerialLog::shutdown");
	sync.lock(Exclusive);

	if (blocking)
		unblockUpdates();

	checkpoint(false);
	
	for (Gopher *gopher = gophers; gopher; gopher = gopher->next)
		gopher->shutdown();
}

void SerialLog::close()
{

}

void SerialLog::putData(uint32 length, const UCHAR *data)
{
	if ((writePtr + length) < writeWarningTrack)
		{
		memcpy(writePtr, data, length);
		writePtr += length;
		writeBlock->length = (int) (writePtr - (UCHAR*) writeBlock);
		writeWindow->currentLength = (int) (writePtr - writeWindow->buffer);

		return;
		}

	// Data didn't fit in block -- find out how much didn't fit, then flush the rest
	// Note: the record code is going to be overwritten by an srlEnd byte.

	int tailLength = (int) (writePtr - recordStart);
	UCHAR recordCode = *recordStart;
	writeBlock->length = (int) (recordStart - (UCHAR*) writeBlock);
	writePtr = recordStart;
	overflowFlush();

	while (writePtr + length + tailLength >= writeWarningTrack)
		overflowFlush();
		
	putVersion();

	// Restore the initial part of the record

	if (tailLength)
		{
		*writePtr++ = recordCode;

		if (tailLength > 1)
			{
			memcpy(writePtr, recordStart + 1, tailLength - 1);
			writePtr += tailLength - 1;
			}
		}

	// And finally finish the copy of data

	memcpy(writePtr, data, length);
	writePtr += length;
	writeBlock->length = (int) (writePtr - (UCHAR*) writeBlock);
	writeWindow->currentLength = (int) (writePtr - writeWindow->buffer);
	recordStart = writeBlock->data;
	//ASSERT(writeWindow->validate(writeBlock));
}

void SerialLog::startRecord()
{
	ASSERT(!recovering);

	if (writeError)
		throw SQLError(IO_ERROR, "Previous I/O error on serial log prevents further processing");

	if (writePtr == writeBlock->data)
		putVersion();

	recordStart = writePtr;
	recordIncomplete = true;
}

void SerialLog::endRecord(void)
{
	recordIncomplete = false;
}

void SerialLog::wakeup()
{	
	for (Gopher *gopher = gophers; gopher; gopher = gopher->next)
		if ((gopher->workerThread) && gopher->workerThread->sleeping)
			{
			gopher->wakeup();
			break;
			}
}

void SerialLog::release(SerialLogWindow *window)
{
	ASSERT(window->inUse == 0);
	
	if (window->buffer)
		{
		releaseBuffer(window->buffer);
		window->buffer = NULL;
		}

	if (window->next)
		window->next->prior = window->prior;
	else
		lastWindow = window->prior;

	if (window->prior)
		window->prior->next = window->next;
	else
		firstWindow = window->next;

	window->next = freeWindows;
	freeWindows = window;
}

SerialLogWindow* SerialLog::allocWindow(SerialLogFile *file, int64 origin)
{
	SerialLogWindow *window = freeWindows;

	if (window)
		{
		freeWindows = window->next;
		window->setPosition(file, origin);
		}
	else
		window = new SerialLogWindow(this, file, origin);

	window->next = NULL;

	if ( (window->prior = lastWindow) )
		lastWindow->next = window;
	else
		firstWindow = window;

	// Set the virtual byte offset of this window
	
	if (window->prior)
		window->virtualOffset = window->prior->virtualOffset + SRL_WINDOW_SIZE;
	else
		window->virtualOffset = 0;

	lastWindow = window;
	window->activateWindow(false);

	return window;
}

void SerialLog::initializeLog(int64 blockNumber)
{
	Sync sync(&syncWrite, "SerialLog::initializeLog");
	sync.lock(Exclusive);
	nextBlockNumber = blockNumber;
	writeWindow = allocWindow(file1, 0);
	writeWindow->firstBlockNumber = nextBlockNumber;
	writeWindow->lastBlockNumber = nextBlockNumber;
	initializeWriteBlock((SerialLogBlock*) writeWindow->buffer);
}

SerialLogWindow* SerialLog::findWindowGivenOffset(uint64 offset)
{
	Sync sync(&syncWrite, "SerialLog::findWindowGivenOffset");
	sync.lock(Exclusive);
	
	for (SerialLogWindow *window = firstWindow; window; window = window->next)
		if (offset >= window->virtualOffset && offset < (window->virtualOffset + window->currentLength))
			{
			window->activateWindow(true);

			return window;
			}

	Log::debug("SerialLog::findWindowGivenOffset -- can't find window\n");

	return NULL;
}

SerialLogWindow* SerialLog::findWindowGivenBlock(uint64 blockNumber)
{
	for (SerialLogWindow *window = firstWindow; window; window = window->next)
		if (blockNumber >= window->firstBlockNumber && 
			 blockNumber <= window->lastBlockNumber)
			{
			//window->activateWindow(true);
			return window;
			}

	return NULL;
}

SerialLogTransaction* SerialLog::findTransaction(TransId transactionId)
{
	Sync sync (&pending.syncObject, "SerialLog::findTransaction");
	sync.lock(Shared);

	for (SerialLogTransaction *transaction = transactions[transactionId % SLT_HASH_SIZE];
		  transaction; transaction = transaction->collision)
		if (transaction->transactionId == transactionId)
			return transaction;

	return NULL;
}

SerialLogTransaction* SerialLog::getTransaction(TransId transactionId)
{
	ASSERT(transactionId > 0);
	SerialLogTransaction *transaction = findTransaction(transactionId);

	if (transaction)
		return transaction;

	Sync sync (&pending.syncObject, "SerialLog::findTransaction");
	sync.lock(Exclusive);
	
	/***
	if (transactionId == 0 || transactionId == 41)
		printf ("SerialLog::getTransaction id "TXIDFORMAT"\n", transactionId);
	***/
	
	int slot = (int) (transactionId % SLT_HASH_SIZE);

	for (transaction = transactions[transactionId % SLT_HASH_SIZE];
		  transaction; transaction = transaction->collision)
		if (transaction->transactionId == transactionId)
			return transaction;

	transaction = new SerialLogTransaction(this, transactionId);
	transaction->collision = transactions[slot];
	transactions[slot] = transaction;
	running.append(transaction);
	
	if (writeWindow)
		{
		transaction->setStart(recordStart, writeBlock, writeWindow);
		transaction->later = NULL;
		
		if ( (transaction->earlier = latest) )
			transaction->earlier->later = transaction;
		else
			earliest = transaction;
		
		latest = transaction;
		transaction->ordered = true;
		}

	return transaction;
}

uint64 SerialLog::getReadBlock()
{
	Sync sync (&pending.syncObject, "SerialLog::getReadBlock");
	sync.lock(Shared);
	uint64 blockNumber = lastBlockWritten;
	
	if (earliest)
		{
		uint64 number = earliest->getBlockNumber();

		if (number > 0) 
			blockNumber = MIN(blockNumber, number);
		}
		
	return blockNumber; 
}

void SerialLog::transactionDelete(SerialLogTransaction *transaction)
{
	Sync sync (&pending.syncObject, "SerialLog::transactionDelete");
	sync.lock(Exclusive);
	int slot = (int) (transaction->transactionId % SLT_HASH_SIZE);

	for (SerialLogTransaction **ptr = transactions + slot; *ptr; ptr = &(*ptr)->collision)
		if (*ptr == transaction)
			{
			*ptr = transaction->collision;
			break;
			}

	if (transaction->ordered)
		{
		if (transaction->earlier)
			transaction->earlier->later = transaction->later;
		else
			earliest = transaction->later;
		
		if (transaction->later)
			transaction->later->earlier = transaction->earlier;
		else
			latest = transaction->earlier;
		
		transaction->ordered = false;
		}
}

void SerialLog::execute(Scheduler *scheduler)
{
	try
		{
		checkpoint(false);
		}
	catch (SQLException& exception)
		{
		Log::log("SerialLog checkpoint failed: %s\n", exception.getText());
		}

	getNextEvent();
	scheduler->addEvent (this);
}

void SerialLog::checkpoint(bool force)
{
	if (force || writePtr != writeBlock->data || getReadBlock() > lastReadBlock + 1 || inactions.first)
		{
		Sync sync(&syncWrite, "SerialLog::checkpoint");
		sync.lock(Shared);
		int64 blockNumber = lastBlockWritten;
		sync.unlock();
		database->flush(blockNumber);
		}
}

void SerialLog::pageCacheFlushed(int64 blockNumber)
{
	if (blockNumber)
		{
		database->sync();
		logControl->checkpoint.append(blockNumber);
		}

	if (blockNumber)
		lastFlushBlock = blockNumber; //preFlushBlock;
		
	Sync sync(&pending.syncObject, "SerialLog::pageCacheFlushed");	// pending.syncObject use for both
	sync.lock(Exclusive);
	
	for (SerialLogTransaction *transaction, **ptr = &inactions.first; (transaction = *ptr);)
		if (transaction->flushing && transaction->maxBlockNumber < lastFlushBlock)
			{
			inactions.remove(transaction);
			delete transaction;
			}
		else
			ptr = &transaction->next;
}

void SerialLog::initializeWriteBlock(SerialLogBlock *block)
{
	writeBlock = block;
	writePtr = writeBlock->data;
	writeBlock->blockNumber = nextBlockNumber++;
	writeBlock->creationTime = (uint32) creationTime;
	writeBlock->version = srlCurrentVersion;
	writeBlock->length = (int) (writePtr - (UCHAR*) writeBlock);
	writeWindow->setLastBlock(writeBlock);
	writeWarningTrack = writeWindow->warningTrack;
	//ASSERT(writeWindow->validate(writeBlock));
}


SerialLogBlock* SerialLog::findLastBlock(SerialLogWindow *window)
{
	SerialLogBlock *lastBlock = window->firstBlock();

	for (;;)
		{
		lastBlock = window->findLastBlock(lastBlock);
		SerialLogWindow *newWindow = allocWindow(window->file, window->origin + window->currentLength);

		if (window->next != newWindow)
			{
			lastWindow = newWindow->prior;
			lastWindow->next = NULL;
			newWindow->next = window->next;
			newWindow->prior = window;
			newWindow->next->prior = newWindow;
			window->next = newWindow;
			}

		SerialLogBlock *newBlock = newWindow->readFirstBlock();

		if (!newBlock || newBlock->blockNumber != lastBlock->blockNumber + 1)
			{
			newWindow->deactivateWindow();
			release(newWindow);
			break;
			}

		window->deactivateWindow();
		window = newWindow;
		lastBlock = newBlock;
		}

	return lastBlock;
}

bool SerialLog::isIndexActive(int indexId, int tableSpaceId)
{
	if (!recoveryIndexes)
		return true;
		
	return recoveryIndexes->isObjectActive(indexId, tableSpaceId);
}


bool SerialLog::isSectionActive(int sectionId, int tableSpaceId)
{
	if (!recoverySections)
		return true;
		
	return recoverySections->isObjectActive(sectionId, tableSpaceId);
}

uint32 SerialLog::appendLog(IO *shadow, int lastPage)
{
	Sync sync(&syncWrite, "SerialLog::appendLog");
	sync.lock(Exclusive);
	shadow->seek(lastPage);
	uint32 totalLength = 0;
	uint64 endBlock = flush(false, 0, NULL);

	for (uint64 blockNumber = getReadBlock(); blockNumber < endBlock; ++blockNumber)
		{
		SerialLogWindow *window = findWindowGivenBlock(blockNumber);
		
		if (window)
			{
			window->activateWindow(true);
			SerialLogBlock *block = window->findBlock(blockNumber);
			uint32 length = ROUNDUP(block->length, window->sectorSize);
			shadow->write(length, block->data);
			totalLength += length;
			window->deactivateWindow();
			}
		}

	return totalLength;
}

void SerialLog::dropDatabase()
{
	if (file1)
		file1->dropDatabase();

	if (file2)
		file2->dropDatabase();
}

void SerialLog::shutdownNow()
{

}

UCHAR* SerialLog::allocBuffer()
{
	if (!buffers.isEmpty())
		return (UCHAR*) buffers.pop();

	for (SerialLogWindow *window = firstWindow; window; window = window->next)
		if (window->buffer && window->inUse == 0)
			{
			ASSERT(window != writeWindow);
			UCHAR *buffer = window->buffer;
			window->setBuffer(NULL);

			return buffer;
			}

	ASSERT(false);

	return NULL;
}

void SerialLog::releaseBuffer(UCHAR *buffer)
{
	buffers.push(buffer);
}

void SerialLog::copyClone(JString fileRoot, int logOffset, int logLength)
{
	file1->open(fileRoot + ".nl1", true);
	defaultDbb->seek(logOffset);
	uint32 bufferLength = 32768;
	UCHAR *buffer = new UCHAR [bufferLength];
	uint32 position = 0;
	
	for (uint32 length = logLength; length > 0;)
		{
		uint32 len = MIN(length, bufferLength);
		defaultDbb->read(len, buffer);
		file1->write(position, len, (SerialLogBlock*) buffer);
		length -= len;
		position += len;
		}
	
	delete [] buffer;
	file1->close();
	file2->open(fileRoot + ".nl2", true);
	file2->close();
}

bool SerialLog::bumpPageIncarnation(int32 pageNumber, int tableSpaceId, int state)
{
	if (pageNumber == tracePage)
		printf("bumpPageIncarnation; page %d\n", tracePage);

	bool ret = recoveryPages->bumpIncarnation(pageNumber, tableSpaceId, state, pass1);
	
	if (ret && pass1)
		{
		Dbb *dbb = getDbb(tableSpaceId);
		dbb->reallocPage(pageNumber);
		}

	return ret;
}

bool SerialLog::bumpSectionIncarnation(int sectionId, int tableSpaceId, int state)
{
	return recoverySections->bumpIncarnation(sectionId, tableSpaceId, state, pass1);
}

bool SerialLog::bumpIndexIncarnation(int indexId, int tableSpaceId, int state)
{
	return recoveryIndexes->bumpIncarnation(indexId, tableSpaceId, state, pass1);
}

void SerialLog::preFlush(void)
{
	Sync sync(&pending.syncObject, "SerialLog::preFlush");
	sync.lock(Shared);
	
	for (SerialLogTransaction *action = inactions.first; action; action = action->next)
		action->flushing = true;
}

int SerialLog::recoverLimboTransactions(void)
{
	int count = 0;
	
	for (SerialLogTransaction *transaction = running.first; transaction; transaction = transaction->next)
		if (transaction->state == sltPrepared)
			++count;
	
	if (count)
		Log::log("Warning: Recovery found %d prepared transactions in limbo\n", count);

	return count;
}

void SerialLog::putVersion()
{
	*writePtr++ = srlVersion | LOW_BYTE_FLAG;
	*writePtr++ = srlCurrentVersion | LOW_BYTE_FLAG;
}

void SerialLog::wakeupFlushQueue(Thread *ourThread)
{
	//ASSERT(syncWrite.getExclusiveThread() == Thread::getThread("SerialLog::wakeupFlushQueue"));
	writer = NULL;
	Thread *thread = srlQueue;
	
	// Start by making sure we're out of the que

	for (Thread *prior = NULL; thread; prior = thread, thread = thread->srlQueue)
		if (thread == ourThread)
			{
			if (prior)
				prior->srlQueue = thread->srlQueue;
			else
				srlQueue = thread->srlQueue;
			
			if (endSrlQueue == thread)
				endSrlQueue = prior;
			
			break;
			}
	
	while ( (thread = srlQueue) )
		{
		srlQueue = thread->srlQueue;
		thread->eventNumber = ++eventNumber;

		if (thread->commitBlockNumber <= highWaterBlock)
			{
			thread->wakeupType = Shared;
			thread->wake();
			}
		else
			{
			writer = thread;
			thread->wakeupType = Exclusive;
			thread->wake();
			break;
			}
		}

	if (!srlQueue)
		endSrlQueue = NULL;
}

void SerialLog::setSectionActive(int id, int tableSpaceId)
{
	if (!recoverySections)
		return;

	getDbb(tableSpaceId);
		
	if (recovering)
		recoverySections->bumpIncarnation(id, tableSpaceId, objInUse, false);
	else
		recoverySections->deleteObject(id, tableSpaceId);
}

void SerialLog::setSectionInactive(int id, int tableSpaceId)
{
	if (!recoverySections)
		recoverySections = new RecoveryObjects(this);
	
	recoverySections->bumpIncarnation(id, tableSpaceId, objDeleted, true);
}

void SerialLog::setIndexActive(int id, int tableSpaceId)
{
	if (!recoveryIndexes)
		return;
	
	getDbb(tableSpaceId);

	if (recovering)
		recoveryIndexes->setActive(id, tableSpaceId);
	else
		recoveryIndexes->deleteObject(id, tableSpaceId);
}

void SerialLog::setIndexInactive(int id, int tableSpaceId)
{
	if (!recoveryIndexes)
		recoveryIndexes = new RecoveryObjects(this);
	
	recoveryIndexes->setInactive(id, tableSpaceId);
}

bool SerialLog::sectionInUse(int sectionId, int tableSpaceId)
{
	TableSpaceInfo *info = getTableSpaceInfo(tableSpaceId);

	return info->sectionUseVector.get(sectionId) > 0;
}

bool SerialLog::indexInUse(int indexId, int tableSpaceId)
{
	TableSpaceInfo *info = getTableSpaceInfo(tableSpaceId);
	return info->indexUseVector.get(indexId) > 0;
}

int SerialLog::getPageState(int32 pageNumber, int tableSpaceId)
{
	return recoveryPages->getCurrentState(pageNumber, tableSpaceId);
}

void SerialLog::redoFreePage(int32 pageNumber, int tableSpaceId)
{
	if (pageNumber == tracePage)
		Log::debug("Redoing free of page %d\n", pageNumber);

	Dbb *dbb = getDbb(tableSpaceId);
	dbb->redoFreePage(pageNumber);
}

void SerialLog::setPhysicalBlock(TransId transId)
{
	if (transId)
		{
		SerialLogTransaction *transaction = findTransaction(transId);
		
		if (transaction)
			transaction->setPhysicalBlock();
		}
}

void SerialLog::reportStatistics(void)
{
	if (!Log::isActive(LogInfo))
		return;
		
	Sync sync(&pending.syncObject, "SerialLog::reportStatistics");
	sync.lock(Shared);
	/***
	int count = 0;
	uint64 minBlockNumber = writeBlock->blockNumber;
		
	for (SerialLogTransaction *action = inactions.first; action; action = action->next)
		{
		++count;
		
		if (action->minBlockNumber < minBlockNumber)
			minBlockNumber = action->minBlockNumber;
		}
	***/
	
	uint64 minBlockNumber = (earliest) ? earliest->minBlockNumber : writeBlock->blockNumber;
	int count = inactions.count;
	uint64 delta = writeBlock->blockNumber - minBlockNumber;
	int reads = windowReads - priorWindowReads;
	priorWindowReads = windowReads;
	int writes = windowWrites - priorWindowWrites;
	priorWindowWrites = windowWrites;
	int windows = maxWindows;
	int commits = commitsComplete - priorCommitsComplete;
	int stalls = backlogStalls - priorBacklogStalls;
	priorCommitsComplete = commitsComplete;
	maxWindows = 0;
	sync.unlock();
	windows = MAX(windows, 1);

	if (count != priorCount || (uint64) delta != priorDelta || priorWrites != writes)
		{
		Log::log(LogInfo, "%d: SerialLog: %d reads, %d writes, %d transactions, %d completed, %d stalls, " I64FORMAT " blocks, %d windows\n", 
				 database->deltaTime, reads, writes, count, commits, stalls, delta, windows);
		priorCount = count;
		priorDelta = delta;
		priorWrites = writes;
		priorBacklogStalls = backlogStalls;
		}	
}

void SerialLog::getSerialLogInfo(InfoTable* tableInfo)
{
	Sync sync(&pending.syncObject, "SerialLog::getSerialLogInfo");
	sync.lock(Shared);
	int numberTransactions = 0;
	uint64 minBlockNumber = writeBlock->blockNumber;
		
	for (SerialLogTransaction *action = inactions.first; action; action = action->next)
		{
		++numberTransactions;
		
		if (action->minBlockNumber < minBlockNumber)
			minBlockNumber = action->minBlockNumber;
		}
	
	int64 delta = writeBlock->blockNumber - minBlockNumber;
	sync.unlock();
	
	Sync syncWindows(&syncWrite, "SerialLog::getSerialLogInfo");
	syncWindows.lock(Shared);
	int windows = 0;
	int buffers = 0;
	
	for (SerialLogWindow *window = firstWindow; window; window = window->next)
		{
		++windows;
		
		if (window->buffer)
			++buffers;
		}

	syncWindows.unlock();
	int n = 0;
//	tableInfo->putString(n++, database->name);
	tableInfo->putInt(n++, numberTransactions);
	tableInfo->putInt64(n++, delta);
	tableInfo->putInt(n++, windows);
	tableInfo->putInt(n++, buffers);
	tableInfo->putRecord();	
}

void SerialLog::commitByXid(int xidLength, const UCHAR* xid)
{
	Sync sync(&pending.syncObject, "SerialLog::commitByXid");
	sync.lock(Shared);
		
	for (SerialLogTransaction *action = inactions.first; action; action = action->next)
		{
		SerialLogTransaction *transaction = (SerialLogTransaction*) action;
		
		if (transaction->isXidEqual(xidLength, xid))
			transaction->commit();
		}
}

void SerialLog::rollbackByXid(int xidLength, const UCHAR* xid)
{
	Sync sync(&pending.syncObject, "SerialLog::rollbackByXid");
	sync.lock(Shared);
		
	for (SerialLogTransaction *action = inactions.first; action; action = action->next)
		{
		SerialLogTransaction *transaction = (SerialLogTransaction*) action;
		
		if (transaction->isXidEqual(xidLength, xid))
			transaction->rollback();
		}
}

void SerialLog::preCommit(Transaction* transaction)
{
	SerialLogTransaction *serialLogTransaction = findTransaction(transaction->transactionId);
	
	if (!serialLogTransaction)
		{
		Sync writeSync(&syncWrite, "SerialLog::preCommit");
		writeSync.lock(Exclusive);
		startRecord();
		serialLogTransaction = getTransaction(transaction->transactionId);
		}
		
	Sync sync (&pending.syncObject, "SerialLog::activate");
	sync.lock(Exclusive);
	running.remove(serialLogTransaction);
	pending.append(serialLogTransaction);
}

void SerialLog::printWindows(void)
{
	Log::debug("Serial Log Windows:\n");
	
	for (SerialLogWindow *window = firstWindow; window; window = window->next)
		window->print();
}

Dbb* SerialLog::getDbb(int tableSpaceId)
{
	if (tableSpaceId == 0)
		return defaultDbb;
		
	return tableSpaceManager->getTableSpace(tableSpaceId)->dbb;
}

Dbb* SerialLog::findDbb(int tableSpaceId)
{
	if (tableSpaceId == 0)
		return defaultDbb;
	
	TableSpace *tableSpace = tableSpaceManager->findTableSpace(tableSpaceId);
	
	if (!tableSpace)
		return NULL;
	
	return tableSpace->dbb;
}

TableSpaceInfo* SerialLog::getTableSpaceInfo(int tableSpaceId)
{
	TableSpaceInfo *info;
	int slot = tableSpaceId %SLT_HASH_SIZE;
	
	for (info = tableSpaces[slot]; info; info = info->collision)
		if (info->tableSpaceId == tableSpaceId)
			return info;
	
	info = new TableSpaceInfo;
	info->tableSpaceId = tableSpaceId;
	info->collision = tableSpaces[slot];
	tableSpaces[slot] = info;
	info->next = tableSpaceInfo;
	tableSpaceInfo = info;
	
	return info;
}

void SerialLog::updateSectionUseVector(uint sectionId, int tableSpaceId, int delta)
{
	TableSpaceInfo *info = getTableSpaceInfo(tableSpaceId);
	
	if (sectionId >= info->sectionUseVector.length)
		info->sectionUseVector.extend(sectionId + 10);
	
	//info->sectionUseVector.vector[sectionId] += delta;
	INTERLOCKED_ADD((volatile INTERLOCK_TYPE*)(info->sectionUseVector.vector + sectionId), delta);
}

void SerialLog::updateIndexUseVector(uint indexId, int tableSpaceId, int delta)
{
	TableSpaceInfo *info = getTableSpaceInfo(tableSpaceId);
	
	if (indexId >= info->indexUseVector.length)
		info->indexUseVector.extend(indexId + 10);
	
	info->indexUseVector.vector[indexId] += delta;
}

void SerialLog::preUpdate(void)
{
	if (!blocking)
		return;
	
	Sync sync(&syncUpdateStall, "SerialLog::preUpdate");
	sync.lock(Shared);
}

uint64 SerialLog::getWriteBlockNumber(void)
{
	return writeBlock->blockNumber;
}

void SerialLog::unblockUpdates(void)
{
	Sync sync(&pending.syncObject, "SerialLog::unblockUpdates");
	sync.lock(Exclusive);
	
	if (blocking)
		{
		blocking = false;
		syncUpdateStall.unlock();
		}
}

void SerialLog::blockUpdates(void)
{
	Sync sync(&pending.syncObject, "SerialLog::blockUpdates");
	sync.lock(Exclusive);
	
	if (!blocking)
		{
		blocking = true;
		syncUpdateStall.lock(NULL, Exclusive);
		++backlogStalls;
		}
}

int SerialLog::getBlockSize(void)
{
	return file1->sectorSize;
}

int	SerialLog::recoverGetNextLimbo(int xidSize, unsigned char *xid)
	{
	SerialLogTransaction *transaction = nextLimboTransaction;

	if (!transaction)
		return 0;

	if (transaction->xidLength == xidSize)
		memcpy(xid, transaction->xid, xidSize);

	for (transaction = transaction->next; transaction; transaction = transaction->next)
		if (transaction->state == sltPrepared)
			break;

	nextLimboTransaction = transaction;
	return 1;
}

