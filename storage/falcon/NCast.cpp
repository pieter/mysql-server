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

// NCast.cpp: implementation of the NCast class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NCast.h"
#include "Value.h"
#include "CompiledStatement.h"
#include "Statement.h"
#include "Syntax.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NCast::NCast(CompiledStatement *statement, Syntax *syntax) : NNode (statement, Cast, 1)
{
	children [0] = statement->compile (syntax->getChild (0));
	type = statement->getType (syntax->getChild(1));
}

NCast::~NCast()
{

}

Value* NCast::eval(Statement *statement)
{
	Value *source = children [0]->eval (statement);
	Value *value = statement->getValue (valueSlot);
	value->setValue (type, source);

	return value;
}
