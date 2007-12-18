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

// RoleModel.h: interface for the RoleModel class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ROLEMODEL_H__4BDA0345_6F6D_11D3_AB78_0000C01D2301__INCLUDED_)
#define AFX_ROLEMODEL_H__4BDA0345_6F6D_11D3_AB78_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "PrivType.h"

#define ROLE_HASH_SIZE		101

class Database;
class User;
class Role;
class PrivilegeObject;
class Coterie;
class Table;

class RoleModel  
{
public:
	void renameTable(Table *table, const char *newSchema, const char *newName);
	void deletePrivileges (Role *role);
	void dropUser (User *user);
	void initialize();
	void dropObject (PrivilegeObject *object);
	void removePrivilege(Role * role, PrivilegeObject *object, int32 mask);
	User* getSystemUser();
	void insertPrivilege(Role * role, PrivilegeObject * object, int32 mask);
	void addUserPrivilege(User *user, PrivilegeObject *object, int32 mask);
	void dropRole (Role *role);
	void revokeUserRole (User *user, Role *role);
	void changePassword (User *user, const char *password, bool encrypted, Coterie *coterie);
	void insertUser (User *user);
	bool updatePrivilege (Role *role, PrivilegeObject *object, int32 mask);
	User* getUser (const char *userName);
	Role* getRole (const char *schema, const char *name);
	void addUserRole (User *user, Role *role, bool defaultRole, int options);
	void addPrivilege (Role *role, PrivilegeObject *object, int32 mask);
	void createRole (User *owner, const char *schema, const char *name);
	Role* findRole(const char * schema, const char * name);
	User* findUser (const char *name);
	User* createUser (const char *name, const char * password, bool encrypted, Coterie *coterie);
	void createTables();
	RoleModel(Database *db);
	virtual ~RoleModel();

	void dropCoterie (Coterie *coterie);
	void insertCoterie (Coterie *coterie);
	Coterie* createCoterie (const char *name);
	Coterie* getCoterie (const char *name);
	Coterie* findCoterie (const char *name);
	void updateCoterie (Coterie *coterie);

	Database	*database;
	User*		users [ROLE_HASH_SIZE];
	Role*		roles [ROLE_HASH_SIZE];
	Coterie		*coteries;
	bool		tablesCreated;
	User		*systemUser;
	User		*publicUser;
};

#endif // !defined(AFX_ROLEMODEL_H__4BDA0345_6F6D_11D3_AB78_0000C01D2301__INCLUDED_)
