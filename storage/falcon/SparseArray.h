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

#ifndef _SPARSE_ARRAY_H_
#define _SPARSE_ARRAY_H_

#include <memory.h>

template <typename T, uint width> class SparseArray
{
#ifdef _DEBUG
#undef THIS_FILE
	const char *THIS_FILE;
#endif

public:
	SparseArray()
		{
#ifdef _DEBUG
		THIS_FILE=__FILE__;
#endif
		level = 0;
		tree = NULL;
		
		for (uint n = 0, val = 1; n < 10; ++n, val *= width)
			counts[n] = val;
		};
	
	~SparseArray()
		{
		if (tree)
			deleteTree(level, tree);
		};
	
	static void deleteTree(int lvl, void **vector)
		{
		if (--lvl >= 0)
			for (uint n = 0; n < width; ++n)
				if (vector[n])
					deleteTree(lvl, (void**) vector[n]);
					
		delete [] vector;
		};
	
	T get (uint index, T nullValue = 0)
		{
		if (level == 0  && index < width && tree)
			return ((T*) tree)[index];
	
		uint idx = index;
		void **vector = tree;
			
		for (uint lvl = level; lvl > 0; --lvl)
			{
			uint n = idx / counts[lvl];
			
			if (n >= width)
				return nullValue;
			
			vector = (void**) (vector[n]);

			if (!vector)
				return nullValue;
			
			idx = idx % counts[lvl];
			}
		
		if (!vector)
			return nullValue;

		if (idx >= width)
			return nullValue;

		return ((T*) vector)[idx];			
		};

	void set (uint index, T element)
		{
		if (level == 0 && index < width)
			{
			if (!tree)
				{
				tree = (void**) new T[width];
				memset(tree, 0, sizeof(T) * width);
				}
		
			((T*) tree)[index] = element;
			
			return;
			}
			
		// Allocate any new levels as we go
		
		while ( index / counts[level] >= width)
			{
			void **oldTree = tree;
			tree = new void*[width];
			memset(tree, 0, sizeof(void*) * width);
			tree[0] = oldTree;
			++level;
			}

		void **vector = tree;
		uint idx = index;
		
		for (uint lvl = level; lvl > 0; --lvl)
			{
			uint n = idx / counts[lvl];
			void **nextVector = (void**) vector[n];

			if (!nextVector)
				{
				if (level == 1)
					{
					nextVector = (void**) new T [width];
					memset(nextVector, 0, sizeof(T) * width);
					}
				else
					{
					nextVector = new void* [width];
					memset(nextVector, 0, sizeof(void*) * width);
					}

				vector[n] = nextVector;
				}

			vector = nextVector;
			idx = idx % counts [lvl];
			}

		((T*) vector)[idx] = element;
		};
	
		

protected:
	uint	level;
	uint	counts[10];
	void	**tree;
};

#endif
