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

// SortMerge.h: interface for the SortMerge class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SORTMERGE_H__CDE71282_956A_11D4_98F4_0000C01D2301__INCLUDED_)
#define AFX_SORTMERGE_H__CDE71282_956A_11D4_98F4_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SortStream.h"

struct SortParameters;

class SortMerge : public SortStream  
{
public:
	virtual void prepare();
	virtual SortRecord* fetch();
	SortMerge(SortParameters *sortParameters, SortStream *strm1, SortStream *strm2);
	virtual ~SortMerge();

	SortParameters	*parameters;
	SortStream		*stream1;
	SortStream		*stream2;
	SortRecord		*record1;
	SortRecord		*record2;
};

#endif // !defined(AFX_SORTMERGE_H__CDE71282_956A_11D4_98F4_0000C01D2301__INCLUDED_)
