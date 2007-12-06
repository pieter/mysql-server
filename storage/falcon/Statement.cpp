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

// Statement.cpp: implementation of the Statement class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "Engine.h"
#include "PStatement.h"
#include "Syntax.h"
#include "Table.h"
#include "Field.h"
#include "Database.h"
#include "Index.h"
#include "CompiledStatement.h"
#include "Value.h"
#include "Connection.h"
#include "Context.h"
#include "NSelect.h"
#include "SQLError.h"
#include "RSet.h"
#include "Record.h"
#include "DateTime.h"
#include "ResultList.h"
#include "ForeignKey.h"
#include "Sort.h"
#include "DataResourceLocator.h"
#include "Bitmap.h"
#include "User.h"
#include "RoleModel.h"
#include "Privilege.h"
#include "View.h"
#include "SequenceManager.h"
#include "Sequence.h"
#include "Transaction.h"
#include "Sync.h"
#include "FilterSet.h"
#include "FilterSetManager.h"
#include "CollationManager.h"
#include "TableSpaceManager.h"
#include "Log.h"
#include "Stream.h"
#include "PrettyPrint.h"
#include "Interlock.h"
#include "Repository.h"
#include "Schema.h"
#include "ValueSet.h"
#include "TableSpace.h"

#ifndef STORAGE_ENGINE
#include "Trigger.h"
#include "Coterie.h"
#include "CoterieRange.h"
#endif

/***
const static char *specialString =
"select *\n"
"  from group_members gm, groups g\n"
"  where gm.user_id = ? and\n"
"        gm.group_id = g.group_id and\n"
"        g.abbreviation = ?";
***/

static const char THIS_FILE[]=__FILE__;

static const int AnalyzeCounts	= 1;
static const int AnalyzeTree	= 2;

struct SyntaxPrivType {
	SyntaxType	type;
    int32		privilege;
	} privilegeTypes [] = {
		nod_priv_select,	PRIV_MASK (PrivSelect),
		nod_priv_insert,	PRIV_MASK (PrivInsert),
		nod_priv_update,	PRIV_MASK (PrivUpdate),
		nod_priv_delete,	PRIV_MASK (PrivDelete),
		nod_priv_grant,		PRIV_MASK (PrivGrant),
		nod_priv_execute,	PRIV_MASK (PrivExecute),
		nod_priv_alter,		PRIV_MASK (PrivAlter),
		nod_priv_all,		PRIV_MASK (PrivSelect)|
							PRIV_MASK (PrivInsert)|
							PRIV_MASK (PrivUpdate)|
							PRIV_MASK (PrivDelete)|
							PRIV_MASK (PrivExecute)|
							//PRIV_MASK (PrivGrant)|
							PRIV_MASK (PrivAlter),
		nod_not, 0
		};

const static char *privObjectTypes [] = {
	"Table",
	"View",
	"Procedure",
	"User",
	"Role",
	"Coterie",
	};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Statement::Statement(Connection *pConnection, Database *db)
{
	database = db;
	connection = pConnection;
	handle = ++connection->nextHandle;
	statement = NULL;
	contexts = NULL;
	numberContexts = 0;
	numberSorts = 0;
	numberBitmaps = 0;
	numberValueSets = 0;
	numberRowSlots = 0;
	slots = NULL;
	sorts = NULL;
	bitmaps = NULL;
	valueSets = NULL;
	rowSlots = NULL;
	parent = NULL;
	tableFilter = NULL;
	useCount = 1;
	javaCount = 0;
	resultSets = NULL;
	resultLists = NULL;
	transaction = NULL;
	cursorName = NULL;
	special = false;
	active = false;
	memset (&stats, 0, sizeof (stats));
}

Statement::~Statement()
{
	/***
	if (special)
		printf ("%8x deleted\n", this);
	***/
	
	reset();
}

bool Statement::execute(const char * sqlString)
{
	return execute (sqlString, false);
}

void Statement::createTable(Syntax * syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName(node);
	const char *schema = getSchemaName(node);
	TableSpace *tableSpace = NULL;
	Syntax *tableSpaceName = syntax->getChild(2);

	if (tableSpaceName)
		tableSpace = database->tableSpaceManager->getTableSpace(tableSpaceName->getString());
	
	Table *table = database->addTable (connection->user, name, schema, tableSpace);

	try
		{
		Syntax *list = syntax->getChild(1);
		LinkedList foreignKeys;

		FOR_SYNTAX (item, list)
			switch (item->type)
				{
				case nod_field:
					addField (table, item);
					break;

				case nod_primary_key:
					createPrimaryKey (table, item);
					break;

				case nod_foreign_key:
					foreignKeys.append (addForeignKey (table, NULL, item));
					break;

				default:
					item->prettyPrint ("node not done");
					NOT_YET_IMPLEMENTED;
				}
		END_FOR;

		FOR_FIELDS (field, table)
			ForeignKey *key;
			if ( (key = field->foreignKey) )
				key->create();
		END_FOR;

		FOR_OBJECTS (ForeignKey*, key, &foreignKeys)
			key->create();
		END_FOR;
		}
	catch (...)
		{
		database->dropTable (table, transaction);
		//delete table;
		throw;
		}

	table->reformat();

	if (!database->formatting)
		{
		Transaction *sysTransaction = database->getSystemTransaction();
		table->create ((connection == database->systemConnection) ? "SYSTEM TABLE" : "TABLE", sysTransaction);
		table->save();
		database->roleModel->addUserPrivilege (connection->user, table, ALL_PRIVILEGES);
		database->commitSystemTransaction();
		}

}

void Statement::createIndex(Syntax * syntax, bool upgrade)
{
	int indexType = SecondaryIndex;

	if (syntax->getChild(0))
		indexType = UniqueIndex;

	const char *name = syntax->getChild(1)->getString();
	Table *table = statement->getTable (syntax->getChild(2));

	// Make sure index doesn't exist on a different table or this table is creation

	Index *oldIndex = NULL;
	Syntax *list = syntax->getChild(3);

	if (!database->formatting)
		{
		Sync sync (&database->syncSysConnection, "Statement::createIndex");
		sync.lock (Shared);

		PreparedStatement *check = database->prepareStatement (
			"select tableName from system.indexes where schema=? and indexName=?");
		check->setString (1, table->schemaName);
		check->setString (2, name);
		ResultSet *resultSet = check->executeQuery();

		if (resultSet->next())
			{
			const char *tableName = resultSet->getSymbol (1);
			
			if (!upgrade || table->name != tableName)
				{
				resultSet->close();
				check->close();
				
				throw SQLError (DDL_ERROR, "index %s already exists on table %s.%s",
								name, table->schemaName, tableName);
				}
				
			oldIndex = table->findIndex (name);
			
			if (oldIndex && oldIndex->numberFields == list->count && oldIndex->type == indexType)
				{
				bool identical = true;
				
				for (int n = 0; n < oldIndex->numberFields; ++n)
					{
					Syntax *segment = list->getChild(n);
					Syntax *partial = segment->getChild(1);
					int partialLength = (partial) ? partial->getNumber() : 0;
					Field *field = statement->findField (table, segment->getChild(0));
					
					if (oldIndex->fields[n] != field || partialLength != oldIndex->getPartialLength(n))
						{
						identical = false;
						break;
						}
					}
					
				if (identical)
					{
					resultSet->close();
					check->close();
					
					return;
					}
				}
			}

		resultSet->close();
		check->close();
		}

	Index *index = table->addIndex (name, list->count, indexType);
	int n = 0;

	FOR_SYNTAX (segment, list)
		Syntax *fld = segment->getChild(0);
		Field *field = statement->findField(table, fld);
		
		if (!field)
			{
			table->deleteIndex (index, transaction);
			throw SQLError (DDL_ERROR, "can't find field %s in table %s",
							fld->getString(), table->getName());
			return;
			}
			
		Syntax *partial = segment->getChild(1);
		
		if (partial)
			index->setPartialLength(n,partial->getNumber());
			
		index->addField (field, n++);
	END_FOR;

	index->checkMaxKeyLength();		// This will throw an exception if too large.

	if (oldIndex)
		{
		Index::deleteIndex(database, table->schemaName, name);
		table->deleteIndex(oldIndex, transaction);
		}

	if (!database->formatting)
		{
		Transaction *sysTransaction = database->getSystemTransaction();
		index->create(sysTransaction);
		index->save();
		database->commitSystemTransaction();
		sysTransaction  = database->getSystemTransaction();
		table->populateIndex (index, sysTransaction);
		database->commitSystemTransaction();
		database->invalidateCompiledStatements (table);
		//database->flush();
		}
}


void Statement::allocParameters(int count)
{	
	parameters.alloc (count);
}

Value* Statement::getParameter(int index)
{
	if (index < 0 || index >= parameters.count)
		throw SQLEXCEPTION (RUNTIME_ERROR, "invalid parameter index");

	return parameters.values + index;
}

void Statement::prepareStatement()
{
	/***
	if (special)
		printf ("%8x prepared\n", this);
	***/

	recordsUpdated = 0;
	
	if (transaction)
		transaction->release();
		
	transaction = connection->getTransaction();
	transaction->addRef();

	clearParameters();

	if (statement->numberValues)
		values.alloc (statement->numberValues);

	int n;

	if (sorts)
		for (n = 0; n < numberSorts; ++n)
			{
			delete sorts[n];
			sorts[n] = NULL;
			}

	if ((numberSorts != statement->numberSorts) && sorts)
		{
		delete [] sorts;
		sorts = NULL;
		}

	if ( (numberSorts = statement->numberSorts) )
		{
		if (!sorts)
			sorts = new Sort* [numberSorts];
			
		memset (sorts, 0, sizeof (Sort*) * numberSorts);
		}

	for (n = 0; n < numberBitmaps; ++n)
		if (!bitmaps [n])
			bitmaps [n]->release();

	if (numberBitmaps != statement->numberBitmaps && bitmaps)
		{
		delete [] bitmaps;
		bitmaps = NULL;
		}

	if ( (numberBitmaps = statement->numberBitmaps) )
		{
		if (!bitmaps)
			bitmaps = new Bitmap* [numberBitmaps];
			
		memset (bitmaps, 0, sizeof (Bitmap*) * numberBitmaps);
		}

	if ( (numberValueSets = statement->numberValueSets) )
		{
		if (!valueSets)
			valueSets = new ValueSet* [numberValueSets];
			
		memset (valueSets, 0, sizeof (ValueSet*) * numberValueSets);
		}

	if ( (numberRowSlots = statement->numberRowSlots) )
		{
		if (!rowSlots)
			rowSlots = new Row* [numberRowSlots];
			
		memset (rowSlots, 0, sizeof (Row*) * numberRowSlots);
		}

	if (statement->numberSlots && !slots)
		slots = new int32 [statement->numberSlots];

	if (!contexts && (numberContexts = statement->numberContexts))
		{
		contexts = new Context [numberContexts];
		
		FOR_OBJECTS (Context*, context, &statement->contexts)
			contexts [context->contextId].initialize (context);
		END_FOR;
		}
}

ResultSet* Statement::executeQuery()
{
	return NULL;
}

void Statement::start(NNode * node)
{
	++database->numberQueries;
	currentResultSet = 0;
	recordsUpdated = 0;
	updateStatements = false;
	memset (&stats, 0, sizeof (stats));
	
	if (transaction)
		transaction->release();
		
	transaction = connection->getTransaction();
	transaction->addRef();
	int savePoint = transaction->createSavepoint();

	try
		{
		active = true;
		node->evalStatement (this);
		}
	catch (SQLException& exception)
		{
		Log::debug ("Verb rollback: %s\n", exception.getText());
		transaction->rollbackSavepoint(savePoint);
		active = false;
		throw;
		}

	if (transaction)
		transaction->releaseSavepoint(savePoint);
}

Context* Statement::getContext(int contextId)
{
	ASSERT (contextId >= 0 && contextId < numberContexts);

	return contexts + contextId;
}

Value* Statement::getValue(int slot)
{
	ASSERT (slot >= 0 && slot < values.count);

	return values.values + slot;
}

void Statement::close()
{
	ASSERT (javaCount == 0);

	if (connection)
		clearConnection();

	Sync sync (&syncObject, "Statement::close");
	sync.lock (Exclusive);

	if (resultSets)
		clearResults (true);

	reset();

	sync.unlock();
	delete this;
}


ResultSet* Statement::createResultSet(NSelect * node, int numberColumns)
{
	ResultSet *resultSet = new ResultSet (this, node, numberColumns);
	resultSet->sibling = NULL;
    ResultSet **ptr;

	for (ptr = &resultSets; *ptr; ptr = &(*ptr)->sibling)
		;

	*ptr = resultSet;

	return resultSet;
}

void Statement::deleteResultSet(ResultSet *resultSet)
{
	Sync sync (&syncObject, "Statement::deleteResultSet");
	sync.lock (Exclusive);

	for (ResultSet **ptr = &resultSets; *ptr; ptr = &((*ptr)->sibling))
		if (*ptr == resultSet)
			{
			*ptr = resultSet->sibling;
			--currentResultSet;
			return;
			}

	Log::log ("Statement::deleteResultSet: apparent misplaced resultSet\n");
}

void Statement::clearParameters()
{
	if (statement->numberParameters)
		parameters.alloc (statement->numberParameters);
}

int Statement::getParameterCount()
{
	return statement->numberParameters;
}


ResultList* Statement::search(const char * string)
{
	ResultList *resultList = new ResultList (this);
	database->search(resultList, string);
	resultList->sibling = resultLists;
	resultLists = resultList;

	return resultList;
}

void Statement::alterTable(Syntax * syntax)
{
	Table *table = CompiledStatement::getTable (connection, syntax->getChild(0));
	checkAlterPriv (table);
	Syntax *clauses = syntax->getChild(1);

	FOR_SYNTAX (clause, clauses)
		switch (clause->type)
			{
			case nod_alter_field:
				alterField (table, clause);
				break;

			case nod_primary_key:
				{
				Index *index = createPrimaryKey (table, clause);
				index->create(transaction);
				index->save ();
				table->populateIndex (index, transaction);
				}
				break;

			case nod_foreign_key:
				{
				ForeignKey *key = addForeignKey (table, NULL, clause);
				
				if (table->findForeignKey (key))
					{
					delete key;
					throw SQLEXCEPTION (DDL_ERROR, "foreign key already exits");
					}
					
				key->create();
				key->save (database);
				}
				break;

			case nod_add_field:
				{
				Field *field = addField (table, clause->getChild(0));
				
				if (field->foreignKey)
					{
					field->foreignKey->create();
					field->foreignKey->save (database);
					}
					
				field->save ();
				table->reformat();
				}
				break;

			case nod_drop_field:
				{
				Field *field = CompiledStatement::findField (table, clause->getChild(0));
				table->dropField (field);
				table->reformat();
				}
				break;

			case nod_drop_foreign_key:
				{
				Field *fields [16];
				Syntax *list = clause->getChild(0);
				
				if (list->count >= 16)
					throw SQLError (DDL_ERROR, "too many segments in a foreign key");
					
				for (int n = 0; n < list->count; ++n)
					fields [n] = CompiledStatement::findField (table, list->getChild(n));
					
				Table *refs = NULL;
				
				if (clause->getChild(1))
					refs = CompiledStatement::getTable (connection, clause->getChild(1));
					
				if (!table->dropForeignKey (list->count, fields, refs))
					throw SQLError (DDL_ERROR, "can't find foreign key");
				}
				break;

			default:
				NOT_YET_IMPLEMENTED;
			}

		database->commitSystemTransaction();
	END_FOR;


}

void Statement::alterField(Table * table, Syntax * syntax)
{
	Field *field = CompiledStatement::findField (table, syntax->getChild(0));
	Syntax *clauses = syntax->getChild(2);

	FOR_SYNTAX (clause, clauses)
		switch (clause->type)
			{
			case nod_searchable:
				field->makeSearchable(transaction, true);
				break;

			case nod_not_searchable:
				field->makeNotSearchable(transaction, true);
				break;

			default:
				NOT_YET_IMPLEMENTED;
			}
	END_FOR;
}

ResultSet* Statement::getResultSet()
{
	int n = 0;
	ResultSet *resultSet;

	for (resultSet = resultSets; resultSet && n < currentResultSet;
		 ++n, resultSet = resultSet->sibling)
		;

	++currentResultSet;

	return resultSet;
}

void Statement::executeDDL()
{
	//Log::debug ("executeDDL: %s\n", (const char*) statement->sqlString);
	Syntax *syntax = statement->syntax;
	ASSERT (syntax);
	Syntax *child;

	switch (syntax->type)
		{
		case nod_create_table:
			createTable (syntax);
			break;

		case nod_create_view:
			createView (syntax, false);
			break;

		case nod_upgrade_view:
			createView (syntax, true);
			break;

		case nod_upgrade_table:
			upgradeTable (syntax);
			break;

		case nod_create_sequence:
			createSequence (syntax, false);
			break;

		case nod_upgrade_sequence:
			createSequence (syntax, true);
			break;

		case nod_drop_sequence:
			dropSequence (syntax);
			break;

		case nod_create_coterie:
			createCoterie (syntax, false);
			break;

		case nod_upgrade_coterie:
			createCoterie (syntax, true);
			break;

		case nod_drop_coterie:
			dropCoterie (syntax);
			break;

		case nod_create_index:
			createIndex (syntax, false);
			break;

		case nod_upgrade_index:
			createIndex (syntax, true);
			break;

		case nod_drop_index:
			dropIndex (syntax);
			break;

		case nod_alter_table:
			alterTable (syntax);
			break;

		case nod_create_trigger:
			createTrigger (syntax, false);
			break;

		case nod_upgrade_trigger:
			createTrigger (syntax, true);
			break;

		case nod_create_repository:
			createRepository (syntax, false);
			break;

		case nod_drop_repository:
			dropRepository (syntax);
			break;

		case nod_upgrade_repository:
			createRepository (syntax, true);
			break;

		case nod_sync_repository:
			syncRepository (syntax);
			break;

		case nod_create_domain:
			createDomain (syntax, false);
			break;

		case nod_upgrade_domain:
			createDomain (syntax, true);
			break;

		case nod_upgrade_schema:
			upgradeSchema (syntax, true);
			break;

		case nod_alter_trigger:
			alterTrigger (syntax);
			break;

		case nod_drop_trigger:
			dropTrigger (syntax);
			break;

		case nod_reindex:
			//database->reindex(transaction);
			reIndex (syntax);
			break;

		case nod_drop_table:
			dropTable (syntax);
			break;

		case nod_rename_table:
			renameTables (syntax);
			break;

		case nod_drop_view:
			dropView (syntax);
			break;

		case nod_create_tablespace:
		case nod_upgrade_tablespace:
			createTableSpace(syntax);
			break;

		case nod_drop_tablespace:
			dropTableSpace(syntax);
			break;
			
		case nod_push_namespace:
			child = syntax->getChild(0);
			connection->pushSchemaName (child->getString());
			break;

		case nod_set_namespace:
			while (!connection->nameSpace.isEmpty())
				connection->popSchemaName();
				
			child = syntax->getChild(0);
			
			FOR_SYNTAX (name, child)
				connection->pushSchemaName (name->getString());
			END_FOR;
			
			break;

		case nod_pop_namespace:
			connection->popSchemaName();
			break;

		case nod_create_user:
			createUser (syntax);
			break;

		case nod_alter_user:
			alterUser (syntax);
			break;

		case nod_drop_user:
			dropUser (syntax);
			break;

		case nod_grant:
			grantPriv (syntax);
			break;

		case nod_revoke:
			revokePriv (syntax);
			break;

		case nod_grant_role:
			grantRole (syntax);
			break;

		case nod_revoke_role:
			revokeRole (syntax);
			break;

		case nod_create_role:
			createRole (syntax, false);
			break;

		case nod_upgrade_role:
			createRole (syntax, true);
			break;

		case nod_drop_role:
			dropRole (syntax);
			break;

		case nod_create_filterset:
			createFilterSet (syntax, false);
			break;

		case nod_upgrade_filterset:
			createFilterSet (syntax, true);
			break;

		case nod_drop_filterset:
			dropFilterSet (syntax);
			break;

		case nod_enable_filterset:
			enableFilterSet (syntax);
			break;

		case nod_disable_filterset:
			disableFilterSet (syntax);
			break;

		case nod_enable_trigger_class:
			enableTriggerClass (syntax);
			break;

		case nod_disable_trigger_class:
			disableTriggerClass (syntax);
			break;

		default:
			syntax->prettyPrint ("Statement::execute");
			NOT_YET_IMPLEMENTED;
		}
}

Field* Statement::addField(Table * table, Syntax * syntax)
{
	const char *fldName = syntax->getChild(0)->getString();

	if (table->findField (fldName))
		throw SQLEXCEPTION (DDL_ERROR, "field %s already exists in table %s.%s",
								fldName,
								(const char*) table->schemaName,
								(const char*) table->name);

	FieldType type;
	int flags = 0;
	getFieldType (syntax->getChild(1), &type);

	Syntax *clauses = syntax->getChild(2);
	Field *field = table->addField (fldName, type.type, type.length, type.precision, type.scale, flags);

	if (clauses)
		FOR_SYNTAX (clause, clauses)
			switch (clause->type)
				{
				case nod_searchable:
					field->makeSearchable(transaction, false);
					break;

				case nod_not_searchable:
					field->makeNotSearchable(transaction, false);
					break;

				case nod_primary_key:
					createPrimaryKey (table, field);
					break;

				case nod_foreign_key:
					field->foreignKey = addForeignKey (table, field, clause);
					break;

				case nod_not_null:
					field->flags |= NOT_NULL;
					break;

				case nod_collation:
					{
					const char *collationName = clause->getChild(0)->getString();
					field->setCollation (CollationManager::getCollation (collationName));
					}
					break;

				case nod_repository:
					{
					if (field->type != Asciiblob && field->type != Binaryblob)
						throw SQLEXCEPTION (DDL_ERROR, "can't specify repository on non-blob field %s\n", field->name);
						
					const char *repositoryName = clause->getChild(0)->getString();
					Repository *repository = database->getRepository (table->schemaName, repositoryName);
					field->setRepository (repository);
					}
					break;

				default:
					NOT_YET_IMPLEMENTED;
				}
		END_FOR;


	return field;
}

ForeignKey* Statement::addForeignKey(Table * foreignTable, Field * column, Syntax * syntax)
{
	LinkedList foreignFields;
	Table *primaryTable = statement->getTable (connection, syntax->getChild(1));
	Index *primaryIndex = primaryTable->getPrimaryKey();

	if (!primaryIndex)
		throw SQLEXCEPTION (DDL_ERROR, "no primary index for table %s", primaryTable->getName());

	int numberFields = primaryIndex->numberFields;
	int numberColumns = (column) ? 1 : syntax->getChild(0)->count;

	if (numberFields != numberColumns)
			throw SQLEXCEPTION (DDL_ERROR, "primary/foreign key mismatch");
		
	ForeignKey *key = new ForeignKey (numberFields, primaryTable, foreignTable);

	for (int n = 0; n < numberFields; ++n)
		key->primaryFields [n] = primaryIndex->fields [n];

	if (column)
		key->foreignFields [0] = column;
	else
		{
		Syntax *list = syntax->getChild(0);
		
		for (int n = 0; n < list->count; ++n)
			{
			Field *field = statement->findField (foreignTable, list->getChild(n));
			key->foreignFields [n] = field;
			}
		}

	Syntax *node = syntax->getChild(3);

	if (node)
		FOR_SYNTAX (option, node)
			switch (option->type)
				{
				case nod_cascade_delete:
					key->setDeleteRule (importedKeyCascade);
					break;

				default:
					NOT_YET_IMPLEMENTED;
				}
		END_FOR;

	return key;
}

void Statement::deleteResultList(ResultList * resultList)
{
	for (ResultList **ptr = &resultLists; *ptr; ptr = &(*ptr)->sibling)
		if (*ptr == resultList)
			{
			resultLists = resultList->sibling;
			
			return;
			}

	ASSERT (false);
}

int Statement::executeUpdate(const char * sqlString)
{
	recordsUpdated = 0;
	execute (sqlString);

	return recordsUpdated;
}

ResultSet* Statement::executeQuery(const char * sqlString)
{
	reset();
	execute (sqlString, true);

	return getResultSet();
}

void Statement::deleteSort(int slot)
{
	if (sorts [slot])
		{
		delete sorts [slot];
		sorts [slot] = NULL;
		}
}


void Statement::setCursorName(const char * name)
{
	const char *symbol = database->getSymbol (name);

	Statement *prior = connection->findStatement (symbol);

	if (prior)
		{
		Log::debug ("setCursorName prior Statement: %s\n", 
					(const char*) prior->statement->sqlString);
		Log::debug ("setCursorName current Statement: %s\n", 
					(const char*) statement->sqlString);
					
		throw SQLError (RUNTIME_ERROR, "cursor name \"%s\" is already in use", name);
		}

	cursorName = symbol;
}

Context* Statement::getUpdateContext()
{
	if (statement->node->type != Select)
		return NULL;

	NSelect *select = (NSelect*) statement->node;

	if (select->numberContexts != 1)
		return NULL;

	return getContext (select->contexts [0]->contextId);
}

void Statement::dropTable(Syntax * syntax)
{
	Table *table = CompiledStatement::getTable (connection, syntax->getChild(0));
	
	if (table)
		{
		checkAlterPriv (table);
		database->dropTable (table, transaction);
		}
}

void Statement::dropView(Syntax *syntax)
{
	Table *table = CompiledStatement::getTable (connection, syntax->getChild(0));

	if (!table->view)
		throw SQLError (DDL_ERROR, "%s.%s is not a view", table->schemaName, table->name);

	checkAlterPriv (table);
	database->dropTable (table, transaction);
}

int Statement::getUpdateCount()
{
	if (!recordsUpdated)
		return -1;

	int n = recordsUpdated;
	recordsUpdated = -1;

	return n;
}

void Statement::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void Statement::release()
{
	ASSERT (useCount > 0);
    ASSERT (useCount > javaCount);

	if (INTERLOCKED_DECREMENT (useCount) == 0)
		close();
}

void Statement::upgradeTable(Syntax * syntax)
{
	Syntax *node = syntax->getChild(0);
	Table *table = statement->findTable (node, false);

	if (table)
		{
		table->clearIndexesRebuild();
		checkAlterPriv (table);
		}
	else
		{
		createTable (syntax);
		return;
		}

	Syntax *list = syntax->getChild(1);
	LinkedList foreignKeys;
	LinkedList newFields;
	LinkedList changedFields;
	LinkedList newIndexes;
	bool reformat = false;
	Field *field;

	for (field = table->fields; field; field = field->next)
		field->foreignKey = NULL;

	FOR_SYNTAX (item, list)
		switch (item->type)
			{
			case nod_field:
				{
				const char *fldName = item->getChild(0)->getString();
				Field *field = table->findField (fldName);
				
				if (field)
					{
					if (upgradeField (table, field, item, newIndexes))
						{
						changedFields.append (field);
						reformat = true;
						}
					}
				else
					{
					field = addField (table, item);
					newFields.append (field);
					reformat = true;
					}
				}
				break;

			case nod_primary_key:
				{
				Index *index = table->getPrimaryKey();
				
				if (index)
					{
					Syntax *fields = item->getChild(0);
					
					if (index->numberFields != fields->count)
						keyChangeError (table);

					int n = 0;
					
					FOR_SYNTAX (segment, fields)
						Syntax *fld = segment->getChild(0);
						Field *field = table->findField (fld->getString());
						
						if (!field)
							{
							table->deleteIndex (index, transaction);
							throw SQLEXCEPTION (DDL_ERROR, "can't find primary key %s in table %s",
									fld->getString(), table->getName());
							}
							
						if (field != index->fields [n++])
							{
							const char *p = fld->getString();
							table->findField (p);
							keyChangeError (table);
							}
					END_FOR;
					
					break;
					}
					
				newIndexes.append (createPrimaryKey (table, item));
				}
				break;

			case nod_foreign_key:
				{
				ForeignKey *key = addForeignKey (table, NULL, item);
				ForeignKey *existingKey = table->findForeignKey(key);
				
				if (existingKey && existingKey->deleteRule == key->deleteRule)
					delete key;
				else
					foreignKeys.append (key);
				}
				break;

			default:
				item->prettyPrint ("feature not done");
				throw SQLError (DDL_ERROR, "createTable -- not yet done");
			}
	END_FOR;

	FOR_OBJECTS (Field*, field, &newFields)
		if (field->foreignKey)
			{
			field->foreignKey->create();
			field->foreignKey->save (database);
			}
		field->save ();
	END_FOR;

	FOR_OBJECTS (Field*, field, &changedFields)
		field->update();
	END_FOR;

	FOR_OBJECTS (Index*, index, &newIndexes)
		index->create(transaction);
		index->save();
		table->populateIndex (index, transaction);
	END_FOR;

	for (field = table->fields; field; field = field->next)
		{
		ForeignKey *key = field->foreignKey;
		if (key)
			{
			ForeignKey *existingKey = table->findForeignKey(key);
			
			if (existingKey)
				{
				existingKey->setDeleteRule (key->deleteRule);
				existingKey->save(database);
				delete key;
				}
			else
				{
				ForeignKey *alt = table->findForeignKey (field, true);
				
				if (alt)
					alt->deleteForeignKey();
					
				field->foreignKey->create();
				field->foreignKey->save (database);
				}
			field->foreignKey = NULL;
			}
		}

	FOR_OBJECTS (ForeignKey*, key, &foreignKeys)
		ForeignKey *existingKey = table->findForeignKey(key);

		if (existingKey)
			{
			existingKey->setDeleteRule (key->deleteRule);
			existingKey->save(database);
			delete key;
			}
		else
			{
			key->create();
			key->save (database);
			}
	END_FOR;

	if (reformat)
		table->reformat();

	database->commitSystemTransaction();
	table->rebuildIndexes(NULL);
	database->commitSystemTransaction();
}

void Statement::reset()
{
	clearResults (false);

	if (statement)
		{
		statement->deleteInstance (this);
		statement = NULL;
		}

	if (contexts)
		{
		delete [] contexts;
		contexts = NULL;
		}

	if (slots)
		{
		delete [] slots;
		slots = NULL;
		}

	if (sorts)
		{
		for (int n = 0; n < numberSorts; ++n)
			if (sorts [n])
				delete sorts [n];
				
		delete [] sorts;
		sorts = NULL;
		}

	if (bitmaps)
		{
		for (int n = 0; n < numberBitmaps; ++n)
			if (bitmaps [0])
				bitmaps[n]->release();
				
		delete [] bitmaps;
		bitmaps = NULL;
		}

	if (valueSets)
		{
		for (int n = 0; n < numberValueSets; ++n)
			if (valueSets[n])
				delete valueSets[n];
				
		delete [] valueSets;
		valueSets = NULL;
		}

	if (rowSlots)
		{
		delete [] rowSlots;
		rowSlots = NULL;
		}

	if (tableFilter)
		{
		tableFilter->release();
		tableFilter = NULL;
		}

	if (transaction)
		{
		transaction->release();
		transaction = NULL;
		}

	parent = NULL;
	numberContexts = 0;
	active = false;
}

void Statement::getFieldType(Syntax * node, FieldType * fieldType)
{
	Type type;
	int length = 0;
	int scale = 0;
	int precision = 0;
	
	switch (node->type)
		{
		case nod_varchar:
			length = node->getChild(0)->getNumber();
			type = Varchar;
			break;

		case nod_char:
			length = node->getChild(0)->getNumber();
			type = Char;
			break;

		case nod_integer:
			type = Long;
			length = sizeof (int32);
			break;

		case nod_tinyint:
		case nod_smallint:
			type = Short;
			length = sizeof (short);
			break;

		case nod_bigint:
			type = Quad;
			length = sizeof (int64);
			break;

		case nod_text:
			type = Asciiblob;
			length = sizeof (int32);
			break;

		case nod_blob:
			type = Binaryblob;
			length = sizeof (int32);
			break;

		case nod_float:
			type = Float;
			length = sizeof (float);
			break;

		case nod_double:
			type = Double;
			length = sizeof (double);
			break;

		case nod_date:
			type = Date;
			//length = sizeof (DateTime);
			length = sizeof (int64);
			break;

		case nod_timestamp:
			type = Timestamp;
			//length = sizeof (TimeStamp);
			length = sizeof (int64) + sizeof (int32);
			break;

		case nod_time:
			type = TimeType;
			length = sizeof (Time);
			break;

		case nod_numeric:
			{
			precision = node->getChild(0)->getNumber();
			
			if (precision < 5)
				{
				type = Short;
				length = sizeof (short);
				}
			else if (precision < 10)
				{
				type = Long;
				length = sizeof (int32);
				}
			else if (precision < 20)
				{
				type = Quad;
				length = sizeof (int64);
				}
			else
				//throw SQLError (DDL_ERROR, "decimal precision great than 19 digits not yet supported");
				{
				type = Biginteger;
				length = sizeof(BigInt);
				}
				
			scale = 0;
			Syntax *sub = node->getChild(1);
			
			if (sub)
				scale = sub->getNumber();
			}
			break;

		default:
			node->prettyPrint ("Type not understood");
			throw SQLError (DDL_ERROR, "Statement::createTable: type not understood");
		}

	if (precision == 0)
		precision = Field::getPrecision(type, length);
		
	fieldType->type = type;
	fieldType->length = length;
	fieldType->scale = scale;
	fieldType->precision = precision;
}

bool Statement::upgradeField(Table * table, Field * field, Syntax *item, LinkedList &newIndexes)
{
	bool changed = false;
	field->foreignKey = NULL;
	FieldType type;
	getFieldType (item->getChild(1), &type);

	if (field->type != type.type ||
		field->length != type.length ||
		field->scale != type.scale)
		{
		field->type = type.type;
		
		if (field->type != type.type ||
			!((type.type == String || type.type == Char || type.type == Varchar) && 
			  field->length > type.length))
			field->length = type.length;
			
		field->scale = type.scale;
		changed = true;
		}

	Syntax *clauses = item->getChild(2);
	bool notNull = false;
	bool wasNotNull = (field->flags & NOT_NULL) != 0;

	if (clauses)
		FOR_SYNTAX (clause, clauses)
			switch (clause->type)
				{
				case nod_searchable:
					field->makeSearchable(transaction, true);
					break;

				case nod_not_searchable:
					field->makeNotSearchable(transaction, true);
					break;

				case nod_primary_key:
					{
					Index *index = table->getPrimaryKey();
					if (index)
						{
						if (index->numberFields != 1 || index->fields [0] != field)
							//throw SQLEXCEPTION (DDL_ERROR, "can't change primary key with UPGRADE");
							keyChangeError (table);
						}
					else
						{
						if (!notNull && !wasNotNull)
							throw SQLEXCEPTION (DDL_ERROR, "primary keys must be \"not null\"");

						Index *index = table->addIndex (table->getPrimaryKeyName(), 1, PrimaryKey);
						index->addField (field, 0);
						newIndexes.append (index);
						}
					}
					notNull = true;
					break;

				case nod_foreign_key:
					field->foreignKey = addForeignKey (table, field, clause);
					break;

				case nod_not_null:
					notNull = true;
					break;

				case nod_collation:
					{
					const char *collationName = clause->getChild(0)->getString();
					Collation *collation = CollationManager::getCollation (collationName);
					
					if (collation != field->collation)
						{
						field->setCollation (collation);
						table->collationChanged (field);
						changed = true;
						}
					}
					break;

				case nod_repository:
					{
					if (field->type != Asciiblob && field->type != Binaryblob)
						throw SQLEXCEPTION (DDL_ERROR, "can't specify repository on non-blob field %s\n", field->name);
						
					const char *repositoryName = clause->getChild(0)->getString();
					
					if (*repositoryName)
						{
						Repository *repository = database->getRepository (table->schemaName, repositoryName);
						
						if (repository != field->repository)
							{
							field->setRepository (repository);
							changed = true;
							}
						}
					else
						{
						field->setRepository (NULL);
						changed = true;
						}
					}
					break;

				default:
					NOT_YET_IMPLEMENTED;
				}
		END_FOR;

	if (notNull != wasNotNull)
		{
		changed = true;
		
		if (notNull)
			field->flags |= NOT_NULL;
		else
			field->flags &= ~NOT_NULL;
		}
	
	return changed;
}

Index* Statement::createPrimaryKey(Table * table, Syntax * item)
{
	if (table->getPrimaryKey())
		throw SQLEXCEPTION (DDL_ERROR, "table %s already has a primary key",
							table->getName());

	Syntax *fields = item->getChild(0);
	Index *index = table->addIndex (table->getPrimaryKeyName(), fields->count, PrimaryKey);
	int n = 0;

	FOR_SYNTAX (segment, fields)
		Syntax *fld = segment->getChild(0);
		const char *fieldName = fld->getString();
		Field *field = table->findField (fieldName);
		
		if (!field)
			{
			table->deleteIndex (index, transaction);
			
			throw SQLEXCEPTION (DDL_ERROR, "can't find primary key %s in table %s",
								fld->getString(), table->getName());
			}
			
		if (!(field->flags & NOT_NULL))
			{
			table->deleteIndex (index, transaction);
			
			throw SQLEXCEPTION (DDL_ERROR, "primary keys must be \"not null\"");
			}
			
		Syntax *partial = segment->getChild(1);
		
		if (partial)
			index->setPartialLength(n,partial->getNumber());

		index->addField (field, n++);
	END_FOR;

	return index;
}

Index* Statement::createPrimaryKey(Table * table, Field * field)
{
	if (table->getPrimaryKey())
		throw SQLEXCEPTION (DDL_ERROR, "table %s already has a primary key",
							table->getName());

	if (!(field->flags & NOT_NULL))
		throw SQLEXCEPTION (DDL_ERROR, "primary keys must be \"not null\"");

	Index *index = table->addIndex (table->getPrimaryKeyName(), 1, PrimaryKey);
	index->addField (field, 0);

	return index;
}


void Statement::setTableFilter(const WCString *tableName)
{
	setTableFilter (database->getSymbol (tableName));
}

void Statement::setTableFilter(const char *tableName)
{
	Table *table = connection->findTable (tableName);

	if (!table)
		throw SQLEXCEPTION (RUNTIME_ERROR, "can't find table %s", tableName);

	if (!tableFilter)
		tableFilter = new Bitmap();

	tableFilter->set (table->tableId);
}


void Statement::setTableFilter(const char *schemaName, const char *tableName)
{
	Table *table = database->findTable (schemaName, tableName);

	if (!table)
		throw SQLEXCEPTION (RUNTIME_ERROR, "can't find table %s.%s", schemaName, tableName);

	if (!tableFilter)
		tableFilter = new Bitmap();

	tableFilter->set (table->tableId);
}

void Statement::setTableFilter(const WCString *schemaName, const WCString *tableName)
{
	setTableFilter (database->getSymbol (schemaName), database->getSymbol (tableName));
}

bool Statement::getMoreResults()
{
	if (currentResultSet < 0)
		return false;

	return getResultSet (currentResultSet) != NULL;
}

void Statement::clearResults(bool forceRelease)
{
	for (ResultSet *resultSet; (resultSet = resultSets);)
		{
		resultSet->clearStatement();
		
		if (forceRelease)
			resultSet->close();
		}

	for (ResultList *resultList; (resultList = resultLists);)
		{
		resultList->clearStatement();
		
		if (forceRelease)
			resultList->close();
		}
}

void Statement::createUser(Syntax * syntax)
{
	const char *name = syntax->getChild(0)->getString();
	const char *password = syntax->getChild(1)->getString();
	Coterie *coterie = NULL;
	Syntax *node = syntax->getChild(2);
	Syntax *encrypted = syntax->getChild(3);

	if (node)
		coterie = database->roleModel->getCoterie (node->getString());

	if (database->roleModel->findUser (name))
		throw SQLEXCEPTION (DDL_ERROR, "user %s already exists", name);

	User *user = database->roleModel->createUser (name, password, encrypted != NULL, coterie);
	database->roleModel->addUserPrivilege (connection->user, user, PrivAlter);
}

void Statement::grantPriv(Syntax * syntax)
{
	RoleModel *roleModel = database->roleModel;
	Syntax *privs = syntax->getChild(0);
	Syntax *obj = syntax->getChild(1);
	Syntax *target = syntax->getChild(2);
	int32 mask = 0;
	PrivilegeObject *object = NULL;

	switch (obj->type)
		{
		case nod_table:
			object = statement->getTable (obj->getChild(0));
			break;

		case nod_role:
			object = roleModel->getRole (getSchemaName (obj->getChild(0)), getName (obj->getChild(0)));
			break;

		case nod_user:
			object = roleModel->getUser (getName (obj->getChild(0)));
			break;

#ifndef STORAGE_ENGINE
		case nod_coterie:
			object = roleModel->getCoterie (getName (obj->getChild(0)));
			break;
#endif

		default:
			NOT_YET_IMPLEMENTED;
		}
	
	FOR_SYNTAX (priv, privs)
		int32 privMask = getPrivilegeMask (priv->type);
		connection->checkAccess (privMask << GRANT_SHIFT, object);
		mask |= privMask;
	END_FOR;

	if (syntax->getChild(3))
		mask |= mask << GRANT_SHIFT;
	

	Role *role = NULL;
	Syntax *ident = target->getChild(0);

	switch (target->type)
		{
		case nod_user:
			role = roleModel->getUser (ident->getString());
			break;

		case nod_role:
			role = roleModel->getRole (getSchemaName (ident), getName (ident));
			break;

		default:
			NOT_YET_IMPLEMENTED;
		}

	roleModel->addPrivilege (role, object, mask);
	database->commitSystemTransaction();
}

void Statement::createRole(Syntax * syntax, bool upgrade)
{
	Syntax *node = syntax->getChild(0);
	const char *schema = getSchemaName (node);
	const char *name = getName (node);
	RoleModel *roleModel = database->roleModel;
	Role *role = roleModel->findRole (schema, name);

	if (role)
		{
		if (upgrade)
			return;
			
		throw SQLEXCEPTION (DDL_ERROR, "role %s.%s already exists", schema, name);
		}

	roleModel->createRole (connection->user, schema, name);
}

int32 Statement::getPrivilegeMask(SyntaxType type)
{
	for (SyntaxPrivType *p = privilegeTypes;; ++p)
		if (p->type == type)
			return p->privilege;
		else if (p->type == nod_and)
			NOT_YET_IMPLEMENTED;
}

void Statement::grantRole(Syntax * syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *schema = getSchemaName (node);
	const char *name = getName (node);
	bool defaultRole = false;
	int options = 0;

	Role *role = database->roleModel->getRole (schema, name);

	if (!connection->checkAccess (GRANT_OPTION, role))
		{
		connection->checkAccess (GRANT_OPTION, role);
		
		throw SQLEXCEPTION (SECURITY_ERROR, "user %s does not have Grant authority to %s.%s",
								(const char*) connection->user->name, 
								(const char*) role->schemaName,
								(const char*) role->name);
		}

	const char *userName = syntax->getChild(1)->getString();
	User *user = database->roleModel->getUser (userName);

	if (syntax->getChild(2))
		options = 1;

	if (syntax->getChild(3))
		defaultRole = true;

	database->roleModel->addUserRole (user, role, defaultRole, options);
}

const char* Statement::getSchemaName(Syntax * syntax)
{
	if (syntax->count == 1)
		return connection->currentSchemaName();

	return syntax->getChild(0)->getString();
}

const char* Statement::getName(Syntax * syntax)
{
	return syntax->getChild(syntax->count - 1)->getString();
}

void Statement::connectionClosed()
{
	addRef();
	Sync sync (&syncObject, "Statement::close");
	sync.lock (Exclusive);

	/***
	if (special)
		printf ("%8x connectionClosed\n", this);
	***/

	for (ResultSet *resultSet; (resultSet = resultSets);)
		{
		resultSets = resultSet->sibling;
		resultSet->statementClosed();
		}

	connection = NULL;

	if (javaCount == 0)
		{
		sync.unlock();
		
		for (int n = useCount; n > 0; --n)
			release();
		}
}

void Statement::addJavaRef()
{
	++javaCount;
	addRef();

	if (special)
		printf ("%p addJavaRef to %d\n", this, javaCount);
}

void Statement::releaseJava()
{
	ASSERT (javaCount > 0);

	/***
	if (special)
		printf ("%8x releaseJava from %d\n", this, javaCount);
	***/

	if (javaCount == 1)
		if (!connection)
			while (useCount > javaCount)
				release();

	INTERLOCKED_DECREMENT (javaCount);
	release();
}

ResultSet* Statement::getResultSet(int sequence)
{
	int n = 0;
    ResultSet *resultSet;

	for (resultSet = resultSets; n < sequence && resultSet;
		 ++n, resultSet = resultSet->sibling)
		;

	return resultSet;
}

void Statement::alterUser(Syntax * syntax)
{
	const char *name = syntax->getChild(0)->getString();
	User *user = database->roleModel->findUser (name);

	if (!user)
		throw SQLEXCEPTION (DDL_ERROR, "user %s doesn't exist", name);

	if (!connection->checkAccess (PrivAlter, user))
		throw SQLEXCEPTION (SECURITY_ERROR, "user %s does not have alter authority to user %s",
								(const char*) connection->user->name, name);

	Syntax *child;
	Coterie *coterie = NULL;
	const char *password = NULL;
	Syntax *encrypted = syntax->getChild(3);

	if ( (child = syntax->getChild(2)) )
		coterie = database->roleModel->getCoterie (child->getString());

	if ( (child = syntax->getChild(1)) )
		password = child->getString();

	database->roleModel->changePassword (user, password, encrypted != NULL, coterie);
}

void Statement::revokeRole(Syntax * syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *schema = getSchemaName (node);
	const char *name = getName (node);
	Role *role = database->roleModel->getRole (schema, name);
	const char *userName = syntax->getChild(1)->getString();
	User *user = database->roleModel->getUser (userName);

	if (!(user->hasRole (role) & HAS_ROLE))
		throw SQLEXCEPTION (DDL_ERROR, "user %s doesn't have role %s.%s",
								userName, schema, name);

	database->roleModel->revokeUserRole (user, role);
}

void Statement::dropRole(Syntax * syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *schema = getSchemaName (node);
	const char *name = getName (node);
	Role *role = database->roleModel->findRole (schema, name);

	if (!role)
		throw SQLEXCEPTION (DDL_ERROR, "role %s.%s doesn't exist", schema, name);

	checkAlterPriv (role);
	database->roleModel->dropRole (role);
}

void Statement::createView(Syntax * syntax, bool upgrade)
{
	Syntax *node = syntax->getChild(0);
	Table *table = statement->findTable (node, false);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);

	if (table)
		{
		if (!upgrade)
			throw SQLEXCEPTION (DDL_ERROR, "table %s.%s is already defined", schema, name);
			
		if (!table->view)
			throw SQLEXCEPTION (DDL_ERROR, "table %s.%s is not a view", schema, name);
		}

	View *view = new View (schema, name);

	try
		{
		view->compile(statement, syntax);
		}
	catch (...)
		{
		delete view;
		throw;
		}

	if (table)
		{
		if (view->isEquiv(table->view))
			{
			delete view;
			return;
			}
			
		database->dropTable(table, transaction);
		}

	table = database->addTable(connection->user, name, schema, NULL);
	table->setType("VIEW");
	table->view = view;
	view->createFields(table);
	table->save();
	database->roleModel->addUserPrivilege(connection->user, table, ALL_PRIVILEGES);
	database->commitSystemTransaction();
}

void Statement::createSequence(Syntax *syntax, bool upgrade)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName(node);
	const char *schema = getSchemaName(node);
	Syntax *clauses = syntax->getChild(1);
	int64 base = 0;

	FOR_SYNTAX (clause, clauses)
		switch (clause->type)
			{
			case nod_start:
				base = clause->getChild(0)->getQuad();
				break;

			default:
				break;
			}
	END_FOR;

	Sequence *sequence = database->sequenceManager->findSequence (schema, name);

	if (sequence)
		{
		if (!upgrade)
			throw SQLEXCEPTION (DDL_ERROR, "sequence %s.%s already exists", schema, name);
			
		int64 n = sequence->updatePhysical (0, transaction);
		
		if (base > n)
			sequence->updatePhysical (base - n, transaction);
			
		return;
		}

	database->sequenceManager->createSequence (schema, name, base);
}

void Statement::dropSequence(Syntax *syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);
	Sequence *sequence = database->sequenceManager->findSequence (schema, name);

	if (!sequence)
		throw SQLEXCEPTION (DDL_ERROR, "sequence %s.%s doesn't exist", schema, name);

	//checkAlterPriv (sequence);
	database->sequenceManager->deleteSequence (schema, name);
}

void Statement::createTrigger(Syntax *syntax, bool update)
{
#ifndef STORAGE_ENGINE
	const char *name = syntax->getChild(0)->getString();
	Table *table = statement->getTable (syntax->getChild(1));
	Trigger *trigger = table->findTrigger (name);

	if (trigger && !update)
		throw SQLEXCEPTION (DDL_ERROR, "trigger \"%s\" already defined for table \"%s.%s\"",
							name, (const char*) table->schemaName, (const char*) table->name);

	Syntax *options = syntax->getChild(2);
	int mask = 0;
	bool active = true;
	bool changed = false;
	int position = 0;
	LinkedList triggerClasses;

	FOR_SYNTAX (child, options)
		switch (child->type)
			{
			case nod_pre_insert:		mask |= PreInsert;	break;
			case nod_post_insert:		mask |= PostInsert;	break;
			case nod_pre_update:		mask |= PreUpdate;	break;
			case nod_post_update:		mask |= PostUpdate;	break;
			case nod_pre_delete:		mask |= PreDelete;	break;
			case nod_post_delete:		mask |= PostDelete;	break;
			case nod_post_commit:		mask |= PostCommit;	break;
			case nod_active:			active = true;		break;
			case nod_inactive:			active = false;		break;
			case nod_position:			position = child->getNumber(0); break;

			case nod_trigger_class:
				changed = true;
				triggerClasses.appendUnique ((void*) child->getChild(0)->getString());
				break;
			}
	END_FOR

	const char *method = syntax->getChild(3)->getString();
	char *dot = strrchr (method, '.');

	if (!dot)
		throw SQLEXCEPTION (DDL_ERROR, "invalid trigger method name");

	JString className (method, dot - method);
	JString methodName (dot + 1);

	if (trigger)
		{
		if (!changed &&
		    trigger->table == table &&
			trigger->className == className &&
			trigger->methodName == methodName &&
			trigger->mask == mask &&
			trigger->position == position &&
			trigger->active == active)
			return;
			
		table->dropTrigger(trigger);
		Trigger::deleteTrigger (database, table->schemaName, name);
		}

	trigger = new Trigger (name, table, mask, position, active, className, methodName);

	FOR_OBJECTS (const char*, triggerClass, &triggerClasses)
		trigger->addTriggerClass (triggerClass);
	END_FOR;

	try
		{
		trigger->loadClass();
		}
	catch (...)
		{
		trigger->release();
		throw;
		}

	table->addTrigger (trigger);
	trigger->save();
	database->commitSystemTransaction();
#endif
}

void Statement::dropTrigger(Syntax *syntax)
{
#ifndef STORAGE_ENGINE
	Syntax *node = syntax->getChild(0);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);
	JString tableName = Trigger::getTableName (database, schema, name);

	if (tableName == "")
		throw SQLEXCEPTION (DDL_ERROR, "trigger %s.%s not defined", name, schema);

	Table *table = database->findTable (schema, tableName);
	Trigger *trigger = table->findTrigger (name);

	if (trigger)
		table->dropTrigger(trigger);

	Trigger::deleteTrigger (database, schema, name);
	database->commitSystemTransaction();
#endif
}

void Statement::dropIndex(Syntax *syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);
	JString tableName = Index::getTableName (database, schema, name);

	if (tableName == "")
		throw SQLEXCEPTION (DDL_ERROR, "index %s.%s not defined", name, schema);

	Table *table = database->findTable (schema, tableName);

	if (!table)
		throw SQLEXCEPTION (DDL_ERROR, "table %s.%s not defined", (const char*) tableName, schema);

	checkAlterPriv (table);
	Index *index = table->findIndex (name);

	if (index)
		{
		table->dropIndex(index);
		index->deleteIndex(transaction);
		delete index;
		}

	Index::deleteIndex (database, schema, name);
	database->commitSystemTransaction();
}

void Statement::dropIndex(Index *index)
{

}

void Statement::revokePriv(Syntax *syntax)
{
	Syntax *privs = syntax->getChild(0);
	Syntax *obj = syntax->getChild(1);
	Syntax *target = syntax->getChild(2);
	PrivObject type = PrivTable;
	PrivilegeObject *object = NULL;

	FOR_SYNTAX (item, obj)
		switch (item->type)
			{
			case nod_table:
				{
				type = PrivTable;
				Table *table = statement->getTable (item->getChild(0));
				object = table;
				}
				break;

			default:
				NOT_YET_IMPLEMENTED;
			}

		Role *role = NULL;
		int32 mask = 0;
		
		FOR_SYNTAX (priv, privs)
			int32 privMask = getPrivilegeMask (priv->type);
			connection->checkAccess (privMask << GRANT_SHIFT, object);
			mask |= privMask | (privMask << GRANT_SHIFT);
		END_FOR;

		FOR_SYNTAX (targ, target)
			Syntax *ident = targ->getChild(0);
			
			switch (targ->type)
				{
				case nod_user:
					role = database->roleModel->getUser (ident->getString());
					break;

				case nod_role:
					role = database->roleModel->getRole (getSchemaName (ident), getName (ident));
					break;

				default:
					NOT_YET_IMPLEMENTED;
				}
				
			database->roleModel->removePrivilege (role, object, mask);
		END_FOR;
	END_FOR;

	database->commitSystemTransaction();
}

void Statement::alterTrigger(Syntax *syntax)
{
#ifndef STORAGE_ENGINE
	const char *name = syntax->getChild(0)->getString();
	Table *table = statement->getTable (syntax->getChild(1));
	Trigger *trigger = table->findTrigger (name);

	if (!trigger)
		throw SQLEXCEPTION (DDL_ERROR, "trigger \"%s\" is not defined for table \"%s.%s\"",
							name, (const char*) table->schemaName, (const char*) table->name);

	Syntax *options = syntax->getChild(2);
	int mask = 0;
	bool active = true;
	int position = trigger->position;

	FOR_SYNTAX (child, options)
		switch (child->type)
			{
			case nod_pre_insert:		mask |= PreInsert;	break;
			case nod_post_insert:		mask |= PostInsert;	break;
			case nod_pre_update:		mask |= PreUpdate;	break;
			case nod_post_update:		mask |= PostUpdate;	break;
			case nod_pre_delete:		mask |= PreDelete;	break;
			case nod_post_delete:		mask |= PostDelete;	break;
			case nod_active:			active = true;		break;
			case nod_inactive:			active = false;		break;
			case nod_position:			position = child->getNumber(0); break;
			}
	END_FOR

	try
		{
		trigger->loadClass();
		}
	catch (...)
		{
		trigger->release();
		throw;
		}

	trigger->active = active;
	trigger->position = position;
	trigger->save();
	database->commitSystemTransaction();
#endif
}

void Statement::createFilterSet(Syntax *syntax, bool upgrade)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);
	FilterSet *filterSet = statement->findFilterset (node);
	
	if (filterSet && !upgrade)
		throw SQLEXCEPTION (DDL_ERROR, "filterset %s.%s already exists", schema, name);

	if (!filterSet)
		{
		filterSet = new FilterSet (database, schema, name);
		database->filterSetManager->addFilterset (filterSet);
		}

	filterSet->setText (statement->sqlString);
	filterSet->save();
	database->commitSystemTransaction();
}

void Statement::dropFilterSet(Syntax *syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);
	FilterSet *filterSet = statement->findFilterset (node);

	if (!filterSet)
		throw SQLEXCEPTION (DDL_ERROR, "filterset %s.%s not defined", schema, name);

	database->filterSetManager->deleteFilterset (filterSet);
	database->commitSystemTransaction();
}

void Statement::enableFilterSet(Syntax *syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);
	FilterSet *filterSet = statement->findFilterset (node);

	if (!filterSet)
		throw SQLEXCEPTION (DDL_ERROR, "filterset %s.%s not defined", schema, name);

	connection->enableFilterSet (filterSet);
}

void Statement::disableFilterSet(Syntax *syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);
	FilterSet *filterSet = statement->findFilterset (node);

	if (!filterSet)
		throw SQLEXCEPTION (DDL_ERROR, "filterset %s.%s not defined", schema, name);

	connection->disableFilterSet (filterSet);
}

void Statement::enableTriggerClass(Syntax *syntax)
{
	connection->enableTriggerClass (getName (syntax));
}

void Statement::disableTriggerClass(Syntax *syntax)
{
	connection->disableTriggerClass (getName (syntax));
}

JString Statement::analyze(int mask)
{
	Stream stream;

	if (statement)
		stream.format ("%s\n", (const char*) statement->sqlString);

	if ((mask & AnalyzeTree) && statement && statement->node)
		{
		PrettyPrint pp (mask, &stream);
		statement->node->prettyPrint (0, &pp);
		}

	if (mask & AnalyzeCounts)
		{
		analyze (&stream, "Index Hits:", stats.indexHits);
		analyze (&stream, "Index Fetches:", stats.indexFetches);
		analyze (&stream, "Exhaustive Fetches:", stats.exhaustiveFetches);
		analyze (&stream, "Records Fetched:", stats.recordsFetched);
		analyze (&stream, "Records Returned:", stats.recordsReturned);
		analyze (&stream, "Inserts:", stats.inserts);
		analyze (&stream, "Updates:", stats.updates);
		analyze (&stream, "Deletions:", stats.deletions);
		analyze (&stream, "Replaces:", stats.replaces);
		}

	return stream.getJString();
}

void Statement::analyze(Stream *stream, const char *name, int count)
{
	if (count)
		stream->format ("  %-18s %d\n", name, count);
}

void Statement::createCoterie(Syntax *syntax, bool upgrade)
{
#ifndef STORAGE_ENGINE
	const char *name = syntax->getChild(0)->getString();
	Syntax *rangeNodes = syntax->getChild(1);
	Coterie *coterie = database->roleModel->findCoterie (name);
	
	if (coterie && !upgrade)
		{
		throw SQLEXCEPTION (DDL_ERROR, "coterie %s already exists", name);
		
		if (!connection->checkAccess (PrivAlter, coterie))
			throw SQLEXCEPTION (SECURITY_ERROR, "coterie %s does not have alter authority to user %s",
								(const char*) connection->user->name, name);
		}

	CoterieRange *ranges = NULL;

	try
		{
		FOR_SYNTAX (rangeNode, rangeNodes)
			Syntax *fromNode = rangeNode->getChild(0);
			char from [256];
			getString (fromNode, from, sizeof (from));
			Syntax *toNode = rangeNode->getChild(1);
			CoterieRange *range;
			
			if (toNode)
				{
				char to [256];
				getString (toNode, to, sizeof (to));
				range = new CoterieRange (from, to);
				}
			else
				range = new CoterieRange (from, from);
				
			range->next = ranges;
			ranges = range;
		END_FOR
		}
	catch (...)
		{
		for (CoterieRange *range; range = ranges;)
			{
			ranges = range->next;
			delete range;
			}
			
		throw;
		}

	if (!coterie)
		{
		coterie = database->roleModel->createCoterie (name);
		database->roleModel->addUserPrivilege (connection->user, coterie, ALL_PRIVILEGES);
		database->commitSystemTransaction();
		}

	coterie->replaceRanges (ranges);
	database->roleModel->updateCoterie (coterie);
#endif
}

void Statement::dropCoterie(Syntax *syntax)
{
#ifndef STORAGE_ENGINE
	const char *name = syntax->getChild(0)->getString();
	Coterie *coterie = database->roleModel->getCoterie (name);
	checkAlterPriv (coterie);
	database->roleModel->dropCoterie (coterie);
#endif
}

char* Statement::getString(Syntax *node, char *buffer, int length)
{
	char *end = buffer + length;
	char *p = buffer;

	switch (node->type)
		{
		case nod_node_name:
			FOR_SYNTAX (name, node)
				if (name->type == nod_wildcard)
					throw SQLEXCEPTION (DDL_ERROR, "coterie wildcards not yet implemented");
					
				if (p != buffer)
					*p++ = '.';
					
				p = getString(name, p, (int) (end - p));
			END_FOR
			break;

		case nod_ip_address:
		case nod_name:
			{
			for (const char *q = node->getString(); *q && p < end;)
				*p++ = *q++;
				
			*p = 0;
			}
			break;

		default:
			throw SQLEXCEPTION (DDL_ERROR, "coterie option not yet implemented");
		}

	return p;
}

void Statement::clearConnection()
{
	Sync sync (&database->syncConnectionStatements, "Statement::clearConnection");
	sync.lock (Shared);

	if (connection)
		connection->deleteStatement (this);

	connection = NULL;
}

void Statement::reIndex(Syntax *syntax)
{
	const char *name = syntax->getChild(0)->getString();
	Table *table = statement->getTable (syntax->getChild(1));
	Index *index;

	if (strcmp (name, "PRIMARY_KEY") == 0)
		index = table->getPrimaryKey();
	else
		index = table->findIndex (name);

	if (!index)
		throw SQLEXCEPTION (DDL_ERROR, "can't find index \"%s\"", name);

	table->rebuildIndex (index, database->getSystemTransaction());
	database->commitSystemTransaction();
}

bool Statement::isPreparedResultSet()
{
	return false;
}

void Statement::dropUser(Syntax *syntax)
{
	const char *name = syntax->getChild(0)->getString();

	User *user = database->roleModel->findUser (name);

	if (!user)
		throw SQLEXCEPTION (DDL_ERROR, "user %s doesn't exist", name);

	if (!connection->checkAccess (PrivDelete, user))
		throw SQLEXCEPTION (SECURITY_ERROR, "user %s does not have drop authority to user %s",
								(const char*) connection->user->name, name);

	database->roleModel->dropUser (user);
}

bool Statement::execute(const char *sqlString, bool isQuery)
{
	if (statement)
		reset();

	statement = database->getCompiledStatement (connection, sqlString);
	statement->addInstance (this);

	if (isQuery && (!statement->node || statement->node->type != Select))
		throw SQLEXCEPTION (RUNTIME_ERROR, "statement is not a Select");

	if (statement->ddl)
		{
		executeDDL();
		
		return false;
		}

	if (!statement->node)
		return false;

	prepareStatement();
	start (statement->node);

	return resultSets != NULL;
}


void Statement::invalidate()
{
	clearResults (true);

	for (int n = 0; n < numberContexts; ++n)
		contexts[n].close();
}

void Statement::keyChangeError(Table *table)
{
	throw SQLEXCEPTION (DDL_ERROR, "can't change primary key for %s.%s with UPGRADE statment",
						table->schemaName, table->name);
}

void Statement::checkAlterPriv(PrivilegeObject *object)
{
	if (!connection->checkAccess (PrivAlter, object))
		throw SQLEXCEPTION (SECURITY_ERROR, "user %s does not have alter authority to %s %s.%s",
								(const char*) connection->user->name, 
								privObjectTypes [object->getPrivilegeType()],
								object->schemaName, object->name);
}

void Statement::print(Stream *stream)
{
	stream->format ("%8x Statement uc %d, jc %d, cncnt %p, state %p, rs %p\n",
					this, useCount, javaCount, connection, statement, resultSets);
					
	if (statement)
		stream->format ("         %s\n", (const char*) statement->sqlString);
}


void Statement::createRepository(Syntax *syntax, bool upgrade)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);
	const char *sequenceName = NULL;
	int	volume = 0;

	if ( (node = syntax->getChild(1)) )
		sequenceName = node->getString();

	const char *fileName = NULL;
	const char *rollovers = NULL;
	Sequence *sequence = NULL;

	if ( (node = syntax->getChild(2)) )
		fileName = node->getString();

	if ( (node = syntax->getChild(3)) )
		volume = node->getNumber ();

	if ( (node = syntax->getChild(4)) )
		{
		rollovers = node->getString();
		Repository::validateRollovers (rollovers);
		}

	Repository *repository = database->findRepository (schema, name);

	if (!upgrade && repository)
		throw SQLEXCEPTION (DDL_ERROR, "repository %s.%s already exists", schema, name);

	if (sequenceName)	 
		sequence = database->sequenceManager->findSequence (schema, sequenceName);

	if (!upgrade && !sequence)
		if (sequenceName)
			throw SQLEXCEPTION (DDL_ERROR, "sequence %s.%s isn't defined", schema, sequenceName);
		else
			throw SQLEXCEPTION (DDL_ERROR, "a sequence name is required for repository %d", schema);

	if (repository)
		{
		if (sequence)
			repository->setSequence (sequence);
			
		if (fileName)
			repository->setFilePattern (fileName);
			
		if (volume)
			repository->setVolume (volume);
			
		if (rollovers)
			repository->setRollover (rollovers);
			
		repository->save();
		}
	else
		repository = database->createRepository (name, schema, sequence, fileName, volume, rollovers);

	repository->save();
}

void Statement::createDomain(Syntax *syntax, bool upgrade)
{
	NOT_YET_IMPLEMENTED;
}

void Statement::upgradeSchema(Syntax *syntax, bool upgrade)
{
	const char *schemaName = syntax->getChild(0)->getString();
	Schema *schema = database->getSchema (schemaName);
	Syntax *node;

	if ( (node = syntax->getChild(1)) )
		schema->setInterval (node->getNumber());

	if ( (node = syntax->getChild(2)) )
		schema->setSystemId (node->getNumber());
}

void Statement::dropRepository(Syntax *syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);
	Repository *repository = database->getRepository (schema, name);
	Sync sync (&database->syncSysConnection, "Statement::dropRepository");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"select field,tableName from system.fields where schema=? and repositoryName=?");
	statement->setString (1, schema);
	statement->setString (2, name);
	ResultSet *resultSet = statement->executeQuery();

	if (resultSet->next())
		{
		JString fieldName = resultSet->getString (1);
		JString tableName = resultSet->getString (2);
		resultSet->close();
		statement->close();
		
		throw SQLError (DDL_ERROR, "repository %s is in use in field %s.%s\n",
						name, (const char*) fieldName, (const char*) tableName);
		}

	sync.unlock();
	resultSet->close();
	statement->close();
	database->deleteRepository (repository);
}

void Statement::syncRepository(Syntax *syntax)
{
	Syntax *node = syntax->getChild(0);
	const char *name = getName (node);
	const char *schema = getSchemaName (node);
	Repository *repository = database->getRepository (schema, name);
	const char *fileName = syntax->getChild(1)->getString();
	repository->synchronize (fileName, connection->getTransaction());
	connection->commit();
}

ResultList* Statement::findResultList(int32 handle)
{
	for (ResultList *resultList = resultLists; resultList; resultList = resultList->sibling)
		if (resultList->handle == handle)
			return resultList;

	return NULL;
}

ValueSet* Statement::getValueSet(int slot)
{
	return valueSets[slot];
}

void Statement::renameTables(Syntax *syntax)
{
	for (int n = 0; n < syntax->count; ++n)
		{
		Syntax *node = syntax->getChild(n);
		Syntax *from = node->getChild(0);
		const char *name;
		const char *schema;
		Syntax *to = node->getChild(1);
		Syntax *newName = to->getChild(0);

		if (from->count == 1)
			{
			name = newName->getString();
			schema = connection->currentSchemaName();
			}
		else
			{
			schema = newName->getString();
			name = to->getChild(1)->getString();
			}

		if (statement->findTable(to, false))
			throw SQLError(DDL_ERROR, "rename table target \"%s.%s\" already exists", name, schema);

		Table *table = statement->getTable(from);
		table->rename(schema, name);
		database->commitSystemTransaction();
		}
}

void Statement::transactionEnded(void)
{
	if (transaction)
		{
		transaction->release();
		transaction = NULL;
		}
}

void Statement::createTableSpace(Syntax *syntax)
{
	TableSpaceManager *tableSpaceManager = database->tableSpaceManager;
	const char *name = syntax->getChild(0)->getString();
	const char *fileName = syntax->getChild(1)->getString();
	TableSpace *tableSpace = tableSpaceManager->findTableSpace(name);
	uint64 initialAllocation = 0;
	
	if (tableSpace)
		{
		if (syntax->type == nod_create_tablespace)
			throw SQLError(TABLESPACE_EXIST_ERROR, "tablespace \"%s\" already exists", name);
		
		if (syntax->type == nod_upgrade_tablespace)
			{
			if (!tableSpace->fileNameEqual(fileName))
				throw SQLError(DDL_ERROR, "filename change not supported for tablespaces");
			
			return;
			}
		}

	if (syntax->type == nod_upgrade_tablespace && tableSpace && tableSpace->filename != fileName)
		throw SQLError(DDL_ERROR, "can't change filename for tablespace");

	Syntax *alloc = syntax->getChild(2);
	
	if (alloc)
		initialAllocation = alloc->getUInt64();
		
	if (!tableSpace)
		tableSpaceManager->createTableSpace(name, fileName, initialAllocation, false);
}

void Statement::dropTableSpace(Syntax* syntax)
{
	const char *name = syntax->getChild(0)->getString();
	TableSpaceManager *tableSpaceManager = database->tableSpaceManager;
	TableSpace *tableSpace = tableSpaceManager->findTableSpace(name);
	
	if (!tableSpace)
		throw SQLError(TABLESPACE_NOT_EXIST_ERROR, "table space \"%s\" is not defined", name);

	Sync sync (&database->syncSysConnection, "Statement::createIndex");
	sync.lock (Shared);

	PStatement statement = database->prepareStatement (
		"select * from system.tables where tablespace=?");
	statement->setString (1, name);
	RSet resultSet = statement->executeQuery();
	
	if (resultSet->next())
		throw SQLError(TABLESPACE_NOT_EMPTY, "table space \"%s\" is not empty", name);
	
	resultSet.close();
	statement.close();
	sync.unlock();
	tableSpaceManager->dropTableSpace(tableSpace);
}
