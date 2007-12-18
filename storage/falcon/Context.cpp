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

// Context.cpp: implementation of the Context class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Context.h"
#include "Table.h"
#include "Statement.h"
#include "Record.h"
#include "Bitmap.h"
#include "Database.h"
#include "Sort.h"
#include "View.h"
#include "SQLError.h"
#include "Connection.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Context::Context(int id, Table *tbl, int32 privMask)
{
	table = tbl;
	contextId = id;
	computable = false;
	bitmap = NULL;
	sort = NULL;
	alias = NULL;
	privilegeMask = privMask;
	viewContexts = NULL;
	eof = true;
	record = NULL;
	select = false;
	type = CtxInnerJoin;
}

Context::Context()
{
	table = NULL;
	contextId = -1;
	bitmap = NULL;
	sort = NULL;
	privilegeMask = -1;
	viewContexts = NULL;
	alias = NULL;
	eof = true;
	record = NULL;
	select = false;
	type = CtxInnerJoin;
}

Context::~Context()
{
	close();

	if (bitmap)
		bitmap->release();

	delete sort;
	delete [] viewContexts;
}

bool Context::setComputable(bool flag)
{
	bool was = computable;
	computable = flag;

	return was;
}

void Context::initialize(Context * context)
{
	contextId = context->contextId;
	table = context->table;
	privilegeMask = context->privilegeMask;
}

bool Context::fetchNext(Statement * statement)
{
	if (eof)
		throw SQLEXCEPTION (RUNTIME_ERROR, "record stream is not open");

	for (;;)
		{
		if (record)
			close();
			
		Record *candidate = table->fetchNext(recordNumber);
		
		if (!candidate)
			{
			eof = true;
			
			return false;
			}
			
		++statement->stats.exhaustiveFetches;
		recordNumber = candidate->recordNumber + 1;
		record = candidate->fetchVersion (statement->transaction);
		
		if (record)
			{
			checkRecordLimits (statement);
			
			if (record != candidate)
				{
				record->addRef();
				candidate->release();
				}

			return true;
			}
			
		candidate->release();
		}
}

void Context::setBitmap(Bitmap * map)
{
	if (bitmap)
		bitmap->release();

	bitmap = map;

}

bool Context::fetchIndexed(Statement * statement)
{
	if (eof)
		throw SQLEXCEPTION (RUNTIME_ERROR, "record stream is not open");

	if (!bitmap)
		return false;

	for (;;)
		{
		if (record)
			close();
			
		recordNumber = bitmap->nextSet(recordNumber);
		
		if (recordNumber < 0)
			{
			eof = true;
			return false;
			}
			
		++statement->stats.indexHits;
		Record *candidate = table->fetch(recordNumber);
		++recordNumber;
		
		if (candidate)
			{
			++statement->stats.indexFetches;
			record = candidate->fetchVersion(statement->transaction);
			
			if (record)
				{
				checkRecordLimits(statement);
				
				if (record != candidate)
					{
					record->addRef();
					candidate->release();
					}
					
				return true;
				}
				
			candidate->release();
			}
		}
}

bool Context::isContextName(const char * name)
{
	return alias == name || (table && table->name == name);
}


void Context::getTableContexts(LinkedList & list, ContextType contextType)
{
	if (viewContexts)
		for (int n = 0; n < table->view->numberTables; ++n)
			viewContexts [n]->getTableContexts(list, contextType);
	else
		{
		type = contextType;
		list.append (this);
		}
}

void Context::open()
{
	recordNumber = 0;
	eof = false;
}

void Context::close()
{
	if (record)
		{
		record->release();
		record = NULL;
		}
}

void Context::setRecord(Record *rec)
{
	if (record)
		close();

	if ( (record = rec) )
		record->addRef();
}

void Context::checkRecordLimits(Statement *statement)
{
	++statement->stats.recordsFetched;
	Connection *connection = statement->connection;

	if (connection->maxRecords >= 0 && 
		++connection->recordCount >= connection->maxRecords)
		throw SQLEXCEPTION (RUNTIME_ERROR, "max connection records (%d) exceeded\n", connection->maxRecords);

	if (connection->maxRecords >= 0 && 
		++statement->stats.records >= connection->maxRecords)
		throw SQLEXCEPTION (RUNTIME_ERROR, "max statement records (%d) exceeded\n", connection->maxRecords);
}
