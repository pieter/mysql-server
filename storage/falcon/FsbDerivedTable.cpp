/* Copyright (C) 2007 MySQL AB

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


#include "Engine.h"
#include "FsbDerivedTable.h"
#include "NSelect.h"
#include "PrettyPrint.h"

FsbDerivedTable::FsbDerivedTable(NSelect *node)
{
	select = node;
}

FsbDerivedTable::~FsbDerivedTable(void)
{
}

void FsbDerivedTable::open(Statement* statement)
{
	select->stream->open(statement);
}

Row* FsbDerivedTable::fetch(Statement* statement)
{
	return select->stream->fetch(statement);
}

void FsbDerivedTable::close(Statement* statement)
{
	select->stream->close(statement);
}

const char* FsbDerivedTable::getType(void)
{
	return "Derived Table\n";
}

void FsbDerivedTable::prettyPrint(int level, PrettyPrint* pp)
{
	pp->indent (level++);
	pp->put (getType());
	select->prettyPrint(level, pp);
}
