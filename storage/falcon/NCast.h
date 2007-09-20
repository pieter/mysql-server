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

// NCast.h: interface for the NCast class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NCAST_H__D6E796F0_7466_4E86_B4C9_CC14000AC9D6__INCLUDED_)
#define AFX_NCAST_H__D6E796F0_7466_4E86_B4C9_CC14000AC9D6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "NNode.h"

class NCast : public NNode  
{
public:
	virtual Value* eval(Statement * statement);
	NCast(CompiledStatement *statement, Syntax *syntax);
	virtual ~NCast();

	Type	type;
};

#endif // !defined(AFX_NCAST_H__D6E796F0_7466_4E86_B4C9_CC14000AC9D6__INCLUDED_)
