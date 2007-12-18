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

// NExists.h: interface for the NExists class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEXISTS_H__CBA114B1_2C65_11D5_9918_0000C01D2301__INCLUDED_)
#define AFX_NEXISTS_H__CBA114B1_2C65_11D5_9918_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "NNode.h"

class NSelect;


class NExists : public NNode  
{
public:
	virtual void prettyPrint (int level, PrettyPrint *pp);
	virtual bool computable(CompiledStatement *statement);
	virtual int evalBoolean (Statement *statement);
	NExists(CompiledStatement *statement, Syntax *syntax);
	virtual ~NExists();

	NSelect		*select;
};

#endif // !defined(AFX_NEXISTS_H__CBA114B1_2C65_11D5_9918_0000C01D2301__INCLUDED_)
