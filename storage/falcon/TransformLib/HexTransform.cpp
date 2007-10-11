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

// HexTransform.cpp: implementation of the HexTransform class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "HexTransform.h"
#include "TransformException.h"

static const char *hexDigits = "0123456789ABCDEF";

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

HexTransform::HexTransform(bool encodeFlag, Transform *src)
{
	encode = encodeFlag;
	source = src;
}

HexTransform::~HexTransform()
{
}

unsigned int HexTransform::getLength()
{
	int len = source->getLength();

	return (encode) ? len * 2 : (len + 1) / 2;
}

unsigned int HexTransform::get(unsigned int bufferLength, UCHAR *buffer)
{
	UCHAR temp [1024];
	UCHAR *out = buffer;
	UCHAR *endBuffer = buffer + bufferLength;

	if (encode)
		{
		while (out < endBuffer)
			{
			int len = (int) source->get((unsigned int) MIN((endBuffer - out) / 2, (int) sizeof(temp)), temp);
			if (len == 0)
				break;
			for (const UCHAR *p = temp, *end = p + len; p < end;)
				{
				UCHAR c = *p++;
				*out++ = hexDigits[c >> 4];
				*out++ = hexDigits[c & 0xf];
				}
			}
		}
	else
		for (;;)
			{
			int len = (unsigned int) (endBuffer - out);

			if (len <= 1)
				break;

			if (!(len = (int)source->get(MIN(len * 2, (int) sizeof(temp)), temp)))
				break;

			for (const UCHAR *p = temp, *end = p + len; p < end;)
				{
				UCHAR c = hexDigit(*p++);
				*out++ = (c << 4) | hexDigit(*p++);
				}
			}

	return (unsigned int) (out - buffer);
}

UCHAR HexTransform::hexDigit(UCHAR c)
{
	if (c >= '0' && c <= '9')
		return c - '0';

	if (c >= 'a' && c <= 'z')
		return 10 + c - 'a';

	if (c >= 'A' && c <= 'Z')
		return 10 + c - 'A';

	throw TransformException ("illegal hex digit");
}

void HexTransform::reset()
{
	source->reset();
}
