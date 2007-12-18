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

// NDelete.cpp: implementation of the NDelete class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NDelete.h"
#include "Syntax.h"
#include "CompiledStatement.h"
#include "Connection.h"
#include "Statement.h"
#include "Context.h"
#include "Fsb.h"
#include "Table.h"
#include "SQLError.h"
#include "Privilege.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NDelete::NDelete(CompiledStatement *statement, Syntax *syntax) : NNode (statement, Delete)
{
	compile (statement, syntax);
}


NDelete::NDelete(CompiledStatement *statement, Syntax *syntax, NType nType) : NNode (statement, nType)
{
	compile (statement, syntax);
}

NDelete::~NDelete()
{
	if (stream)
		delete stream;
}

void NDelete::compile(CompiledStatement *statement, Syntax *syntax)
{
	Context *context = statement->compileContext (syntax->getChild (0), PRIV_MASK (PrivDelete));
	table = context->table;
	contextId = context->contextId;
	statement->pushContext (context);
	Syntax *boolean = syntax->getChild(1);

	if (boolean && boolean->type == nod_cursor)
		{
		cursorName = boolean->getChild (0)->getString();
		stream = NULL;
		}
	else
		{
		LinkedList conjuncts;

		if (boolean)
			{
			NNode *stuff = statement->compile (boolean);
			stuff->decomposeConjuncts (conjuncts);
			}
		stream = statement->compileStream (context, conjuncts, this);
		}

	statement->popContext();
}

void NDelete::evalStatement(Statement * statement)
{
	statement->updateStatements = true;
	Transaction *transaction = statement->transaction;

	// Handle current of cursor case
	
	if (!stream)
		{
		Statement *parent = statement->connection->findStatement (cursorName);
		
		if (!parent)
			throw SQLEXCEPTION (RUNTIME_ERROR, "can't find cursor %s", 
													(const char*) cursorName);
													
		Context *context = parent->getUpdateContext();

		if (!context || context->table != table)
			throw SQLEXCEPTION (RUNTIME_ERROR, "statement %s is not updatable",
												   (const char*) cursorName);
												   
		table->deleteRecord (transaction, context->record);
		
		return;
		}

	// Handle "delete from <table>..."
	
	stream->open (statement);
	Context *context = statement->getContext (contextId);

	while (stream->fetch (statement))
		{
		table->deleteRecord (transaction, context->record);
		++statement->recordsUpdated;
		}

	stream->close (statement);

	return;
}

bool NDelete::references(Table *tbl)
{
	return table == tbl;
}

/***
Value* NDelete::getValue(Statement *statement, int index)
{
	return &nullValue;
}
***/

