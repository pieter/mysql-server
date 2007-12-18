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

// SRLRecordStub.h: interface for the SRLRecordStub class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLRECORDSTUB_H__A3F34C6B_9930_466F_BA0D_A0B426258BF6__INCLUDED_)
#define AFX_SRLRECORDSTUB_H__A3F34C6B_9930_466F_BA0D_A0B426258BF6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLRecordStub : public SerialLogRecord  
{
public:
	virtual void redo();
	virtual void pass1();
	void print();
	virtual void read();
	void append(Dbb *dbb, TransId transId, int32 sectionId, int32 recordNumber);
	SRLRecordStub();
	virtual ~SRLRecordStub();

	int		tableSpaceId;
	int32	sectionId;
	int32	recordId;

};

#endif // !defined(AFX_SRLRECORDSTUB_H__A3F34C6B_9930_466F_BA0D_A0B426258BF6__INCLUDED_)
