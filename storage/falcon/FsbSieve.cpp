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

// FsbSieve.cpp: implementation of the FsbSieve class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "FsbSieve.h"
#include "NNode.h"
#include "Log.h"
#include "PrettyPrint.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FsbSieve::FsbSieve(NNode *node, Fsb *source)
{
	boolean = node;
	stream = source;
}

FsbSieve::~FsbSieve()
{
	delete stream;
}

void FsbSieve::open(Statement * statement)
{
	stream->open(statement);
}

Row* FsbSieve::fetch(Statement * statement)
{
	for (;;)
		{
		Row *row = stream->fetch(statement);
		
		if (!row)
			return NULL;
			
		int result = boolean->evalBoolean(statement);
		
		if (result && result != NULL_BOOLEAN)
			return row;
		}
}

void FsbSieve::close(Statement * statement)
{
	boolean->close(statement);
	stream->close(statement);
}

void FsbSieve::getStreams(int **ptr)
{
	stream->getStreams(ptr);
}

void FsbSieve::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent(level++);
	pp->put("Boolean sieve\n");
	boolean->prettyPrint(level, pp);
	stream->prettyPrint(level, pp);
}
