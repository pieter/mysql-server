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

// TableAttachment.cpp: implementation of the TableAttachment class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "TableAttachment.h"
#include "RecordVersion.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TableAttachment::TableAttachment(int32 flags)
{
	mask = flags;
}

TableAttachment::~TableAttachment()
{
}

void TableAttachment::postCommit(Table *table, RecordVersion * record)
{
	if (!record->priorVersion)
		insertCommit (table, record);
	else if (!record->hasRecord())
		deleteCommit (table, record->priorVersion);
	else
		updateCommit (table, record);
}

void TableAttachment::insertCommit(Table * table, RecordVersion * record)
{
}

void TableAttachment::updateCommit(Table * table, RecordVersion * record)
{
}

void TableAttachment::deleteCommit(Table * table, Record *record)
{
}

void TableAttachment::preInsert(Table * table, RecordVersion * record)
{
}

void TableAttachment::preUpdate(Table * table, RecordVersion * record)
{
}

void TableAttachment::tableDeleted(Table *table)
{
}

void TableAttachment::preDelete(Table *table, RecordVersion *record)
{
}
