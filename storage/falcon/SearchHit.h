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

// SearchHit.h: interface for the SearchHit class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SEARCHHIT_H__84FD197D_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_SEARCHHIT_H__84FD197D_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class SearchHit  
{
public:
	void setWordMask(int mask);
	 SearchHit (int32 table, int32 record, double goodness);
	SearchHit();
	//virtual ~SearchHit();

	SearchHit	*next;
	SearchHit	*prior;
	int32		tableId;
	int32		recordNumber;
	double		score;
	int32		hits;
	int32		wordScore;
};

#endif // !defined(AFX_SEARCHHIT_H__84FD197D_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
