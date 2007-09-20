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
// SRLCreateIndex.cpp: implementation of the SRLCreateIndex class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLCreateIndex.h"
#include "SerialLogControl.h"
#include "Index.h"
#include "IndexRootPage.h"
#include "Dbb.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLCreateIndex::SRLCreateIndex()
{

}

SRLCreateIndex::~SRLCreateIndex()
{

}

void SRLCreateIndex::append(Dbb *dbb, TransId transId, int32 id, int idxVersion)
{
	START_RECORD(srlCreateIndex, "SRLCreateIndex::append");
	log->getTransaction(transId);
	log->setIndexActive(id, dbb->tableSpaceId);
	putInt(dbb->tableSpaceId);
	putInt(id);
	putInt(idxVersion);
	putInt(transId);
	sync.unlock();
}

void SRLCreateIndex::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	indexId = getInt();
	indexVersion = getInt();
	transactionId = getInt();
}

void SRLCreateIndex::pass1()
{
	log->bumpIndexIncarnation(indexId, tableSpaceId, objInUse);
}

void SRLCreateIndex::redo()
{
	if (!log->bumpIndexIncarnation(indexId, tableSpaceId, objInUse))
		return;

	switch (indexVersion)
		{
		case INDEX_VERSION_1:
			IndexRootPage::redoCreateIndex(log->getDbb(tableSpaceId), indexId);
			break;
		
		default:
			ASSERT(false);
		}
}

void SRLCreateIndex::print()
{
	logPrint("Create Index %d\n", indexId);
}

void SRLCreateIndex::commit(void)
{
}
