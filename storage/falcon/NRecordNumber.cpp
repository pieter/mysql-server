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

// NRecordNumber.cpp: implementation of the NRecordNumber class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NRecordNumber.h"
#include "Context.h"
#include "CompiledStatement.h"
#include "Statement.h"
#include "Record.h"
#include "Value.h"
#include "SQLError.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NRecordNumber::NRecordNumber(CompiledStatement *statement, Context *context)
	: NNode (statement, RecordNumber)
{
	contextId = context->contextId;
	valueSlot = statement->getValueSlot();
}

NRecordNumber::~NRecordNumber()
{

}

Value* NRecordNumber::eval(Statement * statement)
{
	Value *value = statement->getValue (valueSlot);
	Context *context = statement->getContext (contextId);

	if (!context->record)
		throw SQLError (INTERNAL_ERROR, "missing record context for record_number");

	value->setValue (context->record->recordNumber);

	return value;
}

bool NRecordNumber::computable(CompiledStatement * statement)
{
	return statement->contextComputable (contextId);
}

NRecordNumber::NRecordNumber(CompiledStatement *statement): NNode (statement, RecordNumber)
{
}


const char* NRecordNumber::getName()
{
	return "RECORD_NUMBER";
}
