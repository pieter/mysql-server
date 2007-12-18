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

// NParameter.cpp: implementation of the NParameter class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "NParameter.h"
#include "Statement.h"
#include "Stream.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NParameter::NParameter(CompiledStatement *statement, int n) : NNode (statement, StatementParameter)
{
	number = n;
}

NParameter::~NParameter()
{

}

Value* NParameter::eval(Statement * statement)
{
	return statement->getParameter (number);
}

void NParameter::gen(Stream * stream)
{
	stream->putSegment ("?");
}

NNode* NParameter::copy(CompiledStatement * statement, Context * context)
{
	return new NParameter (statement, number);
}

bool NParameter::equiv(NNode *node)
{
	if (type != node->type || count != node->count)
		return false;

	if (number != ((NParameter*) node)->number)
		return false;

	return true;
}
