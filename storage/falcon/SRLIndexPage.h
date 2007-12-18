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

// SRLIndexPage.h: interface for the SRLIndexPage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLINDEXPAGE_H__62E2E103_B529_46B9_BAE1_71C82A1D5724__INCLUDED_)
#define AFX_SRLINDEXPAGE_H__62E2E103_B529_46B9_BAE1_71C82A1D5724__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLIndexPage : public SerialLogRecord  
{
public:
	virtual void redo();
	virtual void print();
	virtual void pass2();
	virtual void pass1();
	virtual void read();
	void append(Dbb *dbb, TransId transId, int32 page, int32 lvl, int32 up, int32 left, int32 right, int length, const UCHAR *data);
	SRLIndexPage();
	virtual ~SRLIndexPage();

	int			tableSpaceId;
	int32		pageNumber;
	int32		parent;
	int32		prior;
	int32		next;
	int32		level;
	int32		length;
	const UCHAR	*data;
};

#endif // !defined(AFX_SRLINDEXPAGE_H__62E2E103_B529_46B9_BAE1_71C82A1D5724__INCLUDED_)
