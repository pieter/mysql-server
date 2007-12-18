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

// NAlias.cpp: implementation of the NAlias class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NAlias.h"
#include "CompiledStatement.h"
#include "Syntax.h"
#include "Stream.h"
#include "PrettyPrint.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NAlias::NAlias(CompiledStatement *statement, Syntax *syntax) : NNode (statement, ValueAlias)
{
	expr = statement->compile (syntax->getChild (0));
	Syntax *node = syntax->getChild (1);
	ASSERT (node->type == nod_name);
	name = node->getString();

}

NAlias::~NAlias()
{
}

Value* NAlias::eval(Statement * statement)
{
	return expr->eval (statement);
}

const char* NAlias::getName()
{
	return name;
}

Field* NAlias::getField()
{
	return expr->getField();
}

void NAlias::gen(Stream * stream)
{
	expr->gen (stream);
	stream->putSegment (" as ");
	stream->putSegment (name);
}

NNode* NAlias::copy(CompiledStatement * statement, Context * context)
{
	NAlias *node = new NAlias (statement, name);
	node->expr = expr->copy (statement, context);

	return node;
}

NAlias::NAlias(CompiledStatement * statement, JString alias) : NNode (statement, ValueAlias)
{
	name = alias;
}

bool NAlias::isStatistical()
{
	return expr->isStatistical();
}

void NAlias::increment(Statement *statement)
{
	expr->increment(statement);
}

void NAlias::reset(Statement *statement)
{
	expr->reset(statement);
}

bool NAlias::equiv(NNode *node)
{
	return expr->equiv (node);
}

FieldType NAlias::getType()
{
	return expr->getType();
}

void NAlias::prettyPrint(int level, PrettyPrint* pp)
{
	pp->indent (level++);
	pp->put ("Alias\n");
	expr->prettyPrint (level, pp);
}
