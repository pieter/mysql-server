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

// Record.h: interface for the Record class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RECORD_H__02AD6A50_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_RECORD_H__02AD6A50_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define ALLOCATE_RECORD(n)		(char*) MemMgrRecordAllocate (n, __FILE__, __LINE__)
#define DELETE_RECORD(record)	MemMgrRecordDelete (record);

#define CHECK_RECORD_ACTIVITY

enum RecordEncoding {
	noEncoding = 0,
	traditional,
	valueVector,
	byteVector,
	shortVector,
	longVector
	};

// Record states

static const int recData	= 0;		// record pointer is valid or record is deleted
static const int recDeleted	= 1;		// record has been deleted
static const int recChilled	= 2;		// record data is temporarily stored in serial log
static const int recOnDisk	= 3;		// record is on disk and must be read
static const int recLock	= 4;		// this is a "record lock" and not a record
static const int recNoChill = 5;		// record is in use and should not be chilled
static const int recRollback = 6;		// record is being rolled back
static const int recUnlocked = 7;		// record is being unlocked
static const int recDeleting = 8;		// record is being physically deleted
static const int recPruning	 = 9;		// record is being pruned
static const int recEndChain = 10;		// end of chain for garbage collection

class Format;
class Table;
class Transaction;
class Value;
class Stream;
class Database;
class RecordScavenge;
CLASS(Field);

extern char	*RecordAllocate (int size, const char *file, int line);
extern void	RecordDelete (char *record);

class Record
{
public:
	virtual Transaction* getTransaction();
	virtual TransId	getTransactionId();
	virtual int		getSavePointId();
	virtual void	setSuperceded (bool flag);
	virtual Record* fetchVersion (Transaction *transaction);
	virtual bool	scavenge(RecordScavenge *recordScavenge);
	virtual void	scavenge(TransId targetTransactionId, int oldestActiveSavePointId);
	virtual bool	isVersion();
	virtual bool	isSuperceded();
	virtual bool	isNull(int fieldId);
	virtual Record* releaseNonRecursive(void);
	virtual void	setPriorVersion(Record* record);
	virtual Record* getPriorVersion();
	virtual Record* getGCPriorVersion(void);
	virtual	void	print(void);
	virtual int		thaw();
	virtual const char*	getEncodedRecord();
	virtual int		setRecordData(const UCHAR *dataIn, int dataLength);
	
	const UCHAR*	getEncoding (int index);
	int				setEncodedRecord(Stream *stream, bool interlocked);
	void			getValue (int fieldId, Value* value);
	void			getRawValue (int fieldId, Value* value);
	int				getFormatVersion();
	void			setValue (Transaction *transaction, int id, Value *value, bool cloneFlag, bool copyFlag);
	void			poke ();
	void			release();
	void			addRef();
	int				getBlobId(int fieldId);
	void			finalize(Transaction *transaction);
	void			getEncodedValue (int fieldId, Value *value);
	void			getRecord (Stream *stream);
	int				getEncodedSize();
	void			deleteData(void);
	void			printRecord(const char* header);
	void			validateData(void);
	char*			allocRecordData(int length);
	
	Record (Table *table, Format *recordFormat);
	Record(Table *table, int32 recordNumber, Stream *stream);

	inline int		hasRecord()
		{
		return data.record != NULL;
		};

	inline char* getRecordData()
	{
		if (state == recChilled)
			thaw();
		
		return data.record;
	}
		
protected:
	virtual ~Record();
	
	struct
		{
		char	*record;
		}		data;

public:
	volatile INTERLOCK_TYPE useCount;
	//Table		*table;
	Format		*format;
	int			recordNumber;
	int			size;
	int			generation;
	short		highWater;
	UCHAR		encoding;
	UCHAR		state;

#ifdef CHECK_RECORD_ACTIVITY
	UCHAR		active;					// this is for debugging only
#endif
};

#endif // !defined(AFX_RECORD_H__02AD6A50_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
