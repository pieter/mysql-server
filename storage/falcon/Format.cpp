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

// Format.cpp: implementation of the Format class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "Format.h"
#include "Table.h"
#include "Field.h"
#include "ResultSet.h"
#include "PreparedStatement.h"
#include "Database.h"
#include "Sync.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Format::Format(Table *tbl, int newVersion)
{
	table = tbl;
	version = newVersion;
	maxId = 0;
	saved = false;
	count = 0;

	FOR_FIELDS (field, table)
		maxId = MAX (maxId, field->id);
		++count;
	END_FOR;

	format = new FieldFormat [++maxId];
	memset (format, 0, sizeof (struct FieldFormat) * maxId);
	int offset = sizeof (short) + (count + 7) / 8;
	int index = 0;
	//printf ("Table %s.%s\n", table->schemaName, table->name);

	FOR_FIELDS (field, table)
		int id = field->id;
		FieldFormat *ff = format + id;
		int boundary = field->boundaryRequirement();
		offset = (offset + boundary - 1) / boundary * boundary;
		ff->type = field->type;
		ff->offset = offset;
		ff->length = field->getPhysicalLength();
		ff->scale = field->scale;
		ff->index = index++;
		//printf ("  %s offset %d, length %d\n", field->name, ff->offset, ff->length);
		offset += ff->length;			
	END_FOR;

	index = 0;

	for (int n = 0; n < maxId; ++n)
		{
		FieldFormat *ff = format + n;

		if (ff->offset != 0)
			ff->nullPosition = index++;
		}

	length = offset;
}

Format::Format(Table *tbl, ResultSet * resultSet)
{
	table = tbl;
	format = NULL;
	saved = false;

	while (resultSet->next())
		{
		int id = resultSet->getInt (2);

		if (!format)
			{
			maxId = resultSet->getInt (7);
			format = new FieldFormat [maxId];
			memset (format, 0, sizeof (struct FieldFormat) * maxId);
			version = resultSet->getInt (1);
			}

		FieldFormat *ff = format + id;
		ff->type = (Type) resultSet->getInt (3);
		ff->offset = resultSet->getInt (4);
		ff->length = resultSet->getInt (5);
		ff->scale = (short) resultSet->getInt (6);
		}

	ASSERT (format != NULL);
	length = 0;
	int position = 0;
	count = 0;

	for (int n = 0; n < maxId; ++n)
		{
		FieldFormat *ff = format + n;

		if (ff->offset != 0)
			{
			ff->nullPosition = position++;
			length = MAX (length, ff->offset + ff->length);
			ff->index = count++;
			}
		}
}

Format::~Format()
{
	if (format)
		delete format;
}

bool Format::validate(Table * table)
{
	FOR_FIELDS (field, table)
		if (field->id >= maxId)
			return false;

		int id = field->id;
		FieldFormat *ff = format + id;

		if (field->type != ff->type || 
			field->getPhysicalLength() != ff->length)
			return false;
	END_FOR;

	return true;
}


void Format::save(Table * table)
{
	if (saved)
		return;

	saved = true;
	Database *database = table->database;
	Sync sync (&database->syncSysConnection, "Format::save");
	sync.lock (Shared);

	PreparedStatement *statement = database->prepareStatement (
		"insert Formats (version, tableId, fieldId, datatype, offset, length, scale, maxId) values (?,?,?,?,?,?,?,?)");
	int tableId = table->tableId;

	for (int n = 0; n < maxId; ++n)
		{
		FieldFormat *ff = format + n;
		if (ff->offset != 0)
			{
			statement->setInt (1, version);
			statement->setInt (2, tableId);
			statement->setInt (3, n);
			statement->setInt (4, ff->type);
			statement->setInt (5, ff->offset);
			statement->setInt (6, ff->length);
			statement->setInt (7, ff->scale);
			statement->setInt (8, maxId);
			statement->executeUpdate();
			}
		}

	statement->close();
}
