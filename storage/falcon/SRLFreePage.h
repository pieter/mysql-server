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

// SRLFreePage.h: interface for the SRLFreePage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLFREEPAGE_H__FB37FC48_BFFA_43CE_83E0_A1F1327E170D__INCLUDED_)
#define AFX_SRLFREEPAGE_H__FB37FC48_BFFA_43CE_83E0_A1F1327E170D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLFreePage : public SerialLogRecord  
{
public:
	SRLFreePage();
	virtual ~SRLFreePage();

	virtual void	print();
	virtual void	pass1();
	virtual void	redo();
	virtual void	read();
	virtual void	pass2(void);
	virtual void	commit(void);
	
	void			append (Dbb *dbb, int32 pageNumber);

	int			tableSpaceId;
	int32		pageNumber;
	int			incarnation;
};

#endif // !defined(AFX_SRLFREEPAGE_H__FB37FC48_BFFA_43CE_83E0_A1F1327E170D__INCLUDED_)
