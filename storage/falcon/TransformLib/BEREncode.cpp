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

// BEREncode.cpp: implementation of the BEREncode class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "BEREncode.h"
#include "BERDecode.h"
#include "BERItem.h"
#include "BERObject.h"
#include "BERException.h"
#include "StringTransform.h"
#include "HexTransform.h"
#include "DecodeTransform.h"

static const char *encode = "0123456789ABCDEF";
static const UCHAR zero[] = { 0 };
static const int RSAPrivKey [] = { 1, 2, 840, 113549, 1, 1, 1 };

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

BEREncode::BEREncode()
{
	ptr = NULL;
}


BEREncode::BEREncode(int bufferLength, UCHAR *buffer)
{
	ptr = start = buffer;
	end = ptr + bufferLength;
}

BEREncode::~BEREncode()
{

}

void BEREncode::put(UCHAR c)
{
	if (ptr)
		{
		if (ptr >= end)
			throw BERException("outflow during binary encode");
		*ptr++ = c;
		}
	else
		{
		stream.putCharacter(encode [c >> 4]);
		stream.putCharacter(encode [c & 0x0F]);
		}
}

/***
JString BEREncode::wrapPrivKey(const char *key)
{
	DecodeTransform<StringTransform, HexTransform> transform(key);
	BERDecode rawKey (&transform);
	BERItem *item = new BERItem(0, SEQUENCE, 0, NULL);
	item->addChild(new BERItem(0, INTEGER, 1, zero));
	BERItem *algorithm = new BERItem(0, SEQUENCE, 0, NULL);
	item->addChild(algorithm);
	algorithm->addChild(new BERObject(0, sizeof(RSAPrivKey) /sizeof(RSAPrivKey[0]), RSAPrivKey));
	algorithm->addChild(new BERItem(0, NULL_TAG, 0, NULL));
	item->addChild(new BERItem(0, OCTET_STRING, rawKey.keyLength, rawKey. key));
	//item->prettyPrint(0);
	BEREncode encode;
	item->encode(&encode);

	return encode.getJString();
}
***/

void BEREncode::dump(const char *filename)
{
	FILE *file = fopen(filename, "w");
	JString string = stream.getJString();
	const char *p = string;

	for (int remaining = stream.totalLength; remaining > 60; p += 60, remaining -= 60)
		fprintf (file, "    \"%.60s\"\n", p);

	fprintf (file, "    \"%s\";\n", p);
	fclose(file);
}

JString BEREncode::getJString()
{
	return stream.getJString();
}

int BEREncode::encodeObjectNumbers(int count, int *numbers, int bufferLength, UCHAR *buffer)
{
	UCHAR *p = buffer;
	UCHAR *end = p + bufferLength;

	if (count < 2 || bufferLength < 1)
		return -1;

	*p++ = numbers[0] * 40 + numbers[1];

	for (int n = 2; n < count; ++n)
		{
		int value = numbers[n];
		bool sig = false;
		for (int n = 28; n >= 0; n -= 7)
			{
			if (p >= end)
				return -1;
			UCHAR hunk = value >> n;
			if (n == 0)
				*p++ = hunk & ~0x80;
			else if (hunk || sig)
				{
				*p++ = hunk | 0x80;
				sig = true;
				}
			}
		}

	return p - buffer;
}

int BEREncode::getLength()
{
	return ptr - start;
}
