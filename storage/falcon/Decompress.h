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

// Decompress.h: interface for the Decompress class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DECOMPRESS_H__7183FE59_E15A_11D2_AB69_0000C01D2301__INCLUDED_)
#define AFX_DECOMPRESS_H__7183FE59_E15A_11D2_AB69_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

//#define NATIVE_DECOMPRESS

enum State {
	BLOCK,
	//LENGTH,
	DONE,
	};

enum Mode {
	TYPE,			// need to get compression type (initial mode)
	LENGTH,
	TABLE,
	BTREE,
	FINISHED,
	};

class JavaArchiveFile;
class Stream;

typedef unsigned int UINT;

class Decompress  
{
public:
	static void free (void *opaque, void *memory);
	static void* alloc (void *opaque, unsigned int items, unsigned int size);
	void decompress (UCHAR *compressed, int compressedLength, Stream *decompressed);
	int skipCompressed (JavaArchiveFile *archive);
	void decompress (UCHAR *compressed, int compressedLength, UCHAR *uncompressed, int uncompressedLength);
	Decompress();
	virtual ~Decompress();

#ifdef NATIVE_DECOMPRESS
	State error (const char *msg);
	State getBlock();
	ULONG getBits (int nBits);
	void eatBits (int nBits);
	void needBits (int n);
	void decompress2 (UCHAR * compressed, int compressedSize, UCHAR * uncompressed, int uncompressedSize);

	UCHAR	*nextIn;
	UCHAR	*endIn;
	UCHAR	*nextOut;
	UCHAR	*endOut;
	ULONG	bitBuffer;
	int		bits;			// bits in bit buffer
	UINT	index;
	UINT	hlist;			// number of literal/length codes - 257
	UINT	hdist;			// number of distance codes - 1
	UINT	hclen;			// number of code length codes - 4
	UINT	bb;				// bit length tree depth
#endif
	UINT	*bLengths;
};

#endif // !defined(AFX_DECOMPRESS_H__7183FE59_E15A_11D2_AB69_0000C01D2301__INCLUDED_)
