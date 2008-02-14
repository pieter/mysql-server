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

// Bitmap.cpp: implementation of the Bitmap class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "Bitmap.h"

#ifndef ENGINE
#define ASSERT(n)
#endif

static int sizes [] = {
	0,
	BITS_PER_SEGMENT,
	BITS_PER_SEGMENT * BITMAP_VECTOR_SIZE,
	BITS_PER_SEGMENT * BITMAP_VECTOR_SIZE * BITMAP_VECTOR_SIZE,
	0x7FFFFFFF
	};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


Bitmap::Bitmap()
{
	level = 0;
	vector = NULL;
	count = 0;
	useCount = 1;
}

Bitmap::~Bitmap()
{
	if (vector)
		deleteVector (level, vector);
}

void Bitmap::set (int32 bitNumber)
{
	ASSERT (bitNumber >= 0);
	int max = 1;

	if (!vector)
		{
		if (count == 0)
			{
			unary = bitNumber;
			count = 1;
			
			return;
			}
			
		if (count == 1)
			{
			if (bitNumber == unary)
				return;
				
			max = 2;
			count = 0;
			}
		}

	for (int seq = 0; seq < max; ++seq)
		{
		int32 number = (seq) ? unary : bitNumber;

		// If necessary, increase depth of tree

		if (number >= sizes [level])
			{
			int lvl;
			
			for (lvl = 1; number >= sizes [lvl]; ++lvl)
				;
				
			if (lvl >= 2)
				{
				vector = allocVector (vector);
				
				if (vector [0])
					for (int n = level + 1; n < lvl; ++n)
						vector [0] = allocVector (vector [0]);
				}
				
			level = lvl;
			}

		uint indexes[6];
		decompose (number, indexes);
		void **ptr = (void**) &vector;

		for (int lvl = level; lvl >= 2; --lvl)
			{
			ptr = (void**) *ptr + indexes[lvl];
			
			if (!*ptr && lvl > 2)
				*ptr = allocVector (NULL);
			}

		Bms *segment = (Bms*) *ptr;

		if (!segment)
			{
			*ptr = segment = new Bms;
			segment->count = 1;
			memset (segment, 0, sizeof (Bms));
			}
				
		BitClump mask = 1 << indexes[0];
		BitClump *bits = segment->clump + indexes[1];

		if (!(*bits & mask))
			{
			*bits |= mask;
			++segment->count;
			++count;
			}
		}

}

void Bitmap::clear(int32 number)
{
	if (count == 1 && !vector)
		{
		if (number == unary)
			count = 0;
			
		return;
		}

	if (number >= sizes [level])
		return;

	uint indexes[6];
	decompose (number, indexes);
	void **ptr = (void**) &vector;

	for (int lvl = level; lvl >= 2; --lvl)
		{
		ptr = (void**) *ptr + indexes[lvl];
		
		if (!*ptr)
			return;
		}

	Bms *segment = (Bms*) *ptr;

	if (!segment)
		return;
			
	BitClump mask = 1 << indexes[0];
	BitClump *bits = segment->clump + indexes[1];

	if (*bits & mask)
		{
		*bits &= ~mask;
		
		if (--segment->count == 0)
			{
			*ptr = NULL;
			delete segment;
			
			if (level == 1)
				level = 0;
			}
			
		--count;
		}
}

int32 Bitmap::nextSet(int32 start)
{
	if (!vector)
		{
		if (count == 1)
			return (start <= unary) ? unary : -1;
			
		if (level == 0)
			return -1;
		}

	if (start >= sizes[level])
		return -1;

	uint indexes[6];
	decompose (start, indexes);
    void **vectors [5];
	vectors [level] = vector;
	int index;

	for (int lvl = level; lvl <= level;)
		if (lvl == 1)
			{
			Bms *segment = (Bms*) vectors [1];
			
			for (index = indexes[1]; index < CLUMPS; ++index)
				{
				int32 clump = segment->clump [index];
				
				if (clump)
					for (int bit = indexes[0]; bit < BITS_PER_CLUMP; ++bit)
						if (clump & (1 << bit))
							{
							int32 number = bit + index * BITS_PER_CLUMP;
							
							for (int n = 2; n <= level; ++n)
								number += indexes[n] * sizes [n - 1];
								
							return number;
							}
							
				indexes[0] = 0;
				}
				
			indexes[1] = 0;
			++lvl;
			++indexes[lvl];
			}
		else
			{
			void **vec = vectors [lvl];
			
			for (index = indexes[lvl]; index < BITMAP_VECTOR_SIZE && !vec [index]; ++index)
				;
				
			if (index < BITMAP_VECTOR_SIZE)
				{
				indexes[lvl] = index;
				vectors [--lvl] = (void**) vec [index];
				}
			else
				{
				++lvl;
				
				for (int n = 0; n < lvl; ++n)
					indexes[n] = 0;
					
				++indexes[lvl];
				}
			}

	return -1;
}

void Bitmap::andBitmap(Bitmap * bitmap)
{
	// Make sure we're the shallower tree

	if (level > bitmap->level)
		swap (bitmap);

	// If we don't have a vector, we're either empty or unary

	if (!vector)
		{
		if (count == 1 && !bitmap->isSet (unary))
			count = 0;
			
		bitmap->release();
		
		return;
		}

	int lvl;
	void **vec = bitmap->vector;

	// If the other bitmap is high level, come down the left path.
	// If any vector is null, we're empty

	for (lvl = bitmap->level; lvl > level; --lvl)
		if (!(vec = (void**) vec [0]))
			{
			clear();
			bitmap->release();
			
			return;
			}

	// If we're level 1, "and" the respective segments and we're done

	if (level == 1)
		{
		if (!andSegments ((Bms*) vector, (Bms*) vec))
			clear();
			
		bitmap->release();
		
		return;			
		}

    void **vectors1 [6];
    void **vectors2 [6];
	bool hits [6];
	int indexes[6];
	vectors1 [level] = vector;
	vectors2 [level] = vec;
	indexes[level] = 0;
	hits [level] = false;

	for (lvl = level; lvl <= level;)
		{
		void **vec1 = vectors1 [lvl];
		void **vec2 = vectors2 [lvl];
		int index;
		
		for (index = indexes[lvl]; index < BITMAP_VECTOR_SIZE; ++index)
			{
			void *v1;
			void *v2;
			
			if ( (v1 = vec1 [index]) )
				if ( (v2 = vec2 [index]) )
					{
					if (lvl == 2)
						{
						if (andSegments ((Bms*) v1, (Bms*) v2))
							hits [2] = true;
						else
							{
							vec1 [index] = NULL;
							deleteVector (1, (void**) v1);
							}
						}
					else
						{
						indexes[lvl] = index + 1;
						--lvl;
						vectors1 [lvl] = (void**) v1;
						vectors2 [lvl] = (void**) v2;
						indexes[lvl] = 0;
						hits [lvl] = false;
						
						break;
						}
					}
				else
					{
					vec1 [index] = NULL;
					deleteVector (lvl - 1, (void**) v1);
					}
			}
			
		if (index >= BITMAP_VECTOR_SIZE)
			{
			++lvl;
			
			if (hits [lvl - 1])
				hits [lvl] = true;
			}		
		}

	bitmap->release();
}

void Bitmap::orBitmap(Bitmap * bitmap)
{
	// Make sure we're the deeper bitmap

	if (level < bitmap->level)
		swap (bitmap);

	// If the other guy is truely boring, just skip the whole process

	if (!bitmap->vector)
		{
		if (bitmap->count == 1)
			set (bitmap->unary);
			
		bitmap->release();
		return;
		}

	// Bring ourselves down to the other bitmap's level, if necessary creating intermediates 

	int lvl;
	void **ptr = (void**) &vector;

	for (lvl = level; lvl > bitmap->level; --lvl)
		{
		void **vec = (void**) *ptr;
		
		if (lvl > 2 && !vec [0])
			vec [0] = allocVector (NULL);
			
		ptr = vec;
		}

	// If the other guys is a simple segment, either OR it or just steal it

	if (bitmap->level == 1)
		{
		Bms *segment = (Bms*) *ptr;
		
		if (segment)
			orSegments (segment, (Bms*) bitmap->vector);
		else
			{
			*ptr = bitmap->vector;
			bitmap->vector = NULL;
			bitmap->level = 0;
			}
			
		bitmap->release();
		
		return;			
		}

	// OK, we've got a multi-tier situation.  Time to get fancy

    void **vectors1 [6];
    void **vectors2 [6];
	vectors1 [lvl] = (void**) *ptr;
	vectors2 [lvl] = bitmap->vector;
	int indexes[6];
	indexes[lvl] = 0;

	while (lvl <= bitmap->level)
		{
		void **vec1 = vectors1 [lvl];
		void **vec2 = vectors2 [lvl];
		int index;
		
		for (index = indexes[lvl]; index < BITMAP_VECTOR_SIZE; ++index)
			{
			void *v1;
			void *v2;
			
			if ( (v1 = vec1 [index]) )
				{
				if ( (v2 = vec2 [index]) )
					{
					if (lvl == 2)
						orSegments ((Bms*) v1, (Bms*) v2);
					else
						{
						indexes[lvl] = index + 1;
						--lvl;
						vectors1 [lvl] = (void**) v1;
						vectors2 [lvl] = (void**) v2;
						indexes[lvl] = 0;
						
						break;
						}
					}
				}
			else if ( (v2 = vec2 [index]) )
				{
				vec1 [index] = v2;
				vec2 [index] = NULL;
				}
			}
			
		if (index >= BITMAP_VECTOR_SIZE)
			++lvl;
		}

	bitmap->release();
}

bool Bitmap::isSet(int32 number)
{
	ASSERT (number >= 0);

	if (!vector && count == 1)
		return number == unary;

	if (number >= sizes [level])
		return false;

	uint indexes[6];
	decompose (number, indexes);
	void **vec = vector;

	for (int lvl = level; lvl >= 2; --lvl)
		{
		vec = (void**) (vec [indexes[lvl]]);
		
		if (!vec)
			return false;
		}

	if (!vec)
		return false;

	Bms *segment = (Bms*) vec;
	BitClump mask = 1 << indexes[0];
	BitClump *bits = segment->clump + indexes[1];

	return ((*bits & mask) != 0);
}


void Bitmap::addRef()
{
	++useCount;
}

void Bitmap::release()
{
	if (--useCount == 0)
		delete this;
}

void** Bitmap::allocVector(void *firstItem)
{
	void **vector = new void* [BITMAP_VECTOR_SIZE];
	memset (vector, 0, BITMAP_VECTOR_SIZE * sizeof (void*));
	vector [0] = firstItem;

	return vector;
}

void Bitmap::decompose(int32 number, uint *indexes)
{
	switch (level)
		{
		default:
			ASSERT(false);

		case 5:
			indexes[5] = (number >> (CLUMP_BITS + INDEX_BITS + 3 * VECTOR_BITS)) & (BITMAP_VECTOR_SIZE - 1);
		case 4:
			indexes[4] = (number >> (CLUMP_BITS + INDEX_BITS + 2 * VECTOR_BITS)) & (BITMAP_VECTOR_SIZE - 1);
		case 3:
			indexes[3] = (number >> (CLUMP_BITS + INDEX_BITS + 1 * VECTOR_BITS)) & (BITMAP_VECTOR_SIZE - 1);
		case 2:
			indexes[2] = (number >> (CLUMP_BITS + INDEX_BITS + 0 * VECTOR_BITS)) & (BITMAP_VECTOR_SIZE - 1);
		case 1:
		case 0:
			indexes[1] = (number >> CLUMP_BITS) & (CLUMPS - 1);
			indexes[0] = number & (BITS_PER_CLUMP - 1);
		}	
}

void Bitmap::swap(Bitmap *bitmap)
{
	int lvl = level;
	int cnt = count;
	int uni = unary;
	void **vec = vector;

	level = bitmap->level;
	count = bitmap->count;
	unary = bitmap->unary;
	vector = bitmap->vector;

	bitmap->level = lvl;
	bitmap->count = cnt;
	bitmap->unary = uni;
	bitmap->vector = vec;
}

void Bitmap::clear()
{
	if (vector)
		deleteVector (level, vector);

	vector = NULL;
	level = 0;
	count = 0;
}

bool Bitmap::andSegments(Bms *segment1, Bms *segment2)
{
	bool hit = false;

	for (BitClump *c1 = segment1->clump, *c2 = segment2->clump, *end = c1 + CLUMPS; c1 < end;)
		if (*c1++ &= *c2++)
			hit = true;

	return hit;
}

void Bitmap::orSegments(Bms *segment1, Bms *segment2)
{
	for (BitClump *c1 = segment1->clump, *c2 = segment2->clump, *end = c1 + CLUMPS; c1 < end;)
		*c1++ |= *c2++;
}

void Bitmap::deleteVector(int lvl, void **vector)
{
	void *item;

	switch (lvl)
		{
		case 1:
			delete (Bms*) vector;
			break;

		case 2:
			{
			for (int n = 0; n < BITMAP_VECTOR_SIZE; ++n)
				if ( (item = vector [n]) )
					delete (Bms*) item;
			delete [] vector;
			}
			break;

		default:
			{
			--lvl;
			for (int n = 0; n < BITMAP_VECTOR_SIZE; ++n)
				if ( (item = vector [n]) )
					deleteVector (lvl, (void**) item);
			delete [] vector;
			}
		}
}

bool Bitmap::setSafe(int32 bitNumber)
{
	ASSERT (bitNumber >= 0);
	int max = 1;

	// Handle the zero and unary cases are a hassle and unimportant.
	
	if (count < 2)
		return false;
		
	for (int seq = 0; seq < max; ++seq)
		{
		int32 number = (seq) ? unary : bitNumber;

		// If necessary, increase depth of tree (but don't do it)

		if (number >= sizes [level])
			return false;

		uint indexes[6];
		decompose (number, indexes);
		void **ptr = (void**) &vector;

		for (int lvl = level; lvl >= 2; --lvl)
			{
			ptr = (void**) *ptr + indexes[lvl];
			
			if (!*ptr && lvl > 2)
				{
				//*ptr = allocVector (NULL);
				void **vec = allocVector (NULL);
				
				if (!COMPARE_EXCHANGE_POINTER(ptr, NULL, vec))
					delete vec;
				}
			}

		Bms *segment = (Bms*) *ptr;

		if (!segment)
			{
			//*ptr = segment = new Bms;
			segment = new Bms;
			segment->count = 1;
			memset (segment, 0, sizeof (Bms));
			
			if (!COMPARE_EXCHANGE_POINTER(ptr, NULL, segment))
				{
				delete segment;
				segment = (Bms*) *ptr;
				}
			}
				
		BitClump mask = 1 << indexes[0];
		BitClump *bits = segment->clump + indexes[1];

		for (;;)
			{
			BitClump clump = *bits;
			BitClump newClump = clump | mask;

			if (COMPARE_EXCHANGE(bits, clump, newClump))
				{
				INTERLOCKED_INCREMENT(segment->count);
				INTERLOCKED_INCREMENT(count);
				break;
				}
			}
		}
	
	return true;
}
