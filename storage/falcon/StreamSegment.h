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

// StreamSegment.h: interface for the StreamSegment class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STREAMSEGMENT_H__11469FBB_A55A_4BB1_A96B_D95A4C75F0C7__INCLUDED_)
#define AFX_STREAMSEGMENT_H__11469FBB_A55A_4BB1_A96B_D95A4C75F0C7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Stream;

struct Segment;

class StreamSegment  
{
public:
	char* copy (void *target, int length);
	void advance (int size);
	void advance();
	void setStream (Stream *stream);
	StreamSegment(Stream *stream);
	virtual ~StreamSegment();

	int		available;
	int		remaining;
	char	*data;
	Segment	*segment;
};

#endif // !defined(AFX_STREAMSEGMENT_H__11469FBB_A55A_4BB1_A96B_D95A4C75F0C7__INCLUDED_)
