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

// Role.h: interface for the Role class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ROLE_H__4BDA0346_6F6D_11D3_AB78_0000C01D2301__INCLUDED_)
#define AFX_ROLE_H__4BDA0346_6F6D_11D3_AB78_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "PrivType.h"
#include "PrivilegeObject.h"
#include "SyncObject.h"

#define ROLE_PRIV_SIZE		11

class Privilege;
class Database;
class PrivilegeObject;


class Role : public PrivilegeObject 
{
public:
	Privilege* getPrivilege (PrivilegeObject *object);
	int32 getPrivileges (PrivilegeObject *object);
	Role(Database *db, const char* roleSchema, const char* roleName);

protected:
	virtual ~Role();
public:
	virtual PrivObject getPrivilegeType();
	virtual void dropObject (PrivilegeObject *object);
	void release();
	void addRef();

	int			useCount;
	Role		*collision;
	Privilege	*privileges [ROLE_PRIV_SIZE];
	SyncObject	syncObject;
};

#endif // !defined(AFX_ROLE_H__4BDA0346_6F6D_11D3_AB78_0000C01D2301__INCLUDED_)
