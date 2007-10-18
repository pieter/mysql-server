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

// NUpdate.h: interface for the NUpdate class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NUPDATE_H__02AD6A5A_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_NUPDATE_H__02AD6A5A_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "NNode.h"
#include "Row.h"

class Fsb;
class Table;

class NUpdate : public NNode, public Row 
{
public:
	virtual bool references (Table *table);
	virtual void evalStatement (Statement *statement);
	NUpdate(CompiledStatement *statement, Syntax *syntax);
	virtual ~NUpdate();

	int			contextId;
	int			numberAssignments;
	Field		**fields;
	NNode		*node;
	Fsb			*stream;
	Table		*table;
	const char	*cursorName;
	Context		*context;
};

#endif // !defined(AFX_NUPDATE_H__02AD6A5A_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
