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

// DataPage.h: interface for the DataPage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DATAPAGE_H__6A019C29_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
#define AFX_DATAPAGE_H__6A019C29_A340_11D2_AB5A_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Page.h"

class Dbb;
class Bdb;
class Stream;
class Section;

struct RecordIndex;
struct SectionAnalysis;

struct LineIndex {
	short		offset;
	short		length;				// negative means record has overflow page
	};

class DataPage : public Page
{
public:
	void	analyze (Dbb *dbb, SectionAnalysis *analysis);
	void	validate(Dbb *dbb, Validation *validation);
	void	deleteOverflowPages (Dbb *dbb, int32 overflowPageNumber, TransId transId);
	void	validate(Dbb *dbb);
	int		deleteLine (Dbb *dbb, int line, TransId transId);
	int		storeRecord (Dbb *dbb, Bdb *bdb, RecordIndex *index, int length, Stream *stream, int32 overflowPage, TransId transId, bool earlyWrite);
	bool	fetchRecord (Dbb *dbb, int line, Stream *stream);
	int		compressPage(Dbb *dbb);
	int		updateRecord (Section *section, int line, Stream *stream, TransId transId, bool earlyWrite);
	int		computeSpaceAvailable(int pageSize);
	void	deletePage(Dbb *dbb, TransId transId);
	void	print(void);

	short		maxLine;
	LineIndex	lineIndex [1];
};

#endif // !defined(AFX_DATAPAGE_H__6A019C29_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
