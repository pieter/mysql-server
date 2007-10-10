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

// MemoryResultSet.cpp: implementation of the MemoryResultSet class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "MemoryResultSet.h"
#include "MemoryResultSetColumn.h"
#include "MemoryResultSetMetaData.h"
#include "Value.h"
#include "SQLError.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

MemoryResultSet::MemoryResultSet()
{
	row = NULL;
	numberRows = 0;
	currentRow = 0;
}

MemoryResultSet::~MemoryResultSet()
{
	FOR_OBJECTS (Values*, values, &rows)
		delete values;
	END_FOR;

	FOR_OBJECTS (MemoryResultSetColumn*, object, &columns)
		delete object;
	END_FOR;
}

void MemoryResultSet::addColumn(const char * name, const char * label, int type, int displaySize, int precision, int scale)
{
	MemoryResultSetColumn *column = new MemoryResultSetColumn (name, label, type, displaySize, precision, scale);
	columns.append (column);
	++numberColumns;
}

void MemoryResultSet::addRow()
{
	row = new Values;
	row->alloc (numberColumns);
	rows.append (row);
	++numberRows;
}

void MemoryResultSet::setString(int index, const char * string)
{
	row->values [index-1].setString (string, true);
}

void MemoryResultSet::setInt(int index, int value)
{
	row->values [index-1].setValue (value);
}


void MemoryResultSet::setDouble(int index, double value)
{
	row->values [index-1].setValue (value);
}

bool MemoryResultSet::next()
{
	if (currentRow >= numberRows)
		{
		active = false;
		return false;
		}

	if (conversions)
		for (int n = 0; n < numberColumns; ++n)
			if (conversions [n])
				{
				delete conversions [n];
				conversions [n] = NULL;
				}

	row = (Values*) rows.getElement (currentRow++);
	active = true;

	return true;
}

const char* MemoryResultSet::getString(int id)
{
	Value *value = getValue (id);

	if (!conversions)
		allocConversions();

	if (conversions [id - 1])
		return conversions [id - 1];

	return value->getString (conversions + id - 1);
}


int MemoryResultSet::findColumnIndex(const char * name)
{
	return getColumnIndex (name);
}


int MemoryResultSet::findColumn(const WCString *name)
{
	return getColumnIndex (name);
}

int MemoryResultSet::getColumnIndex(const char * name)
{
	int n = 0;

	FOR_OBJECTS (MemoryResultSetColumn*, column, &columns)
		if (column->name.equalsNoCase (name))
			return n;
		++n;
	END_FOR;

	return -1;
}

int MemoryResultSet::getColumnIndex(const WCString *name)
{
	int n = 0;

	FOR_OBJECTS (MemoryResultSetColumn*, column, &columns)
		if (column->name.equalsNoCase (name))
			return n;
		++n;
	END_FOR;

	return -1;
}


Value* MemoryResultSet::getValue(int index)
{
	if (!active)
		throw SQLEXCEPTION (RUNTIME_ERROR, "no active row in result set ");

	if (index < 1 || index > numberColumns)
		throw SQLEXCEPTION (RUNTIME_ERROR, "invalid column index for result set");

	Value *value = &row->values [index - 1];
	valueWasNull = value->isNull();

	return value;

}

ResultSetMetaData* MemoryResultSet::getMetaData()
{
	if (!metaData)
		metaData = new MemoryResultSetMetaData (this);

	return metaData;
}

MemoryResultSetColumn* MemoryResultSet::getColumn(int index)
{
	void *column = columns.getElement (index - 1);

	if (!column)
		throw SQLEXCEPTION (RUNTIME_ERROR, "invalid column index for result set");

	return (MemoryResultSetColumn*) column;
}
