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

// SRLDropTable.h: interface for the SRLDropTable class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLDROPTABLE_H__4274AAA3_0F34_493F_9F6F_BDCFB2CC28FB__INCLUDED_)
#define AFX_SRLDROPTABLE_H__4274AAA3_0F34_493F_9F6F_BDCFB2CC28FB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLDropTable : public SerialLogRecord  
{
public:
	virtual void print();
	virtual void redo();
	virtual void pass1();
	virtual void commit(void);
	virtual void rollback(void);
	virtual void pass2(void);
	virtual void read();
	void append(Dbb *dbb, TransId transId, int sectionId);
	SRLDropTable();
	virtual ~SRLDropTable();


	int		tableSpaceId;
	int		sectionId;
};

#endif // !defined(AFX_SRLDROPTABLE_H__4274AAA3_0F34_493F_9F6F_BDCFB2CC28FB__INCLUDED_)
