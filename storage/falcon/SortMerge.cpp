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

// SortMerge.cpp: implementation of the SortMerge class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SortMerge.h"
#include "SortRecord.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SortMerge::SortMerge(SortParameters *sortParameters, SortStream *strm1, SortStream *strm2)
{
	parameters = sortParameters;
	stream1 = strm1;
	stream2 = strm2;
	record1 = NULL;
	record2 = NULL;
	next = NULL;
}

SortMerge::~SortMerge()
{
	if (stream1)
		delete stream1;

	if (stream2)
		delete stream2;
}

SortRecord* SortMerge::fetch()
{
	SortRecord *record;

	if (!record1)
		{
		if ( (record = record2) )
			record2 = stream2->fetch();
		}
	else if (!record2 || record1->compare (parameters, record2) < 0)
		{
		record = record1;
		record1 = stream1->fetch();
		}
	else
		{
		record = record2;
		record2 = stream2->fetch();
		}

	return record;
}

void SortMerge::prepare()
{
	stream1->prepare();
	stream2->prepare();
	record1 = stream1->fetch();
	record2 = stream2->fetch();
}
