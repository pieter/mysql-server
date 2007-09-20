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

// SRLRollback.h: interface for the SRLRollback class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLROLLBACK_H__C6C3F75B_8A2E_4CC0_81FC_689692EBB6BA__INCLUDED_)
#define AFX_SRLROLLBACK_H__C6C3F75B_8A2E_4CC0_81FC_689692EBB6BA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLRollback : public SerialLogRecord  
{
public:
	virtual void rollback();
	virtual void print();
	virtual void pass1();
	virtual void read();
	void append(TransId transId, bool updateTransaction);
	SRLRollback();
	virtual ~SRLRollback();

};

#endif // !defined(AFX_SRLROLLBACK_H__C6C3F75B_8A2E_4CC0_81FC_689692EBB6BA__INCLUDED_)
