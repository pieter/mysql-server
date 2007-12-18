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

// FsbOuterJoin.h: interface for the FsbOuterJoin class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FSBOUTERJOIN_H__C7A1FBC1_9FD7_11D5_B8D8_00E0180AC49E__INCLUDED_)
#define AFX_FSBOUTERJOIN_H__C7A1FBC1_9FD7_11D5_B8D8_00E0180AC49E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "FsbJoin.h"
#include "Row.h"
//#include "Value.h"

class FsbOuterJoin : public FsbJoin, public Row
{
public:
	//virtual Value* getValue (Statement *statement, int index);
	virtual void open (Statement *statement);
	virtual Row* fetch(Statement * statement);
	FsbOuterJoin(CompiledStatement *statement, int numberStreams, Row *rowSource);
	virtual ~FsbOuterJoin();

	int		stateSlot;
	//Value	nullValue;
};

#endif // !defined(AFX_FSBOUTERJOIN_H__C7A1FBC1_9FD7_11D5_B8D8_00E0180AC49E__INCLUDED_)
