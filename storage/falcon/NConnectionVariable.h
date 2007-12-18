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

// NConnectionVariable.h: interface for the NConnectionVariable class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NCONNECTIONVARIABLE_H__1F63A284_9414_11D5_899A_CC4599000000__INCLUDED_)
#define AFX_NCONNECTIONVARIABLE_H__1F63A284_9414_11D5_899A_CC4599000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "NNode.h"

class NConnectionVariable : public NNode  
{
public:
	virtual void prettyPrint (int level, PrettyPrint *pp);
	virtual NNode* copy(CompiledStatement * statement, Context * context);
	virtual bool computable(CompiledStatement * statement);
	virtual Value* eval (Statement *statement);
	NConnectionVariable(CompiledStatement *statement, const char *variable);
	virtual ~NConnectionVariable();

	const char	*name;
	int			valueSlot;
};

#endif // !defined(AFX_NCONNECTIONVARIABLE_H__1F63A284_9414_11D5_899A_CC4599000000__INCLUDED_)
