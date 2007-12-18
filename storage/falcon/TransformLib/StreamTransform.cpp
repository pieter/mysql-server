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

// StreamTransform.cpp: implementation of the StreamTransform class.
//
//////////////////////////////////////////////////////////////////////

#include "StreamTransform.h"
#include "Stream.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StreamTransform::StreamTransform(Stream *inputStream)
{
	stream = inputStream;
	reset();
}

StreamTransform::~StreamTransform()
{

}

unsigned int StreamTransform::getLength()
{
	return stream->totalLength;
}

unsigned int StreamTransform::get(unsigned int bufferLength, UCHAR *buffer)
{
	int len = MIN((int) bufferLength, stream->totalLength - offset);

	if (len == 0)
		return 0;

	stream->getSegment(offset, len, buffer);
	offset += len;

	return len;
}

void StreamTransform::reset()
{
	offset = 0;
}
