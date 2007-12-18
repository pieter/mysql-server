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

// Row.h: interface for the Row class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ROW_H__12A51DD1_B381_11D6_B914_00E0180AC49E__INCLUDED_)
#define AFX_ROW_H__12A51DD1_B381_11D6_B914_00E0180AC49E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Value.h"

CLASS(Statement);

class Row  
{
public:
	virtual int getNumberValues();
	Row();
	virtual ~Row();

	virtual Value *getValue (Statement *statement, int index);

	Value	nullValue;
};

#endif // !defined(AFX_ROW_H__12A51DD1_B381_11D6_B914_00E0180AC49E__INCLUDED_)
