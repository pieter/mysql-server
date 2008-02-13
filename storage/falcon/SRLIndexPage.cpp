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

// SRLIndexPage.cpp: implementation of the SRLIndexPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLIndexPage.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Dbb.h"
#include "IndexRootPage.h"
#include "Index2RootPage.h"
#include "Index.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLIndexPage::SRLIndexPage()
{

}

SRLIndexPage::~SRLIndexPage()
{

}

void SRLIndexPage::append(Dbb *dbb, TransId transId, int idxVersion, int32 page, int32 lvl, int32 up, int32 left, int32 right, int length, const UCHAR *data)
{
	START_RECORD(srlIndexPage, "SRLIndexPage::append");
	
	if (transId)
		{
		SerialLogTransaction *trans = log->getTransaction(transId);

		if (trans)
			trans->setPhysicalBlock();
		}
	
	putInt(idxVersion);	
	putInt(dbb->tableSpaceId);
	putInt(page);
	putInt(lvl);
	putInt(up);
	putInt(left);
	putInt(right);
	putInt(length);
	putData(length, data);
}

void SRLIndexPage::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	if (control->version >= srlVersion12)
		indexVersion = getInt();
	else
		indexVersion = INDEX_VERSION_1;

	pageNumber = getInt();
	level = getInt();
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

void SRLIndexPage::pass1()
{
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}

void SRLIndexPage::pass2()
{
	if (log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse))
		{
		if (log->tracePage == pageNumber)
			print();

		if (control->isPostFlush())
			switch (indexVersion)
				{
				case INDEX_VERSION_0:
					Index2RootPage::redoIndexPage(log->getDbb(tableSpaceId), pageNumber, parent, level, prior, next, length, data);
					break;
				
				case INDEX_VERSION_1:
					IndexRootPage::redoIndexPage(log->getDbb(tableSpaceId), pageNumber, parent, level, prior, next, length, data);
					break;
				
				default:
					ASSERT(false);
				}
		}
	else 
		if (log->tracePage == pageNumber)
			print();
}

void SRLIndexPage::print()
{
	logPrint("Index page %d/%d, level %d, parent %d, prior %d, next %d\n", pageNumber, tableSpaceId, level, parent, prior, next);
}

void SRLIndexPage::redo()
{
	log->bumpPageIncarnation(pageNumber, tableSpaceId, objInUse);
}
