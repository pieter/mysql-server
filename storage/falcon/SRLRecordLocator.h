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

// SRLRecordLocator.h: interface for the SRLRecordLocator class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLSECTIONINDEX_H__FCEDA272_8505_45D0_8B3B_A64BC06322E2__INCLUDED_)
#define AFX_SRLSECTIONINDEX_H__FCEDA272_8505_45D0_8B3B_A64BC06322E2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLRecordLocator : public SerialLogRecord  
{

public:
	virtual void print();
	virtual void pass2();
	virtual void redo();
	virtual void pass1();
	void append (Dbb *dbb, TransId transId, int sectionId, int sequence, int32 pageNumber);
	virtual void read();
	SRLRecordLocator();
	virtual ~SRLRecordLocator();

	int		tableSpaceId;
	int		sectionId;
	int		sequence;
	int32	pageNumber;
};

#endif // !defined(AFX_SRLSECTIONINDEX_H__FCEDA272_8505_45D0_8B3B_A64BC06322E2__INCLUDED_)
