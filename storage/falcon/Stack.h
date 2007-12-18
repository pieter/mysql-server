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
 *	MODULE:			Stack.h
 *	DESCRIPTION:	Encapsulated stack
 *
 * copyright (c) 1997 by James A. Starkey
 */

#ifndef __STACK_H
#define __STACK_H

#define FOR_STACK(type,child,stack) {for (Stack *pos=(stack)->getTop();pos;){ type child = (type) (stack)->getPrior (&pos);
#define END_FOR						 }}


class Stack {
    public:
	    bool isMember (void *object);
	    bool equal (Stack *stack);
	    bool insertOrdered (void *object);
	    void clear();
	    bool isMark (void *mark);
	    int count();
	    virtual  ~Stack();
	    bool isEmpty();
	    void* peek();
	    void pop (void *mark);
	    void* mark();

	Stack();

	void		push (void *object);
	void		*pop ();
	Stack		*getTop();
	void		*getPrior (Stack **);

	protected:

	Stack		*prior;
	void		*object;
    };

#endif

