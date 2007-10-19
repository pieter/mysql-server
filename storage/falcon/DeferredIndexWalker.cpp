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

#include "Engine.h"
#include "DeferredIndex.h"
#include "DeferredIndexWalker.h"
#include "IndexKey.h"
#include "Index.h"

DeferredIndexWalker::DeferredIndexWalker(DeferredIndex *deferredIdx, IndexKey *indexKey, int searchFlags)
{
	deferredIndex = deferredIdx;
	currentNode = NULL;
	DIBucket *bucket = (DIBucket*) deferredIndex->root;
	nodePending = true;
	bool isPartial = (searchFlags & Partial) == Partial;

	if (indexKey)
		{
		uint slot;

		for (int level = deferredIndex->levels; level > 0; --level)
			{
			states[level].bucket = bucket;
				
			for (slot = 0;; ++slot)
				{
				int n = deferredIndex->compare(indexKey, bucket->references[slot].node, isPartial);
				
				if (n <= 0)
					{
					if (slot)
						--slot;

					break;
					}

				if (slot + 1 == bucket->count)
					break;
				}

			
			states[level].slot = slot;
			bucket = bucket->references[slot].bucket;
			slot = 0;
			}
		
		DILeaf *leaf = (DILeaf*) bucket;

		for (slot = 0; slot < leaf->count; ++slot)
			{
			int n = deferredIndex->compare(indexKey, leaf->nodes[slot], isPartial);

			if (n <= 0)
				break;
			}

		states[0].bucket = bucket;
		states[0].slot = slot;
		
		if (slot >= leaf->count)
			nodePending = false;
		}
	else
		for (int level = deferredIndex->levels; level >= 0; --level)
			{
			states[level].bucket = bucket;
			states[level].slot = 0;
			bucket = bucket->references[0].bucket;
			}

}

DeferredIndexWalker::~DeferredIndexWalker(void)
{
}

DINode* DeferredIndexWalker::next(void)
{
	DILeaf *leaf = (DILeaf*) states[0].bucket;
	uint slot = states[0].slot;

	// Special case just getting started
	
	if (nodePending)
		{
		nodePending = false;

		return currentNode = (slot >= leaf->count) ? NULL : leaf->nodes[slot];
		}

	++slot;
	DIBucket *bucket;
		
	for (;;)
		{
		// If there's another node at leaf level, just use it

		if (slot < leaf->count)
			{
			states[0].bucket = (DIBucket*) leaf;
			states[0].slot = slot;

			return (currentNode = leaf->nodes[slot]);
			}

		// Back up a level (or levels) and come back down

		int level = 0;

		while (++level)
			{
			if (level > deferredIndex->levels)
				return NULL;

			bucket = states[level].bucket;
			slot = states[level].slot + 1;

			if (slot < bucket->count)
				break;
			}

		for (; level; --level)
			{
			states[level].bucket = bucket;
			states[level].slot = slot;
			bucket = bucket->references[slot].bucket;
			slot = 0;
			}
	
		leaf = (DILeaf*) bucket;
		}
}
