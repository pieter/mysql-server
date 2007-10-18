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

// RSet.h: interface for the RSet class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RSET_H__3E3DE313_DB93_4640_95CE_41D339395A2F__INCLUDED_)
#define AFX_RSET_H__3E3DE313_DB93_4640_95CE_41D339395A2F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Engine.h"
#include "ResultSet.h"


class RSet  
{
public:
	void close();
	RSet (ResultSet *results);
	RSet();
	virtual ~RSet();
	void operator =(ResultSet *results);

	inline ResultSet* operator ->()
		{
		return resultSet;
		}

	inline operator ResultSet* ()
		{ return resultSet; }

	ResultSet	*resultSet;
};

#endif // !defined(AFX_RSET_H__3E3DE313_DB93_4640_95CE_41D339395A2F__INCLUDED_)
