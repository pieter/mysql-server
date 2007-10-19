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

// FsbOuterJoin.cpp: implementation of the FsbOuterJoin class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "FsbOuterJoin.h"
#include "CompiledStatement.h"
#include "Statement.h"
#include "Log.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FsbOuterJoin::FsbOuterJoin(CompiledStatement *statement, int numberStreams, Row *rowSource) : FsbJoin (statement, numberStreams, rowSource)
{
	stateSlot = statement->getGeneralSlot();
}

FsbOuterJoin::~FsbOuterJoin()
{

}

Row* FsbOuterJoin::fetch(Statement *statement)
{
	// If we played along last time, don't do it again.

	if (statement->slots [stateSlot] < 0)
		return NULL;

	// If we get a record, cool

	Row *row = FsbJoin::fetch (statement);

	if (row)
		{
		++statement->slots [stateSlot];
		return row;
		}

	// If we found at least one record, we're done

	if (statement->slots [stateSlot])
		return NULL;

	// Pretend we have an all null row, but don't do it again.

	statement->slots [stateSlot] = -1;

	return this;
}

void FsbOuterJoin::open(Statement *statement)
{
	FsbJoin::open (statement);
	statement->slots [stateSlot] = 0;
}

/***
Value* FsbOuterJoin::getValue(Statement *statement, int index)
{
	return &nullValue;
}
***/
