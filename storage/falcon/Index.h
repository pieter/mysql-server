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

// Index.h: interface for the Index class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INDEX_H__02AD6A44_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_INDEX_H__02AD6A44_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Types.h"
#include "Queue.h"

static const int INDEX_VERSION_0		= 0;
static const int INDEX_VERSION_1		= 1;
static const int INDEX_CURRENT_VERSION	= INDEX_VERSION_1;

//#define CHECK_DEFERRED_INDEXES

#define INDEX_VERSION_FACTOR	10000000
#define INDEX_ID(combined)		(combined % INDEX_VERSION_FACTOR)
#define INDEX_VERSION(combined)	(combined / INDEX_VERSION_FACTOR)
#define INDEX_COMPOSITE(id,vers)(vers * INDEX_VERSION_FACTOR + id)
//* These kind of commented lines implement multiple DI hase sizes.
//*	#define MAX_DI_HASH_TABLES 5

static const int PrimaryKey			= 0;
static const int UniqueIndex		= 1;
static const int SecondaryIndex		= 2;
static const int ForeignKeyIndex	= 3;
static const int IndexTypeMask		= 0x7;
#define INDEX_IS_UNIQUE(t) ((t == PrimaryKey) || (t == UniqueIndex))

static const int StorageEngineIndex	= 0x10;

// search flags

static const int Partial		= 1;
static const int AfterLowKey	= 2;
static const int BeforeHighKey	= 4;

class Table;
CLASS(Field);
class Record;
class Value;
class Bitmap;
class Database;
class Transaction;
class IndexKey;
class DeferredIndex;
class Dbb;
class Connection;

struct IndexSegment {
	short	type;
	short	nullPosition;
	int		offset;
	int		length;
	};

class Index
{
public:
	const char* getSchemaName();
	const char* getIndexName();
	void		init(Table *tbl, const char *indexName, int indexType, int count);
	bool		isMember (Field *field);
	void		damageCheck();
	void		setDamaged();
	void		rebuildIndex(Transaction *transaction);
	bool		changed (Record *record1, Record *record2);
	bool		duplicateKey (IndexKey *key, Record *record);
	void		garbageCollect (Record *leaving, Record *staying, Transaction *transaction, bool quiet);
	void		update (Record *oldRecord, Record *newRecord, Transaction *transaction);
	void		loadFields();
	void		setIndex (int32 id);
	Bitmap*		scanIndex (IndexKey* lowKey, IndexKey* highKey, int searchFlags, Transaction *transaction, Bitmap *bitmap);
	void		deleteIndex(Transaction *transaction);
	void		insert (Record *record, Transaction *transaction);
	void		insert(IndexKey* key, int32 recordNumber, Transaction *transaction);
	DeferredIndex *getDeferredIndex(Transaction *transaction);
	void		create(Transaction *transaction);
	void		save();
	int			matchField (Field *field);
	void		addField (Field *field, int position);
	void		rename(const char* newName);
	int			getPartialLength(int segment);
	void		setPartialLength(int segment, uint partialLength);
	UCHAR		getPadByte(int index);
	void		checkMaxKeyLength(void);

	void		makeKey (Record *record, IndexKey *key);
	void		makeKey (int count, Value **values, IndexKey *key);
	void		makeKey (Field *field, Value *value, int segment, IndexKey *key);

	void		detachDeferredIndex(DeferredIndex *deferredIndex);
	UCHAR		getPadByte(void);
	int			getRootPage(void);
	void		optimize(uint64 cardinality, Connection *connection);

	static JString	getTableName (Database *database, const char *schema, const char *indexName);
	static void		deleteIndex (Database *database, const char *schema, const char *indexName);

//*	uint32		hash(UCHAR *buf, int len, uint hashSize);
	uint32		hash(UCHAR *buf, int len);
	void		addToDIHash(struct DIUniqueNode *uniqueNode);
	void		removeFromDIHash(struct DIUniqueNode *uniqueNode);
	void		scanDIHash(IndexKey* scanKey, int searchFlags, Bitmap *bitmap);

	Index(Table *tbl, const char *indexName, int count, int typ);
	Index(Table *tbl, const char *indexName, int indexType, int id, int numberFields);
	virtual ~Index();

	Table		*table;
	Database	*database;
	Dbb			*dbb;
	Field		**fields;
	Index		*next;				// next in table
	Queue<DeferredIndex>	deferredIndexes;
	JString		name;
	int32		rootPage;
	int			numberFields;
	int			indexId;
	int			indexVersion;
	int			type;
	int			*partialLengths;
	uint64		*recordsPerSegment;
	bool		savePending;
	bool		damaged;
	bool		rebuild;
//*	uint		curHashTable;
//*	DIUniqueNode **DIHashTables[MAX_DI_HASH_TABLES];
//*	int			DIHashTableCounts[MAX_DI_HASH_TABLES];
//*	int			DIHashTableSlotsUsed[MAX_DI_HASH_TABLES];
	DIUniqueNode **DIHashTable;
	int			DIHashTableCounts;
	int			DIHashTableSlotsUsed;
	SyncObject	syncDIHash;
};

#endif // !defined(AFX_INDEX_H__02AD6A44_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
