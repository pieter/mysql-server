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

// Transaction.cpp: implementation of the Transaction class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "Transaction.h"
#include "Configuration.h"
#include "Database.h"
#include "Dbb.h"
#include "Connection.h"
#include "Table.h"
#include "RecordVersion.h"
#include "SQLError.h"
#include "Sync.h"
#include "PageWriter.h"
#include "Table.h"
#include "Interlock.h"
#include "SavePoint.h"
#include "IOx.h"
#include "DeferredIndex.h"
#include "TransactionManager.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "InfoTable.h"
#include "Thread.h"
#include "Format.h"
#include "LogLock.h"

extern uint		falcon_lock_wait_timeout;

static const char *stateNames [] = {
	"Active",
	"Limbo",
	"Committed",
	"RolledBack",
	"Us",
	"Visible",
	"Invisible",
	"WasActive",
	"Deadlock",
	"Available",
	"Initial",
	"ReadOnly"
	};

static const int INDENT = 5;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Transaction::Transaction(Connection *cnct, TransId seq)
{
	states = NULL;
	statesAllocated = 0;
	savePoints = NULL;
	freeSavePoints = NULL;
	useCount = 1;
	initialize(cnct, seq);
}

void Transaction::initialize(Connection* cnct, TransId seq)
{
	Sync sync(&syncObject, "Transaction::initialize");
	sync.lock(Exclusive);
	ASSERT(savePoints == NULL);
	ASSERT(freeSavePoints == NULL);
	connection = cnct;
	isolationLevel = connection->isolationLevel;
	mySqlThreadId = connection->mySqlThreadId;
	database = connection->database;
	TransactionManager *transactionManager = database->transactionManager;
	systemTransaction = database->systemConnection == connection;
	transactionId = seq;
	firstRecord = NULL;
	lastRecord = NULL;
	recordPtr = &firstRecord;
	chillPoint = &firstRecord;
	dependencies = 0;
	commitTriggers = false;
	hasUpdates = false;
	hasLocks = false;
	writePending = true;
	pendingPageWrites = false;
	waitingFor = NULL;
	curSavePointId = 0;
	deferredIndexes = NULL;
	deferredIndexCount = 0;
	xidLength = 0;
	xid = NULL;
	scanIndexCount = 0;
	totalRecordData = 0;
	totalRecords = 0;
	chilledRecords = 0;
	chilledBytes = 0;
	thawedRecords = 0;
	thawedBytes = 0;
	debugThawedRecords = 0;
	debugThawedBytes = 0;
	committedRecords = 0;
	numberStates = 0;
	blockedBy = 0;
	inList = true;
	thread = NULL;
	syncObject.setName("Transaction::syncObject");
	syncActive.setName("Transaction::syncActive");
	syncIndexes.setName("Transaction::syncIndexes");
	//scavenged = false;
	
	if (seq == 0)
		{
		state = Available;
		systemTransaction = false;
		oldestActive = 0;
		writePending = false;

		return;
		}
	
	for (int n = 0; n < LOCAL_SAVE_POINTS; ++n)
		{
		localSavePoints[n].next = freeSavePoints;
		freeSavePoints = localSavePoints + n;
		}
	
	startTime = database->deltaTime;
	blockingRecord = NULL;
	thread = Thread::getThread("Transaction::init");
	syncActive.lock(NULL, Exclusive);
	Transaction *oldest = transactionManager->findOldest();
	oldestActive = (oldest) ? oldest->transactionId : transactionId;
	int count = transactionManager->activeTransactions.count;
	
	if (count > statesAllocated)
		{
		delete [] states;
		statesAllocated = count;
		states = new TransState[statesAllocated];
		}

	if (count)
		for (Transaction *transaction = transactionManager->activeTransactions.first; transaction; transaction = transaction->next)
			if (transaction->isActive() && 
				 !transaction->systemTransaction &&
				 transaction->transactionId < transactionId)
				{
				Sync syncDependency(&transaction->syncObject, "Transaction::initialize");
				syncDependency.lock(Shared);

				if (transaction->isActive() && 
					 !transaction->systemTransaction &&
					 transaction->transactionId < transactionId)
					{
					transaction->addRef();
					INTERLOCKED_INCREMENT(transaction->dependencies);
					TransState *state = states + numberStates;
					state->transaction = transaction;
					state->transactionId = transaction->transactionId;
					state->state = transaction->state;
					++numberStates;
					ASSERT(transaction->transactionId == state->transactionId);
					}
				}

	state = Active;
}

Transaction::~Transaction()
{
	ASSERT(dependencies == 0);
	
	if (state == Active)
		{
		Log::debug("Deleting apparently active transaction %d\n", transactionId);
		ASSERT(false);
		
		if (syncActive.ourExclusiveLock())
			syncActive.unlock();
		}

	if (inList)
		database->transactionManager->removeTransaction(this);

	delete [] states;
	delete [] xid;
	chillPoint = &firstRecord;
	recordPtr = &firstRecord;

	for (RecordVersion *record; (record = firstRecord);)
		{
		removeRecord(record);
		}
	
	releaseSavePoints();
	
	if (deferredIndexes)
		{
		Sync sync(&syncIndexes, "Transaction::~Transaction");
		sync.lock(Exclusive);

		releaseDeferredIndexes();
		}
}

void Transaction::commit()
{
	ASSERT((firstRecord != NULL) || (chillPoint == &firstRecord));

	if (!isActive())
		throw SQLEXCEPTION (RUNTIME_ERROR, "transaction is not active");

	releaseSavePoints();

	if (!hasUpdates)
		{
		commitNoUpdates();
		
		return;
		}

	TransactionManager *transactionManager = database->transactionManager;
	addRef();
	Log::log(LogXARecovery, "%d: Commit transaction %d\n", database->deltaTime, transactionId);

	if (state == Active)
		{
		Sync sync(&syncIndexes, "Transaction::commit");
		sync.lock(Shared);
		
		for (DeferredIndex *deferredIndex= deferredIndexes; deferredIndex;  
			 deferredIndex = deferredIndex->nextInTransaction)
			if (deferredIndex->index)
				database->dbb->logIndexUpdates(deferredIndex);
		
		sync.unlock();
		database->dbb->logUpdatedRecords(this, firstRecord);

		if (pendingPageWrites)
			database->pageWriter->waitForWrites(this);
		}
			
	++transactionManager->committed;

	if (hasLocks)
		releaseRecordLocks();

	Sync syncActiveTransactions(&transactionManager->activeTransactions.syncObject, "Transaction::commit");
	database->serialLog->preCommit(this);
	state = Committed;
	syncActive.unlock();

	for (RecordVersion *record = firstRecord; record; record = record->nextInTrans)
		if (!record->isSuperceded() && record->state != recLock)
			record->format->table->updateRecord (record);

	if (commitTriggers)
		for (RecordVersion *record = firstRecord; record; record = record->nextInTrans)
			if (!record->isSuperceded() && record->state != recLock)
				record->format->table->postCommit (this, record);

	releaseDependencies();
	database->flushInversion(this);
	syncActiveTransactions.lock(Exclusive);
	transactionManager->activeTransactions.remove(this);
	syncActiveTransactions.unlock();
	
	for (RecordVersion *record = firstRecord; record; record = record->nextInTrans)
		if (!record->priorVersion)
			++record->format->table->cardinality;
		else if (record->state == recDeleted && record->format->table->cardinality > 0)
			--record->format->table->cardinality;
			
	Sync syncCommitted(&transactionManager->committedTransactions.syncObject, "Transaction::commit");
	syncCommitted.lock(Exclusive);
	transactionManager->committedTransactions.append(this);
	syncCommitted.unlock();
	database->commit(this);

	delete [] xid;
	xid = NULL;
	xidLength = 0;
	
	// If there's no reason to stick around, just go away
	
	if ((dependencies == 0) && !writePending)
		commitRecords();

	connection = NULL;
	
	// Add ourselves to the list of lingering committed transactions
	
	release();
}


void Transaction::commitNoUpdates(void)
{
	TransactionManager *transactionManager = database->transactionManager;
	addRef();
	ASSERT(!deferredIndexes);
	++transactionManager->committed;
	
	if (deferredIndexes)
		{
		Sync sync(&syncIndexes, "Transaction::commitNoUpdates");
		sync.lock(Exclusive);
		releaseDeferredIndexes();
		}

	if (hasLocks)
		releaseRecordLocks();

	Sync syncActiveTransactions(&transactionManager->activeTransactions.syncObject, "Transaction::commitNoUpdates");
	syncActiveTransactions.lock(Shared);
	state = CommittingReadOnly;
	releaseDependencies();
	
	Sync sync(&syncObject, "Transaction::commitNoUpdates");
	sync.lock(Exclusive);

	if (dependencies)
		{
		transactionManager->expungeTransaction(this);
	
		// There is a tiny race condition between another thread releasing a dependency and
		// TransactionManager::expungeTransaction doing thte same thing.  So spin.
		
		if (dependencies)
			{
			Thread *thread = NULL;
			
			for (int n = 0; dependencies && n < 10; ++n)
				if (thread)
					thread->sleep(1);
				else
					thread = Thread::getThread("Transaction::commitNoUpdates");

			ASSERT(dependencies == 0);
			}
		}
		
	sync.unlock();
	delete [] xid;
	xid = NULL;
	xidLength = 0;
	
	// If there's no reason to stick around, just go away
	
	connection = NULL;
	transactionId = 0;
	writePending = false;
	syncActiveTransactions.unlock();
	syncActive.unlock();
	release();
	state = Available;
}

void Transaction::rollback()
{
	RecordVersion *stack = NULL;
	RecordVersion *record;

	if (!isActive())
		throw SQLEXCEPTION (RUNTIME_ERROR, "transaction is not active");

	//Sync sync(&syncObject, "Transaction::rollback");
	//sync.lock(Exclusive);
	
	if (deferredIndexes)
		{
		Sync sync(&syncIndexes, "Transaction::rollback");
		sync.lock(Exclusive);
		releaseDeferredIndexes();
		}
		
	releaseSavePoints();
	TransactionManager *transactionManager = database->transactionManager;
	Transaction *rollbackTransaction = transactionManager->rolledBackTransaction;
	//state = RolledBack;
	chillPoint = &firstRecord;
	recordPtr = &firstRecord;
	totalRecordData = 0;
	totalRecords = 0;

	// Rollback pending record versions from newest to oldest in case
	// there are multiple record versions on a prior record chain

	while (firstRecord)
		{
		record = firstRecord;
		firstRecord = record->nextInTrans;
		record->prevInTrans = NULL;
		record->nextInTrans = stack;
		stack = record;
		}
		
	lastRecord = NULL;

	while (stack)
		{
		record = stack;
		stack = record->nextInTrans;
		record->nextInTrans = NULL;

		if (record->state == recLock)
			record->format->table->unlockRecord(record, false);
		else
			record->rollback();
		
		record->transaction = rollbackTransaction;
		record->release();
		}

	ASSERT(writePending);
	state = RolledBack;
	writePending = false;
	releaseDependencies();
	syncActive.unlock();
	
	if (hasUpdates)
		database->serialLog->preCommit(this);
		
	database->rollback(this);
	delete [] xid;
	xid = NULL;
	xidLength = 0;
	
	Sync syncActiveTransactions (&transactionManager->activeTransactions.syncObject, "Transaction::rollback");
	syncActiveTransactions.lock (Exclusive);
	++transactionManager->rolledBack;
	
	while (dependencies)
		transactionManager->expungeTransaction(this);
		
	ASSERT(dependencies == 0);
	inList = false;
	transactionManager->activeTransactions.remove(this);
	syncActiveTransactions.unlock();
	//sync.unlock();
	release();
}


void Transaction::expungeTransaction(Transaction * transaction)
{
	TransId oldId = transactionId;
	int orgState = state;
	ASSERT(states != NULL || numberStates == 0);
	int n = 0;
	
	for (TransState *s = states, *end = s + numberStates; s < end; ++s, ++n)
		if (s->transaction == transaction)
			{
			if (COMPARE_EXCHANGE_POINTER(&s->transaction, transaction, NULL))
				transaction->releaseDependency();

			break;
			}
}

void Transaction::prepare(int xidLen, const UCHAR *xidPtr)
{
	if (state != Active)
		throw SQLEXCEPTION (RUNTIME_ERROR, "transaction is not active");

	Log::log(LogXARecovery, "Prepare transaction %d: xidLen = %d\n", transactionId, xidLen);
	releaseSavePoints();

	xidLength = xidLen;

	if (xidLength)
		{
		xid = new UCHAR[xidLength];
		memcpy(xid, xidPtr, xidLength);
		}
		
	database->pageWriter->waitForWrites(this);
	state = Limbo;
	database->dbb->prepareTransaction(transactionId, xidLength, xid);

	Sync sync(&syncIndexes, "Transaction::prepare");
	sync.lock(Shared);
	
	for (DeferredIndex *deferredIndex= deferredIndexes; deferredIndex;  
		deferredIndex = deferredIndex->nextInTransaction)
		if (deferredIndex->index)
			database->dbb->logIndexUpdates(deferredIndex);
	
	sync.unlock();
	database->dbb->logUpdatedRecords(this, firstRecord);

	if (pendingPageWrites)
		database->pageWriter->waitForWrites(this);

	if (hasLocks)
		releaseRecordLocks();
}

void Transaction::chillRecords()
{
	// chillPoint points to a pointer to the first non-chilled record. If any
	// records have been thawed, then reset chillPoint.
	
	if (thawedRecords)
		chillPoint = &firstRecord;
		
	uint32 chilledBefore = chilledRecords;
	uint64 totalDataBefore = totalRecordData;
	
	database->dbb->logUpdatedRecords(this, *chillPoint, true);
	
	Log::debug("Record Chill:      trxId=%-5ld records=%7ld  bytes=%8ld\n",
				transactionId, chilledRecords-chilledBefore, (uint32)(totalDataBefore-totalRecordData), committedRecords);
}

int Transaction::thaw(RecordVersion * record)
{
	// Nothing to do if record is no longer chilled
	
	if (record->state != recChilled)
		return record->size;
		
	// Get pointer to record data in serial log

	SerialLogControl control(database->dbb->serialLog);
	
	// Thaw the record then update the total record data bytes for this transaction
	
	ASSERT(record->transactionId == transactionId);
	bool thawed;
	int bytesRestored = control.updateRecords.thaw(record, &thawed);
	
	if (bytesRestored > 0 && thawed)
		{
		totalRecordData += bytesRestored;
		thawedRecords++;
		thawedBytes += bytesRestored;
		debugThawedRecords++;
		debugThawedBytes += bytesRestored;
		}

	if (debugThawedBytes >= database->configuration->recordChillThreshold)
		{
	//	Log::debug("%06ld Record Thaw/SRL:   recId:%8ld  addr:%p  vofs:%8llx  trxId:%6ld  total recs:%7ld  chilled:%7ld  thawed:%7ld  bytes:%8ld  commits:%6ld\n",
	//				(uint32)clock(), record->recordNumber, record, record->virtualOffset, transactionId, totalRecords, chilledRecords, thawedRecords, (uint32)totalRecordData, committedRecords);
		Log::debug("Record Thaw/SRL:   trxId=%-5ld records=%7ld  bytes=%8ld\n", transactionId, debugThawedRecords, debugThawedBytes);
		debugThawedRecords = 0;
		debugThawedBytes = 0;
		}
	
	return bytesRestored;
}

void Transaction::thaw(DeferredIndex * deferredIndex)
{
	SerialLogControl control(database->dbb->serialLog);
	control.updateIndex.thaw(deferredIndex);
}

void Transaction::addRecord(RecordVersion * record)
{
	ASSERT(record->recordNumber >= 0);
	hasUpdates = true;
	
	if (record->state == recLock)
		hasLocks = true;
		
	totalRecordData += record->getEncodedSize();
	totalRecords++;
	
	if (totalRecordData > database->configuration->recordChillThreshold)
		{
		// Chill all records except the current record, which may be part of an update or insert

		UCHAR saveState = record->state;
		
		if (record->state != recLock && record->state != recChilled)
			record->state = recNoChill;
			
		chillRecords();

		if (record->state == recNoChill)
			record->state = saveState;
		}

	record->addRef();
	ASSERT((lastRecord == NULL) || (recordPtr == &lastRecord->nextInTrans));
	record->nextInTrans = NULL;
	record->prevInTrans = lastRecord;
	lastRecord = record;
	*recordPtr = record;
	recordPtr = &record->nextInTrans;
}

void Transaction::removeRecord(RecordVersion *record)
{
	RecordVersion **ptr;

	if (record->nextInTrans)
		record->nextInTrans->prevInTrans = record->prevInTrans;
	else
		{
		ASSERT(lastRecord == record);
		lastRecord = record->prevInTrans;
		}

	if (record->prevInTrans)
		ptr = &record->prevInTrans->nextInTrans;
	else
		{
		ASSERT(firstRecord == record);
		ptr = &firstRecord;
		}

	*ptr = record->nextInTrans;
	record->prevInTrans = NULL;
	record->nextInTrans = NULL;
	record->transaction = NULL;

	if (recordPtr == &record->nextInTrans)
		recordPtr = ptr;

	for (SavePoint *savePoint = savePoints; savePoint; savePoint = savePoint->next)
		if (savePoint->records == &record->nextInTrans)
			savePoint->records = ptr;

	if (chillPoint == &record->nextInTrans)
		chillPoint = ptr;

	// Adjust total record data count
	if (record->state != recChilled)
		{
		uint32 size = record->getEncodedSize();
		if (totalRecordData >= size)
			totalRecordData -= size;
		}

	if (totalRecords > 0)
		totalRecords--;
	
	record->release();
}

/***
@brief		Determine if changes by another transaction are visible to this.
@details	This function is called for Consistent-Read transactions to determine
			if the sent trans was committed before this transaction started.  If not,
			it is invisible to this transaction.
***/

bool Transaction::visible(Transaction * transaction, TransId transId, int forWhat)
{
	// If the transaction is NULL, it is long gone and therefore committed

	if (!transaction)
		return true;

	// If we're the transaction in question, consider us committed

	if (transId == transactionId)
		return true;

	// If we're the system transaction, just use the state of the other transaction

	if (database->systemConnection->transaction == this)
		return transaction->state == Committed;

	// If the other transaction is not yet committed, the trans is not visible.

	if (transaction->state != Committed)
		return false;

	// The other transaction is committed.  
	// If this is READ_COMMITTED, it is visible.

	if (   IS_READ_COMMITTED(isolationLevel)
		|| (   IS_WRITE_COMMITTED(isolationLevel)
		    && (forWhat == FOR_WRITING)))
		return true;

	// This is REPEATABLE_READ
	ASSERT (IS_REPEATABLE_READ(isolationLevel));

	// If the transaction started after we did, consider the transaction active

	if (transId > transactionId)
		return false;

	// If the transaction was active when we started, use it's state at that point

	for (int n = 0; n < numberStates; ++n)
		if (states [n].transactionId == transId)
			return false;

	return true;
}

void Transaction::releaseDependencies()
{
	if (!numberStates)
		return;

	for (TransState *state = states, *end = states + numberStates; state < end; ++state)
		{
		Transaction *transaction = state->transaction;

		if (transaction)
			{
			if (transaction->transactionId != state->transactionId)
				{
				Transaction *transaction = database->transactionManager->findTransaction(state->transactionId);
				ASSERT(transaction == NULL);
				}

			if (COMPARE_EXCHANGE_POINTER(&state->transaction, transaction, NULL))
				{
				ASSERT(transaction->transactionId == state->transactionId);
				transaction->releaseDependency();
				}
			}
		}
}

/*
 *  Transaction is fully mature and about to go away.
 *  Fully commit all records
 */

void Transaction::commitRecords()
{
	RecordVersion *recordList = firstRecord;
	
	//if (COMPARE_EXCHANGE(&cleanupNeeded, (INTERLOCK_TYPE) 1, (INTERLOCK_TYPE) 0))
	if (recordList && COMPARE_EXCHANGE_POINTER(&firstRecord, recordList, NULL))
		{
		chillPoint = &firstRecord;
		recordPtr = &firstRecord;
		lastRecord = NULL;

		for (RecordVersion *record; (record = recordList);)
			{
			ASSERT (record->useCount > 0);
			recordList = record->nextInTrans;
			record->nextInTrans = NULL;
			record->prevInTrans = NULL;
			record->commit();
			record->release();
			committedRecords++;
			}
		}
}

/***
@brief		Get the relative state between this transaction and 
			the transaction associated with a record version.
***/

State Transaction::getRelativeState(Record* record, uint32 flags)
{
	blockingRecord = record;
	State state = getRelativeState(record->getTransaction(), record->getTransactionId(), flags);
	blockingRecord = NULL;

	return state;
}

/***
@brief		Get the relative state between this transaction and another.
***/

State Transaction::getRelativeState(Transaction *transaction, TransId transId, uint32 flags)
{
	if (transactionId == transId)
		return Us;

	// A record may still have the transId even after the trans itself has been deleted.
	
	if (!transaction)
		{
		// All calls to getRelativeState are for the purpose of writing.
		// So only ConsistentRead can get CommittedInvisible.

		if (IS_CONSISTENT_READ(isolationLevel))
			{
			// If the transaction is no longer around, and the record is,
			// then it must be committed.

			if (transactionId < transId)
				return CommittedInvisible;

			// Be sure it was not active when we started.

			for (int n = 0; n < numberStates; ++n)
				if (states [n].transactionId == transId)
					return CommittedInvisible;
			}

		return CommittedVisible;
		}

	if (transaction->isActive())
		{
		if (flags & DO_NOT_WAIT)
			return Active;

		// If waiting would cause a deadlock, don't try it

		for (Transaction *trans = transaction->waitingFor; trans; trans = trans->waitingFor)
			if (trans == this)
				return Deadlock;

		// OK, add a reference to the transaction to keep the object around, then wait for it go go away

		transaction->addRef();
		waitingFor = transaction;
		transaction->waitForTransaction();
		waitingFor = NULL;
		transaction->release();

		return WasActive;			// caller will need to re-fetch
		}

	if (transaction->state == Committed)
		{
		// Return CommittedVisible if the other trans has a lower TransId and 
		// it was committed when we started.
		
		if (visible (transaction, transId, FOR_WRITING))
			return CommittedVisible;

		return CommittedInvisible;
		}

	return (State) transaction->state;
}

void Transaction::dropTable(Table* table)
{
	Sync sync(&syncIndexes, "Transaction::dropTable");
	sync.lock(Exclusive);

	releaseDeferredIndexes(table);

	// Keep exclusive lock to avoid race condition with writeComplete
	
	for (RecordVersion **ptr = &firstRecord, *rec; (rec = *ptr);)
		if (rec->format->table == table)
			removeRecord(rec);
		else
			ptr = &rec->nextInTrans;
}

void Transaction::truncateTable(Table* table)
{
	Sync sync(&syncIndexes, "Transaction::truncateTable");
	sync.lock(Exclusive);

	releaseDeferredIndexes(table);

	// Keep exclusive lock to avoid race condition with writeComplete
	
	for (RecordVersion **ptr = &firstRecord, *rec; (rec = *ptr);)
		if (rec->format->table == table)
			removeRecord(rec);
		else
			ptr = &rec->nextInTrans;
}

bool Transaction::hasUncommittedRecords(Table* table)
{
	for (RecordVersion *rec = firstRecord; rec; rec = rec->nextInTrans)
		if (rec->format->table == table)
			return true;
	
	return false;
}

void Transaction::writeComplete(void)
{
	ASSERT(writePending);
	ASSERT(state == Committed);
	Sync sync(&syncIndexes, "Transaction::writeComplete");
	sync.lock(Exclusive);
	
	releaseDeferredIndexes();

	// Keep the synIndexes lock to avoid a race condition with dropTable
	
	if (dependencies == 0)
		commitRecords();

	writePending = false;
	sync.unlock();
}

bool Transaction::waitForTransaction(TransId transId)
{
	TransactionManager *transactionManager = database->transactionManager;
	Sync syncActiveTransactions(&transactionManager->activeTransactions.syncObject, "Transaction::waitForTransaction");
	syncActiveTransactions.lock(Shared);
	Transaction *transaction;
	
	// If the transaction is still active, find it

	for (transaction = transactionManager->activeTransactions.first; transaction; transaction = transaction->next)
		if (transaction->transactionId == transId)
			break;
	
	// If the transction is no longer active, see if it is committed
	
	if (!transaction || transaction->state == Available)
		return true;
		
	if (transaction->state == Committed)
		return true;

	// If waiting would cause a deadlock, don't try it

	for (Transaction *trans = transaction->waitingFor; trans; trans = trans->waitingFor)
		if (trans == this)
			return true;

	// OK, add a reference to the transaction to keep the object around, then wait for it to go away

	waitingFor = transaction;
	transaction->addRef();
	syncActiveTransactions.unlock();
	transaction->waitForTransaction();
	transaction->release();
	waitingFor = NULL;

	return transaction->state == Committed;
}

void Transaction::waitForTransaction()
{
	/***
	Thread *exclusiveThread = syncActive.getExclusiveThread();
	
	if (exclusiveThread)
		{
		char buffer[1024];
		connection->getCurrentStatement(buffer, sizeof(buffer));
		Log::debug("Blocking on %d: %s\n", exclusiveThread->threadId, buffer);
		}
	***/
	
	Sync sync(&syncActive, "Transaction::waitForTransaction");
	sync.lock(Shared, falcon_lock_wait_timeout * 1000);
}

void Transaction::addRef()
{
	INTERLOCKED_INCREMENT(useCount);
}

int Transaction::release()
{
	//ASSERT(useCount > dependencies);
	int count = INTERLOCKED_DECREMENT(useCount);

	if (count == 0)
		delete this;

	return count;
}

int Transaction::createSavepoint()
{
	SavePoint *savePoint;
	
	ASSERT((savePoints || freeSavePoints) ? (savePoints != freeSavePoints) : true);
	
	if ( (savePoint = freeSavePoints) )
		freeSavePoints = savePoint->next;
	else
		savePoint = new SavePoint;
	
	savePoint->records = recordPtr;
	savePoint->id = ++curSavePointId;
	savePoint->next = savePoints;
	savePoints = savePoint;

	ASSERT(savePoint->next != savePoint);
 	
	return savePoint->id;
}

void Transaction::releaseSavepoint(int savePointId)
{
	for (SavePoint **ptr = &savePoints, *savePoint; (savePoint = *ptr); ptr = &savePoint->next)
		if (savePoint->id == savePointId)
			{
			int nextLowerSavePointId = (savePoint->next) ? savePoint->next->id : 0;
			*ptr = savePoint->next;
			savePoint->next = freeSavePoints;
			freeSavePoints = savePoint;
			ASSERT((savePoints || freeSavePoints) ? (savePoints != freeSavePoints) : true);

			// commit pending record versions to the next pending savepoint
			
			RecordVersion *record = *savePoint->records;
			
			while ( (record) && (record->savePointId == savePointId) )
				{
				record->savePointId = nextLowerSavePointId;
				record->scavenge(transactionId, nextLowerSavePointId);
				record = record->nextInTrans;
				}

			return;
			}

	//throw SQLError(RUNTIME_ERROR, "invalid savepoint");
}

void Transaction::rollbackSavepoint(int savePointId)
{
	SavePoint *savePoint = savePoints;

	// Be sure the target savepoint is valid before rollong them back.
	
	for (savePoint = savePoints; savePoint; savePoint = savePoint->next)
		if (savePoint->id <= savePointId)
			break;
			
	if ((savePoint) && (savePoint->id != savePointId))
		throw SQLError(RUNTIME_ERROR, "invalid savepoint");

	savePoint = savePoints;
	
	while (savePoint)
		{
		if (savePoint->id < savePointId)
			break;

		// Purge out records from this savepoint

		RecordVersion *record = *savePoint->records;
		if (record)
			lastRecord = record->prevInTrans;
		recordPtr = savePoint->records;
		*recordPtr = NULL;
		RecordVersion *stack = NULL;

		while (record)
			{
			if (chillPoint == &record->nextInTrans)
				chillPoint = savePoint->records;

			RecordVersion *rec = record;
			record = rec->nextInTrans;
			rec->prevInTrans = NULL;
			rec->nextInTrans = stack;
			stack = rec;
			}

		while (stack)
			{
			RecordVersion *rec = stack;
			stack = rec->nextInTrans;
			rec->nextInTrans = NULL;
			rec->rollback();
#ifdef CHECK_RECORD_ACTIVITY
			rec->active = false;
#endif
			rec->transaction = NULL;
			rec->release();
			}

		// Move skipped savepoints object to the free list
		// Leave the target savepoint empty, but connected to the transaction.
		
		if (savePoint->id > savePointId)
			{
			savePoints = savePoint->next;
			savePoint->next = freeSavePoints;
			freeSavePoints = savePoint;
			savePoint = savePoints;
			}
		else
			savePoint = savePoint->next;
		}
}

/***
void Transaction::scavengeRecords(int ageGroup)
{
	scavenged = true;
}
***/

void Transaction::add(DeferredIndex* deferredIndex)
{
	Sync sync(&syncIndexes, "Transaction::add");
	sync.lock(Exclusive);
	deferredIndex->nextInTransaction = deferredIndexes;
	deferredIndexes = deferredIndex;
	deferredIndexCount++;
}

bool Transaction::isXidEqual(int testLength, const UCHAR* test)
{
	if (testLength != xidLength)
		return false;
	
	return memcmp(xid, test, xidLength) == 0;
}

void Transaction::releaseRecordLocks(void)
{
	RecordVersion **ptr;
	RecordVersion *record;

	for (ptr = &firstRecord; (record = *ptr);)
		if (record->state == recLock)
			{
			record->format->table->unlockRecord(record, false);
			removeRecord(record);
			}
		else
			ptr = &record->nextInTrans;
}

void Transaction::print(void)
{
	Log::debug("  %p Id %d, state %d, updates %d, wrtPend %d, states %d, dependencies %d, records %d\n",
			this, transactionId, state, hasUpdates, writePending, 
			numberStates, dependencies, firstRecord != NULL);
}

void Transaction::printBlocking(int level)
{
	int locks = 0;
	int updates = 0;
	int inserts = 0;
	int deletes = 0;
	RecordVersion *record;

	for (record = firstRecord; record; record = record->nextInTrans)
		if (record->state == recLock)
			++locks;
		else if (!record->hasRecord())
			++deletes;
		else if (record->priorVersion)
			++updates;
		else
			++inserts;

	Log::debug ("%*s Trans %d, thread %d, locks %d, inserts %d, deleted %d, updates %d\n", 
				level * INDENT, "", transactionId,
				thread->threadId, locks, inserts, deletes, updates);

	++level;

	if (blockingRecord)
		{
		Table *table = blockingRecord->format->table;
		Log::debug("%*s Blocking on %s.%s record %d\n",
				   level * INDENT, "",
				   table->schemaName, table->name, 
				   blockingRecord->recordNumber);
		}

	for (record = firstRecord; record; record = record->nextInTrans)
		{
		const char *what;

		if (record->state == recLock)
			what = "locked";
		else if (!record->hasRecord())
			what = "deleted";
		else if (record->priorVersion)
			what = "updated";
		else
			what = "inserted";

		Table *table = record->format->table;
		
		Log::debug("%*s Record %s.%s number %d %s\n",
				   level * INDENT, "",
				   table->schemaName,
				   table->name, 
				   record->recordNumber,
				   what);
		}

	database->transactionManager->printBlocking(this, level);
}

void Transaction::getInfo(InfoTable* infoTable)
{
	if (!(state == Available && dependencies == 0))
		{
		const char * ptr;
		switch (state)
			{
			case Active:				ptr = "Active"; break;
			case Limbo:					ptr = "Limbo"; break;
			case Committed:				ptr = "Committed"; break;
			case RolledBack:			ptr = "RolledBack"; break;
			case Available:				ptr = "Available"; break;
			case Initializing:			ptr = "Initializing"; break;
			case CommittingReadOnly:	ptr = "CommittingReadOnly"; break;
			default:					ptr = "Unknown"; break;
			}

		int n = 0;
		infoTable->putString(n++, ptr);
		infoTable->putInt(n++, mySqlThreadId);
		infoTable->putInt(n++, transactionId);
		infoTable->putString(n++, stateNames[state]);
		infoTable->putInt(n++, hasUpdates);
		infoTable->putInt(n++, writePending);
		infoTable->putInt(n++, dependencies);
		infoTable->putInt(n++, oldestActive);
		infoTable->putInt(n++, firstRecord != NULL);
		infoTable->putInt(n++, (waitingFor) ? waitingFor->transactionId : 0);
		
		char buffer[512];
		
		if (connection)
			connection->getCurrentStatement(buffer, sizeof(buffer));
		else
			buffer[0] = 0;

		infoTable->putString(n++, buffer);
		infoTable->putRecord();
		}
}

void Transaction::releaseDependency(void)
{
	ASSERT(useCount >= 2);
	ASSERT(dependencies > 0);
	ASSERT(state != Available);
	INTERLOCKED_DECREMENT(dependencies);

	if ((dependencies == 0) && !writePending && firstRecord)
		commitRecords();
		
	releaseCommittedTransaction();
}

void Transaction::fullyCommitted(void)
{
	ASSERT(inList);

	if (useCount < 2)
		Log::debug("Transaction::fullyCommitted: funny use count\n");

	writeComplete();
	releaseCommittedTransaction();
}

void Transaction::releaseCommittedTransaction(void)
{
	release();

	if ((useCount == 1) && (state == Committed) && (dependencies == 0) && !writePending)
		if (COMPARE_EXCHANGE(&inList, (INTERLOCK_TYPE) true, (INTERLOCK_TYPE) false))
			database->transactionManager->removeCommittedTransaction(this);
}

void Transaction::validateDependencies(bool noDependencies)
{
	for (TransState *state = states, *end = states + numberStates; state < end; ++state)
		if (state->transaction)
			{
			ASSERT(!noDependencies);
			ASSERT(state->transaction->transactionId == state->transactionId);
			}
}

void Transaction::releaseSavePoints(void)
{
	SavePoint *savePoint;
	
	while ( (savePoint = savePoints) )
		{
		savePoints = savePoint->next;
		
		if (savePoint < localSavePoints || savePoint >= localSavePoints + LOCAL_SAVE_POINTS)
			delete savePoint;
		}

	while ( (savePoint = freeSavePoints) )
		{
		freeSavePoints = savePoint->next;
		
		if (savePoint < localSavePoints || savePoint >= localSavePoints + LOCAL_SAVE_POINTS)
			delete savePoint;
		}
}

void Transaction::printBlockage(void)
{
	TransactionManager *transactionManager = database->transactionManager;
	LogLock logLock;
	Sync sync (&transactionManager->activeTransactions.syncObject, "Transaction::printBlockage");
	sync.lock (Shared);
	printBlocking(0);
}

void Transaction::releaseDeferredIndexes(void)
{
	for (DeferredIndex *deferredIndex; (deferredIndex = deferredIndexes);)
		{
		ASSERT(deferredIndex->transaction == this);
		deferredIndexes = deferredIndex->nextInTransaction;
		deferredIndex->detachTransaction();
		deferredIndexCount--;
		}
}

void Transaction::releaseDeferredIndexes(Table* table)
{
	for (DeferredIndex **ptr = &deferredIndexes, *deferredIndex; (deferredIndex = *ptr);)
		{
		if (deferredIndex->index && (deferredIndex->index->table == table))
			{
			*ptr = deferredIndex->nextInTransaction;
			deferredIndex->detachTransaction();
			--deferredIndexCount;
			}
		else
			ptr = &deferredIndex->next;
		}
}
