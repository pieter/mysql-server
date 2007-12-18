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

/*
 *	PROGRAM:		Virtual Data Manager
 *	MODULE:			JString.cpp
 *	DESCRIPTION:	Transportable flexible string
 *
 * copyright (c) 1997 - 2000 by James A. Starkey
 */

#include <memory.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
//#include "Engine.h"
#include "JString.h"

#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif

#ifndef ASSERT
#define ASSERT(expr)
#endif

#define ISLOWER(c)			(c >= 'a' && c <= 'z')
#define UPPER(c)			((ISLOWER (c)) ? c - 'a' + 'A' : c)

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


JString::JString ()
{
/**************************************
 *
 *		J S t r i n g
 *
 **************************************
 *
 * Functional description
 *		Initialize string object.
 *
 **************************************/

	string = NULL;
}

JString::JString (const char *stuff)
{
/**************************************
 *
 *		J S t r i n g
 *
 **************************************
 *
 * Functional description
 *		Initialize string object.
 *
 **************************************/

	string = NULL;
	setString (stuff);
}

JString::JString (const JString& source)
{
/**************************************
 *
 *		J S t r i n g
 *
 **************************************
 *
 * Functional description
 *		Copy constructor.
 *
 **************************************/

	if ((string = source.string))
		++((int*) string)[-1];
}

JString::~JString ()
{
/**************************************
 *
 *		~ J S t r i n g
 *
 **************************************
 *
 * Functional description
 *		Initialize string object.
 *
 **************************************/

	//Error::validateHeap ("JString::~JString 1");
	release();
	//Error::validateHeap ("JString::~JString 1");
}

void JString::append (const char* stuff)
{
/**************************************
 *
 *		a p p e n d
 *
 **************************************
 *
 * Functional description
 *		Append string.
 *
 **************************************/

	if (!string)
		{
		setString (stuff);
		
		return;
		}

	char *temp = string;
	++((int*) temp)[-1];
	int l1 = (int) strlen (temp);
	int	l2 = (int) strlen (stuff);
	release();
	alloc (l1 + l2);
	memcpy (string, temp, l1);
	memcpy (string + l1, stuff, l2);
	temp -= sizeof (int);

	if (--((int*) temp)[0] == 0)
		delete [] temp;
}

void JString::setString (const char* stuff)
{
/**************************************
 *
 *		s e t S t r i n g
 *
 **************************************
 *
 * Functional description
 *		Append string.
 *
 **************************************/

	if (stuff)
		setString (stuff, (int) strlen (stuff));
	else
		release();
}

void JString::Format (const char* stuff, ...)
{
/**************************************
 *
 *		f o r m a t
 *
 **************************************
 *
 * Functional description
 *		Append string.
 *
 **************************************/
	va_list	args;
	va_start (args, stuff);
	char	temp [1024];

	//Error::validateHeap ("JString::Format 1");
	//int length = 
	vsnprintf (temp, sizeof (temp) - 1, stuff, args);
	setString (temp);
	//Error::validateHeap ("JString::Format 2");
}

JString& JString::operator = (const char *stuff)
{
/**************************************
 *
 *		o p e r a t o r   c h a r =
 *
 **************************************
 *
 * Functional description
 *		Return string as string.
 *
 **************************************/

	setString (stuff);

	return *this;
}

JString& JString::operator = (const JString& source)
{
/**************************************
 *
 *		o p e r a t o r   c h a r =
 *
 **************************************
 *
 * Functional description
 *		Return string as string.
 *
 **************************************/

	release();

	if ((string = source.string))
		++((int*) string)[-1];

	return *this;
}

JString& JString::operator+= (const char *stuff)
{
/**************************************
 *
 *		o p e r a t o r   c h a r + =
 *
 **************************************
 *
 * Functional description
 *		Return string as string.
 *
 **************************************/

	append (stuff);

	return *this;
}

JString& JString::operator+= (const JString& stuff)
{
/**************************************
 *
 *		o p e r a t o r   c h a r + =
 *
 **************************************
 *
 * Functional description
 *		Return string as string.
 *
 **************************************/

	append (stuff.string);

	return *this;
}

JString operator + (const JString& string1, const char *string2)
{
/**************************************
 *
 *		o p e r a t o r   c h a r +
 *
 **************************************
 *
 * Functional description
 *		Return string as string.
 *
 **************************************/
 
	JString	s = string1;
	s.append (string2);

	return s;
}

void JString::release ()
{
/**************************************
 *
 *		r e l e a s e
 *
 **************************************
 *
 * Functional description
 *		Clean out string.
 *
 **************************************/

	//Error::validateHeap ("JString::release");

	if (!string)
		return;

	string -= sizeof (int);

	if (--((int*) string)[0] == 0)
		delete [] string;

	string = NULL;
}

bool JString::operator ==(const char * stuff)
{
	if (string)
		return strcmp (string, stuff) == 0;

	return strcmp ("", stuff) == 0;
}


bool JString::operator !=(const char * stuff)
{
	if (string)
		return strcmp (string, stuff) != 0;

	return strcmp ("", stuff) != 0;
}


JString JString::before(char c)
{
	const char *p;

	for (p = string; *p && *p != c;)
		++p;

	if (!*p)
		return *this;

	JString stuff;
	stuff.setString (string, (int) (p - string));

	return stuff;
}

const char* JString::after(char c)
{
	const char *p;

	for (p = string; *p && *p++ != c;)
		;

	return p;
}

bool JString::IsEmpty()
{
return !string || !string [0];
}

int JString::hash(const char * string, int tableSize)
{
	int	value = 0, c;

	while ((c = (unsigned) *string++))
		{
		if (ISLOWER (c))
			c -= 'a' - 'A';
			
		value = value * 11 + c;
		}

	if (value < 0)
		value = -value;

	return value % tableSize;
}


int JString::hash(int tableSize)
{
	if (!string)
		return 0;

	return hash (string, tableSize);
}

void JString::setString(const char * source, int length)
{
	alloc (length);
	memcpy (string, source, length);
}

int JString::findSubstring(const char * string, const char * sub)
{
    for (const char *p = string; *p; ++p)
		{
		const char *s, *q;
		
		for (q = p, s = sub; *s && *q == *s; ++s, ++q)
			;
			
		if (!*s)
			return (int) (p - string);
		}

	return -1;
}

JString JString::upcase(const char * source)
{
	JString string;
	int len = (int) strlen (source);
	string.alloc (len);
	
	for (int n = 0; n < len; ++n)
		{
		char c = source [n];
		string.string [n] = UPPER (c);
		}

	return string;
}



void JString::alloc(int length)
{
	release();
	string = new char [length + 1 + sizeof (int)];
	*((int*) string) = 1;
	string += sizeof (int);
	string [length] = 0;
}

bool JString::equalsNoCase(const char * string2)
{
	if (!string)
		return string2 [0] == 0;

	const char *p;

	for (p = string; *p && *string2; ++p, ++string2)
		if (UPPER (*p) != UPPER (*string2))
			return false;

	return *p == *string2;
}


int JString::length()
{
	if (!string)
		return 0;

	const char *p;

	for (p = string; *p; ++p)
		;

	return (int) (p - string);
}

JString::JString(const char * source, int length)
{
	string = NULL;
	setString (source, length);
}

char* JString::getBuffer(int length)
{
	alloc (length);

	return string;
}

void JString::releaseBuffer()
{
}

