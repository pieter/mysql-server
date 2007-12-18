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

// SRLCommit.cpp: implementation of the SRLCommit class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLCommit.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "SerialLogWindow.h"
#include "Sync.h"
#include "Thread.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLCommit::SRLCommit()
{

}

SRLCommit::~SRLCommit()
{

}

void SRLCommit::append(Transaction *transaction)
{
	transaction->addRef();
	START_RECORD(srlCommit, "SRLCommit::append");
	putInt(transaction->transactionId);
	//uint64 commitBlockNumber = log->nextBlockNumber;
	uint64 commitBlockNumber = log->getWriteBlockNumber();
	SerialLogTransaction *srlTransaction = log->getTransaction(transaction->transactionId);
	
	if (transaction->hasUpdates)
		log->flush(false, commitBlockNumber, &sync);
	else
		sync.unlock();

	srlTransaction->setTransaction(transaction);
	srlTransaction->setState(sltCommitted);
	wakeup();
}

void SRLCommit::read()
{
	transactionId = getInt();
}

void SRLCommit::print()
{
	logPrint("Commit SerialLogTransaction "TXIDFORMAT"\n", transactionId);
}

void SRLCommit::pass1()
{
	SerialLogTransaction *srlTransaction = control->getTransaction(transactionId);
	srlTransaction->setState(sltCommitted);
}

void SRLCommit::commit()
{
	SerialLogTransaction *srlTransaction = log->findTransaction(transactionId);
	srlTransaction->setFinished();
}
