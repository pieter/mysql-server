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

// NBitmap.h: interface for the NBitmap class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NBITMAP_H__02AD6A63_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_NBITMAP_H__02AD6A63_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "NNode.h"

class Index;
class IndexNode;
class IndexKey;

class NBitmap : public NNode  
{
public:
	virtual bool isUniqueIndex();
	virtual void prettyPrint (int level, PrettyPrint *pp);
	bool isRedundantIndex (NBitmap *node);
	virtual bool isRedundantIndex(LinkedList *indexes);
	virtual NNode* clone();
	void makeKey (Statement * statement, NNode **exprs, IndexKey *indexKey);
	virtual Bitmap* evalInversion (Statement *statement);
	NBitmap(CompiledStatement *statement, Index *idx, int numberNodes, NNode **min, NNode **max);
	virtual ~NBitmap();

	int		nodeCount;
	int		values;			// number of values for equality
	Index	*index;
	NNode	**minNodes;
	NNode	**maxNodes;
	bool	partial;
	bool	equality;
	bool	redundant;
};

#endif // !defined(AFX_NBITMAP_H__02AD6A63_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
