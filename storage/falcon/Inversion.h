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

// Inversion.h: interface for the Inversion class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INVERSION_H__84FD1976_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_INVERSION_H__84FD1976_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SyncObject.h"

#define MAX_INV_KEY		128
#define MAX_INV_WORD	100

static const int INVERSION_VERSION_NUMBER	= 0;

class Dbb;
class InversionFilter;
class Bdb;
class ResultList;
class Validation;
class IndexKey;

class Inversion  
{
public:
	void removeWord(int keyLength, UCHAR *key, TransId transId);
	void removeFromInversion(InversionFilter *filter, TransId transId);
	void validate (Validation *validation);
	void flush (TransId transId);
	static int compare (UCHAR *key1, UCHAR *key2);
	void sort(UCHAR **records, int size);
	void summary();
	void deleteInversion (TransId transId);
	Bdb* findInversion (IndexKey *indexKey, LockType lockType);
	void propogateSplit (int level, IndexKey *indexKey, int32 pageNumber, TransId transId);
	void updateInversionRoot (int32 pageNumber, TransId transId);
	Bdb* findRoot (LockType lockType);
	void addWord (int keyLength, UCHAR *key, TransId transId);
	void init();
	void createInversion (TransId transId);
	int32 addInversion (InversionFilter *filter, TransId transId, bool insertFlag);
	Inversion(Dbb *db);
	virtual ~Inversion();

	SyncObject		syncObject;
	bool		inserting;
	Dbb			*dbb;
	UCHAR		**sortBuffer;
	int			records;
	int			size;
	UCHAR		*lastRecord;
	int			wordsAdded;
	int			wordsRemoved;
	int			runs;
	int			state;
};

#endif // !defined(AFX_INVERSION_H__84FD1976_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
