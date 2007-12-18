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

// Privilege.h: interface for the Privilege class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PRIVILEGE_H__4BDA0349_6F6D_11D3_AB78_0000C01D2301__INCLUDED_)
#define AFX_PRIVILEGE_H__4BDA0349_6F6D_11D3_AB78_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "PrivType.h"

class PrivilegeObject;

class Privilege  
{
public:
	Privilege(PrivObject type, PrivilegeObject *object, int32 mask);
	virtual ~Privilege();

	int32			privilegeMask;
	PrivObject		objectType;
	PrivilegeObject	*object;
	Privilege		*collision;
};

#endif // !defined(AFX_PRIVILEGE_H__4BDA0349_6F6D_11D3_AB78_0000C01D2301__INCLUDED_)
