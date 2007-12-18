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

// ValueSet.cpp: implementation of the ValueSet class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "ValueSet.h"
#include "Value.h"
#include "Index.h"
#include "IndexKey.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ValueSet::ValueSet(Index *idx)
{
	index = idx;
	field = index->fields[0];
}

ValueSet::~ValueSet()
{

}

void ValueSet::addValue(Value *value)
{
	//UCHAR key[MAX_KEY_LENGTH];
	//int len = index->makeKey(field, value, key);
	IndexKey indexKey(index);
	index->makeKey(field, value, 0, &indexKey);
}

bool ValueSet::hasValue(Value *value)
{
	//UCHAR key[MAX_KEY_LENGTH];
	//int len = index->makeKey(field, value, key);
	IndexKey indexKey(index);
	index->makeKey(field, value, 0, &indexKey);

	return true;
}

void ValueSet::reset()
{

}
