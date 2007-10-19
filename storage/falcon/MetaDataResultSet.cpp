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

// MetaDataResultSet.cpp: implementation of the MetaDataResultSet class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "MetaDataResultSet.h"
#include "SQLError.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

MetaDataResultSet::MetaDataResultSet(ResultSet *source) : ResultSet (source->numberColumns)
{
	resultSet = source;
	resultSet->addJavaRef();
	resultSet->release();
	select = resultSet->select;
}

MetaDataResultSet::~MetaDataResultSet()
{
	if (resultSet)
		resultSet->releaseJavaRef();
}

Value* MetaDataResultSet::getValue(const char *name)
{
	if (!active)
		throw SQLEXCEPTION (RUNTIME_ERROR, "no active row in result set ");

	int index = resultSet->getColumnIndex (name);

	return getValue (index + 1);
}

Value* MetaDataResultSet::getValue(int index)
{
	return resultSet->getValue (index);
}

bool MetaDataResultSet::next()
{
	deleteBlobs();
	values.clear();
	valueWasNull = -1;

	for (int n = 0; n < numberColumns; ++n)
		if (conversions [n])
			{
			delete conversions [n];
			conversions [n] = NULL;
			}

	return resultSet->next();
}

bool MetaDataResultSet::wasNull()
{
	return resultSet->wasNull();
}

void MetaDataResultSet::close()
{
	if (resultSet)
		{
		resultSet->releaseJavaRef();
		resultSet = NULL;
		}

	ResultSet::close();
}


Statement* MetaDataResultSet::getStatement()
{
	if (!resultSet)
		throw SQLError (RUNTIME_ERROR, "No statement is currently associated with ResultSet");

	return resultSet->getStatement();
}
