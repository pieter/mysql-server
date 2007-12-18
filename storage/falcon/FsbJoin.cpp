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

// FsbJoin.cpp: implementation of the FsbJoin class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "FsbJoin.h"
#include "Statement.h"
#include "CompiledStatement.h"
#include "Log.h"
#include "PrettyPrint.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FsbJoin::FsbJoin(CompiledStatement *statement, int numberStreams, Row *rowSource)
{
	count = numberStreams;
	streams = new Fsb* [count];
	memset (streams, 0, sizeof (Fsb*) * count);
	slot = statement->getGeneralSlot();
	row = rowSource;
}

FsbJoin::~FsbJoin()
{
	if (streams)
		{
		for (int n = 0; n < count; ++n)
			if (streams [n])
				delete streams [n];
				
		delete [] streams;
		}
}

void FsbJoin::setStream(int index, Fsb * stream)
{
	streams [index] = stream;
}

void FsbJoin::open(Statement * statement)
{
	statement->slots [slot] = 0;
	streams [0]->open (statement);
}

Row* FsbJoin::fetch(Statement * statement)
{
	int n = statement->slots [slot];

	for (;;)
		{
		if (streams [n]->fetch (statement))
			{
			if (n == count - 1)
				{
				statement->slots [slot] = n;
				
				return row;
				}
				
			streams [++n]->open (statement);
			}
		else
			{
			if (n == 0)
				{
				statement->slots [slot] = n;
				close (statement);
				
				return NULL;
				}
				
			streams [n--]->close (statement);
			}
		}
}

void FsbJoin::close(Statement * statement)
{
	for (int n = 0; n < count; ++n)
		streams [n]->close (statement);
}


void FsbJoin::getStreams(int **ptr)
{
	for (int n = 0; n < count; ++n)
		streams [n]->getStreams (ptr);
}

void FsbJoin::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent (level++);
	pp->put (getType());

	for (int n = 0; n < count; ++n)
		streams [n]->prettyPrint (level, pp);
}

const char* FsbJoin::getType()
{
	return "Join\n";
}
