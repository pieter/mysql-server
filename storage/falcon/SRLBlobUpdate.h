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

// SRLBlobUpdate.h: interface for the SRLBlobUpdate class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLBLOBUPDATE_H__7E4768DE_57C2_43B3_9F54_18044725A897__INCLUDED_)
#define AFX_SRLBLOBUPDATE_H__7E4768DE_57C2_43B3_9F54_18044725A897__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLBlobUpdate : public SerialLogRecord  
{
public:
	SRLBlobUpdate();
	virtual ~SRLBlobUpdate();

	virtual void	read();
	virtual void	print();
	void			append(Dbb *dbb, TransId transId, int32 locPage, int locLine, int32 blobPage, int blobLine);
	virtual void	pass1(void);
	virtual void	pass2(void);
	virtual void	redo(void);

	int			tableSpaceId;
	int32		sectionId;
	int32		locatorPage;
	int			locatorLine;
	int32		dataPage;
	int			dataLine;
};

#endif // !defined(AFX_SRLBLOBUPDATE_H__7E4768DE_57C2_43B3_9F54_18044725A897__INCLUDED_)
