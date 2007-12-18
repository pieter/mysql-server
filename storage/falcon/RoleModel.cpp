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

// RoleModel.cpp: implementation of the RoleModel class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "RoleModel.h"
#include "User.h"
#include "UserRole.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Privilege.h"
#include "PrivilegeObject.h"
#include "SQLError.h"
#include "Table.h"
#include "Log.h"
#include "Sync.h"

#ifndef STORAGE_ENGINE
#include "Coterie.h"
#include "CoterieRange.h"
#endif

#define HASH(address,size)				(int)(((UIPTR) address >> 2) % size)

static const char* ddl [] = 
	{
	"upgrade table system.privileges ("
		"holderType tinyint not null,"
		"holderSchema varchar (128) not null,"
		"holderName varchar (128) not null,"
		"objectType tinyint not null,"
		"objectSchema varchar (128) not null,"
		"objectName varchar (128) not null,"
		"privilegeMask integer,"
		"primary key (holderType, holderSchema, holderName, objectType, objectSchema, objectName))",
		
	"upgrade table system.roles (\n\
		schema varchar (128) not null,\n\
		roleName varchar (128) not null,\n\
		primary key (schema, roleName))",
		
	"upgrade table system.users (\n"
		"userName varchar (128)  not null primary key,\n"
		"password varchar (32),\n"
		"coterie varchar (132))",
		
	"upgrade table system.userRoles ("
		"userName varchar (128) not null references system.users,"
		"roleSchema varchar (128) not null,"
		"roleName varchar (128) not null,"
		"options tinyint,"
		"defaultRole tinyint,"
		"foreign key (roleSchema, roleName) references system.roles,"
		"primary key (userName, roleSchema, roleName))\n",
		
	"upgrade table system.coteries ("
		"coterie varchar (128) not null,\n"
		"sequence smallint not null,\n" 
		"ip_start varchar (128),\n"
		"ip_end varchar (20),\n"
		"primary key (coterie, sequence))\n",
		
	"upgrade index role_user on system.userRoles (userName)",
	"upgrade index object_privileges on system.privileges(objectType, objectSchema, objectName)",
	"grant select on system.privileges to public",
	"grant select on system.roles to public",
	"grant select on system.users to public",
	"grant select on system.userRoles to public",
	"grant select on system.coteries to public",
	NULL
	};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

RoleModel::RoleModel(Database *db)
{
	database = db;
	memset (users, 0, sizeof (users));
	memset (roles, 0, sizeof (roles));
	tablesCreated = false;
	coteries = NULL;
	const char *nullSchema = database->getSymbol ("");
	systemUser = new User (database, database->getSymbol ("SYSTEM"), nullSchema, NULL, true);
	publicUser = new User (database, database->getSymbol ("PUBLIC"), nullSchema, NULL, false);
	insertUser (publicUser);
}

RoleModel::~RoleModel()
{
	systemUser->release();
	//publicUser->release();
	Role *role;
    int n;

	for (n = 0; n < ROLE_HASH_SIZE; ++n)
		while ( (role = users [n]) )
			{
			users [n] = (User*) role->collision;
			role->release();
			}

	for (n = 0; n < ROLE_HASH_SIZE; ++n)
		while ( (role = roles [n]) )
			{
			roles [n] = role->collision;
			role->release();
			}

#ifndef STORAGE_ENGINE
	for (Coterie *coterie; coterie = coteries;)
		{
		coteries = coterie->next;
		delete coterie;
		}
#endif
}

void RoleModel::createTables()
{
	tablesCreated = true;
	Table *table = database->findTable("SYSTEM", "PRIVILEGES");
	
	if (!table || !table->findIndex("OBJECT_PRIVILEGES"))
		{
		Statement *statement = database->createStatement();
		
		for (const char **ptr = ddl; *ptr; ++ptr)
			try
				{
				statement->executeUpdate (*ptr);
				}
			catch (SQLException &exception)
				{
				Log::log ("RoleModel::createTables: %s\n", exception.getText());
				throw;
				}
				
		statement->close();
		database->commitSystemTransaction();
		}
}

User* RoleModel::createUser(const char *userName, const char *password, bool encrypted, Coterie *coterie)
{
	ASSERT (database->isSymbol (userName));

	if (!tablesCreated)
		createTables();

	JString encryptedPassword = (encrypted) ? (JString) password : User::encryptPassword (password);
	Sync sync (&database->syncSysConnection, "RoleModel::createUser");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
			"insert into system.users (username,password,coterie) values (?,?,?)");
	statement->setString (1, userName);
	statement->setString (2, encryptedPassword);
	
#ifndef STORAGE_ENGINE
	if (coterie)
		statement->setString (3, coterie->name);
#endif
		
	statement->execute();
	statement->close();

	sync.unlock();

	database->commitSystemTransaction();
	User *user = new User (database, userName, encryptedPassword, coterie, false);
	insertUser (user);

	return user;
}

User* RoleModel::findUser(const char * name)
{
	ASSERT (database->isSymbol (name));

	if (publicUser->name == name)
		return publicUser;

	if (!tablesCreated)
		createTables();

	int slot = HASH (name, ROLE_HASH_SIZE);
	User *user;

	for (user = users [slot]; user; user = (User*) user->collision)
		if (user->name == name)
			return user;

	const char *userName = name;

	/***
	Log::log("Warning:: Security backdoor is active!");
	if (!strcmp (userName, "PANIC"))
		{
		user = new User (database, userName, User::encryptPassword("panic"), NULL, true);
		insertUser (user);
		return user;
		}
	***/
	
	Sync sync (&database->syncSysConnection, "RoleModel::findUser");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
			"select password, coterie from system.users where userName=?");
	statement->setString (1, userName);
	ResultSet *resultSet = statement->executeQuery();
	const char *password = NULL;
	const char *coterieName = NULL;

	if (!resultSet->next())
		{
		resultSet->close();
		statement->close();
		return NULL;
		}

	password = resultSet->getString (1);
	coterieName = resultSet->getString (2);
	Coterie *coterie = NULL;

#ifndef STORAGE_ENGINE
	if (coterieName [0])
		coterie = findCoterie (database->getSymbol (coterieName));
#endif

	user = new User (database, userName, password, coterie, false);
	insertUser (user);
	resultSet->close();
	statement->close();

	statement = database->prepareStatement (
		"select roleSchema,roleName,options,defaultRole from system.userRoles where userName=?");
	statement->setString (1, userName);
	resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		int n = 1;
		const char *roleSchema = resultSet->getString (n++);
		const char *roleName = resultSet->getString (n++);
		int options = resultSet->getInt (n++);
		bool defaultRole = resultSet->getInt (n++) != 0;
		Role *role = findRole (database->getSymbol (roleSchema), 
							   database->getSymbol (roleName));
		if (role)
			user->addRole (role, defaultRole, options);
		}

	resultSet->close();
	statement->close();

	return user;
}

Role* RoleModel::findRole(const char * schema, const char * name)
{
	ASSERT (database->isSymbol (schema));
	ASSERT (database->isSymbol (name));

	if (!tablesCreated)
		createTables();

	int slot = HASH (name, ROLE_HASH_SIZE);
	Role *role;

	for (role = roles [slot]; role; role = role->collision)
		if (role->name == name && role->schemaName == schema)
			return role;

	Sync sync (&database->syncSysConnection, "RoleModel::findRole");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"select roleName from system.roles where schema=? and roleName=?");
	statement->setString (1, JString::upcase (schema));
	statement->setString (2, JString::upcase (name));
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		role = new Role (database, schema, name);
		role->collision = roles [slot];
		roles [slot] = role;
		}

	resultSet->close();
	statement->close();

	return role;
}


void RoleModel::createRole(User *owner, const char *schema, const char * name)
{
	if (!tablesCreated)
		createTables();

	Sync sync (&database->syncSysConnection, "RoleModel::createRole");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
			"insert into system.roles (schema, roleName) values (?,?)");
	statement->setString (1, schema);
	statement->setString (2, name);
	statement->execute();
	statement->close();
	sync.unlock();

	int slot = HASH (name, ROLE_HASH_SIZE);
	Role *role = new Role (database, schema, name);
	role->collision = roles [slot];
	roles [slot] = role;

	addUserRole (owner, role, true, 1);
	addPrivilege (owner, role, -1);
	database->commitSystemTransaction();
}

void RoleModel::addPrivilege(Role * role, PrivilegeObject *object, int32 mask)
{
	Privilege *privilege = role->getPrivilege (object);
	privilege->privilegeMask |= mask;

	if (!database->formatting)
		if (!updatePrivilege (role, object, privilege->privilegeMask))
			insertPrivilege (role, object, privilege->privilegeMask);
}

void RoleModel::addUserPrivilege(User * user, PrivilegeObject * object, int32 mask)
{
	if (!tablesCreated)
		createTables();

	// The following is necessay to avoid assessing SYSTEM.PRIVILEGES while it is being created

	if (mask != ALL_PRIVILEGES)
		{
		Privilege *privilege = user->getPrivilege (object);
		privilege->privilegeMask |= mask;
		}

	if (updatePrivilege (user, object, mask))
		return;

	insertPrivilege (user, object, mask);
}

void RoleModel::addUserRole(User *user, Role * role, bool defaultRole, int options)
{
	Sync sync (&database->syncSysConnection, "RoleModel::addUserRole");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"replace into system.userRoles"
			"(userName,roleSchema,roleName,defaultRole,options) values (?,?,?,?,?)");
	int n = 1;
	statement->setString (n++, user->name);
	statement->setString (n++, role->schemaName);
	statement->setString (n++, role->name);
	statement->setInt (n++, defaultRole);
	statement->setInt (n++, options);

	try
		{
		statement->executeUpdate();
		statement->close();
		sync.unlock();
		}
	catch (...)
		{
		statement->close();
		throw;
		}

	database->commitSystemTransaction();
	user->updateRole (role, defaultRole, options);
}

Role* RoleModel::getRole(const char * schema, const char * name)
{
	Role *role = findRole (schema, name);

	if (!role)
		throw SQLEXCEPTION (DDL_ERROR, "role %s.%s does not exist", schema, name);

	return role;
}

User* RoleModel::getUser(const char * userName)
{
	User *user = findUser (userName);

	if (!user)
		throw SQLEXCEPTION (DDL_ERROR, "user %s does not exist", userName);

	return user;
}

bool RoleModel::updatePrivilege(Role * role, PrivilegeObject * object, int32 mask)
{
	Sync sync (&database->syncSysConnection, "RoleModel::updatePrivilege");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"update system.privileges set privilegeMask=? where "
			"holderType=? and "
			"holderSchema=? and "
			"holderName=? and "
			"objectType=? and "
			"objectSchema=? and "
			"objectName=?");
	int n = 1;
	statement->setInt (n++, mask);
	statement->setInt (n++, role->getPrivilegeType());
	statement->setString (n++, role->schemaName);
	statement->setString (n++, role->name);
	statement->setInt (n++, object->getPrivilegeType());
	statement->setString (n++, object->schemaName);
	statement->setString (n++, object->name);
	int count = statement->executeUpdate();
	statement->close();

	return count == 1;
}

void RoleModel::insertUser(User * user)
{
	int slot = HASH (user->name, ROLE_HASH_SIZE);
	user->collision = users [slot];
	users [slot] = user;
}

void RoleModel::changePassword(User *user, const char * password, bool encrypted, Coterie *coterie)
{
	user->coterie = coterie;
	JString encryptedPassword = user->password;

	if (password)
		encryptedPassword = (encrypted) ? (JString) password : User::encryptPassword (password);

	Sync sync (&database->syncSysConnection, "RoleModel::changePassword");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"update system.users set password=?,coterie=? where userName=?");
	statement->setString (1, encryptedPassword);
	statement->setString (3, user->name);

#ifndef STORAGE_ENGINE
	if (coterie)
		statement->setString (2, coterie->name);
	else
		statement->setNull (2, 0);
#endif

	int count = statement->executeUpdate();
	statement->close();

	if (count < 1)
		throw SQLEXCEPTION (DDL_ERROR, "couldn't change password");

	sync.unlock();
	database->commitSystemTransaction();
	user->changePassword (encryptedPassword);
}

void RoleModel::revokeUserRole(User * user, Role * role)
{
	Sync sync (&database->syncSysConnection, "RoleModel::revokeUserRole");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"delete from system.userRoles where userName=? and roleSchema=? and roleName=?");
	int n = 1;
	statement->setString (n++, user->name);
	statement->setString (n++, role->schemaName);
	statement->setString (n++, role->name);

	try
		{
		statement->executeUpdate();
		statement->close();
		sync.unlock();
		}
	catch (...)
		{
		statement->close();
		throw;
		}

	database->commitSystemTransaction();
	user->revokeRole (role);
}

void RoleModel::dropRole(Role *role)
{
	if (!tablesCreated)
		createTables();

	Sync sync (&database->syncSysConnection, "RoleModel::dropRole");
	sync.lock (Shared);
	deletePrivileges (role);
	PreparedStatement *statement = database->prepareStatement (
			"delete from system.roles where schema=? and roleName=?");
	statement->setString (1, role->schemaName);
	statement->setString (2, role->name);
	statement->execute();
	statement->close();

	statement = database->prepareStatement (
			"delete from system.userroles where roleschema=? and roleName=?");
	statement->setString (1, role->schemaName);
	statement->setString (2, role->name);
	statement->execute();
	statement->close();

	sync.unlock();
	database->commitSystemTransaction();

	int slot = HASH (role->name, ROLE_HASH_SIZE);

	for (Role **ptr = roles + slot; *ptr; ptr = &(*ptr)->collision)
		if (*ptr == role)
			{
			*ptr = role->collision;
			break;
			}

	for (int n = 0; n < ROLE_HASH_SIZE; ++n)
		for (User *user = users [n]; user; user = (User*) user->collision)
			user->revokeRole (role);

	role->release();
}

void RoleModel::insertPrivilege(Role * role, PrivilegeObject * object, int32 mask)
{
	Sync sync (&database->syncSysConnection, "RoleModel::insertPrivilege");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"insert into system.privileges"
			"(holderType, holderSchema,holderName,objectType,objectSchema,objectName,privilegeMask)"
			"values (?,?,?,?,?,?,?)");
	int n = 1;
	statement->setInt (n++, role->getPrivilegeType());
	statement->setString (n++, role->schemaName);
	statement->setString (n++, role->name);
	statement->setInt (n++, object->getPrivilegeType());
	statement->setString (n++, object->schemaName);
	statement->setString (n++, object->name);
	statement->setInt (n++, mask);

	try
		{
		statement->executeUpdate();
		statement->close();
		sync.unlock();
		}
	catch (...)
		{
		statement->close();
		throw;
		}

	database->commitSystemTransaction();
}

User* RoleModel::getSystemUser()
{
	return systemUser;
}

void RoleModel::removePrivilege(Role *role, PrivilegeObject *object, int32 mask)
{
	Privilege *privilege = role->getPrivilege (object);
	privilege->privilegeMask &= ~mask;
	updatePrivilege (role, object, privilege->privilegeMask);
}

void RoleModel::dropObject(PrivilegeObject *object)
{
	PrivObject type = object->getPrivilegeType();
	Sync sync (&database->syncSysConnection, "RoleModel::dropObject");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"delete from system.privileges where objectType=? and objectSchema=? and objectName=?");
	int n = 1;
	statement->setInt (n++, type);
	statement->setString (n++, object->schemaName);
	statement->setString (n++, object->name);

	try
		{
		statement->executeUpdate();
		statement->close();
		}
	catch (...)
		{
		statement->close();
		throw;
		}

	sync.unlock();

	for (n = 0; n < ROLE_HASH_SIZE; ++n)
		for (Role *role = roles [n]; role; role = role->collision)
			role->dropObject (object);

	for (n = 0; n < ROLE_HASH_SIZE; ++n)
		for (Role *role = users [n]; role; role = role->collision)
			role->dropObject (object);
}

void RoleModel::initialize()
{
	createTables();
}

Coterie* RoleModel::findCoterie(const char *name)
{
	Coterie *coterie = NULL;

#ifndef STORAGE_ENGINE
	for (coterie = coteries; coterie; coterie = coterie->next)
		if (coterie->name == name)
			return coterie;

	Sync sync (&database->syncSysConnection, "RoleModel::findCoterie");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"select ip_start,ip_end from system.coteries where coterie=?");
	statement->setString (1, name);
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		{
		if (!coterie)
			{
			coterie = new Coterie (database, database->getSymbol (name));
			insertCoterie (coterie);
			}
		coterie->addRange (resultSet->getString (1), resultSet->getString (2));
		}

	resultSet->close();
#endif

	return coterie;
}

Coterie* RoleModel::getCoterie(const char *name)
{
	Coterie *coterie = findCoterie (name);

	if (!coterie)
		throw SQLEXCEPTION (SECURITY_ERROR, "coterie %s does not exist", name);

	return coterie;
}

Coterie* RoleModel::createCoterie(const char *name)
{
	Coterie *coterie = NULL;

#ifndef STORAGE_ENGINE
	coterie = new Coterie (database, name);
	insertCoterie (coterie);
#endif

	return coterie;
}

void RoleModel::insertCoterie(Coterie *coterie)
{
#ifndef STORAGE_ENGINE
	coterie->next = coteries;
	coteries = coterie;
#endif
}

void RoleModel::updateCoterie(Coterie *coterie)
{
#ifndef STORAGE_ENGINE
	Sync sync (&database->syncSysConnection, "RoleModel::updateCoterie");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"delete from system.coteries where coterie=?");
	statement->setString (1, coterie->name);
	statement->executeUpdate();
	statement->close();
	statement = database->prepareStatement (
		"insert into system.coteries (coterie,sequence,ip_start,ip_end) values (?,?,?,?)");
	statement->setString (1, coterie->name);
	int sequence = 1;

	for (CoterieRange *range = coteries->ranges; range; range = range->next, ++sequence)
		{
		statement->setInt (2, sequence);
		statement->setString (3, range->fromString);
		statement->setString (4, range->toString);
		statement->executeUpdate();
		}

	sync.unlock();
	database->commitSystemTransaction();
	statement->close();
#endif
}

void RoleModel::dropCoterie(Coterie *coterie)
{
#ifndef STORAGE_ENGINE
	Sync sync (&database->syncSysConnection, "RoleModel::dropCoterie");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"select username from system.users where coterie=?");
	statement->setString (1, coterie->name);
	ResultSet *resultSet = statement->executeQuery();
	
	if (resultSet->next())
		{
		JString user = resultSet->getString (1);
		resultSet->close();
		statement->close();
		throw SQLEXCEPTION (DDL_ERROR, "coterie %s is in use for user %s", 
							(const char*) coterie->name,
							(const char*) user);
		}

	resultSet->close();
	statement->close();

	for (Coterie **ptr = &coteries; *ptr; ptr = (Coterie**) &(*ptr)->next)
		if (*ptr == coterie)
			{
			*ptr = (Coterie*) coterie->next;
			break;
			}

	sync.unlock();
	dropObject (coterie);
	sync.lock (Shared);
	statement = database->prepareStatement (
			"delete from system.coteries where coterie=?");
	statement->setString (1, coterie->name);
	statement->execute();
	statement->close();
	sync.unlock();
	database->commitSystemTransaction();

	delete coterie;
#endif
}


void RoleModel::dropUser(User *user)
{
	int slot = HASH (user->name, ROLE_HASH_SIZE);

	for (User **ptr = users + slot; *ptr; ptr = (User**) &(*ptr)->collision)
		if (*ptr == user)
			{
			*ptr = (User*) user->collision;
			break;
			}

	Sync sync (&database->syncSysConnection, "RoleModel::dropUser");
	sync.lock (Shared);
	deletePrivileges (user);
	PreparedStatement *statement = database->prepareStatement (
			"delete from system.users where username=?");
	statement->setString (1, user->name);
	statement->execute();
	statement->close();
	sync.unlock();
	database->commitSystemTransaction();

	user->release();
}

void RoleModel::deletePrivileges(Role *role)
{
	PreparedStatement *statement = database->prepareStatement (
			"delete from system.privileges where holderschema=? and holdername=? and holdertype=?");
	statement->setString (1, role->schemaName);
	statement->setString (2, role->name);
	statement->setInt (3, role->getPrivilegeType());
	statement->execute();
	statement->close();

	statement = database->prepareStatement (
			"delete from system.privileges where objectschema=? and objectname=? and objecttype=?");
	statement->setString (1, role->schemaName);
	statement->setString (2, role->name);
	statement->setInt (3, role->getPrivilegeType());
	statement->execute();
	statement->close();
}


void RoleModel::renameTable(Table *table, const char *newSchema, const char *newName)
{
	PreparedStatement *statement = database->prepareStatement(
		"update system.privileges set objectSchema=?, objectName=? "
		"  where objectSchema=? and objectName=? and objectType=?");
	int n = 1;
	statement->setString(n++, newSchema);
	statement->setString(n++, newName);
	statement->setString(n++, table->schemaName);
	statement->setString(n++, table->name);
	statement->setInt(n++, PrivTable);
	int count = statement->executeUpdate();
	statement->close();

	statement = database->prepareStatement(
		"update system.privileges set holderSchema=?, holderName=? "
		"  where holderSchema=? and holderName=? and holderType=?");
	n = 1;
	statement->setString(n++, newSchema);
	statement->setString(n++, newName);
	statement->setString(n++, table->schemaName);
	statement->setString(n++, table->name);
	statement->setInt(n++, PrivTable);
	count = statement->executeUpdate();
	statement->close();
}
