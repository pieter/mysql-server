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

// InversionFilter.h: interface for the InversionFilter class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INVERSIONFILTER_H__EB3207E1_D03A_11D4_98FF_0000C01D2301__INCLUDED_)
#define AFX_INVERSIONFILTER_H__EB3207E1_D03A_11D4_98FF_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class InversionWord;

class InversionFilter  
{
public:
	virtual InversionWord* getWord () = 0;
	virtual void start () = 0;
	virtual ~InversionFilter();
};

#endif // !defined(AFX_INVERSIONFILTER_H__EB3207E1_D03A_11D4_98FF_0000C01D2301__INCLUDED_)