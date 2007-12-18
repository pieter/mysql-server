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

// FsbGroup.h: interface for the FsbGroup class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FSBGROUP_H__2296B9D1_3812_43D1_BD30_4F9DDFA389E3__INCLUDED_)
#define AFX_FSBGROUP_H__2296B9D1_3812_43D1_BD30_4F9DDFA389E3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Fsb.h"
#include "Row.h"

class NSelect;
class NNode;

class FsbGroup : public Fsb, public Row 
{
public:
	virtual int getNumberValues();
	Value* getValue (Statement *statement, int index);
	void close(Statement * statement);
	virtual Row* fetch(Statement * statement);
	virtual void open(Statement * statement);
	FsbGroup(NSelect *select, NNode *groupList, Fsb *source);
	virtual ~FsbGroup();

	Fsb		*stream;
	int		numberColumns;
	int		countSlot;
	int		rowSlot;
	int		*groupSlots;
	int		*valueSlots;
	NNode	*groups;
	NNode	*values;
};

#endif // !defined(AFX_FSBGROUP_H__2296B9D1_3812_43D1_BD30_4F9DDFA389E3__INCLUDED_)
