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

// NExists.cpp: implementation of the NExists class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NExists.h"
#include "CompiledStatement.h"
#include "Syntax.h"
#include "Statement.h"
#include "SQLError.h"
#include "NSelect.h"
#include "ResultSet.h"
#include "PrettyPrint.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NExists::NExists(CompiledStatement *statement, Syntax *syntax) : NNode (statement, Exists)
{
	select = (NSelect*) statement->compile (syntax->getChild (0));
}

NExists::~NExists()
{

}

int NExists::evalBoolean(Statement *statement)
{
	select->evalStatement (statement);
	ResultSet *resultSet = statement->getResultSet();
	int ret = FALSE_BOOLEAN;

	if (resultSet->next())
		ret = TRUE_BOOLEAN;

	resultSet->release();

	return ret;
}

bool NExists::computable(CompiledStatement *statement)
{
	return select->computable (statement);
}


void NExists::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent (level++);
	pp->put ("Exists\n");
	select->prettyPrint (level, pp);
}
