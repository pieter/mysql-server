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

// NInSelectBitmap.cpp: implementation of the NInSelectBitmap class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NInSelectBitmap.h"
#include "NInSelect.h"
#include "Statement.h"
#include "SQLError.h"
#include "NSelect.h"
#include "ResultSet.h"
#include "Bitmap.h"
#include "Index.h"
#include "IndexKey.h"
#include "PrettyPrint.h"
#include "CompiledStatement.h"
#include "ValueSet.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NInSelectBitmap::NInSelectBitmap(NInSelect *node, Index *idx) : NNode (node->statement, InSelectBitmap)
{
	select = node->inversion;
	expr = node->expr;
	index = idx;
	valueSetSlot = statement->getValueSetSlot();
}

NInSelectBitmap::~NInSelectBitmap()
{

}

Bitmap* NInSelectBitmap::evalInversion(Statement *statement)
{
	select->evalStatement (statement);
	ResultSet *resultSet = statement->getResultSet();
	Bitmap *bitmap = NULL;
	ValueSet *valueSet = statement->getValueSet(valueSetSlot);

	if (valueSet)
		valueSet->reset();

	while (resultSet->next())
		{
		IndexKey indexKey(index);
		Value *value = resultSet->getValue (1);
		index->makeKey (1, &value, &indexKey);
		Bitmap *bits = index->scanIndex (&indexKey, &indexKey, index->numberFields > 1, statement->transaction, NULL);

		if (bits)
			{
			if (!valueSet)
				statement->valueSets[valueSetSlot] = valueSet = new ValueSet(index);

			valueSet->addValue(value);

			if (bitmap)
				bitmap->orBitmap (bits);
			else
				bitmap = bits;
			}
		}

	resultSet->close();

	if (!bitmap)
		bitmap = new Bitmap;

	return bitmap;
}

void NInSelectBitmap::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent (level++);
	pp->put ("In Select Bitmap\n");
	expr->prettyPrint (level, pp);
	select->prettyPrint (level, pp);
}

bool NInSelectBitmap::hasValue(Statement *statement, Value *value)
{
	ValueSet *valueSet = statement->getValueSet(valueSetSlot);

	if (!valueSet)
		return false;

	return valueSet->hasValue(value);
}
