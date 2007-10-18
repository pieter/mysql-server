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

// PStatement.h: interface for the PStatement class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PSTATEMENT_H__BE6A5E81_18DB_474D_B2FD_AC4A9AE7202F__INCLUDED_)
#define AFX_PSTATEMENT_H__BE6A5E81_18DB_474D_B2FD_AC4A9AE7202F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "PreparedStatement.h"


class PStatement  
{
public:
	void close();
	PStatement (PreparedStatement *stmt);
	PStatement();
	virtual ~PStatement();
	void operator =(PreparedStatement *stmt);

	inline PreparedStatement* operator ->()
		{
		return statement;
		}

	PreparedStatement	*statement;
};

#endif // !defined(AFX_PSTATEMENT_H__BE6A5E81_18DB_474D_B2FD_AC4A9AE7202F__INCLUDED_)
