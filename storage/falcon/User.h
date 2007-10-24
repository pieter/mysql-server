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

// User.h: interface for the User class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_USER_H__E4859523_3879_11D3_AB77_0000C01D2301__INCLUDED_)
#define AFX_USER_H__E4859523_3879_11D3_AB77_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define ROLES_HASH_SIZE		101

#include "Role.h"

class UserRole;
class Privilege;
class Coterie;

class User : public Role
{
public:
	void updateRole (Role * role, bool defRole, int options);
	void revokeRole (Role *role);
	void changePassword (const char *password);
	int  hasRole (Role *role);
	bool validatePassword (const char *passwd);
	void addRole (Role *role, bool defRole, int options);
	User (Database *db, const char *account, const char *password, Coterie *coterie, bool sys);

protected:
	virtual ~User();
public:
	bool validateAddress (int32 address);
	virtual PrivObject getPrivilegeType();
	static JString encryptPassword (const char *password);

	UserRole	*roles [ROLES_HASH_SIZE];
	UserRole	*roleList;
	Coterie		*coterie;
	JString		password;
	UCHAR		passwordDigest[20];
	bool		system;
};

#endif // !defined(AFX_USER_H__E4859523_3879_11D3_AB77_0000C01D2301__INCLUDED_)
