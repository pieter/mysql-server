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

// NBitmap.cpp: implementation of the NBitmap class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "NBitmap.h"
#include "Statement.h"
#include "Index.h"
#include "LinkedList.h"
#include "Log.h"
#include "PrettyPrint.h"
#include "IndexNode.h"
#include "Bitmap.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NBitmap::NBitmap(CompiledStatement *statement, Index *idx, int numberNodes, NNode **min, NNode **max) : NNode (statement, BitmapNode)
{
	minNodes = copyVector (numberNodes, min);
	maxNodes = copyVector (numberNodes, max);
	index = idx;
	nodeCount = numberNodes;
	partial = nodeCount != index->numberFields;
	equality = true;
	redundant = false;
	values = 0;

	for (int n = 0; n < nodeCount; ++n)
		{
		if (minNodes [n] != maxNodes [n])
			equality = false;
		if (maxNodes [n])
			values = n + 1;
		else
			{
			partial = true;
			break;
			}
		}
}

NBitmap::~NBitmap()
{
	delete [] minNodes;
	delete [] maxNodes;
}

Bitmap* NBitmap::evalInversion(Statement *statement)
{
	IndexKey lower(index);
	IndexKey *low = &lower;
	makeKey (statement, minNodes, &lower);

	if (!minNodes[0])
		{
		lower.keyLength = 0;
		low = NULL;
		}

	if (equality)
		return index->scanIndex (low, low, partial, statement->transaction, NULL);

	IndexKey upper(index);
	IndexKey *high = &upper;
	makeKey (statement, maxNodes, &upper);

	if (!maxNodes[0])
		{
		upper.keyLength = 0;
		high = NULL;
		}

	return index->scanIndex (low, high, partial, statement->transaction, NULL);
}

void NBitmap::makeKey(Statement *statement, NNode **exprs, IndexKey *indexKey)
{
	Value *values [MAX_KEY_SEGMENTS], **value = values;

	for (int n = 0; n < nodeCount; ++n)
		if (exprs [n])
			*value++ = exprs [n]->eval (statement);
		else
			{
			*value = NULL;
			break;
			}

	index->makeKey(nodeCount, values, indexKey);
}

NNode* NBitmap::clone()
{
	ASSERT (false);

	return NULL;
}

void NBitmap::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent (level++);
	pp->format ("Bitmap index %s\n", (const char*) index->name);
	bool diff = false;

	for (int n = 0; n < nodeCount; ++n)
		{
		printNode (level, minNodes [n], pp);
		if (minNodes [n] != maxNodes [n])
			diff = true;
		}

	if (diff)
		for (int n = 0; n < nodeCount; ++n)
			printNode (level, maxNodes [n], pp);
}

bool NBitmap::isRedundantIndex(LinkedList *indexes)
{
	FOR_OBJECTS (NBitmap*, node, indexes)
		if (node != this && node->type == BitmapNode && !node->redundant)
			if (isRedundantIndex (node))
				return true;
	END_FOR;

	return false;
}

bool NBitmap::isRedundantIndex(NBitmap *node)
{
	// If either is non-equality, we can't tell anything

	if (!equality || !node->equality)
		return false;

	// If we more specific than the other guy, we're done

	if (values > node->values)
		return false;

	// See if the two indexes have equivalent leading values

	int cnt = MIN (values, node->values);
	int n;

	for (n = 0; n < cnt; ++n)
		if (!minNodes [n]->equiv (node->minNodes [n]))
			return false;

	// If we match the same number of values, pick the denser (shorter)
	// index.

	if (values == node->values)
		{
		if (index->numberFields < node->index->numberFields)
			return false;
		}

	// We're either worse than or equivalent to the other guy.  Drop out.

	redundant = true;

	return true;
}

bool NBitmap::isUniqueIndex()
{
	// If this isn't equality or is only partial, it isn't unique

	if (!equality || partial)
		return false;

	if (!INDEX_IS_UNIQUE(index->type))
		return false;

	return true;
}
