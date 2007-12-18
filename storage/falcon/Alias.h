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

// Alias.h: interface for the Alias class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ALIAS_H__AC49FDD7_6F8F_4128_8605_66355A2672B1__INCLUDED_)
#define AFX_ALIAS_H__AC49FDD7_6F8F_4128_8605_66355A2672B1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Alias  
{
public:
	Alias(JString keyString, JString valueString);
	virtual ~Alias();

	JString		key;
	JString		value;
	Alias		*collision;
	Alias		*collision2;
};

#endif // !defined(AFX_ALIAS_H__AC49FDD7_6F8F_4128_8605_66355A2672B1__INCLUDED_)
