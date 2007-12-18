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

// View.cpp: implementation of the View class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "View.h"
#include "Syntax.h"
#include "CompiledStatement.h"
#include "PreparedStatement.h"
#include "NField.h"
#include "Table.h"
#include "SQLError.h"
#include "Context.h"
#include "Field.h"
#include "Stream.h"
#include "Database.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

View::View(const char *viewSchema, const char *viewName)
{
	schema = viewSchema;
	name = viewName;
	tables = NULL;
	columnNames = NULL;
	predicate = NULL;
	columns = NULL;
	sort = NULL;
	nodeList = NULL;
	distinct = false;
}

View::~View()
{
	if (tables)
		delete [] tables;

	if (columnNames)
		delete [] columnNames;

	for (NNode *node; (node = nodeList);)
		{
		nodeList = node->nextNode;
		delete node;
		}
		
}

void View::compile(CompiledStatement * statement, Syntax * viewSyntax)
{

	Syntax *aliases = viewSyntax->getChild (1);
	Syntax *syntax = viewSyntax->getChild (2);
	Syntax *pUnion = syntax->getChild (0);

	if (pUnion->count != 1)
		throw SQLEXCEPTION (FEATURE_NOT_YET_IMPLEMENTED, "NSelect: union not done");

	Syntax *branch = pUnion->getChild (0);
	Syntax *items = branch->getChild (1);
	Syntax *tableList = branch->getChild(2);
	numberColumns = 0;
	numberTables = tableList->count;
	Context **contexts = new Context* [numberTables];
	memset (contexts, 0, sizeof (Context*) * numberTables);
	tables = new Table* [numberTables];
	memset (tables, 0, sizeof (Table*) * numberTables);

	// Process table list totalling number of fields in case of SELECT *

	int n = 0;

	FOR_SYNTAX (table, tableList)
		Context *context = statement->compileContext (table, PRIV_MASK (PrivSelect));
		contexts [n] = context;
		tables [n] = context->table;
		statement->pushContext (context);
		numberColumns += context->table->numberFields();
		++n;
	END_FOR;

	// If there isn't an item list, generate a value list

	if (items)
		{
		numberColumns = items->count;
		columns = statement->compile (items);
		}
	else
		{
		columns = new NNode (statement, List, numberColumns);
		int n = 0;
		for (int c = 0; c < numberTables; ++c)
			{
			Context *context = contexts [c];
			FOR_FIELDS (field, context->table)
				columns->setChild (n++, new NField (statement, field, context));
			END_FOR;
			}
		}

	LinkedList conjuncts;
	Syntax *boolean = branch->getChild(3);

	if (boolean)
		predicate = statement->compile (boolean);

	Syntax *sortClause = syntax->getChild (1);

	if (sortClause)
		sort = statement->compile (sortClause);

	if (branch->getChild (0))
		distinct = true;

	for (n = 0; n < numberTables; ++n)
		statement->popContext();

	columnNames = new JString [numberColumns];

	if (aliases)
		for (n = 0; n < numberColumns; ++n)
			columnNames [n] = aliases->getChild (n)->getString();
	else
		for (n = 0; n < numberColumns; ++n)
			columnNames [n] = columns->children [n]->getName();

	for (n = 0; n < numberColumns; ++n)
		for (int m = n + 1; m < numberColumns; ++m)
			if (columnNames [n] == columnNames [m])
				throw SQLEXCEPTION (SYNTAX_ERROR, "duplication column name \"%s\" in view %s.%s",
									(const char*) columnNames [n],
									(const char*) schema,
									(const char*) name);
	nodeList = statement->nodeList;
	statement->nodeList = NULL;

	for (NNode *node = nodeList; node; node = node->nextNode)
		node->statement = NULL;

	delete [] contexts;
}

void View::gen(Stream * stream)
{
	int n;
	const char *sep = " (";

	for (n = 0; n < numberColumns; ++n)
		{
		stream->putSegment (sep);
		stream->putSegment (columnNames [n]);
		sep = ",";
		}

	stream->putSegment (") as\n  select ");

	if (distinct)
		stream->putSegment ("distinct ");

	for (n = 0; n < numberColumns; ++n)
		{
		if (n != 0)
			stream->putCharacter (',');
		columns->children [n]->gen (stream);
		}

	sep = "\n    from ";

	for (n = 0; n < numberTables; ++n)
		{
		if (schema == tables [n]->schemaName)
			stream->format ("%s%s ctx%d", sep, 
					 (const char*) tables [n]->name, n);
		else
			stream->format ("%s%s.%s ctx%d", sep, 
					 (const char*) tables [n]->schemaName,
					 (const char*) tables [n]->name, n);
		sep = ",";
		}
	
	if (predicate)
		{
		stream->putSegment ("\n    where ");
		predicate->gen (stream);
		}
}

void View::createFields(Table *table)
{
	for (int n = 0; n < numberColumns; ++n)
		{
		FieldType type = columns->children [n]->getType();
		int flags = 0;
		//Field *field = 
		table->addField (columnNames [n], type.type, type.length, type.precision, type.scale, flags);
		}

}

void View::save(Database *database)
{
	PreparedStatement *statement = database->prepareStatement (
		"insert into system.view_tables (viewName,viewSchema,sequence,tableName,schema) values (?,?,?,?,?)");
	statement->setString (1, name);
	statement->setString (2, schema);

	for (int n = 0; n < numberTables; ++n)
		{
		statement->setInt (3, n);
		statement->setString (4, tables [n]->name);
		statement->setString (5, tables [n]->schemaName);
		statement->executeUpdate();
		}

	statement->close();
}

void View::drop(Database *database)
{
	PreparedStatement *statement = database->prepareStatement (
		"delete from system.view_tables where viewName=? and viewSchema=?");
	statement->setString (1, name);
	statement->setString (2, schema);
	statement->executeUpdate();
	statement->close();
}

bool View::isEquiv(View *view)
{
	if (numberTables != view->numberTables ||
	    numberColumns != view->numberColumns ||
		distinct != view->distinct)
		return false;

	if (!equiv (predicate, view->predicate) ||
		!equiv (columns, view->columns) ||
	    !equiv (sort, view->sort))
		return false;


	int n;

	for (n = 0; n < numberTables; ++n)
		if (tables [n] != view->tables [n])
			return false;

	for (n = 0; n < numberColumns; ++n)
		if (columnNames [n] != view->columnNames [n])
			return false;

	return true;
}

bool View::equiv(NNode *node1, NNode *node2)
{
	if (node1)
		{
		if (!node2)
			return false;
		return node1->equiv (node2);
		}

	return node2 == NULL;
}
