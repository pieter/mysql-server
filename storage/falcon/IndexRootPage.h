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

// IndexRootPage.h: interface for the IndexRootPage class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INDEXROOTPAGE_H__6A019C27_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
#define AFX_INDEXROOTPAGE_H__6A019C27_A340_11D2_AB5A_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "RootPage.h"
#include "IndexPage.h"
#include "SynchronizationObject.h"

class Dbb;
class Bdb;
class Bitmap;
class Btn;
class IndexKey;
class SRLUpdateIndex;
struct IndexAnalysis;

class IndexRootPage : public RootPage  
{
public:
	static void		debugBucket (Dbb *dbb, int indexId, int recordNumber, TransId transactionId);
	static void		deleteIndex (Dbb *dbb, int32 indexId, TransId transId);
	static bool		deleteIndexEntry (Dbb *dbb, int32 indexId, IndexKey *key, int32 recordNumber, TransId transId);
	static bool		splitIndexPage (Dbb *dbb, int32 indexId, Bdb *bdb, TransId transId,
									AddNodeResult addResult, IndexKey *indexKey, int recordNumber);
	static void		scanIndex (Dbb *dbb, int32 indexId, int32 rootPage, IndexKey *low, IndexKey *high, int searchFlags, TransId transId, Bitmap *bitmap);
	static Bdb*		findRoot (Dbb *dbb, int32 indexId, int32 rootPage, LockType lockType, TransId transId);
	static Bdb*		findLeaf (Dbb *dbb, int32 indexId, int32 rootPage, IndexKey *key, LockType lockType, TransId transId);
	static Bdb*		findInsertionLeaf (Dbb *dbb, int32 indexId, IndexKey *key, int32 recordNumber, TransId transId);
	static bool		addIndexEntry (Dbb *dbb, int32 indexId, IndexKey *key, int32 recordNumber, TransId transId);
	static int32	createIndex (Dbb *dbb, TransId transId);
	static void		create (Dbb *dbb, TransId transId);
	static void		indexMerge(Dbb *dbb, int indexId, SRLUpdateIndex *indexNodes, TransId transId);
	static Bdb*		createIndexRoot(Dbb* dbb, TransId transId);
	static void		analyzeIndex(Dbb* dbb, int indexId, IndexAnalysis *indexAnalysis);
	static int32	getIndexRoot(Dbb* dbb, int indexId);

	static void		redoIndexPage(Dbb* dbb, int32 pageNumber, int32 parentPageNumber, int level, int32 prior, int32 next, int length, const UCHAR *data);
	static void		setIndexRoot(Dbb* dbb, int indexId, int32 pageNumber, TransId transId);
	static void		redoIndexDelete(Dbb* dbb, int indexId);
	static void		redoCreateIndex(Dbb* dbb, int indexId);
};

#endif // !defined(AFX_INDEXROOTPAGE_H__6A019C27_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
