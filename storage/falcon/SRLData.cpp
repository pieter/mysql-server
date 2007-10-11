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

// SRLData.cpp: implementation of the SRLData class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLData.h"
#include "Stream.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Dbb.h"
#include "Transaction.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLData::SRLData()
{

}

SRLData::~SRLData()
{

}

void SRLData::append(Dbb *dbb, Transaction *transaction, int32 sectionId, int32 recordId, Stream *stream)
{
	START_RECORD(srlDataUpdate, "SRLData::append");
	SerialLogTransaction *trans = log->getTransaction(transaction->transactionId);
	trans->setTransaction(transaction);
	ASSERT(transaction->writePending);
	putInt(dbb->tableSpaceId);
	putInt(transaction->transactionId);
	putInt(sectionId);
	putInt(recordId);
	putStream(stream);
}

void SRLData::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	transactionId = getInt();
	sectionId = getInt();
	recordId = getInt();
	length = getInt();
	ASSERT(length >= 0);
	data = getData(length);
}

void SRLData::print()
{
	char temp[40];
	logPrint("Data trans %d, section %d, record %d, length %d %s\n",
			transactionId, sectionId, recordId, length, format(length, data, sizeof(temp), temp));
}

void SRLData::pass1()
{
	control->getTransaction(transactionId);
}

void SRLData::redo()
{
	SerialLogTransaction *transaction = control->getTransaction(transactionId);

	if (transaction->state == sltCommitted && log->isSectionActive(sectionId, tableSpaceId))
		{
		Stream stream;
		stream.putSegment(length, (const char*) data, false);
		log->getDbb(tableSpaceId)->updateRecord(sectionId, recordId, &stream, transactionId, false);
		}
		
}

void SRLData::commit()
{
	redo();
}

void SRLData::recoverLimbo(void)
{
}
