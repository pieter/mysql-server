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

// FsbInversion.cpp: implementation of the FsbInversion class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "FsbInversion.h"
#include "Context.h"
#include "NNode.h"
#include "Table.h"
#include "Statement.h"
#include "Log.h"
#include "PrettyPrint.h"
#include "Bitmap.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FsbInversion::FsbInversion(Context *context, NNode *node, Row *rowSource)
{
	table = context->table;
	contextId = context->contextId;
	inversion = node;
	row = rowSource;
}

FsbInversion::~FsbInversion()
{
}

void FsbInversion::open(Statement * statement)
{
	Bitmap *bitmap = inversion->evalInversion (statement);
	Context *context = statement->getContext (contextId);
	context->setBitmap (bitmap);
	context->open();
}

Row* FsbInversion::fetch(Statement * statement)
{
	if (statement->getContext (contextId)->fetchIndexed (statement))
		return row;

	return NULL;
}

void FsbInversion::close(Statement * statement)
{
	statement->getContext (contextId)->close();
}

void FsbInversion::getStreams(int **ptr)
{
	*(*ptr)++ = contextId;
}

void FsbInversion::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent (level++);
	pp->format ("Inversion %s.%s (%d)\n", table->getSchema(), table->getName(), contextId);
	inversion->prettyPrint (level, pp);
}
