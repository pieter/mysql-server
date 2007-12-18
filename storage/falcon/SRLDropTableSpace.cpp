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

// SRLDropTableSpace.cpp: implementation of the SRLDropTableSpace class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SRLDropTableSpace.h"
#include "TableSpace.h"
#include "TableSpaceManager.h"
#include "SRLVersion.h"
#include "SerialLogControl.h"
#include "Transaction.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLDropTableSpace::SRLDropTableSpace()
{

}

SRLDropTableSpace::~SRLDropTableSpace()
{

}

void SRLDropTableSpace::append(TableSpace *tableSpace, Transaction *transaction)
{
	START_RECORD(srlDropTableSpace, "SRLDropTableSpace::append");
	log->getTransaction(transaction->transactionId);
	putInt(tableSpace->tableSpaceId);
	putInt(transaction->transactionId);
}

void SRLDropTableSpace::read()
{
	tableSpaceId = getInt();

	if (control->version >= srlVersion10)
		transactionId = getInt();
	else
		transactionId = 0;
}

void SRLDropTableSpace::pass1()
{

}

void SRLDropTableSpace::pass2()
{

}

void SRLDropTableSpace::commit()
{
	log->tableSpaceManager->expungeTableSpace(tableSpaceId);
}

void SRLDropTableSpace::redo()
{
	log->tableSpaceManager->expungeTableSpace(tableSpaceId);
}
