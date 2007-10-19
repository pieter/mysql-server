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

// SRLSectionPage.cpp: implementation of the SRLSectionPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLSectionPage.h"
#include "SerialLog.h"
#include "Section.h"
#include "Dbb.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLSectionPage::SRLSectionPage()
{

}

SRLSectionPage::~SRLSectionPage()
{

}

void SRLSectionPage::append(Dbb *dbb, TransId transId, int32 parent, int32 page, int slot, int id, int seq, int lvl)
{
	START_RECORD(srlSectionPage, "SRLSectionPage::append");
	
	if (transId)
		{
		SerialLogTransaction *trans = log->getTransaction(transId);
		
		if (trans)
			trans->setPhysicalBlock();
		}

	putInt(dbb->tableSpaceId);
	putInt(parent);
	putInt(page);
	putInt(slot);
	putInt(id);
	putInt(seq);
	putInt(lvl);
}

void SRLSectionPage::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;
		
	parentPage = getInt();
	pageNumber = getInt();
	sectionSlot = getInt();
	sectionId = getInt();
	sequence = getInt();
	level = getInt();
	
	if ((log->tracePage && log->tracePage == pageNumber) ||
		(log->tracePage && log->tracePage && log->tracePage == parentPage))
		print();
}

void SRLSectionPage::pass1()
{
	log->bumpPageIncarnation(parentPage, tableSpaceId, objInUse);
	
	if (pageNumber)
		log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLSectionPage::pass2()
{
	if (pageNumber && log->tracePage == pageNumber)
		print();
	
	if (log->bumpPageIncarnation(parentPage, tableSpaceId, objInUse))
		{
		if (control->isPostFlush())
			Section::redoSectionPage(log->getDbb(tableSpaceId), parentPage, pageNumber, sectionSlot, sectionId, sequence, level);
		}

	if (pageNumber)
		log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLSectionPage::redo()
{
	log->bumpPageIncarnation(parentPage, tableSpaceId, objInUse);
	
	if (pageNumber)
		log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLSectionPage::print()
{
	logPrint("Section Page parent %d/%d, page %d, slot %d, sectionId %d, sequence %d, level %d\n",
			parentPage, tableSpaceId, pageNumber, sectionSlot, sectionId, sequence, level);
}
