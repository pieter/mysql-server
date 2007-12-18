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

// FsbSort.h: interface for the FsbSort class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FSBSORT_H__02AD6A5F_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_FSBSORT_H__02AD6A5F_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Fsb.h"

enum SortType {
	order,					// Sort with explicit sort order
	distinct,				// Get rid of duplicates
	simple,					// Just sort; don't worry about direction (group by)
	};

class Sort;
class CompiledStatement;
class Collation;
struct SortParameters;

class FsbSort : public Fsb  
{
public:
	virtual void prettyPrint (int level, PrettyPrint *pp);
	virtual void getStreams (int **ptr);
	virtual void close (Statement *statement);
	virtual Row* fetch (Statement *statement);
	virtual void open (Statement *statement);
	FsbSort(CompiledStatement *statement, NNode *expr, Fsb *source, SortType type);
	virtual ~FsbSort();

	Fsb			*stream;
	NNode		*node;
	SortType	type;
	SortParameters		*parameters;
	Collation	**collations;
	int			sortSlot;
	int			numberKeys;
	int			numberContexts;
	int			contextIds [32];
};

#endif // !defined(AFX_FSBSORT_H__02AD6A5F_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
