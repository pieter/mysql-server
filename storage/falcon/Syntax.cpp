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
 *	MODULE:			Syntax.cpp
 *	DESCRIPTION:	Syntax Node code
 *
 * copyright (c) 1997 by James A. Starkey
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "Engine.h"
#include "Syntax.h"
#include "LinkedList.h"
#include "Log.h"

#define NODE(id,name)	name,
static const char		*nodeNames [] = {
#include "nodes.h"
};
#undef NODE

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

Syntax::Syntax (SyntaxType type, int count)
{
/**************************************
 *
 *		S y n t a x
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

init (type, count);
}

Syntax::Syntax (SyntaxType type)
{
/**************************************
 *
 *		S y n t a x
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

init (type, 0);
}

Syntax::Syntax (SyntaxType type, Syntax *child)
{
/**************************************
 *
 *		S y n t a x
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

init (type, 1);
children [0] = child;
}

Syntax::Syntax (SyntaxType type, Syntax *child1, Syntax *child2)
{
/**************************************
 *
 *		S y n t a x
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

init (type, 2);
children [0] = child1;
children [1] = child2;
}

Syntax::Syntax (SyntaxType type, Syntax *child1, Syntax *child2, Syntax *child3)
{
/**************************************
 *
 *		S y n t a x
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

init (type, 3);
children [0] = child1;
children [1] = child2;
children [2] = child3;
}
Syntax::Syntax (SyntaxType type, const char *val)
{
/**************************************
 *
 *		S y n t a x
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

init (type, 0);

if (type == nod_name)
	value = (char*) val;
else
	{
	value = new char [strlen (val) + 1];
	strcpy ((char*) value, val);
	}
}

Syntax::Syntax (SyntaxType type, LinkedList &list)
{
/**************************************
 *
 *		S y n t a x
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

initList (type, &list);
}

Syntax::Syntax (LinkedList &list)
{
/**************************************
 *
 *		S y n t a x
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

initList (nod_list, &list);
}

Syntax::~Syntax ()
{
/**************************************
 *
 *		~ S y n t a x
 *
 **************************************
 *
 * Functional description
 *		Delete object.  Let somebody else worry about the children
 *
 **************************************/

	/***
	if (children)
		{
		for (int n = 0; n < count; ++n)
			if (children [n])
				delete children [n];
		delete children;
		children = NULL;
		}
	***/

	delete [] children;

	if (value && type != nod_name)
		delete [] (char*) value;
}

Syntax *Syntax::getChild (int n)
{
/**************************************
 *
 *		g e t C h i l d
 *
 **************************************
 *
 * Functional description
 *		Fetch a child.
 *
 **************************************/

return children [n];
}

Syntax **Syntax::getChildren ()
{
/**************************************
 *
 *		g e t C h i l d r e n
 *
 **************************************
 *
 * Functional description
 *		Fetch a child.
 *
 **************************************/

return children;
}

int Syntax::getNumber ()
{
/**************************************
 *
 *		g e t N u m b e r
 *
 **************************************
 *
 * Functional description
 *		Return value as number.
 *
 **************************************/
	int		n = 0;
	char	c;

	for (const char	*p = value; (c = *p++);)
		if (c >= '0' && c <= '9')
			n = n * 10 + c - '0';

	return n;
}

int64 Syntax::getQuad ()
{
/**************************************
 *
 *		g e t Q u a d
 *
 **************************************
 *
 * Functional description
 *		Return value as number.
 *
 **************************************/
	int64	n = 0;
	char	c;

	for (const char *p = value; (c = *p++);)
		if (c >= '0' && c <= '9')
			n = n * 10 + c - '0';

	return n;
}

int Syntax::getNumber (int child)
{
/**************************************
 *
 *		g e t C h i l d r e n
 *
 **************************************
 *
 * Functional description
 *		Return value as number.
 *
 **************************************/

return children [child]->getNumber();
}

const char *Syntax::getString ()
{
/**************************************
 *
 *		g e t C h i l d r e n
 *
 **************************************
 *
 * Functional description
 *		Return value as string.
 *
 **************************************/

return value;
}

void Syntax::init (SyntaxType typ, int cnt)
{
/**************************************
 *
 *		i n i t
 *
 **************************************
 *
 * Functional description
 *		Initialize object.
 *
 **************************************/

type = typ;
count = cnt;
value = NULL;
//insertPoint = 0;

if (count)
	{
	int size = sizeof (Syntax*) * count;
	children = new Syntax* [size];
	memset (children, 0, size);
	}
else
	children = NULL;

}

void Syntax::initList (SyntaxType typ, LinkedList *list)
{
/**************************************
 *
 *		i n i t L i s t
 *
 **************************************
 *
 * Functional description
 *		Construct a syntax node with unknown children.
 *
 **************************************/

init (typ, list->count());

int n = 0;
FOR_OBJECTS (Syntax*, child, list)
    children [n++] = child;
END_FOR;
}

#if FALSE
void *Syntax::operator new (size_t stAllocateBlock, Pool *pool)
{
/**************************************
 *
 *		o p e r a t o r   n e w
 *
 **************************************
 *
 * Functional description
 *		Allocate node from existing pool.
 *
 **************************************/
void	*node = pool->allocBlock (stAllocateBlock);

return node;
}
#endif

void Syntax::prettyPrint (const char *text)
{
/**************************************
 *
 *		p r e t t y P r i n t
 *
 **************************************
 *
 * Functional description
 *		Pretty print a tree.
 *
 **************************************/

Log::debug ("%s\n", text);
prettyPrint (1);
}

void Syntax::prettyPrint (int level)
{
/**************************************
 *
 *		p r e t t y P r i n t
 *
 **************************************
 *
 * Functional description
 *		Pretty print a tree.
 *
 **************************************/

Log::debug ("%*s%s %s\n", level * 3, "", nodeNames [type],
		(value) ? value : "");
++level;

for (int n = 0; n < count; ++n)
	if (children [n])
		children [n]->prettyPrint (level);
	else
		Log::debug ("%*s*** null ***\n", level * 3, "");
}

void Syntax::setChild (int n, Syntax *child)
{
/**************************************
 *
 *		s e t C h i l d
 *
 **************************************
 *
 * Functional description
 *		Fetch a child.
 *
 **************************************/

if (n >= count)
    Log::debug ("Syntax::setChild -- bad index\n");

children [n] = child;
}

const char* Syntax::getTypeString()
{
	return nodeNames [type];
}

Syntax::Syntax(SyntaxType type, Syntax *child1, Syntax *child2, Syntax *child3, Syntax *child4)
{
	init (type, 3);
	children [0] = child1;
	children [1] = child2;
	children [2] = child3;
	children [3] = child4;
}

uint64 Syntax::getUInt64(void)
{
	uint64 n = 0;
	char c;

	for (const char	*p = value; (c = *p++);)
		if (c >= '0' && c <= '9')
			n = n * 10 + c - '0';
		else if (c == 'm' || c == 'M')
			n *= 1000000;
		else if (c == 'g' || c == 'G')
			n *= QUAD_CONSTANT(1000000000);

	return n;
}
