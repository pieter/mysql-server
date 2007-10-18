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

// SRLDelete.cpp: implementation of the SRLDelete class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLDelete.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Dbb.h"
#include "Transaction.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLDelete::SRLDelete()
{

}

SRLDelete::~SRLDelete()
{

}

void SRLDelete::append(Dbb *dbb, Transaction *transaction, int32 sectionId, int32 recordId)
{
	START_RECORD(srlDelete, "SRLDelete::append");
	SerialLogTransaction *trans = log->getTransaction(transaction->transactionId);
	trans->setTransaction(transaction);
	ASSERT(transaction->writePending);
	putInt(dbb->tableSpaceId);
	putInt(transaction->transactionId);
	putInt(sectionId);
	putInt(recordId);
	sync.unlock();
}

void SRLDelete::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	transactionId = getInt();
	sectionId = getInt();
	recordId = getInt();
}

void SRLDelete::print()
{
	logPrint("Delete: transaction %d, section %d/%d, record %d\n",
			transactionId, sectionId, tableSpaceId, recordId);
}

void SRLDelete::pass1()
{

}

void SRLDelete::redo()
{
	SerialLogTransaction *transaction = control->getTransaction(transactionId);

	if (transaction->state == sltCommitted && log->isSectionActive(sectionId, tableSpaceId))
		log->getDbb(tableSpaceId)->updateRecord(sectionId, recordId, NULL, transactionId, false);
}

void SRLDelete::commit()
{
	Sync sync(&log->syncSections, "SRLDelete::commit");
	sync.lock(Shared);
	redo();
}
