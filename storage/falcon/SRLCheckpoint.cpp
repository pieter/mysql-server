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

// SRLCheckpoint.cpp: implementation of the SRLCheckpoint class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SRLCheckpoint.h"
#include "SRLVersion.h"
#include "SerialLogControl.h"
#include "SerialLog.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLCheckpoint::SRLCheckpoint()
{

}

SRLCheckpoint::~SRLCheckpoint()
{

}

void SRLCheckpoint::append(int64 blockNumber)
{
	START_RECORD(srlCheckpoint, "SRLCheckpoint::append");
	putInt64(blockNumber);
	log->flush(false, 0, &sync);
}

void SRLCheckpoint::read()
{
	if (control->version >= srlVersion9)
		blockNumber = getInt64();
	else
		blockNumber = 0;
}

void SRLCheckpoint::pass1()
{
	control->haveCheckpoint(blockNumber);
}

void SRLCheckpoint::redo()
{

}

void SRLCheckpoint::print()
{
	logPrint("Checkpoint block " I64FORMAT "\n", blockNumber);
}
