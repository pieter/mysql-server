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

// FsbUnion.cpp: implementation of the FsbUnion class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "FsbUnion.h"
#include "Statement.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FsbUnion::FsbUnion(CompiledStatement *statement, int numberStreams, Row *rowSource)
	: FsbJoin (statement, numberStreams, rowSource)
{

}

FsbUnion::~FsbUnion()
{
	if (streams)
		{
		delete [] streams;
		streams = NULL;
		}
}

Row* FsbUnion::fetch(Statement *statement)
{
	Row *row;

	for (int n = statement->slots [slot];;)
		{
		if ((row = streams [n]->fetch (statement)))
			return row;
		streams [n]->close (statement);
		statement->slots [slot] = ++n;
		if (n >= count)
			break;
		streams [n]->open (statement);
		}

	return NULL;
}

const char* FsbUnion::getType()
{
	return "Union\n";
}

int FsbUnion::getStreamIndex(Statement *statement)
{
	return statement->slots [slot];
}
