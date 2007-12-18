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
#include "SRLSequence.h"
#include "SerialLog.h"
#include "Database.h"
#include "Dbb.h"
#include "Log.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SRLSequence::SRLSequence()
{

}

SRLSequence::~SRLSequence()
{

}

void SRLSequence::read()
{
	sequenceId = getInt();
	sequence = getInt64();
}

void SRLSequence::append(int sequenceId, int64 sequence)
{
	START_RECORD(srlSequence, "SRLSequence::append");
	putInt(sequenceId);
	putInt64(sequence);
	sync.unlock();
}


void SRLSequence::redo()
{
	log->defaultDbb->redoSequence(sequenceId, sequence);
}

void SRLSequence::print(void)
{
	logPrint("Sequence id %d, value " I64FORMAT "\n", sequenceId, sequence);
}
