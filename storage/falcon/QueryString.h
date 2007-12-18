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

// QueryString.h: interface for the QueryString class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_QUERYSTRING_H__30CBCF61_B24B_11D2_AB5E_0000C01D2301__INCLUDED_)
#define AFX_QUERYSTRING_H__30CBCF61_B24B_11D2_AB5E_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define QUERY_HASH_SIZE		11

class Parameter;
struct WCString;

class QueryString  
{
public:
	void release();
	void addRef();
	Parameter* findParameter (const WCString *name);
	const char* getParameter (const WCString *name);
	QueryString();
	const char* getParameter (const char *name, const char *defaultValue);
	static int getParameter (const char * queryString, const char * name, int bufferLength, char * buffer);
	QueryString(const char *string);
	virtual ~QueryString();
	void setString (const char *string);
	char getHex (const char **ptr);

	const char		*queryString;
	Parameter		*variables [QUERY_HASH_SIZE];
	volatile INTERLOCK_TYPE	useCount;
};

#endif // !defined(AFX_QUERYSTRING_H__30CBCF61_B24B_11D2_AB5E_0000C01D2301__INCLUDED_)
