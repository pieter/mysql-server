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

// SRLIndexDelete.h: interface for the SRLIndexDelete class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLINDEXDELETE_H__55E77064_3AC3_4192_AC6B_10931ABB99BC__INCLUDED_)
#define AFX_SRLINDEXDELETE_H__55E77064_3AC3_4192_AC6B_10931ABB99BC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class IndexKey;

class SRLIndexDelete : public SerialLogRecord  
{
public:
	virtual void print();
	void append(Dbb *dbb, int32 indexId, int idxVersion, IndexKey *key, int32 recordNumber, TransId transactionId);
	virtual void redo();
	virtual void read();
	SRLIndexDelete();
	virtual ~SRLIndexDelete();

	int			tableSpaceId;
	int32		indexId;
	int32		recordId;
	int32		length;
	int			indexVersion;
	const UCHAR	*data;
};

#endif // !defined(AFX_SRLINDEXDELETE_H__55E77064_3AC3_4192_AC6B_10931ABB99BC__INCLUDED_)
