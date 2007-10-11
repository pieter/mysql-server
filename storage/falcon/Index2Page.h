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

// Index2Page.h: interface for the Index2Page class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _INDEX2_PAGE_H_
#define _INDEX2_PAGE_H_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "RootPage.h"
#include "Btn.h"

class Dbb;
class Bdb;
class Bitmap;
class IndexKey;

class Index2Page : public Page  
{
public:
	AddNodeResult addNode (Dbb *dbb, IndexKey *key, int32 recordNumber);
	Btn*		appendNode (IndexKey *indexKey, int32 recordNumber, int pageSize);
	Btn*		appendNode(int length, UCHAR *key, int32 recordNumber, int pageSize);
	int			deleteNode (Dbb *dbb, IndexKey *key, int32 recordNumber);
	Btn*		findNodeInBranch (IndexKey *indexKey);
	Btn*		findNode (IndexKey *key, IndexKey *expandedKey);
	Btn*		findInsertionPoint (IndexKey *indexKey, int32 recordNumber, IndexKey *expandedKey);
	Bdb*		splitPage (Dbb *dbb, Bdb *bdb, TransId transId);
	Bdb*		splitIndexPageEnd(Dbb *dbb, Bdb *bdb, TransId transId, IndexKey *insertKey, int recordNumber);
	Bdb*		splitIndexPageMiddle(Dbb *dbb, Bdb *bdb, IndexKey *splitKey, TransId transId);
	bool		isLastNode (Btn *node);

	void		analyze (int pageNumber);
	//void		validateInsertion (int keyLength, UCHAR * key, int32 recordNumber);
	void		validateNodes (Dbb *dbb, Validation *validation, Bitmap *children, int32 parentPageNumber);
	void		validate (Dbb *dbb, Validation *validation, Bitmap *pages, int32 parentPageNumber);
	void		validate(void *before);

	static int		computePrefix (int l1, UCHAR *v1, int l2, UCHAR *v2);
	static int		computePrefix (IndexKey *key1, IndexKey *key2);
	static int		keyCompare(int length1, UCHAR *key1, int length2, UCHAR *key2);
	static Bdb*		findLevel (Dbb *dbb, Bdb *bdb, int level, IndexKey *indexKey, int32 recordNumber);
	static Bdb*		createNewLevel (Dbb *dbb, int level, int version, int32 page1, int32 page2, IndexKey *key2, TransId transId);
	static void		printPage (Bdb *bdb, bool inversion);
	static void		printPage (Index2Page *page, int32 pageNumber, bool inversion);
	static int32	getRecordNumber(const UCHAR *ptr);
	static void		logIndexPage (Bdb *bdb, TransId transId);
	static Btn*		findInsertionPoint(int level, IndexKey* indexKey, int32 recordNumber, IndexKey* expandedKey, Btn* nodes, Btn* bucketEnd);

	int32	parentPage;
	int32	priorPage;
	int32	nextPage;
	//short	level;
	UCHAR	level;
	UCHAR	version;
	short	length;
	Btn		nodes [1];
};

#endif // !defined(AFX_INDEXPAGE_H__5DD7F231_A406_11D2_AB5B_0000C01D2301__INCLUDED_)
