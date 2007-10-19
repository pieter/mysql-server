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

// DESKeyTransform.cpp: implementation of the DESKeyTransform class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "DESKeyTransform.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DESKeyTransform::DESKeyTransform(bool encodeFlag, Transform *src)
{
	source = src;
	reset();
}

DESKeyTransform::~DESKeyTransform()
{

}

unsigned int DESKeyTransform::getLength()
{
	return sizeof(key);
}

unsigned int DESKeyTransform::get(unsigned int bufferLength, UCHAR *buffer)
{
	if (offset)
		{
		unsigned int len = sizeof(key) - offset;
		if (len <= 0)
			return 0;
		if (len > bufferLength)
			len = bufferLength;
		memcpy(buffer, key + offset, len);
		offset += len;
		return len;
		}

	memset (key, 0, sizeof(key));
	UCHAR input[128];

	for (int n = 0;;)
		{
		int len = source->get(sizeof(input), input);

		if (len == 0)
			break;

		for (UCHAR *p = input, *end = p + len; p < end; ++n)
			{
			UCHAR c = *p++;
			if ((n % 16) < 8)
				key [n % 8] ^= c << 1;
			else
				{
				c = (UCHAR) (((c << 4) & 0xf0) | ((c >> 4) & 0x0f));
				c = (UCHAR) (((c << 2) & 0xcc) | ((c >> 2) & 0x33));
				c = (UCHAR) (((c << 1) & 0xaa) | ((c >> 1) & 0x55));
				key [7 - (n % 8)] ^= c;
				}
			}
		}

	int offset = MIN(sizeof(key), bufferLength);
	memcpy(buffer, key, offset);

	return offset;
}

void DESKeyTransform::reset()
{
	offset = 0;
	source->reset();
}
