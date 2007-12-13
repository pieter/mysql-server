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

// DataPage.cpp: implementation of the DataPage class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "DataPage.h"
#include "DataOverflowPage.h"
#include "Dbb.h"
#include "BDB.h"
#include "Section.h"
#include "RecordLocatorPage.h"
#include "Stream.h"
#include "Validation.h"
#include "Log.h"
#include "LogLock.h"
#include "SerialLog.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


int DataPage::updateRecord(Section *section, int lineNumber, Stream *stream, TransId transId, bool earlyWrite)
{
	Dbb *dbb = section->dbb;
	LineIndex *index = lineIndex + lineNumber;
	int length = stream->totalLength;

	/* If there is an overflow page (or change), get rid of them now */

	if (index->length < 0)
		{
		char *ptr = (char*) this + index->offset;
		int32 overflowPageNumber;
		memcpy (&overflowPageNumber, ptr, sizeof (int32));

		if (overflowPageNumber && !dbb->serialLog->recovering)
			deleteOverflowPages (dbb, overflowPageNumber, transId);
		}

	/* If the new record is the same size or smaller than the predecessor,
	   just overwrite the old version.
	*/

	if (index->length >= length)
		{
		char *ptr = ((char*) this) + index->offset;
		stream->getSegment (0, length, ptr);
		index->length = (short) length;
		//validate(dbb);
		int spaceAvailable = computeSpaceAvailable(dbb->pageSize);
		
		return (spaceAvailable > 0) ? spaceAvailable : 1;
		}

	/* Find out how much space is used as well the current high water mark */

	short indexEnd = OFFSET (DataPage*, lineIndex) + maxLine * sizeof (LineIndex);
	short used = indexEnd;
	short highWater = dbb->pageSize;
	LineIndex *idx = lineIndex;
	LineIndex *end = idx + maxLine;

	for (int n = 0; idx < end; ++idx, ++n)
		{
		ASSERT (idx->offset == 0 || (idx->offset >= indexEnd && idx->offset + idx->length <= dbb->pageSize));
		
		if (idx->offset && n != lineNumber)
			{
			used += ABS(idx->length);
			
			if (idx->offset < highWater)
				highWater = idx->offset;
			}
		}

	/* If there isn't space for the whole thing, write to tail out separately */

	int overflowPageNumber = 0;
	int available = dbb->pageSize - used;

	/* If available is less that current, there is a bug (we know better),
	   [this is a work around of a unknown bug, 2/13/00, jas]

	if (available < index->length)
		{
		available = index->length;
		highWater = index->offset + index->length;
		}
	 */

	if (available < (int) sizeof(int32))
		return 0;
		
	if (available < length)
		{
		overflowPageNumber = section->storeTail(stream, available, &length, transId, earlyWrite);
		length += sizeof(int32);
		available -= sizeof(int32);
		}

	/* If there isn't room between line index and first record, compress */

	if (indexEnd > highWater - length)
		{
		index->length = 0;					// Take us out
		highWater = compressPage (dbb);
		}

	/* OK, move in the record */

	available -= length;
	highWater -= (short) length;
	ASSERT (highWater >= indexEnd);
	index->offset = highWater;
	char *record = (char*) this + highWater;

	if (overflowPageNumber)
		{
		memcpy (record, &overflowPageNumber, sizeof (int32));
		stream->getSegment (0, length - sizeof (int32), record + sizeof (int32));
		index->length = (short) -length;
		}
	else
		{
		index->length = length;
		stream->getSegment (0, length, record);
		}

	//validate(dbb);

	return (available > 0) ? available : 1;
}

int DataPage::compressPage(Dbb *dbb)
{
	char *buffer = new char [dbb->pageSize];
	DataPage *copy = (DataPage*) buffer;
	memcpy (copy, this, dbb->pageSize);
	int highWater = dbb->pageSize;

	for (LineIndex *index = lineIndex, *end = index + maxLine; index < end; ++index)
		if (index->offset)
			{
			int length = ABS (index->length);
			highWater -= length;
			memcpy ((char*) this + highWater, (char*) copy + index->offset, length);
			index->offset = highWater;
			}

	delete [] buffer;

	return highWater;
}

bool DataPage::fetchRecord(Dbb *dbb, int slot, Stream *stream)
{
	//ASSERT (slot < maxLine);
	
	if (slot >= maxLine)
		return false;

	int offset = lineIndex [slot].offset;
	int length = lineIndex [slot].length;
	
	if (length == 0 && offset == 0)
		return false;
		
	ASSERT (offset >= OFFSET (DataPage*, lineIndex) && offset + length <= dbb->pageSize);
	char *ptr = (char*) this + offset;

	if (length >= 0)
		{
		stream->putSegment (length, ptr, true);
		
		return true;
		}

	// Record is fragment, chase down tail

	int32 overflowPageNumber;
	memcpy (&overflowPageNumber, ptr, sizeof (int32));
	stream->putSegment (-length - 4, ptr + 4, true);

	while (overflowPageNumber)
		{
		Bdb *bdb = dbb->fetchPage (overflowPageNumber, PAGE_data_overflow, Shared);
		BDB_HISTORY(bdb);
		DataOverflowPage *page = (DataOverflowPage*) bdb->buffer;
		overflowPageNumber = page->nextPage;
		stream->putSegment (page->length, (char*) page + sizeof (DataOverflowPage), true);
		bdb->release(REL_HISTORY);
		}
	
	return true;
}

// Return the amount of space remaining on page; zero means "didn't fit"

int DataPage::storeRecord(Dbb *dbb, Bdb *bdb, RecordIndex * index, int length, Stream *stream, int32 overflowPageNumber, TransId transId, bool earlyWrite)
{
	if (overflowPageNumber)
		length += sizeof (int32);

	short id = -1;
	short highWater = dbb->pageSize;
	short used = OFFSET (DataPage*, lineIndex) + maxLine * sizeof (LineIndex);
	LineIndex *line, *end;

	for (line = lineIndex, end = line + maxLine; line < end; ++line)
		if (line->offset)
			{
			if (line->offset < highWater)
				highWater = line->offset;
				
			used += ABS (line->length);
			}
		else if (id == -1)
			id = (int) (line - lineIndex);

	if (id == -1)
		{
		used += sizeof (LineIndex);
		++end;
		}

	int spaceRemaining = dbb->pageSize - used - length;
	
	if (spaceRemaining < 0)
		return 0;

	bdb->mark(transId);

	/***
	if (earlyWrite)
		bdb->setWriter();
	***/

	if ((short) ((char*) end - (char*) this) > highWater - length)
		highWater = compressPage (dbb);

	if (id == -1)
		id = maxLine++;

	line = lineIndex + id;
	highWater -= (short) length;
	line->offset = highWater;
	char *record = (char*) this + highWater;

	if (overflowPageNumber)
		{
		memcpy (record, &overflowPageNumber, sizeof (int32));
		stream->getSegment (0, length - sizeof (int32), record + sizeof (int32));
		line->length = (short) -length;
		}
	else
		{
		stream->getSegment (0, length, record);
		line->length = (short) length;
		}

	index->page = bdb->pageNumber;
	index->line = id;
	/***
	Log::debug ("stored record, page %d (%x), line %d, length %d, offset %d\n",
			bdb->pageNumber, this, id, line->length, line->offset);
	***/
	//validate(dbb);

	return (spaceRemaining) ? spaceRemaining : 1;
}

// Delete segment and return amount of available space remaining

int DataPage::deleteLine (Dbb *dbb, int line, TransId transId)
{
	// Handle overflow pages first

	if (lineIndex [line].length < 0)
		{
		char *ptr = (char*) this + lineIndex [line].offset;
		int32 overflowPageNumber;
		memcpy (&overflowPageNumber, ptr, sizeof (int32));

		if (overflowPageNumber && !dbb->serialLog->recovering)
			deleteOverflowPages (dbb, overflowPageNumber, transId);
		}

	// Next, zap the line and recompute max line number

	lineIndex [line].offset = 0;
	lineIndex [line].length = 0;
    int max, n;
	int available = dbb->pageSize - OFFSET (DataPage*, lineIndex);
	
	for (max = 0, n = 0; n < maxLine; ++n)
		if (lineIndex [n].offset)
			{
			max = n + 1;
			available -= lineIndex[n].length;
			}

	maxLine = max;
	available -= maxLine * sizeof(lineIndex);
	
	if (available <= 0)
		available = 1;
		
	return (maxLine > 0) ? available : 0;
}

void DataPage::validate(Dbb *dbb)
{
	short indexEnd = OFFSET (DataPage*, lineIndex) + maxLine * sizeof (LineIndex);

	for (int n = 0; n < maxLine; ++n)
		{
		LineIndex *index = lineIndex + n;
		ASSERT (index->offset == 0 || (index->offset >= indexEnd && index->offset + index->length <= dbb->pageSize));
		}
}

void DataPage::deleteOverflowPages(Dbb * dbb, int32 overflowPageNumber, TransId transId)
{
	while (overflowPageNumber)
		{
		Bdb *bdb = dbb->fetchPage (overflowPageNumber, PAGE_data_overflow, Exclusive);
		BDB_HISTORY(bdb);
		DataOverflowPage *page = (DataOverflowPage*) bdb->buffer;
		overflowPageNumber = page->nextPage;
		dbb->freePage (bdb, transId);
		}
}

void DataPage::validate(Dbb *dbb, Validation *validation)
{
	for (int n = 0; n < maxLine; ++n)
		if (lineIndex [n].offset && lineIndex [n].length < 0)
			{
			char *ptr = (char*) this + lineIndex[n].offset;
			int32 overflowPageNumber;
			memcpy (&overflowPageNumber, ptr, sizeof (int32));
			
			while (overflowPageNumber)
				{
				Bdb *bdb = dbb->fetchPage (overflowPageNumber, PAGE_any, Shared);
				BDB_HISTORY(bdb);
				
				if (validation->isPageType (bdb, PAGE_data_overflow, "DataOverFlowPage"))
					{
					validation->inUse (overflowPageNumber, "overflow page number");
					DataOverflowPage *page = (DataOverflowPage*) bdb->buffer;
					overflowPageNumber = page->nextPage;
					}
				else
					overflowPageNumber = 0;
					
				bdb->release(REL_HISTORY);
				}
			}
}

void DataPage::analyze(Dbb *dbb, SectionAnalysis *analysis)
{
	++analysis->dataPages;
	int spaceUsed = sizeof (DataPage) + (maxLine - 1) * sizeof (LineIndex);

	for (int n = 0; n < maxLine; ++n)
		if (lineIndex [n].offset)
			{
			++analysis->records;
			int length = lineIndex [n].length;
			
			if (length < 0)
				{
				spaceUsed -= length;
				char *ptr = (char*) this + lineIndex[n].offset;
				int32 overflowPageNumber;
				memcpy (&overflowPageNumber, ptr, sizeof (int32));
				
				while (overflowPageNumber)
					{
					++analysis->overflowPages;
					Bdb *bdb = dbb->fetchPage (overflowPageNumber, PAGE_any, Shared);
					BDB_HISTORY(bdb);
					DataOverflowPage *page = (DataOverflowPage*) bdb->buffer;
					overflowPageNumber = page->nextPage;
					bdb->release(REL_HISTORY);
					}
				}
			else
				length += length;
			}

	analysis->spaceAvailable = dbb->pageSize - spaceUsed;
	}

int DataPage::computeSpaceAvailable(int pageSize)
{
	//int spaceUsed = sizeof(DataPage) - sizeof(LineIndex);  // Page overhead
	int spaceUsed = OFFSET(DataPage*, lineIndex);
	
	for (int n = 0; n < maxLine; ++n)
		if (lineIndex[n].offset)
			spaceUsed += ABS(lineIndex[n].length);
	
	int space = pageSize - maxLine * sizeof(LineIndex) - spaceUsed;
	
	if (space < 0)
		{
		Log::debug("DataPage::computeSpaceAvailable got a negative number\n");
		print();

		return 1;
		}
	
	return space;
}

void DataPage::deletePage(Dbb *dbb, TransId transId)
{
	for (int line = 0; line < maxLine; ++line)
		if (lineIndex[line].length < 0)
			{
			char *ptr = (char*) this + lineIndex[line].offset;
			int32 overflowPageNumber;
			memcpy (&overflowPageNumber, ptr, sizeof (int32));

			if (overflowPageNumber && !dbb->serialLog->recovering)
				deleteOverflowPages (dbb, overflowPageNumber, transId);
			}
}

void DataPage::print(void)
{
	LogLock lock;
#ifdef STORAGE_ENGINE
	Log::debug("Data page %d, max line %d\n", pageNumber, maxLine);
#else
	Log::debug("Data page, max line %d\n", maxLine);
#endif

	for (int n = 0; n < maxLine; ++n)
		{
		LineIndex *index = lineIndex + n;
		
		if (index->length || index->offset)
			Log::debug("    %d. offset %d, length %d\n", n, index->offset, index->length);
		}

}
