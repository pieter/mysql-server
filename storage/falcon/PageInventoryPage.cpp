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

// PageInventoryPage.cpp: implementation of the PageInventoryPage class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "PageInventoryPage.h"
#include "Dbb.h"
#include "BDB.h"
#include "Validation.h"
#include "Bitmap.h"
#include "RecordLocatorPage.h"
#include "IndexPage.h"
#include "DataPage.h"
#include "DataOverflowPage.h"
#include "SectionPage.h"
#include "Transaction.h"
#include "Log.h"
#include "SQLError.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//#define STOP_PAGE	98

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

/***
PageInventoryPage::PageInventoryPage()
{

}

PageInventoryPage::~PageInventoryPage()
{

}
***/

void PageInventoryPage::create(Dbb * dbb, TransId transId)
{
	Bdb *bdb = createInventoryPage (dbb, PIP_PAGE, transId);
	PageInventoryPage *space = (PageInventoryPage*) bdb->buffer;
	space->freePages [0] &= ~(MASK (PIP_PAGE) | 
							  MASK (SECTION_ROOT) |
							  MASK (HEADER_PAGE) | 
							  MASK (INDEX_ROOT));
	bdb->release(REL_HISTORY);
}

Bdb* PageInventoryPage::createInventoryPage(Dbb * dbb, int32 pageNumber, TransId transId)
{
	Bdb *bdb = dbb->fakePage(pageNumber, PAGE_inventory, transId);
	BDB_HISTORY(bdb);
	PageInventoryPage *page = (PageInventoryPage*) bdb->buffer;

	for (int n = 0; n < dbb->pipSlots; ++n)
		page->freePages [n] = -1;

	return bdb;
}

Bdb* PageInventoryPage::allocPage(Dbb * dbb, PageType pageType, TransId transId)
{
	for (int32 pip = dbb->lastPageAllocated / dbb->pagesPerPip;; ++pip)
		{
		int32 pipPageNumber = (pip == 0) ? PIP_PAGE : pip * dbb->pagesPerPip - 1;
		Bdb *bdb = dbb->fetchPage (pipPageNumber, PAGE_inventory, Exclusive);
		BDB_HISTORY(bdb);
		PageInventoryPage *page = (PageInventoryPage*) bdb->buffer;
		int32 mask;
		
		for (int slot = 0; slot < dbb->pipSlots; ++slot)
			if ( (mask = page->freePages[slot]) )
				for (int n = 0; n < PIP_BITS; ++n)
					if (mask & MASK (n))
						{
						bdb->mark(transId);
						page->freePages [slot] &= ~MASK (n);
						int32 pageNumber = n + slot * PIP_BITS + pip * dbb->pagesPerPip;
						
						if (n + slot * PIP_BITS == dbb->pagesPerPip - 1)
							createInventoryPage(dbb, pageNumber, transId)->release(REL_HISTORY);
						else
							{
							bdb->release(REL_HISTORY);
							dbb->lastPageAllocated = pageNumber;
							Bdb *newBdb = dbb->fakePage(pageNumber, pageType, transId);
							BDB_HISTORY(newBdb );
							//newBdb->setPrecedence(pipPageNumber);

#ifdef STOP_PAGE
							if (pageNumber == STOP_PAGE)
								Log::debug("page %d/%d allocated\n", pageNumber, dbb->tableSpaceId);
#endif

#ifdef DEBUG_INDEX_PAGE
							newBdb->buffer->pageNumber = pageNumber;
#endif
							return newBdb;
							}
						}
						
		bdb->release(REL_HISTORY);
		}
}

void PageInventoryPage::freePage(Dbb *dbb, int32 pageNumber, TransId transId)
{
#ifdef STOP_PAGE
	if (pageNumber == STOP_PAGE)
		Log::debug("page %d/%d released\n", pageNumber, dbb->tableSpaceId);
#endif

	dbb->freePage (pageNumber);
	int32 pip = pageNumber / dbb->pagesPerPip;
	int32 n = pageNumber % dbb->pagesPerPip;

	if (pip == 0)
		pip = PIP_PAGE;
	else
		pip = pip * dbb->pagesPerPip - 1;

	Bdb *bdb = dbb->fetchPage(pip, PAGE_inventory, Exclusive);
	BDB_HISTORY(bdb);
	PageInventoryPage *page = (PageInventoryPage*) bdb->buffer;
	bdb->mark(transId);
	page->freePages[n / PIP_BITS] |= MASK (n % PIP_BITS);
	bdb->release(REL_HISTORY);
	dbb->lastPageAllocated = MIN(pageNumber - 1, dbb->lastPageAllocated);
}

void PageInventoryPage::markPageInUse(Dbb *dbb, int32 pageNumber, TransId transId)
{
	int32 pip = pageNumber / dbb->pagesPerPip;
	int32 n = pageNumber % dbb->pagesPerPip;

	if (pip == 0)
		pip = PIP_PAGE;
	else
		pip = pip * dbb->pagesPerPip - 1;

	Bdb *bdb = dbb->fetchPage (pip, PAGE_inventory, Exclusive);
	BDB_HISTORY(bdb);
	PageInventoryPage *page = (PageInventoryPage*) bdb->buffer;
	bdb->mark(transId);
	page->freePages [n / PIP_BITS] &= ~MASK (n % PIP_BITS);
	bdb->release(REL_HISTORY);
}

bool PageInventoryPage::isPageInUse(Dbb *dbb, int32 pageNumber)
{
	int32 pip = pageNumber / dbb->pagesPerPip;
	int32 n = pageNumber % dbb->pagesPerPip;

	if (pip == 0)
		pip = PIP_PAGE;
	else
		pip = pip * dbb->pagesPerPip - 1;

	Bdb *bdb = dbb->fetchPage (pip, PAGE_inventory, Shared);
	BDB_HISTORY(bdb);
	PageInventoryPage *page = (PageInventoryPage*) bdb->buffer;
	bool used = !(page->freePages [n / PIP_BITS] & MASK (n % PIP_BITS));
	bdb->release(REL_HISTORY);

	return used;
}

void PageInventoryPage::validate(Dbb *dbb, Validation *validation)
{
	int lastPage = dbb->pagesPerPip - 1;
	int lastSlot = lastPage / PIP_BITS;
	int lastBit = MASK (lastPage % PIP_BITS);

	for (int32 pageNumber = PIP_PAGE, sequence = 0; pageNumber; ++sequence)
		{
		Bdb *bdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
		BDB_HISTORY(bdb);
		
		if (validation->isPageType (bdb, PAGE_inventory, "PageInventoryPage"))
			{
			validation->inUse (bdb, "PageInventoryPage");
			PageInventoryPage *page = (PageInventoryPage*) bdb->buffer;
			
			if (page->freePages [lastSlot] & lastBit)
				pageNumber = 0;
			else
				pageNumber = (sequence + 1) * dbb->pagesPerPip - 1;
			}
		else
			pageNumber = 0;
			
		bdb->release(REL_HISTORY);
		}
}

void PageInventoryPage::validateInventory(Dbb *dbb, Validation *validation)
{
	int lastPage = dbb->pagesPerPip - 1;
	int lastSlot = lastPage / PIP_BITS;
	int lastBit = MASK (lastPage % PIP_BITS);
	Bitmap *usedPages = &validation->pages;
	int tableSpaceId = dbb->tableSpaceId;

	for (int32 pageNumber = PIP_PAGE, sequence = 0; pageNumber; ++sequence)
		{
		Bdb *pipBdb = dbb->fetchPage (pageNumber, PAGE_any, Exclusive);
		BDB_HISTORY(pipBdb);
		
		if (validation->isPageType (pipBdb, PAGE_inventory, "PageInventoryPage"))
			{
			PageInventoryPage *page = (PageInventoryPage*) pipBdb->buffer;
			int32 base = dbb->pagesPerPip * sequence;
			
			for (int n = 0; n < dbb->pagesPerPip; ++n)
				{
				bool used = usedPages->isSet (base + n);
				
				if (page->freePages [n / PIP_BITS] & MASK (n % PIP_BITS))
					{
					if (used)
						{
						validation->error ("unallocated page %d in use", base + n);
						
						if (validation->isRepair())
							{
							pipBdb->mark(NO_TRANSACTION);
							page->freePages [n / PIP_BITS] &= ~MASK (n % PIP_BITS);
							}
						}
					}
				else
					if (!used)
						{
						int32 pageNumber = base + n;
						//Bdb *bdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
						Bdb *bdb = dbb->trialFetch(pageNumber, PAGE_any, Shared);
						BDB_HISTORY(bdb);
						
						if (bdb)
							{
							Page *page = bdb->buffer;
							
							switch (page->pageType)
								{
								case PAGE_record_locator:
									validation->warning("orphan section index page %d/%d, section %d, seq %d", 
													pageNumber, tableSpaceId,
													((RecordLocatorPage*) page)->section,
													((RecordLocatorPage*) page)->sequence);
									break;

								case PAGE_btree:
									{
									IndexPage *ipg = (IndexPage*) page;
									validation->warning("orphan index page %d/%d, level %d, parent %d, prior %d, next %d", 
													pageNumber,  tableSpaceId,
													ipg->level, 
													ipg->parentPage, 
													ipg->priorPage, 
													ipg->nextPage);
									}
									break;

								case PAGE_data:
									{
									DataPage *pg = (DataPage*) page;
									validation->warning("orphan data page %d/%d, maxLine %d", pageNumber,  tableSpaceId, pg->maxLine);
									}
									break;

								case PAGE_data_overflow:
									{
									DataOverflowPage *pg = (DataOverflowPage*) page;
									validation->warning("orphan data overflow page %d/%d, section=%d, next=%d", pageNumber,  tableSpaceId, pg->section, pg->nextPage);
									}
									break;

								case PAGE_sections:
									{
									SectionPage *pg = (SectionPage*) page;
									validation->warning("orphan section page %d/%d, section=%d, seq=%d, level=%d, flgs=%d", 
														pageNumber, tableSpaceId, pg->section, pg->level, pg->flags);
									}
									break;

								case PAGE_free:
									validation->warning("orphan free page %d", pageNumber);
									break;

								default:
									validation->warning("orphan page %d/%d, type %d", 
													pageNumber, tableSpaceId, page->pageType);
								}
							}
						else
							validation->warning("possible unwritten orphan page %d/%d", pageNumber, tableSpaceId);
							
						if (validation->isRepair())
							{
							pipBdb->release(REL_HISTORY);
							
							if (bdb)
								dbb->freePage (bdb, 0);
							else
								freePage(dbb, pageNumber, NO_TRANSACTION);
								
							pipBdb = dbb->fetchPage (pageNumber, PAGE_any, Shared);
							BDB_HISTORY(pipBdb);
							page = (PageInventoryPage*) pipBdb->buffer;
							}
						else if (bdb)
							bdb->release(REL_HISTORY);
						}
				}
				
			if (page->freePages [lastSlot] & lastBit)
				pageNumber = 0;
			else
				pageNumber = (sequence + 1) * dbb->pagesPerPip - 1;
			}
		else
			pageNumber = 0;

		pipBdb->release(REL_HISTORY);
		}
}

int32 PageInventoryPage::getLastPage(Dbb *dbb)
{
	for (int32 pip = dbb->lastPageAllocated / dbb->pagesPerPip;; ++pip)
		{
		Bdb *bdb = dbb->trialFetch(
						(pip == 0) ? PIP_PAGE : pip * dbb->pagesPerPip - 1,
						 PAGE_inventory, Shared);
		BDB_HISTORY(bdb);
		
		if (!bdb)
			{
			Log::log("Recovering from lost page inventory page %d\n", pip);			
			--pip;
			
			if (pip == 0)
				throw SQLError(DATABASE_CORRUPTION, "Transaction inventory page not found");

			bdb = dbb->trialFetch(pip * dbb->pagesPerPip - 1, PAGE_inventory, Exclusive);
			BDB_HISTORY(bdb);
			bdb->mark(NO_TRANSACTION);
			PageInventoryPage *page = (PageInventoryPage*) bdb->buffer;
			page->freePages[dbb->pipSlots - 1] |= MASK(PIP_BITS - 1);
			}

		PageInventoryPage *page = (PageInventoryPage*) bdb->buffer;
		int32 mask;
		
		for (int slot = dbb->pipSlots - 1; slot >= 0; --slot)
			if ((mask = page->freePages[slot]) != -1)
				for (int n = PIP_BITS - 1; n >= 0; --n)
					if (!(mask & MASK (n)))
						{
						int32 pageNumber = n + slot * PIP_BITS + pip * dbb->pagesPerPip;
						
						if (n + slot * PIP_BITS == dbb->pagesPerPip - 1)
							goto nextPip;
							
						bdb->release(REL_HISTORY);
						
						return pageNumber + 1;
						}
						
		nextPip:
		bdb->release(REL_HISTORY);
		}
}

void PageInventoryPage::reallocPage(Dbb *dbb, int32 pageNumber)
{
#ifdef STOP_PAGE
	if (pageNumber == STOP_PAGE)
		Log::debug("page %d/%d reallocated\n", pageNumber, dbb->tableSpaceId);
#endif

	int32 pip = pageNumber / dbb->pagesPerPip;
	int32 n = pageNumber % dbb->pagesPerPip;

	if (pip == 0)
		pip = PIP_PAGE;
	else
		pip = pip * dbb->pagesPerPip - 1;

	//Bdb *bdb = dbb->fetchPage (pip, PAGE_inventory, Exclusive);
	Bdb *bdb = dbb->trialFetch(pip, PAGE_inventory, Exclusive);
	BDB_HISTORY(bdb);

	if (!bdb)
		bdb = createInventoryPage(dbb, pip, NO_TRANSACTION);

	PageInventoryPage *page = (PageInventoryPage*) bdb->buffer;
	int slot = n / PIP_BITS;

	if (page->freePages [slot] & MASK(n % PIP_BITS))
		{
		bdb->mark(NO_TRANSACTION);
		page->freePages [slot] &= ~MASK(n % PIP_BITS);
		}

	bdb->release(REL_HISTORY);
}

void PageInventoryPage::analyzePages(Dbb* dbb, PagesAnalysis* pagesAnalysis)
{
	for (int32 pip = 0;; ++pip)
		{
		Bdb *bdb = dbb->fetchPage (
						(pip == 0) ? PIP_PAGE : pip * dbb->pagesPerPip - 1,
						 PAGE_inventory, Shared);
		BDB_HISTORY(bdb);
		PageInventoryPage *page = (PageInventoryPage*) bdb->buffer;
		pagesAnalysis->totalPages += dbb->pagesPerPip;
		int clump = 0;
		int maxPage = 0;
			
		for (int slot = 0; slot < dbb->pipSlots; ++slot)
			{
			clump = page->freePages[slot];
			int count = 0;
			
			if (clump == 0)
				{
				count = PIP_BITS;
				maxPage = pip * dbb->pagesPerPip + (slot + 1) * PIP_BITS - 1;
				}
			else if (clump != -1)
				for (int n = 0; n < PIP_BITS; ++n)
					if (!(clump & MASK(n)))
						{
						++count;
						maxPage = pip * dbb->pagesPerPip + (slot) * PIP_BITS + n;
						}
			
			pagesAnalysis->allocatedPages += count;
			
			if (maxPage > pagesAnalysis->maxPage)
				pagesAnalysis->maxPage = maxPage;
			}
		
		bdb->release(REL_HISTORY);
		
		if (clump & MASK(PIP_BITS - 1))
			break;
		}
}
