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


#include <stdio.h>
#include "Engine.h"
#include "MemMgr.h"
#include "MemFreeBlock.h"

static const int TWIN	= 10;

#define RESET_PARENT(newChild)		\
	if (parent->smaller == this)		\
		parent->smaller = newChild;	 \
	else							\
		parent->larger = newChild;

static int lastRemove;

MemFreeBlock::MemFreeBlock(void)
{
	larger = NULL;
	smaller = NULL;
	balance = 0;
	nextTwin = priorTwin = this;
	memHeader.length = 0;
}

MemFreeBlock::~MemFreeBlock(void)
{
}

void MemFreeBlock::rotateLeft(void)
{
	MemFreeBlock *root = larger;
	
	if ( (larger = larger->smaller) )
		larger->parent = this;

	root->smaller = this;
	balance -= (1 + MAX(root->balance, 0));
	root->balance -= (1 - MIN(balance, 0));
	RESET_PARENT(root);
	root->parent = parent;
	parent = root;
}

void MemFreeBlock::rotateRight(void)
{
	MemFreeBlock *root = smaller;
	
	if ( (smaller = smaller->larger) )
		smaller->parent = this;
		
	root->larger = this;
	balance += (1 - MIN(root->balance, 0));
	root->balance += (1 + MAX(balance, 0));
	RESET_PARENT(root);
	root->parent = parent;
	parent = root;
}

void MemFreeBlock::insert(MemFreeBlock *newNode)
{
	newNode->larger = NULL;
	newNode->smaller = NULL;
	newNode->balance = 0;
	MemFreeBlock *node = this;
	
	// Find insertion point and insert new node as leaf
	
	while (node)
		if (newNode->memHeader.length < node->memHeader.length)
			{
			if (node->smaller)
				node = node->smaller;
			else
				{
				node->smaller = newNode;
				--node->balance;
				break;
				}
			}
		else if (newNode->memHeader.length > node->memHeader.length)
			{
			if (node->larger)
				node = node->larger;
			else
				{
				node->larger = newNode;
				++node->balance;
				break;
				}
			}
		else
			{
			newNode->balance = TWIN;
			newNode->nextTwin = node->nextTwin;
			newNode->nextTwin->priorTwin = newNode;
			newNode->priorTwin = node;
			node->nextTwin = newNode;
			//node->nextLarger = NULL;
			
			return;
			}
	
	// New node is inserted.  Balance tree upwards
	
	newNode->parent = node;
	newNode->nextTwin = newNode->priorTwin = newNode;

	for (MemFreeBlock *nodeParent; node->balance && (nodeParent = node->parent) && nodeParent->parent; node = nodeParent)
		{
		if (nodeParent->smaller == node)
			{
			if (--nodeParent->balance < -1)
				{
				nodeParent->rebalance();
				break;
				}
			}
		else
			{
			if (++nodeParent->balance > +1)
				{
				nodeParent->rebalance();
				break;
				}
			}
		}
}

MemFreeBlock* MemFreeBlock::findNextLargest(int size)
{
	ASSERT(size > 0);
	MemFreeBlock *block = this;
	
	// Travse down the tree looking for a block that fits
	
	while (block)
		if (size < block->memHeader.length)
			{
			if (block->smaller)
				block = block->smaller;
			else
				break;
			}
		else if (size > block->memHeader.length)
			{
			if (block->larger)
				block = block->larger;
			else
				break;
			}
		else
			break;
			
	for (; block; block = block->parent)
		if (size <= block->memHeader.length)
			{
			if (block->nextTwin == block)
				{
				block->remove();
				
				return block;
				}
				
			// We've got a twin of the same size.  Take it out of the twin list.
			
			MemFreeBlock *twin = block->nextTwin;
			twin->nextTwin->priorTwin = twin->priorTwin;
			twin->priorTwin->nextTwin = twin->nextTwin;
			
			return twin;
			}
			
	return NULL;
}

void MemFreeBlock::remove()
{
	// If we a simple twin, zip out of the twin list and we're done
	
	if (balance == TWIN)
		{
		nextTwin->priorTwin = priorTwin;
		priorTwin->nextTwin = nextTwin;
		lastRemove = 1;
		
		return;
		}
		
	// If we have a twin, promote the first twin into the tree
	
	if (nextTwin != this)
		{
		MemFreeBlock *twin = nextTwin;
		priorTwin->nextTwin = twin;
		twin->priorTwin = priorTwin;
		
		twin->balance = balance;
		twin->parent = parent;
		
		if ( (twin->smaller = smaller) )
			smaller->parent = twin;
			
		if ( (twin->larger = larger) )
			larger->parent = twin;
	
		RESET_PARENT(twin);
		parent->validate();
		//validateFreeList();
		lastRemove = 2;
		
		return;
		}
		
	// There are three cases a) No children, b) One child, c) Two children

	if (!smaller || !larger)
		{
		MemFreeBlock *next = (smaller) ? smaller : larger;
		
		if (next)
			next->parent = parent;
		
		if (parent->smaller == this)
			{
			parent->smaller = next;
			parent->rebalanceUpward(+1);
			}
		else
			{
			parent->larger = next;
			parent->rebalanceUpward(-1);
			}	

		lastRemove = 3;
		
		return;	
		}

	// We're an equality with two children.  Bummer.  Find successor node (next larger value)
	// and swap it into our place
	
	bool shallower;
	MemFreeBlock *node = larger->getSuccessor(&larger, &shallower);
	
	if ( (node->smaller = smaller) )
		smaller->parent = node;
		
	if ( (node->larger = larger) )
		larger->parent = node;
		
	node->balance = balance;
	node->parent = parent;
	RESET_PARENT(node);
	lastRemove = 4;
	
	// If we got shallower, adjust balance and rebalance if necessary

	if (shallower)
		{
		lastRemove = 5;
		node->rebalanceUpward(-1);
		}
}

void MemFreeBlock::rebalance(void)
{
	if (balance > 1)
		{
		if (larger->balance < 0)
			larger->rotateRight();
					
		rotateLeft();
		}
	else if (balance < 1)
		{
		if (smaller->balance > 0)
			smaller->rotateLeft();
			
		rotateRight();
		}
}

bool MemFreeBlock::rebalanceDelete()
{
	if (balance > 1)
		{
		if (larger->balance < 0)
			{
			larger->rotateRight();
			rotateLeft();
			
			return true;
			}
					
		rotateLeft();
		
		return parent->balance == 0;
		}
		
	if (balance < 1)
		{
		if (smaller->balance > 0)
			{
			smaller->rotateLeft();
			rotateRight();
			
			return true;
			}
			
		rotateRight();
		
		return parent->balance == 0;
		}
	
	return false;
}

MemFreeBlock* MemFreeBlock::getSuccessor(MemFreeBlock** parentPointer, bool* shallower)
{
	// If there's a smaller node, recurse down it rebalancing
	// if the subtree gets shallower
	
	if (smaller)
		{
		int was = balance;
		MemFreeBlock *node = smaller->getSuccessor(&smaller, shallower);

		// If we got shallower, adjust balance and rebalance if necessary

		if (*shallower)
			{
			if (++balance > 1)
				*shallower = rebalanceDelete();
			else if (!was && (*parentPointer)->balance)
				*shallower = false;
			}
			
		return node;
		}
	
	// We're the bottom.  Remove us from our parent and we're done.
	
	RESET_PARENT(larger);
	
	if (larger)
		larger->parent = parent;
		
	*shallower = true;

	return this;
}

void MemFreeBlock::rebalanceUpward(int delta)
{
	MemFreeBlock *node = this;
	
	for (;;)
		{
		MemFreeBlock *nodeParent = node->parent;
		
		if (!nodeParent)
			{
			node->balance += delta;
			
			return;
			}

		int parentDelta = (nodeParent->smaller == node) ? 1 : -1;
		node->balance += delta;
		
		if (node->balance == delta)
			break;
		
		if (node->balance > 1 || node->balance < -1)
			if (!node->rebalanceDelete())
				break;
		
		delta = parentDelta;			
		node = nodeParent;
		}
}

MemFreeBlock* MemFreeBlock::getFirst(void)
{
	MemFreeBlock *node = larger;
	
	if (!node)
		return node;
	
	while (node->smaller)
		node = node->smaller;
	
	return node;	
}

MemFreeBlock* MemFreeBlock::getNext(void)
{
	MemFreeBlock *node = larger;
	
	if (node)
		{
		while (node->smaller)
			node = node->smaller;
		
		return node;
		}

	for (node = this; node && node->parent; node = node->parent)
		if (node == parent->smaller)
			return parent;
	
	return NULL;
}

int MemFreeBlock::validate(void)
{
	int rightDepth = 0;
	int leftDepth = 0;
	
	if (smaller)
		{
		if (smaller->memHeader.length >= memHeader.length)
			corrupt("out of order");
			
		if (smaller->parent != this)
			corrupt("bad parent");
		
		leftDepth = smaller->validate();
		}
	
	if (larger)
		{
		if (larger->memHeader.length < memHeader.length)
			corrupt("out of order");
			
		if (larger->parent != this)
			corrupt("bad right parent");
		
		rightDepth = larger->validate();
		}
	
	if (parent && balance != rightDepth - leftDepth)
		corrupt("bad balance");
		
	return MAX(rightDepth, leftDepth) + 1;
}

void MemFreeBlock::corrupt(const char *text)
{
	ASSERT(false);
}

int MemFreeBlock::count(void)
{
	int count = 1;
	
	for (MemFreeBlock *block = nextTwin; block != this; block = block->nextTwin)
		++count;
		
	if (smaller)
		count += smaller->count();
	
	if (larger)
		count += larger->count();
		
	return count;
}
