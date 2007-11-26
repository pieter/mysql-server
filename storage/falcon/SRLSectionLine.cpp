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

#include <stdio.h>
#include "Engine.h"
#include "SRLSectionLine.h"
#include "SerialLog.h"
#include "Section.h"
#include "Dbb.h"
#include "SerialLogControl.h"

SRLSectionLine::SRLSectionLine(void)
{
}

SRLSectionLine::~SRLSectionLine(void)
{
}

void SRLSectionLine::append(Dbb *dbb, int32 sectionPageNumber, int32 dataPageNumber)
{
	START_RECORD(srlSectionLine, "SRLSectionLine::append");
	putInt(dbb->tableSpaceId);
	putInt(sectionPageNumber);
	putInt(dataPageNumber);
	sync.unlock();
}

void SRLSectionLine::read(void)
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;
	
	pageNumber = getInt();
	dataPageNumber = getInt();
}

void SRLSectionLine::pass1(void)
{
	if (pageNumber == log->tracePage || dataPageNumber == log->tracePage)
		print();
		
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
	log->bumpPageIncarnation(dataPageNumber, tableSpaceId, objInUse);
}

void SRLSectionLine::pass2(void)
{
	if (pageNumber == log->tracePage || dataPageNumber == log->tracePage)
		print();
		
	log->bumpPageIncarnation(dataPageNumber, tableSpaceId, objInUse);

	if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
		Section::redoSectionLine(log->getDbb(tableSpaceId), pageNumber, dataPageNumber);

}

void SRLSectionLine::redo(void)
{
	if (pageNumber == log->tracePage || dataPageNumber == log->tracePage)
		print();
		
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
	log->bumpPageIncarnation(dataPageNumber, tableSpaceId, objInUse);
}

void SRLSectionLine::print(void)
{
	logPrint("SectionLine: locator page %d/%d, data page %d\n", pageNumber, tableSpaceId, dataPageNumber);
}
