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

// DESTransform.cpp: implementation of the DESTransform class.
//
//////////////////////////////////////////////////////////////////////

#include "DESTransform.h"
#include "TransformException.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DESTransform::DESTransform(int operatingMode, Transform *src)
{
	mode = operatingMode;
	source = src;
	reset();
}

DESTransform::~DESTransform()
{

}

unsigned int DESTransform::getLength()
{
	int len = source->getLength();

	return (len + 7) / 8 * 8;
}

unsigned int DESTransform::get(unsigned int bufferLength, UCHAR *buffer)
{
	UCHAR	*p = buffer;
	UCHAR	*endBuffer = buffer + bufferLength;
	UCHAR input[8];

	if (ptr < end)
		{
		int len = MIN(bufferLength, (unsigned int)(end - ptr));
		memcpy (p, ptr, len);
		ptr += len;
		p += len;
		if (p == end)
			return p - buffer;
		}

	while (p < endBuffer)
		{
		int len = source->get(sizeof(input), input);
		if (len == 0)
			break;
		if (len < sizeof(input))
			memset(input + len, 0, sizeof(input) - len);

		switch(mode)
			{
			case DES_encrypt:
				des_ecb_encrypt(input, block, &schedule);
				break;

			case DES_decrypt:
				if (len < sizeof(input))
					throw TransformException("partial DES block");
				des_ecb_decrypt(input, block, &schedule);
				break;
			}

		len = MIN(endBuffer - p, sizeof(block));
		memcpy(p, block, len);
		p += len;

		if (len < sizeof (block))
			{
			ptr = block + len;
			break;
			}
		}

	return p - buffer;
}

void DESTransform::setKey(Transform *keyTransform)
{
	UCHAR key[8];
	int len = keyTransform->get(sizeof(key), key);
	setKey(key);
}


void DESTransform::setKey(const UCHAR key[8])
{
	int ret = des_setup((UCHAR*) key, 8, 0, &schedule);
}

void DESTransform::reset()
{
	end = block + sizeof(block);
	ptr = end;
	source->reset();
}
