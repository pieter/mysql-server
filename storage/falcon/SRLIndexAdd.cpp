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
#include "SRLIndexAdd.h"
#include "IndexKey.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Dbb.h"
#include "Transaction.h"
#include "Index.h"

// SRLIndexAdd.cpp: implementation of the SRLIndexAdd class.
//
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLIndexAdd::SRLIndexAdd()
{

}

SRLIndexAdd::~SRLIndexAdd()
{

}

void SRLIndexAdd::append(Dbb *dbb, int32 indexId, int idxVersion, IndexKey *key, int32 recordNumber, TransId transactionId)
{
	START_RECORD(srlIndexAdd, "SRLIndexAdd::append");
	putInt(dbb->tableSpaceId);
	log->getTransaction(transactionId);
	putInt(transactionId);
	putInt(indexId);
	putInt(idxVersion);
	putInt(recordNumber);
	putInt(key->keyLength);
	log->putData(key->keyLength, key->key);
}

void SRLIndexAdd::read()
{
	if (control->version >= srlVersion8)
		tableSpaceId = getInt();
	else
		tableSpaceId = 0;

	transactionId = getInt();
	indexId = getInt();
	indexVersion = getInt();
	recordId = getInt();
	length = getInt();
	data = getData(length);
}

void SRLIndexAdd::redo()
{
	if (!log->isIndexActive(indexId, tableSpaceId))
		return;

	//SerialLogTransaction *transaction = 
	control->getTransaction(transactionId);
	IndexKey indexKey(length, data);
	log->getDbb(tableSpaceId)->addIndexEntry(indexId, indexVersion, &indexKey, recordId, NO_TRANSACTION);
}

void SRLIndexAdd::pass1()
{
	control->getTransaction(transactionId);
}

void SRLIndexAdd::print()
{
	logPrint("Index Add transaction %d, indexId %d/%d, recordId %d, length %d\n", 
			transactionId, indexId, tableSpaceId, recordId, length);
}
