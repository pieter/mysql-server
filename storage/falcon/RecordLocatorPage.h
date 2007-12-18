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

// SECTIONINDEXPAGE.h: interface for the SECTIONINDEXPAGE class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SECTIONINDEXPAGE_H__6A019C23_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
#define AFX_SECTIONINDEXPAGE_H__6A019C23_A340_11D2_AB5A_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Page.h"

struct RecordIndex {
	int32	page;
	short	line;
	short	spaceAvailable;
	};

class Validation;
class Bitmap;

struct SectionAnalysis;

class RecordLocatorPage : public Page  
{
public:
	//RecordLocatorPage();
	//~RecordLocatorPage();

	void	repair (Dbb *dbb, int sectionId, int sequence);
	void	analyze (Dbb *dbb, SectionAnalysis *analysis, int sectionId, int Sequence, Bitmap *dataPages);
	bool	validate(Dbb *dbb, Validation *validation, int sectionId, int sequence, Bitmap *dataPages);
	void	deleteLine (int line, int spaceAvailable);
	int		nextSpaceSlot(int priorSlot);
	int		findSpaceSlot(int32 pageNumber);
	void	validateSpaceSlots(void);
	void	corrupt(void);
	void	printPage(void);
	void	setIndexSlot(int slot, int32 page, int line, int availableSpace);
	void	expungeDataPage(int32 pageNumber);
	void	deleteDataPages(Dbb* dbb, TransId transId);

protected:
	void	insertSpaceSlot(int slot, int availableSpace);
	void	unlinkSpaceSlot(int slot);
	void	linkSpaceSlot(int from, int to);

public:
	inline bool isSpaceSlot (int slot)
		{
		return elements[slot].spaceAvailable > 0;
		}

	int			section;
	int			sequence;
	int			maxLine;
	RecordIndex elements [1];
};

#endif // !defined(AFX_SECTIONINDEXPAGE_H__6A019C23_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
