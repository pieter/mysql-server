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

// SRLDropTable.cpp: implementation of the SRLDropTable class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLDropTable.h"
#include "SerialLog.h"
#include "SerialLogTransaction.h"
#include "SerialLogControl.h"
#include "Database.h"
#include "Section.h"
#include "Transaction.h"
#include "Log.h"
#include "Sync.h"
#include "Dbb.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLDropTable::SRLDropTable()
{

}

SRLDropTable::~SRLDropTable()
{

}

void SRLDropTable::append(Dbb *dbb, TransId transId, int section)
{
	Sync syncSections(&log->syncSections, "SRLDropTable::append");
	syncSections.lock(Exclusive);

	START_RECORD(srlDropTable, "SRLDropTable::append");
	putInt(dbb->tableSpaceId);
	log->getTransaction(transId);
	log->setSectionInactive(section, dbb->tableSpaceId);
	putInt(transId);
	putInt(section);
}

void SRLDropTable::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	transactionId = getInt();
	sectionId = getInt();
}

void SRLDropTable::pass1()
{
	log->bumpSectionIncarnation(sectionId, tableSpaceId, objDeleted);
}

void SRLDropTable::pass2(void)
{
	log->bumpSectionIncarnation(sectionId, tableSpaceId, objDeleted);
}

void SRLDropTable::redo()
{
	if (!log->bumpSectionIncarnation(sectionId, tableSpaceId, objDeleted))
		return;

	Dbb *dbb = log->findDbb(tableSpaceId);
	
	if (dbb)
		dbb->deleteSection(sectionId, NO_TRANSACTION);
}

void SRLDropTable::print()
{
	logPrint("Drop Section %d/%d\n", sectionId, tableSpaceId);
}

void SRLDropTable::commit(void)
{
	Dbb *dbb = log->findDbb(tableSpaceId);
	
	if (dbb)
		Section::deleteSection(dbb, sectionId, NO_TRANSACTION);
}

void SRLDropTable::rollback(void)
{
}
