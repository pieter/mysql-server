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

// SerialLogTransaction.cpp: implementation of the SerialLogTransaction class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "SerialLogTransaction.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "SerialLogWindow.h"
#include "Transaction.h"
#include "Database.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SerialLogTransaction::SerialLogTransaction(SerialLog *serialLog, TransId transId) //: SerialLogAction(serialLog)
{
	log = serialLog;
	flushing = false;

	transactionId = transId;
	state = sltUnknown;
	window = NULL;
	finished = false;
	ordered = false;
	transaction = NULL;
	blockNumber = maxBlockNumber = minBlockNumber = physicalBlockNumber = 0;
	xidLength = 0;
	xid = NULL;
}

SerialLogTransaction::~SerialLogTransaction()
{
	if (transaction)
		transaction->release();

	if (window)
		window->release();
		
	log->transactionDelete(this);
	delete [] xid;
}

void SerialLogTransaction::commit()
{
	ASSERT(!transaction || transaction->transactionId == transactionId);
	SerialLogControl control(log);
	window->activateWindow(true);
	SerialLogBlock *block = (SerialLogBlock*) (window->buffer + blockOffset);
	control.setWindow(window, block, recordOffset);
	window->deactivateWindow();
	finished = false;
	int windows = 0;
	
	for (SerialLogWindow *w = window; w; w = w->next)
		++windows;
	
	log->maxWindows = MAX(log->maxWindows, windows);
	int skipped = 0;
	int processed = 0;
	
	for (SerialLogRecord *record; !finished && (record = control.nextRecord());)
		if (record->transactionId == transactionId)
			{
			++processed;
			record->commit();
			}
		else
			++skipped;
	
	++log->commitsComplete;

	if (transaction)
		{
		transaction->fullyCommitted();
		transaction->release();
		transaction = NULL;
		}
}

void SerialLogTransaction::rollback()
{
	SerialLogControl control(log);
	window->activateWindow(true);
	SerialLogBlock *block = (SerialLogBlock*) (window->buffer + blockOffset);
	control.setWindow(window, block, recordOffset);
	window->deactivateWindow();
	finished = false;

	for (SerialLogRecord *record; !finished && (record = control.nextRecord());)
		if (record->transactionId == transactionId)
			record->rollback();
}

void SerialLogTransaction::setStart(const UCHAR *record, SerialLogBlock *block, SerialLogWindow *win)
{
	if (win != window)
		{
		win->addRef();
		
		if (window)
			window->release();
		}
		
	window = win;
	blockNumber = block->blockNumber;
	minBlockNumber = blockNumber;
	maxBlockNumber = blockNumber;
	blockOffset = (int) ((UCHAR*) block - window->buffer);
	recordOffset = (int) (record - block->data);
}

void SerialLogTransaction::setState(sltState newState)
{
	state = newState;
}

void SerialLogTransaction::setFinished()
{
	finished = true;
}

bool SerialLogTransaction::isRipe()
{
	return (state == sltCommitted ||
		    state == sltRolledBack);
}

void SerialLogTransaction::doAction()
{
	if (state == sltCommitted)
		commit();
	else
		rollback();

}

void SerialLogTransaction::preRecovery()
{
	if (state == sltUnknown)
		setState(sltRolledBack);
}

bool SerialLogTransaction::completedRecovery()
{
	if (state == sltPrepared)
		return false;

	return true;
}

uint64 SerialLogTransaction::getBlockNumber()
{
	return blockNumber;
}


void SerialLogTransaction::setPhysicalBlock()
{
	uint64 nextBlockNumber = log->nextBlockNumber;
	
	if (nextBlockNumber > physicalBlockNumber)
		{
		physicalBlockNumber = nextBlockNumber;
		maxBlockNumber = MAX(physicalBlockNumber, maxBlockNumber);
		}
}

void SerialLogTransaction::setXID(int length, const UCHAR* xidPtr)
{
	xidLength = length;
	xid = new UCHAR[xidLength];
	memcpy(xid, xidPtr, xidLength);
}

bool SerialLogTransaction::isXidEqual(int testLength, const UCHAR* test)
{
	if (testLength != xidLength)
		return false;
	
	return memcmp(xid, test, xidLength) == 0;
}

void SerialLogTransaction::setTransaction(Transaction* trans)
{
	if (!transaction)
		{
		transaction = trans;
		transaction->addRef();
		}
}
