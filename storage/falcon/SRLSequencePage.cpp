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

// SRLSequence.cpp: implementation of the SRLSequence class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLSequencePage.h"
#include "Dbb.h"
#include "SerialLogControl.h"

SRLSequencePage::SRLSequencePage(void)
{
}

SRLSequencePage::~SRLSequencePage(void)
{
}

void SRLSequencePage::append(Dbb *dbb, int pageSeq, int32 page)
{
	START_RECORD(srlSequencePage, "SRLSequencePage::append");
	putInt(dbb->tableSpaceId);
	putInt(pageSeq);
	putInt(page);
	sync.unlock();
}

void SRLSequencePage::read(void)
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;
	
	pageSequence = getInt();
	pageNumber = getInt();
	
	if (pageNumber == log->tracePage)
		print();
}

void SRLSequencePage::pass1(void)
{
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLSequencePage::pass2(void)
{
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLSequencePage::redo(void)
{
	if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
		log->getDbb(tableSpaceId)->redoSequencePage(pageSequence, pageNumber);
}

void SRLSequencePage::print(void)
{
	logPrint("Sequence Page: sequence %d, page %d/%d\n", pageSequence, pageNumber, tableSpaceId);
}
