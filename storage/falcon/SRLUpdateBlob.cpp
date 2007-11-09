/* Copyright (C) 2007 MySQL AB

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

#include "Engine.h"
#include "SRLUpdateBlob.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Dbb.h"
#include "Stream.h"

SRLUpdateBlob::SRLUpdateBlob(void)
{
}

SRLUpdateBlob::~SRLUpdateBlob(void)
{
}

void SRLUpdateBlob::append(Dbb *dbb, int32 sectionId, TransId transId, int recordNumber, Stream* stream)
{
	START_RECORD(srlSmallBlob, "SRLUpdateBlob::append");
	putInt(dbb->tableSpaceId);
	putInt(sectionId);
	putInt(transId);
	putInt(recordNumber);
	putStream(stream);
}

void SRLUpdateBlob::read(void)
{
	tableSpaceId = getInt();
	sectionId = getInt();
	transactionId = getInt();
	recordNumber = getInt();
	length = getInt();
	data = getData(length);
}

void SRLUpdateBlob::pass1(void)
{
	control->getTransaction(transactionId);
}

void SRLUpdateBlob::pass2(void)
{
}

void SRLUpdateBlob::commit(void)
{
}

void SRLUpdateBlob::redo(void)
{
	SerialLogTransaction *transaction = control->getTransaction(transactionId);

	if (transaction->state == sltCommitted && log->isSectionActive(sectionId, tableSpaceId))
		{
		Stream stream;
		stream.putSegment(length, (const char*) data, false);
		log->getDbb(tableSpaceId)->updateRecord(sectionId, recordNumber, &stream, transactionId, false);
		}
}

void SRLUpdateBlob::print(void)
{
	logPrint("UpdateBlob trans %d, section %d, record %d, length %d \n",
			transactionId, sectionId, recordNumber, length);
}
