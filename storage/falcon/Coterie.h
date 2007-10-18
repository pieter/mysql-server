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

// Coterie.h: interface for the Coterie class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COTERIE_H__63A2B5C3_EA76_11D5_B8F6_00E0180AC49E__INCLUDED_)
#define AFX_COTERIE_H__63A2B5C3_EA76_11D5_B8F6_00E0180AC49E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "PrivType.h"
#include "PrivilegeObject.h"

class CoterieRange;

class Coterie : public PrivilegeObject
{
public:
	virtual PrivObject getPrivilegeType();
	void addRange (const char *from, const char *to);
	void replaceRanges (CoterieRange *newRanges);
	void clear();
	bool validateAddress (int32 address);
	Coterie(Database *db, const char *coterieName);
	virtual ~Coterie();

	Coterie			*next;
	CoterieRange	*ranges;
};

#endif // !defined(AFX_COTERIE_H__63A2B5C3_EA76_11D5_B8F6_00E0180AC49E__INCLUDED_)
