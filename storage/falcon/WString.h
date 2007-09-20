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

// WString.h: interface for the WString class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WSTRING_H__D4F96B36_CF3E_11D2_AB65_0000C01D2301__INCLUDED_)
#define AFX_WSTRING_H__D4F96B36_CF3E_11D2_AB65_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifdef _WIN32
#include <wchar.h>
typedef wchar_t			WCHAR;
#else
typedef unsigned short WCHAR;
#endif

class JString;

class WString  
{
public:
	static JString getString (const WCHAR *name, int length);
	WString (const WCHAR *str);
	WString (const char *string);
	WString();
	WString (WString& source);
	void setString(const WCHAR * wString, int len);
	int length();
	operator JString ();
	WString& operator = (const WCHAR *str);
	WString& operator = (const char *str);
	operator const WCHAR*();
	void assign (const WCHAR *str);
	void assign (const char *str);
	void release();
	~WString();

	static int length (const WCHAR* string);
	static int hash (const WCHAR *string, int length, int hashSize);

	WCHAR	*string;
};

#endif // !defined(AFX_WSTRING_H__D4F96B36_CF3E_11D2_AB65_0000C01D2301__INCLUDED_)
