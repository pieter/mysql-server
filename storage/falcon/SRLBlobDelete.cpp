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

#include <stdio.h>
#include "Engine.h"
#include "SRLBlobDelete.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Dbb.h"
#include "Section.h"

SRLBlobDelete::SRLBlobDelete(void)
{
}

SRLBlobDelete::~SRLBlobDelete(void)
{
}

void SRLBlobDelete::append(Dbb* dbb, int32 locPage, int locLine, int32 blobPage, int blobLine)
{
	START_RECORD(srlBlobDelete, "SRLBlobDelete::append");
	putInt(dbb->tableSpaceId);
	putInt(locPage);
	putInt(locLine);
	putInt(blobPage);
	putInt(blobLine);
}

void SRLBlobDelete::read(void)
{
	tableSpaceId = getInt();
	locatorPage = getInt();
	locatorLine = getInt();
	dataPage = getInt();
	dataLine = getInt();
	
	if (locatorPage == log->tracePage || dataPage == log->tracePage)
		print();
}

void SRLBlobDelete::pass1(void)
{
	log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
	log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);
}

void SRLBlobDelete::pass2(void)
{
	bool ret1 = log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
	bool ret2 = log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);
	
	if (ret1)
		{
		Dbb *dbb = log->getDbb(tableSpaceId);
		
		if (control->isPostFlush())
			Section::redoBlobDelete(dbb, locatorPage, locatorLine, dataPage, dataLine, ret2);
		}
}

void SRLBlobDelete::redo(void)
{
	log->bumpPageIncarnation(locatorPage, tableSpaceId, objInUse);
	log->bumpPageIncarnation(dataPage, tableSpaceId, objInUse);
}

void SRLBlobDelete::print(void)
{
	logPrint("BlobDelete locator page %d/%d, line %d, blob page %d/%d\n",
			 locatorPage, tableSpaceId, locatorLine, dataPage, dataLine);
}
