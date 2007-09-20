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
 *	MODULE:			Stack.cpp
 *	DESCRIPTION:	Encapsulated stack
 *
 * copyright (c) 1997 by James A. Starkey
 */

#include "Engine.h"
#include "Stack.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


Stack::Stack ()
{
/**************************************
 *
 *		S t a c k
 *
 **************************************
 *
 * Functional description
 *		Constructor.
 *
 **************************************/

prior = NULL;
}

void *Stack::getPrior (Stack **ptr)
{
/**************************************
 *
 *		g e t P r i o r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
Stack *node = *ptr;
*ptr = node->prior;

return node->object;
}

Stack *Stack::getTop ()
{
/**************************************
 *
 *		g e t T o p
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

return prior;
}

void *Stack::pop ()
{
/**************************************
 *
 *		p o p
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
Stack *node = prior;
void *object = node->object;
prior = node->prior;
node->prior = NULL;
delete node;

return object;
}


void Stack::push (void *object)
{
/**************************************
 *
 *		p u s h
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

Stack *node = new Stack;
node->object = object;
node->prior = prior;
prior = node;
}


void* Stack::mark()
{
	return prior;	
}

void Stack::pop(void * mark)
{
	while (prior && prior != mark)
		pop();	
}

void* Stack::peek()
{
	if (!prior)
		return NULL;

	return prior->object;
}

bool Stack::isEmpty()
{
	return prior == NULL;
}

Stack::~Stack()
{
	while (prior)
		pop();	
}

int Stack::count()
{
	int n = 0;

	for (Stack *node = prior; node; node = node->prior)
		++n;

	return n;
}

bool Stack::isMark(void * mark)
{
	return prior == mark;
}

void Stack::clear()
{
	while (prior)
		pop();	
}

bool Stack::insertOrdered(void *object)
{
	Stack **ptr = &prior;
	Stack *node;

	for (; (node = *ptr); ptr = &node->prior)
		if (node->object <= object)
			{
			if (node->object == object)
				return false;
			break;
			}

	Stack *s = *ptr = new Stack;
	s->object = object;
	s->prior = node;

	return true;
}

bool Stack::equal(Stack *stack)
{
	Stack *s1 = prior;
	Stack *s2 = stack->prior;

	for (; s1 && s2; s1 = s1->prior, s2 = s2->prior)
		if (s1->object != s2->object)
			return false;

	if (s1 || s2)
		return false;

	return true;
}

bool Stack::isMember(void *object)
{
	for (Stack *node = prior; node; node = node->prior)
		if (node->object == object)
			return true;

	return false;
}
