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

// NField.cpp: implementation of the NField class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "NField.h"
#include "Context.h"
#include "CompiledStatement.h"
#include "Index.h"
#include "Record.h"
#include "Statement.h"
#include "Value.h"
#include "Field.h"
#include "Table.h"
#include "Stream.h"
#include "Log.h"
#include "PrettyPrint.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NField::NField(CompiledStatement *statement, Field *fld, Context *context) : NNode (statement, nField)
{
	field = fld;
	contextId = context->contextId;
	valueSlot = statement->getValueSlot();
}

NField::NField(CompiledStatement *statement) : NNode (statement, nField)
{

}

NField::~NField()
{

}

bool NField::computable(CompiledStatement * statement)
{
	return statement->contextComputable (contextId);
}

int NField::matchField(Context * context, Index * index)
{
	if (context->contextId == contextId)
		return index->matchField (field);

	return -1;
}

Value* NField::eval(Statement * statement)
{
	Value *value = statement->getValue(valueSlot);
	Context *context = statement->getContext(contextId);

	if (context->record)
		context->record->getValue(field->id, value);
	else
		value->setNull();

	return value;
}

Field* NField::getField()
{
	return field;
}

bool NField::isMember(Table * table)
{
	return field->table == table;
}


const char* NField::getName()
{
	return field->getName();
}


void NField::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent(level++);
	pp->format("Field %s.%s.%s (%d)\n", 
			field->table->getSchema(),
			field->table->getName(),
			field->getName(),
			contextId);
}

void NField::gen(Stream * stream)
{
	stream->format("ctx%d.", contextId);
	stream->putSegment(field->name);
}

FieldType NField::getType()
{
	FieldType type;
	type.type = field->type;
	type.scale = field->scale;
	type.length = field->length;
	type.precision = field->precision;

	return type;
}

NNode* NField::copy(CompiledStatement * statement, Context * context)
{
	ASSERT(context->viewContexts);
	Context *ctx = context->viewContexts [contextId];

	return new NField(statement, field, ctx);
}

bool NField::equiv(NNode *node)
{
	if (type != node->type || count != node->count)
		return false;

	if (field != ((NField*) node)->field ||
		contextId != ((NField*) node)->contextId)
		return false;

	return true;
}

Collation* NField::getCollation()
{
	return field->collation;
}
