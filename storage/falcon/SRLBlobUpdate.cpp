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

// SRLBlobUpdate.cpp: implementation of the SRLBlobUpdate class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLBlobUpdate.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Dbb.h"
#include "Section.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLBlobUpdate::SRLBlobUpdate()
{

}

SRLBlobUpdate::~SRLBlobUpdate()
{

}

void SRLBlobUpdate::append(Dbb *dbb, TransId transId, int32 locPage, int locLine, int32 blobPage, int blobLine)
{
	START_RECORD(srlLargBlob, "SRLBlobUpdate::append");
	getTransaction(transId);
	putInt(dbb->tableSpaceId);
	putInt(transId);
	putInt(locPage);
	putInt(locLine);
	putInt(blobPage);
	putInt(blobLine);
}

void SRLBlobUpdate::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	transactionId = getInt();
	locatorPage = getInt();
	locatorLine = getInt();
	dataPage = getInt();
	dataLine = getInt();

	if (log->tracePage == locatorPage || (dataPage && log->tracePage == dataPage))
		print();
}

void SRLBlobUpdate::pass1(void)
{
	log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
	log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);
}

void SRLBlobUpdate::pass2(void)
{
	if (transactionId == 11847)
		print();

	/***
	if (log->tracePage == locatorPage || (dataPage && log->tracePage == dataPage))
		print();
	***/
	
	bool ret1 = log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
	bool ret2 = log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);
	
	if (ret1)
		{
		SerialLogTransaction *transaction = log->findTransaction(transactionId);
		Dbb *dbb = log->getDbb(tableSpaceId);
		
		if (control->isPostFlush())
			{
			if (transaction && ret2 && transaction->state == sltCommitted)
				Section::redoBlobUpdate(dbb, locatorPage, locatorLine, dataPage, dataLine);
			else
				Section::redoBlobUpdate(dbb, locatorPage, locatorLine, 0, 0);
			}
		}
}

void SRLBlobUpdate::redo(void)
{
	log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
	log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);
}

void SRLBlobUpdate::print()
{
	logPrint("Blob tid %d, locator page %d/%d, line %d, blob page %d/%d\n",
			transactionId, locatorPage, tableSpaceId, locatorLine, dataPage, dataLine);
}
