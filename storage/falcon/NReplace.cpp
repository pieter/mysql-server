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

// NReplace.cpp: implementation of the NReplace class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NReplace.h"
#include "CompiledStatement.h"
#include "Context.h"
#include "Index.h"
#include "Table.h"
#include "Field.h"
#include "Fsb.h"
#include "SQLError.h"
#include "NField.h"
#include "NSelect.h"
#include "Statement.h"
#include "Record.h"
#include "Value.h"
#include "ResultSet.h"
#include "PrettyPrint.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NReplace::NReplace(CompiledStatement *statement, Syntax *syntax) :
	NInsert (statement, syntax)
{
	type = Replace;
	Context *context = statement->makeContext (table, PRIV_MASK (PrivUpdate));
	statement->pushContext (context);
	contextId = context->contextId;
	LinkedList conjuncts;
	index = table->getPrimaryKey();

	if (!index)
		throw SQLEXCEPTION (SYNTAX_ERROR, "no primary key for replace table \"%s\"",
							(const char*) table->name);

	for (int n = 0; n < index->numberFields; ++n)
		{
		Field *field = index->fields [n];
		NNode *node = NULL;
		for (int m = 0; m < numberFields; ++m)
			if (fields [m] == field)
				{
				if (select)
					node = select->values->children [m];
				else
					node = children [m];
				break;
				}
		if (!node)
			throw SQLEXCEPTION (SYNTAX_ERROR, "primary key field \"%s\" not in replace field list",
								(const char*) field->name);
		NNode *expr = new NNode (statement, Eql, 2);
		expr->setChild (0, new NField (statement, field, context));
		expr->setChild (1, node);
		expr->fini();
		conjuncts.append (expr);			
		}

	void *contextMark = statement->markContextStack();

	if (select)
		select->pushContexts();

	stream = statement->compileStream (context, conjuncts, this);
	statement->popContexts (contextMark);
}

NReplace::~NReplace()
{
	if (stream)
		delete stream;
}

void NReplace::evalStatement(Statement *statement)
{
	if (select)
		{
		select->evalStatement (statement);
		ResultSet *resultSet = statement->getResultSet();
		while (resultSet->next())
			doReplace (statement, resultSet);
		}
	else
		doReplace (statement, NULL);
}

void NReplace::doReplace(Statement *statement, ResultSet *resultSet)
{
	statement->updateStatements = true;
	Value **values = new Value* [numberFields];

	try
		{
		Transaction *transaction = statement->transaction;
		int n;

		// Compute values for fields

		for (n = 0; n < numberFields; ++n)
			if (resultSet)
				values [n] = resultSet->getValue (n + 1);
			else
				values [n] = children [n]->eval (statement);

		// See if we fetch a record.  If not, do an insert

		stream->open (statement);

		if (!stream->fetch (statement))
			{
			stream->close (statement);

			try
				{
				table->insert (statement->transaction, numberFields, fields, values);
				}
			catch (...)
				{
				stream->open(statement);
				stream->fetch(statement);
				stream->close(statement);
				table->insert (statement->transaction, numberFields, fields, values);
				}

			++statement->stats.inserts;
			++statement->stats.replaces;
			++statement->recordsUpdated;
			delete [] values;

			return;
			}

		Context *context = statement->getContext (contextId);
		Record *record = context->record;
		bool diff = false;

		// Check for any differences.  If not, it's a no-op

		for (n = 0; n < numberFields; ++n)
			if (!diff)
				{
				Value value;
				record->getValue (fields [n]->id, &value);
				if (value.compare (values [n]) != 0)
					{
					diff = true;
					break;
					}
				}

		// If anything changed, update the record

		if (diff)
			{
			table->update (transaction, context->record, numberFields, fields, values);
			++statement->stats.updates;
			++statement->stats.replaces;
			++statement->recordsUpdated;
			}

		stream->close (statement);
		delete [] values;

		return;
		}
	catch (...)
		{
		delete [] values;
		throw;
		}
}

void NReplace::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent (level++);
	pp->format ("Replace %s.%s %d\n", table->getSchema(), table->getName(), contextId);

	if (stream)
		stream->prettyPrint (level, pp);

	if (select)
		select->prettyPrint (level, pp);
}

