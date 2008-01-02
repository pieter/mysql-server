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

// Section.cpp: implementation of the Section class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include "Engine.h"
#include "Section.h"
#include "SectionPage.h"
#include "RecordLocatorPage.h"
#include "Dbb.h"
#include "BDB.h"
#include "DataPage.h"
#include "DataOverflowPage.h"
#include "Stream.h"
#include "SQLException.h"
#include "Validation.h"
#include "Log.h"
#include "Sync.h"
#include "SerialLog.h"
#include "SerialLogControl.h"
#include "SRLSectionPromotion.h"
#include "SRLRecordLocator.h"
#include "SRLSectionLine.h"
#include "SRLOverflowPages.h"
#include "Transaction.h"
#include "PageInventoryPage.h"

//#define STOP_PAGE	114
//#define STOP_SECTION	40

#ifdef STOP_SECTION
static int stopSection = 40;
#endif

static const int MAX_LEVELS			= 4;

//#define VALIDATE_SPACE_SLOTS(page)		page->validateSpaceSlots();

#ifndef VALIDATE_SPACE_SLOTS
#define VALIDATE_SPACE_SLOTS(page)
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


Section::Section(Dbb *pDbb, int32 id, TransId transId)
{
	dbb = pDbb;
	sectionId = id;
	getSectionRoot();
	//dataPage = 0;
	nextLine = 0;
	level = -1;
	reservedRecordNumbers = NULL;
	freeLines = NULL;
	syncObject.setName("Section::syncObject");
	syncInsert.setName("Section::syncInsert");
}

Section::~Section()
{
	if (reservedRecordNumbers)
		reservedRecordNumbers->release();

	if (freeLines)
		freeLines->release();
}

int32 Section::createSection(Dbb * dbb, TransId transId)
{
	SectionPage	*sections = NULL;
	SectionPage *page;
	int sequence = -1;
	Bdb *sectionsBdb = NULL;
	int32 sectionSkipped = 0;
	
	for (int32 id = dbb->nextSection;; ++id)
		{
		int n = id / dbb->pagesPerSection;

		if (n != sequence)
			{
			sequence = n;

			if (sectionsBdb)
				sectionsBdb->release(REL_HISTORY);

			sectionsBdb = getSectionPage(dbb, SECTION_ROOT, n, Exclusive, transId);
			BDB_HISTORY(sectionsBdb);
			sections = (SectionPage*) sectionsBdb->buffer;
			}

		int slot = id % dbb->pagesPerSection;

		if (sections->pages[slot] == 0)
			{
			if (dbb->sectionInUse(id))
				{
				if (!sectionSkipped)
					sectionSkipped = id;
				}
			else
				{
				Bdb *sectionBdb = dbb->allocPage(PAGE_sections, transId);
				BDB_HISTORY(sectionBdb);
				int32 sectionPageNumber = sectionBdb->pageNumber;
				page = (SectionPage*) sectionBdb->buffer;
				page->section = id;
				sectionBdb->release(REL_HISTORY);
				
				dbb->setPrecedence(sectionsBdb, sectionPageNumber);
				sectionsBdb->mark(transId);
				sections->pages [slot] = sectionPageNumber;
				dbb->nextSection = (sectionSkipped) ? sectionSkipped : (id + 1);

				if (!dbb->serialLog->recovering)
					dbb->serialLog->logControl->sectionPage.append(dbb, transId, sectionsBdb->pageNumber, sectionPageNumber, slot, id, 0, 0);
					
				sectionsBdb->release(REL_HISTORY);
				//Log::debug("Section::createSection: section %d created\n", id);

				return id;
				}
			}
		}
}


void Section::createSection(Dbb *dbb, int32 sectionId, TransId transId)
{
	//Log::debug("Section::createSection: recreating section %d\n", sectionId);
	int sequence = sectionId / dbb->pagesPerSection;
	Bdb *sectionsBdb = getSectionPage (dbb, SECTION_ROOT, sequence, Exclusive, transId);
	BDB_HISTORY(sectionsBdb);
	SectionPage *sections = (SectionPage*) sectionsBdb->buffer;
	int slot = sectionId % dbb->pagesPerSection;

	if (sections->pages [slot] == 0)
		{
		sectionsBdb->mark(transId);
		Bdb *sectionBdb = dbb->allocPage(PAGE_sections, transId);
		BDB_HISTORY(sectionBdb);
		Log::debug("Section::createSection: recreating section %d, root %d\n", 
					sectionId, sectionBdb->pageNumber);
		sections->pages [slot] = sectionBdb->pageNumber;
		SectionPage	 *page = (SectionPage*) sectionBdb->buffer;
		page->section = sectionId;
		sectionBdb->release(REL_HISTORY);
		}

	sectionsBdb->release(REL_HISTORY);
}

Bdb* Section::getSectionPage(Dbb *dbb, int32 root, int32 sequence, LockType requestedLockType, TransId transId)
{
	LockType lockTypes [4];

	for (int n = 0; n < 4; ++n)
		lockTypes[n] = Shared;

	lockTypes [0] = requestedLockType;
	int level = -1;
	int slots[MAX_LEVELS];

	for (;;)
		{
		LockType lockType = (level < 0) ? requestedLockType : lockTypes[level];
		Bdb *bdb = dbb->fetchPage(root, PAGE_any, lockType);
		BDB_HISTORY(bdb);
		SectionPage *page = (SectionPage*) bdb->buffer;
		ASSERT(page->pageType == PAGE_sections || page->pageType == 0);
		level = page->level;

		// Knock off the simple case first

		if (page->level == 0 && sequence == 0)
			return bdb;

		// If we need to create another level, do it here.

		if (!decomposeSequence(dbb, sequence, level, slots))
			{
			if (lockType != Exclusive)
				{
				bdb->release(REL_HISTORY);
				lockTypes[level] = Exclusive;

				continue;
				}

			// Keep the old root, but copy contents, bump level, etc.

			bdb->mark(transId);
			Bdb *newBdb = dbb->allocPage(PAGE_sections, transId);
			BDB_HISTORY(newBdb);
			
			if (!dbb->serialLog->recovering)
				dbb->serialLog->logControl->sectionPromotion.append(dbb, page->section, bdb->pageNumber, dbb->pageSize, 
																	(const UCHAR*) page, newBdb->pageNumber);

			SectionPage *newPage = (SectionPage*) newBdb->buffer;
			memcpy(newPage, page, dbb->pageSize);
			newBdb->setPageHeader(newPage->pageType);
			memset(page, 0, dbb->pageSize);
			bdb->setPageHeader(PAGE_sections);
			page->section = newPage->section;
			page->level = newPage->level + 1;
			page->pages[0] = newBdb->pageNumber;
			newBdb->release(REL_HISTORY);
			bdb->release(REL_HISTORY);

			continue;
			}

		for (;;)
			{
			int slot = slots [page->level - 1];
			int32 pageNumber = page->pages [slot];

			if (pageNumber)
				{
				if (page->level == 1)
					{
					bdb = dbb->handoffPage(bdb, pageNumber, PAGE_sections, requestedLockType);
					BDB_HISTORY(bdb);
					
					return bdb;
					}

				lockType = lockTypes [page->level - 1];
				bdb = dbb->handoffPage(bdb, pageNumber, PAGE_sections, lockType);
				BDB_HISTORY(bdb);
				page = (SectionPage*) bdb->buffer;
				}
			else
				{
				if (lockType != Exclusive)
					{
					bdb->release(REL_HISTORY);
					lockTypes [page->level] = Exclusive;
					requestedLockType = Exclusive;
					level = -1;
					break;
					}

				Bdb *newBdb = dbb->allocPage(PAGE_sections, transId);
				BDB_HISTORY(newBdb);
				SectionPage *newPage = (SectionPage*) newBdb->buffer;
				int32 newPageNumber = newBdb->pageNumber;
				int sectionId = page->section;
				newPage->section = sectionId;
				newPage->sequence = sequence;
				newPage->level = page->level - 1;

				dbb->setPrecedence(bdb, newPageNumber);
				bdb->mark(transId);
				page->pages [slot] = newPageNumber;
				int32 parentPage = bdb->pageNumber;
				bdb->release(REL_HISTORY);

				if (newPage->level == 0)
					return newBdb;

				bdb = newBdb;
				page = newPage;
				lockType = Exclusive;

				if (!dbb->serialLog->recovering)
					dbb->serialLog->logControl->sectionPage.append(dbb, transId, parentPage, newPageNumber, slot, sectionId, sequence, page->level - 1);
				}
			}
		}
}

int32 Section::insertStub(TransId transId)
{
	int32 segment = -1;
	Bdb *bdb = NULL;
	int linesPerPage = dbb->linesPerPage;
	int linesPerSection = dbb->pagesPerSection * linesPerPage;
	Sync sync(&syncInsert, "Section::insertStub");
	sync.lock(Exclusive);

	for (int32 line = (freeLines) ? 0 : nextLine;; ++line)
		{
		ASSERT(sync.state == Exclusive);
		
		// If there are some recently released free record numbers, use them first

		if (freeLines)
			{
			line = freeLines->nextSet(line);

			if (line >= 0)
				freeLines->clear(line);
			else
				{
				freeLines->release();
				freeLines = NULL;
				line = nextLine;
				}
			}

		// If the recordNumber is still in use, don't use it

		if (reservedRecordNumbers && reservedRecordNumbers->isSet(line))
			continue;

		sync.unlock();
		int32 indexSequence = line / linesPerPage;
		RecordLocatorPage *page;

		// OK, go see if there's space there.  If the whole section page is
		// full, don't bother

		if (bdb == NULL || indexSequence != segment)
			{
			int sequence = indexSequence / dbb->pagesPerSection;

			if (bdb)
				bdb->release(REL_HISTORY);

			bdb = getSectionPage(sequence, Exclusive, transId);
			BDB_HISTORY(bdb);
			SectionPage *pages = (SectionPage*) bdb->buffer;

			if (pages->flags & SECTION_FULL)
				{
				bdb->release(REL_HISTORY);
				bdb = NULL;
				line = (line + linesPerSection) / linesPerSection * linesPerSection - 1;
				sync.lock(Exclusive);
				
				continue;
				}

			int indexSlot = indexSequence % dbb->pagesPerSection;

			if (pages->pages [indexSlot])
				{
				bdb = dbb->handoffPage(bdb, pages->pages [indexSlot], PAGE_record_locator, Exclusive);
				BDB_HISTORY(bdb);
				}
			else
				{
				Bdb *newBdb = dbb->allocPage(PAGE_record_locator, transId);
				BDB_HISTORY(newBdb);

				dbb->setPrecedence(bdb, newBdb->pageNumber);
				bdb->mark(transId);
				pages->pages [indexSlot] = newBdb->pageNumber;
				bdb->release(REL_HISTORY);

				bdb = newBdb;
				RecordLocatorPage *page = (RecordLocatorPage*) bdb->buffer;
				page->section = pages->section;
				page->sequence = line / linesPerPage;
				
				if (!dbb->serialLog->recovering)
					dbb->serialLog->logControl->recordLocator.append(dbb, transId, sectionId, page->sequence, bdb->pageNumber);
				}

			segment = indexSequence;
			}

		page = (RecordLocatorPage*) bdb->buffer;
		VALIDATE_SPACE_SLOTS(page);
		int slot = line % linesPerPage;
		RecordIndex *index = page->elements + slot;

		if (!index->page && !index->line)
			{
			// OK, reserve the slot by setting the line leaving the page number as zero
			
			bdb->mark(transId);
			index->line = 1;

			for (int n = page->maxLine; n <= slot; ++n)
				page->elements[n].spaceAvailable = 0;

			page->maxLine = MAX(page->maxLine, slot + 1);
			ASSERT(page->maxLine <= dbb->pagesPerSection);
			VALIDATE_SPACE_SLOTS(page);
			bdb->release(REL_HISTORY);

			// We have our line.  Find the next potential line, and if it isn't in this
			// section, mark this section as full.

			int sequence = indexSequence / dbb->pagesPerSection;

			if (reservedRecordNumbers || freeLines)
				{
				sync.lock(Shared);
				int next = nextLine;

				if (reservedRecordNumbers)
					{
					int n = reservedRecordNumbers->nextSet(line + 1);
					
					if (n >= 0 && n < next)
						next = n;
					}

				if (freeLines)
					{
					int n = freeLines->nextSet(line + 1);
					
					if (n >= 0 && n < next)
						next = n;
					}

				int nextSequence = next / linesPerSection;
				sync.unlock();
				
				if (nextSequence > sequence)
					markFull(true, sequence, transId);

				if (!freeLines)
					nextLine = line + 1;
				}
			else
				{
				nextLine = line + 1;

				if (nextLine % linesPerSection == 0)
					markFull(true, sequence, transId);
				}
				
			return line;
			}

		if (line % linesPerSection == linesPerSection - 1 &&
			(!reservedRecordNumbers || reservedRecordNumbers->nextSet(0) > line))
			{
			bdb->release(REL_HISTORY);
			bdb = NULL;
			markFull(true, indexSequence / dbb->pagesPerSection, transId);
			}
			
		sync.lock(Exclusive);
		}
}


void Section::reInsertStub(int32 recordNumber, TransId transId)
{
	Bdb *bdb = fetchLocatorPage (root, recordNumber, Exclusive, transId);
	BDB_HISTORY(bdb);
	if (!bdb)
		//NOT_YET_IMPLEMENTED;
		return;

	RecordLocatorPage *locatorPage = (RecordLocatorPage*) bdb->buffer;
	int slot = recordNumber % dbb->linesPerPage;
	RecordIndex *index = locatorPage->elements + slot;

	if (slot > locatorPage->maxLine || (!index->line && !index->page))
		{
		bdb->mark(transId);
		locatorPage->maxLine = MAX (locatorPage->maxLine, slot + 1);
		index->line = 1;
		ASSERT (locatorPage->maxLine <= dbb->pagesPerSection);
		}

	bdb->release(REL_HISTORY);
}

void Section::updateRecord(int32 recordNumber, Stream *stream, TransId transId, bool earlyWrite)
{
#ifdef STOP_SECTION
	if (sectionId == stopSection)
		Log::debug("UpdateRecord %d in section %d/%d\n", recordNumber, sectionId, dbb->tableSpaceId);
#endif

	// Do some fancy accounting to avoid premature use of record number.
	// If the record number has been reserved, don't bother to delete it.

	if (!stream)
		{
		Sync sync (&syncInsert, "Section::updateRecord");
		sync.lock (Exclusive);

		if (!reservedRecordNumbers)
			reservedRecordNumbers = new Bitmap;

		reservedRecordNumbers->set (recordNumber);
		}

	// Find section page and slot for record number within section

	int32 slot = recordNumber / dbb->linesPerPage;
	int sequence = slot / dbb->pagesPerSection;
	Bdb *bdb = getSectionPage (sequence, Shared, transId);
	BDB_HISTORY(bdb);
	SectionPage *sectionPage = (SectionPage*) bdb->buffer;
	int flags = sectionPage->flags;
	int32 pageNumber = sectionPage->pages [slot % dbb->pagesPerSection];
	ASSERT (pageNumber);
	bdb = dbb->handoffPage (bdb, pageNumber, PAGE_record_locator, Exclusive);
	BDB_HISTORY(bdb);
		
	// We've found the section index page.  Mark it

	bdb->mark(transId);
	RecordLocatorPage *locatorPage = (RecordLocatorPage*) bdb->buffer;
	ASSERT(locatorPage->section == sectionId || locatorPage->section == 0);
	VALIDATE_SPACE_SLOTS(locatorPage);
	int line = recordNumber % dbb->linesPerPage;
	RecordIndex *index = locatorPage->elements + line;

	// If the data page exits, and the record fits on the page, update it.

	if (index->page)
		{
		Bdb *dataBdb = dbb->fetchPage (index->page, PAGE_data, Exclusive);
		BDB_HISTORY(dataBdb);
		DataPage *dataPage = (DataPage*) dataBdb->buffer;
		//validate(locatorPage, dataBdb);
		dataBdb->mark(transId);

		if (!stream)
			{
			if (earlyWrite)
				dbb->serialLog->logControl->blobDelete.append(dbb, bdb->pageNumber, line, index->page, index->line);
			
			if (line < locatorPage->maxLine)
				{
				int spaceAvailable = deleteLine(dataBdb, index->line, bdb->pageNumber, transId, locatorPage, line);
				locatorPage->deleteLine(line, spaceAvailable);
				ASSERT(index->page == 0 && index->line == 0);
				VALIDATE_SPACE_SLOTS(locatorPage);
				}
			else
				dataBdb->release(REL_HISTORY);

			bdb->release(REL_HISTORY);

			if (flags & SECTION_FULL)
				markFull (false, sequence, transId);

			return;
			}

		int spaceAvailable = dataPage->updateRecord (this, index->line, stream, transId, earlyWrite);
		
		if (spaceAvailable)
			{
			locatorPage->setIndexSlot(line, index->page, index->line, spaceAvailable);
			VALIDATE_SPACE_SLOTS(locatorPage);
			dataBdb->release(REL_HISTORY);
			dbb->setPrecedence(bdb, index->page);
			bdb->release(REL_HISTORY);

			return;
			}

		deleteLine(dataBdb, index->line, bdb->pageNumber, transId, locatorPage, line);
		}
	else if (!stream)
		{
		locatorPage->deleteLine(line, false);		// deleting unfulfilled stub
		VALIDATE_SPACE_SLOTS(locatorPage);
		}

	if (stream)
		{
		storeRecord(locatorPage, bdb->pageNumber, index, stream, transId, earlyWrite);
		dbb->setPrecedence(bdb, index->page);
		}

	bdb->release(REL_HISTORY);
}

Bdb* Section::fetchLocatorPage(int32 root, int32 recordNumber, LockType lockType, TransId transId)
{
	int32 n = recordNumber / dbb->linesPerPage;
	Bdb *bdb = getSectionPage (n / dbb->pagesPerSection, Shared, transId);
	BDB_HISTORY(bdb);
	SectionPage *sectionPage = (SectionPage*) bdb->buffer;
	int32 pageNumber = sectionPage->pages [n % dbb->pagesPerSection];

	if (!pageNumber)
		{
		bdb->release(REL_HISTORY);

		return NULL;
		}

	bdb = dbb->handoffPage (bdb, pageNumber, PAGE_record_locator, lockType);
	RecordLocatorPage *locatorPage = (RecordLocatorPage*) bdb->buffer;
	ASSERT(locatorPage->section == sectionId || locatorPage->section == 0);
	BDB_HISTORY(bdb);
	
	return bdb;
}

int Section::deleteLine(Bdb * bdb, int line, int32 sectionPageNumber, TransId transId, RecordLocatorPage *locatorPage, int locatorLine)
{
	bdb->mark(transId);
	DataPage *page = (DataPage*) bdb->buffer;
	//validate(locatorPage, bdb);
	// int maxLine = page->maxLine;
	int spaceAvailable = page->deleteLine(dbb, line, transId);
	
	// Unless the page is empty, return the amount of space remaining
	
	if (page->maxLine > 0)
		{
		bdb->release(REL_HISTORY);
		
		return spaceAvailable;
		}

	for (int n = 0; n < locatorPage->maxLine; ++n)
		if (n != locatorLine && locatorPage->elements[n].page == bdb->pageNumber)
			{
			bdb->release(REL_HISTORY);
			Log::debug("Section::deleteLine -- locator page %d line %d points to empty data page %d:%d\n",
						sectionPageNumber, n, locatorPage->elements[n].page, locatorPage->elements[n].line);
					
			return spaceAvailable;
			}
	
	if (!dbb->serialLog->recovering)
		dbb->serialLog->logControl->sectionLine.append(dbb, sectionPageNumber, bdb->pageNumber);
				
	dbb->freePage(bdb, transId);

	return 0;
}

void Section::storeRecord(RecordLocatorPage *recordLocatorPage, int32 indexPageNumber, RecordIndex * index, Stream *stream, TransId transId, bool earlyWrite)
{
	/* If record won't fit on a page, store the tail on dedicated
	   pages, leaving only the head on the data page */

	int indexSlot = (int) (index - recordLocatorPage->elements);
	int length = stream->totalLength;
	int effectiveLength = length;
	int maxRecord = OVERFLOW_RECORD_SIZE;
	int32 overflowPageNumber = 0;
	VALIDATE_SPACE_SLOTS(recordLocatorPage);

	if (length > maxRecord)
		{
		overflowPageNumber = storeTail (stream, maxRecord, &length, transId, earlyWrite);
		effectiveLength += sizeof(int32);
		}

	for (int slot = -1; (slot = recordLocatorPage->nextSpaceSlot(slot)) >= 0;)
		{
		RecordIndex *recordLocator = recordLocatorPage->elements + slot;
		
		if (recordLocator->spaceAvailable >= effectiveLength)
			{
			Bdb *bdb = dbb->fetchPage (recordLocator->page, PAGE_data, Exclusive);
			BDB_HISTORY(bdb);
			DataPage *page = (DataPage*) bdb->buffer;

			RecordIndex temp;
			int spaceAvailable = page->storeRecord(dbb, bdb, &temp, length, stream, overflowPageNumber, transId, earlyWrite);
			VALIDATE_SPACE_SLOTS(recordLocatorPage);
			
			if (spaceAvailable > 0)
				{
				recordLocatorPage->setIndexSlot(indexSlot, temp.page, temp.line, spaceAvailable);
				VALIDATE_SPACE_SLOTS(recordLocatorPage);

				if (!dbb->serialLog->recovering)
					{
					if (earlyWrite)
						{
						dbb->serialLog->logControl->largeBlob.append(dbb, transId, indexPageNumber, indexSlot, temp.page, temp.line);
						bdb->setWriter();
						}
					else
						dbb->serialLog->setPhysicalBlock(transId);
					}

				bdb->release(REL_HISTORY);

				return;
				}

			bdb->release(REL_HISTORY);
			VALIDATE_SPACE_SLOTS(recordLocatorPage);
			}
		}
		
	/* Record doesn't fit on last used data page, make another */

	Bdb *bdb = dbb->allocPage(PAGE_data, transId);
	BDB_HISTORY(bdb);
	DataPage *page = (DataPage*) bdb->buffer;
	page->maxLine = 0;
	VALIDATE_SPACE_SLOTS(recordLocatorPage);
	RecordIndex temp;
	int spaceAvailable = page->storeRecord(dbb, bdb, &temp, length, stream, overflowPageNumber, transId, earlyWrite);
	
	if (spaceAvailable <= 0)
		{
		bdb->release(REL_HISTORY);
		ASSERT (false);
		}

	recordLocatorPage->setIndexSlot(indexSlot, temp.page, temp.line, spaceAvailable);

	if (!dbb->serialLog->recovering)
		{
		if (earlyWrite)
			{
			dbb->serialLog->logControl->largeBlob.append(dbb, transId, indexPageNumber, indexSlot, temp.page, temp.line);
			bdb->setWriter();
			}
		else
			{
			dbb->serialLog->logControl->dataPage.append(dbb, transId, sectionId, indexPageNumber, bdb->pageNumber);
			dbb->serialLog->setPhysicalBlock(transId);
			}
		}

	bdb->release(REL_HISTORY);
}


int Section::storeTail(Stream * stream, int maxRecord, int *pLength, TransId transId, bool earlyWrite)
{
	ASSERT (maxRecord > 0);
	int32 overflowPageNumber = 0;
	int length = *pLength;
	int32 offset = length;
	int32 pageSpace = dbb->pageSize - sizeof (DataOverflowPage);
	Bitmap pageNumbers;
	
	while (length + (int) sizeof (int32) > maxRecord)
		{
		Bdb *bdb = dbb->allocPage(PAGE_data_overflow, transId);
		BDB_HISTORY(bdb);
		pageNumbers.set(bdb->pageNumber);
		
		if (earlyWrite)
			bdb->setWriter();

		DataOverflowPage *overflowPage = (DataOverflowPage*) bdb->buffer;
		overflowPage->nextPage = overflowPageNumber;
		overflowPage->section = (short) sectionId;
		overflowPageNumber = bdb->pageNumber;
		overflowPage->length = MIN (pageSpace, length);
		offset -= overflowPage->length;
		stream->getSegment (offset, overflowPage->length, (char*) overflowPage + sizeof (DataOverflowPage));
		length -= overflowPage->length;
		bdb->release(REL_HISTORY);
		}

	*pLength = length;
	
	if (!dbb->serialLog->recovering)
		dbb->serialLog->logControl->overflowPages.append(dbb, &pageNumbers);
	
	return overflowPageNumber;
}


bool Section::fetchRecord(int32 recordNumber, Stream *stream, TransId transId)
{
	Bdb *bdb = fetchLocatorPage(root, recordNumber, Shared, transId);
	BDB_HISTORY(bdb);
	if (!bdb)
		return false;

	RecordLocatorPage *locatorPage = (RecordLocatorPage*) bdb->buffer;
	int slot = recordNumber % dbb->linesPerPage;

	/* If line isn't allocated, record doesn't exist */

	if (slot > locatorPage->maxLine)
		{
		bdb->release(REL_HISTORY);
		
		return false;
		}

	RecordIndex *index = locatorPage->elements + slot;

	/* If there isn't a page number, there isn't a record */

	if (index->page == 0)
		{
		bdb->release(REL_HISTORY);
		
		return false;
		}

	/* OK, go get the data page */

	slot = index->line;
	bdb = dbb->handoffPage(bdb, index->page, PAGE_data, Shared);
	BDB_HISTORY(bdb);
	DataPage *dataPage = (DataPage*) bdb->buffer;
	bool ret = dataPage->fetchRecord(dbb, slot, stream);
	bdb->release(REL_HISTORY);

	return ret;
}

int32 Section::findNextRecord(int32 startingRecord, Stream *stream)
{
	return findNextRecord (root, startingRecord, stream);
}

int32 Section::findNextRecord(int32 pageNumber, int32 startingRecord, Stream *stream)
{
	Bdb *bdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
	BDB_HISTORY(bdb);
	RecordLocatorPage *locatorPage = (RecordLocatorPage*) bdb->buffer;

	/* If this is a section index page, just look for a line in use */

	if (locatorPage->pageType == PAGE_record_locator)
		{
		ASSERT(locatorPage->section == sectionId || locatorPage->section == 0);
		
		for (int slot = startingRecord % dbb->linesPerPage; slot < locatorPage->maxLine; ++slot)
			if (locatorPage->elements [slot].page)
				{
				if (stream)
					{
					/* OK, go get the data page */
					int32 line = locatorPage->elements [slot].line;
					Bdb *dataBdb;
					
					try
						{
						dataBdb = dbb->handoffPage (bdb, locatorPage->elements [slot].page, PAGE_data, Shared);
						BDB_HISTORY(dataBdb);
						}
					catch (SQLException &exception)
						{
						Log::log ("Corrupted data page, section %d, slot %d, index page %d, corrupt page %d\n",
								sectionId, slot, bdb->pageNumber, locatorPage->elements [slot].page);
						Log::log ("-%s\n", exception.getText());
						
						continue;
						}
						
					DataPage *dataPage = (DataPage*) dataBdb->buffer;
					
					if (line >= dataPage->maxLine || dataPage->lineIndex[line].offset == 0)
						{
						Log::log("Lost record, page %d, slot %d, data page %d\n", pageNumber, slot, dataBdb->pageNumber);
						dataBdb->release(REL_HISTORY);
						bdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
						BDB_HISTORY(bdb);
						locatorPage = (RecordLocatorPage*) bdb->buffer;
						
						continue;
						}
						
					dataPage->fetchRecord (dbb, line, stream);
					dataBdb->release(REL_HISTORY);
					}
				else
					bdb->release(REL_HISTORY);
					
				return slot;
				}
				
		bdb->release(REL_HISTORY);
		
		return -1;
		}

	/* This is an intermediate page.  Find child page to recurse. */

	ASSERT (locatorPage->pageType == PAGE_sections);
	SectionPage *sectionPage = (SectionPage*) locatorPage;
	int factor = dbb->linesPerPage;

	for (int n = 0; n < sectionPage->level; ++n)
		factor *= dbb->pagesPerSection;

	int32 next = startingRecord % factor;
	int32 pgNumber;

	for (int slot = startingRecord / factor; slot < dbb->pagesPerSection; ++slot)
		if ( (pgNumber = sectionPage->pages [slot]) )
			{
			int32 recordNumber = findNextRecord (pgNumber, next, stream);
			
			if (recordNumber >= 0)
				{
				bdb->release(REL_HISTORY);
				
				return slot * factor + recordNumber;
				}
				
			next = 0;
			}

	bdb->release(REL_HISTORY);

	return -1;
}

void Section::deleteSection(Dbb * dbb, int32 sectionId, TransId transId)
{
	//Log::debug("Section::deleteSection: deleting section %d\n", sectionId);

	// Find sections page; pick up section root; zap section slot

	int rel = sectionId / dbb->pagesPerSection;
	int slot = sectionId % dbb->pagesPerSection;
	Bdb *sectionsBdb = getSectionPage(dbb, SECTION_ROOT, rel, Exclusive, transId);
	BDB_HISTORY(sectionsBdb);
	sectionsBdb->mark(transId);
	SectionPage *sections = (SectionPage*) sectionsBdb->buffer;
	int32 pageNumber = sections->pages [slot];
	sections->pages [slot] = 0;
	dbb->nextSection = MIN(sectionId, dbb->nextSection);

	if (!dbb->serialLog->recovering)
		dbb->serialLog->logControl->sectionPage.append(dbb, transId, sectionsBdb->pageNumber, 0, slot, sections->section, sections->sequence, sections->level);

	sectionsBdb->release(REL_HISTORY);

	// Section is officially gone.  Now recover the pages

	if (pageNumber)
		deleteSectionLevel(dbb, pageNumber, transId);
}

void Section::deleteSectionLevel(Dbb * dbb, int32 pageNumber, TransId transId)
{
	Bdb *sectionBdb = dbb->fetchPage (pageNumber, PAGE_sections, Exclusive);
	BDB_HISTORY(sectionBdb);
	SectionPage *page = (SectionPage*) sectionBdb->buffer;

	if (page->level > 0)
		{
		for (int n = 0; n < dbb->pagesPerSection; ++n)
			{
			if (!dbb->serialLog->recovering)
				dbb->serialLog->logControl->sectionPage.append(dbb, transId, pageNumber, 0, n, page->section, page->sequence, page->level);
				
			if (page->pages [n])
				deleteSectionLevel(dbb, page->pages [n], transId);
			}
		}
	else
		{
		for (int n = 0; n < dbb->pagesPerSection; ++n)
			if (page->pages [n])
				{
				Bdb *recordLocatorBdb = dbb->fetchPage (page->pages [n], PAGE_record_locator, Exclusive);
				BDB_HISTORY(recordLocatorBdb);
				RecordLocatorPage *locatorPage = (RecordLocatorPage*) recordLocatorBdb->buffer;

				if (locatorPage->maxLine > 0 && locatorPage->elements[0].spaceAvailable != 0)
					locatorPage->deleteDataPages(dbb, transId);
				else
					for (int line = 0; line < locatorPage->maxLine; ++line)
						{
						RecordIndex *index = locatorPage->elements + line;

						if (index->page)
							{
							Bdb *bdb = dbb->fetchPage (index->page, PAGE_data, Exclusive);
							BDB_HISTORY(bdb);
							bdb->mark(transId);
							DataPage *page = (DataPage*) bdb->buffer;

							if (page->deleteLine(dbb, index->line, transId))
								bdb->release(REL_HISTORY);
							else
								dbb->freePage(bdb, transId);
							}					
						}

				dbb->freePage (recordLocatorBdb, transId);
				}
		}

	dbb->freePage (sectionBdb, transId);
}

int32 Section::getMaxPage(int32 root, Dbb *dbb)
{
	Bdb *bdb = dbb->fetchPage (root, PAGE_any, Shared);
	BDB_HISTORY(bdb);
	SectionPage *page = (SectionPage*) bdb->buffer;
	
	for (int n = dbb->pagesPerSection - 1; n >= 0; --n)
		if (page->pages [n])
			{
			++n;
			for (int level = page->level; level; --level)
				n *= dbb->pagesPerSection;
			return n;
			}

	return 0;
}

void Section::validateSections(Dbb *dbb, Validation *validation)
{
	validation->inUse (SECTION_ROOT, "section root page");
	Bdb *bdb = dbb->fetchPage (SECTION_ROOT, PAGE_sections, Shared);
	BDB_HISTORY(bdb);
	SectionPage *sectionPage = (SectionPage*) bdb->buffer;
	sectionPage->validateSections (dbb, validation, 0);
	bdb->release(REL_HISTORY);
}

void Section::validateIndexes(Dbb *dbb, Validation *validation)
{
	validation->inUse (INDEX_ROOT, "index root page");
	Bdb *bdb = dbb->fetchPage (INDEX_ROOT, PAGE_sections, Shared);
	BDB_HISTORY(bdb);
	SectionPage *sectionPage = (SectionPage*) bdb->buffer;
	sectionPage->validateIndexes (dbb, validation, 0);
	bdb->release(REL_HISTORY);
}

void Section::validate(Dbb *dbb, Validation *validation, int sectionId, int pageNumber)
{
	validation->inUse (pageNumber, "sections page");
	Bdb *bdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
	BDB_HISTORY(bdb);
	Bitmap dataPages;

	if (bdb->buffer->pageType == PAGE_record_locator)
		{
		RecordLocatorPage *recordLocatorPage = (RecordLocatorPage*) bdb->buffer;

		if (recordLocatorPage->section != sectionId)
			validation->error ("RecordLocatorPage %d, section %d, sequence %d has wrong section (%d)",
							   pageNumber, sectionId, 0, recordLocatorPage->section);
		else if (recordLocatorPage->sequence != 0)
			validation->error ("RecordLocatorPage %d, section %d, sequence %d has wrong sequence (%d)",
							   pageNumber, sectionId, 0, recordLocatorPage->sequence);
		else if (!recordLocatorPage->validate (dbb, validation, sectionId, 0, &dataPages) &&
				 validation->isRepair())
			{
			bdb->release(REL_HISTORY);
			bdb = dbb->fetchPage (pageNumber, PAGE_record_locator, Exclusive);
			BDB_HISTORY(bdb);
			bdb->mark(0);
			recordLocatorPage = (RecordLocatorPage*) bdb->buffer;
			recordLocatorPage->repair (dbb, sectionId, 0);
			}
		}
	else
		{
		SectionPage *sectionPage = (SectionPage*) bdb->buffer;

		if (sectionPage->section != sectionId)
			validation->error ("SectionPage %d, section %d, sequence %d has wrong section (%d)",
							   pageNumber, sectionId, 0, sectionPage->section);
		/***
		else if (dbb->sequenceSectionId && (sectionId == dbb->sequenceSectionId))
			{
			if (sectionPage->level == 1)
				for (int n = 0; n < dbb->pagesPerSection; ++n)
					{
					int32 pageNumber = sectionPage->pages[n];

					if (pageNumber)
						{
						Bdb *bdb2 = dbb->fetchPage (pageNumber, PAGE_any, Shared);
						BDB_HISTORY(bdb2);
						validation->isPageType(bdb2, PAGE_sections, "Sequence Page, relative page %d", n);
						validation->inUse(bdb2, "Sequences Page");
						bdb2->release();
						}
					}
			}
		***/
		else 
			sectionPage->validate (dbb, validation, sectionId, 0, &dataPages);
		}

	bdb->release(REL_HISTORY);
}


bool Section::decomposeSequence(Dbb *dbb, int32 sequence, int level, int *slots)
{
	ASSERT(level <= MAX_LEVELS);
	int seq = sequence;

	for (int n = 0; n < level; ++n)
		{
		slots [n] = seq % dbb->pagesPerSection;
		seq /= dbb->pagesPerSection;
		}

	return seq == 0;
}

void Section::analyze(SectionAnalysis *analysis, int pageNumber)
{
	Bdb *bdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
	BDB_HISTORY(bdb);
	Bitmap dataPages;

	if (bdb->buffer->pageType == PAGE_record_locator)
		{
		RecordLocatorPage *recordLocatorPage = (RecordLocatorPage*) bdb->buffer;

		if (recordLocatorPage->section != sectionId)
			Log::logBreak ("RecordLocatorPage %d, section %d, sequence %d has wrong section (%d)",
							   pageNumber, sectionId, 0, recordLocatorPage->section);
		else if (recordLocatorPage->sequence != 0)
			Log::logBreak ("RecordLocatorPage %d, section %d, sequence %d has wrong sequence (%d)",
							   pageNumber, sectionId, 0, recordLocatorPage->sequence);
		else
			recordLocatorPage->analyze (dbb, analysis, sectionId, 0, &dataPages);
		}
	else
		{
		SectionPage *sectionPage = (SectionPage*) bdb->buffer;

		if (sectionPage->section != sectionId)
			Log::logBreak ("SectionPage %d, section %d, sequence %d has wrong section (%d)",
							   pageNumber, sectionId, 0, sectionPage->section);
			
		else if (!dbb->sequenceSectionId || (sectionId != dbb->sequenceSectionId))
			sectionPage->analyze (dbb, analysis, sectionId, 0, &dataPages);
		}

	bdb->release(REL_HISTORY);
}

void Section::expungeRecord(int32 recordNumber)
{
	Sync sync (&syncInsert, "Section::expungeRecord");
	sync.lock (Exclusive);

	if (reservedRecordNumbers && reservedRecordNumbers->isSet (recordNumber))
		{
		reservedRecordNumbers->clear (recordNumber);
		if (reservedRecordNumbers->count == 0)
			{
			reservedRecordNumbers->release();
			reservedRecordNumbers = NULL;
			}
		}

	if (!freeLines)
		freeLines = new Bitmap;

	freeLines->set (recordNumber);
}

void Section::markFull(bool isFull, int sequence, TransId transId)
{
	Bdb *bdb = getSectionPage (sequence, Exclusive, transId);
	BDB_HISTORY(bdb);
	bdb->mark (transId);
	SectionPage *pages = (SectionPage*) bdb->buffer;

	if (isFull)
		pages->flags |= SECTION_FULL;
	else
		pages->flags &= ~SECTION_FULL;
		
	bdb->release(REL_HISTORY);
}

/***
bool Section::isCleanupRequired()
{
	return reservedRecordNumbers != NULL;
}
***/

void Section::redoSectionPage(Dbb *dbb, int32 parentPage, int32 pageNumber, int slot, int sectionId, int sequence, int level)
{
	Bdb *bdb = dbb->fetchPage (parentPage, PAGE_any, Exclusive);
	BDB_HISTORY(bdb);
	SectionPage *page = (SectionPage*) bdb->buffer;

	//  Unless parent page is already leaf, probe and if necessary rebuild section page

	//if (pageNumber && (page->level > 0 || parentPage == SECTION_ROOT))
	if (pageNumber && (page->level > 0 || page->section == -1))
		{
		Bdb *sectionBdb = dbb->fakePage(pageNumber, PAGE_any, 0);
		BDB_HISTORY(bdb);
		SectionPage *sectionPage = (SectionPage*) sectionBdb->buffer;
		
		if (!dbb->trialRead(sectionBdb) ||
			sectionPage->pageType != PAGE_sections ||
			sectionPage->section != sectionId ||
			sectionPage->sequence != sequence ||
			sectionPage->level != level)
			{
			memset(sectionPage, 0, dbb->pageSize);
			//sectionPage->pageType = PAGE_sections;
			sectionBdb->setPageHeader(PAGE_sections);
			sectionPage->section = sectionId;
			sectionPage->sequence = sequence;
			sectionPage->level = level;
			}

		PageInventoryPage::markPageInUse(dbb, pageNumber, NO_TRANSACTION);
		sectionBdb->release(REL_HISTORY);
		}

	// Now try to store it in the right place

	if (page->pages[slot] != pageNumber)
		{
		bdb->mark(NO_TRANSACTION);
		page->pages[slot] = pageNumber;
		}
	
	bdb->release(REL_HISTORY);
}

int32 Section::getSectionRoot()
{
	Bdb *bdb = getSectionPage (dbb, SECTION_ROOT, sectionId / dbb->pagesPerSection, Shared, NO_TRANSACTION);
	BDB_HISTORY(bdb);
	SectionPage *sectionPage = (SectionPage*) bdb->buffer;
	root = sectionPage->pages [sectionId % dbb->pagesPerSection];
	bdb->release(REL_HISTORY);

	return root;
}

void Section::redoRecordLocatorPage(int sequence, int32 pageNumber, bool isPostFlush)
{
	Bdb *bdb = getSectionPage (sequence / dbb->pagesPerSection, Exclusive, NO_TRANSACTION);
	BDB_HISTORY(bdb);
	SectionPage *sectionPage = (SectionPage*) bdb->buffer;
	int slot = sequence % dbb->pagesPerSection;

	if (pageNumber)
		{
		bool rebuild = isPostFlush;
		
		if (!isPostFlush)
			{
			Bdb *locatorBdb = dbb->trialFetch(pageNumber, PAGE_record_locator, Shared);
			BDB_HISTORY(locatorBdb);
			
			if (locatorBdb)
				{
				RecordLocatorPage *locatorPage = (RecordLocatorPage*) locatorBdb->buffer;
				ASSERT(locatorPage->section == sectionId || locatorPage->section == 0);
				
				if (locatorPage->section != sectionId || locatorPage->sequence != sequence)
					rebuild = true;

				locatorBdb->release(REL_HISTORY);
				}
			else
				rebuild = true;
			}
				
		if (rebuild)
			{
			Bdb *locatorBdb = dbb->fakePage(pageNumber, PAGE_record_locator, 0);
			BDB_HISTORY(locatorBdb);
			RecordLocatorPage *locatorPage = (RecordLocatorPage*) locatorBdb->buffer;
			//locatorBdb->setPageHeader(PAGE_record_locator);
			locatorPage->section = sectionId;
			locatorPage->sequence = sequence;
			locatorPage->maxLine = 0;

			locatorBdb->release(REL_HISTORY);
			}
		}

	if (sectionPage->pages[slot] != pageNumber)
		{
		bdb->mark(NO_TRANSACTION);
		sectionPage->pages[slot] = pageNumber;
		}

	bdb->release(REL_HISTORY);
}


void Section::redoSectionPromotion(Dbb* dbb, int sectionId, int32 rootPageNumber, int pageLength, const UCHAR* pageData, int32 newPageNumber)
{
	Bdb *rootBdb = dbb->fetchPage(rootPageNumber, PAGE_sections, Exclusive);
	BDB_HISTORY(rootBdb);
	SectionPage *page = (SectionPage*) rootBdb->buffer;
	
	if (page->section == sectionId && page->level == ((SectionPage*) pageData)->level + 1)
		{
		rootBdb->release(REL_HISTORY);
		
		return;
		}

	Bdb *bdb = dbb->fakePage(newPageNumber, PAGE_sections, NO_TRANSACTION);
	BDB_HISTORY(bdb);
	SectionPage *newPage = (SectionPage*) bdb->buffer;
	memcpy(newPage, pageData, pageLength);
	bdb->setPageHeader(PAGE_sections);
	rootBdb->mark(NO_TRANSACTION);
	memset(page, 0, dbb->pageSize);
	rootBdb->setPageHeader(PAGE_sections);
	page->section = newPage->section;
	page->level = newPage->level + 1;
	page->pages[0] = newPageNumber;
	rootBdb->release(REL_HISTORY);
	bdb->release(REL_HISTORY);
}

void Section::redoDataPage(int32 pageNumber, int32 locatorPageNumber)
{
#ifdef STOP_SECTION
	if (sectionId == stopSection)
		Log::debug("Redo data page %d, locator page %d in section %d/%d\n", pageNumber, locatorPageNumber, sectionId, dbb->tableSpaceId);
#endif

	Bdb *bdb = dbb->fakePage(pageNumber, PAGE_data, NO_TRANSACTION);
	BDB_HISTORY(bdb);
	DataPage *dataPage = (DataPage*) bdb->buffer;
	//memset(dataPage, 0, dbb->pageSize);			// page may have been dirtied by partial read
	//dataPage->pageType = PAGE_data;
	memset(&dataPage->maxLine, 0, dbb->pageSize - OFFSET(DataPage*, maxLine));
	
	Bdb *locatorBdb = dbb->fetchPage(locatorPageNumber, PAGE_record_locator, Shared);
	BDB_HISTORY(locatorBdb);
	RecordLocatorPage *locatorPage = (RecordLocatorPage*) locatorBdb->buffer;
	VALIDATE_SPACE_SLOTS(locatorPage);
	
	for (int n = 0; n < locatorPage->maxLine; ++n)
		if (locatorPage->elements[n].page == pageNumber)
			{
			int line = locatorPage->elements[n].line;
			
			if (line >= dataPage->maxLine)
				dataPage->maxLine = line + 1;
			
			dataPage->lineIndex[line].offset = dbb->pageSize - 1;
			dataPage->lineIndex[line].length = 0;
			}
	
	VALIDATE_SPACE_SLOTS(locatorPage);
	locatorBdb->release(REL_HISTORY);
	bdb->release(REL_HISTORY);
}

Bdb* Section::getSectionPage(int sequence, LockType lockType, TransId transId)
{
	Sync sync(&syncObject, "Section::getSectionPage");
	sync.lock(Shared);
	int pageNumber = sectionPages.get(sequence);
	sync.unlock();
	
	if (pageNumber)
		{
		Bdb *bdb = dbb->fetchPage(pageNumber, PAGE_any, lockType);
		BDB_HISTORY(bdb);
		SectionPage *page = (SectionPage*) bdb->buffer;
		
		// There is a chance that the first page has been promoted upwards.  If so, fix it.
		
		if (page->level == 0)
			return bdb;
		
		bdb->release(REL_HISTORY);
		}
	
	sync.lock(Exclusive);
	Bdb *bdb = getSectionPage(dbb, root, sequence, lockType, transId);
	BDB_HISTORY(bdb);
	sectionPages.set(sequence, bdb->pageNumber);
	
	return bdb;
}

void Section::redoSectionLine(Dbb* dbb, int32 pageNumber, int32 dataPageNumber)
{
	Bdb *bdb = dbb->fetchPage(pageNumber, PAGE_record_locator, Exclusive);
	BDB_HISTORY(bdb);
	bdb->mark(NO_TRANSACTION);
	RecordLocatorPage *page = (RecordLocatorPage*) bdb->buffer;
	VALIDATE_SPACE_SLOTS(page);
	page->expungeDataPage(dataPageNumber);
	bdb->release(REL_HISTORY);
}

bool Section::dataPageInUse(Dbb* dbb, int32 recordLocatorPage, int32 dataPage)
{
	Bdb *bdb = dbb->fetchPage(recordLocatorPage, PAGE_record_locator, Shared);
	BDB_HISTORY(bdb);
	RecordLocatorPage *recordLocator = (RecordLocatorPage*) bdb->buffer;
	
	for (int n = 0; n < recordLocator->maxLine; ++n)
		if (recordLocator->elements[n].page == dataPage)
			{
			bdb->release(REL_HISTORY);
			
			return true;
			}
	
	bdb->release(REL_HISTORY);
	
	return false;
}

void Section::redoBlobUpdate(Dbb* dbb, int32 locatorPage, int locatorLine, int32 dataPage, int dataLine)
{
	Bdb *bdb = dbb->fetchPage(locatorPage, PAGE_record_locator, Exclusive);
	BDB_HISTORY(bdb);
	bdb->mark(NO_TRANSACTION);
	RecordLocatorPage *page = (RecordLocatorPage*) bdb->buffer;
	
	if (page->maxLine <= locatorLine)
		page->maxLine = locatorLine + 1;

	page->setIndexSlot(locatorLine, dataPage, dataLine, dbb->pageSize);
	bdb->release(REL_HISTORY);
}

void Section::validate(RecordLocatorPage* locatorPage, Bdb *dataPageBdb)
{
	DataPage* dataPage = (DataPage*) dataPageBdb->buffer;
	
	for (int n = 0; n < locatorPage->maxLine; ++n)
		if (locatorPage->elements[n].page == dataPageBdb->pageNumber)
			{
			int l = locatorPage->elements[n].line;
			ASSERT(l < dataPage->maxLine && dataPage->lineIndex[l].length != 0);
			}
}

void Section::redoBlobDelete(Dbb* dbb, int32 locatorPage, int locatorLine, int32 dataPageNumber, int dataLine, bool dataPageActive)
{
	int spaceAvailable = 0;
	
	if (dataPageActive)
		{
		Bdb *bdb = dbb->fetchPage(dataPageNumber, PAGE_data, Exclusive);
		BDB_HISTORY(bdb);
		bdb->mark(NO_TRANSACTION);
		DataPage *dataPage = (DataPage*) bdb->buffer;
		spaceAvailable = dataPage->deleteLine(dbb, dataLine, NO_TRANSACTION);
		bdb->release(REL_HISTORY);
		}
		
	Bdb *bdb = dbb->fetchPage(locatorPage, PAGE_record_locator, Exclusive);
	BDB_HISTORY(bdb);
	bdb->mark(NO_TRANSACTION);
	RecordLocatorPage *page = (RecordLocatorPage*) bdb->buffer;
	page->setIndexSlot(locatorLine, 0, 0, dbb->pageSize);
	bdb->release(REL_HISTORY);
}
