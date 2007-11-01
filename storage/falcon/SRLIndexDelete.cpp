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

// SRLDataPage.cpp: implementation of the SRLDataPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLIndexDelete.h"
#include "IndexKey.h"
#include "SerialLogControl.h"
#include "Dbb.h"
#include "Transaction.h"
#include "Index.h"
#include "Log.h"

// SRLIndexDelete.cpp: implementation of the SRLIndexDelete class.
//
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLIndexDelete::SRLIndexDelete()
{

}

SRLIndexDelete::~SRLIndexDelete()
{

}

void SRLIndexDelete::append(Dbb *dbb, int32 indexId, int idxVersion, IndexKey *key, int32 recordNumber, TransId transactionId)
{
	ASSERT(idxVersion <= INDEX_CURRENT_VERSION);
	START_RECORD(srlIndexDelete, "SRLIndexDelete::append");
	putInt(dbb->tableSpaceId);
	putInt(indexId);
	putInt(idxVersion);
	putInt(recordNumber);
	putInt(key->keyLength);
	log->putData(key->keyLength, key->key);
}

void SRLIndexDelete::read()
{
	if (control->version >= srlVersion7)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	indexId = getInt();
	indexVersion = getInt();
	recordId = getInt();
	length = getInt();
	data = getData(length);
	ASSERT(indexVersion <= INDEX_CURRENT_VERSION);
}


void SRLIndexDelete::redo()
{
	if (!log->isIndexActive(indexId, tableSpaceId))
		return;
		
	IndexKey indexKey(length, data);
	log->getDbb(tableSpaceId)->deleteIndexEntry(indexId, indexVersion, &indexKey, recordId, NO_TRANSACTION);
}

void SRLIndexDelete::print()
{
	logPrint("Index Delete indexId %d, recordId %d, length %d\n", indexId, recordId, length);
}
