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

// ValueEx.h: interface for the ValueEx class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_VALUEEX_H__4E465281_D67B_11D3_98CB_0000C01D2301__INCLUDED_)
#define AFX_VALUEEX_H__4E465281_D67B_11D3_98CB_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Value.h"

class Record;

class ValueEx : public Value  
{
public:
	 ValueEx (Record *record, int fieldId);
	const char* getString ();
	ValueEx();
	virtual ~ValueEx();

	char	*temp;
};

#endif // !defined(AFX_VALUEEX_H__4E465281_D67B_11D3_98CB_0000C01D2301__INCLUDED_)
