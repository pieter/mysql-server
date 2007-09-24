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

// Stream.cpp: implementation of the Stream class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <string.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include "Engine.h"
#include "Stream.h"
#include "StreamSegment.h"
#include "SQLError.h"
#include "Blob.h"
#include "Log.h"

#ifdef ENGINE
#include "Record.h"
#else
#undef ASSERT
#define ASSERT(a)
#endif

#ifdef _WIN32
#define vsnprintf	_vsnprintf
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Stream::Stream(int minSegmentSize)
{
	segments = NULL;
	current = NULL;
	totalLength = 0;
	minSegment = minSegmentSize;
	copyFlag = true;
	useMalloc = false;
}

Stream::~Stream()
{
	clear();
}

void Stream::putCharacter(char c)
{
	if (!segments || current->length >= currentLength)
		allocSegment (MAX (100, minSegment));

	current->address [current->length] = c;
	++current->length;
	++totalLength;
}

void Stream::putSegment(int length, const char *ptr, bool copy)
{
	ASSERT (length >= 0);

	if (length == 0)
		return;

	const char *address = (char*) ptr;
	totalLength += length;

	if (!segments)
		{
		if (copyFlag = copy)
			{
			allocSegment (MAX (length, minSegment));
			current->length = length;
			memcpy (current->address, address, length);
			}
		else
			{
			current = segments = &first;
			current->length = length;
			current->address = (char*) address;
			current->next = NULL;
			}
		}
	else if (copyFlag)
		{
		int l = currentLength - current->length;
		
		if (l > 0)
			{
			int l2 = MIN (l, length);
			memcpy (current->address + current->length, address, l2);
			current->length += l2;
			length -= l2;
			address += l2;
			}
			
		if (length)
			{
			allocSegment (MAX (length, minSegment));
			current->length = length;
			memcpy (current->address, address, length);
			}
		}
	else
		{
		allocSegment (0);
		current->address = (char*) address;
		current->length = length;
		}
}


int Stream::getSegment(int offset, int len, void * ptr)
{
	int n = 0;
	int length = len;
	char *address = (char*) ptr;

	for (Segment *segment = segments; segment; n += segment->length, segment = segment->next)
		if (n + segment->length >= offset)
			{
			int off = offset - n;
			int l = MIN (length, segment->length - off);
			memcpy (address, segment->address + off, l);
			address += l;
			length -= l;
			offset += l;
			
			if (!length)
				break;
			}

	return len - length;
}

void Stream::setSegment(Segment * segment, int length, void* address)
{
	segment->length = length;
	totalLength += length;

	if (copyFlag)
		{
		segment->address = new char [length];
		memcpy (segment->address, address, length);
		}
	else
		segment->address = (char*) address;
}


void Stream::setMinSegment(int length)
{
	minSegment = length;
}

Segment* Stream::allocSegment(int tail)
{
	Segment *segment;
	int length = tail;

	if (!current && tail <= FIXED_SEGMENT_SIZE)
		{
		segment = &first;
		length = FIXED_SEGMENT_SIZE;
		}
	else if (useMalloc)
		segment = (Segment*) malloc (sizeof (Segment) - FIXED_SEGMENT_SIZE + tail);
	else
		segment = (Segment*) new char [sizeof (Segment) - FIXED_SEGMENT_SIZE + tail];

	segment->address = segment->tail;
	segment->next = NULL;
	segment->length = 0;
	currentLength = length;

	if (current)
		{
		current->next = segment;
		current = segment;
		}
	else
		segments = current = segment;

	return segment;
}

int Stream::getSegment(int offset, int len, void * ptr, char delimiter)
{
	int n = 0;
	int length = len;
	char *address = (char*) ptr;

	for (Segment *segment = segments; segment; n += segment->length, segment = segment->next)
		if (n + segment->length >= offset)
			{
			int off = offset - n;
			int l = MIN (length, segment->length - off);
			char *p = segment->address + off;
			
			for (char *end = p + l; p < end;)
				{
				char c = *address++ = *p++;
				--length;
				if (c == delimiter)
					return len - length;
				}
				
			if (!length)
				break;
			}

	return len - length;
}

void Stream::putSegment(const char * string)
{
	if (string [0])
		putSegment((int) strlen (string), string, true);
}

char* Stream::getString()
{
	char *string = new char [totalLength + 1];
	getSegment (0, totalLength, string);
	string [totalLength] = 0;

	return string;
}

#ifdef ENGINE
void Stream::compress(int length, void * address)
{
	//printShorts ("Original data", (length + 1) / 2, (short*) address);
	Segment *segment = allocSegment (length + 5);
	short *q = (short*) segment->address;
	short *p = (short*) address;
	short *end = p + (length + 1) / 2;
	short *yellow = end - 2;
	*q++ = length;

	while (p < end)
		{
		short *start = ++q;

		while (p < end && 
			   ((p >= yellow) || (p [0] != p [1] || p [1] != p [2])))
			*q++ = *p++;

		IPTR n = q - start;

		if (n)
			start [-1] = (short) -n;
		else
			--q;

		if (p >= end)
			break;

		start = p++;

		while (p < end && *p == *start)
			++p;

		n = p - start;
		*q++ = (short) n;
		*q++ = *start;
		}

	totalLength = segment->length = (int) ((char*) q - segment->address);
}

char* Stream::decompress(int tableId, int recordNumber)
{
	char *data = NULL;
	short *q = NULL, *limit = NULL;
	int run = 0;
	decompressedLength = 0;

	for (Segment *segment = segments; segment; segment = segment->next)
		{
		if (segment->length == 0)
			continue;

		short *p = (short*) segment->address;
		short *end = (short*) (segment->address + segment->length);

		if (decompressedLength == 0)
			{
			decompressedLength = *p++;

			if (decompressedLength <= 0)
				{
				Log::log ("corrupted record, table %d, record %d, decompressed length %d\n", 
						  tableId, recordNumber, decompressedLength);
						  
				throw SQLEXCEPTION (RUNTIME_ERROR, "corrupted record, table %d, record %d, decompressed length %d", 
									tableId, recordNumber, decompressedLength);
				}

			int len = (decompressedLength + 1) / 2 * 2;
			data = ALLOCATE_RECORD (len);
			limit = (short*) (data + decompressedLength);

			if (decompressedLength & 1)
				++limit;

			q = (short*) data;
			}
		while (p < end)
			{
			short n = *p++;
//#ifdef ENGINE
			if (n == 0 && run == 0)
				{
				Log::log ("corrupted record (zero run), table %d, record %d\n", tableId, recordNumber);
				printShorts ("Zero run", (segment->length + 1)/2, (short*) segment->address);
				printChars ("Zero run", segment->length, segment->address);
				}
//#endif
			if (run > 0)
				for (; run; --run)
					*q++ = n;
			else if (run < 0)
				{
				*q++ = n;
				++run;
				}
			else
				{
				run = n;
				
				if (q + run > limit)
					{
//#ifdef ENGINE
					Log::log ("corrupted record (overrun), table %d, record %d\n", tableId, recordNumber);
					printShorts ("Compressed", (segment->length + 1)/2, (short*) segment->address);
					printChars ("Compressed", segment->length, segment->address);
//#endif
					if (q == limit)
						return data;
						
					throw SQLEXCEPTION (RUNTIME_ERROR, "corrupted record, table %d, record %d", tableId, recordNumber);
					}
				}
			}
		}
	
	//printShorts ("Decompressed", (decompressedLength + 1) / 2, (short*) data);	
	return data;
}

void Stream::printShorts(const char * msg, int length, short * data)
{
	Log::debug ("%s", msg);

	for (int n = 0; n < length; ++n)
		{
		if (n % 10 == 0)
			Log::debug ("\n    ");
			
		Log::debug ("%d, ", data [n]);
		}

	Log::debug ("\n");
}

void Stream::printChars(const char * msg, int length, const char * data)
{
	Log::debug ("%s", msg);

	for (int n = 0; n < length; ++n)
		{
		if (n % 50 == 0)
			Log::debug ("\n    ");
			
		char c = data [n];
		
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			putchar (c);
		else
			putchar ('.');
		}

	Log::debug ("\n");
}
#endif

void Stream::clear()
{
	Segment *segment;

	if (useMalloc)
		while ( (segment = segments) )
			{
			segments = segment->next;
			
			if (segment != &first)
				free (segment);
			}
	else
		while ( (segment = segments) )
			{
			segments = segment->next;
			
			if (segment != &first)
				delete [] (char*) segment;
			}

	current = NULL;
	totalLength = 0;
}

void Stream::putSegment(int length, const WCHAR *chars)
{
	if (length == 0)
		return;

	totalLength += length;
	const WCHAR *wc = chars;

	if (!segments)
		{
		allocSegment (MAX (length, minSegment));
		current->length = length;
		}
	else
		{
		int l = currentLength - current->length;
		
		if (l > 0)
			{
			int l2 = MIN (l, length);
			char *p = current->address + current->length;
			
			for (int n = 0; n < l2; ++n)
				*p++ = (char) *wc++;

			current->length += l2;
			length -= l2;
			}
			
		if (length)
			{
			allocSegment (MAX (length, minSegment));
			current->length = length;
			}
		}

	char *p = current->address;

	for (int n = 0; n < length; ++n)
		*p++ = (char) *wc++;

}

int Stream::getLength()
{
	return totalLength;
}


int Stream::getSegmentLength(int offset)
{
	int n = 0;

	for (Segment *segment = segments; segment; segment = segment->next)
		{
		if (offset >= n && offset < n + segment->length)
			return n + segment->length - offset;
			
		n += segment->length;
		}

	return 0;
}

void* Stream::getSegment(int offset)
{
	int n = 0;

	for (Segment *segment = segments; segment; segment = segment->next)
		{
		if (offset >= n && offset < n + segment->length)
			return segment->address + offset - n;
			
		n += segment->length;
		}

	return NULL;
}

void Stream::putSegment(Stream * stream)
{
	ASSERT(stream);

	if (stream->totalLength == 0)
		return;

	// The source stream has had an error fetching a blob. Propogate the error

	if (stream->totalLength < 0)
		{
		ASSERT (totalLength == 0);
		totalLength = stream->totalLength;
		
		return;
		}

	StreamSegment seg = stream;

	if (current)
		for (int len = currentLength - current->length; len > 0 && seg.available;)
			{
			int l = MIN (len, seg.available);
			putSegment (l, seg.data, true);
			seg.advance (l);
			len -= l;
			}

	if (seg.remaining > 0)
		seg.copy (alloc (seg.remaining), seg.remaining);
#ifdef ENGINE
	else if (seg.remaining < 0)
		Log::debug ("Stream::putSegment: apparent blob overflow\n");
#endif
}


void Stream::putSegment(Blob * blob)
{
	Stream *stream = blob->getStream();

	if (stream)
		putSegment(stream);
}

void Stream::putSegment(Clob * blob)
{
	Stream *stream = blob->getStream();

	if (stream)
		putSegment (stream);
}

JString Stream::getJString()
{
	JString string;
	char *p = string.getBuffer (totalLength);

	for (Segment *segment = segments; segment; segment = segment->next)
		{
		memcpy (p, segment->address, segment->length);
		p += segment->length;
		}

	string.releaseBuffer ();

	return string;
}

char* Stream::alloc(int length)
{
	totalLength += length;

	if (!current || length > currentLength - current->length)
		allocSegment (length);

	char *p = current->tail + current->length;
	current->length += length;

	return p;
}

void Stream::setMalloc(bool flag)
{
	if (!segments)
		useMalloc = flag;
}

void Stream::format(const char *pattern, ...)
{
	va_list		args;
	va_start	(args, pattern);
	char		temp [1024];

	if (vsnprintf (temp, sizeof (temp) - 1, pattern, args) < 0)
		temp [sizeof (temp) - 1] = 0;

	putSegment (temp);
}

void Stream::truncate(int length)
{
	int n = 0;

	for (Segment *segment = segments; segment; segment = segment->next)
		{
		if (length >= n && length < n + segment->length)
			{
			current = segment;
			current->length = length - n;
			totalLength = length;

			while ( (segment = current->next) )
				{
				current->next = segment->next;
				delete segment;
				}
				
			return;
			}
			
		n += segment->length;
		}

}

int Stream::compare(Stream *stream)
{
	for (int offset = 0;;)
		{
		int length1 = getSegmentLength(offset);
		int length2 = stream->getSegmentLength(offset);
		
		if (length1 == 0)
			if (length2)
				return -1;
			else
				return 0;
				
		if (length2 == 0)
			return 1;
			
		int length = MIN (length1, length2);
		const char *p1 = (const char*) getSegment (offset);
		const char *p2 = (const char*) stream->getSegment (offset);
		
		for (const char *end = p1 + length; p1 < end;)
			{
			int n = *p1++ - *p2++;
			
			if (n)
				return n;
			}
		offset += length;
		}
}

void Stream::transfer(Stream *stream)
{
	Segment *segment = stream->segments;

	if (segment == &stream->first)
		{
		putSegment(segment->length, segment->address, true);

		if (current && segment->next)
			current->next = segment->next;

		totalLength += stream->totalLength - segment->length;
		segment->next = NULL;
		}
	else
		{
		if (current)
			current->next = segment;
		else
			current = segment;

		totalLength += stream->totalLength;
		stream->segments = NULL;
		}

	while (current && current->next)
		current = current->next;

	currentLength = 0;
}

void Stream::indent(int spaces)
{
	for (int n = 0; n < spaces; ++n)
		putCharacter(' ');
}
