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

// SRLData.h: interface for the SRLData class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLDATA_H__112218C9_BFAE_4147_8093_5C1AFE9902D9__INCLUDED_)
#define AFX_SRLDATA_H__112218C9_BFAE_4147_8093_5C1AFE9902D9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class Stream;
class Transaction;

class SRLData : public SerialLogRecord  
{
public:
	SRLData();
	virtual ~SRLData();

	virtual void	commit();
	virtual void	redo();
	virtual void	pass1();
	virtual void	print();
	virtual void	read();
	virtual void	recoverLimbo(void);
	
	void			append(Dbb *dbb, Transaction *transaction, int32 sectionId, int32 recordId, Stream *stream);

	int			tableSpaceId;
	int32		sectionId;
	int32		recordId;
	int32		length;
	const UCHAR	*data;
};

#endif // !defined(AFX_SRLDATA_H__112218C9_BFAE_4147_8093_5C1AFE9902D9__INCLUDED_)
