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

// Dbb.h: interface for the Dbb class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DBB_H__6A019C20_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
#define AFX_DBB_H__6A019C20_A340_11D2_AB5A_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "IOx.h"
#include "HdrState.h"
#include "PageType.h"
#include "SyncObject.h"
#include "SparseArray.h"

#define TRANSACTION_ID(transaction)		((transaction) ? transaction->transactionId : 0)

// default parameters

#define PAGE_SIZE		4096
#define CACHE_SIZE		1024

// Selective debugging

#define DEBUG_NONE               0
#define DEBUG_KEYS               1 << 0
#define DEBUG_PAGES              1 << 1
#define DEBUG_SYSTEM_TABLES      1 << 2
#define DEBUG_NODES_ADDED        1 << 3
#define DEBUG_NODES_DELETED      1 << 4
#define DEBUG_SPLIT_PAGE         1 << 5
#define DEBUG_FIND_LEAF          1 << 6
#define DEBUG_FIND_LEVEL         1 << 7
#define DEBUG_SCAN_INDEX         1 << 8
#define DEBUG_INDEX_MERGE        1 << 9
#define DEBUG_INVERSION          1 << 10
#define DEBUG_DEFERRED_INDEX     1 << 11
#define DEBUG_RECORD_SCAVENGE    1 << 12
#define DEBUG_PAGE_LEVEL         1 << 13
#define DEBUG_ALL               -1

static const int FillLevels = 5;

struct SectionAnalysis 
{
   int32		recordLocatorPages;
   int32		dataPages;
   int32		overflowPages;
   int32		spaceAvailable;
   int32		records;
};

struct IndexAnalysis
{
	int		levels;
	int		upperLevelPages;
	int		leafPages;
	int		leafNodes;
	int		fillLevels[FillLevels];
	int64	leafSpaceUsed;	
};

struct PagesAnalysis
{
	int		maxPage;
	int		allocatedPages;
	int		totalPages;
};
	
class Cache;
class Section;
class Bitmap;
class Stream;
class Filter;
class ResultList;
class Inversion;
class Database;
class Validation;
class SerialLog;
class Transaction;
class IndexKey;
class RecordVersion;
class DeferredIndex;
class DatabaseCopy;
class DatabaseClone;

class Dbb : public IO  
{
public:
	void updateTableSpaceSection (int id);
	Dbb(Database *database);
	Dbb (Dbb *dbb, int tableSpaceId);
	virtual ~Dbb();

	void	setPrecedence (Bdb *lower, int32 higherPageNumber);
	void	createSection(int32 sectionId, TransId transId);
	void	dropDatabase();
	void	enableSerialLog();
	void	rollback (TransId transId, bool updateTransaction);
	void	updateRecord(int32 sectionId, int32 recordId, Stream *stream, TransId transId, bool earlyWrite);
	void	validateAndUpdateRecord(int32 sectionId, int32 recordNumber, Stream* stream, TransId transactionId, bool earlyWrite);
	void	prepareTransaction(TransId transId, int xidLength, const UCHAR *xid);
	void	commit(Transaction *transaction);
	void	reportStatistics();
	bool	hasDirtyPages();
	bool	deleteShadow (DatabaseCopy *shadow);
	void	close();
	void	init();
	void	initRepository(Hdr *header);
	void	freePage (int32 pageNumber);
	void	expungeRecord (Section *section, int32 recordNumber);
	void	printPage (int pageNumber);
	//void	cloneFile (DatabaseClone *file, bool isShadow);
	void	cloneFile (Database *database, const char *fileName, bool createShadow);
	void	analyzeSection (int sectionId, const char *sectionName, int indentation, Stream *stream);
	void	analyseIndex(int32 indexId, int indexVersion, const char *indexName, int indentation, Stream *stream);
	void	createInversion(TransId transId);
	Section* getSequenceSection(TransId transId);
	Bdb*	getSequencePage (int sequenceId, LockType lockType, TransId transId);
	int64	updateSequence(int sequenceId, int64 delta, TransId transId);
	int		createSequence(QUAD initialValue, TransId transId);
	bool	deleteIndexEntry (int32 indexId, int indexVersion, IndexKey *key, int32 recordNumber, TransId transId);
	void	validate (int optionMask);
	void	shutdown (TransId transId);
	void	deleteSection (int32 sectionId, TransId transId);
	void	freePage (Bdb *bdb, TransId transId);
	void	clearDebug();
	void	setDebug();
	void	deleteIndex (int32 indexId, int indexVersion, TransId transId);
	Cache*	open (const char *fileName, int64 cacheSize, TransId transId);
	void	flush();
	bool	addIndexEntry (int32 indexId, int indexVersion, IndexKey *key, int32 recordNumber, TransId transId);
	int32	createIndex(TransId transId, int indexVersion);
	int32	findNextRecord (Section *section, int32 startingRecord, Stream *stream);
	bool	fetchRecord (int32 sectionId, int32 recordNumber, Stream *stream);

	void	redoSequencePage(int pageSequence, int32 pageNumber);
	int64	redoSequence(int sequenceId, int64 sequence);
	void	redoDataPage (int sectionId, int32 pageNumber, int32 locatorPageNumber);
	void	redoRecordLocatorPage(int sectionId, int sequence, int32 pageNumber, bool isPostFlush);
	void	redoFreePage (int32 pageNumber);
	void	reInsertStub(int32 sectionId, int32 recordId, TransId transId);
	void	reallocPage (int32 pageNumber);

	Section*	findSection (int32 sectionId);
	void	logRecord (int32 sectionId, int32 recordId, Stream *stream, Transaction *transaction);
	int32	insertStub (int32 sectionId, Transaction *transaction);
	int32	createSection(TransId transId);
	Bdb*	handoffPage (Bdb *bdb, int32 pageNumber, PageType pageType, LockType lockType);
	Bdb*	allocPage (PageType pageType, TransId transId);
	Bdb*	fetchPage (int32 pageNumber, PageType pageType, LockType lockType);
	Bdb*	fakePage (int32 pageNumber, PageType pageType, TransId transId);
	Bdb*	trialFetch(int32 pageNumber, PageType pageType, LockType lockType);
	void	init(int pageSz, int cacheSize);
	Cache*	create (const char *fileName, int pageSize, int64 cacheSize, FileType fileType, TransId transId, const char *logRoot, uint64 initialAllocation);
	void	validateCache(void);
	void	logUpdatedRecords(Transaction* transaction, RecordVersion* records, bool chill = false);
	void	logIndexUpdates(DeferredIndex* deferredIndex);
	bool	sectionInUse(int sectionId);
	bool	indexInUse(int indexId);
	void	analyzeSpace(int indentation, Stream* stream);
	void	upgradeSequenceSection(void);
	int32	insertStub(Section* section, Transaction* transaction);
	void	updateRecord(Section* section, int32 recordId, Stream* stream, Transaction* transaction, bool earlyWrite);
	bool	fetchRecord(Section* section, int32 recordNumber, Stream* stream);
	void	addShadow(DatabaseCopy* shadow);
	void	skewHeader(Hdr* header);
	void	printPage(Bdb* bdb);
	void	updateBlob(Section *blobSection, int recordNumber, Stream* blob, Transaction* transaction);
	void	updateSerialLogBlockSize(void);
	
	Cache		*cache;
	Database	*database;
	int			tableSpaceId;
	int			pagesPerPip;
	short		pipSlots;
	int32		lastPageAllocated;
	int32		nextSection;
	int32		nextIndex;
	int32		highPage;
	int32		tableSpaceSectionId;
	short		pagesPerSection;
	short		linesPerPage;
	short		sequencesPerPage;
	int			sequencesPerSection;
	bool		utf8;
	Section		**sections;
	int			debug;
	int			sequence;
	int			odsVersion;
	int			odsMinorVersion;
	int			logOffset;
	int			logLength;
	int			defaultIndexVersion;
	int32		sequenceSectionId;
	Section		*sequenceSection;
	HdrState	priorState;
	Inversion	*inversion;
	DatabaseCopy *shadows;
	SyncObject	cloneSyncObject;
	SyncObject	sequencesSyncObject;
	SerialLog	*serialLog;
	JString		logRoot;
	SparseArray<int32, 100>	sequencePages;
};

#endif // !defined(AFX_DBB_H__6A019C20_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
