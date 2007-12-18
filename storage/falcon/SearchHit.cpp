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

// SearchHit.cpp: implementation of the SearchHit class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "SearchHit.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SearchHit::SearchHit()
{
	hits = 0;
}

/***
SearchHit::~SearchHit()
{

}
***/

SearchHit::SearchHit(int32 table, int32 record, double goodness)
{
	tableId = table;
	recordNumber = record;
	score = goodness;
	hits = 1;
	wordScore = 0;
}

void SearchHit::setWordMask(int wordMask)
{
	int count = 0;
	
	for (int mask = 1; wordMask; mask <<= 1)
		if (wordMask & mask)
			{
			++count;
			wordMask &= ~mask;
			}

	double boost = (count - wordScore) * 20;
	//printf ("hit %d:%d:\t%g + %g -> %g\n", tableId, recordNumber, score, boost, boost + score);
	score += boost;
	wordScore = count;
}
