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

// FsbExhaustive.cpp: implementation of the FsbExhaustive class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "FsbExhaustive.h"
#include "Context.h"
#include "Table.h"
#include "Statement.h"
#include "Log.h"
#include "PrettyPrint.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FsbExhaustive::FsbExhaustive(Context *context, Row *rowSource)
{
	contextId = context->contextId;
	table = context->table;
	row = rowSource;
}

FsbExhaustive::~FsbExhaustive()
{

}

void FsbExhaustive::open(Statement * statement)
{
	statement->getContext (contextId)->open();
}

Row* FsbExhaustive::fetch(Statement * statement)
{
	if (statement->getContext (contextId)->fetchNext (statement))
		return row;

	return NULL;
}

void FsbExhaustive::close(Statement * statement)
{
	statement->getContext (contextId)->close();
}

void FsbExhaustive::getStreams(int **ptr)
{
	*(*ptr)++ = contextId;
}

void FsbExhaustive::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent (level++);
	pp->format ("Exhaustive %s.%s (%d)\n", table->getSchema(), table->getName(), contextId);
}
