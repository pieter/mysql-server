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

// Table.h: interface for the Table class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TABLE_H__02AD6A42_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
#define AFX_TABLE_H__02AD6A42_A433_11D2_AB5B_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "PrivilegeObject.h"
#include "LinkedList.h"
#include "SyncObject.h"
#include "Types.h"
#include "Index.h"

static const int PreInsert	= 1;
static const int PostInsert = 2;
static const int PreUpdate	= 4;
static const int PostUpdate = 8;
static const int PreDelete	= 16;
static const int PostDelete = 32;
static const int PreCommit	= 64;
static const int PostCommit	= 128;

static const int checkUniqueWaited	= 0;
static const int checkUniqueIsDone	= 1;
static const int checkUniqueNext	= 2;

#define FORMAT_HASH_SIZE		20
#define FOR_FIELDS(field,table)	{for (Field *field=table->fields; field; field = field->next){
#define FOR_INDEXES(index,table)	{for (Index *index=table->indexes; index; index = index->next){

class Database;
class Dbb;
class Index;
class Transaction;
class Value;
CLASS(Field);
class Format;
class Record;
class RecordSection;
class RecordVersion;
class ForeignKey;
class TableAttachment;
class View;
class Trigger;
class InversionFilter;
class Bitmap;
class Collation;
class Repository;
class BlobReference;
class AsciiBlob;
class BinaryBlob;
class Section;
class TableSpace;
class RecordScavenge;

class Table : public PrivilegeObject
{
public:
	Table(Database *db, const char * schema, const char * tableName, int id, int version, uint64 numberRecords, TableSpace *tblSpace);
	Table(Database *db, int tableId, const char *schema, const char *name, TableSpace *tableSpace);
	virtual ~Table();

	void		expunge(Transaction *transaction);
	JString		getPrimaryKeyName(void);
	void		getBlob(int recordNumber, Stream *stream);
	int			storeBlob(Transaction *transaction, uint32 length, const UCHAR *data);
	void		rename(const char *newSchema, const char *newName);
	void		getIndirectBlob (int recordId, BlobReference *blob);
	BinaryBlob* getBinaryBlob (int recordId);
	AsciiBlob*	getAsciiBlob (int recordId);
	int32		getIndirectId (BlobReference *reference, Transaction *transaction);
	void		refreshFields();
	void		insertView(Transaction *transaction, int count, Field **fieldVector, Value **values);
	void		bind (Table *table);
	void		clearIndexesRebuild();
	void		rebuildIndexes (Transaction *transaction, bool force = false);
	void		collationChanged (Field *field);
	void		validateBlobs (int optionMask);
	void		cleanupRecords(RecordScavenge *recordScavenge);
	void		rebuildIndex (Index *index, Transaction *transaction);
	int			retireRecords (RecordScavenge *recordScavenge);
	int			countActiveRecords();
	bool		foreignKeyMember (ForeignKey *key);
	void		makeNotSearchable (Field *field, Transaction *transaction);
	bool		dropForeignKey (int fieldCount, Field **fields, Table *references);
	bool		checkUniqueIndexes (Transaction *transaction, RecordVersion *record, Sync *sync);
	bool		checkUniqueIndex(Index *index, Transaction *transaction, RecordVersion *record, Sync *sync);
	int			checkUniqueRecordVersion(int32 recordNumber, Index *index, Transaction *transaction, RecordVersion *record, Sync *sync);
	bool		isDuplicate (Index *index, Record *record1, Record *record2);
	void		checkDrop();
	Field*		findField (const WCString *fieldName);
	void		setType (const char *typeName);
	InversionFilter* getFilters (Field *field, Record *records, Record *limit);
	void		garbageCollectInversion (Field *field, Record * leaving, Record * staying, Transaction *transaction);
	void		postCommit (Transaction *transaction, RecordVersion *record);
	void		buildFieldVector();
	int			nextPrimaryKeyColumn (int previous);
	int			nextColumnId (int previous);
	void		loadStuff();
	void		clearAlter(void);
	bool		setAlter(void);
	
	void		addTrigger (Trigger *trigger);
	void		dropTrigger (Trigger *trigger);
	Trigger*	findTrigger (const char *name);
	void		fireTriggers (Transaction *transaction, int operation, Record *before, RecordVersion *after);

#ifndef STORAGE_ENGINE
	void zapLinkages();
#endif

	void		addIndex (Index *index);
	void		dropIndex (Index *index);
	void		reIndex (Transaction *transaction);
	void		loadIndexes();

	void		garbageCollect (Record *leaving, Record *staying, Transaction *transaction, bool quiet);
	void		expungeBlob (Value *blob);
	bool		duplicateBlob (Value *blob, int fieldId, Record *recordChain);
	void		expungeRecordVersions (RecordVersion *record, RecordScavenge *recordScavenge);
	void		setView (View *view);
	Index*		findIndex (const char *indexName);
	virtual		PrivObject getPrivilegeType();
	void		populateIndex (Index *index, Transaction *transaction);
	const char* getSchema();
	ForeignKey* dropForeignKey (ForeignKey *key);
	void		dropField (Field *field);
	void		addAttachment (TableAttachment *attachment);
	void		addField (Field *field);
	void		checkNullable (Record *record);
	virtual void	drop(Transaction *transaction);
	virtual void	truncate(Transaction *transaction);
	void		updateInversion (Record *record, Transaction *transaction);
	int			getFieldId (const char *name);
	ForeignKey* findForeignKey (ForeignKey *key);
	bool		indexExists (ForeignKey *foreignKey);
	ForeignKey* findForeignKey (Field *field, bool foreign);
	Field*		findField (int id);
	void		addForeignKey (ForeignKey *key);
	Index*		getPrimaryKey();
	bool		isCreated();
	void		reIndexInversion(Transaction *transaction);
	void		makeSearchable (Field *field, Transaction *transaction);
	int32		getBlobId (Value *value, int32 oldId, bool cloneFlag, Transaction *transaction);
	void		addFormat (Format *format);
	Record*		rollbackRecord (RecordVersion *recordVersion);
	Record*		fetch (int32 recordNumber);
	void		init(int id, const char *schema, const char *tableName, TableSpace *tblSpace);
	void		loadFields();
	void		setBlobSection (int32 section);
	void		setDataSection (int32 section);
	void		deleteIndex (Index *index, Transaction *transaction);
	Record*		databaseFetch (int32 recordNumber);
	Record*		fetchNext (int32 recordNumber);
	int			numberFields();
	void		updateRecord (RecordVersion *record);
	void		reformat();
	Format*		getFormat (int version);
	void		save();
	void		create (const char *tableType, Transaction *transaction);
	const char* getName();
	Index*		addIndex (const char *name, int numberFields, int type);
	Field*		addField (const char *name, Type type, int length, int precision, int scale, int flags);
	Field*		findField (const char *name);
	int			getFormatVersion();
	void		validateAndInsert(Transaction *transaction, RecordVersion *record);
	bool		hasUncommittedRecords(Transaction* transaction);
	void		checkAncestor(Record* current, Record* oldRecord);
	int64		estimateCardinality(void);
	void		optimize(Connection *connection);
	void		findSections(void);
	bool		validateUpdate(int32 recordNumber, TransId transactionId);
	
	RecordVersion*	allocRecordVersion(Format* format, Transaction* transaction, Record* priorVersion);
	Record*			allocRecord(int recordNumber, Stream* stream);
	void			inventoryRecords(RecordScavenge* recordScavenge);
	Format*			getCurrentFormat(void);
	Record*			fetchForUpdate(Transaction* transaction, Record* record, bool usingIndex);
	RecordVersion*	lockRecord(Record* record, Transaction* transaction);
	void			unlockRecord(int recordNumber);
	void			unlockRecord(RecordVersion* record, bool remove);

	void		insert (Transaction *transaction, int count, Field **fields, Value **values);
	uint		insert (Transaction *transaction, Stream *stream);
	bool		insert (Record *record, Record *prior, int recordNumber);
	
	void		update (Transaction *transaction, Record *record, int numberFields, Field **fields, Value** values);
	void		update(Transaction * transaction, Record *oldRecord, Stream *stream);
	
	void		deleteRecord (Transaction *transaction, Record *record);
	void		deleteRecord (int recordNumber);
	void		deleteRecord (RecordVersion *record);

	Dbb				*dbb;
	SyncObject		syncObject;
	SyncObject		syncTriggers;
	SyncObject		syncScavenge;
	SyncObject		syncUpdate;
	SyncObject		syncAlter;				// prevent concurrent Alter statements.
	Table			*collision;				// Hash collision in database
	Table			*idCollision;			// mod(id) collision in database
	Table			*next;					// next in database linked list
	Field			*fields;
	Field			**fieldVector;
	Index			*indexes;
	ForeignKey		*foreignKeys;
	LinkedList		attachments;
	Format			**formats;
	Format			*format;
	RecordSection	*records;
	Index			*primaryKey;
	View			*view;
	Trigger			*triggers;
	Bitmap			*recordBitmap;
	Bitmap			*emptySections;
	Section			*dataSection;
	Section			*blobSection;
	TableSpace		*tableSpace;
	uint64			cardinality;
	uint64			priorCardinality;
	int				tableId;
	int				dataSectionId;
	int				blobSectionId;
	int				nextFieldId;
	int				formatVersion;
	int				fieldCount;
	int				maxFieldId;
	bool			changed;
	bool			eof;
	bool			markedForDelete;
	bool			activeVersions;
	bool			alterIsActive;
	int32			highWater;
	int32			ageGroup;
	uint32			debugThawedRecords;
	uint64			debugThawedBytes;

protected:
	const char		*type;
};

#endif // !defined(AFX_TABLE_H__02AD6A42_A433_11D2_AB5B_0000C01D2301__INCLUDED_)
