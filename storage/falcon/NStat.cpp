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

// NStat.cpp: implementation of the NStat class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NStat.h"
#include "Syntax.h"
#include "Value.h"
#include "Statement.h"
#include "CompiledStatement.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NStat::NStat(CompiledStatement *statement, NType typ, Syntax *syntax) : NNode (statement, typ, 1)
{
	Syntax *expr = syntax->getChild (1);

	if (expr)
		children [0] = statement->compile (expr);

	if (type == Avg)
		countSlot = statement->getValueSlot();

	valueSlot = statement->getValueSlot();
	distinct = syntax->getChild (0) != NULL;
}

NStat::~NStat()
{

}

void NStat::reset(Statement * statement)
{
	Value *value = statement->getValue (valueSlot);

	switch (type)
		{
		case Sum:
		case Count:
			value->setValue ((int) 0);
			break;

		case Avg:
			statement->getValue (countSlot)->setValue ((int) 0);
		default:
			value->clear();
		}
}

bool NStat::isStatistical()
{
	return true;
}

void NStat::increment(Statement * statement)
{
	Value *value = statement->getValue(valueSlot);
	Value *incr = NULL;
	Type valueType = value->getType();
	Type incrType = Null;
	
	if (children [0])
		{
		incr = children [0]->eval (statement);
		incrType = incr->getType();
		}	

	switch (type)
		{
		case Count:
			if (!incr || incrType != Null)
				value->setValue ((int) (value->getInt() + 1));
			break;

		case Max:
			if ((incrType != Null) && (valueType == Null || incr->compare(value) > 0))
				value->setValue (incr, true);
			break;

		case Min:
			if ((incrType != Null) && (valueType == Null || incr->compare(value) < 0))
				value->setValue (incr, true);
			break;

		case Avg:
			{
			Value *count = statement->getValue (countSlot);
			
			if (incrType == Null)
				break;
				
			count->add (1);
			}
		case Sum:
			if (incrType != Null)
				value->add (incr);
			break;
		
		default:
			break;
		}
}

Value* NStat::eval(Statement * statement)
{
	Value *value = statement->getValue (valueSlot);

	if (type == Avg)
		{
		Value *count = statement->getValue (countSlot);
		if (count->getInt() == 0)
			value->setNull();
		else
			value->divide (count);
		}

	return value;
}

const char* NStat::getName()
{
	switch (type)
		{
		case Count:		return "COUNT";
		case Max:		return "MAX";
		case Min:		return "MIN";
		case Sum:		return "SUM";
		case Avg:		return "AVG";
		
		default:
			return "";
		}
}
