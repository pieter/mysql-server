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

// NMatching.h: interface for the NMatching class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NMATCHING_H__B065FE15_037E_11D5_9911_0000C01D2301__INCLUDED_)
#define AFX_NMATCHING_H__B065FE15_037E_11D5_9911_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "NNode.h"

class Bitmap;
class NField;

class NMatching : public NNode  
{
public:
	virtual void close (Statement *statement);
	virtual void prettyPrint (int level, PrettyPrint *pp);
	virtual bool computable (CompiledStatement *statement);
	virtual bool isInvertible (CompiledStatement *statement, Context *context);
	virtual int evalBoolean(Statement *statement);
	virtual Bitmap* evalInversion(Statement * statement);
	NMatching(CompiledStatement *statement, Syntax *syntax);
	virtual ~NMatching();

	NField	*field;
	NNode	*expr;
	int		slot;
};

#endif // !defined(AFX_NMATCHING_H__B065FE15_037E_11D5_9911_0000C01D2301__INCLUDED_)
