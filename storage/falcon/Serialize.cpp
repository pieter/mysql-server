/* Copyright (C) 2008 MySQL AB

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
#include "Serialize.h"
#include "SQLError.h"

// The following has been cloned from SerialLogRecord.cpp.  Unlike SerialLogRecord, however,
// Serialization (at this point, at least) does not need to be stable from release to release,
// so cloning makes more sense that trying to maintain a single source copy.

#define GET_BYTE	((data < end) ? *data++ : getByte())

static const int FIXED_INT_LENGTH	= 5;
static const int LOW_BYTE_FLAG		= 0x80;

#define BYTES_POS(n)	((n < (1<<6)) ? 1 : \
						(n < (1<<13)) ? 2 : \
						(n < (1<<20)) ? 3 : \
						(n < (1<<27)) ? 4 : 5)

#define BYTES_NEG(n)	((n >= -(1<<6)) ? 1 : \
						(n >= -(1<<13)) ? 2 : \
						(n >= -(1<<20)) ? 3 : \
						(n >= -(1<<27)) ? 4 : 5)

#define BYTE_COUNT(n)	((n >= 0) ? BYTES_POS(n) : BYTES_NEG(n))

#define BYTES_POS64(n)	((n < (1<<6)) ? 1 : \
						(n < (1<<13)) ? 2 : \
						(n < (1<<20)) ? 3 : \
						(n < (1<<27)) ? 4 : \
						(n < ((int64) 1<<34)) ? 5 : \
						(n < ((int64) 1<<41)) ? 6 : \
						(n < ((int64) 1<<48)) ? 7 : \
						(n < ((int64) 1<<55)) ? 8 : 9)

#define BYTES_NEG64(n)	((n >= -(1<<6)) ? 1 : \
						(n >= -(1<<13)) ? 2 : \
						(n >= -(1<<20)) ? 3 : \
						(n >= -(1<<27)) ? 4 : \
						(n >= -((int64) 1<<34)) ? 5 : \
						(n >= -((int64) 1<<41)) ? 6 : \
						(n >= -((int64) 1<<48)) ? 7 : \
						(n >= -((int64) 1<<55)) ? 8 : 9)

#define BYTE_COUNT64(n)	((n >= 0) ? BYTES_POS64(n) : BYTES_NEG64(n))


static const UCHAR lengthShifts[] = { 0, 7, 14, 21, 28, 35, 42, 49, 56 };

Serialize::Serialize(void)
{
	data = NULL;
	end = NULL;
	segment = NULL;
}

Serialize::~Serialize(void)
{
}

void Serialize::putInt(int value)
{
	int count = BYTE_COUNT(value);
	UCHAR *data = reserve(6);
	UCHAR *p = data;

	while (--count > 0)
		*p++ = (value >> (lengthShifts [count])) & 0x7f;

	*p++ = value | LOW_BYTE_FLAG;
	release(p - data);
}

void Serialize::putInt64(int64 value)
{
	int64 count = BYTE_COUNT64(value);
	UCHAR *data = reserve(10);
	UCHAR *p = data;

	while (--count > 0)
		*p++ = (UCHAR) (value >> (lengthShifts [count])) & 0x7f;

	*p++ = ((UCHAR) value) | LOW_BYTE_FLAG;
	release(p - data);
}

void Serialize::putData(uint length, const UCHAR* data)
{
}

int Serialize::getInt(void)
{
	UCHAR c = GET_BYTE;
	int number = (c & 0x40) ? -1 : 0;

	for (;;)
		{
		number = (number << 7) | (c & 0x7f);

		if (c & LOW_BYTE_FLAG)
			break;

		c = GET_BYTE;
		}

	return number;
}

int64 Serialize::getInt64(void)
{
	UCHAR c = GET_BYTE;
	int64 number = (c & 0x40) ? -1 : 0;

	for (;;)
		{
		number = (number << 7) | (c & 0x7f);

		if (c & LOW_BYTE_FLAG)
			break;

		c = GET_BYTE;
		}

	return number;
}

int Serialize::getDataLength(void)
{
	return 0;
}

void Serialize::getData(uint length, UCHAR* buffer)
{
}

UCHAR* Serialize::reserve(uint length)
{
	if (!current || (int) length > currentLength - current->length)
		allocSegment (length);

	char *p = current->tail + current->length;

	return (UCHAR*) p;
}

/***
void Serialize::release(uint actualLength)
{
}
***/

UCHAR Serialize::getByte(void)
{
	for (;;)
		{
		if (segment)
			{
			if (data < end)
				return *data++;

			segment = segment->next;
			}
		else
			segment = segments;
		
		if (!segment)
			throw SQLError(RUNTIME_ERROR, "deserialization overflow error");
			
		data = (UCHAR*) segment->address;
		end = data + segment->length;
		}
}
