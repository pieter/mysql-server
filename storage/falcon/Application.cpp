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

// Application.cpp: implementation of the Application class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include "Engine.h"
#include "JavaVM.h"
#include "Application.h"
#include "Applications.h"
#include "Connection.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "SQLError.h"
#include "Session.h"
#include "ImageManager.h"
#include "Images.h"
#include "Image.h"
#include "RoleModel.h"
#include "Scheduler.h"
#include "Interlock.h"
#include "Module.h"
#include "Log.h"
#include "Table.h"
#include "ValueEx.h"
#include "RecordVersion.h"
#include "TemplateManager.h"
#include "Templates.h"
#include "Alias.h"
#include "Sync.h"
#include "Agent.h"

static const char *urlDdl [] =
	{
	"upgrade table %s.query_string_aliases (\n"
		"alias varchar (10),\n"
		"query_string varchar (80))",
	"grant select,insert on %s.modules to role %s.application",
	"upgrade sequence %s.alias_sequence",
	NULL
	};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Application::Application(Applications *apps, const char *appName, Application *extendsApp, const char *clsName)
		 : TableAttachment (POST_COMMIT)
{
	applications = apps;
	name = appName;
	schema = applications->database->getSymbol (name);
	className = clsName;
	extends = extendsApp;
	children = NULL;
	images = NULL;
	database = applications->database;
	useCount = 1;
	modules = NULL;
	insertQueryAliases = NULL;
	aliasConnection = NULL;
	aliasesTable = NULL;
	modulesTable = database->findTable (name, "MODULES");
	agentsTable = database->findTable (name, "USER_AGENTS");
	agentList = NULL;
	memset (aliases, 0, sizeof (aliases));
	memset (queryStrings, 0, sizeof (queryStrings));
	pendingAliases = 0;

	if (modulesTable)
		{
		modulesTable->addAttachment (this);
		JString sql;
		sql.Format ("select module,schema_name from %s.modules order by sequence desc", (const char*) name);
		PreparedStatement *statement = apps->database->systemConnection->prepareStatement (sql);
		ResultSet *resultSet = statement->executeQuery();

		while (resultSet->next())
			{
			Module *module = new Module (resultSet->getString (1), resultSet->getString (2));
			module->next = modules;
			modules = module;
			}

		resultSet->release();
		statement->release();
		}

	if (agentsTable)
		{
		agentsTable->addAttachment (this);
		loadAgents();
		}

	rehash();
}

Application::~Application()
{
	database->scheduler->deleteEvents (this);

	if (insertQueryAliases)
		insertQueryAliases->close();

	if (aliasConnection)
		aliasConnection->close();

	for (Module *module; module = modules;)
		{
		modules = module->next;
		delete module;
		}

	for (int n = 0; n < ALIAS_HASH_SIZE; ++n)
		for (Alias *node; node = aliases [n];)
			{
			aliases [n] = node->collision;
			delete node;
			}

	for (Agent *agent; agent = agentList;)
		{
		agentList = agent->next;
		delete agent;
		}
}

void Application::pushNameSpace(Connection * connection)
{
	if (extends)
		extends->pushNameSpace (connection);

	connection->pushSchemaName (schema);
}

Image* Application::getImage(const char *imageName)
{
	if (!images)
		images = applications->database->imageManager->getImages (name, this);

	return images->findImage (imageName);
}

Role* Application::findRole(const char * roleName)
{
	return database->roleModel->findRole (schema, roleName);
}

void Application::addRef()
{
	INTERLOCKED_INCREMENT (useCount);
}

void Application::release()
{
	if (INTERLOCKED_DECREMENT (useCount) == 0)
		delete this;
}

void Application::tableAdded(Table *table)
{
	if (strcmp (table->name, "MODULES") == 0)
		{
		modulesTable = table;
		modulesTable->addAttachment (this);
		}
	else if (strcmp (table->name, "USER_AGENTS") == 0)
		{
		agentsTable = table;
		agentsTable->addAttachment (this);
		}
}

void Application::insertCommit(Table *table, RecordVersion *record)
{
	if (table == modulesTable)
		insertModule (record);
	else if (table == agentsTable)
		insertAgent (record);
}

const char* Application::findQueryString(Connection *connection, const char *alias)
{
	if (!insertQueryAliases)
		initializeQueryLookup (connection);

	Sync sync (&syncObject, "Application::findQueryString");
	sync.lock (Shared);

	int slot = JString::hash (alias, ALIAS_HASH_SIZE);
	Alias *node;

	for (node = aliases [slot]; node; node = node->collision)
		if (node->key == alias)
			return node->value;

	return NULL;
}


const char* Application::getAlias(Connection *connection, const char *queryString)
{
	if (!insertQueryAliases)
		{
		initializeQueryLookup (connection);
		if (!insertQueryAliases)
			{
			Log::log ("Application::getAlias: initialization failed\n");
			return "lost";
			}
		}


	// First, see if we already know about it.

	Sync sync (&syncObject, "Application::getAlias");
	sync.lock (Shared);

	int slot = JString::hash (queryString, ALIAS_HASH_SIZE);
	Alias *node;

	for (node = queryStrings [slot]; node; node = node->collision2)
		if (node->value == queryString)
			return node->key;

	// Nope, didn't find it.  Prepare to make one up.

	sync.unlock();
	sync.lock (Exclusive);

	// Take another look, just in case

	for (node = queryStrings [slot]; node; node = node->collision2)
		if (node->value == queryString)
			return node->key;

	// Not there; make up a new one

	int64 sequence = connection->getSequenceValue ("alias_sequence");
	JString alias;
	alias.Format ("a%d", (int) sequence);
	node = insertAlias (alias, queryString);
	Connection *cnct = aliasConnection;
	PreparedStatement *statement = insertQueryAliases;
	insertQueryAliases->setString (1, queryString);
	insertQueryAliases->setString (2, alias);
	insertQueryAliases->executeUpdate();
	++pendingAliases;

	return node->key;
}

void Application::initializeQueryLookup(Connection *connection)
{
	JString sql;
	aliasesTable = database->findTable (name, "query_string_aliases");
	Statement *ddlStatement = connection->createStatement();

	if (!aliasesTable)
		{
		for (const char **ddl = urlDdl; *ddl; ++ddl)
			{
			sql.Format (*ddl, (const char*) name, (const char*) name);
			ddlStatement->execute (sql);
			}
		aliasesTable = database->findTable (name, "query_string_aliases");
		}

	ddlStatement->close();
	aliasesTable->addAttachment (this);
	Sync sync (&syncObject, "Application::initializeQueryLookup");
	sync.lock (Exclusive);

	if (aliasConnection)
		return;

	aliasConnection = connection->clone();
	Role *role = aliasConnection->findRole (name, "APPLICATION");

	if (!role)
		{
		sync.unlock();
		Log::log ("Can't find role \"APPLICATION\"\n");
		return;
		}

	aliasConnection->assumeRole (role);

	PreparedStatement *statement = aliasConnection->prepareStatement (
		"select alias, query_string from query_string_aliases");
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		insertAlias (resultSet->getString (1), resultSet->getString (2));

	resultSet->close();
	statement->close();

	insertQueryAliases = aliasConnection->prepareStatement (
		"insert into query_string_aliases (query_string, alias) values (?,?)");
}

Alias* Application::insertAlias(const char *alias, const char *queryString)
{
	Alias *node = new Alias (alias, queryString);
	int slot = node->key.hash (ALIAS_HASH_SIZE);
	node->collision = aliases [slot];
	aliases [slot] = node;

	slot = node->value.hash (ALIAS_HASH_SIZE);
	node->collision2 = queryStrings [slot];
	queryStrings [slot] = node;

	return node;
}

void Application::tableDeleted(Table *table)
{
	if (table == aliasesTable)
		{
		aliasesTable = NULL;
		if (insertQueryAliases)
			{
			insertQueryAliases->close();
			insertQueryAliases = NULL;
			}
		}
}

void Application::rehash()
{
	if (extends)
		memcpy (agents, extends->agents, sizeof (agents));
	else
		memset (agents, 0, sizeof (agents));

	for (Agent *agent = agentList; agent; agent = agent->next)
		{
		int slot = agent->name.hash (AGENT_HASH_SIZE);
		agent->collision = agents [slot];
		agents [slot] = agent;
		}

	for (Application *child = children; child; child = child->sibling)
		child->rehash();
}

void Application::addChild(Application *child)
{
	child->sibling = children;
	children = child;
}

Agent* Application::getAgent(const char *userAgent)
{
	for (Agent *agent = agents [JString::hash (userAgent, AGENT_HASH_SIZE)]; agent; agent = agent->next)
		if (agent->name == userAgent)
			return agent;

	return NULL;
}

void Application::loadAgents()
{
	JString sql;
	sql.Format ("select agent,action from %s.user_agents", (const char*) name);
	PreparedStatement *statement = applications->database->systemConnection->prepareStatement (sql);
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		const char *agentName = resultSet->getString (1);
		const char *agentAction = resultSet->getString (2);
		Agent *agent = new Agent (agentName, agentAction);
		agent->next = agentList;
		agentList = agent;
		}

	resultSet->release();
	statement->release();
}

void Application::insertModule(RecordVersion *record)
{
	int nameId = modulesTable->getFieldId ("MODULE");
	int	schemaId = modulesTable->getFieldId ("SCHEMA_NAME");
	ValueEx moduleName (record, nameId);
	ValueEx schema (record, schemaId);

	Module *module = new Module (moduleName.getString(), schema.getString());
	module->next = modules;
	modules = module;

	Templates *templates = database->templateManager->findTemplates (name);

	if (templates)
		templates->addModule (module);

	Images *images = database->imageManager->findImages (name);

	if (images)
		images->addModule (module);
}

void Application::insertAgent(RecordVersion *record)
{
	int nameId = agentsTable->getFieldId ("AGENT");
	ValueEx agentName (record, nameId);
	int actionId = agentsTable->getFieldId ("ACTION");
	ValueEx action (record, actionId);

	Agent *agent = new Agent (agentName.getString(), action.getString());
	agent->next = agentList;
	agentList = agent;
	rehash();
}

void Application::checkout(Connection *connection)
{
	if (pendingAliases && aliasConnection)
		{
		Sync sync (&syncObject, "Application::checkout");
		sync.lock (Exclusive);
		aliasConnection->commit();
		pendingAliases = 0;
		}
}
