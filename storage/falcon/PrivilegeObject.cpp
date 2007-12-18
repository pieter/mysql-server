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

// PrivilegeObject.cpp: implementation of the PrivilegeObject class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "PrivilegeObject.h"
#include "Database.h"
#include "RoleModel.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PrivilegeObject::PrivilegeObject(Database *db)
{
	database = db;
	catalogName = "";
}

PrivilegeObject::~PrivilegeObject()
{

}

void PrivilegeObject::drop()
{
	database->roleModel->dropObject (this);
}

void PrivilegeObject::setName(const char *objectSchema, const char *objectName)
{
	ASSERT (database->isSymbol (objectSchema));
	ASSERT (database->isSymbol (objectName));
	schemaName = objectSchema;
	name = objectName;
}

bool PrivilegeObject::isNamed(const char *objectSchema, const char *objectName)
{
	return !strcmp (objectSchema, schemaName) && !strcmp (objectName, name);
}
