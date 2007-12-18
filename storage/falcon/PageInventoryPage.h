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

// PageInventoryPage.h: interface for the PageInventoryPage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PAGEINVENTORYPAGE_H__6A019C22_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
#define AFX_PAGEINVENTORYPAGE_H__6A019C22_A340_11D2_AB5A_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Page.h"

static const int PIP_BITS	= 16;

class Dbb;
class Bdb;
struct PagesAnalysis;

class PageInventoryPage : public Page  
{
public:
	static void		reallocPage(Dbb *dbb, int32 pageNumber);
	static int32	getLastPage (Dbb *dbb);
	static void		validateInventory (Dbb *dbb, Validation *validation);
	static void		validate (Dbb *dbb, Validation *validation);
	static bool		isPageInUse (Dbb *dbb, int32 pageNumber);
	static void		markPageInUse (Dbb *dbb, int32 pageNumber, TransId transId);
	static void		freePage (Dbb *dbb, int32 pageNumber, TransId transId);
	static Bdb*		allocPage (Dbb *dbb, PageType pageType, TransId transId);
	static Bdb*		createInventoryPage (Dbb *dbb, int32 pageNumber, TransId transId);
	static void		create (Dbb* dbb, TransId transId);
	//PageInventoryPage();
	//~PageInventoryPage();

	short		freePages [1];
	static void analyzePages(Dbb* dbb, PagesAnalysis* pagesAnalysis);
};

#endif // !defined(AFX_PAGEINVENTORYPAGE_H__6A019C22_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
