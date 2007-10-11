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

// WString.cpp: implementation of the WString class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "WString.h"

static const WCHAR wcharNull [1] = { 0 };

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

WString::WString()
{
	string = NULL;
}

WString::~WString()
{
	release();
}

WString::WString(const char * str)
{
	string = NULL;
	assign (str);
}

WString::WString(const WCHAR * str)
{
	string = NULL;
	assign (str);
}

void WString::release()
{
	if (!string)
		return;

	--string;

	if (--string [0] > 0)
		{
		string = NULL;
		return;
		}

	delete [] string;
	string = NULL;
}

void WString::assign(const char * str)
{
	if (string)
		release();

	if (!str)
		return;

	UIPTR l1 = strlen (str);
	string = new WCHAR [l1 + 2];
	*string++ = 1;
	
	for (WCHAR *q = string; (*q++ = *str++);)
		;
}

void WString::assign(const WCHAR * str)
{
	if (string)
		release();

	if (!str)
		return;

	int l1 = length (str);
	string = new WCHAR [l1 + 2];
	*string++ = 1;

	for (WCHAR *p = string; (*p++ = *str++);)
		;
}

WString::operator const WCHAR *()
{
	return (string) ? string : wcharNull;
}

WString& WString::operator =(const char * str)
{
	assign (str);

	return *this;
}

WString& WString::operator =(const WCHAR * str)
{
	assign (str);

	return *this;
}

WString::WString(WString & source)
{
	if ((string = source.string))
		++(string [-1]);
}

WString::operator JString()
{
	JString str;
	int l = length();

	if (l)
		{
		char *buffer = str.getBuffer (l + 1);
		char *q = buffer;
		for (const WCHAR *p = string; *p;)
			*q++ = (char) *p++;
		*q = 0;
		str.releaseBuffer();
		}

	return str;
}

int WString::length()
{
	return length (string);
}

int WString::hash(const WCHAR * string, int length, int tableSize)
{
	int	value = 0;
    const WCHAR *end = string + length;

	for (const WCHAR *p = string; p < end;)
		{
		WCHAR c = *p++;
		if (ISLOWER (c))
			c -= 'a' - 'A';
		value = value * 11 + c;
		}

	if (value < 0)
		value = -value;

	return value % tableSize;
}

void WString::setString(const WCHAR * wString, int len)
{
	release();

	if (len == 0)
		return;

	string = new WCHAR [len + 2];
	*string++ = 1;

	for (int n = 0; n < len; ++n)
		string [n] = (WCHAR) wString [n];

	string [len] = 0;
}

int WString::length(const WCHAR * string)
{
	int count = 0;

	for (const WCHAR *p = string; *p++;)
		++count;

	return count;
}

JString WString::getString(const WCHAR * name, int length)
{
	JString string;
	char *buffer = string.getBuffer (length + 1);
	char *p = buffer;

	for (; length; --length)
		*p++ = (char) *name++;

	*p = 0;
	string.releaseBuffer ();

	return string;
}
