/* Copyright (C) 2007 MySQL AB

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


#ifndef _MEM_FREE_BLOCK_H_
#define _MEM_FREE_BLOCK_H_

class MemFreeBlock :
	public MemBigObject
{
public:
	MemFreeBlock(void);
	~MemFreeBlock(void);

	MemFreeBlock	*smaller;
	MemFreeBlock	*larger;
	MemFreeBlock	*parent;
	MemFreeBlock	*nextTwin;
	MemFreeBlock	*priorTwin;
	
	int				balance;
	
	void			rotateLeft(void);
	void			rotateRight(void);
	void			insert(MemFreeBlock *node);
	MemFreeBlock*	findNextLargest(int size);
	void			rebalance(void);
	bool			rebalanceDelete();
	MemFreeBlock*	getSuccessor(MemFreeBlock** parentPointer, bool* shallower);
	void			remove();
	void			rebalanceUpward(int delta);
	MemFreeBlock*	getFirst(void);
	MemFreeBlock*	getNext(void);
	int				validate(void);
	void			corrupt(const char *text);
	int				count(void);
};

#endif
