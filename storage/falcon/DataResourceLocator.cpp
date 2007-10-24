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

// DataResourceLocator.cpp: implementation of the DataResourceLocator class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "DataResourceLocator.h"
#include "QueryString.h"
#include "PreparedStatement.h"
#include "TemplateContext.h"
#include "Stream.h"
#include "Field.h"
#include "Table.h"
#include "ForeignKey.h"
#include "SQLError.h"
#include "Index.h"
#include "ResultSet.h"
#include "Connection.h"
#include "Application.h"

static const char *hex = "0123456789ABCDEF";

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DataResourceLocator::DataResourceLocator()
{

}

DataResourceLocator::~DataResourceLocator()
{

}


bool DataResourceLocator::getToken(char * * pChar, char * token)
{
	char *p = *pChar;

	while (*p && *p != ';' && *p != '(' && *p != ')' && *p != '=')
		*token++ = *p++;

	*token = 0;

	if (p == *pChar)
		return false;

	*pChar = p;

	return true;
}

void DataResourceLocator::copy(char** to, const char * from)
{
	char *p = *to;

	while (*from)
		*p++ = *from++;

	*to = p;
}

PreparedStatement* DataResourceLocator::prepareStatement(Connection * connection, const char * drl)
{
	QueryString queryString (drl);
	const char* locator;
	bool first = true;
	char valueBuffer [1024], *valuePtr = valueBuffer;
	char *values [16];
	char **value = values;
	char sql [1024], *q = sql;

	if (locator = queryString.getParameter (PARM_DRL, drl))
		{
		const char *p = locator;
		char c;
		copy (&q, "select ");

		// Pick up field list.  If none, toss in *

		if (*p == ';')
			copy (&q, "* ");
		else
			while (*p && *p != ';')
				*q++ = *p++;

		if (!*p++)
			throw SQLEXCEPTION (RUNTIME_ERROR, 
									"invalid Drl \"%.100s\"", drl);

		copy (&q, " from ");

		// Pick up table list

		if (*p == ';')
			copy (&q, "* ");
		else
			while (*p && (c = *p++) != ';')
				*q++ = c;

		// Pick up boolean, if any

		if (*p)
			if (*p == ';')
				++p;
			else
				{
				copy (&q, " where ");
				while (*p && (c = *p++) != ';')
					{
					if (c == '\'')
						{
						*q++ = '?';
						*value++ = valuePtr;
						while (c = *p++)
							if (c == '\'')
								break;
							else if (c == '\\')
								*valuePtr++ = *p++;
							else if (c == '+')
								*valuePtr++ = ' ';
							else
								*valuePtr++ = c;
						*valuePtr++ = 0;
						}
					else if (c == '+')
						*q++ = ' ';
					else
						*q++ = c;
					}
				}

		*q = 0;
		PreparedStatement *statement = connection->prepareStatement (sql);
		int count = value - values;
		
		for (int n = 0; n < count; ++n)
			statement->setString (n + 1, values [n]);

		return statement;
		}

	throw SQLEXCEPTION (RUNTIME_ERROR, "can't understand Drl \"%s\".", drl);
	
	return NULL;
}

void DataResourceLocator::copyValue(char **to, const char * from)
{
	char *q = *to;
	const UCHAR *p = (const UCHAR*) from;

	// Sick in leading single quote

	copy (&q, "=\'");

	// Copy value string, substituting for special characters (esp. blanks)

	for (UCHAR c; c = *p++;)
		if (c == ' ')
			*q++ = '+';
		else if (c < 128)
			{
			if (c == '\'' || c == '\\' || c == '%')
				*q++ = '\\';
			*q++ = c;
			}
		else
			{
			*q++ = '%';
			*q++ = hex[c >> 4];
			*q++ = hex[c & 0xf];
			}

	// Trim trailing blanks

	while (q [-1] == '+')
		--q;

	// And terminate value

	*q++ = '\'';
	*to = q;
}

void DataResourceLocator::genDrl(TemplateContext *context, ForeignKey *key)
{
	ResultSet *resultSet = context->resultSet;
	Stream *stream = context->stream;
	stream->putSegment ("&" PARM_DRL "=");
	char temp [256], *q = temp;

	*q++ = ';';

	if (!context->application->name.equalsNoCase (key->primaryTable->schemaName))
		{
		copy (&q, key->primaryTable->schemaName);
		*q++ = '.';
		}

	copy (&q, key->primaryTable->getName());
	*q++ = ';';

	for (int n = 0; n < key->numberFields; ++n)
		{
		if (n)
			copy (&q, "+and+");
		copy (&q, key->primaryFields [n]->getName());
		copyValue (&q, resultSet->getString (key->foreignFields [n]->getName()));
		}

	// Trim unnecessay clause separators

	while (q [-1] == ';')
		--q;

	q [0] = 0;
	stream->putSegment (q - temp, temp, true);
}

void DataResourceLocator::genDrl(TemplateContext *context, Index *index)
{
	Table *table = index->table;
	ResultSet *resultSet = context->resultSet;
	Stream *stream = context->stream;
	//putLocator (context, stream);
	stream->putSegment ("&" PARM_DRL "=");
	char temp [256], *q = temp;

	*q++ = ';';
	if (!context->application->name.equalsNoCase (table->schemaName))
		{
		copy (&q, table->schemaName);
		*q++ = '.';
		}

	copy (&q, table->getName());
	*q++ = ';';

	for (int n = 0; n < index->numberFields; ++n)
		{
		if (n)
			copy (&q, "+and+");
		Field *fld = index->fields [n];
		int index = resultSet->getIndex (fld);
		copy (&q, fld->getName());
		copyValue (&q, resultSet->getString (index));
		}

	// Trim unnecessay clause separators

	while (q [-1] == ';')
		--q;

	q [0] = 0;
	stream->putSegment (q - temp, temp, true);
}

JString DataResourceLocator::getLocator(ResultSet *resultSet, Index * index)
{
	Table *table = index->table;
	char temp [256], *q = temp;

	*q++ = ';';
	copy (&q, table->schemaName);
	*q++ = '.';
	copy (&q, table->getName());
	*q++ = ';';

	for (int n = 0; n < index->numberFields; ++n)
		{
		if (n)
			copy (&q, "+and+");
		Field *fld = index->fields [n];
		const char *name = fld->getName();
		const char *value = resultSet->getString (name);
		/***
		sprintf (q, "%s='%s'", name, value);
		while (*q) 
			++q;
		***/
		copy (&q, name);
		copyValue (&q, value);
		}

	// Trim unnecessay clause separators

	while (q [-1] == ';')
		--q;

	*q = 0;

	return temp;
}

JString DataResourceLocator::getLocator(ResultSet *resultSet, ForeignKey *key)
{
	char temp [256], *q = temp;

	*q++ = ';';
	copy (&q, key->primaryTable->schemaName);
	*q++ = '.';
	copy (&q, key->primaryTable->getName());
	*q++ = ';';

	for (int n = 0; n < key->numberFields; ++n)
		{
		if (n)
			copy (&q, "+and+");
		copy (&q, key->primaryFields [n]->getName());
		copyValue (&q, resultSet->getString (key->foreignFields [n]->getName()));
		}

	// Trim unnecessay clause separators

	while (q [-1] == ';')
		--q;

	*q = 0;

	return temp;
}
