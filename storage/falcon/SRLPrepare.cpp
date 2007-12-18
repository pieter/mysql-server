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

// SRLPrepare.cpp: implementation of the SRLPrepare class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLPrepare.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "SRLVersion.h"
#include "Sync.h"
#include "Thread.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLPrepare::SRLPrepare()
{

}

SRLPrepare::~SRLPrepare()
{

}

void SRLPrepare::append(TransId transId, int xidLength, const UCHAR *xid)
{
	START_RECORD(srlPrepare, "");
	putInt(transId);
	putInt(xidLength);
	putData(xidLength, xid);
	SerialLogTransaction *transaction = log->getTransaction(transId);

	log->flush(false, log->nextBlockNumber, &sync);

	if (transaction)
		transaction->setState(sltPrepared);

	if (transaction)
		wakeup();
}

void SRLPrepare::read()
{
	transactionId = getInt();

	if (control->version >= srlVersion7)
		{
		xidLength = getInt();
		xid = getData(xidLength);
		}
	else
		xidLength = 0;
}

void SRLPrepare::pass1()
{
	SerialLogTransaction *transaction = control->getTransaction(transactionId);
	transaction->setState(sltPrepared);
	
	if (xidLength)
		transaction->setXID(xidLength, xid);
}

void SRLPrepare::print()
{
	logPrint("Prepare Transaction "TXIDFORMAT"\n", transactionId);
}


void SRLPrepare::commit()
{
	SerialLogTransaction *srlTransaction = log->findTransaction(transactionId);
	srlTransaction->setFinished();
}

void SRLPrepare::rollback()
{
	SerialLogTransaction *srlTransaction = log->findTransaction(transactionId);
	srlTransaction->setFinished();
}
