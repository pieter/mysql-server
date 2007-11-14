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

#ifndef _STORAGE_TABLE_SHARE_H_
#define _STORAGE_TABLE_SHARE_H_

#include "JString.h"

#ifndef _WIN32
#define __int64			long long
#endif

typedef __int64			INT64;

static const int MaxIndexSegments	= 16;

class StorageDatabase;
class StorageConnection;
class StorageHandler;
class Table;
class Index;
class SyncObject;
class Sequence;
class SyncObject;

struct StorageSegment {
	short			type;
	short			nullPosition;
	int				offset;
	int				length;
	unsigned char	nullBit;
	char			isUnsigned;
	void			*mysql_charset;
	};

struct StorageIndexDesc {
	int			unique;
	int			primaryKey;
	int			numberSegments;
	const char	*name;
	Index		*index;
	uint64		*segmentRecordCounts;
	StorageSegment segments[MaxIndexSegments];
	};


enum StorageError {
	StorageErrorRecordNotFound		= -1,
	StorageErrorDupKey				= -2,
	StorageErrorTableNotFound		= -3,
	StorageErrorNoIndex				= -4,
	StorageErrorBadKey				= -5,
	StorageErrorTableExits			= -6,
	StorageErrorNoSequence			= -7,
	StorageErrorUpdateConflict		= -8,
	StorageErrorUncommittedUpdates	= -9,		// specific for drop table
	StorageErrorDeadlock			= -10,
	StorageErrorTruncation			= -11,
	StorageErrorUncommittedRecords	= -12,		// more general; used for alter table
	StorageErrorIndexOverflow		= -13,		// key value is too long
	StorageWarningSerializable		= -101,
	StorageWarningReadUncommitted	= -102,
	StorageErrorTablesSpaceOperationFailed = -103,
	StorageErrorOutOfMemory			= -104,		// memory pool limit reached or system memory exhausted
	StorageErrorOutOfRecordMemory	= -105,		// memory pool limit reached or system memory exhausted
	StorageErrorLockTimeout			= -106
	StorageErrorTableSpaceExist = -107
	};
	
static const int StoreErrorIndexShift	= 10;
static const int StoreErrorIndexMask	= 0x3ff;

class StorageTableShare
{
public:
	//StorageTableShare(StorageDatabase *db, const char *tableName, const char *schemaName, int impureSize);
	StorageTableShare(StorageHandler *handler, const char * path, const char *tableSpaceName, int lockSize, bool tempTbl);
	virtual ~StorageTableShare(void);
	
	virtual void		lock(bool exclusiveLock);
	virtual void		unlock(void);
	virtual int			createIndex(StorageConnection *storageConnection, const char* name, const char* sql);
	virtual int			renameTable(StorageConnection *storageConnection, const char* newName);
	virtual INT64		getSequenceValue(int delta);
	virtual int			setSequenceValue(INT64 value);
	virtual int			haveIndexes(void);
	virtual void		cleanupFieldName(const char* name, char* buffer, int bufferLength);
	virtual void		setTablePath(const char* path, bool tempTable);
	virtual void		registerCollation(const char* collationName, void* arg);

	int					open(void);
	StorageIndexDesc*	getIndex(int indexCount, int indexId, StorageIndexDesc* indexDesc);
	StorageIndexDesc*	getIndex(int indexId);

	int					getIndexId(const char* schemaName, const char* indexName);
	int					create(StorageConnection *storageConnection, const char* sql, int64 autoIncrementValue);
	int					deleteTable(StorageConnection *storageConnection);
	void				load(void);
	void				registerTable(void);
	void				unRegisterTable(void);
	void				setPath(const char* path);
	void				getDefaultPath(char *buffer);
	void				findDatabase(void);
	void				setDatabase(StorageDatabase* db);
	uint64				estimateCardinality(void);
	bool				tableExists(void);
	JString				lookupPathName(void);

	static const char*	getDefaultRoot(void);
	static const char*	cleanupTableName(const char* name, char* buffer, int bufferLength, char *schema, int schemaLength);
	
	JString				name;
	JString				schemaName;
	JString				pathName;
	JString				tableSpace;
	JString				givenName;
	JString				givenSchema;
	StorageTableShare	*collision;
	unsigned char		*impure;
	int					initialized;
	SyncObject			*syncObject;
	StorageDatabase		*storageDatabase;
	StorageHandler		*storageHandler;
	Table				*table;
	StorageIndexDesc	**indexes;
	Sequence			*sequence;
	int					numberIndexes;
	bool				tempTable;
};

#endif

