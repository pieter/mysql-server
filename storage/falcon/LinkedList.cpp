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

/*
 *	PROGRAM:		Virtual Data Manager
 *	MODULE:			LinkedList.c
 *	DESCRIPTION:	Generic Linked List
 *
 * copyright (c) 1997 - 2000 by James A. Starkey
 */

#include <stdio.h>
#include "Engine.h"
#include "LinkedList.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


LinkedList::LinkedList ()
{
/**************************************
 *
 *		L i n k e d L i s t
 *
 **************************************
 *
 * Functional description
 *		Constructor for both nodes and list head.
 *
 **************************************/

//addressCheck (this);

next = prior = NULL;
}

LinkedList::~LinkedList ()
{
/**************************************
 *
 *		~ L i n k e d L i s t
 *
 **************************************
 *
 * Functional description
 *		Constructor for both nodes and list head.
 *
 **************************************/
LinkedNode *node;

//addressCheck (this);

while ( (node = next) )
    {
	next = node->next;
	delete node;
	}

}

LinkedNode::LinkedNode (void *obj)
{
/**************************************
 *
 *		L i n k e d N o d e
 *
 **************************************
 *
 * Functional description
 *		Constructor for both nodes and list head.
 *
 **************************************/

object = obj;
}

LinkedNode::~LinkedNode ()
{
/**************************************
 *
 *		~ L i n k e d N o d e
 *
 **************************************
 *
 * Functional description
 *		Constructor for both nodes and list head.
 *
 **************************************/

next = prior = NULL;
}

void LinkedList::append (void *object)
{
/**************************************
 *
 *		a p p e n d
 *
 **************************************
 *
 * Functional description
 *		Append object/node to list.
 *
 **************************************/

LinkedNode *node = new LinkedNode (object);
addressCheck (node);

if (prior)
    {
	prior->next = node;
	node->prior = prior;
	}
else
	next = node;

prior = node;
}

bool LinkedList::appendUnique (void *object)
{
/**************************************
 *
 *		a p p e n d U n i q u e
 *
 **************************************
 *
 * Functional description
 *		Append object/node to list.
 *
 **************************************/
LinkedNode	*node;

for (node = next; node; node = node->next)
    if (node->object == object)
		return false;

append (object);

return true;
}

void LinkedList::clear ()
{
/**************************************
 *
 *		c l e a r
 *
 **************************************
 *
 * Functional description
 *		Delete all elements of list.
 *
 **************************************/
LinkedNode	*node;

while ( (node = next) )
    {
	next = node->next;
	delete node;
	}

prior = NULL;
}

int LinkedList::count ()
{
/**************************************
 *
 *		c o u n t
 *
 **************************************
 *
 * Functional description
 *		Count nodes.
 *
 **************************************/
int	n = 0;

for (LinkedList *node = next; node; node = node->next)
    ++n;

return n;
}

bool LinkedList::deleteItem (void *object)
{
/**************************************
 *
 *		d e l e t e I t e m
 *
 **************************************
 *
 * Functional description
 *		Delete node from linked list.  Return true is the item
 *		was on the list, otherwise return false.
 *
 **************************************/

for (LinkedNode *node = next; node; node = node->next)
    if (node->object == object)
		{
		if (node->prior)
			node->prior->next = node->next;
		else
			next = node->next;
		if (node->next)
			node->next->prior = node->prior;
		else
			prior = node->prior;
		delete node;
		return true;
		}

return false;
}

void *LinkedList::getElement (int position)
{
/**************************************
 *
 *		g e t E l e m e n t
 *
 **************************************
 *
 * Functional description
 *		Get element in list by position.
 *
 **************************************/
int			n;
LinkedNode	*node;

for (node = next, n = 0; node; node = node->next, ++n)
	if (n == position)
		return node->object;

return NULL;
}

void *LinkedList::getPrior (LinkedList **node)
{
/**************************************
 *
 *		g e t P r i o r
 *
 **************************************
 *
 * Functional description
 *		Return object of current node; advance pointer.
 *
 **************************************/
//void	*object;

LinkedNode *n = (*node)->prior;
*node = n;
//*node = (*node)->prior;
//object = (*node)->object;

return n->object;
}

LinkedList *LinkedList::getTail ()
{
/**************************************
 *
 *		g e t T a i l
 *
 **************************************
 *
 * Functional description
 *		Get last list node; used for iterating.
 *
 **************************************/

return this;
}

bool LinkedList::isEmpty ()
{
/**************************************
 *
 *		i s E m p t y
 *
 **************************************
 *
 * Functional description
 *		Return whether iteration has more members to go.
 *
 **************************************/

return next == NULL;
}

bool LinkedList::isMember (void *object)
{
/**************************************
 *
 *		i s M e m b e r
 *
 **************************************
 *
 * Functional description
 *		Is object in list?
 *
 **************************************/

for (LinkedNode *node = next; node; node = node->next)
    if (node->object == object)
		return 1;

return 0;
}

bool LinkedList::moreBackwards (LinkedList *node)
{
/**************************************
 *
 *		m o r e B a c k w a r d s
 *
 **************************************
 *
 * Functional description
 *		Return whether iteration has more members to go.
 *
 **************************************/

return (node->prior != NULL);
}

bool LinkedList::insertBefore(void * insertItem, void * item)
{
/**************************************
 *
 *		i n s e r t B e f o r e
 *
 **************************************
 *
 * Functional description
 *		Add item in middle of list.
 *
 **************************************/

	for (LinkedNode *node = next; node; node = node->next)
		if (node->object == item)
			{
			LinkedNode *insert = new LinkedNode (insertItem);
			
			if ( (insert->prior = node->prior) )
				insert->prior->next = insert;
			else
				next = insert;
				
			node->prior = insert;
			insert->next = node;
			
			return true;
			}

	return false;
}

void LinkedList::addressCheck(void * address)
{
	/***
	if ((IPTR) address == 0x00CDE290 ||
		(IPTR) address == 0x00cdde70)
		printf ("hit %x\n", address);
	***/
}
