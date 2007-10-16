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

// SerialLogRecord.h: interface for the SerialLogRecord class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERIALLOGRECORD_H__CD68DD89_7B64_4E00_B668_45D86A59A34F__INCLUDED_)
#define AFX_SERIALLOGRECORD_H__CD68DD89_7B64_4E00_B668_45D86A59A34F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLog.h"
#include "Sync.h"

#define START_RECORD(id,where)\
	Sync sync(&log->syncWrite, where);\
	sync.lock(Exclusive);\
	startRecord();\
	putInt(id);

static const int srlEnd				= 0;
static const int srlSwitchLog		= 1;
static const int srlCommit			= 2;
static const int srlPrepare			= 3;
static const int srlDataUpdate		= 4;
static const int srlIndexUpdate		= 5;
static const int srlWordUpdate		= 6;
static const int srlRecordStub		= 7;
static const int srlCheckpoint		= 8;
static const int srlSequence		= 9;
static const int srlBlobUpdate		= 10;
static const int srlRollback		= 11;
static const int srlDelete			= 12;
static const int srlDropTable		= 13;
static const int srlCreateSection	= 14;
static const int srlSectionPage		= 15;
static const int srlFreePage		= 16;
static const int srlSectionIndex	= 17;
static const int srlDataPage		= 18;
static const int srlIndexAdd		= 19;
static const int srlIndexDelete		= 20;
static const int srlIndexPage		= 21;
static const int srlInversionPage	= 22;
static const int srlCreateIndex		= 23;
static const int srlDeleteIndex		= 24;
static const int srlVersion			= 25;
static const int srlUpdateRecords	= 26;
static const int srlUpdateIndex		= 27;
static const int srlSectionPromotion= 28;
static const int srlSequencePage	= 29;
static const int srlSectionLine		= 30;
static const int srlOverflowPages	= 31;
static const int srlCreateTableSpace= 32;
static const int srlDropTableSpace	= 33;
static const int srlBlobDelete		= 34;
static const int srlUpdateBlob		= 35;
static const int srlMax				= 36;


class SerialLog;

class SerialLogRecord
{
public:
	virtual void pass2();
	SerialLogRecord();
	virtual ~SerialLogRecord();

	virtual	void	read() = 0;
	virtual void	rollback();
	virtual void	commit();
	virtual void	pass1();
	virtual void	redo();
	virtual void	print();
	virtual void	recoverLimbo(void);
	
	void			logPrint(const char* text, ...);
	int				byteCount(int value);
	int				getInt();
	int64			getInt64();
	int				getInt(const UCHAR** ptr);
	const UCHAR*	getData(int32 length);
	void			putStream (Stream *stream);
	void			wakeup();
	void			startRecord();
	void			putData(uint32 length, const UCHAR *data);
	void			putInt(int32 number);
	void			putInt64(int64 number);
	UCHAR*			putFixedInt(int value);
	void			putFixedInt(int value, UCHAR* ptr);
	char*			format(int length, const UCHAR* data, int tempLength, char *temp);
	SerialLogTransaction* getTransaction(TransId transactionId);

	SerialLog			*log;
	SerialLogControl	*control;
	TransId				transactionId;
};


#endif // !defined(AFX_SERIALLOGRECORD_H__CD68DD89_7B64_4E00_B668_45D86A59A34F__INCLUDED_)
