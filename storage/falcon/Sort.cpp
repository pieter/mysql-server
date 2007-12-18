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

// Sort.cpp: implementation of the Sort class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Sort.h"
#include "SortRecord.h"
#include "SortRun.h"
#include "SortMerge.h"
#include "Stack.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Sort::Sort(SortParameters *sortParameters, int runLen, bool flushDups)
{
	runLength = runLen;
	run = new SortRun (runLength);
	flushDuplicates = flushDups;
	parameters = sortParameters;
	merge = NULL;
}

Sort::~Sort()
{
	while (run)
		{
		SortRun *prior = run;
		run = (SortRun*) prior->next;
		delete prior;
		}

	if (merge)
		delete merge;
}

void Sort::add(SortRecord * record)
{
	if (!run->add (record))
		{
		run->sort (parameters, flushDuplicates);
		SortRun *newRun = new SortRun (runLength);
		newRun->next = run;
		run = newRun;
		
		if (!run->add (record))
			ASSERT (false);
		}
}

void Sort::sort()
{
	run->sort (parameters, flushDuplicates);
	prior = NULL;
	merge = run;
	run = NULL;

	// Build binary merge tree from runs

	while (merge->next)
		{
		SortStream *streams = NULL;
		
		while (merge)
			if (merge->next)
				{
				SortStream *stream = new SortMerge (parameters, merge, merge->next);
				stream->next = streams;
				streams = stream;
				merge = merge->next->next;
				}
			else
				{
				merge->next = streams;
				streams = merge;
				break;
				}
				
		merge = streams;
		}

	merge->prepare();
}

SortRecord* Sort::fetch()
{
	for (;;)
		{
		SortRecord *record = merge->fetch();
		
		if (!record)
			return record;
			
		if (!flushDuplicates || prior == NULL ||
			record->compare (parameters, prior) != 0)
			{
			prior = record;
			return record;
			}
		}
}

void Sort::setDescending(int key)
{
	parameters[key].direction = true;
	//direction [key] = true;
}
