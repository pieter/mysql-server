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

// Decompress.cpp: implementation of the Decompress class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "Decompress.h"
#include "Log.h"
#include "Stream.h"
#include "zlib.h"
#include "zutil.h"

#ifdef ENGINE
#include "JavaArchiveFile.h"
#endif

#ifdef NATIVE_DECOMPRESS
static const UINT bOrder [] = {
	/* Order of the bit length code lengths */
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
	};
#endif


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Decompress::Decompress()
{
	bLengths = NULL;
}

Decompress::~Decompress()
{
	if (bLengths)
		delete [] bLengths;
}

void Decompress::decompress(UCHAR * compressed, int compressedSize, UCHAR * uncompressed, int uncompressedSize)
{
	uint32 uncomprLen = uncompressedSize;
	z_stream stream;
	int err;

	stream.next_in = (Bytef*)compressed;
	stream.avail_in = (uInt)compressedSize;

	stream.next_out = uncompressed;
	stream.avail_out = (uInt)uncompressedSize;
	stream.zalloc = alloc;
	stream.zfree = free;

	//err = inflateInit(&stream);
	err = inflateInit2(&stream, -DEF_WBITS);

	int err2 = inflate(&stream, Z_FINISH);
	if (err2 != Z_STREAM_END) 
		{
		inflateEnd(&stream);
		err = err2;
		}
	//*destLen = stream.total_out;

	err2 = inflateEnd(&stream);

#ifdef ENGINE
	ASSERT (compressedSize == (int) stream.total_in);
	ASSERT (uncompressedSize == (int) stream.total_out);
#endif

#ifdef NATIVE_DECOMPRESS
	UCHAR *altBuffer = new UCHAR [uncompressedSize];
	decompress2 (compressed, compressedSize, altBuffer, uncompressedSize);
	delete [] altBuffer;
#endif
}


#ifdef NATIVE_DECOMPRESS
void Decompress::decompress2(UCHAR * compressed, int compressedSize, UCHAR * uncompressed, int uncompressedSize)
{
	nextIn = compressed;
	endIn = nextIn + compressedSize;
	nextOut = uncompressed;
	endOut = nextOut + uncompressedSize;
	bitBuffer = 0;
	bits = 0;
	State state = BLOCK;

	while (state != DONE)
		switch (state)
			{
			case BLOCK:
				state = getBlock();
				break;

			default:
				error ("not yet implemented");
				break;
			}

}

void Decompress::needBits(int n)
{
	while (bits < n)
		{
		bitBuffer |= (*nextIn++) << bits;
		bits += 8;
		}
}

void Decompress::eatBits(int nBits)
{
	bitBuffer >>= nBits;
	bits -= nBits;
}

ULONG Decompress::getBits(int nBits)
{
	while (bits < nBits)
		{
		bitBuffer |= (*nextIn++) << bits;
		bits += 8;
		}

	int n = bitBuffer & ((1 << nBits) - 1);
	bitBuffer >>= nBits;
	bits -= nBits;

	return n;
}

State Decompress::getBlock()
{
	Mode mode = TYPE;
	UINT header;

	while (mode != FINISHED)
		switch (mode)
			{
			case TYPE:
				header = getBits (3);
				switch (header >> 1)
					{
					case 0:
						mode = LENGTH;
						eatBits (bits & 0x7);
						break;

					case 1:
						return error ("not yet implemented");
						
					case 2:
						mode = TABLE;
						break;

					case 3:
						return error ("bad block type");

					default:
						return error ("not yet implemented");
					}
				break;

			case TABLE:
				{
				//t = 258 + (t & 0x1f) + ((t >> 5) & 0x1f);
				hlist = getBits (5);
				hdist = getBits (5);
				hclen = getBits (4);
				UINT size = 258 + hlist + hdist;
				bLengths = new UINT [size];
				memset (bLengths, 0, size * sizeof (UINT));
				index = 0;
				mode = BTREE;
				}
				//break;		// drop through to BTREE

			case BTREE:
				// get code lengths for code length alphabets
				while (index < 4 + hclen)
					bLengths [bOrder [index++]] = getBits (3);
				while (index < 19)
					bLengths [bOrder [index++]] = 0;
				bb = 7;
				break;

			default:
				error ("not yet implemented");
				return DONE;
			}
				
	return DONE;
}

State Decompress::error(const char * msg)
{
	Log::debug ("%s\n", msg);

	return DONE;
}
#endif

int Decompress::skipCompressed(JavaArchiveFile * archive)
{
#ifdef ENGINE
	Bytef input [1024], output [1024];
	z_stream stream;
	memset (&stream, 0, sizeof (stream));
	stream.zalloc = alloc;
	stream.zfree = free;
	int err = inflateInit2(&stream, -DEF_WBITS);
	UINT count;

	for (;;)
		{
		if (!stream.next_in || (stream.avail_out > 0 && stream.avail_in == 0))
			{
			count = archive->read (input, sizeof (input), false);
			stream.next_in = input;
			stream.avail_in = count;
			}
		stream.next_out = output;
		stream.avail_out = sizeof (output);
		int err2 = inflate(&stream, 0);
		switch (err2)
			{
			case Z_STREAM_END:
				if (count > stream.avail_in)
					archive->backup (stream.avail_in);
				inflateEnd (&stream);
				return stream.total_in;

			case Z_OK:
				break;

			case Z_BUF_ERROR:
				break;

			default:
				ASSERT (false);
			}
		}
#endif

	return -1;
}

void Decompress::decompress(UCHAR *compressed, int compressedSize, Stream *decompressed)
{
	z_stream stream;
	stream.msg = NULL;
	stream.next_in = (Bytef*)compressed;
	stream.avail_in = (uInt)compressedSize;
	stream.zalloc = alloc;
	stream.zfree = free;
	char uncompressed [2048];
	ULONG uncomprLen =  sizeof (uncompressed);

	stream.zalloc = alloc;
	stream.zfree = free;

	int err = inflateInit2(&stream, -DEF_WBITS);
	//int err = inflateInit(&stream);

	for (;;)
		{
		stream.next_out = (Bytef*) uncompressed;
		stream.avail_out = uncomprLen;
		int err = inflate(&stream, Z_SYNC_FLUSH);
		int length = uncomprLen - stream.avail_out;
		
		if (length > 0)
			decompressed->putSegment(length, uncompressed, true);
			
		if (err == Z_STREAM_END)
			break;
			
		if (err != Z_OK)
			{
			if (stream.msg)
				Log::log("inflate failed: %s\n", stream.msg);
			else
				Log::log("inflated failed with %d\n", err);
				
			break;
			}
		}

	/***
	while (err2 != Z_STREAM_END) 
		{
		inflateEnd(&stream);
		err = err2;
		}
	***/

	err = inflateEnd(&stream);
}

void* Decompress::alloc(void *opaque, unsigned int items, unsigned int size)
{
	return new UCHAR [items * size];
}

void Decompress::free(void *opaque, void *memory)
{
	delete [] (UCHAR*) memory;
}
