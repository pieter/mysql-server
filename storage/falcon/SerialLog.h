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

// SerialLog.h: interface for the SerialLog class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERIALLOG_H__D2A71E6B_A3B0_41C8_9C73_628DA067F722__INCLUDED_)
#define AFX_SERIALLOG_H__D2A71E6B_A3B0_41C8_9C73_628DA067F722__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SyncObject.h"
#include "Schedule.h"
#include "Stack.h"
#include "DenseArray.h"
#include "Queue.h"

static const unsigned int altLogFlag	= 0x80000000;
static const int srlSignature			= 123456789;
static const int SLT_HASH_SIZE			= 1001;
static const int SRL_WINDOW_SIZE		= 1048576;

// States for recovery objects

static const int objUnknown				= 0;
static const int objInUse				= 1;
static const int objDeleted				= 2;

struct SerialLogBlock 
{
	uint64	blockNumber;
	uint64	readBlockNumber;
	uint32	length;
	uint32	creationTime;
	uint16	version;
	UCHAR	data[1];
};

struct TableSpaceInfo
{
	int						tableSpaceId;
	TableSpaceInfo			*collision;
	TableSpaceInfo			*next;
	DenseArray<volatile long, 200>	sectionUseVector;
	DenseArray<volatile long, 200>	indexUseVector;
};

class SerialLogFile;
class Database;
class Dbb;
class Thread;
class SerialLogControl;
class SerialLogWindow;
class SerialLogTransaction;
class Bitmap;
class IO;
class RecoveryObjects;
class Sync;
class Transaction;
class InfoTable;
class TableSpaceManager;
class Gopher;

class SerialLog : public Schedule, public SyncObject
{
public:
	SerialLog(Database *db, JString schedule, int maxTransactionBacklog);
	virtual ~SerialLog();

	void			putVersion();
	void			pageCacheFlushed(int64 flushArg);

	void			releaseBuffer (UCHAR *buffer);
	UCHAR*			allocBuffer();
	void			shutdownNow();
	void			dropDatabase();
	uint32			appendLog (IO *shadow, int lastPage);
	bool			isSectionActive(int sectionId, int tableSpaceId);
	bool			isIndexActive(int indexId, int tableSpaceId);
	SerialLogBlock* findLastBlock (SerialLogWindow *window);
	void			initializeWriteBlock (SerialLogBlock *block);
	void			checkpoint(bool force);
	virtual void	execute(Scheduler * scheduler);
	void			transactionDelete (SerialLogTransaction *transaction);
	uint64			getReadBlock();
	SerialLogTransaction* getTransaction (TransId transactionId);
	SerialLogTransaction* findTransaction (TransId transactionId);
	SerialLogWindow* findWindowGivenOffset(uint64 virtualOffset);
	SerialLogWindow* findWindowGivenBlock(uint64 blockNumber);
	SerialLogWindow* allocWindow(SerialLogFile *file, int64 origin);
	void			initializeLog(int64 blockNumber);
	void			release (SerialLogWindow *window);
	void			wakeup();
	void			startRecord();
	void			putData(uint32 length, const UCHAR *data);
	void			close();
	void			shutdown();
	uint64			flush(bool forceNewWindow, uint64 commitBlockNumber, Sync *syncPtr);
	void			recover();
	void			start();
	void			open(JString fileRoot, bool createFlag);
	void			copyClone(JString fileRoot, int logOffset, int logLength);
	int				recoverLimboTransactions(void);
	int				recoverGetNextLimbo(int xidSize, unsigned char *xid);

	void			preFlush(void);
	void			wakeupFlushQueue(Thread *ourThread);
	
	void			setSectionActive(int id, int tableSpaceId);
	void			setSectionInactive(int id, int tableSpaceId);
	void			setIndexActive(int id, int tableSpaceId);
	void			setIndexInactive(int id, int tableSpaceId);
	void			updateSectionUseVector(uint sectionId, int tableSpaceId, int delta);
	void			updateIndexUseVector(uint indexId, int tableSpaceId, int delta);
	bool			sectionInUse(int sectionId, int tableSpaceId);
	bool			bumpIndexIncarnation(int indexId, int tableSpaceId, int state);
	bool			bumpSectionIncarnation (int sectionId, int tableSpaceId, int state);
	bool			bumpPageIncarnation (int32 pageNumber, int tableSpaceId, int state);

	int				getPageState(int32 pageNumber, int tableSpaceId);
	void			redoFreePage(int32 pageNumber, int tableSpaceId);
	
	bool			indexInUse(int indexId, int tableSpaceId);
	void			overflowFlush(void);
	void			createNewWindow(void);
	void			setPhysicalBlock(TransId transactionId);
	void			reportStatistics(void);
	void			getSerialLogInfo(InfoTable* tableInfo);
	void			commitByXid(int xidLength, const UCHAR* xid);
	void			rollbackByXid(int xidLength, const UCHAR* xid);
	void			preCommit(Transaction* transaction);
	void			printWindows(void);
	void			endRecord(void);
	Dbb*			getDbb(int tableSpaceId);
	TableSpaceInfo	*getTableSpaceInfo(int tableSpaceId);
	void			preUpdate(void);
	Dbb*			findDbb(int tableSpaceId);
	uint64			getWriteBlockNumber(void);
	void			unblockUpdates(void);
	void			blockUpdates(void);
	int				getBlockSize(void);
	
	TableSpaceManager	*tableSpaceManager;
	SerialLogFile		*file1;
	SerialLogFile		*file2;
	SerialLogWindow		*firstWindow;
	SerialLogWindow		*lastWindow;
	SerialLogWindow		*freeWindows;
	SerialLogWindow		*writeWindow;
	SerialLogTransaction	*transactions[SLT_HASH_SIZE];
	SerialLogTransaction	*nextLimboTransaction;
	Database			*database;
	RecoveryObjects		*recoveryPages;
	RecoveryObjects		*recoverySections;
	RecoveryObjects		*recoveryIndexes;
	Dbb					*defaultDbb;
	Gopher				*gophers;
	Thread				*srlQueue;
	Thread				*endSrlQueue;
	SerialLogControl	*logControl;
	uint64				nextBlockNumber;
	uint64				highWaterBlock;
	uint64				lastFlushBlock;
	uint64				lastReadBlock;
	uint64				recoveryBlockNumber;
	uint64				lastBlockWritten;
	UCHAR				*recordStart;
	UCHAR				*writePtr;
	UCHAR				*writeWarningTrack;
	SerialLogBlock		*writeBlock;
	SyncObject			syncWrite;
	SyncObject			syncSections;
	SyncObject			syncIndexes;
	SyncObject			syncGopher;
	SyncObject			syncUpdateStall;
	Stack				buffers;
	UCHAR				*bufferSpace;
	time_t				creationTime;
	bool				active;
	bool				pass1;
	volatile bool		finishing;
	bool				recordIncomplete;
	bool				recovering;
	bool				blocking;
	bool				writeError;
	int					logicalFlushes;
	int					physicalFlushes;
	int					recoveryPhase;
	int					eventNumber;
	int					windowReads;
	int					windowWrites;
	int					commitsComplete;
	int					backlogStalls;
	int					priorWindowReads;
	int					priorWindowWrites;
	int					priorCommitsComplete;
	int					priorBacklogStalls;
	int					windowBuffers;
	int					maxWindows;
	int					priorCount;
	int					maxTransactions;
	uint64				priorDelta;
	int					priorWrites;
	int32				tracePage;
	uint32				chilledRecords;
	uint64				chilledBytes;
	
	TableSpaceInfo		*tableSpaces[SLT_HASH_SIZE];
	TableSpaceInfo		*tableSpaceInfo;
	SerialLogTransaction		*earliest;
	SerialLogTransaction		*latest;
	
	Queue<SerialLogTransaction>	pending;
	Queue<SerialLogTransaction>	inactions;
	Queue<SerialLogTransaction>	running;
	
private:
	Thread *volatile	writer;
};

#endif // !defined(AFX_SERIALLOG_H__D2A71E6B_A3B0_41C8_9C73_628DA067F722__INCLUDED_)
