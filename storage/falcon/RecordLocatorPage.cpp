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

// RecordLocatorPage.cpp: implementation of the RecordLocatorPage class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Engine.h"
#include "RecordLocatorPage.h"
#include "Validation.h"
#include "Dbb.h"
#include "BDB.h"
#include "DataPage.h"
#include "Bitmap.h"
#include "Log.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#define VALIDIF(expr)		if (!(expr)) corrupt();
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

/***
RecordLocatorPage::RecordLocatorPage()
{

}

RecordLocatorPage::~RecordLocatorPage()
{

}
***/

void RecordLocatorPage::deleteLine(int line, int spaceAvailable)
{
	RecordIndex *index = elements + line;
	int32 pageNumber = index->page;
	setIndexSlot(line, 0, 0, spaceAvailable);
	
	while (maxLine > 0 && elements [maxLine - 1].line == 0 && 
						  elements [maxLine - 1].page == 0)
		{
		VALIDIF(!isSpaceSlot(maxLine - 1));
		--maxLine;
		}

	if (spaceAvailable == 0 && pageNumber)
		for (int n = 0; n < maxLine; ++n)
			//ASSERT (elements [n].page != pageNumber);
			if (elements [n].page == pageNumber)
				{
				elements [n].page = 0;
				elements [n].line = 0;
				}
}

bool RecordLocatorPage::validate(Dbb *dbb, Validation *validation, int sectionId, int sequence, Bitmap *dataPages)
{
	bool ok = true;

	for (int n = 0; n < maxLine; ++n)
		{
		RecordIndex *index = elements + n;
		
		if (!index->page && index->line)
			{
			if (validation->minutia())
				validation->error("Abandoned list slot, section %d, sequence %d, line %d",
								   sectionId, sequence, n);
			ok = false;
			}
			
		if (index->page)
			{
			Bdb *bdb = dbb->fetchPage(index->page, PAGE_any, Shared);
			BDB_HISTORY(bdb);
			
			if (validation->isPageType(bdb, PAGE_data, "DataPage, section %d, sequence %d", sectionId, sequence))
				{
				DataPage *dataPage = (DataPage*) bdb->buffer;

				if (!dataPages->isSet(index->page))
					{
					validation->inUse(index->page, "data page");
					dataPage->validate(dbb, validation);
					}
				
				if (index->line >= dataPage->maxLine)
					validation->error("Missing data record, section %d, sequence %d, line %d, data page %d",
									  sectionId, sequence, n, bdb->pageNumber);
				}
			else
				{
				ok = false;
				
				if (!dataPages->isSet(index->page))
					validation->inUse(index->page, "data page (bad)");
				}
				
			dataPages->set(index->page);
			bdb->release(REL_HISTORY);
			}
		}

	return ok;
}

void RecordLocatorPage::repair(Dbb *dbb, int sectionId, int sequence)
{
	for (int n = 0; n < maxLine; ++n)
		{
		RecordIndex *index = elements + n;
		
		if (!index->page && index->line)
			index->line = 0;
			
		if (index->page)
			{
			Bdb *bdb = dbb->fetchPage (index->page, PAGE_any, Exclusive);
			BDB_HISTORY(bdb);
			DataPage *dataPage = (DataPage*) bdb->buffer;
			
			if (dataPage->pageType == PAGE_data)
				{
				//dataPage->repair (dbb, validation);
				}
			else
				{
				Log::log ("RecordLocatorPage::repair: dropping DataPage %d from section %d, sequence %d\n", 
						  bdb->pageNumber, sectionId, sequence);
				index->page = 0;
				}
				
			bdb->release(REL_HISTORY);
			}
		}
}

void RecordLocatorPage::analyze(Dbb *dbb, SectionAnalysis *analysis, int sectionId, int Sequence, Bitmap *dataPages)
{
	for (int n = 0; n < maxLine; ++n)
		{
		RecordIndex *index = elements + n;
		
		if (index->page && !dataPages->isSet (index->page))
			{
			dataPages->set (index->page);
			Bdb *bdb = dbb->fetchPage (index->page, PAGE_any, Shared);
			BDB_HISTORY(bdb);
			DataPage *dataPage = (DataPage*) bdb->buffer;
			dataPage->analyze (dbb, analysis);
			bdb->release(REL_HISTORY);
			}
		}
}

int RecordLocatorPage::nextSpaceSlot(int priorSlot)
{
	for (int slot = priorSlot + 1; slot < maxLine; ++slot)
		{
		if (elements[slot].spaceAvailable > 0)
			return slot;
		
		if (elements[slot].spaceAvailable < 0)
			return -elements[slot].spaceAvailable;
		}
	
	return -1;
}

int RecordLocatorPage::findSpaceSlot(int32 pageNumber)
{
	int slot;
	
	for (slot = -1; (slot = nextSpaceSlot(slot)) >= 0; )
		if (elements[slot].page == pageNumber)
			return slot;
	
	for (slot = 0; slot < maxLine; ++slot)
		if (elements[slot].page == pageNumber)
			if (elements[slot].spaceAvailable > 0)
				return slot;
				
	return -1;
}

void RecordLocatorPage::linkSpaceSlot(int from, int to)
{
	VALIDIF(to == 0 || elements[to].spaceAvailable > 0);
	
	if (from < 0)
		from = 0;
	
	int end = (to) ? to : maxLine;
	
	for (int slot = from; slot < end; ++slot)
		if (elements[slot].spaceAvailable <= 0)
			{
			elements[slot].spaceAvailable = -to;
			break;
			}
}

void RecordLocatorPage::validateSpaceSlots(void)
{
	int nextSpaceSlot = 0;
	RecordIndex *element = elements;
	int empty = 0;
	int32 pageNumber = 0;
	
	for (int n = 0; n < maxLine; ++n, ++element)
		{
		if (element->spaceAvailable > 0)
			{
			VALIDIF(nextSpaceSlot == n || (nextSpaceSlot == 0 && empty == 0));
			VALIDIF(element->page != pageNumber);
			empty = 0;
			nextSpaceSlot = 0;
			pageNumber = element->page;
			}
		else if (element->spaceAvailable < 0)
			{
			VALIDIF(-element->spaceAvailable < maxLine);
			VALIDIF(empty == 0);
			VALIDIF(nextSpaceSlot == 0);
			nextSpaceSlot = -element->spaceAvailable;
			VALIDIF(nextSpaceSlot > n);
			VALIDIF(elements[nextSpaceSlot].spaceAvailable > 0);
			pageNumber = element->page;
			}
		else if (empty == 0)
			empty = n;
		}
}

void RecordLocatorPage::corrupt(void)
{
	Log::debug("Corrupt RecordLocatorPage space management structure\n");
	printPage();
	ASSERT(false);
}

void RecordLocatorPage::printPage(void)
{
	RecordIndex *element = elements;
	
	for (int n = 0; n < maxLine; ++n, ++element)
		Log::debug("%d.  page %d, line %d, space %d\n", n, element->page, element->line, element->spaceAvailable);
	
}

void RecordLocatorPage::setIndexSlot(int slot, int32 pageNumber, int line, int availableSpace)
{
	ASSERT(availableSpace >= 0);
	
	RecordIndex *element = elements + slot;
	//validateSpaceSlots();
	
	// Start by special casing the delete operation
	
	if (pageNumber == 0)
		{
		if (element->page && element->spaceAvailable > 0)
			{
			unlinkSpaceSlot(slot);
			
			// See if there is another slot for that page that should be promoted
			
			for (int n = slot + 1; n < maxLine; ++n)
				if (elements[n].page == element->page)
					{
					insertSpaceSlot(n, availableSpace);
					break;
					}
			}
		else if (element->page && availableSpace)
			{
			int spaceSlot = findSpaceSlot(element->page);
			
			if (spaceSlot >= 0)
				elements[spaceSlot].spaceAvailable = availableSpace;
			}
		
		element->page = pageNumber;
		element->line = line;
		//validateSpaceSlots();
		
		return;
		}
					
	// Page hasn't changed and is space management page
	
	if (element->page == pageNumber && element->spaceAvailable > 0)
		{
		element->line = line;
		element->spaceAvailable = availableSpace;
		//validateSpaceSlots();
		
		return;
		}
	
	int spaceSlot = findSpaceSlot(pageNumber);
	VALIDIF(spaceSlot != slot);
	
	// If we're now the space slot, perform the switcheroo
	
	if (slot < spaceSlot)
		{
		unlinkSpaceSlot(spaceSlot);
		element->page = pageNumber;
		element->line = line;
		insertSpaceSlot(slot, availableSpace);
		//validateSpaceSlots();
		
		return;
		}
		
	// This page has a space management slot elsewhere.  Update it.
	
	if (spaceSlot >= 0)
		{
		if (spaceSlot < slot)
			{
			elements[spaceSlot].spaceAvailable = availableSpace;
			element->page = pageNumber;
			element->line = line;
			//validateSpaceSlots();
			
			return;
			}
		
		unlinkSpaceSlot(spaceSlot);
		element->page = pageNumber;
		element->line = line;
		insertSpaceSlot(slot, availableSpace);	
		//validateSpaceSlots();
		
		return;
		}
	
	// Slot is a new space management slot.  Link it in
	
	element->page = pageNumber;
	element->line = line;
	insertSpaceSlot(slot, availableSpace);
	//validateSpaceSlots();
}

void RecordLocatorPage::insertSpaceSlot(int slot, int availableSpace)
{
	int priorSlot;
	int nextSlot;
	
	for (priorSlot = -1; (nextSlot = nextSpaceSlot(priorSlot)) >= 0 && nextSlot < slot; priorSlot = nextSlot)
		;

	elements[slot].spaceAvailable = availableSpace;
	
	if (slot > 0)
		linkSpaceSlot(priorSlot, slot);
		
	linkSpaceSlot(slot, (nextSlot>= 0) ? nextSlot : 0);
}

void RecordLocatorPage::unlinkSpaceSlot(int slot)
{
	int priorSlot;
	int nextSlot;
	
	if (maxLine == 0)
		return;

	for (priorSlot = -1; (nextSlot = nextSpaceSlot(priorSlot)) >= 0 && nextSlot < slot; priorSlot = nextSlot)
		;

	if (slot < nextSlot)
		{
		Log::log("RecordLocatorPage: transient corruption corrected\n");
		elements[slot].spaceAvailable = 0;
		
		return;
		}

	VALIDIF(nextSlot == slot);
	nextSlot = nextSpaceSlot(slot);
	elements[slot].spaceAvailable = 0;
	
	// If we're not a space management slot, the next guy isn't an index, either
	
	if (slot <= maxLine && elements[slot + 1].spaceAvailable < 0)
		elements[slot + 1].spaceAvailable = 0;
		
	linkSpaceSlot(priorSlot, (nextSlot >= 0) ? nextSlot : 0);
}

void RecordLocatorPage::expungeDataPage(int32 pageNumber)
{
	RecordIndex *element = elements;
	
	for (int slot = 0; slot < maxLine; ++slot, ++element)
		if (element->page == pageNumber)
			{
			if (element->spaceAvailable > 0)
				unlinkSpaceSlot(slot);
			
			element->page = 0;
			}
}

void RecordLocatorPage::deleteDataPages(Dbb* dbb, TransId transId)
{
	for (int slot; (slot = nextSpaceSlot(-1)) >= 0;)
		{
		int pageNumber = elements[slot].page;
		expungeDataPage(pageNumber);
		Bdb *bdb = dbb->fetchPage(pageNumber, PAGE_data, Exclusive);
		BDB_HISTORY(bdb);
		bdb->mark(transId);
		DataPage *dataPage = (DataPage*) bdb->buffer;
		dataPage->deletePage(dbb, transId);
		dbb->freePage(bdb, transId);
		}
}
