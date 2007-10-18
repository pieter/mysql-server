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

// NSelectExpr.cpp: implementation of the NSelectExpr class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NSelectExpr.h"
#include "CompiledStatement.h"
#include "Syntax.h"
#include "Value.h"
#include "Statement.h"
#include "SQLError.h"
#include "NSelect.h"
#include "ResultSet.h"
#include "PrettyPrint.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NSelectExpr::NSelectExpr(CompiledStatement *statement, Syntax *syntax) : NNode (statement, SelectExpr)
{
	valueSlot = statement->getValueSlot();
	select = (NSelect*) statement->compile (syntax->getChild (0));
}

NSelectExpr::~NSelectExpr()
{

}

Value* NSelectExpr::eval(Statement *statement)
{
	Value *value = statement->getValue (valueSlot);
	select->evalStatement (statement);
	ResultSet *resultSet = statement->getResultSet();

	if (resultSet->next())
		value->setValue (resultSet->getValue (1), true);
	else
		value->clear();

	resultSet->release();

	return value;
}

void NSelectExpr::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent (level++);
	pp->put ("Select Expression\n");
	select->prettyPrint (level, pp);
}

