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

// ZLibTransform.cpp: implementation of the ZLibTransform class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "ZLibTransform.h"
#include "Stream.h"
#include "TransformException.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ZLibTransform::ZLibTransform(bool encodeFlag, Transform *src)
{
	encode = encodeFlag;
	source = src;
	active = false;
	reset();
	zStream.zalloc = alloc;
	zStream.zfree = free;
}

ZLibTransform::~ZLibTransform()
{
	reset();
}

unsigned int ZLibTransform::get(unsigned int bufferLength, UCHAR *buffer)
{
	if (eof)
		return 0;

	int flush = false;

	if (encode)
		{
		zStream.next_out = buffer;
		zStream.avail_out = bufferLength;
		int err;
		
		if (!active)
			{
			err = deflateInit2(&zStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
								-DEF_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
			active = true;
			}

		int initialOut = zStream.total_out;

		for (;;)
			{
			if (!zStream.next_in || (zStream.avail_out > 0 && zStream.avail_in == 0))
				{
				unsigned int len = source->get(sizeof(temp), temp);
				if (len < sizeof(temp))
					flush = true;
				zStream.next_in = temp;
				zStream.avail_in = len;
				}

			int err2 = deflate(&zStream, flush);

			switch (err2)
				{
				case Z_BUF_ERROR:
				case Z_STREAM_END:
					residualLength = zStream.avail_in;
					inflateEnd (&zStream);
					active = false;
					eof = true;
					return zStream.total_out - initialOut;

				case Z_OK:
					if (zStream.avail_out == 0)
						return zStream.total_out - initialOut;
					break;

				default:
					throw TransformException ("zlib error %d", err2);
				}
			}
		}
	else
		{
		zStream.next_out = buffer;
		zStream.avail_out = bufferLength;
		int err;
		
		if (!active)
			{
			err = inflateInit2(&zStream, -DEF_WBITS);
			active = true;
			}

		int initialOut = zStream.total_out;

		for (;;)
			{
			if (!zStream.next_in || (zStream.avail_out > 0 && zStream.avail_in == 0))
				{
				unsigned int len = source->get(sizeof(temp), temp);
				if (len < sizeof(temp))
					flush = true;
				zStream.next_in = temp;
				zStream.avail_in = len;
				}

			int err2 = inflate(&zStream, flush);

			switch (err2)
				{
				case Z_BUF_ERROR:
				case Z_STREAM_END:
					residualLength = zStream.avail_in;
					inflateEnd (&zStream);
					active = false;
					eof = true;
					return zStream.total_out - initialOut;

				case Z_OK:
					if (zStream.avail_out == 0)
						return zStream.total_out - initialOut;
					break;

				default:
					throw TransformException ("zlib error %d", err2);
				}
			}
		}
}

unsigned int ZLibTransform::getLength()
{
	return source->getLength();
}

void ZLibTransform::reset()
{
	zStream.avail_in = 0;
	eof = false;

	if (active)
		{
		active = false;
		if (encode)
			deflateEnd (&zStream);
		else
			inflateEnd (&zStream);
		}

}

void* ZLibTransform::alloc(void *opaque, unsigned int items, unsigned int size)
{
	return new UCHAR [items * size];
}

void ZLibTransform::free(void *opaque, void *memory)
{
	delete [] (UCHAR*) memory;
}
