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

#include "Engine.h"
#include "MySQLCollation.h"
#include "IndexKey.h"
#include "Value.h"

MySQLCollation::MySQLCollation(JString collationName, void *arg)
{
	name = collationName;
	charset = arg;
	padChar = falcon_get_pad_char(charset);
	isBinary = (0 != falcon_cs_is_binary(charset));
	mbMaxLen = falcon_get_mbmaxlen(charset);
	minSortChar = falcon_get_min_sort_char(charset);
}

MySQLCollation::~MySQLCollation(void)
{
}

int MySQLCollation::compare (Value *value1, Value *value2)
{
	const char *string1 = value1->getString();
	const char *string2 = value2->getString();
	uint len1 = value1->getStringLength();
	uint len2 = value2->getStringLength();

	// If the string is not BINARY, truncate the string of all 0x00, padChar, and minSortChar

	if (!isBinary)
		{
		len1 = computeKeyLength(len1, string1, padChar, minSortChar);
		len2 = computeKeyLength(len2, string2, padChar, minSortChar);
		}

	return falcon_strnncoll(charset, string1, len1, string2, len2, false);
}

int MySQLCollation::makeKey (Value *value, IndexKey *key, int partialKey, int maxKeyLength)
{
	if (partialKey > maxKeyLength )
		partialKey = maxKeyLength;

	char temp[MAX_PHYSICAL_KEY_LENGTH];
	int srcLen;

	if (partialKey)
		{
		srcLen = value->getTruncatedString(partialKey, temp);
		srcLen = falcon_strntrunc(charset, partialKey, temp, srcLen);
		}
	else
		srcLen = value->getString (sizeof(temp), temp);

	if (!isBinary)
		srcLen = computeKeyLength(srcLen, temp, padChar, minSortChar);

	// Since some collations make dstLen > srcLen, be sure dstLen is < partialKey.

	int dstLen = falcon_strnxfrmlen(charset, temp, srcLen, partialKey, MAX_PHYSICAL_KEY_LENGTH);
	int len = falcon_strnxfrm(charset, (char*) key->key, dstLen, maxKeyLength, temp, srcLen);
	key->keyLength = len;
	
	return len;
}

const char *MySQLCollation::getName ()
{
	return name;
}

bool MySQLCollation::starting (const char *string1, const char *string2)
{
	NOT_YET_IMPLEMENTED;
	return false;
}

bool MySQLCollation::like (const char *string, const char *pattern)
{
	NOT_YET_IMPLEMENTED;
	return false;
}

char MySQLCollation::getPadChar(void)
{
	return padChar;
}

int MySQLCollation::truncate(Value *value, int partialLength)
{
	const char *string = value->getString();
	int len = value->getStringLength();

	if (falcon_cs_is_binary(charset))
		len = MIN(len, partialLength);
	else
		len = falcon_strntrunc(charset, partialLength, string, len);

	value->truncateString(len);

	return len;
}
