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

// NInSelect.h: interface for the NInSelect class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NINSELECT_H__BCE57031_AF8F_11D4_98F9_0000C01D2301__INCLUDED_)
#define AFX_NINSELECT_H__BCE57031_AF8F_11D4_98F9_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "NNode.h"

class NSelect;
class NInSelectBitmap;

class NInSelect : public NNode  
{
public:
	virtual void prettyPrint(int level, PrettyPrint *pp);
	virtual bool isInvertible(CompiledStatement *statement, Context *context);
	virtual NNode* makeInversion (CompiledStatement *statement, Context *context, Index *index);
	virtual bool computable(CompiledStatement * statement);
	virtual int evalBoolean(Statement * statement);
	NInSelect(CompiledStatement *statement, Syntax *syntax);
	virtual ~NInSelect();

	NSelect		*select;
	NSelect		*inversion;
	NNode		*expr;
	NInSelectBitmap		*bitmap;
};

#endif // !defined(AFX_NINSELECT_H__BCE57031_AF8F_11D4_98F9_0000C01D2301__INCLUDED_)
