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

// CompiledStatement.cpp: implementation of the CompiledStatement class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include "Engine.h"
#include "CompiledStatement.h"
#include "SQLParse.h"
#include "SQLError.h"
#include "Syntax.h"
#include "Connection.h"
#include "Database.h"
#include "NField.h"
#include "NReplace.h"
#include "NParameter.h"
#include "Table.h"
#include "Field.h"
#include "NSelect.h"
#include "NUpdate.h"
#include "NRepair.h"
#include "NConstant.h"
#include "NRecordNumber.h"
#include "NBitmap.h"
#include "NAlias.h"
#include "NStat.h"
#include "NInSelect.h"
#include "NSelectExpr.h"
#include "NExists.h"
#include "NSequence.h"
#include "NMatching.h"
#include "NCast.h"
#include "NConnectionVariable.h"
#include "Context.h"
#include "Value.h"
#include "Index.h"
#include "IndexKey.h"
#include "FsbInversion.h"
#include "FsbExhaustive.h"
#include "FsbSieve.h"
#include "NBitSet.h"
#include "Statement.h"
#include "View.h"
#include "Sequence.h"
#include "SequenceManager.h"
#include "Sync.h"
#include "FilterSet.h"
#include "FilterSetManager.h"
#include "TableFilter.h"
#include "Log.h"
#include "Interlock.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CompiledStatement::CompiledStatement(Connection *cnct)
{
	connection = cnct;
	database = connection->database;
	lastUse = database->timestamp;
	useCount = 1;
	useable = false;
	node = NULL;
	numberParameters = 0;
	numberValues = 0;
	numberContexts = 0;
	numberSlots = 0;
	numberSorts = 0;
	numberBitmaps = 0;
	numberValueSets = 0;
	numberRowSlots = 0;
	syntax = NULL;
	nodeList = NULL;
	firstInstance = lastInstance = NULL;
	select = NULL;
	parse = NULL;
}

CompiledStatement::~CompiledStatement()
{
	if (parse)
		delete parse;

	for (NNode *node; (node = nodeList);)
		{
		nodeList = node->nextNode;
		delete node;
		}
		
	FOR_OBJECTS (Context*, context, &contexts)
		delete context;
	END_FOR

	while (!filters.isEmpty())
		{
		TableFilter *filter = (TableFilter*) filters.pop();
		filter->release();
		}
}

void CompiledStatement::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void CompiledStatement::release()
{
	if (INTERLOCKED_DECREMENT (useCount) == 0)
		delete this;
}

NNode* CompiledStatement::compile(Syntax * syntax)
{
	if (syntax == NULL)
		return NULL;

	NType type = Nothing;
	ddl = false;

	switch (syntax->type)
		{
		case nod_alter_table:
		case nod_create_table:
		case nod_create_view:
		case nod_upgrade_view:
		case nod_drop_view:
		case nod_upgrade_table:
		case nod_create_index:
		case nod_upgrade_index:
		case nod_drop_index:
		case nod_drop_table:
		case nod_rename_table:
		case nod_reindex:
		case nod_set_namespace:
		case nod_push_namespace:
		case nod_pop_namespace:
		case nod_alter_user:
		case nod_create_user:
		case nod_create_role:
		case nod_upgrade_role:
		case nod_drop_user:
		case nod_drop_role:
		case nod_grant:
		case nod_revoke:
		case nod_revoke_role:
		case nod_assume:
		case nod_revert:
		case nod_grant_role:
		case nod_create_sequence:
		case nod_upgrade_sequence:
		case nod_drop_sequence:
		case nod_create_trigger:
		case nod_upgrade_trigger:
		case nod_create_repository:
		case nod_drop_repository:
		case nod_sync_repository:
		case nod_upgrade_repository:
		case nod_create_domain:
		case nod_upgrade_domain:
		case nod_upgrade_schema:
		case nod_alter_trigger:
		case nod_drop_trigger:
		case nod_upgrade_filterset:
		case nod_create_filterset:
		case nod_drop_filterset:
		case nod_enable_filterset:
		case nod_disable_filterset:
		case nod_enable_trigger_class:
		case nod_disable_trigger_class:
		case nod_create_coterie:
		case nod_upgrade_coterie:
		case nod_drop_coterie:
		case nod_create_tablespace:
		case nod_upgrade_tablespace:
		case nod_drop_tablespace:
			ddl = true;
			
			return NULL;

		case nod_and:		type = And; break;
		case nod_not:		type = Not; break;
		case nod_eql:		type = Eql; break;
		case nod_neq:		type = Neq; break;
		case nod_gtr:		type = Gtr; break;
		case nod_geq:		type = Geq; break;
		case nod_lss:		type = Lss; break;
		case nod_leq:		type = Leq; break;
		case nod_null: 		type = NNull; break;
		case nod_is_null:	type = IsNull; break;
		case nod_is_active_role:	type = IsActiveRole; break;
		case nod_not_null:	type = NotNull; break;
		case nod_like:		type = Like; break;
		case nod_starting:	type = Starting; break;
		case nod_containing:type = Containing; break;
		case nod_list:		type = List; break;
		case nod_assign:	type = Assign; break;

		case nod_case:		type = Case; break;
		case nod_case_search: type = CaseSearch; break;
		case nod_add:		type = Add; break;
		case nod_subtract:	type = Subtract; break;
		case nod_multiply:	type = Multiply; break;
		case nod_divide:	type = Divide; break;
		case nod_mod:		type = Mod; break;
		case nod_concat:	type = Concat; break;
		case nod_negate:	type = Negate; break;
		case nod_log_bool:	type = LogBoolean; break;

		case nod_or:
			{
			LinkedList adjuncts;
			decomposeAdjuncts (syntax, adjuncts);
			NNode *node = new NNode (this, Or, adjuncts.count());
			int n = 0;

			FOR_OBJECTS (Syntax*, child, &adjuncts)
				node->setChild (n++, compile (child));
			END_FOR;
			
			return node;
			}

		case nod_alias:
			return new NAlias (this, syntax);

		case nod_count:
			return new NStat (this, Count, syntax);

		case nod_min:
			return new NStat (this, Min, syntax);

		case nod_max:
			return new NStat (this, Max, syntax);

		case nod_avg:
			return new NStat (this, Avg, syntax);

		case nod_sum:
			return new NStat (this, Sum, syntax);

		case nod_identifier:
			return compileField (syntax);
		
		case nod_select:
			return compileSelect(syntax, NULL);
									
		case nod_update:
			return new NUpdate (this, syntax);
						
		case nod_delete:
			return new NDelete (this, syntax);
						
		case nod_repair:
			return new NRepair (this, syntax);
						
		case nod_insert:
			return new NInsert (this, syntax);
						
		case nod_replace:
			return new NReplace (this, syntax);
						
		case nod_quoted_string:
			return new NConstant (this, syntax->getString());

		case nod_number:
			{
			const char *p = syntax->getString();
			
			if (strlen (p) < 9)
				return new NConstant (this, atoi (syntax->getString()));
			}
		case nod_decimal_number:
			return new NConstant (this, syntax->getString());

		case nod_parameter:
			return new NParameter (this, numberParameters++);

		case nod_next_value:
			return new NSequence (this, syntax);

		case nod_in_select:
			return new NInSelect (this, syntax);
											
		case nod_exists:
			return new NExists (this, syntax);
											
		case nod_select_expr:
			return new NSelectExpr (this, syntax);
											
		case nod_descending:
		case nod_order:
			break;

		case nod_matching:
			return new NMatching (this, syntax);

		case nod_cast:
			return new NCast (this, syntax);

		case nod_function:
			return compileFunction(syntax);

		default:
			{
			syntax->prettyPrint ("CompiledStatement.compile");
			
			throw SQLEXCEPTION (FEATURE_NOT_YET_IMPLEMENTED, 
									"Operator %s is not yet implemented",
									syntax->getTypeString());
			}
		}

	NNode *node = new NNode (this, type, syntax->count);
	int n = 0;

	FOR_SYNTAX (child, syntax)
		node->setChild (n++, compile (child));
	END_FOR;

	node->fini();

	return node;
}


Field* CompiledStatement::findField(Table * table, Syntax * syntax)
{
	Field *field = NULL;
	const char *name = "-unknown-";

	switch (syntax->type)
		{
		case nod_name:
			name = syntax->getString();
			field = table->findField (name);
			break;
		
		default:
			break;
		}

	if (!field)
		throw SQLError (SYNTAX_ERROR, "can't find field %s in table %s", name, table->getName());

	return field;
}

int CompiledStatement::getValueSlot()
{
	return numberValues++;
}

Context* CompiledStatement::makeContext(Table * table, int32 privMask)
{
	Context *context = new Context (numberContexts++, table, privMask);
	contexts.append (context);
	View *view = table->view;

	if (view)
		{
		context->viewContexts = new Context* [view->numberTables];
		
		for (int n = 0; n < view->numberTables; ++n)
			context->viewContexts [n] = makeContext (view->tables [n], privMask);
		}

	return context;
}

Context* CompiledStatement::compileContext(Syntax * syntax, int32 privMask)
{
	if (syntax->type != nod_table)
		throw SQLError (INTERNAL_ERROR, "expected table node");

	Table *table = getTable (syntax->getChild (0));
	Context *context = makeContext (table, privMask);
	Syntax *alias = syntax->getChild (1);

	if (alias)
		context->alias = alias->getString();

	return context;
}

void CompiledStatement::pushContext(Context * context)
{
	context->setComputable (true);
	contextStack.push (context);
}

Context* CompiledStatement::popContext()
{
	Context *context = (Context*) contextStack.pop();
	context->setComputable (false);

	return context;
}

void* CompiledStatement::markContextStack()
{
	return contextStack.mark();
}

void CompiledStatement::popContexts(void * base)
{
	while (!contextStack.isMark (base))
		{
		if (contextStack.isEmpty())
			throw SQLError (INTERNAL_ERROR, "internal error: context stack corrupted");
			
		Context *context = (Context*) contextStack.pop();
		context->setComputable (false);
		}
}

NNode* CompiledStatement::compileField(Syntax * syntax)
{
	const char *name;
	const char *ctxName;

	switch (syntax->count)
		{
		case 1:
			name = syntax->getChild (0)->getString();
			
			FOR_STACK (Context*, context, &contextStack)
				if (!strcmp (name, "RECORD_NUMBER"))
					return new NRecordNumber (this, context);
					
				Field *field = context->table->findField (name);
				
				if (field)
					return compileField (field, context);
			END_FOR;
			
			if (select)
				{
				NNode *node = select->getValue (name);
				
				if (node)
					return node;
				}
				
			throw SQLEXCEPTION (SYNTAX_ERROR, "can't resolve field %s", 
									name);

		case 2:
			ctxName = syntax->getChild (0)->getString();
			name = syntax->getChild (1)->getString();
			
			FOR_STACK (Context*, context, &contextStack)
				if (context->isContextName (ctxName))
					{
					if (!strcmp (name, "RECORD_NUMBER"))
						return new NRecordNumber (this, context);
						
					Field *field = context->table->findField (name);
					
					if (field)
						return compileField (field, context);
					}
			END_FOR;
			
			if (strcmp (ctxName, "CONNECTION") == 0)
				return new NConnectionVariable (this, name);
				
			/***
			if (strcmp (ctxName, "SQL") == 0 &&
				strcmp (name, "SCHEMA") == 0)
				{
				FOR_STACK (Context*, context, &contextStack)
					if (context->table)
						return new NConstant (this, context->table->schemaName);
				END_FOR
				}
			***/
			//syntax->prettyPrint ("CompiledStatement.compileField");
			throw SQLEXCEPTION (SYNTAX_ERROR, "can't resolve field %s.%s", 
									ctxName, name); 

		default:
			throw SQLEXCEPTION (SYNTAX_ERROR, "compileField: too many field qualifiers");
		}

	return NULL;
}


Fsb* CompiledStatement::compileStream(Context * context, LinkedList & conjuncts, Row *row)
{
	context->setComputable (true);
	LinkedList booleans;
	Fsb *stream = NULL;
	Table *table = context->table;

	FOR_OBJECTS (NNode*, node, &conjuncts)
		if (node->computable (this))
			{
			booleans.append (node);
			conjuncts.deleteItem (node);
			}
	END_FOR;

	if (!booleans.isEmpty())
		{
		context->setComputable (false);
		NNode *inversion = NULL;

		FOR_OBJECTS (NNode*, node, &booleans)
			switch (node->type)
				{
				case Matching:
					if (isInvertible (context, node))
						inversion = mergeInversion (inversion, node);
						
					break;

				case Or:
					if (isInvertible (context, node))
						inversion = mergeInversion (inversion, genInversion (context, node, &booleans));
						
					break;

				case Eql:
					{
					NNode *ref = isDirectReference (context, node);
					
					if (ref)
						inversion = mergeInversion (inversion, new NBitSet (this, ref));
					}
					break;
				
				default:
					break;
				}
		END_FOR;

		LinkedList inversions;

		FOR_INDEXES (index, table)
			FOR_OBJECTS (NNode*, node, &booleans)
				NNode *invert = node->makeInversion (this, context, index);
				
				if (invert)
					inversion = mergeInversion (inversion, invert);
			END_FOR;
			
			NNode *invert = genInversion (context, index, booleans);
			
			if (invert)
				inversions.append (invert);
		END_FOR;

		NNode *uniqueKey = NULL;

		FOR_OBJECTS (NNode*, invert, &inversions)
			if (invert->isUniqueIndex())
				uniqueKey = invert;
		END_FOR;

		if (uniqueKey)
			inversion = mergeInversion (inversion, uniqueKey);
		else
			FOR_OBJECTS (NNode*, invert, &inversions)
				if (!invert->isRedundantIndex (&inversions))
					inversion = mergeInversion (inversion, invert);
			END_FOR;

		if (inversion)
			stream = new FsbInversion (context, inversion, row);

		context->setComputable (true);
		}

	if (!stream)
		stream = new FsbExhaustive (context, row);

	int count = booleans.count();

	if (count > 0)
		{
		NNode *boolean = false;

		if (count > 1)
			boolean = new NNode (this, And, count);

		int n = 0;

		FOR_OBJECTS (NNode*, child, &booleans)
			if (count == 1)
				boolean = child;
			else
				boolean->setChild (n++, child);
		END_FOR;

		stream = new FsbSieve (boolean, stream);
		}

	return stream;
}

bool CompiledStatement::contextComputable(int contextId)
{
	FOR_STACK (Context*, context, &contextStack)
		if (context->contextId == contextId)
			return context->computable;
	END_FOR;

	return false;
}

NNode* CompiledStatement::genInversion(Context * context, Index * index, LinkedList & booleans)
{
	int count = index->numberFields;
	NNode *min [MAX_KEY_SEGMENTS];
	memset (min, 0, sizeof (min));
	NNode *max [MAX_KEY_SEGMENTS];
	memset (max, 0, sizeof (max));

	FOR_OBJECTS (NNode*, node, &booleans)
		if (node->type != Starting)
			node->matchIndex (this, context, index, min, max);
	END_FOR;

	NBitmap *bitmap = NULL;

	if (min [0] || max [0])
		bitmap =  new NBitmap (this, index, count, min, max);

	return bitmap;
}

void CompiledStatement::compile(JString sqlStr)
{
	sqlString = sqlStr;
	parse = new SQLParse;
	syntax = parse->parse (sqlString, database->symbolManager);

	try
		{
		if ( (node = compile (syntax)) )
			{
			delete parse;
			parse = NULL;
			syntax = NULL;
			}

		useable = true;
		}
	catch (SQLException &exception)
		{
		throw SQLEXCEPTION (exception.getSqlcode(), "%s\nSQL: %s", 
							(const char*) exception.getText(), 
							(const char*) sqlString);
		}
}

int CompiledStatement::getGeneralSlot()
{
	return numberSlots++;
}

int CompiledStatement::getSortSlot()
{
	return numberSorts++;
}


int CompiledStatement::getBitmapSlot()
{
	return numberBitmaps++;
}

Table* CompiledStatement::getTable(Connection *connection, Syntax * syntax)
{
	Database *database = connection->database;

	if (syntax->count == 1)
		{
		Syntax *name = syntax->getChild (0);
		Table *table = connection->findTable (name->getString());
		
		if (table)
			return table;
			
		throw SQLEXCEPTION (NO_SUCH_TABLE, "can't find table \"%s\"",  name->getString());
		}

	Syntax *schema = syntax->getChild (0);
	Syntax *name = syntax->getChild (1);
	Table *table = database->findTable (schema->getString(), name->getString());

	if (!table)
		throw SQLEXCEPTION (NO_SUCH_TABLE, "can't find table \"%s.%s\"",  
							schema->getString(), name->getString());

	return table;
}

NNode* CompiledStatement::mergeInversion(NNode * node1, NNode * node2)
{
	if (!node1)
		return node2;

	if (!node2)
		return node1;

	NNode *temp = new NNode (this, BitmapAnd, 2);
	temp->children [0] = node1;
	temp->children [1] = node2;
	
	return temp;
}

bool CompiledStatement::references(Table * table)
{
	return node->references (table);
}


Table* CompiledStatement::getTable(Syntax * syntax)
{
	Table *table = getTable (connection, syntax);

	if (table && syntax->count == 1)
		noteUnqualifiedTable (table);

	return table;
}

Table* CompiledStatement::findTable(Syntax * syntax, bool search)
{
	Database *database = connection->database;

	if (syntax->count == 1)
		{
		const char *name = syntax->getChild (0)->getString();
		Table *table;
		
		if (search)
			table = connection->findTable (name);
		else
			table = database->findTable (connection->currentSchemaName(), name);
			
		if (table)
			noteUnqualifiedTable (table);
			
		return table;
		}

	Syntax *schema = syntax->getChild (0);
	Syntax *name = syntax->getChild (1);
	Table *table = database->findTable (schema->getString(), name->getString());

	return table;
}

void CompiledStatement::noteUnqualifiedTable(Table *table)
{
	FOR_STACK (Table*, tbl, &unqualifiedTables)
		if (tbl == table)
			return;
	END_FOR;

	unqualifiedTables.push (table);
}

bool CompiledStatement::validate(Connection * connection)
{
	// First, make sure all unqualified tables resolve correctly

	FOR_STACK (Table*, table, &unqualifiedTables)
		Table *tbl = connection->findTable (table->name);
		
		if (tbl != table)
			return false;
	END_FOR;

	// Next, make sure the same table filter have been applied

	Stack statementFilters;
	LinkedList *activeFilters = &connection->filterSets;

	FOR_OBJECTS (Context*, context, &contexts)
		if (context->select)
			FOR_OBJECTS (FilterSet*, filterSet, activeFilters)
				TableFilter *filter = filterSet->findFilter (context->table);
				
				if (filter)
					statementFilters.insertOrdered (filter);
			END_FOR;				
	END_FOR;

	if (!statementFilters.equal (&filters))
		return false;

	return true;
}


void CompiledStatement::addInstance(Statement * instance)
{
	Sync sync (&syncObject, "CompiledStatement::addInstance");
	sync.lock (Exclusive);
	instance->next = NULL;
	instance->prior = lastInstance;

	if (lastInstance)
		lastInstance->next = instance;
	else
		firstInstance = instance;

	lastInstance = instance;
}

void CompiledStatement::deleteInstance(Statement * instance)
{
	Sync sync (&syncObject, "CompiledStatement::deleteInstance");
	sync.lock (Exclusive);
	lastUse = database->timestamp;

	/***
	for (Statement **ptr = &instances; *ptr; ptr = &((*ptr)->next))
		if (*ptr == instance)
			{
			*ptr = instance->next;
			sync.unlock();
			release();
			return;
			}

	sync.unlock();
	ASSERT (false);
	***/

	if (instance->prior)
		instance->prior->next = instance->next;
	else
		{
		ASSERT (firstInstance == instance);
		firstInstance = instance->next;
		}

	if (instance->next)
		instance->next->prior = instance->prior;
	else
		{
		ASSERT (lastInstance == instance);
		lastInstance = instance->prior;
		}

	sync.unlock();
	release();
}

View* CompiledStatement::getView(const char * sqlStr)
{
	sqlString = sqlStr;
	parse = new SQLParse;
	syntax = parse->parse(sqlString, database->symbolManager);
	Syntax *node = syntax->getChild (0);
	const char *name = node->getChild (node->count - 1)->getString();
	const char *schema = node->getChild (0)->getString();
	View *view = new View (schema, name);
	view->compile (this, syntax);

	return view;
}

NNode* CompiledStatement::compileField(Field * field, Context * context)
{
	if (!context->viewContexts)
		return new NField (this, field, context);

	View *view = context->table->view;
	ASSERT (view);
	NNode *expr = view->columns->children [field->id];

	return expr->copy (this, context);
}


bool CompiledStatement::isInvertible(Context *context, NNode *node)
{
	// See if node thinks it's invertible

	if (node->isInvertible (this, context))
		return true;

	// If we match an index, we're also invertible

	FOR_INDEXES (index, context->table)
		NNode *min [MAX_KEY_SEGMENTS];
		memset (min, 0, sizeof (min));
		NNode *max [MAX_KEY_SEGMENTS];
		memset (max, 0, sizeof (max));
		node->matchIndex (this, context, index, min, max);
		
		if (min [0] || max [0])
			return true;
	END_FOR;

	return false;	
}

NNode* CompiledStatement::genInversion(Context *context, NNode *node, LinkedList *booleans)
{
	// If this is a direct reference to a record number, we're invertible

	NNode *ref = isDirectReference (context, node);

	if (ref)
		return new NBitSet (this, ref);

	// Matching is always invertable

	if (node->type == Matching)
		return node;

	// If this is starting, look for an index

	if (node->type == Starting || node->type == InSelect)
		{
		FOR_INDEXES (index, context->table)
			NNode *inversion = node->makeInversion (this, context, index);
			
			if (inversion)
				return inversion;
				
		END_FOR;
		
		return NULL;
		}

	// If this is a adjust and both branches are invertible, we're invertible

	if (node->type == Or)
		{
		NNode *temp = new NNode (this, BitmapOr, node->count);
		
		for (int n = 0; n < node->count; ++n)
			temp->children [n] = genInversion (context, node->children [n], booleans);
			
		return temp;
		}

	// If we match an index, we're also invertible

	FOR_INDEXES (index, context->table)
		int count = index->numberFields;
		NNode *min [MAX_KEY_SEGMENTS];
		memset (min, 0, sizeof (min));
		NNode *max [MAX_KEY_SEGMENTS];
		memset (max, 0, sizeof (max));
		node->matchIndex (this, context, index, min, max);
		
		if (min [0] || max [0])
			{
			if (booleans)
				FOR_OBJECTS (NNode*, expr, booleans)
					expr->matchIndex (this, context, index, min, max);
				END_FOR;
				
			return new NBitmap (this, index, count, min, max);
			}
	END_FOR;

	return NULL;	
}

NNode* CompiledStatement::isDirectReference(Context *context, NNode *node)
{
	if (node->type != Eql)
		return NULL;

	NNode *child1 = node->children [0];
	NNode *child2 = node->children [1];

	if (child2->type == RecordNumber)
		{
		child1 = child2;
		child2 = node->children [0];
		}

	if ((child1->type == RecordNumber &&
		((NRecordNumber*) child1)->contextId == context->contextId &&
		child2->computable (this)))
		return child2;

	return NULL;
}

Sequence* CompiledStatement::findSequence(Syntax *syntax, bool search)
{
	Database *database = connection->database;
	SequenceManager *manager = database->sequenceManager;

	if (syntax->count == 1)
		{
		const char *name = syntax->getChild (0)->getString();
		Sequence *sequence;
		
		if (search)
			sequence = connection->findSequence (name);
		else
			sequence = manager->findSequence (connection->currentSchemaName(), name);
			
		if (sequence)
			{
			FOR_STACK (Sequence*, seq, &unqualifiedTables)
				if (seq == sequence)
					return sequence;
			END_FOR;
			
			unqualifiedSequences.push (sequence);
			}
		return sequence;
		}

	Syntax *schema = syntax->getChild (0);
	Syntax *name = syntax->getChild (1);

	return manager->findSequence (schema->getString(), name->getString());
}

void CompiledStatement::checkAccess(Connection *connection)
{
	FOR_OBJECTS (Context*, context, &contexts)
		if (!connection->checkAccess (context->privilegeMask, context->table))
			{
			Log::debug ("requested access to %s.%s is denied\n", 
							(const char*) context->table->schemaName,			
							(const char*) context->table->name);
			connection->checkAccess (context->privilegeMask, context->table);
			
			throw SQLError (SECURITY_ERROR, "requested access to %s.%s is denied", 
							(const char*) context->table->schemaName,			
							(const char*) context->table->name);
			}			
	END_FOR
}

FilterSet* CompiledStatement::findFilterset(Syntax *syntax)
{
	Database *database = connection->database;
	FilterSetManager *manager = database->filterSetManager;

	if (syntax->count == 1)
		{
		const char *name = syntax->getChild (0)->getString();
		const char *schema = connection->currentSchemaName();
		return manager->findFilterSet (schema, name);
		}

	const char *schema = syntax->getChild (0)->getString();
	const char *name = syntax->getChild (1)->getString();

	return manager->findFilterSet (schema, name);
}

bool CompiledStatement::addFilter(TableFilter *filter)
{
	if (filters.insertOrdered (filter))
		{
		filter->addRef();
		return true;
		}

	return false;
}

int CompiledStatement::countInstances()
{
	Sync sync (&syncObject, "CompiledStatement::addInstance");
	sync.lock (Shared);
	int count = 0;

	for (Statement *instance = firstInstance; instance; instance = instance->next)
		++count;

	return count;
}

void CompiledStatement::decomposeAdjuncts(Syntax *syntax, LinkedList &adjuncts)
{
	if (syntax->type == nod_or)
		FOR_SYNTAX (child, syntax)
			decomposeAdjuncts (child, adjuncts);
		END_FOR
	else
		adjuncts.append (syntax);
}

Context* CompiledStatement::getContext(int contextId)
{
	FOR_OBJECTS (Context*, context, &contexts)
		if (context->contextId == contextId)
			return context;
	END_FOR;

	throw SQLEXCEPTION (COMPILE_ERROR, "can't find internal context");
}

Type CompiledStatement::getType(Syntax *syntax)
{
	switch (syntax->type)
		{
		case nod_varchar:			return Varchar;
		case nod_char:				return Char;
		case nod_integer:			return Long;
		case nod_tinyint:
		case nod_smallint:			return Short;
		case nod_bigint:			return Quad;
		case nod_text:				return Asciiblob;
		case nod_blob:				return Binaryblob;
		case nod_float:				return Float;
		case nod_double:			return Double;
		case nod_date:				return Date;
		case nod_timestamp:			return Timestamp;
		case nod_time:				return TimeType;
		case nod_numeric:
			{
			int precision = syntax->getChild(0)->getNumber();
			
			if (precision < 5)
				return Short;
			else if (precision < 10)
				return Long;
			else
				return Quad;
			}
			break;

		default:
			break;
		}

	syntax->prettyPrint ("Type not understood");
	throw SQLError (DDL_ERROR, "Statement::createTable: type not understood");
}

void CompiledStatement::invalidate()
{
	Sync sync (&syncObject, "CompiledStatement::addInstance");
	sync.lock (Shared);

	for (Statement *instance = firstInstance; instance; instance = instance->next)
		instance->invalidate();
}

NNode* CompiledStatement::compileSelect(Syntax *syntax, NNode *inExpr)
{
	return new NSelect (this, syntax, inExpr);
}

int CompiledStatement::getValueSetSlot()
{
	return numberValueSets++;
}

int CompiledStatement::getRowSlot()
{
	return numberRowSlots++;
}

void CompiledStatement::renameTables(Syntax *syntax)
{

}


NNode* CompiledStatement::compileFunction(Syntax *syntax)
{
	Syntax *qualifiedName = syntax->getChild(0);
	Syntax *arguments = syntax->getChild(1);
	NNode *node = NULL;
	const char *name;
	const char *qualifier = NULL;
	int reqArgs;

	switch (qualifiedName->count)
		{
		case 1:
			name = qualifiedName->getChild(0)->getString();
			break;

		case 2:
			qualifier = qualifiedName->getChild(0)->getString();
			name = qualifiedName->getChild(1)->getString();
			break;

		default:
			throw SQLEXCEPTION (SYNTAX_ERROR, "too many function qualifiers");
		}

	if (strcasecmp(name, "BlobRef") == 0 && (qualifier == NULL || strcasecmp(qualifier, "SYSTEM") == 0))
		{
		reqArgs = 1;
		node = new NNode (this, BlobRef, arguments->count);
		}
	else if (strcasecmp(name, "Upper") == 0 && (qualifier == NULL || strcasecmp(qualifier, "SYSTEM") == 0))
		{
		reqArgs = 1;
		node = new NNode (this, Upper, arguments->count);
		}
	else if (strcasecmp(name, "Lower") == 0 && (qualifier == NULL || strcasecmp(qualifier, "SYSTEM") == 0))
		{
		reqArgs = 1;
		node = new NNode (this, Lower, arguments->count);
		}
	else
		throw SQLError(SYNTAX_ERROR, "function \"%s\" is undefined", (const char*) getNameString(qualifiedName));

	if (arguments->count != reqArgs)
		throw SQLError(SYNTAX_ERROR, "wrong number of arguments for function \"%s\"", (const char*) getNameString(qualifiedName));

	int n = 0;

	FOR_SYNTAX (child, arguments)
		node->setChild (n++, compile (child));
	END_FOR;

	node->fini();

	return node;
}

JString CompiledStatement::getNameString(Syntax *syntax)
{
	char temp [512];
	char *p = temp;

	for (int n = 0; n < syntax->count; ++n)
		{
		if (n)
			*p++ = '.';

		for (const char *q = syntax->getChild(n)->getString(); *q;)
			*p++ = *q++;
		}

	*p = 0;

	return JString(temp, (int) (p - temp));
}
