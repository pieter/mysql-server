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

// Page.h: interface for the Page class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PAGE_H__6A019C1D_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
#define AFX_PAGE_H__6A019C1D_A340_11D2_AB5A_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "PageType.h"

#ifdef STORAGE_ENGINE
#define HAVE_PAGE_NUMBER
#endif

// Hardwired page numbers

#define HEADER_PAGE		0
#define PIP_PAGE		1
#define SECTION_ROOT	2
#define INDEX_ROOT		3

class Validation;
class Dbb;


class Page  
{
public:
	void setType(short pageType, int32 pageNumber);

	short	pageType;
	short	checksum;

#ifdef HAVE_PAGE_NUMBER
	int32	pageNumber;
#endif

};

#endif // !defined(AFX_PAGE_H__6A019C1D_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
