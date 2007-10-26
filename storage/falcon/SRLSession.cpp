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
#include "SRLSession.h"

SRLSession::SRLSession(void)
{
}

SRLSession::~SRLSession(void)
{
}

void SRLSession::append(int64 priorRecoveryBlock, int64 priorCheckpointBlock)
{
	START_RECORD(srlCheckpoint, "SRLCheckpoint::append");
	putInt64(priorRecoveryBlock);
	putInt64(priorCheckpointBlock);
}

void SRLSession::read(void)
{
	recoveryBlock = getInt64();
	checkpointBlock = getInt64();
}

void SRLSession::print(void)
{
	logPrint("Session start recovery " I64FORMAT ", checkpoint " I64FORMAT ", \n", recoveryBlock, checkpointBlock);
}
