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

// SRLRecordStub.cpp: implementation of the SRLRecordStub class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLRecordStub.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Dbb.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLRecordStub::SRLRecordStub()
{

}

SRLRecordStub::~SRLRecordStub()
{

}

void SRLRecordStub::append(Dbb *dbb, TransId transId, int32 section, int32 record)
{
	START_RECORD(srlRecordStub, "SRLRecordStub::append");
	putInt(dbb->tableSpaceId);
	putInt(transId);
	putInt(section);
	putInt(record);
	getTransaction(transId);
	sync.unlock();
}

void SRLRecordStub::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	transactionId = getInt();
	sectionId = getInt();
	recordId = getInt();
}

void SRLRecordStub::print()
{
	logPrint("Record stub: transaction %d, section %d/%d, record %d\n",
			transactionId, sectionId, tableSpaceId, recordId);
}

void SRLRecordStub::pass1()
{
	control->getTransaction(transactionId);
}

void SRLRecordStub::redo()
{
	if (!log->isSectionActive(sectionId, tableSpaceId))
		return;

	SerialLogTransaction *transaction = control->getTransaction(transactionId);

	if ((transaction->state == sltCommitted) || (transaction->state == sltPrepared))
		log->getDbb(tableSpaceId)->reInsertStub(sectionId, recordId, transactionId);
}
