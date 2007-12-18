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

// PagePrecedence.h: interface for the PagePrecedence class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PAGEPRECEDENCE_H__A8C8A877_7EC4_497A_BC51_1D335EFDBB3B__INCLUDED_)
#define AFX_PAGEPRECEDENCE_H__A8C8A877_7EC4_497A_BC51_1D335EFDBB3B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Bdb;


class PagePrecedence  
{
public:
	PagePrecedence(Bdb *low, Bdb *high);
	virtual ~PagePrecedence();

	Bdb				*higher;
	Bdb				*lower;
	PagePrecedence	*nextHigh;
	PagePrecedence	*nextLow;
	void setPrecedence(Bdb* low, Bdb* high);
};

#endif // !defined(AFX_PAGEPRECEDENCE_H__A8C8A877_7EC4_497A_BC51_1D335EFDBB3B__INCLUDED_)
