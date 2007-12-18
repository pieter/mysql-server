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

// NRepair.h: interface for the NRepair class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NREPAIR_H__3F203DDE_56CB_484A_A451_7BDE06F95B53__INCLUDED_)
#define AFX_NREPAIR_H__3F203DDE_56CB_484A_A451_7BDE06F95B53__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "NDelete.h"

class NRepair : public NDelete  
{
public:
	virtual void evalStatement(Statement * statement);
	NRepair(CompiledStatement *statement, Syntax *syntax);
	virtual ~NRepair();

};

#endif // !defined(AFX_NREPAIR_H__3F203DDE_56CB_484A_A451_7BDE06F95B53__INCLUDED_)
