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
// SRLCreateIndex.h: interface for the SRLCreateIndex class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLCREATEINDEX_H__4044BC3E_E422_4CB3_B61F_316DD6ED5B96__INCLUDED_)
#define AFX_SRLCREATEINDEX_H__4044BC3E_E422_4CB3_B61F_316DD6ED5B96__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLCreateIndex : public SerialLogRecord  
{
public:
	SRLCreateIndex();
	virtual ~SRLCreateIndex();

	void			append (Dbb *dbb, TransId transId, int32 indexId, int indexVersion);
	virtual void	pass1();
	virtual void	print();
	virtual void	redo();
	virtual void	read();
	virtual void	commit(void);

	int			tableSpaceId;
	int32		indexId;
	int			indexVersion;
};

#endif // !defined(AFX_SRLCREATEINDEX_H__4044BC3E_E422_4CB3_B61F_316DD6ED5B96__INCLUDED_)
