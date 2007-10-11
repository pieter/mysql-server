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

// Bitmap.h: interface for the Bitmap class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BITMAP_H__5DD7F233_A406_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_BITMAP_H__5DD7F233_A406_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Interlock.h"


#define CLUMP_BITS			5
#define INDEX_BITS			5
#define VECTOR_BITS			7
#define BITS_PER_CLUMP		32
#define CLUMPS				32
#define BITS_PER_SEGMENT	(CLUMPS * BITS_PER_CLUMP)
#define BITMAP_VECTOR_SIZE	128

typedef INTERLOCK_TYPE BitClump;

/* Bitmap Segment */

struct Bms {
    INTERLOCK_TYPE		count;
    BitClump	clump[CLUMPS];
	};


class Bitmap  
{
public:
	Bitmap();
	virtual ~Bitmap();

	void	orSegments (Bms *segment1, Bms *segment2);
	void	clear();
	void	clear (int32 number);
	bool	isSet (int32 number);
	void	orBitmap (Bitmap* bitmap);
	void	andBitmap (Bitmap *bitmap);
	int32	nextSet (int32 start);
	void	set (int32 number);
	bool	setSafe(int32 bitNumber);
	void	release();
	void	addRef();

	INTERLOCK_TYPE		count;

protected:
	void	decompose (int32 number, uint *indexes);
	void**	allocVector(void *firstItem);
	void	deleteVector (int lvl, void **vector);
	void	swap (Bitmap *bitmap);
	bool	andSegments (Bms *segment1, Bms *segment2);

	int32	unary;
	int		level;
	void	**vector;
	int		useCount;
};

#endif // !defined(AFX_BITMAP_H__5DD7F233_A406_11D2_AB5B_0000C01D2301__INCLUDED_)
