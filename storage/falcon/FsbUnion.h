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

// FsbUnion.h: interface for the FsbUnion class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FSBUNION_H__8416D511_0540_11D6_B8F7_00E0180AC49E__INCLUDED_)
#define AFX_FSBUNION_H__8416D511_0540_11D6_B8F7_00E0180AC49E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "FsbJoin.h"

class CompiledStatement;

class FsbUnion : public FsbJoin 
{
public:
	virtual int getStreamIndex(Statement *statement);
	virtual const char* getType();
	virtual Row* fetch(Statement * statement);
	FsbUnion(CompiledStatement *statement, int numberStreams, Row *rowSource);
	virtual ~FsbUnion();

};

#endif // !defined(AFX_FSBUNION_H__8416D511_0540_11D6_B8F7_00E0180AC49E__INCLUDED_)
