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

// CollationCaseless.cpp: implementation of the CollationCaseless class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "CollationCaseless.h"
#include "Value.h"
#include "Index.h"
#include "IndexKey.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CollationCaseless::CollationCaseless()
{
	for (int n = 0; n < 256; ++n)
		caseTable [n] = n;

	for (int c = 'a'; c <= 'z'; ++c)
		caseTable [c] = c - 'a' + 'A';
}

/***
CollationCaseless::~CollationCaseless()
{

}
***/

int CollationCaseless::compare(Value *value1, Value *value2)
{
	Type t1 = value1->getType();
	Type t2 = value2->getType();
	
	if (MIN (t1, t2) < String ||
	    MAX (t1, t2) > Varchar)
		return value1->compare (value2);

	const char *p = value1->getString();
	const char *q = value2->getString();
	int l1 = value1->getStringLength();
	int l2 = value2->getStringLength();
	int l = MIN (l1, l2);
	int n;

	for (n = 0; n < l; ++n)
		{
		int c = caseTable [(int) *p++] - caseTable [(int) *q++];
		
		if (c)
			return c;
		}

	int c;

	if (n < l1)
		{
		for (; n < l1; ++n)
			if ( (c = caseTable [(int) *p++] - ' ') )
				return c;
				
		return 0;
		}

	if (n < l2)
		{
		for (; n < l2; ++n)
			if ((c = ' ' - caseTable [(int) *q++]))
				return c;
		}

	return 0;
}

int CollationCaseless::makeKey(Value *value, IndexKey *key, int partialKey, int maxKeyLength)
{
	int l = value->getString (sizeof(key->key), (char*) key->key);
	
	UCHAR *p = key->key;
	UCHAR *q = p + l;

	while (q > p && q [-1] == ' ')
		--q;

	l = q - p;

	for (int n = 0; n < l; ++n)
		p [n] = caseTable [p [n]];

	key->keyLength = l;

	return l;
}

const char* CollationCaseless::getName()
{
	return "CASE_INSENSITIVE";
}

bool CollationCaseless::starting(const char *string1, const char *string2)
{
	for (const char *p = string1, *q = string2; *q;)
		if (caseTable [(int) *p++] != caseTable [(int) *q++])
			return false;

	return true;
}


bool CollationCaseless::like(const char *string, const char *pattern)
{
	char c;
	const char *s = string;

	for (const char *p = pattern; (c = *p++); ++s)
		if (c == '%')
			{
			if (!*p)
				return true;
			for (; *s; ++s)
				if (like (s, pattern+1))
					return true;
			return false;
			}
		else if (!*s)
			return false;
		else if (c != '_' && caseTable [(int) c] != caseTable [(int) *s])
			return false;

	return (!c && !*s);
}

char CollationCaseless::getPadChar(void)
{
	return ' ';
}

int CollationCaseless::truncate(Value *value, int partialLength)
{
	value->truncateString(partialLength);
	return value->getStringLength();
}
