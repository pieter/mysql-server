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

// SRLDataPage.cpp: implementation of the SRLDataPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLDataPage.h"
#include "SerialLog.h"
#include "SerialLogTransaction.h"
#include "SerialLogControl.h"
#include "Section.h"
#include "Dbb.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLDataPage::SRLDataPage()
{

}

SRLDataPage::~SRLDataPage()
{

}

void SRLDataPage::append(Dbb *dbb, TransId transId, int id, int32 locatorPage, int32 page)
{
	START_RECORD(srlDataPage, "SRLDataPage::append");

	if (transId)
		{
		SerialLogTransaction *trans = log->getTransaction(transId);
		
		if (trans)
			trans->setPhysicalBlock();
		}

	putInt(dbb->tableSpaceId);
	putInt(id);
	putInt(locatorPage);
	putInt(page);
	
	if (page == log->tracePage)
		{
		sectionId = id;
		pageNumber = page;
		locatorPageNumber = locatorPage;
		tableSpaceId = dbb->tableSpaceId;
		print();
		}
}

void SRLDataPage::read()
{
	if (control->version >= srlVersion7)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	sectionId = getInt();
	locatorPageNumber = getInt();
	pageNumber = getInt();
}

void SRLDataPage::pass1()
{
	if (log->tracePage == pageNumber || log->tracePage == locatorPageNumber)
		print();

	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
	log->bumpPageIncarnation(locatorPageNumber, tableSpaceId, objInUse);
	log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
}

void SRLDataPage::pass2()
{
	if (log->tracePage == pageNumber || log->tracePage == locatorPageNumber)
		print();
	
	bool sectionActive = log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
	bool locatorPageActive = log->bumpPageIncarnation(locatorPageNumber, tableSpaceId, objInUse);

	if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
		{
		if (sectionActive)
			{
			if (control->isPostFlush())
				log->getDbb(tableSpaceId)->redoDataPage(sectionId, pageNumber, locatorPageNumber);
			}
		else
			log->redoFreePage(pageNumber, tableSpaceId);
		}
	else if (sectionActive && locatorPageActive)
		Section::redoSectionLine(log->getDbb(tableSpaceId), locatorPageNumber, pageNumber);
}

void SRLDataPage::redo()
{
	log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
	log->bumpPageIncarnation(locatorPageNumber, tableSpaceId, objInUse);
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLDataPage::print()
{
	logPrint("DataPage section %d/%d, locator page %d, page %d, \n",
			 sectionId, tableSpaceId, locatorPageNumber, pageNumber);
}
