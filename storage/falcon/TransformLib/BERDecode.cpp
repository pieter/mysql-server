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

// BERDecode.cpp: implementation of the BERDecode class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include "Engine.h"
#include "BERDecode.h"
#include "BERException.h"
#include "BERObject.h"
#include "Transform.h"
//#include "TransformException.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


BERDecode::BERDecode(BERDecode *parent)
{
	buffer = NULL;
	ptr = parent->content;
	end = ptr + parent->contentLength;
	if (parent->tagNumber == BIT_STRING)
		*ptr++;
	level = 0;
}


BERDecode::BERDecode(int length, const UCHAR *gook)
{
	buffer = NULL;
	ptr = gook;
	end = ptr + length;
	level = 0;
}

BERDecode::BERDecode(Transform *src)
{
	buffer = NULL;
	setSource(src);
	level = 0;
}


BERDecode::~BERDecode()
{
	delete [] buffer;
}

UCHAR BERDecode::getOctet()
{
	if (ptr >= end)
		{
		if (source)
			{
			int len = source->get(sizeof(buffer), buffer);
			ptr = buffer;
			end = buffer + len;
			}
		if (ptr >= end)
			throw BERException("key overrun");
		}
	
	return *ptr++;	
}


int BERDecode::getLength()
{
	// Parse tagLength

	UCHAR c = getOctet();
	int length = 0;

	if (c == 0x80)
		definiteLength = false;
	else
		{
		definiteLength = true;

		if (c & 0x80)
			for (int count = c & 0x7f; count--;)
				length = (length << 8) + getOctet();
		else
			length = c;
		}
	
	return length;
}

int BERDecode::getType()
{
	tagClass = getOctet();
	int type = 0;

	if ((tagClass & TAG_MASK) == TAG_MASK)
		for (;;)
			{
			type = 0;
			UCHAR c = getOctet();
			type = (type << 7) + (c & TAG_MASK);
			if (!(c & 0x80))
				break;
			}
	else
		type = tagClass & TAG_MASK;

	return type;
}

void BERDecode::setSource(Transform *src)
{
	delete buffer;
	source = src;
	int length = source->getLength();
	ptr = buffer = new UCHAR [length];
	end = ptr + source->get(length, buffer);
}

bool BERDecode::next()
{
	if (ptr >= end)
		return false;

	tagNumber = getType();
	contentLength = getLength();
	content = ptr;
	ptr += contentLength;

	return true;
}

void BERDecode::next(int requiredTagNumber)
{
	if (!next())
		throw BERException("expected BER object");

	if (tagNumber != requiredTagNumber)
		throw BERException("expected BER object %d, got %d", requiredTagNumber, tagNumber);
}

void BERDecode::pushDown()
{
	if (level + 2 >= MAX_LEVELS)
		throw BERException("max decode levels exceeded");

	state[level++] = ptr;
	state[level++] = end;
	ptr = content;
	end = ptr + contentLength;

	if (tagNumber == BIT_STRING)
		++ptr;
}

void BERDecode::popUp()
{
	if (level == 0)
		throw BERException("decode stack underflow");

	end = state[--level];
	ptr = state[--level];
}

void BERDecode::print()
{
	const UCHAR *start = ptr;

	if (next())
		print(0);

	ptr = start;
}


void BERDecode::print(int level)
{
	JString identifier = getTagString(tagNumber);
	printf ("%*s %s length %d", level * 3, "", (const char*) identifier, contentLength);

	switch (tagNumber)
		{
		case OBJECT_TAG:
			{
			const UCHAR *p = content;
			const UCHAR *end = p + contentLength;
			UCHAR c = *p++;
			printf (" (%d, %d", c / 40, c % 40);

			while (p < end)
				{
				int n = 0;

				while (p < end)
					{
					c = *p++;
					n = n * 128 + (c & 0x7f);
					if (!(c & 0x80))
						{
						printf(", %d", n);
						break;
						}
					}
				}
			printf (")\n");
			}
			break;

		case INTEGER:
			if (contentLength <= 4)
				printf(" (%d)\n", getInteger());
			else
				{
				for (int n = 0; n < 5; ++n)
					printf (" %.2x,", content[n]);
				printf(" ...\n");
				}
			break;

		case SEQUENCE:
		case BIT_STRING:
		case OCTET_STRING:
			printf("\n");
			pushDown();
			++level;
			while (next())
				print(level);
			popUp();
			break;

		default:
			printf("\n");
		}
}

JString BERDecode::getTagString(int tagNumber)
{
	char temp[128];
	const char *identifier = temp;

	switch (tagNumber)
		{
		case INTEGER:		identifier = "Integer"; break;
		case BIT_STRING:	identifier = "BitString"; break;
		case OCTET_STRING:	identifier = "OctetString"; break;
		case NULL_TAG:		identifier = "Null"; break;
		case OBJECT_TAG:	identifier = "Object"; break;
		case SEQUENCE:		identifier = "Sequence"; break;
		case SET:			identifier = "Set"; break;
		case PRINTABLE:		identifier = "Printable"; break;
		case T61String:		identifier = "T61String"; break;
		case IA5String:		identifier = "IA5String"; break;
		case UTCTime:		identifier = "UTCTime"; break;

		default:
			sprintf(temp, "Unknown tag %d", tagNumber);
		}

	return identifier;
}

int BERDecode::getRawLength()
{
	return end - ptr;
}

int BERDecode::getInteger()
{
	int value = 0;

	for (int n = 0; n < contentLength; ++n)
		value = (value << 8) | content[n];

	return value;
}
