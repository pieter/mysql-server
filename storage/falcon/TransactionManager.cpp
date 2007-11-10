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


#include <memory.h>
#include "Engine.h"
#include "TransactionManager.h"
#include "Transaction.h"
#include "Sync.h"
#include "Interlock.h"
#include "SQLError.h"
#include "Database.h"
#include "Connection.h"
#include "InfoTable.h"
#include "Log.h"
#include "LogLock.h"
#include "Synchronize.h"

static const int EXTRA_TRANSACTIONS = 10;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


TransactionManager::TransactionManager(Database *db)
{
	database = db;
	transactionSequence = 1;
	committed = 0;
	rolledBack = 0;
	priorCommitted = 0;
	priorRolledBack = 0;
	rolledBackTransaction = new Transaction(database->systemConnection, 0);
	rolledBackTransaction->state = RolledBack;
	rolledBackTransaction->inList = false;
	syncObject.setName("TransactionManager::syncObject");
	syncInitialize.setName("TransactionManager::syncInitialize");
}

TransactionManager::~TransactionManager(void)
{
	rolledBackTransaction->release();
	
	for (Transaction *transaction; (transaction = activeTransactions.first);)
		{
		transaction->inList = false;
		transaction->state = Committed;
		activeTransactions.first = transaction->next;
		transaction->release();
		}
}

TransId TransactionManager::findOldestActive()
{
	Sync sync (&activeTransactions.syncObject, "TransactionManager::findOldestActive");
	sync.lock (Shared);
	Transaction *oldest = findOldest();

	if (oldest)
		{
		//oldest->scavenged = false;
	
		return oldest->oldestActive;
		}
		
	return transactionSequence;
}

Transaction* TransactionManager::findOldest(void)
{
	Transaction *oldest = NULL;
	
	for (Transaction *transaction = activeTransactions.first; transaction; transaction = transaction->next)
		if (transaction->isActive() && (!oldest || transaction->transactionId < oldest->transactionId))
			oldest = transaction;
	
	return oldest;
}

Transaction* TransactionManager::startTransaction(Connection* connection)
{
	Sync sync (&activeTransactions.syncObject, "Database::startTransaction");
	sync.lock (Shared);
	Sync syncInit(&syncInitialize, "TransactionManager::startTransaction");
	Transaction *transaction;
	
	for (transaction = activeTransactions.first; transaction; transaction = transaction->next)
		if (transaction->state == Available && transaction->dependencies == 0)
			if (COMPARE_EXCHANGE(&transaction->state, Available, Initializing))
				{
				syncInit.lock(Exclusive);
				transaction->initialize(connection, ++transactionSequence);
				
				return transaction;
				}
				
	sync.unlock();
	sync.lock(Exclusive);
	syncInit.lock(Exclusive);
	transaction = new Transaction (connection, ++transactionSequence);
	activeTransactions.append(transaction);
	syncInit.unlock();

	// And, just for yucks, add another 10 Available transactions
	
	for (int n = 0; n < EXTRA_TRANSACTIONS; ++n)
		{
		Transaction *trans = new Transaction(connection, 0);
		activeTransactions.append(trans);
		}
	
	return transaction;	
}

void TransactionManager::dropTable(Table* table, Transaction* transaction)
{
	Sync committedTrans (&committedTransactions.syncObject, "TransactionManager::dropTable");
	committedTrans.lock (Shared);
	
	for (Transaction *trans = committedTransactions.first; trans; trans = trans->next)
		trans->dropTable(table);
	
	committedTrans.unlock();
}

bool TransactionManager::hasUncommittedRecords(Table* table, Transaction* transaction)
{
	Sync syncTrans (&activeTransactions.syncObject, "TransactionManager::hasUncommittedRecords");
	syncTrans.lock (Shared);
	
	for (Transaction *trans = activeTransactions.first; trans; trans = trans->next)
		if (trans != transaction && trans->isActive() && trans->hasUncommittedRecords(table))
			return true;

	return false;
}

/***
void TransactionManager::scavengeRecords(int threshold)
{
	Sync sync (&activeTransactions.syncObject, "TransactionManager::scavengeRecord");
	sync.lock (Shared);
	
	for (bool again = true; again;)
		{
		again = false;
		
		for (Transaction *transaction = activeTransactions.first; transaction; transaction = transaction->next)
			if (transaction != database->systemConnection->transaction && transaction->isActive() && !transaction->scavenged)
				{
				transaction->addRef();
				sync.unlock();
				transaction->scavengeRecords(threshold);
				transaction->release();
				sync.lock(Shared);
				again = true;
				break;
				}
		}
}
***/

void TransactionManager::commitByXid(int xidLength, const UCHAR* xid)
{
	Sync sync (&activeTransactions.syncObject, "TransactionManager::commitByXid");
	sync.lock (Shared);
	
	for (bool again = true; again;)
		{
		again = false;
		
		for (Transaction *transaction = activeTransactions.first; transaction; transaction = transaction->next)
			if (transaction->state == Limbo && transaction->isXidEqual(xidLength, xid))
				{
				sync.unlock();
				transaction->commit();
				sync.lock(Shared);
				again = true;
				break;
				}
		}
}

void TransactionManager::rollbackByXid(int xidLength, const UCHAR* xid)
{
	Sync sync (&activeTransactions.syncObject, "TransactionManager::rollbackByXid");
	sync.lock (Shared);
	
	for (bool again = true; again;)
		{
		again = false;
		
		for (Transaction *transaction = activeTransactions.first; transaction; transaction = transaction->next)
			if (transaction->state == Limbo && transaction->isXidEqual(xidLength, xid))
				{
				sync.unlock();
				transaction->rollback();
				sync.lock(Shared);
				again = true;
				break;
				}
		}
}

void TransactionManager::print(void)
{
	Sync sync (&activeTransactions.syncObject, "TransactionManager::print");
	sync.lock (Exclusive);
	Sync committedTrans (&committedTransactions.syncObject, "TransactionManager::print");
	committedTrans.lock (Exclusive);
	Transaction *transaction;
	Log::debug("Active Transaction:\n");
	
	for (transaction = activeTransactions.first; transaction; transaction = transaction->next)
		transaction->print();
		
	Log::debug("Committed Transaction:\n");
	
	for (transaction = committedTransactions.first; transaction; transaction = transaction->next)
		transaction->print();
		
}

void TransactionManager::getTransactionInfo(InfoTable* infoTable)
{
	Sync sync (&activeTransactions.syncObject, "TransactionManager::getTransactionInfo");
	sync.lock (Exclusive);
	Sync committedTrans (&committedTransactions.syncObject, "TransactionManager::getTransactionInfo");
	committedTrans.lock (Exclusive);
	Transaction *transaction;
	
	for (transaction = activeTransactions.first; transaction; transaction = transaction->next)
		transaction->getInfo(infoTable);
	
	for (transaction = committedTransactions.first; transaction; transaction = transaction->next)
		transaction->getInfo(infoTable);
}

void TransactionManager::purgeTransactions()
{
	Sync syncCommitted(&committedTransactions.syncObject, "Transaction::commit");
	syncCommitted.lock(Exclusive);
	
	// And, while we're at it, check for any fully mature transactions to ditch
	
	for (Transaction *transaction, *next = committedTransactions.first; (transaction = next);)
		{
		next = transaction->next;

		if ((transaction->state == Committed) && 
			(transaction->dependencies == 0) && 
			!transaction->writePending)
			{
			transaction->commitRecords();

			if (COMPARE_EXCHANGE(&transaction->inList, (INTERLOCK_TYPE) true, (INTERLOCK_TYPE) false))
				{
				committedTransactions.remove(transaction);
				transaction->release();
				}
			}
		}
}

void TransactionManager::getSummaryInfo(InfoTable* infoTable)
{
	Sync sync (&activeTransactions.syncObject, "TransactionManager::getSummaryInfo");
	sync.lock (Exclusive);
	Sync committedTrans (&committedTransactions.syncObject, "TransactionManager::getSummaryInfo");
	committedTrans.lock (Exclusive);
	int numberCommitted = committed;
	int numberRolledBack = rolledBack;
	int numberActive = 0;
	int numberPendingCommit = 0;
	int numberPendingCompletion = 0;

	Transaction *transaction;
	
	for (transaction = activeTransactions.first; transaction; transaction = transaction->next)
		{
		if (transaction->state == Active)
			++numberActive;
			
		if (transaction->state == Committed)
			++numberPendingCommit;
		}

	for (transaction = committedTransactions.first; transaction; transaction = transaction->next)
		if (transaction->writePending)
			++numberPendingCompletion;
	
	committedTrans.unlock();
	sync.unlock();
	
	int n = 0;
	infoTable->putString(n++, database->name);
	infoTable->putInt(n++, numberCommitted);
	infoTable->putInt(n++, numberRolledBack);
	infoTable->putInt(n++, numberActive);
	infoTable->putInt(n++, numberPendingCommit);
	infoTable->putInt(n++, numberPendingCompletion);
	infoTable->putRecord();
}

void TransactionManager::reportStatistics(void)
{
	Sync sync (&activeTransactions.syncObject, "Database::reportStatistics");
	sync.lock (Shared);
	Transaction *transaction;
	int active = 0;
	time_t maxTime = 0;
	
	for (transaction = activeTransactions.first; transaction; transaction = transaction->next)
		if (transaction->state == Active)
			{
			++active;
			time_t age = database->deltaTime - transaction->startTime;
			maxTime = MAX(age, maxTime);
			}
			
	int pendingCleanup = committedTransactions.count;
	int numberCommitted = committed - priorCommitted;
	int numberRolledBack = rolledBack - priorRolledBack;
	priorCommitted = committed;
	priorRolledBack = rolledBack;
	
	if ((active || numberCommitted || numberRolledBack) && Log::isActive(LogInfo))
		Log::log (LogInfo, "%d: Transactions: %d committed, %d rolled back, %d active, %d post-commit, oldest %d seconds\n",
				  database->deltaTime, numberCommitted, numberRolledBack, active, pendingCleanup, maxTime);
}

void TransactionManager::removeCommittedTransaction(Transaction* transaction)
{
	Sync syncCommitted(&committedTransactions.syncObject, "TransactionManager::removeCommittedTransaction");
	syncCommitted.lock(Exclusive);
	committedTransactions.remove(transaction);
	syncCommitted.unlock();
	transaction->release();
}

void TransactionManager::expungeTransaction(Transaction *transaction)
{
	Sync sync(&syncInitialize, "TransactionManager::expungeTransaction");
	sync.lock(Shared);

	for (Transaction *trans = activeTransactions.first; trans; trans = trans->next)
		if (trans->transactionId > transaction->transactionId)
			trans->expungeTransaction(transaction);
}

Transaction* TransactionManager::findTransaction(TransId transactionId)
{
	Sync sync(&committedTransactions.syncObject, "TransactionManager::findTransaction");
	sync.lock(Shared);
	Transaction *transaction;

	for (transaction = activeTransactions.first; transaction; transaction = transaction->next)
		if (transaction->transactionId == transactionId)
			return transaction;
	
	sync.unlock();
	Sync syncCommitted(&committedTransactions.syncObject, "TransactionManager::findTransaction");
	syncCommitted.lock(Shared);

	for (transaction = committedTransactions.first; transaction; transaction = transaction->next)
		if (transaction->transactionId == transactionId)
			return transaction;
	
	return NULL;	
}

void TransactionManager::validateDependencies(void)
{
	Sync sync(&committedTransactions.syncObject, "TransactionManager::validateDepedendencies");
	sync.lock(Shared);
	Transaction *transaction;

	for (transaction = activeTransactions.first; transaction; transaction = transaction->next)
		if (transaction->isActive())
			transaction->validateDependencies(false);
			
	sync.unlock();
	Sync syncCommitted(&committedTransactions.syncObject, "TransactionManager::validateDepedendencies");
	syncCommitted.lock(Shared);

	for (transaction = committedTransactions.first; transaction; transaction = transaction->next)
		transaction->validateDependencies(true);
}

void TransactionManager::removeTransaction(Transaction* transaction)
{
	if (transaction->state == Committed)
		{
		Sync sync(&committedTransactions.syncObject, "TransactionManager::removeTransaction");
		sync.lock(Exclusive);
		
		for (Transaction *trans = committedTransactions.first; trans; trans = trans->next)
			if (trans == transaction)
				{
				committedTransactions.remove(transaction);
				break;
				}
		}
	else
		{
		Sync sync(&activeTransactions.syncObject, "TransactionManager::removeTransaction");
		sync.lock(Exclusive);
		
		for (Transaction *trans = activeTransactions.first; trans; trans = trans->next)
			if (trans == transaction)
				{
				activeTransactions.remove(transaction);
				break;
				}
		}
}

void TransactionManager::printBlockage(void)
{
	LogLock logLock;
	Sync sync (&activeTransactions.syncObject, "TransactionManager::printBlockage");
	sync.lock (Shared);

	for (Transaction *trans = activeTransactions.first; trans; trans = trans->next)
		if (trans->state == Active && !trans->waitingFor)
			trans->printBlocking(0);

	Synchronize::freezeSystem();
}

void TransactionManager::printBlocking(Transaction* transaction, int level)
{
	for (Transaction *trans = activeTransactions.first; trans; trans = trans->next)
		if (trans->state == Active && trans->waitingFor == transaction)
			trans->printBlocking(level);
}
