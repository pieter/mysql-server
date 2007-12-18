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

// SRLDeleteIndex.h: interface for the SRLDeleteIndex class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLDELETEINDEX_H__9F0B73E5_60FE_40C3_B8A8_AD1F645532AB__INCLUDED_)
#define AFX_SRLDELETEINDEX_H__9F0B73E5_60FE_40C3_B8A8_AD1F645532AB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLDeleteIndex : public SerialLogRecord  
{
public:
	SRLDeleteIndex();
	virtual ~SRLDeleteIndex();
	virtual void	redo();
	virtual void	pass1();
	virtual void	read();
	virtual void	commit(void);
	void			append(Dbb *dbb, TransId transId, int indexId, int idxVersion);
	void			print();

	int		tableSpaceId;
	int32	indexId;
	int		indexVersion;
};

#endif // !defined(AFX_SRLDELETEINDEX_H__9F0B73E5_60FE_40C3_B8A8_AD1F645532AB__INCLUDED_)
