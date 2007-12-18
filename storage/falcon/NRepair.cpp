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

// NRepair.cpp: implementation of the NRepair class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NRepair.h"
#include "Syntax.h"
#include "CompiledStatement.h"
#include "Connection.h"
#include "Statement.h"
#include "Context.h"
#include "Fsb.h"
#include "Table.h"
#include "SQLError.h"
#include "Privilege.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NRepair::NRepair(CompiledStatement *statement, Syntax *syntax) : NDelete (statement, syntax, Repair)
{
}

NRepair::~NRepair()
{

}

void NRepair::evalStatement(Statement *statement)
{
	statement->updateStatements = true;
	stream->open (statement);
	//Transaction *transaction = statement->transaction;
	Context *context = statement->getContext (contextId);

	for (;;)
		{
		try
			{
			if (!stream->fetch (statement))
				break;
			}
		catch (SQLException&)
			{
			table->deleteRecord (context->recordNumber);
			++statement->recordsUpdated;
			++context->recordNumber;
			}
		}

	stream->close (statement);

	return;
}
