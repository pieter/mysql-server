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

// SectionRootPage.cpp: implementation of the SectionRootPage class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SectionRootPage.h"
#include "Dbb.h"
#include "BDB.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

/***
SectionRootPage::SectionRootPage()
{

}

SectionRootPage::~SectionRootPage()
{

}
***/

void SectionRootPage::create(Dbb * dbb, TransId transId)
{
	Bdb *bdb = dbb->fakePage (SECTION_ROOT, PAGE_sections, transId);
	BDB_HISTORY(bdb);
	SectionRootPage *sections = (SectionRootPage*) bdb->buffer;
	sections->section = -1;
	sections->level = 0;
	sections->sequence = 0;
	bdb->release(REL_HISTORY);
}
