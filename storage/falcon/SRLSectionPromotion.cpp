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
#include "SRLSectionPromotion.h"
#include "SerialLog.h"
#include "Section.h"
#include "Dbb.h"
#include "SerialLogControl.h"


SRLSectionPromotion::SRLSectionPromotion(void)
{
}

SRLSectionPromotion::~SRLSectionPromotion(void)
{
}


void SRLSectionPromotion::append(Dbb *dbb, int id, int32 rootPage, int pageLength, const UCHAR* pageData, int32 page)
{
	START_RECORD(srlSectionPromotion, "SRLSectionPromotion::append");
	putInt(dbb->tableSpaceId);
	putInt(id);
	putInt(rootPage);
	putInt(page);
	putInt(pageLength);
	putData(pageLength, pageData);
	sync.unlock();
}

void SRLSectionPromotion::read(void)
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;
		
	sectionId = getInt();
	rootPageNumber = getInt();
	pageNumber = getInt();
	length = getInt();
	data = getData(length);
	
	if (log->tracePage == pageNumber ||
		(log->tracePage && log->tracePage == rootPageNumber))
		print();
}

void SRLSectionPromotion::pass1(void)
{
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLSectionPromotion::pass2(void)
{
	if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
		if (control->isPostFlush())
			Section::redoSectionPromotion(log->getDbb(tableSpaceId), sectionId, rootPageNumber, length, data, pageNumber);
}

void SRLSectionPromotion::redo(void)
{
	if (!log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
		return;

	//Section::redoSectionPromotion(log->getDbb(tableSpaceId), sectionId, rootPageNumber, length, data, pageNumber);
}

void SRLSectionPromotion::print(void)
{
	logPrint("Section Promotion: section %d/%d, pageNumber, root page %d\n",
			sectionId, tableSpaceId, pageNumber, rootPageNumber);
}
