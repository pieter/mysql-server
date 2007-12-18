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

// PageWriter.h: interface for the PageWriter class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PAGEWRITER_H__6FAAACDB_F765_4816_90DA_291C969267BE__INCLUDED_)
#define AFX_PAGEWRITER_H__6FAAACDB_F765_4816_90DA_291C969267BE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SyncObject.h"
#include "Mutex.h"

class Cache;
class Database;
class Dbb;
class Thread;
class Transaction;
struct DirtyTrans;

struct DirtyPage {
	DirtyPage	*nextPage;
	DirtyPage	*priorPage;
	DirtyPage	*pageCollision;
	DirtyTrans	*transaction;
	DirtyPage	*transNext;
	DirtyPage	*transPrior;
	Dbb			*dbb;
	int32		pageNumber;
	};

struct DirtyTrans {
	DirtyTrans	*collision;
	DirtyTrans	*next;
	TransId		transactionId;
	DirtyPage	*firstPage;
	DirtyPage	*lastPage;
	Thread		*thread;

	void removePage(DirtyPage *page);
	void clear();
	void addPage (DirtyPage *page);
	};

static const int pagesPerLinen = 200;
static const int dirtyHashSize = 101;

struct DirtyLinen {
	DirtyLinen	*next;
	DirtyPage	dirtyPages[pagesPerLinen];
	};

struct DirtySocks {
	DirtySocks	*next;
	DirtyTrans	transactions[pagesPerLinen];
	};

class PageWriter
{
public:
	void validate();
	void shutdown (bool panic);
	void release (DirtyTrans *transaction);
	DirtyTrans* getDirtyTransaction(TransId transactionId);
	DirtyTrans* findDirtyTransaction(TransId transactionId);
	void waitForWrites(Transaction *transaction);
	void removeElement (DirtyPage *element);
	void pageWritten(Dbb *dbb, int32 pageNumber);
	void start();
	void writer();
	static void writer (void *arg);
	void release (DirtyPage *element);
	DirtyPage* getElement();
	void writePage(Dbb *dbb, int32 pageNumber, TransId transactionId);
	PageWriter(Database *db);
	virtual ~PageWriter();

	DirtyPage	*firstPage;
	DirtyPage	*lastPage;
	DirtyPage	*freePages;
	DirtyTrans	*freeTransactions;
	DirtyLinen	*dirtyLinen;
	DirtySocks	*dirtySocks;
	Cache		*cache;
	Database	*database;
	Mutex		syncObject;
	Thread		*thread;
	DirtyPage	*pageHash[dirtyHashSize];
	DirtyTrans	*transactions[dirtyHashSize];
	bool		shuttingDown;
};

#endif // !defined(AFX_PAGEWRITER_H__6FAAACDB_F765_4816_90DA_291C969267BE__INCLUDED_)
