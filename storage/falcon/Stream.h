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

// Stream.h: interface for the Stream class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STREAM_H__02AD6A53_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_STREAM_H__02AD6A53_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "JString.h"
#include "WString.h"

#define FIXED_SEGMENT_SIZE		1024

struct Segment
    {
	int		length;
	char	*address;
	Segment	*next;
	char	tail [FIXED_SEGMENT_SIZE];
	};

class Blob;
class Clob;

class Stream  
{
public:
	void transfer(Stream *stream);
	int				compare (Stream *stream);
	void			truncate (int length);
	virtual void	format (const char *pattern, ...);
	void			setMalloc(bool flag);
	void			putSegment (int length, const WCHAR *chars);
	void			putSegment (Stream *stream);
	virtual void	putSegment (const char *string);
	virtual void	putSegment (int length, const char *address, bool copy);
	void			putSegment (Blob *blob);
	void			putSegment (Clob *blob);
	void			putCharacter (char c);

	virtual void	setSegment (Segment *segment, int length, void *address);
	virtual int		getSegment (int offset, int length, void* address);
	virtual int		getSegment (int offset, int len, void *ptr, char delimiter);
	void*			getSegment (int offset);
	int				getSegmentLength(int offset);

	JString			getJString();
	virtual char*	getString();
	void			clear();
	virtual int		getLength();
	virtual char*	alloc (int length);

	Segment*		allocSegment (int tail);
	void			setMinSegment (int length);

#ifdef ENGINE
	char*			decompress(int tableId, int recordNumber);
	void			compress (int length, void *address);
	void			printShorts (const char *msg, int length, short *data);
	void			printChars (const char *msg, int length, const char *data);
#endif

	Stream (int minSegmentSize = FIXED_SEGMENT_SIZE);
	virtual ~Stream();

	int		totalLength;
	int		minSegment;
	int		currentLength;
	int		decompressedLength;
	int		useCount;
	bool	copyFlag;
	bool	useMalloc;
	Segment	first;
	Segment	*segments;
	Segment *current;
	void indent(int spaces);
};

#endif // !defined(AFX_STREAM_H__02AD6A53_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
