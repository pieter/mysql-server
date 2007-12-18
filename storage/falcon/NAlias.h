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

// NAlias.h: interface for the NAlias class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NALIAS_H__CD175F86_E6E3_11D2_AB6C_0000C01D2301__INCLUDED_)
#define AFX_NALIAS_H__CD175F86_E6E3_11D2_AB6C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "NNode.h"
//#include "Types.h"	// Added by ClassView


class NAlias : public NNode
{
public:
	virtual FieldType getType();
	virtual bool equiv (NNode *node);
	virtual void reset(Statement * statement);
	virtual void increment (Statement *statement);
	virtual bool isStatistical();
	 NAlias (CompiledStatement *statement, JString alias);
	virtual NNode* copy(CompiledStatement * statement, Context * context);
	virtual void gen (Stream *stream);
	virtual Field* getField();
	const char* getName();
	virtual Value* eval (Statement *statement);
	NAlias(CompiledStatement *statement, Syntax *syntax);
	virtual ~NAlias();

	JString		name;
	NNode		*expr;
	void prettyPrint(int level, PrettyPrint* pp);
};

#endif // !defined(AFX_NALIAS_H__CD175F86_E6E3_11D2_AB6C_0000C01D2301__INCLUDED_)
