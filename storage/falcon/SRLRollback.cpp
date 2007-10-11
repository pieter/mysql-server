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

// SRLRollback.cpp: implementation of the SRLRollback class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLRollback.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Sync.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLRollback::SRLRollback()
{

}

SRLRollback::~SRLRollback()
{

}

void SRLRollback::append(TransId transId, bool updateTransaction)
{
	START_RECORD(srlRollback, "");
	putInt(transId);
	uint64 commitBlockNumber = log->nextBlockNumber;
	SerialLogTransaction *transaction = log->findTransaction(transId);

	if (updateTransaction)
		log->flush(false, commitBlockNumber, &sync);
	else
		sync.unlock();
	
	if (transaction)
		{
		transaction->setState(sltRolledBack);
		wakeup();
		}
}

void SRLRollback::read()
{
	transactionId = getInt();
}

void SRLRollback::pass1()
{
	SerialLogTransaction *transaction = control->getTransaction(transactionId);
	transaction->setState(sltRolledBack);
}

void SRLRollback::print()
{
	logPrint("Rollback transaction "TXIDFORMAT"\n", transactionId);
}

void SRLRollback::rollback()
{
	SerialLogTransaction *transaction = log->findTransaction(transactionId);
	transaction->setFinished();		
}
