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

// Sequence.cpp: implementation of the Sequence class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Sequence.h"
#include "Database.h"
#include "Schema.h"
#include "SequenceManager.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Sequence::Sequence(Database *db, const char *sequenceSchema, const char *sequenceName, int sequenceId)
{
	schemaName = sequenceSchema;
	name = sequenceName;
	id = sequenceId;
	database = db;
	schema = database->getSchema (schemaName);
}

Sequence::~Sequence()
{

}

int64 Sequence::update(int64 delta, Transaction *transaction)
{
	int64 value = database->updateSequence (id, delta, transaction);

	if (schema->sequenceInterval)
		value = value * schema->sequenceInterval + schema->systemId;

	return value;
}

int64 Sequence::updatePhysical(int64 delta, Transaction *transaction)
{
	return database->updateSequence (id, delta, transaction);
}

void Sequence::rename(const char* newName)
{
	database->sequenceManager->renameSequence(this, newName);
}

Sequence* Sequence::recreate(void)
{
	return database->sequenceManager->recreateSequence(this);
}
