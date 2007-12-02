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


// SRLRecordLocator.cpp: implementation of the SRLRecordLocator class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLRecordLocator.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Section.h"
#include "Dbb.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLRecordLocator::SRLRecordLocator()
{
}

SRLRecordLocator::~SRLRecordLocator()
{
}

void SRLRecordLocator::append(Dbb *dbb, TransId transId, int id, int seq, int32 page)
{
	START_RECORD(srlRecordLocator, "SRLRecordLocator::append");

	if (transId)
		{
		SerialLogTransaction *trans = log->getTransaction(transId);
		
		if (trans)
			trans->setPhysicalBlock();
		}

	putInt(dbb->tableSpaceId);
	putInt(id);
	putInt(seq);
	putInt(page);
}

void SRLRecordLocator::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;
	
	sectionId = getInt();
	sequence = getInt();
	pageNumber = getInt();
}

void SRLRecordLocator::pass1()
{
	if (log->tracePage == pageNumber)
		print();
	
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLRecordLocator::pass2()
{
	if (log->tracePage == pageNumber)
		print();
	
	if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
		log->getDbb(tableSpaceId)->redoRecordLocatorPage(sectionId, sequence, pageNumber, control->isPostFlush());
}

void SRLRecordLocator::redo()
{
	if (!log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
		return;
}


void SRLRecordLocator::print()
{
	logPrint("RecordLocator sectionId %d/%d, sequence %d, page %d\n",
			sectionId, tableSpaceId, sequence, pageNumber);
}
