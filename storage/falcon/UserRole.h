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

// UserRole.h: interface for the UserRole class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_USERROLE_H__5327FF01_B89E_11D3_98BF_0000C01D2301__INCLUDED_)
#define AFX_USERROLE_H__5327FF01_B89E_11D3_98BF_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class Role;

class UserRole  
{
public:
	UserRole(Role *userRole, bool defRole, int opts);
	virtual ~UserRole();

	UserRole	*collision;
	UserRole	*next;
	Role		*role;
	int			options;
	bool		defaultRole;
	int			mask;	
};

#endif // !defined(AFX_USERROLE_H__5327FF01_B89E_11D3_98BF_0000C01D2301__INCLUDED_)
