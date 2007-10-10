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

// SRLDataPage.h: interface for the SRLDataPage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLDATAPAGE_H__3CABD399_4046_4A48_BD1B_157253D2D038__INCLUDED_)
#define AFX_SRLDATAPAGE_H__3CABD399_4046_4A48_BD1B_157253D2D038__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLDataPage : public SerialLogRecord  
{
public:
	virtual void print();
	virtual void pass2();
	void redo();
	virtual void pass1();
	virtual void read();
	void append (Dbb *dbb, TransId transId, int sectionId, int32 locatorPageNumber, int32 pageNumber);
	SRLDataPage();
	virtual ~SRLDataPage();

	int		tableSpaceId;
	int		sectionId;
	int32	pageNumber;
	int32	locatorPageNumber;
};

#endif // !defined(AFX_SRLDATAPAGE_H__3CABD399_4046_4A48_BD1B_157253D2D038__INCLUDED_)
