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

// Filter.h: interface for the Filter class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILTER_H__84FD197C_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_FILTER_H__84FD197C_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Stream.h"
#include "InversionFilter.h"
#include "InversionWord.h"

class Value;

class Filter : public InversionFilter
{
public:
	virtual InversionWord* getWord();
	const char* nextSegment();
	virtual void start();
	virtual int getWord (int bufferLength, char *buffer);
	Filter(int tableId, int fieldId, int recordId, Value *value);
	virtual ~Filter();

	int				wordNumber;
	InversionWord	word;
	Stream			stream;
	Segment			*segment;
	const char		*p;
	const char		*endSegment;
	bool			html;
};

#endif // !defined(AFX_FILTER_H__84FD197C_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
