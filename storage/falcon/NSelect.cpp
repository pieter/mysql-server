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

// NSelect.cpp: implementation of the NSelect class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include "Engine.h"
#include "NSelect.h"
#include "Syntax.h"
#include "Table.h"
#include "View.h"
#include "Field.h"
#include "Context.h"
#include "CompiledStatement.h"
#include "SQLError.h"
#include "NField.h"
#include "FsbOuterJoin.h"
#include "FsbSort.h"
#include "FsbUnion.h"
#include "FsbGroup.h"
#include "FsbDerivedTable.h"
#include "ResultSet.h"
#include "Statement.h"
#include "Privilege.h"
#include "Value.h"
#include "Connection.h"
#include "FilterSet.h"
#include "TableFilter.h"
#include "PrettyPrint.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NSelect::NSelect(CompiledStatement *statement, Syntax *syntax, NNode *inExpr) : NNode (statement, Select)
{
	int debug = 0;
	compiledStatement = statement;
	compiledStatement->select = this;
	void *contextMark = statement->markContextStack();
	contexts = NULL;
	values = NULL;
	stream = NULL;
	orgBoolean = NULL;
	groups = NULL;
	groupSlots = NULL;
	unionBranches = NULL;
	countSlot = -1;
	statistical = false;
	Syntax *sort = NULL;
	Syntax *branch = NULL;
	columnNames = NULL;
	select = NULL;

	if (syntax->type == nod_select)
		{
		Syntax *pUnion = syntax->getChild (0);
		sort = syntax->getChild (1);
		
		if (pUnion->count != 1)
			{
			int count = pUnion->count;
			unionBranches = new NSelect* [count];
			FsbUnion *fsbUnion = new FsbUnion (statement, count, this);
			stream = fsbUnion;
			numberContexts = 0;
			
			for (int n = 0; n < count; ++n)
				{
				NSelect *subStream = unionBranches [n] = new NSelect (statement, pUnion->getChild (n), NULL);
				
				if (n == 0)
					numberColumns = subStream->numberColumns;
				else if (numberColumns != subStream->numberColumns)
					throw SQLEXCEPTION (COMPILE_ERROR, "inconsistent select list in union branches");
					
				numberContexts += subStream->numberContexts;
				fsbUnion->setStream (n, subStream->stream);
				}
				
			return;
			}
			
		branch = pUnion->getChild (0);
		}
	else
		branch = syntax;

	Syntax *items = branch->getChild (1);
	Syntax *tableList = branch->getChild(2);
	numberColumns = 0;
	LinkedList conjuncts;
	LinkedList sourceContexts;
	LinkedList tables;

	// Process table list totalling number of fields in case of SELECT *

	NSelectEnv env;
	env.sourceContexts = &sourceContexts;
	env.tables = &tables;
	env.conjuncts = &conjuncts;
	
	if (tableList->type == nod_select)
		{
		select = new NSelect(statement, tableList, NULL);
		numberColumns += select->numberColumns;
		}
	else
		numberColumns += compileJoin(tableList, true, &env);

	// If there isn't an item list, generate a value list

	if (items)
		{
		numberColumns = items->count;
		values = statement->compile(items);
		}
	else
		{
		values = new NNode(statement, List, numberColumns);
		int n = 0;
		
		if (select)
			{
			}
		else
			FOR_OBJECTS (Context*, context, &sourceContexts)
				View *view = context->table->view;
				
				if (view)
					for (int id = 0; id < view->numberColumns; ++id)
						values->setChild(n++, view->columns->children[id]->copy(statement, context));
				else
					FOR_FIELDS (field, context->table)
						values->setChild(n++, new NField(statement, field, context));
					END_FOR;
					
			END_FOR;
		}

	columnNames = new const char* [numberColumns];
	int n;

	for (n = 0; n < numberColumns; ++n)
		columnNames [n] = values->children [n]->getName();

	Syntax *groupBy = branch->getChild(4);

	if (groupBy)
		{
		groups = statement->compile (groupBy);
		groupSlots = new int [groups->count];
		
		for (int n = 0; n < groups->count; ++n)
			groupSlots [n] = statement->getValueSlot();
			
		//Syntax *having = branch->getChild(5);
		
		if (countSlot < 0)
			countSlot = statement->getGeneralSlot();
		}

	for (n = 0; n < values->count; ++n)
		if (values->children [n]->isStatistical())
			statistical = true;

	for (n = 0; n < values->count; ++n)
		{
		NNode *child = values->children [n];
		bool stat = child->isStatistical();
		
		if (stat != statistical)
			if (!groups || !groups->isMember (child))
				throw SQLEXCEPTION (COMPILE_ERROR, "mix of statistic/scalar expression in select list");
		}

	// Handle predicate, if any

	Syntax *boolean = branch->getChild(3);

	if (boolean)
		{
		orgBoolean = statement->compile (boolean);
		orgBoolean->decomposeConjuncts (conjuncts);
		}

	// Generate equality test for <expr> in (select ...)

	if (inExpr)
		{
		NNode *expr = new NNode (statement, Eql, 2);
		expr->setChild(0, inExpr);
		expr->setChild(1, values->children[0]);
		conjuncts.append(expr);
		}

	// Figure out which are the real tables (mean not views)

	numberContexts = 0;
	LinkedList *activeFilters = &statement->connection->filterSets;

	FOR_OBJECTS (Context*, context, &tables)
		++numberContexts;
		statement->pushContext (context);
		context->select = true;
		Table *table = context->table;
		
		FOR_OBJECTS (FilterSet*, filterSet, activeFilters)
			TableFilter *filter = filterSet->findFilter (table);
			
			if (filter)
				{
				if (statement->filteredTables.isMember (table))
					throw SQLEXCEPTION (COMPILE_ERROR, "filterset %s.%s has circular reference to %s.%s",
										filterSet->schema, filterSet->name, table->schemaName, table->name);
										
				statement->filteredTables.push (table);
				statement->addFilter (filter);
				const char* oldAlias = context->alias;
				context->alias = filter->alias;
				NNode *filterBoolean = statement->compile (filter->syntax);
				compiledStatement->select = this;
				filterBoolean->decomposeConjuncts (conjuncts);
				context->alias = oldAlias;
				statement->filteredTables.pop();
				}
		END_FOR
		
		statement->popContext();			
	END_FOR;

	contexts = new Context* [numberContexts];

	if (select)
		{
		stream = new FsbDerivedTable(select);
		}
	else if (numberContexts == 1)
		{
		contexts [0] = (Context*) tables.getElement (0);
		stream = statement->compileStream (contexts [0], conjuncts, this);
		}
	else
		{
		int inner = 0;
		int outer = 0;
		
		FOR_OBJECTS (Context*, context, &tables)
			context->setComputable (false);
			
			if (outer == 0 && context->type == CtxInnerJoin)
				++inner;
			else
				++outer;
		END_FOR;
		
		FsbJoin *join = new FsbJoin (statement, inner + ((outer) ? 1 : 0), this);
		stream = join;
		FsbOuterJoin *outerJoin = NULL;

		if (outer)
			{
			outerJoin = new FsbOuterJoin (statement, outer, this);
			join->setStream (inner, outerJoin);
			}

		int streamNumber = 0;

		FOR_OBJECTS (Context*, context, &tables)
			contexts [streamNumber] = context;
			Fsb *stream = statement->compileStream(context, conjuncts, this);
			
			if (streamNumber < inner)
				join->setStream (streamNumber, stream);
			else
				outerJoin->setStream(streamNumber - inner, stream);
				
			++streamNumber;
		END_FOR;
		}

	if (groups)
		{
		stream = new FsbSort(statement, groups, stream, simple);
		stream = new FsbGroup(this, groups, stream);
		}

	if (branch->getChild(0))
		stream = new FsbSort(statement, values, stream, distinct);

	if (sort)
		{
		NNode *nsort = statement->compile(sort);
		stream = new FsbSort(statement, nsort, stream, order);
		}

	statement->popContexts(contextMark);
	
	if (debug)
		{
		PrettyPrint pp(-1, NULL);
		stream->prettyPrint(0, &pp);
		}
}

NSelect::~NSelect()
{
	delete [] contexts;
	delete stream;
	delete [] columnNames;
	delete [] groupSlots;
	delete [] unionBranches;
}

void NSelect::evalStatement(Statement * statement)
{
	statement->eof = false;
	stream->open (statement);
	statement->createResultSet(this, numberColumns);
	
	if (countSlot >= 0)
		statement->slots [countSlot] = 0;
}

bool NSelect::next(Statement * statement, ResultSet * resultSet)
{
	ASSERT (statement->statement == compiledStatement);
	Row *row;

	if (statistical && !groups)
		{
		if (statement->eof)
			return false;
			
		NNode **children = values->children;
		
		for (int n = 0; n < numberColumns; ++n)
			children [n]->reset(statement);
			
		while (stream->fetch (statement))
			for (int n = 0; n < numberColumns; ++n)
				children [n]->increment(statement);
				
		stream->close(statement);
		statement->eof = true;
		row = this;
		}
	else if (!(row = stream->fetch(statement)))
		{
		stream->close(statement);
		
		return false;
		}

	/***
	if (unionBranches)
		unionBranches [stream->getStreamIndex (statement)]->setValues (statement, resultSet);
	else
		setValues (statement, resultSet);
	***/

	for (int n = 0; n < numberColumns; ++n)
		{
		Value *value = row->getValue(statement, n);
		resultSet->setValue(n, value);
		}

	return true;
}


Field* NSelect::getField(int index)
{
	if (unionBranches)
		return unionBranches [0]->getField (index);

	return values->children [index]->getField();
}


bool NSelect::isMember(Table * table)
{
	return values->isMember (table);
}

bool NSelect::references(Table * table)
{
	for (int n = 0; n < numberContexts; ++n)
		if (contexts [n]->table == table)
			return true;

	return false;
}

Field* NSelect::getField(const char * fieldName)
{
	for (int n = 0; n < numberColumns; ++n)
		{
		Field *field = values->children [n]->getField();
		if (field && !strcasecmp (fieldName, field->name))
			return field;
		}

	int column = getColumnIndex(fieldName);

	if (column >= 0)
		{
		Field *field = values->children [column]->getField();
		if (field)
			return field;
		}

	return NULL;
}

const char* NSelect::getColumnName(int index)
{
	if (unionBranches)
		return unionBranches [0]->getColumnName (index);

    return values->children [index]->getName();
}

int NSelect::getColumnIndex(const char * columnName)
{
	if (unionBranches)
		return unionBranches [0]->getColumnIndex (columnName);

	for (int n = 0; n < numberColumns; ++n)
		if (!strcasecmp (columnNames [n], columnName))
			return n;

	return -1;
}

int NSelect::getIndex(Field * field)
{
	for (int n = 0; n < numberColumns; ++n)
		{
		Field *fld = values->children [n]->getField();
		if (field == fld)
			return n;
		}

	return -1;
}

bool NSelect::computable(CompiledStatement *statement)
{
	if (!orgBoolean)
		return true;

	int n;

	for (n = 0; n < numberContexts; ++n)
		statement->pushContext (contexts [n]);
				
	bool ret = orgBoolean->computable (statement);

	for (n = 0; n < numberContexts; ++n)
		statement->popContext();
				
	return ret;
}


int NSelect::compileJoin(Syntax *node, bool innerJoin, NSelectEnv *env)
{
	int numberColumns = 0;
	bool type = innerJoin;

	switch (node->type)
		{
		case nod_join:
			FOR_SYNTAX (table, node)
				numberColumns += compileJoin (table, type, env);
				type = true;
			END_FOR;
			
			break;

		case nod_table:
			{
			Context *context = statement->compileContext (node, PRIV_MASK (PrivSelect));
			statement->pushContext (context);
			env->sourceContexts->append (context);
			context->getTableContexts (*env->tables, (innerJoin) ? CtxInnerJoin : CtxOuterJoin);
			numberColumns += context->table->numberFields();
			View *view = context->table->view;
			
			if (view)
				{
				statement->popContext();
				
				for (int n = 0; n < view->numberTables; ++n)
					statement->pushContext (context->viewContexts [n]);
					
				if (view->predicate)
					{
					NNode *predicate = view->predicate->copy (statement, context);
					predicate->decomposeConjuncts (*env->conjuncts);
					}
				}
			}
			break;

		case nod_inner:
			{
			numberColumns += compileJoin (node->getChild(0), type, env);
			numberColumns += compileJoin (node->getChild(1), true, env);
			Syntax *boolean = node->getChild (2);
			
			if (boolean)
				{
				NNode *predicate = statement->compile (boolean);
				predicate->decomposeConjuncts (*env->conjuncts);
				}
			}
			break;

		case nod_join_term:
			{
			numberColumns += compileJoin (node->getChild(0), type, env);
			Syntax *boolean = node->getChild (1);
			
			if (boolean)
				{
				NNode *predicate = statement->compile (boolean);
				predicate->decomposeConjuncts (*env->conjuncts);
				}
			}
			break;

		case nod_left_outer:
			{
			numberColumns += compileJoin (node->getChild(0), innerJoin, env);
			numberColumns += compileJoin (node->getChild(1), false, env);
			Syntax *boolean = node->getChild (2);
			
			if (boolean)
				{
				NNode *predicate = statement->compile (boolean);
				predicate->decomposeConjuncts (*env->conjuncts);
				}
			}
			break;

		default:
			node->prettyPrint ("CompiledStatement.compile");
			throw SQLEXCEPTION (FEATURE_NOT_YET_IMPLEMENTED, "join type is not yet implemented");
		}

	return numberColumns;
}

void NSelect::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent (level++);
	pp->put ("Select\n");
	printNode (level, values, pp);

	for (int n = 0; n < count; ++n)
		printNode (level, children [n], pp);

	if (stream)
		stream->prettyPrint (level, pp);
}

NNode* NSelect::getValue(const char *name)
{
	if (columnNames)
		for (int n = 0; n < numberColumns; ++n)
			if (!strcmp (columnNames [n], name))
				return values->children [n];

	return NULL;
}

Value* NSelect::getValue(Statement *statement, int index)
{
	return values->children[index]->eval (statement);
}

int NSelect::getNumberValues()
{
	return numberColumns;
}

void NSelect::pushContexts()
{
	for (int n = 0; n < numberContexts; ++n)
		statement->pushContext (contexts [n]);
}
