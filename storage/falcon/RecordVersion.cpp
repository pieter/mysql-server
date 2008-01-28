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

// RecordVersion.cpp: implementation of the RecordVersion class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "Database.h"
#include "Configuration.h"
#include "RecordVersion.h"
#include "Transaction.h"
#include "Table.h"
#include "Connection.h"
#include "SerialLogControl.h"
#include "Stream.h"
#include "Dbb.h"
#include "RecordScavenge.h"
#include "Format.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RecordVersion::RecordVersion(Table *tbl, Format *format, Transaction *trans, Record *oldVersion) :
	Record (tbl, format)
{
	virtualOffset = 0;
	transaction   = trans;
	transactionId = transaction->transactionId;
	savePointId   = transaction->curSavePointId;
	superceded    = false;

	if ((priorVersion = oldVersion))
		{
		priorVersion->addRef();
		recordNumber = oldVersion->recordNumber;
		
		if (trans == priorVersion->getTransaction())
			oldVersion->setSuperceded (true);
		}
	else
		recordNumber = -1;
}

RecordVersion::~RecordVersion()
{
	state = recDeleting;
	Record *prior = priorVersion;
	priorVersion = NULL;

	// Avoid recursion here. May crash from too many levels
	// if the same record is updated too often and quickly.
	
	while (prior)
		prior = prior->releaseNonRecursive();
}

// Release the priorRecord reference without doing it recursively.
// The caller needs to do this for what is returned it is if not null;

Record* RecordVersion::releaseNonRecursive()
{
	Record *prior = NULL;

	if (useCount == 1)
		{
		prior = priorVersion;
		priorVersion = NULL;
		}

	release();

	return prior;
}

Record* RecordVersion::fetchVersion(Transaction * trans)
{
	// Unless the record is at least as old as the transaction, it's not for us

	Transaction *recTransaction = transaction;

	if (state != recLock)
		{
		if (IS_READ_COMMITTED(trans->isolationLevel))
			{
			int state = (recTransaction) ? recTransaction->state : 0;
			
			if (!transaction || state == Committed || recTransaction == trans)
				return (getRecordData()) ? this : NULL;
			}
		// else IS_REPEATABLE_READ(trans->isolationLevel)
		else if (transactionId <= trans->transactionId)
			{
			if (trans->visible(recTransaction, transactionId, FOR_READING))
				return (getRecordData()) ? this : NULL;
			}
		}

	if (!priorVersion)
		return NULL;
		
	return priorVersion->fetchVersion(trans);
}

Record* RecordVersion::rollback()
{
	if (superceded)
		return NULL;

	return format->table->rollbackRecord (this);
}

bool RecordVersion::isVersion()
{
	return true;
}

/*
 *	Parent transaction is now fully mature (and about to go
 *	away).  Cleanup any multiversion stuff.
 */

void RecordVersion::commit()
{
	transaction = NULL;
	poke();
}

// Scavenge record versions by the scavenger thread.  Return true if the
// record is a scavenge candidate

bool RecordVersion::scavenge(RecordScavenge *recordScavenge)
{
	if (useCount != 1)
		return false;

	if (transaction || (transactionId >= recordScavenge->transactionId))
		{
		format->table->activeVersions = true;

		if (priorVersion)
			priorVersion->scavenge(recordScavenge);

		return false;
		}

	if (priorVersion)
		format->table->expungeRecordVersions(this, recordScavenge);

	return true;
}

// Scavenge record versions replaced within a savepoint.

void RecordVersion::scavenge(TransId targetTransactionId, int oldestActiveSavePointId)
{
	if (!priorVersion)
		return;

	Record *rec = priorVersion;
	Record *ptr = NULL;
	
	// Loop through versions 'till we find somebody rec (or run out of versions looking
	
	for (; rec && rec->getTransactionId() == targetTransactionId && rec->getSavePointId() >= savePointId;
		  rec = rec->getPriorVersion())
		{
		ptr = rec;
#ifdef CHECK_RECORD_ACTIVITY
		rec->active = false;
#endif
		Transaction *trans = rec->getTransaction();

		if (trans)
			trans->removeRecord( (RecordVersion*) rec);
		}
	
	// If we didn't find anyone, there's nothing to do
	
	if (!ptr)
		return;
	
	// There are intermediate versions to collapse.  Splice the unnecessary ones out of the loop
	
	Record *prior = priorVersion;
	prior->addRef();
	setPriorVersion(rec);
	//ptr->setPriorVersion(NULL);
	ptr->state = recEndChain;
	format->table->garbageCollect(prior, this, transaction, false);
	prior->release();
}

Record* RecordVersion::getPriorVersion()
{
	return priorVersion;
}

Record* RecordVersion::getGCPriorVersion(void)
{
	return (state == recEndChain) ? NULL : priorVersion;
}

void RecordVersion::setSuperceded(bool flag)
{
	superceded = flag;
}

Transaction* RecordVersion::getTransaction()
{
	return transaction;
}

bool RecordVersion::isSuperceded()
{
	return superceded;
}

void RecordVersion::setPriorVersion(Record *oldVersion)
{
	if (oldVersion)
		{
		ASSERT(oldVersion->state != recLock);
		oldVersion->addRef();
		}

	if (priorVersion)
		priorVersion->release();

	priorVersion = oldVersion;
}

TransId RecordVersion::getTransactionId()
{
	return transactionId;
}

int RecordVersion::getSavePointId()
{
	return savePointId;
}

void RecordVersion::setVirtualOffset(uint64 offset)
{
	virtualOffset = offset;
}

uint64 RecordVersion::getVirtualOffset()
{
	return (virtualOffset);
}

int RecordVersion::thaw()
{
	int bytesRestored = 0;
	Transaction *trans = transaction;
	
	// Nothing to do if the record is no longer chilled
	
	if (state != recChilled)
		return size;
		
	// First, try to thaw from the serial log. If transaction->writePending is 
	// true, then the record data can be restored from the serial log. If writePending
	// is false, then the record data has been written to the data pages.
	
	bool wasWritePending = (trans) ? trans->writePending : false;

	if (trans && trans->writePending)
		{
		trans->addRef();
		bytesRestored = trans->thaw(this);
		
		if (bytesRestored == 0)
			trans->thaw(this);

		trans->release();
		}
	
	// The record data is no longer available in the serial log, so zap the
	// virtual offset and restore from the data page.
		
	if (state != recChilled)
		return size;
		
	bool recordFetched = false;

	if (bytesRestored == 0)
		{
		Stream stream;
		Table *table = format->table;
		
		if (table->dbb->fetchRecord(table->dataSection, recordNumber, &stream))
			{
			bytesRestored = setEncodedRecord(&stream, true);
			recordFetched = true;
			}
			
		if (bytesRestored > 0)
			{
			virtualOffset = 0;
			table->debugThawedRecords++;
			table->debugThawedBytes += bytesRestored;
			
			if (table->debugThawedBytes >= table->database->configuration->recordChillThreshold)
				{
				Log::debug("Record Thaw/Fetch: table=%-5ld records=%7ld  bytes=%8ld\n",table->tableId, table->debugThawedRecords, table->debugThawedBytes);
				table->debugThawedRecords = 0;
				table->debugThawedBytes = 0;
				}
			}
		}
		
	if (bytesRestored <= 0)
		Log::debug("RecordVersion::thaw: writePending %d, was %d, recordFetched %d, data %p\n",
					trans->writePending, wasWritePending, recordFetched, data.record);

	ASSERT(bytesRestored > 0 || data.record == NULL);
	state = recData;
		
	return bytesRestored;
}

/***
char* RecordVersion::getRecordData()
{
	if (state == recChilled)
		thaw();
		
	return data.record;
}
***/

void RecordVersion::print(void)
{
	Log::debug("  %p\tId %d, enc %d, state %d, tid %d, use %d, grp %d, prior %p\n",
			this, recordNumber, encoding, state, transactionId, useCount,
			generation, priorVersion);
	
	if (priorVersion)
		priorVersion->print();
}
