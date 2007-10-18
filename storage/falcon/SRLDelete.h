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

// SRLDelete.h: interface for the SRLDelete class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLDELETE_H__E543E560_4DA8_4CCE_8C7C_2F36328F31AA__INCLUDED_)
#define AFX_SRLDELETE_H__E543E560_4DA8_4CCE_8C7C_2F36328F31AA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class Transaction;

class SRLDelete : public SerialLogRecord  
{
public:
	virtual void commit();
	virtual void redo();
	void pass1();
	virtual void print();
	virtual void read();
	void append(Dbb *dbb, Transaction *transaction, int32 sectionId, int32 recordId);
	SRLDelete();
	virtual ~SRLDelete();

	int			tableSpaceId;
	int32		sectionId;
	int32		recordId;
	int32		length;
};

#endif // !defined(AFX_SRLDELETE_H__E543E560_4DA8_4CCE_8C7C_2F36328F31AA__INCLUDED_)
