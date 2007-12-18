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

// SRLSectionPage.h: interface for the SRLSectionPage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLSECTIONPAGE_H__435DFA97_3663_4A97_8427_364B8205CAAB__INCLUDED_)
#define AFX_SRLSECTIONPAGE_H__435DFA97_3663_4A97_8427_364B8205CAAB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

class SRLSectionPage : public SerialLogRecord  
{
public:
	virtual void print();
	virtual void pass2();
	void redo();
	virtual void pass1();
	void append(Dbb *dbb, TransId transId, int32 parent, int32 page, int slot, int id, int seq, int lvl);
	virtual void read();
	SRLSectionPage();
	virtual ~SRLSectionPage();

	int		tableSpaceId;
	int32	parentPage;
	int32	pageNumber;
	int		sectionSlot;
	int		sequence;
	int		level;
	int		sectionId;
	int		incarnation;
};

#endif // !defined(AFX_SRLSECTIONPAGE_H__435DFA97_3663_4A97_8427_364B8205CAAB__INCLUDED_)
