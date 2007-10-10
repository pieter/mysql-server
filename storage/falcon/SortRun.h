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

// SortRun.h: interface for the SortRun class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SORTRUN_H__84FD1963_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_SORTRUN_H__84FD1963_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SortStream.h"

struct SortParameters;

class SortRun : public SortStream
{
public:
	virtual void prepare();
	void sort (SortParameters *sortParameters, bool flushDuplicate);
	virtual SortRecord* fetch();
	bool add (SortRecord *record);
	SortRun(int len);
	virtual ~SortRun();

	int			size;
	int			length;
	int			n;
	SortRecord	**records;
};

#endif // !defined(AFX_SORTRUN_H__84FD1963_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
