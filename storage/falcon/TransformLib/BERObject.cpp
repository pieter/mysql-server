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

// BERObject.cpp: implementation of the BERObject class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "BERObject.h"
#include "BERDecode.h"
#include "BEREncode.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


BERObject::BERObject(BERDecode *decode) : BERItem (decode)
{
	int *number = numbers;
	const UCHAR *p = content;
	const UCHAR *end = p + contentLength;
	UCHAR c = *p++;
	*number++ = c / 40;
	*number++ = c % 40;

	while (p < end)
		{
		int n = 0;

		while (p < end)
			{
			c = *p++;
			n = n * 128 + (c & 0x7f);
			if (!(c & 0x80))
				break;
			}

		*number++ = n;
		}

	count = number - numbers;
}

/***
BERObject::BERObject(BERDecode *decode, UCHAR itemClass, int itemNumber)
	: BERItem (decode, itemClass, itemNumber)
{
	int *number = numbers;
	const UCHAR *p = content;
	const UCHAR *end = p + contentLength;
	UCHAR c = *p++;
	*number++ = c / 40;
	*number++ = c % 40;

	while (p < end)
		{
		int n = 0;

		while (p < end)
			{
			c = *p++;
			n = n * 128 + (c & 0x7f);
			if (!(c & 0x80))
				break;
			}

		*number++ = n;
		}

	count = number - numbers;
}
***/

BERObject::BERObject(UCHAR itemClass, int valueCount, const int *values)
	: BERItem(itemClass, OBJECT_TAG, 0, NULL)
{
	count = valueCount;

	for (int n = 0; n < count; ++n)
		numbers [n] = values[n];

	content = NULL;
}

BERObject::~BERObject()
{

}

void BERObject::printContents()
{
	const char *sep = ": ";

	for (int n = 0; n < count; ++n)
		{
		printf ("%s%d", sep, numbers [n]);
		sep = ", ";
		}
}

int BERObject::getContentLength()
{
	int length = 1;

	for (int n = 2; n < count; ++n)
		length += getComponentLength(numbers [n]);

	return length;
}

void BERObject::encodeContent(BEREncode *encode, int length)
{
	encode->put(numbers[0] * 40 + numbers[1]);

	for (int n = 2; n < count; ++n)
		encodeComponent(encode, numbers[n]);
}
