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

// SRLCommit.h: interface for the SRLCommit class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLCOMMIT_H__6B4B950D_A21E_448F_82CB_3EB934C664F4__INCLUDED_)
#define AFX_SRLCOMMIT_H__6B4B950D_A21E_448F_82CB_3EB934C664F4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"
#include "Transaction.h"

class SRLCommit : public SerialLogRecord  
{
public:
	virtual void commit();
	virtual void pass1();
	void print();
	virtual void read();
	void append(Transaction *transaction);
	SRLCommit();
	virtual ~SRLCommit();

};

#endif // !defined(AFX_SRLCOMMIT_H__6B4B950D_A21E_448F_82CB_3EB934C664F4__INCLUDED_)
