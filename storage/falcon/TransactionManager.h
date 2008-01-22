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

#ifndef _TRANSACTION_MANAGER_H_
#define _TRANSACTION_MANAGER_H_

#include "SyncObject.h"
#include "Queue.h"

class Transaction;
class Database;
class Connection;
class Table;

class TransactionManager
{
public:
	TransactionManager(Database *database);
	~TransactionManager(void);
	
	TransId			findOldestActive();
	Transaction*	startTransaction(Connection* connection);
	void			dropTable(Table* table, Transaction* transaction);
	void			truncateTable(Table* table, Transaction* transaction);
	bool			hasUncommittedRecords(Table* table, Transaction* transaction);
	void			commitByXid(int xidLength, const UCHAR* xid);
	void			rollbackByXid(int xidLength, const UCHAR* xid);
	void			print(void);
	Transaction*	findOldest(void);
	void			getTransactionInfo(InfoTable* infoTable);
	void			purgeTransactions();
	void			getSummaryInfo(InfoTable* infoTable);
	void			reportStatistics(void);
	void			expungeTransaction(Transaction *transaction);
	Transaction* 	findTransaction(TransId transactionId);
	void 			validateDependencies(void);
	void			removeCommittedTransaction(Transaction* transaction);
	void			removeTransaction(Transaction* transaction);
	void			printBlockage(void);
	void			printBlocking(Transaction* transaction, int level);
	
	TransId			transactionSequence;
	Database		*database;
	SyncObject		syncObject;
	//SyncObject	syncInitialize;
	Transaction		*rolledBackTransaction;
	int				committed;
	int				rolledBack;
	int				priorCommitted;
	int				priorRolledBack;
	Queue<Transaction>	activeTransactions;
	Queue<Transaction>	committedTransactions;
};

#endif
