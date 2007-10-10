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

// SequencePage.h: interface for the SequencePage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SEQUENCEPAGE_H__52A2DA16_7937_11D4_98F0_0000C01D2301__INCLUDED_)
#define AFX_SEQUENCEPAGE_H__52A2DA16_7937_11D4_98F0_0000C01D2301__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Page.h"

class SequencePage : public Page  
{
public:
	int32	filler;			// gcc doesn't align quads
	int64	sequences[1];
};

#endif // !defined(AFX_SEQUENCEPAGE_H__52A2DA16_7937_11D4_98F0_0000C01D2301__INCLUDED_)
