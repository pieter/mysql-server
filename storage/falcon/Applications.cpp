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

// Applications.cpp: implementation of the Applications class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <string.h>
#include <stdio.h>
#include "Engine.h"
#include "Applications.h"
#include "Application.h"
#include "ResultSet.h"
#include "SQLError.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Database.h"
#include "Table.h"
#include "RecordVersion.h"
#include "Value.h"
#include "SessionManager.h"
#include "JavaVM.h"
#include "Java.h"
#include "Scheduler.h"
#include "Log.h"
#include "Manifest.h"
#include "Sync.h"

#define APP_HASH_SIZE		101
#define APPLICATION			"netfrastructure/model/Application"
#define BASE				"base"

static const char *ddl [] =
	{
    "upgrade table Applications ("
			"application varchar (30) not null primary key,"
			"extends varchar (30),"
			"classname varchar (128))",
	"grant select on applications to public",
	NULL
	};
;

static const char populate [] =
	"insert into Applications (application,classname)\
		    values ('" BASE "', '" APPLICATION "')";

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Applications::Applications(Database *db) : TableAttachment (POST_COMMIT)
{
	database = db;
	memset (applications, 0, sizeof (applications));
}

Applications::~Applications()
{
	Application *application;

	for (int n = 0; n < APP_HASH_SIZE; ++n)
		while (application = applications [n])
			{
			applications [n] = application->collision;
			application->release();
			}
}

Application* Applications::getApplication(const char *applicationName)
{
	int slot = JString::hash (applicationName, APP_HASH_SIZE);
	Application *application;

	for (application = applications [slot];
	     application; application = application->collision)
		if (application->name == applicationName)
			return application;

	Table *table = database->findTable ("SYSTEM", "APPLICATIONS");

	if (!table)
		{
		for (const char **p = ddl; *p; ++p)
			database->execute (*p);
			
		database->execute (populate);
		}
	
	Sync sync (&database->syncSysConnection, "Applications::getApplication");

	PreparedStatement *statement = database->prepareStatement (
		"select extends,classname"
		" from system.applications where application=?");
	statement->setString (1, applicationName);
	int n;

	for (n = 0; !application && n < 2; ++n)
		{
		sync.lock (Shared);
		ResultSet *resultSet = statement->executeQuery();
		
		if (resultSet->next())
			{
			Application *extends = NULL;
			const char *extendsName = resultSet->getString (1);
			
			if (extendsName [0])
				extends = getApplication (extendsName);
				
			JString className = resultSet->getString (2);
			resultSet->close();
			sync.unlock();
			bool mandatory = className == "netfrastructure/model/Application";
			Manifest *manifest = database->java->findManifest (className);
			
			if (!manifest)
				{
				Log::debug ("can't find manifest for %s\n", (const char*) className);
				
				if (mandatory)
					throw SQLEXCEPTION (SECURITY_ERROR, "can't find manifest for %s\n", (const char*) className);
				}
			else if (!manifest->valid)
				{
				Log::debug ("invalid manifest for %s\n", (const char*) className);
				
				if (mandatory)
					throw SQLEXCEPTION (SECURITY_ERROR, "invalid manifest for %s\n", (const char*) className);
				}
				
			application = new Application (this, applicationName, extends, className);
			
			if (extends)
				extends->addChild (application);
			insert (application);
			
			database->scheduler->loadEvents (application);
			}
		else
			{
			resultSet->close();
			sync.unlock();
			}
		if (!application && !strcmp (applicationName, BASE))
			{
			database->execute (populate);
			database->commitSystemTransaction();
			
			continue;
			}
		break;
		}

	statement->close();

	return application;
}

void Applications::insert(Application * application)
{
	int slot = JString::hash (application->name, APP_HASH_SIZE);
	application->collision = applications [slot];
	applications [slot] = application;
}

void Applications::tableAdded(Table * table)
{
	if (table->isNamed ("SYSTEM", "APPLICATIONS"))
		table->addAttachment (this);
	else if (strcmp (table->name, "MODULES") == 0 ||
			 strcmp (table->name, "USER_AGENTS") == 0)
		for (int slot = 0; slot < APP_HASH_SIZE; ++slot)
			for (Application *application = applications [slot]; application; application = application->collision)
				if (application->name.equalsNoCase (table->schemaName))
					{
					application->tableAdded (table);
					break;
					}
}

void Applications::deleteCommit(Table * table, Record * record)
{
	Application *application = getApplication (table, record);

	if (!application)
		return;

	int slot = JString::hash (application->name, APP_HASH_SIZE);

	for (Application **ptr = applications + slot; *ptr; ptr = &(*ptr)->collision)
		if (*ptr == application)
			{
			*ptr = application->collision;
			break;
			}

	application->release();
}

/***
void Applications::insertCommit(Table * table, RecordVersion * record)
{

}

void Applications::updateCommit(Table * table, RecordVersion * record)
{

}
***/

Application* Applications::getApplication(Table * table, Record * record)
{
	int fieldId = table->getFieldId ("APPLICATION");
	Value value;
	record->getValue (fieldId, &value);
	char *temp = NULL;
	const char *name = value.getString (&temp);
	Application *application = getApplication (name);

	if (temp)
		delete [] temp;
	
	return application;	
}
