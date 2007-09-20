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

// SRLInversionPage.cpp: implementation of the SRLInversionPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLInversionPage.h"
#include "SerialLogControl.h"
#include "Dbb.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLInversionPage::SRLInversionPage()
{

}

SRLInversionPage::~SRLInversionPage()
{

}

void SRLInversionPage::append(Dbb *dbb, int32 page, int32 up, int32 left, int32 right, int length, const UCHAR *data)
{
	START_RECORD(srlInversionPage, "SRLInversionPage::append");
	putInt(dbb->tableSpaceId);
	putInt(page);
	putInt(up);
	putInt(left);
	putInt(right);
	putInt(length);
	putData(length, data);

	sync.unlock();
}

void SRLInversionPage::pass1()
{
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLInversionPage::pass2()
{
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLInversionPage::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	pageNumber = getInt();
	parent = getInt();
	prior = getInt();
	next = getInt();
	length = getInt();
	data = getData(length);

	if (log->tracePage && (log->tracePage == pageNumber ||
						   log->tracePage == prior ||
						   log->tracePage == next))
		print();
}

void SRLInversionPage::redo()
{
	if (!log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
		return;
}

void SRLInversionPage::print()
{
	logPrint("Inversion page %d, parent %d, prior %d, next %d\n", pageNumber, parent, prior, next);
}
