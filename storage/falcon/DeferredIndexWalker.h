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

#ifndef _DEFERRED_INDEX_WALKER_H_
#define _DEFERRED_INDEX_WALKER_H_

#include "DeferredIndex.h"

class DeferredIndex;
class IndexKey;
struct DINode;

class DeferredIndexWalker
{
public:
	DeferredIndexWalker(DeferredIndex *deferredIdx, IndexKey *indexKey, int searchFlags = 0);
	~DeferredIndexWalker(void);
	
	DINode		*next(void);

	DeferredIndex	*deferredIndex;
	DIState			states[DEFERRED_INDEX_MAX_LEVELS];
	DINode			*currentNode;
	bool			nodePending;
};

#endif

