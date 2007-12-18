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

// FsbSort.cpp: implementation of the FsbSort class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "FsbSort.h"
#include "Sort.h"
#include "SortRecord.h"
#include "NNode.h"
#include "Value.h"
#include "Statement.h"
#include "Context.h"
#include "CompiledStatement.h"
#include "Collation.h"
#include "Log.h"
#include "PrettyPrint.h"
#include "Record.h"
#include "Connection.h"
#include "SQLError.h"

#define SORT_RUN_LENGTH		1000

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FsbSort::FsbSort(CompiledStatement *statement, NNode *expr, Fsb *source, SortType sortType)
{
	stream = source;
	node = expr;
	type = sortType;
	numberKeys = node->count;
	parameters = new SortParameters[numberKeys];
	sortSlot = statement->getSortSlot();
	
	for (int n = 0; n < node->count; ++n)
		if (type == order)
			{
			NNode *order = node->children [n];
			parameters[n].direction = order->children [1] != NULL;
			parameters[n].collation = order->children [0]->getCollation();
			}
		else
			{	
			parameters[n].direction = false;
			parameters[n].collation = NULL;
			}

	int *ptr = contextIds;
	getStreams(&ptr);
	numberContexts = ptr - contextIds;
}

FsbSort::~FsbSort()
{
	delete [] parameters;
	delete stream;
}

void FsbSort::open(Statement * statement)
{
	statement->deleteSort(sortSlot);
	stream->open(statement);
}

Row* FsbSort::fetch(Statement * statement)
{
	SortRecord *record = NULL;
	Sort *sort = statement->sorts[sortSlot];

	if (sort == NULL)
		{
		sort = new Sort(parameters, SORT_RUN_LENGTH, type == distinct);
		statement->sorts [sortSlot] = sort;
		
		for (;;)
			{
			Row *row = stream->fetch (statement);
			
			if (!row)
				break;
				
			Connection *connection = statement->connection;
			
			if (connection->maxSort >= 0 && 
				 ++connection->sortCount >= connection->maxSort)
				throw SQLEXCEPTION (RUNTIME_ERROR, "max connection sort records (%d) exceeded\n", 
									connection->maxSort);
									
			if (connection->statementMaxSort >= 0)
				{
				if (++statement->stats.recordsSorted >= connection->statementMaxSort)
					throw SQLEXCEPTION (RUNTIME_ERROR, "max statement sort records (%d) exceeded\n", 
										connection->statementMaxSort);
										
				if (statement->stats.recordsSorted == connection->largeSort)
					Log::debug ("Large (%d > %d) sort: %s\n", 
								statement->stats.recordsSorted,
								connection->largeSort,
								(const char*) statement->statement->sqlString);
				}
				
			if (node != NULL)
				{
				Record **records = NULL;
				
				if (numberContexts)
					{
					records = new Record*[numberContexts];
					
					for (int n = 0; n < numberContexts; ++n)
						{
						Record *record = statement->getContext(contextIds [n])->record;
						
						if (record)
							record->addRef();
							
						records [n] = record;
						}
					}
					
				record = new SortRecord (statement, row, numberKeys, numberContexts, records);
				
				for (int n = 0; n < numberKeys; ++n)
					{
					NNode *expr = node->children[n];
					
					if (type == order)
						expr = expr->children[0];
						
					record->keys[n].setValue(expr->eval(statement), true);
					}

				/***
				if (type != order)
					for (int n = 0; n < numberKeys; ++n)
						record->keys[n].setValue(node->children [n]->eval (statement), true);
				else
					for (int n = 0; n < numberKeys; ++n)
						record->keys[n].setValue(node->children [n]->children [0]->eval (statement), true);
				***/
				}
				
			sort->add(record);
			}
			
		sort->sort();
		}

	record = sort->fetch();

	if (record == NULL)
		{
		close(statement);
		
		return NULL;
		}

	for (int n = 0; n < numberContexts; ++n)
		statement->getContext(contextIds [n])->setRecord (record->object [n]);

	return record->row;
}

void FsbSort::close(Statement * statement)
{
	stream->close(statement);
	statement->deleteSort(sortSlot);
}

void FsbSort::prettyPrint(int level, PrettyPrint *pp)
{
	pp->indent(level++);
	pp->put("Sort\n");
	node->prettyPrint(level, pp);
	stream->prettyPrint(level, pp);
}

void FsbSort::getStreams(int **ptr)
{
	stream->getStreams(ptr);
}
