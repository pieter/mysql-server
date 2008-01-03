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

// SRLFreePage.cpp: implementation of the SRLFreePage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLFreePage.h"
#include "SerialLog.h"
#include "Dbb.h"
#include "SerialLogControl.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLFreePage::SRLFreePage()
{

}

SRLFreePage::~SRLFreePage()
{

}

void SRLFreePage::append(Dbb *dbb, int32 page)
{
	START_RECORD(srlFreePage, "SRLFreePage::append");
	putInt(dbb->tableSpaceId);
	putInt(page);
	sync.unlock();
}

void SRLFreePage::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	pageNumber = getInt();
	
	if (log->tracePage == pageNumber)
		print();
}

void SRLFreePage::pass1()
{
	incarnation = log->bumpPageIncarnation(pageNumber, tableSpaceId, objDeleted);
}

void SRLFreePage::pass2(void)
{
	if (!log->bumpPageIncarnation(pageNumber, tableSpaceId, objDeleted))
		return;

	if (pageNumber == log->tracePage)
		print();

	log->redoFreePage(pageNumber, tableSpaceId);
}

void SRLFreePage::redo()
{
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objDeleted);
}

void SRLFreePage::print()
{
	logPrint("Free Page %d  tableSpaceId %d\n", pageNumber, tableSpaceId);
}

void SRLFreePage::commit(void)
{
	if (pageNumber == log->tracePage)
		print();
}
