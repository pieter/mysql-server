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

// SerialLogControl.h: interface for the SerialLogControl class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERIALLOGCONTROL_H__77229761_E146_4AE4_8BBC_2114F6A0FC93__INCLUDED_)
#define AFX_SERIALLOGCONTROL_H__77229761_E146_4AE4_8BBC_2114F6A0FC93__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SRLSwitchLog.h"
#include "SRLData.h"
#include "SRLCommit.h"
#include "SRLRollback.h"
#include "SRLPrepare.h"
#include "SRLIndexUpdate.h"
#include "SRLWordUpdate.h"
#include "SRLPrepare.h"
#include "SRLRecordStub.h"
#include "SRLSequence.h"
#include "SRLCheckpoint.h"
#include "SRLBlobUpdate.h"
#include "SRLDelete.h"
#include "SRLDropTable.h"
#include "SRLCreateSection.h"
#include "SRLSectionPage.h"
#include "SRLFreePage.h"
#include "SRLRecordLocator.h"
#include "SRLDataPage.h"
#include "SRLIndexAdd.h"
#include "SRLIndexDelete.h"
#include "SRLIndexPage.h"
#include "SRLInversionPage.h"
#include "SRLCreateIndex.h"
#include "SRLDeleteIndex.h"
#include "SRLVersion.h"
#include "SRLUpdateRecords.h"
#include "SRLUpdateIndex.h"
#include "SRLSectionPromotion.h"
#include "SRLSequencePage.h"
#include "SRLSectionLine.h"
#include "SRLOverflowPages.h"
#include "SRLCreateTableSpace.h"
#include "SRLDropTableSpace.h"
#include "SRLBlobDelete.h"
#include "SRLUpdateBlob.h"
#include "SRLSession.h"

#define LOW_BYTE_FLAG	0x80

class SerialLogControl  
{
public:
	SerialLogRecord* getRecordManager(int which);
	SerialLogControl(SerialLog *serialLog);
	virtual ~SerialLogControl();

	void		setVersion (int newVersion);
	void		validate(SerialLogWindow *window, SerialLogBlock *block);
	uint64		getBlockNumber();
	int			getOffset();
	SerialLogTransaction* getTransaction(TransId transactionId);
	const UCHAR* getData(int length);
	SerialLogRecord* nextRecord();
	bool		atEnd();
	UCHAR		getByte();
	int			getInt();
	void		fini(void);
	void		setWindow (SerialLogWindow *window, SerialLogBlock *block, int offset);
	void		printBlock(SerialLogBlock *block);
	void		haveCheckpoint(int64 blockNumber);
	bool		isPostFlush(void);

	uint64			lastCheckpoint;
	SerialLog		*log;
	SerialLogWindow	*inputWindow;
	SerialLogBlock	*inputBlock;
	const UCHAR		*input;
	const UCHAR		*inputEnd;
	const UCHAR		*recordStart;
	int				version;
	bool			debug;
	bool			singleBlock;
	SerialLogRecord	*records[srlMax];
	
	SRLCommit			commit;
	SRLRollback			rollback;
	SRLPrepare			prepare;
	SRLData				dataUpdate;
	SRLIndexUpdate		indexUpdate;
	SRLWordUpdate		wordUpdate;
	SRLSwitchLog		switchLog;
	SRLRecordStub		recordStub;
	SRLSequence			sequence;
	SRLCheckpoint		checkpoint;
	SRLBlobUpdate		largeBlob;
	SRLDelete			deleteData;
	SRLDropTable		dropTable;
	SRLCreateSection	createSection;
	SRLSectionPage		sectionPage;
	SRLFreePage			freePage;
	SRLRecordLocator	recordLocator;
	SRLDataPage			dataPage;
	SRLIndexAdd			indexAdd;
	SRLIndexDelete		indexDelete;
	SRLIndexPage		indexPage;
	SRLInversionPage	inversionPage;
	SRLCreateIndex		createIndex;
	SRLDeleteIndex		deleteIndex;
	SRLVersion			logVersion;
	SRLUpdateRecords	updateRecords;
	SRLUpdateIndex		updateIndex;
	SRLSectionPromotion	sectionPromotion;
	SRLSequencePage		sequencePage;
	SRLSectionLine		sectionLine;
	SRLOverflowPages	overflowPages;
	SRLCreateTableSpace	createTableSpace;
	SRLDropTableSpace	dropTableSpace;
	SRLBlobDelete		blobDelete;
	SRLUpdateBlob		smallBlob;
	SRLSession			session;
};

#endif // !defined(AFX_SERIALLOGCONTROL_H__77229761_E146_4AE4_8BBC_2114F6A0FC93__INCLUDED_)
