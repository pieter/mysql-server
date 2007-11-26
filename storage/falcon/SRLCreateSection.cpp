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

// SRLCreateSection.cpp: implementation of the SRLCreateSection class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLCreateSection.h"
#include "SerialLog.h"
#include "SerialLogTransaction.h"
#include "Database.h"
#include "Dbb.h"
#include "Log.h"
#include "SerialLogControl.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLCreateSection::SRLCreateSection()
{

}

SRLCreateSection::~SRLCreateSection()
{

}

void SRLCreateSection::append(Dbb *dbb, TransId transId, int32 sectionId)
{
	START_RECORD(srlCreateSection, "SRLCreateSection::append");
	log->setSectionActive(sectionId, dbb->tableSpaceId);
	//SerialLogTransaction *trans = 
	log->getTransaction(transId);
	putInt(dbb->tableSpaceId);
	putInt(transId);
	putInt(sectionId);
	sync.unlock();
}

void SRLCreateSection::read()
{
	if (control->version >= srlVersion7)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	transactionId = getInt();
	sectionId = getInt();
}

void SRLCreateSection::pass1()
{
	log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
}

void SRLCreateSection::pass2(void)
{
	log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
}

void SRLCreateSection::commit()
{
	//Log::debug("SRLCreateSection::commit: marking active section %d, tid %d\n", sectionId, transactionId);
}

void SRLCreateSection::redo()
{
	if (!log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse))
		return;

	log->getDbb(tableSpaceId)->createSection(sectionId, transactionId);
}

void SRLCreateSection::print(void)
{
	logPrint("Create Section %d/%d\n", sectionId, tableSpaceId);
}
