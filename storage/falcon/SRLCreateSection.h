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

// SRLCreateSection.h: interface for the SRLCreateSection class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLCREATESECTION_H__939767F3_F5AF_49EC_81CC_E64B4D81EECE__INCLUDED_)
#define AFX_SRLCREATESECTION_H__939767F3_F5AF_49EC_81CC_E64B4D81EECE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLCreateSection : public SerialLogRecord  
{
public:
	virtual void redo();
	virtual void commit();
	virtual void pass1();
	virtual void read();
	virtual void print(void);
	virtual void pass2(void);
	void append(Dbb *dbb, TransId transId, int32 id);
	SRLCreateSection();
	virtual ~SRLCreateSection();

	int		tableSpaceId;
	int		sectionId;
};

#endif // !defined(AFX_SRLCREATESECTION_H__939767F3_F5AF_49EC_81CC_E64B4D81EECE__INCLUDED_)
