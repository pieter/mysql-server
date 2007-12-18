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
 *	MODULE:			Syntax.h
 *	DESCRIPTION:	Syntax Node definitions
 *
 * copyright (c) 1997 by James A. Starkey
 */

#ifndef __SYNTAX_H
#define __SYNTAX_H

#undef NODE
#define NODE(id,name)	id,
enum SyntaxType {
#include "nodes.h"
    };
#undef NODE

#define FOR_SYNTAX(child,parent) {for (int _n =0; _n < parent->count; ++_n)\
									{ Syntax *child = parent->children [_n];

class CSQLGen;
class LinkedList;

class Syntax {
    public:
	    Syntax (SyntaxType type, Syntax *child1, Syntax *child2, Syntax *child3, Syntax *child4);
	    const char* getTypeString();

	Syntax (SyntaxType type);
	Syntax (SyntaxType type, int count);
	Syntax (SyntaxType type, Syntax *child);
	Syntax (SyntaxType type, Syntax *child1, Syntax *child2);
	Syntax (SyntaxType type, Syntax *child1, Syntax *child2, Syntax *child3);
	Syntax (SyntaxType type, const char *value);
	Syntax (SyntaxType type, LinkedList &list);
	Syntax (LinkedList &list);
	virtual ~Syntax ();

	//void *operator new (size_t stAllocateBlock, Pool*);

	virtual void		init (SyntaxType type, int count);
	virtual void		initList (SyntaxType type, LinkedList *list);
	virtual Syntax		*getChild (int n);
	virtual Syntax		**getChildren();
	virtual const char	*getString ();
	virtual int			getNumber();
	virtual QUAD		getQuad();
	virtual int			getNumber (int child);
	virtual void		setChild (int h, Syntax *child);
	virtual void		prettyPrint (const char *text);
	virtual void		prettyPrint (int level);

	SyntaxType	type;
	int			count;
	Syntax		**children;
	Syntax		*next;
	const char	*value;
	uint64 getUInt64(void);
};

#endif
