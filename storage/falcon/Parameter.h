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

// Parameter.h: interface for the Parameter class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey


#if !defined(AFX_PARAMETER_H__BD560E65_B194_11D3_AB9F_0000C01D2301__INCLUDED_)
#define AFX_PARAMETER_H__BD560E65_B194_11D3_AB9F_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

struct WCString;


class Parameter
{
public:
	bool isNamed (const WCString *parameter);
	void setValue (const char *newValue);
	Parameter (Parameter *nxt, const char *nam, int namLen, const char *val, int valLen);
	virtual ~Parameter();

	int			nameLength;
	char		*name;
	int			valueLength;
	char		*value;
	Parameter	*next;
	Parameter	*collision;
};

#endif // !defined(AFX_PARAMETER_H__BD560E65_B194_11D3_AB9F_0000C01D2301__INCLUDED_)
