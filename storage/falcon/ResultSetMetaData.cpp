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

// ResultSetMetaData.cpp: implementation of the ResultSetMetaData class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "Engine.h"
#include "ResultSetMetaData.h"
#include "ResultSet.h"
#include "Field.h"
#include "Table.h"
#include "Statement.h"
#include "CompiledStatement.h"
#include "ValueEx.h"
#include "SQLError.h"
#include "SyncObject.h"
#include "Interlock.h"
#include "Collation.h"
#include "Value.h"
#include "Repository.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


ResultSetMetaData::ResultSetMetaData(ResultSet *results)
{
	resultSet = results;
	useCount = 1;
}

ResultSetMetaData::~ResultSetMetaData()
{

}

int ResultSetMetaData::getColumnCount()
{
	checkResultSet();

	return resultSet->numberColumns;
}

const char* ResultSetMetaData::getColumnName(int index)
{
	checkResultSet();

	return resultSet->getColumnName (index);
}

const char* ResultSetMetaData::getTableName(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field)
		return "";

	return field->table->getName();
}

JdbcType ResultSetMetaData::getColumnType(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field)
		return jdbcNULL;

	return field->getSqlType();
}

int ResultSetMetaData::getColumnDisplaySize(int column)
{
	checkResultSet();
	Field *field = resultSet->getField(column);

	if (!field)
		return 10;

	return field->getDisplaySize();
}

const char* ResultSetMetaData::getQuery()
{
	checkResultSet();
	Stream	stream;
	Statement *statement = resultSet->getStatement();
	int count = statement->getParameterCount();
	const char *sql = statement->statement->sqlString;
	const char *p;
	int n = 0;

	for (p = sql; *p;)
		{
		char c = *p++;
		if (c == '\'' || c == '"')
			{
			bool quote = false;
			while (*p && !(*p++ == c && !quote))
				quote = (quote) ? false : p [-1] == '\\';
			}
		else if (c == '?')
			{
			stream.putSegment ((int) (p - sql), sql, true);

			if (n < count)
				{
				char *temp;
				const char *value = statement->getParameter(n++)->getString (&temp);
				stream.putCharacter ('\'');
				for (char c; (c = *value++);)
					{
					stream.putCharacter (c);
					if (c == '\'')
						stream.putCharacter (c);
					}
				stream.putCharacter ('\'');
				stream.putCharacter (' ');
				if (temp)
					delete [] temp;
				}
			else
				stream.putCharacter ('?');
			sql = p;
			}
		}

	stream.putSegment (sql);
	sqlString = stream.getJString();
		
	return sqlString;
}

const char* ResultSetMetaData::getSchemaName(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field)
		return "";

	return field->table->schemaName;
}

int ResultSetMetaData::getPrecision(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field)
		return 10;

	return field->precision;
}

int ResultSetMetaData::getScale(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field)
		return 0;

	return field->scale;
}

const char* ResultSetMetaData::getCatalogName(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field)
		return "";

	return field->table->catalogName;
}

void ResultSetMetaData::addRef()
{
	INTERLOCKED_INCREMENT (useCount);

	if (resultSet)
		resultSet->addRef();
}

void ResultSetMetaData::addJavaRef()
{
	INTERLOCKED_INCREMENT (useCount);

	if (resultSet)
		resultSet->addJavaRef();
}

void ResultSetMetaData::release()
{
	ASSERT (useCount > 0);

	if (resultSet)
		resultSet->release();

	//INTERLOCKED_DECREMENT (useCount);
	if (INTERLOCKED_DECREMENT (useCount) == 0)
		delete this;
}

void ResultSetMetaData::releaseJavaRef()
{
	if (resultSet)
		resultSet->releaseJavaRef();

	//INTERLOCKED_DECREMENT (useCount);
	if (INTERLOCKED_DECREMENT (useCount) == 0)
		delete this;
}

bool ResultSetMetaData::isSearchable(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field)
		return Null;

	return (field->flags & SEARCHABLE) ? true : false;
}

bool ResultSetMetaData::isNullable(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field)
		return Null;

	return (field->flags & NOT_NULL) ? columnNoNulls : columnNullable;
}

void ResultSetMetaData::resultSetClosed()
{
	resultSet = NULL;
	release();
}

void ResultSetMetaData::checkResultSet()
{
	if (!resultSet)
		throw SQLEXCEPTION (RUNTIME_ERROR, "ResultSet for MetaDataResultSet has been closed");
}

const char* ResultSetMetaData::getCollationSequence(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field || !field->collation)
		return NULL;

	return field->collation->getName();
}

const char* ResultSetMetaData::getDefaultValue(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (field->defaultValue)
		return field->defaultValue->getString();

	return NULL;
}

const char* ResultSetMetaData::getColumnTypeName(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field)
		return "<unknown>";

	return field->getSqlTypeName();
}

const char* ResultSetMetaData::getRepositoryName(int index)
{
	checkResultSet();
	Field *field = resultSet->getField(index);

	if (!field || !field->repository)
		return NULL;

	return field->repository->name;
}
