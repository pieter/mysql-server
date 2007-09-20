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

// NConnectionVariable.cpp: implementation of the NConnectionVariable class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "NConnectionVariable.h"
#include "CompiledStatement.h"
#include "Statement.h"
#include "Value.h"
#include "Connection.h"
#include "PrettyPrint.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NConnectionVariable::NConnectionVariable(CompiledStatement *statement, const char *variable) : NNode (statement, ConnectionVariable)
{
	valueSlot = statement->getValueSlot();
	name = variable;
}

NConnectionVariable::~NConnectionVariable()
{

}

Value* NConnectionVariable::eval(Statement *statement)
{
	Value *value = statement->getValue (valueSlot);
	const char *p = statement->connection->getAttribute (name, NULL);

	if (p)
		value->setString (p, true);
	else
		value->clear();

	return value;
}

bool NConnectionVariable::computable(CompiledStatement *statement)
{
	return true;
}

NNode* NConnectionVariable::copy(CompiledStatement *statement, Context *context)
{
	return new NConnectionVariable (statement, name);
}

void NConnectionVariable::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent (level++);
	pp->format ("Connection Variable %s\n", name);
}
