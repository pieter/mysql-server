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

// PrivilegeObject.h: interface for the PrivilegeObject class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PRIVILEGEOBJECT_H__4BDA034A_6F6D_11D3_AB78_0000C01D2301__INCLUDED_)
#define AFX_PRIVILEGEOBJECT_H__4BDA034A_6F6D_11D3_AB78_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Privilege.h"

class Database;

class PrivilegeObject  
{
public:
	bool isNamed (const char *objectSchema, const char *objectName);
	void setName (const char *objectSchema, const char *objectName);
	virtual void drop();
	PrivilegeObject(Database *db);
	virtual ~PrivilegeObject();
	virtual	PrivObject	getPrivilegeType() = 0;

	Database		*database;
	const char*		name;
	const char*		schemaName;
	const char*		catalogName;
};

#endif // !defined(AFX_PRIVILEGEOBJECT_H__4BDA034A_6F6D_11D3_AB78_0000C01D2301__INCLUDED_)
