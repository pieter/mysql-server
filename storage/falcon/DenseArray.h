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

#ifndef _DENSE_ARRAY_H_
#define _DENSE_ARRAY_H_

#include <memory.h>

template <typename T, uint width> class DenseArray
{
#ifdef _DEBUG
#undef THIS_FILE
	const char *THIS_FILE;
#endif

public:
	DenseArray()
		{
#ifdef _DEBUG
		THIS_FILE=__FILE__;
#endif
	
		length = width;
		vector = new T[length];
		zap();
		};
	
	~DenseArray()
		{
		delete [] vector;
		};
	
	void extend (uint newLength)
		{
		if (newLength < length)
			return;
			
		T *oldVector = vector;
		vector = new T[newLength];
		memcpy((void*) vector, (void*) oldVector, length * sizeof(T));
		memset((void*) (vector + length), 0, (newLength - length) * sizeof(T));
		length = newLength;
		};
	
	void zap ()
		{
		memset((void*) vector, 0, length * sizeof(T));
		};
	
	T get (uint index)
		{
		if (index >= length)
			return 0;
		
		return vector[index];
		};
		
	T		*vector;
	uint	length;
};

#endif
