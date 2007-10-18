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

// NSequence.cpp: implementation of the NSequence class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NSequence.h"
#include "Database.h"
#include "Value.h"
#include "Statement.h"
#include "Sequence.h"
#include "Syntax.h"
#include "SQLError.h"
#include "CompiledStatement.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NSequence::NSequence(CompiledStatement *statement, Syntax *syntax) : NNode (statement, NextValue, 0)
{
	Syntax *identifier = syntax->getChild (0);
	sequence = statement->findSequence (identifier, true);

	if (!sequence)
		throw SQLEXCEPTION (COMPILE_ERROR, "can't find sequence \"%s\"",
				identifier->getChild (identifier->count - 1)->getString());
}

NSequence::NSequence(CompiledStatement *statement, Sequence *seq) : NNode (statement, NextValue)
{
	sequence = seq;
}

NSequence::~NSequence()
{

}

NNode* NSequence::copy(CompiledStatement *statement, Context *context)
{
	return new NSequence (statement, sequence);
}

Value* NSequence::eval(Statement *statement)
{
	Value *value = statement->getValue (valueSlot);
	value->setValue (sequence->update (1, statement->transaction));

	return value;
}

FieldType NSequence::getType()
{
	FieldType type;
	type.type = Quad;
	type.precision = 19;
	type.scale = 0;
	type.length = sizeof (int64);

	return type;
}


const char* NSequence::getName()
{
	return sequence->name;
}
