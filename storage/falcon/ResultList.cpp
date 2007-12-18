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

// ResultList.cpp: implementation of the ResultList class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "ResultList.h"
#include "Table.h"
#include "Field.h"
#include "Database.h"
#include "Connection.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "SQLError.h"
#include "Bitmap.h"
#include "Log.h"
#include "Interlock.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ResultList::ResultList(Statement *stmt)
{
	parent = stmt;
	tableFilter = parent->tableFilter;
	fieldFilter = NULL;
	init (stmt->connection);
}


ResultList::ResultList(Connection *connection, Field *field)
{
	parent = NULL;
	tableFilter = NULL;
	fieldFilter = field;
	init (connection);
}

void ResultList::init(Connection *cnct)
{
	connection = cnct;
	database = connection->database;
	count = 0;
	hits.next = &hits;
	hits.prior = &hits;
	useCount = 1;
	nextHit = &hits;
	statement = NULL;
}

ResultList::~ResultList()
{
	clearStatement();

	while (hits.next != &hits)
		{
		SearchHit *node = hits.next;
		hits.next = node->next;
		delete node;
		}

	if (statement)
		{
		statement->release();
		statement = NULL;
		}
}

SearchHit* ResultList::add(int32 tableId, int32 recordNumber, int32 fieldId, double score)
{
	if (tableFilter && !tableFilter->isSet (tableId))
		return NULL;

	if (fieldFilter && !(fieldFilter->id == fieldId && fieldFilter->table->tableId == tableId))
		return NULL;

	++count;
	SearchHit *hit = new SearchHit (tableId, recordNumber, score);
	hit->prior = hits.prior;
	hit->next = &hits;
	hits.prior->next = hit;
	hits.prior = hit;

	return hit;
}

void ResultList::sort()
{
	SearchHit **records = new SearchHit* [count], **ptr = records;

	for (SearchHit *hit = hits.next; hit != &hits; hit = hit->next)
		if (database->getTable (hit->tableId))
			*ptr++ = hit;

	int size = count = (int) (ptr - records);
	int i, j, r, stack = 0;
	int low [128];
	int high [128];
	SearchHit *temp;

	if (size > 2)
		{
		low [stack] = 0;
		high [stack++] = size - 1;
		}

	while (stack > 0)
		{
		SearchHit *key;
		r = low [--stack];
		j = high [stack];
		//Debug.print ("sifting " + r + " thru " + j + ": " + range (r, j));
		key = records [r];
		//Debug.print (" key", key);
		int limit = j;
		i = r + 1;
		
		for (;;)
			{
			while (records [i]->score >= key->score && i < limit)
				++i;
				
			while (records [j]->score <= key->score && j > r)
				--j;
				
			//Debug.print ("  i " + i, records [i]);
			//Debug.print ("  j " + j, records [j]);
			
			if (i >= j)
				break;
				
			temp = records [i];
			records [i] = records [j];
			records [j] = temp;
			}
			
		i = high [stack];
		records [r] = records [j];
		records [j] = key;
		//Debug.print (" midpoint " + j + ": " +  range (r, i));
		
		if ((j - 1) - r >= 2)
			{
			low [stack] = r;
			high [stack++] = j - 1;
			}
			
		if (i - (j + 1) >= 2)
			{
			low [stack] = j + 1;
			high [stack++] = i;
			}
		}

	int n;

	for (n = 1; n < size; ++n)
		if (records [n - 1]->score < records [n]->score)
			{
			//Debug.print ("Flipping");
			temp = records [n - 1];
			records [n - 1] = records [n];
			records [n] = temp;
			}

	hits.next = hits.prior = &hits;

	for (n = 0; n < size; ++n)
		{
		SearchHit *hit = records [n];
		hit->prior = hits.next;
		hit->next = &hits;
		hits.prior->next = hit;
		hits.prior = hit;
		}

	delete records;
}

void ResultList::addRef()
{
	INTERLOCKED_INCREMENT (useCount);

}

void ResultList::release()
{
	if (INTERLOCKED_DECREMENT (useCount) == 0)
		delete this;
}

void ResultList::print()
{
	for (SearchHit *hit = hits.next; hit != &hits; hit = hit->next)
		{
		Table *table = database->getTable (hit->tableId);
		if (!table)
			database->getTable (hit->tableId);
		Log::debug ("Table %s (%d) record %d score %f\n",
				(table) ? table->getName() : "???",
				hit->tableId, 
				hit->recordNumber, hit->score);
		}

	Log::debug ("%d hits\n", count);
}

bool ResultList::next()
{
	if (nextHit->next == &hits)
		return false;

	nextHit = nextHit->next;

	return true;
}

ResultSet* ResultList::fetchRecord()
{
	if (!nextHit)
		throw SQLEXCEPTION (RUNTIME_ERROR, "no current search hit");

	Table *table = database->getTable (nextHit->tableId);

	if (!table)
		return NULL;

	char sql [256];
	sprintf (sql, "select * from %s.%s where record_number = ?", 
			 (const char*) table->schemaName, (const char*) table->name);

	if (statement)
		{
		statement->close();
		statement = NULL;
		}

	PreparedStatement *statement = connection->prepareStatement (sql);
	statement->setInt (1, nextHit->recordNumber);
	ResultSet *resultSet = statement->executeQuery();
	statement->release();

	return resultSet;
}

void ResultList::close()
{
	delete this;
}

int ResultList::getCount()
{
	return count;
}

double ResultList::getScore()
{
	if (nextHit)
		return nextHit->score;

	return 0;
}

const char* ResultList::getTableName()
{
	if (!nextHit)
		return "";

	Table *table = database->getTable (nextHit->tableId);

	if (!table)
		return "";
			
	return table->getName();
}


const char* ResultList::getSchemaName()
{
	if (!nextHit)
		return "";

	Table *table = database->getTable (nextHit->tableId);

	if (!table)
		return "";
			
	return table->getSchema();
}

void ResultList::clearStatement()
{
	if (parent)
		{
		parent->deleteResultList (this);
		parent = NULL;
		}

}

