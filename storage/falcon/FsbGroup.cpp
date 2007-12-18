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

// FsbGroup.cpp: implementation of the FsbGroup class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "FsbGroup.h"
#include "Statement.h"
#include "CompiledStatement.h"
#include "NSelect.h"
#include "Value.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FsbGroup::FsbGroup(NSelect *select, NNode *groupList, Fsb *source)
{
	stream = source;
	groups = groupList;
	CompiledStatement *statement = select->compiledStatement;
	values = select->values;
	numberColumns = values->count;
	valueSlots = new int [numberColumns];
	int n;

	for (n = 0; n < numberColumns; ++n)
		valueSlots [n] = statement->getValueSlot();

	countSlot = statement->getGeneralSlot();
	rowSlot = statement->getRowSlot();
	groups = groupList;
	groupSlots = new int [groups->count];

	for (n = 0; n < groups->count; ++n)
		groupSlots [n] = statement->getValueSlot();
}

FsbGroup::~FsbGroup()
{
	delete [] groupSlots;
	delete [] valueSlots;
}

void FsbGroup::open(Statement *statement)
{
	stream->open (statement);
	statement->slots [countSlot] = 0;
}

Row* FsbGroup::fetch(Statement *statement)
{
	// If we're already at EOF we shouldn't even be here

	if (statement->eof)
		return NULL;

	// If this is the first time in, fetch a record.  If nothing there, we're done

	Row *row = statement->rowSlots [rowSlot];

	if (!statement->slots [countSlot]++)
		if (!(row = stream->fetch (statement)))
			{
			statement->eof = true;
			return NULL;
			}

	// Get current values for this control group

	int n;

	for (n = 0; n < groups->count; ++n)
		{
		Value *value = groups->children [n]->eval (statement);
		statement->values [groupSlots [n]].setValue (value, true);
		}

	NNode **children = values->children;

	// Propogate non-statistical values into the result set while we still have them

	for (n = 0; n < numberColumns; ++n)
		{
		NNode *child = children [n];
		children [n]->reset (statement);

		if (!child->isStatistical())
			statement->values [valueSlots [n]].setValue (child->eval (statement), true);
		}

	int count = 0;

	for (bool brk = false; !brk;)
		{
		int n;

		for (n = 0; n < numberColumns; ++n)
			children [n]->increment (statement);

		if (!(row = stream->fetch (statement)))
			{
			statement->eof = true;
			break;
			}

		++count;

		for (n = 0; n < groups->count; ++n)
			{
			Value *value = groups->children [n]->eval (statement);
			if (statement->values [groupSlots[n]].compare (value) != 0)
				{
				brk = true;
				break;
				}
			}
		}

	for (n = 0; n < numberColumns; ++n)
		{
		NNode *child = children [n];

		if (child->isStatistical())
			{
			Value *value = values->children[n]->eval (statement);
			statement->values [valueSlots [n]].setValue (value, true);
			}
		}

	statement->rowSlots [rowSlot] = row;

	return this;
}

void FsbGroup::close(Statement *statement)
{
	stream->close (statement);
}

Value* FsbGroup::getValue(Statement *statement, int index)
{
	return &statement->values [valueSlots[index]];
}

int FsbGroup::getNumberValues()
{
	return numberColumns;
}
