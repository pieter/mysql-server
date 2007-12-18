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

// TableFilter.cpp: implementation of the TableFilter class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "TableFilter.h"
#include "Syntax.h"
#include "NNode.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TableFilter::TableFilter(const char *table, const char *schema, const char *aliasName, Syntax *filterSyntax)
{
	tableName = table;
	schemaName = schema;
	alias = aliasName;
	useCount = 1;
	syntax = filterSyntax;
}

TableFilter::~TableFilter()
{

}

void TableFilter::addRef()
{
	++useCount;
}

void TableFilter::release()
{
	if (--useCount <= 0)
		delete this;
}
