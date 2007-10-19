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

// NStat.h: interface for the NStat class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NSTAT_H__3C895E42_F564_11D3_98D9_0000C01D2301__INCLUDED_)
#define AFX_NSTAT_H__3C895E42_F564_11D3_98D9_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "NNode.h"

class NStat : public NNode  
{
public:
	virtual const char* getName();
	virtual Value* eval(Statement * statement);
	virtual void increment (Statement *statement);
	virtual bool isStatistical();
	virtual void reset(Statement * statement);
	NStat(CompiledStatement *statement, NType typ, Syntax *syntax);
	virtual ~NStat();

	bool	distinct;
	int		countSlot;
};

#endif // !defined(AFX_NSTAT_H__3C895E42_F564_11D3_98D9_0000C01D2301__INCLUDED_)
