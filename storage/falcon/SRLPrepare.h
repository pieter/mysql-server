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

// SRLPrepare.h: interface for the SRLPrepare class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLPREPARE_H__377DE32A_DBF5_45F9_A0E2_7DA4384F5A92__INCLUDED_)
#define AFX_SRLPREPARE_H__377DE32A_DBF5_45F9_A0E2_7DA4384F5A92__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLPrepare : public SerialLogRecord  
{
public:
	virtual void print();
	virtual void pass1();
	virtual void read();
	virtual void commit();
	virtual void rollback();

	SRLPrepare();
	virtual ~SRLPrepare();

	virtual void append(TransId transId, int xidLength, const UCHAR *xid);
	
	int			xidLength;
	const UCHAR	*xid;
};

#endif // !defined(AFX_SRLPREPARE_H__377DE32A_DBF5_45F9_A0E2_7DA4384F5A92__INCLUDED_)
