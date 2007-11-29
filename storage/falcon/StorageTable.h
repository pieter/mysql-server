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

#ifndef _STORAGE_TABLE_H_
#define _STORAGE_TABLE_H_

#include "EncodedDataStream.h"
#include "Stream.h"
#include "IndexKey.h"
#include "SQLException.h"

static const int UpperBound	= 1;
static const int LowerBound = 2;

static const int MaxRetryAferWait = 5;

struct StorageKey {
	int			numberSegments;
	IndexKey	indexKey;
	};

struct StorageBlob {
	unsigned int	length;
	unsigned char	*data;
	int				blobId;
	StorageBlob		*next;
	};

	
class StorageConnection;
class StorageTableShare;
class StorageInterface;
class StorageDatabase;
class Index;
class Record;
class SyncObject;

struct StorageIndexDesc;

class StorageTable
{
public:
	StorageTable(StorageConnection *connection, StorageTableShare *tableShare);
	virtual ~StorageTable(void);

	void			transactionEnded(void);
	void			setRecord(Record* record, bool locked);
	int				alterCheck(void);
	void			clearAlter(void);
	bool			setAlter(void);
	
	void			clearTruncateLock(void);
	void			setTruncateLock();
	
	virtual void	setConnection(StorageConnection* connection);
	virtual void	clearIndexBounds(void);
	virtual void	clearRecord(void);
	virtual void	clearBitmap(void);
	virtual int		create(const char *sql, int64 autoIncrementValue);
	virtual int		open(void);
	virtual int		deleteTable(void);
	virtual int		deleteRow(int recordNumber);
	virtual int		truncateTable(void);
	virtual int		setIndex(int numberIndexes, int indexId, StorageIndexDesc* storageIndex);
	virtual int		indexScan();
	virtual int		setIndex(int indexId);
	virtual void	indexEnd(void);
	virtual int		setIndexBound(const unsigned char* key, int keyLength, int which);
	virtual int		storeBlob(StorageBlob* blob);
	virtual void	getBlob(int recordNumber, StorageBlob* blob);
	virtual void	release(StorageBlob* blob);
	virtual void	deleteStorageTable(void);
	virtual void	freeBlob(StorageBlob* blob);
	virtual void	preInsert(void);
	virtual int		insert(void);
	
	virtual int		next(int recordNumber, bool lockForUpdate);
	virtual int		nextIndexed(int recordNumber, bool lockForUpdate);
	virtual int		fetch(int recordNumber, bool lockForUpdate);
	
	virtual int		updateRow(int recordNumber);
	virtual const unsigned char* getEncoding(int fieldIndex);
	virtual const char*			 getName(void);
	virtual const char*			 getSchemaName(void);
	virtual int		compareKey(const unsigned char* key, int keyLength);
	virtual int		translateError(SQLException *exception, int defaultStorageError);
	virtual int		isKeyNull(const unsigned char* key, int keyLength);
	virtual void	setPartialKey(void);
	virtual void	setReadBeforeKey(void);
	virtual void	setReadAfterKey(void);
	virtual void	unlockRow(void);
	virtual int		optimize(void);
	virtual void	setLocalTable(StorageInterface* handler);

	SyncObject			*syncTruncate;
	JString				name;
	StorageTable		*collision;
	StorageConnection	*storageConnection;
	StorageDatabase		*storageDatabase;
	StorageTableShare	*share;
	StorageInterface	*localTable;
	StorageIndexDesc	*currentIndex;
	void				*bitmap;
	StorageKey			lowerKey;
	StorageKey			upperKey;
	StorageKey			*lowerBound;
	StorageKey			*upperBound;
	Record				*record;
	EncodedDataStream	dataStream;
	Stream				insertStream;
	int					searchFlags;
	bool				recordLocked;
	bool				haveTruncateLock;
};

#endif
