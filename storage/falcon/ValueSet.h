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

// ValueSet.h: interface for the ValueSet class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_VALUESET_H__D0D35F0F_BD7A_4D21_A33F_EB355F5D7F97__INCLUDED_)
#define AFX_VALUESET_H__D0D35F0F_BD7A_4D21_A33F_EB355F5D7F97__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Value;
class Index;
CLASS(Field);

class ValueSet  
{
public:
	void reset();
	bool hasValue (Value *value);
	void addValue (Value *value);
	ValueSet(Index *index);
	virtual ~ValueSet();

	Index	*index;
	Field	*field;
};

#endif // !defined(AFX_VALUESET_H__D0D35F0F_BD7A_4D21_A33F_EB355F5D7F97__INCLUDED_)
