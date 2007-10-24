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

// NBitSet.cpp: implementation of the NBitSet class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NBitSet.h"
#include "CompiledStatement.h"
#include "Value.h"
#include "Bitmap.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NBitSet::NBitSet(CompiledStatement *statement, NNode *child)
		: NNode (statement, BitSet)
{
	node = child; //child->clone();
}

NBitSet::~NBitSet()
{
}

Bitmap* NBitSet::evalInversion(Statement * statement)
{
	Value *value = node->eval (statement);
	Bitmap *bitmap = new Bitmap;
	bitmap->set (value->getInt());

	return bitmap;
}

NNode* NBitSet::clone()
{
	ASSERT (false);
	return NULL;
}
