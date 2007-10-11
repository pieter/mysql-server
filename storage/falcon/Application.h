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

// Application.h: interface for the Application class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_APPLICATION_H__07386551_D59F_11D2_AB65_0000C01D2301__INCLUDED_)
#define AFX_APPLICATION_H__07386551_D59F_11D2_AB65_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "TableAttachment.h"
#include "SyncObject.h"

static const int ALIAS_HASH_SIZE			= 101;
static const int AGENT_HASH_SIZE			= 101;
static const int MAX_ALIASED_QUERY_STRING	= 80;

class Applications;
class Connection;
class Session;
class Image;
class Images;
class Database;
class Role;
class Module;
class PreparedStatement;
class Agent;
class Table;
class Alias;

class Application   : public TableAttachment
{
public:
	Role* findRole (const char *roleName);
	Image* getImage (const char *name);
	void pushNameSpace (Connection *connection);
	Application (Applications *apps, const char *appName, Application *extends, const char *appClass);

protected:
	virtual ~Application();

public:
	void checkout (Connection *connection);
	void insertAgent (RecordVersion *record);
	void insertModule (RecordVersion *record);
	void loadAgents();
	Agent* getAgent (const char *userAgent);
	void addChild (Application *child);
	void rehash();
	virtual void tableDeleted (Table *table);
	const char* getAlias (Connection *connection, const char *queryString);
	Alias* insertAlias (const char *alias, const char *queryString);
	void initializeQueryLookup(Connection *connection);
	const char* findQueryString (Connection *connection, const char *string);
	virtual void insertCommit(Table * table, RecordVersion * record);
	void tableAdded (Table *table);
	void release();
	void addRef();

	Application		*extends;
	Application		*sibling;
	Application		*children;
	Application		*collision;
	Connection		*aliasConnection;
	int				pendingAliases;
	Database		*database;
	JString			name;
	JString			className;
	Applications	*applications;
	Images			*images;
	Module			*modules;
	const char		*schema;
	volatile INTERLOCK_TYPE	useCount;
	Table			*modulesTable;
	Table			*aliasesTable;
	Table			*agentsTable;
	PreparedStatement	*insertQueryAliases;
	SyncObject		syncObject;
	Alias			*aliases [ALIAS_HASH_SIZE];
	Alias			*queryStrings [ALIAS_HASH_SIZE];
	Agent			*agents [AGENT_HASH_SIZE];
	Agent			*agentList;
};

#endif // !defined(AFX_APPLICATION_H__07386551_D59F_11D2_AB65_0000C01D2301__INCLUDED_)
