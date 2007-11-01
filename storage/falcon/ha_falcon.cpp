/* Copyright (C) 2006, 2007 MySQL AB

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

/* XXX correct? */

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "mysql_priv.h"

#ifdef _WIN32
#pragma pack()
#endif

#include "ha_falcon.h"
#include "StorageConnection.h"
#include "StorageTable.h"
#include "StorageTableShare.h"
#include "StorageHandler.h"
#include "CmdGen.h"
#include "InfoTable.h"

#ifdef _WIN32
#define I64FORMAT			"%I64d"
#else
#define I64FORMAT			"%lld"
#endif

#include "ScaledBinary.h"
#include "BigInt.h"

//#define NO_OPTIMIZE

#ifndef MIN
#define MIN(a,b)			((a <= b) ? (a) : (b))
#define MAX(a,b)			((a >= b) ? (a) : (b))
#endif

static const uint LOAD_AUTOCOMMIT_RECORDS = 10000;
static const char falcon_hton_name[] = "Falcon";

static const char *falcon_extensions[] = {
	".fts",
	".fl1",
	".fl2",
	NullS
};

static StorageHandler	*storageHandler;

#define PARAMETER(name, text, min, deflt, max, flags, function) uint falcon_##name;
#include "StorageParameters.h"
#undef PARAMETER

unsigned long long		falcon_record_memory_max;
uint					falcon_record_scavenge_threshold;
uint					falcon_record_scavenge_floor;
unsigned long long		falcon_initial_allocation;
uint					falcon_allocation_extent;
my_bool					falcon_disable_fsync;
unsigned long long		falcon_page_cache_size;
uint					falcon_page_size;
uint					falcon_serial_log_buffers;
char*					falcon_serial_log_dir;
char*					falcon_checkpoint_schedule;
char*					falcon_scavenge_schedule;
//uint					falcon_debug_mask;
//uint					falcon_debug_trace;
my_bool					falcon_debug_server;
FILE					*falcon_log_file;
uint					falcon_index_chill_threshold;
uint					falcon_record_chill_threshold;
uint					falcon_max_transaction_backlog;


static struct st_mysql_show_var falconStatus[]=
{
  //{"static",     (char*)"just a static text",     SHOW_CHAR},
  //{"called",     (char*)&number_of_calls, SHOW_LONG},
  {0,0,SHOW_UNDEF}
};

extern THD*		current_thd;

static handler *falcon_create_handler(handlerton *hton,
                                      TABLE_SHARE *table, MEM_ROOT *mem_root)
{
	return new (mem_root) StorageInterface(hton, table);
}

handlerton *falcon_hton;

void openFalconLogFile(const char *file)
{
	if (falcon_log_file)
		fclose(falcon_log_file);
	falcon_log_file = fopen(file, "a");
}

void closeFalconLogFile()
{
	if (falcon_log_file)
		{
		fclose(falcon_log_file);
		falcon_log_file = NULL;
		}
}

void flushFalconLogFile()
{
	if (falcon_log_file)
		fflush(falcon_log_file);
}

int StorageInterface::falcon_init(void *p)
{
	DBUG_ENTER("falcon_init");
	falcon_hton = (handlerton *)p;

	if (!storageHandler)
		storageHandler = getFalconStorageHandler(sizeof(THR_LOCK));

	falcon_hton->state = SHOW_OPTION_YES;
	falcon_hton->db_type = DB_TYPE_FALCON;
	falcon_hton->savepoint_offset = sizeof(void*);
	falcon_hton->close_connection = StorageInterface::closeConnection;
	falcon_hton->savepoint_set = StorageInterface::savepointSet;
	falcon_hton->savepoint_rollback = StorageInterface::savepointRollback;
	falcon_hton->savepoint_release = StorageInterface::savepointRelease;
	falcon_hton->commit = StorageInterface::commit;
	falcon_hton->rollback = StorageInterface::rollback;
	falcon_hton->create = falcon_create_handler;
	falcon_hton->drop_database  = StorageInterface::dropDatabase;
	falcon_hton->panic  = StorageInterface::panic;
#if 0
	falcon_hton->alter_table_flags  = StorageInterface::alter_table_flags;
#endif

#ifdef XA_ENABLED
	falcon_hton->prepare = StorageInterface::prepare;
#endif

	falcon_hton->commit_by_xid = StorageInterface::commit_by_xid;
	falcon_hton->rollback_by_xid = StorageInterface::rollback_by_xid;

	falcon_hton->alter_tablespace = StorageInterface::alter_tablespace;
	//falcon_hton->show_status  = StorageInterface::show_status;
	falcon_hton->flags = HTON_NO_FLAGS;

	storageHandler->addNfsLogger(-1, StorageInterface::logger, NULL);

	if (falcon_debug_server)
		storageHandler->startNfsServer();

	//TimeTest timeTest;
	//timeTest.testScaled(16, 2, 100000);

	//pluginHandler = new NfsPluginHandler;

	DBUG_RETURN(0);
}


int StorageInterface::falcon_deinit(void *p)
{
	storageHandler->shutdownHandler();

	return 0;
}

int falcon_strnxfrm (void *cs,
                     const char *dst, uint dstlen,
                     const char *src, uint srclen)
{
	CHARSET_INFO *charset = (CHARSET_INFO*) cs;

	return charset->coll->strnxfrm(charset, (uchar *) dst, dstlen, MAX_INDEX_KEY_LENGTH,
	                              (uchar *) src, srclen, 0);
}

char falcon_get_pad_char (void *cs)
{
	return (char) ((CHARSET_INFO*) cs)->pad_char;
}

int falcon_cs_is_binary (void *cs)
{
	return (0 == strcmp(((CHARSET_INFO*) cs)->name, "binary"));
//	return ((((CHARSET_INFO*) cs)->state & MY_CS_BINSORT) == MY_CS_BINSORT);
}

unsigned int falcon_get_mbmaxlen (void *cs)
{
	return ((CHARSET_INFO*) cs)->mbmaxlen;
}

char falcon_get_min_sort_char (void *cs)
{
	return (char) ((CHARSET_INFO*) cs)->min_sort_char;
}

// Return the actual number of characters in the string
// Note, this is not the number of characters with collatable weight.

uint falcon_strnchrlen(void *cs, const char *s, uint l)
{
	CHARSET_INFO *charset = (CHARSET_INFO*) cs;

	if (charset->mbmaxlen == 1)
		return l;

	uint chrCount = 0;
	uchar *ch = (uchar *) s;
	uchar *end = ch + l;

	while (ch < end)
		{
		int len = charset->cset->mbcharlen(charset, *ch);
		if (len == 0)
			break;  // invalid character.

		ch += len;
		chrCount++;
		}

	return chrCount;
}

// Determine how many bytes are required to store the output of cs->coll->strnxfrm()
// cs is how the source string is formatted.
// srcLen is the number of bytes in the source string.
// partialKey is the max key buffer size if not zero.
// bufSize is the ultimate maximum destSize.
// If the string is multibyte, strnxfrmlen expects srcLen to be
// the maximum number of characters this can be.  Falcon wants to send
// a number that represents the actual number of characters in the string
// so that the call to cs->coll->strnxfrm() will not pad.

uint falcon_strnxfrmlen(void *cs, const char *s, uint srcLen,
						int partialKey, int bufSize)
{
	CHARSET_INFO *charset = (CHARSET_INFO*) cs;
	uint chrLen = falcon_strnchrlen(cs, s, srcLen);
	int maxChrLen = partialKey ? min(chrLen, partialKey / charset->mbmaxlen) : chrLen;

	return min(charset->coll->strnxfrmlen(charset, maxChrLen * charset->mbmaxlen), (uint) bufSize);
}

// Return the number of bytes used in s to hold a certain number of characters.
// This partialKey is a byte length with charset->mbmaxlen figured in.
// In other words, it is the number of characters times mbmaxlen.

uint falcon_strntrunc(void *cs, int partialKey, const char *s, uint l)
{
	CHARSET_INFO *charset = (CHARSET_INFO*) cs;

	if ((charset->mbmaxlen == 1) || (partialKey == 0))
		return min((uint) partialKey, l);

	int charLimit = partialKey / charset->mbmaxlen;
	uchar *ch = (uchar *) s;
	uchar *end = ch + l;

	while ((ch < end) && charLimit)
		{
		int len = charset->cset->mbcharlen(charset, *ch);
		if (len == 0)
			break;  // invalid character.

		ch += len;
		charLimit--;
		}

	return ch - (uchar *) s;
}

int falcon_strnncoll(void *cs, const char *s1, uint l1, const char *s2, uint l2, char flag)
{
	CHARSET_INFO *charset = (CHARSET_INFO*) cs;

	return charset->coll->strnncoll(charset, (uchar *) s1, l1, (uchar *) s2, l2, flag);
}

int falcon_strnncollsp(void *cs, const char *s1, uint l1, const char *s2, uint l2, char flag)
{
	CHARSET_INFO *charset = (CHARSET_INFO*) cs;

	return charset->coll->strnncollsp(charset, (uchar *) s1, l1, (uchar *) s2, l2, flag);
}

int (*strnncoll)(struct charset_info_st *, const uchar *, uint, const uchar *, uint, my_bool);

/***
#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif
***/

#ifndef DIG_PER_DEC1
typedef decimal_digit_t dec1;
typedef longlong      dec2;

#define DIG_PER_DEC1 9
#define DIG_MASK     100000000
#define DIG_BASE     1000000000
#define DIG_MAX      (DIG_BASE-1)
#define DIG_BASE2    ((dec2)DIG_BASE * (dec2)DIG_BASE)
#define ROUND_UP(X)  (((X)+DIG_PER_DEC1-1)/DIG_PER_DEC1)
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StorageInterface::StorageInterface(handlerton *hton, st_table_share *table_arg)
  : handler(hton, table_arg)
{
	ref_length = sizeof(lastRecord);
	stats.records = 1000;
	stats.data_file_length = 10000;
	tableLocked = false;
	lockForUpdate = false;
	storageTable = NULL;
	storageConnection = NULL;
	storageShare = NULL;
	share = table_arg;
	lastRecord = -1;
	mySqlThread = NULL;
	activeBlobs = NULL;
	freeBlobs = NULL;
	errorText = NULL;

	if (table_arg)
		{
		recordLength = table_arg->reclength;
		tempTable = false;
		}
}


StorageInterface::~StorageInterface(void)
{
	if (activeBlobs)
		freeActiveBlobs();

	for (StorageBlob *blob; (blob = freeBlobs); )
		{
		freeBlobs = blob->next;
		delete blob;
		}

	if (storageTable)
		{
		storageTable->deleteStorageTable();
		storageTable = NULL;
		}

	if (storageConnection)
		{
		storageConnection->release();
		storageConnection = NULL;
		}

	//delete [] dbName;
}

int StorageInterface::rnd_init(bool scan)
{
	DBUG_ENTER("StorageInterface::rnd_init");
	nextRecord = 0;
	lastRecord = -1;

	DBUG_RETURN(0);
}


int StorageInterface::open(const char *name, int mode, uint test_if_locked)
{
	DBUG_ENTER("StorageInterface::open");

	if (!mySqlThread)
		mySqlThread = current_thd;

	if (!storageTable)
		{
		storageShare = storageHandler->findTable(name);
		storageConnection = storageHandler->getStorageConnection(storageShare, mySqlThread, mySqlThread->thread_id, OpenDatabase);

		if (!storageConnection)
			DBUG_RETURN(HA_ERR_NO_CONNECTION);

		*((StorageConnection**) thd_ha_data(mySqlThread, falcon_hton)) = storageConnection;
		storageConnection->addRef();
		storageTable = storageConnection->getStorageTable(storageShare);

		if (!storageShare->initialized)
			{
			storageShare->lock(true);

			if (!storageShare->initialized)
				{
				thr_lock_init((THR_LOCK *)storageShare->impure);
				storageShare->setTablePath(name, tempTable);
				storageShare->initialized = true;
				}

			// Register any collations used

			uint fieldCount = (table) ? table->s->fields : 0;

			for (uint n = 0; n < fieldCount; ++n)
				{
				Field *field = table->field[n];
				CHARSET_INFO *charset = field->charset();

				if (charset)
					storageShare->registerCollation(charset->name, charset);
				}

			storageShare->unlock();
			}
		}

	int ret = storageTable->open();

	if (ret)
		DBUG_RETURN(error(ret));

	thr_lock_data_init((THR_LOCK *)storageShare->impure, &lockData, NULL);

	ret = setIndexes();
	if (ret)
		DBUG_RETURN(error(ret));

	DBUG_RETURN(0);
}


StorageConnection* StorageInterface::getStorageConnection(THD* thd)
{
	return *(StorageConnection**) thd_ha_data(thd, falcon_hton);
}

int StorageInterface::close(void)
{
  DBUG_ENTER("StorageInterface::close");
  DBUG_RETURN(0);
}

int StorageInterface::rnd_next(uchar *buf)
{
	DBUG_ENTER("StorageInterface::rnd_next");
	ha_statistic_increment(&SSV::ha_read_rnd_next_count);

	if (activeBlobs)
		freeActiveBlobs();

	lastRecord = storageTable->next(nextRecord, lockForUpdate);

	if (lastRecord < 0)
		{
		if (lastRecord == StorageErrorRecordNotFound)
			{
			lastRecord = -1;
			table->status = STATUS_NOT_FOUND;
			DBUG_RETURN(HA_ERR_END_OF_FILE);
			}

		DBUG_RETURN(error(lastRecord));
		}
	else
		table->status = 0;

	decodeRecord(buf);
	nextRecord = lastRecord + 1;

	DBUG_RETURN(0);
}


void StorageInterface::unlock_row(void)
{
	storageTable->unlockRow();
}

int StorageInterface::rnd_pos(uchar *buf, uchar *pos)
{
	int recordNumber;
	DBUG_ENTER("StorageInterface::rnd_pos");
	ha_statistic_increment(&SSV::ha_read_rnd_next_count);

	memcpy(&recordNumber, pos, sizeof(recordNumber));

	if (activeBlobs)
		freeActiveBlobs();

	int ret = storageTable->fetch(recordNumber, lockForUpdate);

	if (ret)
		{
		table->status = STATUS_NOT_FOUND;
		DBUG_RETURN(error(ret));
		}

	lastRecord = recordNumber;
	decodeRecord(buf);
	table->status = 0;

	DBUG_RETURN(0);
}

void StorageInterface::position(const uchar *record)
{
  DBUG_ENTER("StorageInterface::position");
  memcpy(ref, &lastRecord, sizeof(lastRecord));
  DBUG_VOID_RETURN;
}

int StorageInterface::info(uint what)
{
	DBUG_ENTER("StorageInterface::info");

#ifndef NO_OPTIMIZE
	if (what & HA_STATUS_VARIABLE)
		getDemographics();
#endif

	if (what & HA_STATUS_AUTO)
		stats.auto_increment_value = storageShare->getSequenceValue(0);

	if (what & HA_STATUS_ERRKEY)
		errkey = indexErrorId;

	DBUG_RETURN(0);
}


void StorageInterface::getDemographics(void)
{
	DBUG_ENTER("StorageInterface::getDemographics");

	stats.records = (ha_rows) storageShare->estimateCardinality();
	// Temporary fix for Bug#28686. (HK) 2007-05-26.
	if (!stats.records)
		stats.records = 2;

	stats.block_size = 4096;

	for (uint n = 0; n < table->s->keys; ++n)
		{
		KEY *key = table->s->key_info + n;
		StorageIndexDesc *desc = storageShare->getIndex(n);

		if (desc)
			{
			ha_rows rows = 1 << desc->numberSegments;

			for (uint segment = 0; segment < key->key_parts; ++segment, rows >>= 1)
				{
				ha_rows recordsPerSegment = (ha_rows) desc->segmentRecordCounts[segment];
				key->rec_per_key[segment] = (ulong) MAX(recordsPerSegment, rows);
				}
			}
		}

	DBUG_VOID_RETURN;
}

int StorageInterface::optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
	DBUG_ENTER("StorageInterface::optimize");

	int ret = storageTable->optimize();

	if (ret)
		DBUG_RETURN(error(ret));

	DBUG_RETURN(0);
}

uint8 StorageInterface::table_cache_type(void)
{
	return HA_CACHE_TBL_TRANSACT;
}

const char *StorageInterface::table_type(void) const
{
	DBUG_ENTER("StorageInterface::table_type");
	DBUG_RETURN(falcon_hton_name);
}


const char **StorageInterface::bas_ext(void) const
{
	DBUG_ENTER("StorageInterface::bas_ext");
	DBUG_RETURN(falcon_extensions);
}


ulonglong StorageInterface::table_flags(void) const
{
	DBUG_ENTER("StorageInterface::table_flags");
	DBUG_RETURN(HA_REC_NOT_IN_SEQ | HA_NULL_IN_KEY | // HA_AUTO_PART_KEY |
	            HA_PARTIAL_COLUMN_READ | HA_CAN_GEOMETRY | HA_BINLOG_ROW_CAPABLE);
}


ulong StorageInterface::index_flags(uint idx, uint part, bool all_parts) const
{
	DBUG_ENTER("StorageInterface::index_flags");
	DBUG_RETURN(HA_READ_RANGE | HA_KEY_SCAN_NOT_ROR);
}


int StorageInterface::create(const char *mySqlName, TABLE *form,
                            HA_CREATE_INFO *info)
{
	DBUG_ENTER("StorageInterface::create");
	tempTable = (info->options & HA_LEX_CREATE_TMP_TABLE) ? true : false;
	OpenOption openOption = (tempTable) ? OpenTemporaryDatabase : OpenOrCreateDatabase;

	if (storageTable)
		{
		storageTable->deleteStorageTable();
		storageTable = NULL;
		}

	if (!mySqlThread)
		mySqlThread = current_thd;

	storageShare = storageHandler->createTable(mySqlName, info->tablespace, tempTable);

	if (!storageShare)
		DBUG_RETURN(HA_ERR_TABLE_EXIST);

	storageConnection = storageHandler->getStorageConnection(storageShare, mySqlThread, mySqlThread->thread_id, openOption);
	*((StorageConnection**) thd_ha_data(mySqlThread, falcon_hton)) = storageConnection;

	if (!storageConnection)
		DBUG_RETURN(HA_ERR_NO_CONNECTION);

	storageTable = storageConnection->getStorageTable(storageShare);
	storageTable->setLocalTable(this);
	
	int ret;
	int64 incrementValue = 0;
	uint n;
	CmdGen gen;
	const char *tableName = storageTable->getName();
	const char *schemaName = storageTable->getSchemaName();
	gen.gen("create table \"%s\".\"%s\" (\n", schemaName, tableName);
	const char *sep = "";
	char nameBuffer[129];

	for (n = 0; n < form->s->fields; ++n)
		{
		Field *field = form->field[n];
		CHARSET_INFO *charset = field->charset();

		if (charset)
			storageShare->registerCollation(charset->name, charset);

		storageShare->cleanupFieldName(field->field_name, nameBuffer,
										sizeof(nameBuffer));
		gen.gen("%s  \"%s\" ", sep, nameBuffer);
		int ret = genType(field, &gen);

		if (ret)
			DBUG_RETURN(ret);

		if (!field->maybe_null())
			gen.gen(" not null");

		sep = ",\n";
		}

	if (form->found_next_number_field) // && form->s->next_number_key_offset == 0)
		{
		incrementValue = info->auto_increment_value;

		if (incrementValue == 0)
			incrementValue = 1;
		}

	if (form->s->primary_key < form->s->keys)
		{
		KEY *key = form->key_info + form->s->primary_key;
		gen.gen(",\n  primary key ");
		genKeyFields(key, &gen);
		}

	gen.gen (")");
	const char *tableSpace = NULL;

	if (tempTable)
		tableSpace = TEMPORARY_TABLESPACE;
	else if (info->tablespace)
		tableSpace = info->tablespace;
	else
		tableSpace = DEFAULT_TABLESPACE;

	if (tableSpace)
		gen.gen(" tablespace %s", tableSpace);

	DBUG_PRINT("info",("incrementValue = " I64FORMAT, (long long int)incrementValue));

	if ((ret = storageTable->create(gen.getString(), incrementValue)))
		DBUG_RETURN(error(ret));

	for (n = 0; n < form->s->keys; ++n)
		if (n != form->s->primary_key)
			if ((ret = createIndex(schemaName, tableName, form->key_info + n, n)))
				{
				storageTable->deleteTable();

				DBUG_RETURN(error(ret));
				}

	DBUG_RETURN(0);
}

int StorageInterface::add_index(TABLE* table_arg, KEY* key_info, uint num_of_keys)
{
	DBUG_ENTER("StorageInterface::add_index");
	int ret = createIndex(storageTable->getSchemaName(), storageTable->getName(), key_info, table_arg->s->keys);

	DBUG_RETURN(ret);
}

int StorageInterface::createIndex(const char *schemaName, const char *tableName,
                                 KEY *key, int indexNumber)
{
	CmdGen gen;
	const char *unique = (key->flags & HA_NOSAME) ? "unique " : "";
	gen.gen("create %sindex \"%s$%d\" on %s.\"%s\" ", unique, tableName,
	        indexNumber, schemaName, tableName);
	genKeyFields(key, &gen);
	const char *sql = gen.getString();

	return storageTable->share->createIndex(storageConnection, key->name, sql);
}

#if 0
uint StorageInterface::alter_table_flags(uint flags)
{
	if (flags & ALTER_DROP_PARTITION)
		return 0;

	return HA_ONLINE_ADD_INDEX | HA_ONLINE_ADD_UNIQUE_INDEX;
}
#endif

bool StorageInterface::check_if_incompatible_data(HA_CREATE_INFO* create_info, uint table_changes)
{
	if (true || create_info->auto_increment_value != 0)
		return COMPATIBLE_DATA_NO;

	return COMPATIBLE_DATA_YES;
}

THR_LOCK_DATA **StorageInterface::store_lock(THD *thd, THR_LOCK_DATA **to,
                                            enum thr_lock_type lock_type)
{
	DBUG_ENTER("StorageInterface::store_lock");
	//lockForUpdate = (lock_type == TL_WRITE && thd->lex->sql_command == SQLCOM_SELECT);
	lockForUpdate = (lock_type == TL_WRITE);

	if (lock_type != TL_IGNORE && lockData.type == TL_UNLOCK)
		{
		/*
		  Here is where we get into the guts of a row level lock.
		  If TL_UNLOCK is set
		  If we are not doing a LOCK TABLE or DISCARD/IMPORT
		  TABLESPACE, then allow multiple writers
		*/

		if ( (lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) &&
		     !thd_in_lock_tables(thd))
			lock_type = TL_WRITE_ALLOW_WRITE;

		/*
		  In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
		  MySQL would use the lock TL_READ_NO_INSERT on t2, and that
		  would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
		  to t2. Convert the lock to a normal read lock to allow
		  concurrent inserts to t2.
		*/

		#ifdef XXX_TALK_TO_SERGEI
		if (lock_type == TL_READ_NO_INSERT && !thd_in_lock_tables(thd))
			lock_type = TL_READ;
		#endif


		lockData.type = lock_type;
		}

	*to++ = &lockData;
	DBUG_RETURN(to);
}


int StorageInterface::delete_table(const char *tableName)
{
	DBUG_ENTER("StorageInterface::delete_table");

	if (!mySqlThread)
		mySqlThread = current_thd;

	if (!storageShare)
		storageShare = storageHandler->findTable(tableName);

	if (!storageConnection)
		if ( !(storageConnection = storageHandler->getStorageConnection(storageShare, mySqlThread, mySqlThread->thread_id, OpenDatabase)) )
			DBUG_RETURN(0);

	if (!storageTable)
		storageTable = storageConnection->getStorageTable(storageShare);

	if (storageShare)
		{
		storageShare->lock(true);

		if (storageShare->initialized)
			{
			thr_lock_delete((THR_LOCK*) storageShare->impure);
			storageShare->initialized = false;
			//DBUG_ASSERT(false);
			}

		storageShare->unlock();
		}

	int res = storageTable->deleteTable();
	storageTable->deleteStorageTable();
	storageTable = NULL;

	// (hk) Fix for Bug#31465 Running Falcon test suite leads
	//                        to warnings about temp tables
	// This fix could affect other DROP TABLE scenarios.
	// if (res == StorageErrorTableNotFound)
	if (res != StorageErrorUncommittedUpdates)
		res = 0;

	DBUG_RETURN(error(res));
}

uint StorageInterface::max_supported_keys(void) const
{
	DBUG_ENTER("StorageInterface::max_supported_keys");
	DBUG_RETURN(MAX_KEY);
}


int StorageInterface::write_row(uchar *buff)
{
	DBUG_ENTER("StorageInterface::write_row");
	ha_statistic_increment(&SSV::ha_write_count);

	/* If we have a timestamp column, update it to the current time */

	if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
		table->timestamp_field->set_time();

	/*
		If we have an auto_increment column and we are writing a changed row
		or a new row, then update the auto_increment value in the record.
	*/

	if (table->next_number_field && buff == table->record[0])
		{
		update_auto_increment();

		/*
		   If the new value is less than the current highest value, it will be
		   ignored by setSequenceValue().
		*/

		int code = storageShare->setSequenceValue(table->next_number_field->val_int());

		if (code)
			DBUG_RETURN(error(code));
		}

	encodeRecord(buff, false);
	lastRecord = storageTable->insert();

	if (lastRecord < 0)
		{
		int code = lastRecord >> StoreErrorIndexShift;
		indexErrorId = (lastRecord & StoreErrorIndexMask) - 1;

		DBUG_RETURN(error(code));
		}

	if (++insertCount > LOAD_AUTOCOMMIT_RECORDS)
		switch (thd_sql_command(mySqlThread))
			{
			case SQLCOM_LOAD:
			case SQLCOM_ALTER_TABLE:
				storageHandler->commit(mySqlThread);
				storageConnection->startTransaction(thd_tx_isolation(mySqlThread));
				storageConnection->markVerb();
				insertCount = 0;
				break;

			default:
				;
			}

	DBUG_RETURN(0);
}


int StorageInterface::update_row(const uchar* oldData, uchar* newData)
{
	DBUG_ENTER("StorageInterface::update_row");
	DBUG_ASSERT (lastRecord >= 0);

	ha_statistic_increment(&SSV::ha_update_count);

	/* If we have a timestamp column, update it to the current time */

	if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
		table->timestamp_field->set_time();

	/* If we have an auto_increment column, update the sequence value.  */

	Field *autoInc = table->found_next_number_field;

	if ((autoInc) && bitmap_is_set(table->read_set, autoInc->field_index))
		{
		int code = storageShare->setSequenceValue(autoInc->val_int());

		if (code)
			DBUG_RETURN(error(code));
		}

	encodeRecord(newData, true);

	int ret = storageTable->updateRow(lastRecord);

	if (ret)
		{
		int code = ret >> StoreErrorIndexShift;
		indexErrorId = (ret & StoreErrorIndexMask) - 1;
		DBUG_RETURN(error(code));
		}

	DBUG_RETURN(0);
}


int StorageInterface::delete_row(const uchar* buf)
{
	DBUG_ENTER("StorageInterface::delete_row");
	DBUG_ASSERT (lastRecord >= 0);
	ha_statistic_increment(&SSV::ha_delete_count);

	if (activeBlobs)
		freeActiveBlobs();

	int ret = storageTable->deleteRow(lastRecord);

	if (ret < 0)
		DBUG_RETURN(error(ret));

	lastRecord = -1;

	DBUG_RETURN(0);
}


int StorageInterface::commit(handlerton *hton, THD* thd, bool all)
{
	DBUG_ENTER("StorageInterface::commit");
	StorageConnection *storageConnection = getStorageConnection(thd);

	if (all || !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
		{
		if (storageConnection)
			storageConnection->commit();
		else
			storageHandler->commit(thd);
		}
	else
		{
		if (storageConnection)
			storageConnection->releaseVerb();
		else
			storageHandler->releaseVerb(thd);
		}

	DBUG_RETURN(0);
}

int StorageInterface::prepare(handlerton* hton, THD* thd, bool all)
{
	DBUG_ENTER("StorageInterface::prepare");

	if (all || !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
		storageHandler->prepare(thd, sizeof(thd->transaction.xid), (const unsigned char*) &thd->transaction.xid);
	else
		{
		StorageConnection *storageConnection = getStorageConnection(thd);

		if (storageConnection)
			storageConnection->releaseVerb();
		else
			storageHandler->releaseVerb(thd);
		}

	DBUG_RETURN(0);
}

int StorageInterface::rollback(handlerton *hton, THD *thd, bool all)
{
	DBUG_ENTER("StorageInterface::rollback");
	StorageConnection *storageConnection = getStorageConnection(thd);

	if (all || !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
		{
		if (storageConnection)
			storageConnection->rollback();
		else
			storageHandler->rollback(thd);
		}
	else
		{
		if (storageConnection)
			storageConnection->rollbackVerb();
		else
			storageHandler->rollbackVerb(thd);
		}

	DBUG_RETURN(0);
}

int StorageInterface::commit_by_xid(handlerton* hton, XID* xid)
{
	DBUG_ENTER("StorageInterface::commit_by_xid");
	int ret = storageHandler->commitByXID(sizeof(XID), (const unsigned char*) xid);
	DBUG_RETURN(ret);
}

int StorageInterface::rollback_by_xid(handlerton* hton, XID* xid)
{
	DBUG_ENTER("StorageInterface::rollback_by_xid");
	int ret = storageHandler->rollbackByXID(sizeof(XID), (const unsigned char*) xid);
	DBUG_RETURN(ret);
}

const COND* StorageInterface::cond_push(const COND* cond)
{
#ifdef COND_PUSH_ENABLED
	DBUG_ENTER("StorageInterface::cond_push");
	char buff[256];
	String str(buff,(uint32) sizeof(buff), system_charset_info);
	str.length(0);
	Item *cond_ptr= (COND *)cond;
	cond_ptr->print(&str);
	str.append('\0');
	DBUG_PRINT("StorageInterface::cond_push", ("%s", str.ptr()));
	DBUG_RETURN(0);
#else
	return handler::cond_push(cond);
#endif
}

void StorageInterface::startTransaction(void)
{
	threadSwitch(table->in_use);

	if (!storageConnection->transactionActive)
		{
		storageConnection->startTransaction(thd_tx_isolation(mySqlThread));
		trans_register_ha(mySqlThread, true, falcon_hton);
		}

	switch (thd_tx_isolation(mySqlThread))
		{
		case ISO_READ_UNCOMMITTED:
			error(StorageWarningReadUncommitted);
			break;

		case ISO_SERIALIZABLE:
			error(StorageWarningSerializable);
			break;
		}
}


int StorageInterface::savepointSet(handlerton *hton, THD *thd, void *savePoint)
{
	return storageHandler->savepointSet(thd, savePoint);
}


int StorageInterface::savepointRollback(handlerton *hton, THD *thd, void *savePoint)
{
	return storageHandler->savepointRollback(thd, savePoint);
}


int StorageInterface::savepointRelease(handlerton *hton, THD *thd, void *savePoint)
{
	return storageHandler->savepointRelease(thd, savePoint);
}


int StorageInterface::index_read(uchar *buf, const uchar *keyBytes, uint key_len,
                                enum ha_rkey_function find_flag)
{
	DBUG_ENTER("StorageInterface::index_read");
	int ret, which = 0;
	ha_statistic_increment(&SSV::ha_read_key_count);

	// XXX This needs to be revisited
	switch(find_flag)
		{
		case HA_READ_KEY_EXACT:
			which = UpperBound | LowerBound;
			break;

		case HA_READ_KEY_OR_NEXT:
			storageTable->setPartialKey();
			which = LowerBound;
			break;

		case HA_READ_AFTER_KEY:     // ???
			if (!storageTable->isKeyNull((const unsigned char*) keyBytes, key_len))
				which = LowerBound;

			break;

		case HA_READ_BEFORE_KEY:    // ???
		case HA_READ_PREFIX_LAST_OR_PREV: // ???
		case HA_READ_PREFIX_LAST:   // ???
		case HA_READ_PREFIX:
		case HA_READ_KEY_OR_PREV:
		default:
			DBUG_RETURN(HA_ERR_UNSUPPORTED);
		}

	const unsigned char *key = (const unsigned char*) keyBytes;

	if (which)
		if ((ret = storageTable->setIndexBound(key, key_len, which)))
			DBUG_RETURN(error(ret));

	if ((ret = storageTable->indexScan()))
		DBUG_RETURN(error(ret));

	nextRecord = 0;

	for (;;)
		{
		int ret = index_next(buf);

		if (ret)
			DBUG_RETURN(ret);

		int comparison = storageTable->compareKey(key, key_len);

		if ((which & LowerBound) && comparison < 0)
			continue;

		if ((which & UpperBound) && comparison > 0)
			continue;

		DBUG_RETURN(0);
		}
}


int StorageInterface::index_init(uint idx, bool sorted)
{
	DBUG_ENTER("StorageInterface::index_init");
	active_index = idx;
	nextRecord = 0;
	haveStartKey = false;
	haveEndKey = false;

	if (!storageTable->setIndex(idx))
		DBUG_RETURN(0);

	StorageIndexDesc indexDesc;
	getKeyDesc(table->key_info + idx, &indexDesc);

	if (idx == table->s->primary_key)
		indexDesc.primaryKey = true;

	int ret = storageTable->setIndex(table->s->keys, idx, &indexDesc);

	if (ret)
		DBUG_RETURN(error(ret));

	DBUG_RETURN(0);
}


int StorageInterface::index_end(void)
{
	DBUG_ENTER("StorageInterface::index_end");
	storageTable->indexEnd();
	DBUG_RETURN(0);
}

ha_rows StorageInterface::records_in_range(uint indexId, key_range *lower,
                                         key_range *upper)
{
	DBUG_ENTER("StorageInterface::records_in_range");

#ifdef NO_OPTIMIZE
	DBUG_RETURN(handler::records_in_range(indexId, lower, upper));
#endif

	ha_rows cardinality = (ha_rows) storageShare->estimateCardinality();

	if (!lower)
		DBUG_RETURN(MAX(cardinality, 2));

	StorageIndexDesc *index = storageShare->getIndex(indexId);

	if (!index)
		DBUG_RETURN(MAX(cardinality, 2));

	int numberSegments = 0;

	for (int map = lower->keypart_map; map; map >>= 1)
		++numberSegments;

	if (index->unique && numberSegments == index->numberSegments)
		DBUG_RETURN(1);

	ha_rows segmentRecords = (ha_rows) index->segmentRecordCounts[numberSegments - 1];
	ha_rows guestimate = cardinality;

	if (lower->flag == HA_READ_KEY_EXACT)
		{
		if (segmentRecords)
			guestimate = segmentRecords;
		else
			for (int n = 0; n < numberSegments; ++n)
				guestimate /= 10;
		}
	else
		guestimate /= 3;

	DBUG_RETURN(MAX(guestimate, 2));
}


void StorageInterface::getKeyDesc(KEY *keyInfo, StorageIndexDesc *indexInfo)
{
	int numberKeys = keyInfo->key_parts;
	indexInfo->numberSegments = numberKeys;
	indexInfo->name = keyInfo->name;
	indexInfo->unique = (keyInfo->flags & HA_NOSAME);
	indexInfo->primaryKey = false;

	for (int n = 0; n < numberKeys; ++n)
		{
		StorageSegment *segment = indexInfo->segments + n;
		KEY_PART_INFO *part = keyInfo->key_part + n;
		segment->offset = part->offset;
		segment->length = part->length;
		segment->type = part->field->key_type();
		segment->nullBit = part->null_bit;
		segment->isUnsigned = (part->field->flags & ENUM_FLAG) ?
			true : ((Field_num*) part->field)->unsigned_flag;

		switch (segment->type)
			{
			case HA_KEYTYPE_TEXT:
			case HA_KEYTYPE_VARTEXT1:
			case HA_KEYTYPE_VARTEXT2:
			case HA_KEYTYPE_VARBINARY1:
			case HA_KEYTYPE_VARBINARY2:
				segment->mysql_charset = part->field->charset();
				break;

			default:
				segment->mysql_charset = NULL;
			}
		}
}


int StorageInterface::rename_table(const char *from, const char *to)
{
	DBUG_ENTER("StorageInterface::rename_table");
	//tempTable = storageHandler->isTempTable(from) > 0;
	table = 0; // XXX hack?
	int ret = open(from, 0, 0);

	if (ret)
		DBUG_RETURN(ret);

	if ( (ret = storageShare->renameTable(storageConnection, to)) )
		DBUG_RETURN(ret);
	
	DBUG_RETURN(error(ret));
}


double StorageInterface::read_time(uint index, uint ranges, ha_rows rows)
{
	DBUG_ENTER("StorageInterface::read_time");
	DBUG_RETURN(rows2double(rows / 3));
}


int StorageInterface::read_range_first(const key_range *start_key,
                                      const key_range *end_key,
                                      bool eq_range_arg, bool sorted)
{
	DBUG_ENTER("StorageInterface::read_range_first");
	storageTable->clearIndexBounds();
	haveStartKey = false;
	haveEndKey = false;

	if (start_key && !storageTable->isKeyNull((const unsigned char*) start_key->key, start_key->length))
		{
		haveStartKey = true;
		startKey = *start_key;

		if (start_key->flag == HA_READ_KEY_OR_NEXT)
			storageTable->setPartialKey();
		else if (start_key->flag == HA_READ_AFTER_KEY)
			storageTable->setReadAfterKey();

		int ret = storageTable->setIndexBound((const unsigned char*) start_key->key,
												start_key->length, LowerBound);
		if (ret)
			DBUG_RETURN(error(ret));
		}

	if (end_key)
		{
		if (end_key->flag == HA_READ_BEFORE_KEY)
			storageTable->setReadBeforeKey();

		int ret = storageTable->setIndexBound((const unsigned char*) end_key->key,
												end_key->length, UpperBound);
		if (ret)
			DBUG_RETURN(error(ret));
		}

	storageTable->indexScan();
	nextRecord = 0;
	lastRecord = -1;
	eq_range = eq_range_arg;
	end_range = 0;

	if (end_key)
		{
		haveEndKey = true;
		endKey = *end_key;
		end_range = &save_end_range;
		save_end_range = *end_key;
		key_compare_result_on_equal = ((end_key->flag == HA_READ_BEFORE_KEY) ? 1 :
										(end_key->flag == HA_READ_AFTER_KEY) ? -1 :
										0);
		}

	range_key_part = table->key_info[active_index].key_part;

	for (;;)
		{
		int result = index_next(table->record[0]);

		if (result)
			{
			if (result == HA_ERR_KEY_NOT_FOUND)
				result = HA_ERR_END_OF_FILE;

			table->status = result;
			DBUG_RETURN(result);
			}

		DBUG_RETURN(0);
		}
}


int StorageInterface::index_next(uchar *buf)
{
	DBUG_ENTER("StorageInterface::index_next");
	ha_statistic_increment(&SSV::ha_read_next_count);

	if (activeBlobs)
		freeActiveBlobs();

	for (;;)
		{
		lastRecord = storageTable->nextIndexed(nextRecord, lockForUpdate);

		if (lastRecord < 0)
			{
			if (lastRecord == StorageErrorRecordNotFound)
				{
				lastRecord = -1;
				table->status = STATUS_NOT_FOUND;
				DBUG_RETURN(HA_ERR_END_OF_FILE);
				}

			DBUG_RETURN(error(lastRecord));
			}

		nextRecord = lastRecord + 1;

		if (haveStartKey)
			{
			int n = storageTable->compareKey((const unsigned char*) startKey.key, startKey.length);

			if (n < 0 || (n == 0 && startKey.flag == HA_READ_AFTER_KEY))
				{
				storageTable->unlockRow();
				continue;
				}
			}

		if (haveEndKey)
			{
			int n = storageTable->compareKey((const unsigned char*) endKey.key, endKey.length);

			if (n > 0 || (n == 0 && endKey.flag == HA_READ_BEFORE_KEY))
				{
				storageTable->unlockRow();
				continue;
				}
			}

		decodeRecord(buf);
		table->status = 0;

		DBUG_RETURN(0);
		}
}


int StorageInterface::index_next_same(uchar *buf, const uchar *key, uint key_len)
{
	DBUG_ENTER("StorageInterface::index_next_same");
	ha_statistic_increment(&SSV::ha_read_next_count);

	for (;;)
		{
		int ret = index_next(buf);

		if (ret)
			DBUG_RETURN(ret);

		int comparison = storageTable->compareKey((const unsigned char*) key, key_len);

		if (comparison == 0)
			DBUG_RETURN(0);
		}
}


double StorageInterface::scan_time(void)
{
	DBUG_ENTER("StorageInterface::scan_time");
	DBUG_RETURN((double)stats.records * 1000);
}

bool StorageInterface::threadSwitch(THD* newThread)
{
	if (newThread == mySqlThread)
		return false;

	if (storageConnection)
		{
		if (!storageConnection->mySqlThread && !mySqlThread)
			{
			storageConnection->setMySqlThread(newThread);
			mySqlThread = newThread;

			return false;
			}

		storageConnection->release();
		}

	storageConnection = storageHandler->getStorageConnection(storageShare, newThread, newThread->thread_id, OpenDatabase);

	if (storageTable)
		storageTable->setConnection(storageConnection);
	else
		storageTable = storageConnection->getStorageTable(storageShare);

	mySqlThread = newThread;

	return true;
}


int StorageInterface::threadSwitchError(void)
{
	return 1;
}


int StorageInterface::error(int storageError)
{
	DBUG_ENTER("StorageInterface::error");

	if (storageError == 0)
		{
		DBUG_PRINT("info", ("returning 0"));
		DBUG_RETURN(0);
		}

	switch (storageError)
		{
		case StorageErrorDupKey:
			DBUG_PRINT("info", ("StorageErrorDupKey"));
			DBUG_RETURN(HA_ERR_FOUND_DUPP_KEY);

		case StorageErrorDeadlock:
			DBUG_PRINT("info", ("StorageErrorDeadlock"));
			DBUG_RETURN(HA_ERR_LOCK_DEADLOCK);

		case StorageErrorLockTimeout:
			DBUG_PRINT("info", ("StorageErrorLockTimeout"));
			DBUG_RETURN(HA_ERR_LOCK_WAIT_TIMEOUT);

		case StorageErrorRecordNotFound:
			DBUG_PRINT("info", ("StorageErrorRecordNotFound"));
			DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);

		case StorageErrorTableNotFound:
			DBUG_PRINT("info", ("StorageErrorTableNotFound"));
			DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);

		case StorageErrorNoIndex:
			DBUG_PRINT("info", ("StorageErrorNoIndex"));
			DBUG_RETURN(HA_ERR_WRONG_INDEX);

		case StorageErrorBadKey:
			DBUG_PRINT("info", ("StorageErrorBadKey"));
			DBUG_RETURN(HA_ERR_WRONG_INDEX);

		case StorageErrorTableExits:
			DBUG_PRINT("info", ("StorageErrorTableExits"));
			DBUG_RETURN(HA_ERR_TABLE_EXIST);

		case StorageErrorUpdateConflict:
			DBUG_PRINT("info", ("StorageErrorUpdateConflict"));
			DBUG_RETURN(HA_ERR_RECORD_CHANGED);

		case StorageErrorUncommittedUpdates:
			DBUG_PRINT("info", ("StorageErrorUncommittedUpdates"));
			DBUG_RETURN(HA_ERR_TABLE_EXIST);

		case StorageErrorUncommittedRecords:
			DBUG_PRINT("info", ("StorageErrorUncommittedRecords"));
			DBUG_RETURN(200 - storageError);

		case StorageErrorNoSequence:
			DBUG_PRINT("info", ("StorageErrorNoSequence"));

			if (storageConnection)
				storageConnection->setErrorText("no sequenced defined for autoincrement operation");

			DBUG_RETURN(200 - storageError);

		case StorageErrorTruncation:
			DBUG_PRINT("info", ("StorageErrorTruncation"));
			DBUG_RETURN(HA_ERR_TO_BIG_ROW);

		case StorageWarningSerializable:
			push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			                    ER_CANT_CHANGE_TX_ISOLATION,
			                    "Falcon does not support SERIALIZABLE ISOLATION, using REPEATABLE READ instead.");
			DBUG_RETURN(0);

		case StorageWarningReadUncommitted:
			push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			                    ER_CANT_CHANGE_TX_ISOLATION,
			                    "Falcon does not support READ UNCOMMITTED ISOLATION, using REPEATABLE READ instead.");
			DBUG_RETURN(0);

		case StorageErrorOutOfMemory:
			DBUG_PRINT("info", ("StorageErrorOutOfMemory"));
			DBUG_RETURN(HA_ERR_OUT_OF_MEM);

		case StorageErrorOutOfRecordMemory:
			DBUG_PRINT("info", ("StorageErrorOutOfRecordMemory"));
			DBUG_RETURN(200 - storageError);

		default:
			DBUG_PRINT("info", ("Unknown Falcon Error"));
			DBUG_RETURN(200 - storageError);
		}

	DBUG_RETURN(storageError);
}

int StorageInterface::start_stmt(THD *thd, thr_lock_type lock_type)
{
	DBUG_ENTER("StorageInterface::start_stmt");
	threadSwitch(thd);

	if (storageConnection->markVerb())
		trans_register_ha(thd, FALSE, falcon_hton);

	DBUG_RETURN(0);
}


int StorageInterface::external_lock(THD *thd, int lock_type)
{
	DBUG_ENTER("StorageInterface::external_lock");
	threadSwitch(thd);

	if (lock_type == F_UNLCK)
		{
		storageConnection->setCurrentStatement(NULL);

		if (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
			storageConnection->endImplicitTransaction();
		else
			storageConnection->releaseVerb();

		if (storageTable)
			{
			storageTable->clearRecord();
			storageTable->clearBitmap();
			storageTable->clearAlter();
			}
		}
	else
		{
		if (storageConnection && thd->query)
			storageConnection->setCurrentStatement(thd->query);

		insertCount = 0;

		switch (thd_sql_command(thd))
			{
			case SQLCOM_ALTER_TABLE:
			case SQLCOM_DROP_INDEX:
			case SQLCOM_CREATE_INDEX:
				{
				int ret = storageTable->alterCheck();

				if (ret)
					DBUG_RETURN(error(ret));
				}
				break;

			default:
				break;
			}

		if (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
			{
			if (storageConnection->startTransaction(thd_tx_isolation(thd)))
				trans_register_ha(thd, true, falcon_hton);

			if (storageConnection->markVerb())
				trans_register_ha(thd, false, falcon_hton);
			}
		else
			{
			if (storageConnection->startImplicitTransaction(thd_tx_isolation(thd)))
				trans_register_ha(thd, false, falcon_hton);
			}

		switch (thd_tx_isolation(mySqlThread))
			{
			case ISO_READ_UNCOMMITTED:
				error(StorageWarningReadUncommitted);
				break;

			case ISO_SERIALIZABLE:
				error(StorageWarningSerializable);
				break;
			}
		}

	DBUG_RETURN(0);
}


void StorageInterface::get_auto_increment(ulonglong offset, ulonglong increment,
                                         ulonglong nb_desired_values,
                                         ulonglong *first_value,
                                         ulonglong *nb_reserved_values)
{
	DBUG_ENTER("StorageInterface::get_auto_increment");
	*first_value = storageShare->getSequenceValue(1);
	*nb_reserved_values = 1;

	DBUG_VOID_RETURN;
}

int StorageInterface::reset_auto_increment(ulonglong value)
{
	return handler::reset_auto_increment(value);
}

const char *StorageInterface::index_type(uint key_number)
{
  DBUG_ENTER("StorageInterface::index_type");
  DBUG_RETURN("BTREE");
}

void StorageInterface::dropDatabase(handlerton *hton, char *path)
{
	DBUG_ENTER("StorageInterface::dropDatabase");
	storageHandler->dropDatabase(path);

	DBUG_VOID_RETURN;
}


void StorageInterface::freeActiveBlobs(void)
{
	for (StorageBlob *blob; (blob = activeBlobs); )
		{
		activeBlobs = blob->next;
		storageTable->freeBlob(blob);
		blob->next = freeBlobs;
		freeBlobs = blob;
		}
}


void StorageInterface::shutdown(handlerton *htons)
{
	storageHandler->shutdownHandler();
}


int StorageInterface::panic(handlerton* hton, ha_panic_function flag)
{
	storageHandler->shutdownHandler();

	return 0;
}


int StorageInterface::closeConnection(handlerton *hton, THD *thd)
{
	DBUG_ENTER("NfsStorageEngine::closeConnection");
	storageHandler->closeConnections(thd);
	*thd_ha_data(thd, hton) = NULL;

	DBUG_RETURN(0);
}


int StorageInterface::alter_tablespace(handlerton* hton, THD* thd, st_alter_tablespace* ts_info)
{
	DBUG_ENTER("NfsStorageEngine::alter_tablespace");
	int ret = 0;

	/*
	CREATE TABLESPACE tablespace
		ADD DATAFILE 'file'
		USE LOGFILE GROUP logfile_group
		[EXTENT_SIZE [=] extent_size]
		INITIAL_SIZE [=] initial_size
		ENGINE [=] engine
	*/

	switch (ts_info->ts_cmd_type)
		{
		case CREATE_TABLESPACE:
			ret = storageHandler->createTablespace(ts_info->tablespace_name, ts_info->data_file_name);
			break;

		case DROP_TABLESPACE:
			ret = storageHandler->deleteTablespace(ts_info->tablespace_name);
			break;

		default:
			DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
		}

	DBUG_RETURN(ret);
}

uint StorageInterface::max_supported_key_length(void) const
{
	// Assume 4K page unless proven otherwise.
	if (storageConnection)
		return storageConnection->getMaxKeyLength();

	return MAX_INDEX_KEY_LENGTH_4K;  // Default value.
}


uint StorageInterface::max_supported_key_part_length(void) const
{
	// Assume 4K page unless proven otherwise.
	if (storageConnection)
		return storageConnection->getMaxKeyLength();

	return MAX_INDEX_KEY_LENGTH_4K;  // Default for future sizes.
}


void StorageInterface::logger(int mask, const char* text, void* arg)
{
	if (mask & falcon_debug_mask)
		{
		printf ("%s", text);

		if (falcon_log_file)
			fprintf(falcon_log_file, "%s", text);
		}
}


int StorageInterface::setIndexes(void)
{
	if (!table || storageShare->haveIndexes())
		return 0;

	storageShare->lock(true);

	if (!storageShare->haveIndexes())
		{
		StorageIndexDesc indexDesc;

		for (uint n = 0; n < table->s->keys; ++n)
			{
			getKeyDesc(table->key_info + n, &indexDesc);

			if (n == table->s->primary_key)
				indexDesc.primaryKey = true;

			int ret = storageTable->setIndex(table->s->keys, n, &indexDesc);
			if (ret)
				return ret;
			}
		}

	storageShare->unlock();
	return 0;
}


int StorageInterface::genType(Field* field, CmdGen* gen)
{
	const char *type;
	const char *arg = NULL;
	int length = 0;

	switch (field->real_type())
		{
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_BIT:
			type = "smallint";
			break;

		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_YEAR:
			type = "int";
			break;

		case MYSQL_TYPE_FLOAT:
			type = "float";
			break;

		case MYSQL_TYPE_DOUBLE:
			type = "double";
			break;

		case MYSQL_TYPE_TIMESTAMP:
			type = "timestamp";
			break;

		case MYSQL_TYPE_SET:
		case MYSQL_TYPE_LONGLONG:
			type = "bigint";
			break;

		/*
			Falcon's date and time types don't handle invalid dates like MySQL's do,
			so we just use an int for storage
		*/

		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_ENUM:
		case MYSQL_TYPE_NEWDATE:
			type = "int";
			break;

		case MYSQL_TYPE_DATETIME:
			type = "bigint";
			break;

		case MYSQL_TYPE_VARCHAR:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
			{
			CHARSET_INFO *charset = field->charset();

			if (charset)
				{
				arg = charset->name;
				type = "varchar (%d) collation %s";
				}
			else
				type = "varchar (%d)";

			length = field->field_length;
			}
			break;

		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_GEOMETRY:
			if (field->field_length < 256)
				type = "varchar (256)";
			else
				type = "blob";
			break;

		case MYSQL_TYPE_NEWDECIMAL:
			{
			Field_new_decimal *newDecimal = (Field_new_decimal*) field;

			/***
			if (newDecimal->precision > 18 && newDecimal->dec > 9)
				{
				errorText = "columns with greater than 18 digits precision and greater than 9 digits of fraction are not supported";

				return HA_ERR_UNSUPPORTED;
				}
			***/

			gen->gen("numeric (%d,%d)", newDecimal->precision, newDecimal->dec);

			return 0;
			}

		default:
			errorText = "unsupported Falcon data type";

			return HA_ERR_UNSUPPORTED;
		}

	gen->gen(type, length, arg);

	return 0;
}


void StorageInterface::genKeyFields(KEY* key, CmdGen* gen)
{
	const char *sep = "(";
	char nameBuffer[129];

	for (uint n = 0; n < key->key_parts; ++n)
		{
		KEY_PART_INFO *part = key->key_part + n;
		Field *field = part->field;
		storageShare->cleanupFieldName(field->field_name, nameBuffer,
										sizeof(nameBuffer));

		if (part->key_part_flag & HA_PART_KEY_SEG)
			gen->gen("%s\"%s\"(%d)", sep, nameBuffer, part->length);
		else
			gen->gen("%s\"%s\"", sep, nameBuffer);

		sep = ", ";
		}

	gen->gen(")");
}


void StorageInterface::encodeRecord(uchar *buf, bool updateFlag)
{
	storageTable->preInsert();
	my_ptrdiff_t ptrDiff = buf - table->record[0];
	my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->read_set);

	for (uint n = 0; n < table->s->fields; ++n)
		{
		Field *field = table->field[n];

		if (ptrDiff)
			field->move_field_offset(ptrDiff);

		if (updateFlag && !bitmap_is_set(table->write_set, field->field_index))
			{
			const unsigned char *p = storageTable->getEncoding(n);
			storageTable->dataStream.encodeEncoding(p);
			}
		else if (field->is_null())
			storageTable->dataStream.encodeNull();
		else
			switch (field->real_type())
				{
				case MYSQL_TYPE_TINY:
				case MYSQL_TYPE_SHORT:
				case MYSQL_TYPE_INT24:
				case MYSQL_TYPE_LONG:
				case MYSQL_TYPE_LONGLONG:
				case MYSQL_TYPE_YEAR:
				case MYSQL_TYPE_DECIMAL:
				case MYSQL_TYPE_ENUM:
				case MYSQL_TYPE_SET:
				case MYSQL_TYPE_BIT:
					storageTable->dataStream.encodeInt64(field->val_int());
					break;

				case MYSQL_TYPE_NEWDECIMAL:
					{
					int precision = ((Field_new_decimal *)field)->precision;
					int scale = ((Field_new_decimal *)field)->dec;

					if (precision < 19)
						{
						int64 value = ScaledBinary::getInt64FromBinaryDecimal((const char *) field->ptr,
																			precision,
																			scale);
						storageTable->dataStream.encodeInt64(value, scale);
						}
					else
						{
						BigInt bigInt;
						ScaledBinary::getBigIntFromBinaryDecimal((const char*) field->ptr, precision, scale, &bigInt);

						// Handle value as int64 if possible. Even if the number fits
						// an int64, it can only be scaled within 18 digits or less.

						if (bigInt.fitsInInt64() && scale < 19)
							{
							int64 value = bigInt.getInt();
							storageTable->dataStream.encodeInt64(value, scale);
							}
						else
							storageTable->dataStream.encodeBigInt(&bigInt);
						}
					}
					break;

				case MYSQL_TYPE_DOUBLE:
				case MYSQL_TYPE_FLOAT:
					storageTable->dataStream.encodeDouble(field->val_real());
					break;

				case MYSQL_TYPE_TIMESTAMP:
					{
					my_bool nullValue;
					int64 value = ((Field_timestamp*) field)->get_timestamp(&nullValue);
					storageTable->dataStream.encodeDate(value * 1000);
					}
					break;

				case MYSQL_TYPE_DATE:
					storageTable->dataStream.encodeInt64(field->val_int());
					break;

				case MYSQL_TYPE_NEWDATE:
					//storageTable->dataStream.encodeInt64(field->val_int());
					storageTable->dataStream.encodeInt64(uint3korr(field->ptr));
					break;

				case MYSQL_TYPE_TIME:
					storageTable->dataStream.encodeInt64(field->val_int());
					break;

				case MYSQL_TYPE_DATETIME:
					storageTable->dataStream.encodeInt64(field->val_int());
					break;

				case MYSQL_TYPE_VARCHAR:
				case MYSQL_TYPE_VAR_STRING:
				case MYSQL_TYPE_STRING:
					{
					String string;
					String buffer;
					field->val_str(&buffer, &string);
					storageTable->dataStream.encodeOpaque(string.length(), string.ptr());
					}
					break;

				case MYSQL_TYPE_TINY_BLOB:
					{
					Field_blob *blob = (Field_blob*) field;
					uint length = blob->get_length();
					uchar *ptr;
					blob->get_ptr(&ptr);
					storageTable->dataStream.encodeOpaque(length, (const char*) ptr);
					}
					break;

				case MYSQL_TYPE_LONG_BLOB:
				case MYSQL_TYPE_BLOB:
				case MYSQL_TYPE_MEDIUM_BLOB:
				case MYSQL_TYPE_GEOMETRY:
					{
					Field_blob *blob = (Field_blob*) field;
					uint length = blob->get_length();
					uchar *ptr;
					blob->get_ptr(&ptr);
					StorageBlob *storageBlob;
					uint32 blobId;

					for (storageBlob = activeBlobs; storageBlob; storageBlob = storageBlob->next)
						if (storageBlob->data == (uchar*) ptr)
							{
							blobId = storageBlob->blobId;
							break;
							}

					if (!storageBlob)
						{
						StorageBlob storageBlob;
						storageBlob.length = length;
						storageBlob.data = (uchar *)ptr;
						blobId = storageTable->storeBlob(&storageBlob);
						blob->set_ptr(storageBlob.length, storageBlob.data);
						}

					storageTable->dataStream.encodeBinaryBlob(blobId);
					}
					break;

				default:
					storageTable->dataStream.encodeOpaque(field->field_length, (const char*) field->ptr);
				}

		if (ptrDiff)
			field->move_field_offset(-ptrDiff);
		}

	dbug_tmp_restore_column_map(table->read_set, old_map);
}

void StorageInterface::decodeRecord(uchar *buf)
{
	EncodedDataStream *dataStream = &storageTable->dataStream;
	my_ptrdiff_t ptrDiff = buf - table->record[0];
	my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
	DBUG_ENTER("StorageInterface::decodeRecord");

	for (uint n = 0; n < table->s->fields; ++n)
		{
		Field *field = table->field[n];
		dataStream->decode();

		if (ptrDiff)
			field->move_field_offset(ptrDiff);

		if (dataStream->type == edsTypeNull || !bitmap_is_set(table->read_set, field->field_index))
			field->set_null();
		else
			{
			field->set_notnull();

			switch (field->real_type())
				{
				case MYSQL_TYPE_TINY:
				case MYSQL_TYPE_SHORT:
				case MYSQL_TYPE_INT24:
				case MYSQL_TYPE_LONG:
				case MYSQL_TYPE_LONGLONG:
				case MYSQL_TYPE_YEAR:
				case MYSQL_TYPE_DECIMAL:
				case MYSQL_TYPE_ENUM:
				case MYSQL_TYPE_SET:
				case MYSQL_TYPE_BIT:
					field->store(dataStream->getInt64(),
								((Field_num*)field)->unsigned_flag);
					break;

				case MYSQL_TYPE_NEWDECIMAL:
					{
					int precision = ((Field_new_decimal*) field)->precision;
					int scale = ((Field_new_decimal*) field)->dec;

					if (dataStream->type == edsTypeBigInt)
						ScaledBinary::putBigInt(&dataStream->bigInt, (char*) field->ptr, precision, scale);
					else
						{
						int64 value = dataStream->getInt64(scale);
						ScaledBinary::putBinaryDecimal(value, (char*) field->ptr, precision, scale);
						}
					}
					break;

				case MYSQL_TYPE_DOUBLE:
				case MYSQL_TYPE_FLOAT:
					field->store(dataStream->value.dbl);
					break;

				case MYSQL_TYPE_TIMESTAMP:
					{
					int value = (int) (dataStream->value.integer64 / 1000);
					longstore(field->ptr, value);
					}
					break;

				case MYSQL_TYPE_DATE:
					field->store(dataStream->getInt64(), false);
					break;

				case MYSQL_TYPE_NEWDATE:
					//field->store(dataStream->getInt64(), false);
					int3store(field->ptr, dataStream->getInt32());
					break;

				case MYSQL_TYPE_TIME:
					field->store(dataStream->getInt64(), false);
					break;

				case MYSQL_TYPE_DATETIME:
					//field->store(dataStream->getInt64(), false);
					int8store(field->ptr, dataStream->getInt64());
					break;

				case MYSQL_TYPE_VARCHAR:
				case MYSQL_TYPE_VAR_STRING:
				case MYSQL_TYPE_STRING:
					field->store((const char*) dataStream->value.string.data,
								dataStream->value.string.length, field->charset());
					break;

				case MYSQL_TYPE_TINY_BLOB:
					{
					Field_blob *blob = (Field_blob*) field;
					blob->set_ptr(dataStream->value.string.length,
					              (uchar*) dataStream->value.string.data);
					}
					break;

				case MYSQL_TYPE_LONG_BLOB:
				case MYSQL_TYPE_BLOB:
				case MYSQL_TYPE_MEDIUM_BLOB:
				case MYSQL_TYPE_GEOMETRY:
					{
					Field_blob *blob = (Field_blob*) field;
					StorageBlob *storageBlob = freeBlobs;

					if (storageBlob)
						freeBlobs = storageBlob->next;
					else
						storageBlob = new StorageBlob;

					storageBlob->next = activeBlobs;
					activeBlobs = storageBlob;
					storageBlob->blobId = dataStream->value.blobId;
					storageTable->getBlob(storageBlob->blobId, storageBlob);
					blob->set_ptr(storageBlob->length, (uchar*) storageBlob->data);
					}
					break;

				default:
					{
					uint l = dataStream->value.string.length;

					if (field->field_length < l)
						l = field->field_length;

					memcpy(field->ptr, dataStream->value.string.data, l);
					}
				}
			}

		if (ptrDiff)
			field->move_field_offset(-ptrDiff);
		}
	dbug_tmp_restore_column_map(table->write_set, old_map);

	DBUG_VOID_RETURN;
}


int StorageInterface::extra(ha_extra_function operation)
{
	DBUG_ENTER("StorageInterface::extra");
	DBUG_RETURN(0);
}

bool StorageInterface::get_error_message(int error, String *buf)
{
	if (storageConnection)
		{
		const char *text = storageConnection->getLastErrorString();
		buf->set(text, strlen(text), system_charset_info);
		}
	else if (errorText)
		buf->set(errorText, strlen(errorText), system_charset_info);

	return false;
}

void StorageInterface::unlockTable(void)
{
	if (tableLocked)
		{
		storageShare->unlock();
		tableLocked = false;
		}
}

//*****************************************************************************
//
// NfsPluginHandler
//
//*****************************************************************************
NfsPluginHandler::NfsPluginHandler()
{
	storageConnection	= NULL;
	storageTable		= NULL;
}

NfsPluginHandler::~NfsPluginHandler()
{
}

//*****************************************************************************
//
// System Memory Usage
//
//*****************************************************************************
int NfsPluginHandler::call_fillSystemMemoryDetailTable(THD *thd, TABLE_LIST *tables, COND *cond)
{
	InfoTableImpl infoTable(thd, tables, system_charset_info);

	if (storageHandler)
		storageHandler->getMemoryDetailInfo(&infoTable);

	return infoTable.error;
}


ST_FIELD_INFO memoryDetailFieldInfo[]=
{
	{"FILE",		  120, MYSQL_TYPE_STRING,	0, 0, "File", SKIP_OPEN_TABLE},
	{"LINE",			4, MYSQL_TYPE_LONG,		0, 0, "Line", SKIP_OPEN_TABLE},
	{"OBJECTS_IN_USE",	4, MYSQL_TYPE_LONG,		0, 0, "Objects in Use", SKIP_OPEN_TABLE},
	{"SPACE_IN_USE",	4, MYSQL_TYPE_LONG,		0, 0, "Space in Use", SKIP_OPEN_TABLE},
	{"OBJECTS_DELETED", 4, MYSQL_TYPE_LONG,		0, 0, "Objects Deleted", SKIP_OPEN_TABLE},
	{"SPACE_DELETED",	4, MYSQL_TYPE_LONG,		0, 0, "Space Deleted", SKIP_OPEN_TABLE},
	{0,					0, MYSQL_TYPE_STRING,	0, 0, 0, SKIP_OPEN_TABLE}
};

int NfsPluginHandler::initSystemMemoryDetail(void *p)
{
	DBUG_ENTER("initSystemMemoryDetail");
	ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE*) p;
	schema->fields_info = memoryDetailFieldInfo;
	schema->fill_table = NfsPluginHandler::call_fillSystemMemoryDetailTable;
	DBUG_RETURN(0);
}

int NfsPluginHandler::deinitSystemMemoryDetail(void *p)
{
	DBUG_ENTER("deinitSystemMemoryDetail");
	DBUG_RETURN(0);
}

//*****************************************************************************
//
// System memory usage summary
//
//*****************************************************************************

int NfsPluginHandler::call_fillSystemMemorySummaryTable(THD *thd, TABLE_LIST *tables, COND *cond)
{
	//return(pluginHandler->fillSystemMemorySummaryTable(thd, tables, cond));
	InfoTableImpl infoTable(thd, tables, system_charset_info);

	if (storageHandler)
		storageHandler->getMemorySummaryInfo(&infoTable);

	return infoTable.error;
}

ST_FIELD_INFO memorySummaryFieldInfo[]=
{
	{"TOTAL_SPACE",		4, MYSQL_TYPE_LONGLONG,		0, 0, "Total Space", SKIP_OPEN_TABLE},
	{"FREE_SPACE",		4, MYSQL_TYPE_LONGLONG,		0, 0, "Free Space", SKIP_OPEN_TABLE},
	{"FREE_SEGMENTS",	4, MYSQL_TYPE_LONG,			0, 0, "Free Segments", SKIP_OPEN_TABLE},
	{"BIG_HUNKS",		4, MYSQL_TYPE_LONG,			0, 0, "Big Hunks", SKIP_OPEN_TABLE},
	{"SMALL_HUNKS",		4, MYSQL_TYPE_LONG,			0, 0, "Small Hunks", SKIP_OPEN_TABLE},
	{"UNIQUE_SIZES",	4, MYSQL_TYPE_LONG,			0, 0, "Unique Sizes", SKIP_OPEN_TABLE},
	{0,					0, MYSQL_TYPE_STRING,		0, 0, 0, SKIP_OPEN_TABLE}
};

int NfsPluginHandler::initSystemMemorySummary(void *p)
{
	DBUG_ENTER("initSystemMemorySummary");
	ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
	schema->fields_info = memorySummaryFieldInfo;
	schema->fill_table = NfsPluginHandler::call_fillSystemMemorySummaryTable;

	DBUG_RETURN(0);
}

int NfsPluginHandler::deinitSystemMemorySummary(void *p)
{
	DBUG_ENTER("deinitSystemMemorySummary");
	DBUG_RETURN(0);
}

//*****************************************************************************
//
// Record cache usage detail
//
//*****************************************************************************

int NfsPluginHandler::call_fillRecordCacheDetailTable(THD *thd, TABLE_LIST *tables, COND *cond)
{
	InfoTableImpl infoTable(thd, tables, system_charset_info);

	if (storageHandler)
		storageHandler->getRecordCacheDetailInfo(&infoTable);

	return infoTable.error;
}

ST_FIELD_INFO recordDetailFieldInfo[]=
{
	{"FILE",		  120, MYSQL_TYPE_STRING,	0, 0, "File", SKIP_OPEN_TABLE},
	{"LINE",			4, MYSQL_TYPE_LONG,		0, 0, "Line", SKIP_OPEN_TABLE},
	{"OBJECTS_IN_USE",	4, MYSQL_TYPE_LONG,		0, 0, "Objects in Use", SKIP_OPEN_TABLE},
	{"SPACE_IN_USE",	4, MYSQL_TYPE_LONG,		0, 0, "Space in Use", SKIP_OPEN_TABLE},
	{"OBJECTS_DELETED", 4, MYSQL_TYPE_LONG,		0, 0, "Objects Deleted", SKIP_OPEN_TABLE},
	{"SPACE_DELETED",	4, MYSQL_TYPE_LONG,		0, 0, "Space Deleted", SKIP_OPEN_TABLE},
	{0,					0, MYSQL_TYPE_STRING,	0, 0, 0, SKIP_OPEN_TABLE}
};

int NfsPluginHandler::initRecordCacheDetail(void *p)
{
	DBUG_ENTER("initRecordCacheDetail");
	ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
	schema->fields_info = recordDetailFieldInfo;
	schema->fill_table = NfsPluginHandler::call_fillRecordCacheDetailTable;

	DBUG_RETURN(0);
}

int NfsPluginHandler::deinitRecordCacheDetail(void *p)
{
	DBUG_ENTER("deinitRecordCacheDetail");
	DBUG_RETURN(0);
}

//*****************************************************************************
//
// Record cache usage summary
//
//*****************************************************************************

int NfsPluginHandler::call_fillRecordCacheSummaryTable(THD *thd, TABLE_LIST *tables, COND *cond)
{
	InfoTableImpl infoTable(thd, tables, system_charset_info);

	if (storageHandler)
		storageHandler->getRecordCacheSummaryInfo(&infoTable);

	return infoTable.error;
}

ST_FIELD_INFO recordSummaryFieldInfo[]=
{
	{"TOTAL_SPACE",		4, MYSQL_TYPE_LONGLONG,		0, 0, "Total Space", SKIP_OPEN_TABLE},
	{"FREE_SPACE",		4, MYSQL_TYPE_LONGLONG,		0, 0, "Free Space", SKIP_OPEN_TABLE},
	{"FREE_SEGMENTS",	4, MYSQL_TYPE_LONG,			0, 0, "Free Segments", SKIP_OPEN_TABLE},
	{"BIG_HUNKS",		4, MYSQL_TYPE_LONG,			0, 0, "Big Hunks", SKIP_OPEN_TABLE},
	{"SMALL_HUNKS",		4, MYSQL_TYPE_LONG,			0, 0, "Small Hunks", SKIP_OPEN_TABLE},
	{"UNIQUE_SIZES",	4, MYSQL_TYPE_LONG,			0, 0, "Unique Sizes", SKIP_OPEN_TABLE},
	{0,					0, MYSQL_TYPE_STRING,		0, 0, 0, SKIP_OPEN_TABLE}
};

int NfsPluginHandler::initRecordCacheSummary(void *p)
{
	DBUG_ENTER("initRecordCacheSummary");
	ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
	schema->fields_info = recordSummaryFieldInfo;
	schema->fill_table = NfsPluginHandler::call_fillRecordCacheSummaryTable;

	DBUG_RETURN(0);
}

int NfsPluginHandler::deinitRecordCacheSummary(void *p)
{
	DBUG_ENTER("deinitRecordCacheSummary");
	DBUG_RETURN(0);
}

//*****************************************************************************
//
// Database IO
//
//*****************************************************************************

int NfsPluginHandler::call_fillDatabaseIOTable(THD *thd, TABLE_LIST *tables, COND *cond)
{
	InfoTableImpl infoTable(thd, tables, system_charset_info);

	if (storageHandler)
		storageHandler->getIOInfo(&infoTable);

	return infoTable.error;
}

ST_FIELD_INFO databaseIOFieldInfo[]=
{
	{"TABLESPACE",	  120, MYSQL_TYPE_STRING,	0, 0, "Tablespace", SKIP_OPEN_TABLE},
	{"PAGE_SIZE",		4, MYSQL_TYPE_LONG,		0, 0, "Page Size", SKIP_OPEN_TABLE},
	{"BUFFERS",			4, MYSQL_TYPE_LONG,		0, 0, "Buffers", SKIP_OPEN_TABLE},
	{"PHYSICAL_READS",	4, MYSQL_TYPE_LONG,		0, 0, "Physical Reads", SKIP_OPEN_TABLE},
	{"WRITES",			4, MYSQL_TYPE_LONG,		0, 0, "Writes", SKIP_OPEN_TABLE},
	{"LOGICAL_READS",	4, MYSQL_TYPE_LONG,		0, 0, "Logical Reads", SKIP_OPEN_TABLE},
	{"FAKES",			4, MYSQL_TYPE_LONG,		0, 0, "Fakes", SKIP_OPEN_TABLE},
	{0,					0, MYSQL_TYPE_STRING,	0, 0, 0, SKIP_OPEN_TABLE}
};

int NfsPluginHandler::initDatabaseIO(void *p)
{
	DBUG_ENTER("initDatabaseIO");
	ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
	schema->fields_info = databaseIOFieldInfo;
	schema->fill_table = NfsPluginHandler::call_fillDatabaseIOTable;

	DBUG_RETURN(0);
}

int NfsPluginHandler::deinitDatabaseIO(void *p)
{
	DBUG_ENTER("deinitDatabaseIO");
	DBUG_RETURN(0);
}

int NfsPluginHandler::callTablesInfo(THD *thd, TABLE_LIST *tables, COND *cond)
{
	InfoTableImpl infoTable(thd, tables, system_charset_info);

	if (storageHandler)
		storageHandler->getTablesInfo(&infoTable);

	return infoTable.error;
}


ST_FIELD_INFO tablesFieldInfo[]=
{
	{"SCHEMA_NAME",	  127, MYSQL_TYPE_STRING,	0, 0, "Schema Name", SKIP_OPEN_TABLE},
	{"TABLE_NAME",	  127, MYSQL_TYPE_STRING,	0, 0, "Table Name", SKIP_OPEN_TABLE},
	{"PARTITION",	  127, MYSQL_TYPE_STRING,	0, 0, "Partition Name", SKIP_OPEN_TABLE},
	{"TABLESPACE",	  127, MYSQL_TYPE_STRING,	0, 0, "Tablespace", SKIP_OPEN_TABLE},
	{0,					0, MYSQL_TYPE_STRING,	0, 0, 0, SKIP_OPEN_TABLE}
};

int NfsPluginHandler::initTablesInfo(void *p)
{
	DBUG_ENTER("initTablesInfo");
	ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
	schema->fields_info = tablesFieldInfo;
	schema->fill_table = NfsPluginHandler::callTablesInfo;

	DBUG_RETURN(0);
}

int NfsPluginHandler::deinitTablesInfo(void *p)
{
	DBUG_ENTER("initTables");
	DBUG_RETURN(0);
}

//*****************************************************************************
//
// Transaction Information
//
//*****************************************************************************

int NfsPluginHandler::callTransactionInfo(THD *thd, TABLE_LIST *tables, COND *cond)
{
	InfoTableImpl infoTable(thd, tables, system_charset_info);

	if (storageHandler)
		storageHandler->getTransactionInfo(&infoTable);

	return infoTable.error;
}

ST_FIELD_INFO transactionInfoFieldInfo[]=
{
	{"DATABASE",		120, MYSQL_TYPE_STRING,		0, 0, "Database", SKIP_OPEN_TABLE},
	{"THREAD_ID",		4, MYSQL_TYPE_LONG,			0, 0, "Thread Id", SKIP_OPEN_TABLE},
	{"ID",				4, MYSQL_TYPE_LONG,			0, 0, "Id", SKIP_OPEN_TABLE},
	{"STATE",			10, MYSQL_TYPE_STRING,		0, 0, "State", SKIP_OPEN_TABLE},
	{"UPDATES",			4, MYSQL_TYPE_LONG,			0, 0, "Has Updates", SKIP_OPEN_TABLE},
	{"PENDING",			4, MYSQL_TYPE_LONG,			0, 0, "Write Pending", SKIP_OPEN_TABLE},
	{"DEP",				4, MYSQL_TYPE_LONG,			0, 0, "Dependencies", SKIP_OPEN_TABLE},
	{"OLDEST",			4, MYSQL_TYPE_LONG,			0, 0, "Oldest Active", SKIP_OPEN_TABLE},
	{"RECORDS",			4, MYSQL_TYPE_LONG,			0, 0, "Has Records", SKIP_OPEN_TABLE},
	{"WAITING_FOR",		4, MYSQL_TYPE_LONG,			0, 0, "Waiting For", SKIP_OPEN_TABLE},
	{"STATEMENT",	  120, MYSQL_TYPE_STRING,		0, 0, "Statement", SKIP_OPEN_TABLE},
	{0,					0, MYSQL_TYPE_STRING,		0, 0, 0, SKIP_OPEN_TABLE}
};

int NfsPluginHandler::initTransactionInfo(void *p)
{
	DBUG_ENTER("initTransactionInfo");
	ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
	schema->fields_info = transactionInfoFieldInfo;
	schema->fill_table = NfsPluginHandler::callTransactionInfo;

	DBUG_RETURN(0);
}

int NfsPluginHandler::deinitTransactionInfo(void *p)
{
	DBUG_ENTER("deinitTransactionInfo");
	DBUG_RETURN(0);
}

//*****************************************************************************
//
// Transaction Summary Information
//
//*****************************************************************************

int NfsPluginHandler::callTransactionSummaryInfo(THD *thd, TABLE_LIST *tables, COND *cond)
{
	InfoTableImpl infoTable(thd, tables, system_charset_info);

	if (storageHandler)
		storageHandler->getTransactionSummaryInfo(&infoTable);

	return infoTable.error;
}

ST_FIELD_INFO transactionInfoFieldSummaryInfo[]=
{
	{"DATABASE",		120, MYSQL_TYPE_STRING,		0, 0, "Database", SKIP_OPEN_TABLE},
	{"COMMITTED",		4, MYSQL_TYPE_LONG,			0, 0, "Committed Transaction.", SKIP_OPEN_TABLE},
	{"ROLLED_BACK",		4, MYSQL_TYPE_LONG,			0, 0, "Transactions Rolled Back.", SKIP_OPEN_TABLE},
	{"ACTIVE",   		4, MYSQL_TYPE_LONG,			0, 0, "Active Transactions", SKIP_OPEN_TABLE},
	{"PENDING_COMMIT",	4, MYSQL_TYPE_LONG,			0, 0, "Transaction Pending Commit", SKIP_OPEN_TABLE},
	{"PENDING_COMPLETION",4, MYSQL_TYPE_LONG,		0, 0, "Transaction Pending Completion", SKIP_OPEN_TABLE},
	{0,					0, MYSQL_TYPE_STRING,		0, 0, 0, SKIP_OPEN_TABLE}
};

int NfsPluginHandler::initTransactionSummaryInfo(void *p)
{
	DBUG_ENTER("initTransactionSummaryInfo");
	ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
	schema->fields_info = transactionInfoFieldSummaryInfo;
	schema->fill_table = NfsPluginHandler::callTransactionSummaryInfo;

	DBUG_RETURN(0);
}

int NfsPluginHandler::deinitTransactionSummaryInfo(void *p)
{
	DBUG_ENTER("deinitTransactionInfo");
	DBUG_RETURN(0);
}


//*****************************************************************************
//
// SerialLog Information
//
//*****************************************************************************

int NfsPluginHandler::callSerialLogInfo(THD *thd, TABLE_LIST *tables, COND *cond)
{
	InfoTableImpl infoTable(thd, tables, system_charset_info);

	if (storageHandler)
		storageHandler->getSerialLogInfo(&infoTable);

	return infoTable.error;
}

ST_FIELD_INFO serialSerialLogFieldInfo[]=
{
	{"DATABASE",		120, MYSQL_TYPE_STRING,		0, 0, "Database", SKIP_OPEN_TABLE},
	{"TRANSACTIONS",	4, MYSQL_TYPE_LONG,			0, 0, "Transactions", SKIP_OPEN_TABLE},
	{"BLOCKS",			8, MYSQL_TYPE_LONGLONG,		0, 0, "Blocks", SKIP_OPEN_TABLE},
	{"WINDOWS",			4, MYSQL_TYPE_LONG,			0, 0, "Windows", SKIP_OPEN_TABLE},
	{"BUFFERS",			4, MYSQL_TYPE_LONG,			0, 0, "Buffers", SKIP_OPEN_TABLE},
	{0,					0, MYSQL_TYPE_STRING,		0, 0, 0, SKIP_OPEN_TABLE}
};

int NfsPluginHandler::initSerialLogInfo(void *p)
{
	DBUG_ENTER("initSerialLogInfoInfo");
	ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
	schema->fields_info = serialSerialLogFieldInfo;
	schema->fill_table = NfsPluginHandler::callSerialLogInfo;

	DBUG_RETURN(0);
}

int NfsPluginHandler::deinitSerialLogInfo(void *p)
{
	DBUG_ENTER("deinitSerialLogInfo");
	DBUG_RETURN(0);
}


//*****************************************************************************
//
// Sync Information
//
//*****************************************************************************

int NfsPluginHandler::callSyncInfo(THD *thd, TABLE_LIST *tables, COND *cond)
{
	InfoTableImpl infoTable(thd, tables, system_charset_info);

	if (storageHandler)
		storageHandler->getSyncInfo(&infoTable);

	return infoTable.error;
}

ST_FIELD_INFO syncInfoFieldInfo[]=
{
	{"CALLER",			120, MYSQL_TYPE_STRING,		0, 0, "Caller", SKIP_OPEN_TABLE},
	{"SHARED",			4, MYSQL_TYPE_LONG,			0, 0, "Shared", SKIP_OPEN_TABLE},
	{"EXCLUSIVE",		4, MYSQL_TYPE_LONG,			0, 0, "Exclusive", SKIP_OPEN_TABLE},
	{"WAITS",			4, MYSQL_TYPE_LONG,			0, 0, "Waits", SKIP_OPEN_TABLE},
	{"QUEUE_LENGTH",	4, MYSQL_TYPE_LONG,			0, 0, "Queue Length", SKIP_OPEN_TABLE},
	{0,					0, MYSQL_TYPE_STRING,		0, 0, 0, SKIP_OPEN_TABLE}
};

int NfsPluginHandler::initSyncInfo(void *p)
{
	DBUG_ENTER("initSyncInfo");
	ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
	schema->fields_info = syncInfoFieldInfo;
	schema->fill_table = NfsPluginHandler::callSyncInfo;

	DBUG_RETURN(0);
}

int NfsPluginHandler::deinitSyncInfo(void *p)
{
	DBUG_ENTER("deinitSyncInfo");
	DBUG_RETURN(0);
}

static void updateIndexChillThreshold(MYSQL_THD thd,
                                      struct st_mysql_sys_var *var,
                                      void *var_ptr, void *save)
{
	// TBD; Apply this setting to all configurations associated with 'thd'.
	//uint newFalconIndexChillThreshold = *((uint *) save);
}

static void updateRecordChillThreshold(MYSQL_THD thd,
                                      struct st_mysql_sys_var *var,
                                      void *var_ptr, void *save)
{
	// TBD; Apply this setting to all configurations associated with 'thd'.
	//uint newFalconRecordChillThreshold = *((uint *) save);
}


void StorageInterface::updateFsyncDisable(MYSQL_THD thd, struct st_mysql_sys_var* variable, void *var_ptr, void *save)
{
	falcon_disable_fsync = *(my_bool*) save;

	if (storageHandler)
		storageHandler->setSyncDisable(falcon_disable_fsync);
}

void StorageInterface::updateRecordMemoryMax(MYSQL_THD thd, struct st_mysql_sys_var* variable, void* var_ptr, void* save)
{
	falcon_record_memory_max = *(unsigned long long*) save;

	if (storageHandler)
		storageHandler->setRecordMemoryMax(falcon_record_memory_max);
}

void StorageInterface::updateRecordScavengeThreshold(MYSQL_THD thd, struct st_mysql_sys_var* variable, void* var_ptr, void* save)
{
	falcon_record_scavenge_threshold = *(int*) save;

	if (storageHandler)
		storageHandler->setRecordScavengeThreshold(falcon_record_scavenge_threshold);
}

void StorageInterface::updateRecordScavengeFloor(MYSQL_THD thd, struct st_mysql_sys_var* variable, void* var_ptr, void* save)
{
	falcon_record_scavenge_floor = *(int*) save;

	if (storageHandler)
		storageHandler->setRecordScavengeFloor(falcon_record_scavenge_floor);
}


static MYSQL_SYSVAR_BOOL(debug_server, falcon_debug_server,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Enable Falcon debug code.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(disable_fsync, falcon_disable_fsync,
  PLUGIN_VAR_NOCMDARG, // | PLUGIN_VAR_READONLY,
  "Disable periodic fsync().",
  NULL, StorageInterface::updateFsyncDisable, FALSE);

static MYSQL_SYSVAR_STR(serial_log_dir, falcon_serial_log_dir,
  PLUGIN_VAR_RQCMDARG| PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
  "Falcon serial log file directory.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(checkpoint_schedule, falcon_checkpoint_schedule,
  PLUGIN_VAR_RQCMDARG| PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
  "Falcon checkpoint schedule.",
  NULL, NULL, "7 * * * * *");

static MYSQL_SYSVAR_STR(scavenge_schedule, falcon_scavenge_schedule,
  PLUGIN_VAR_RQCMDARG| PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
  "Falcon record scavenge schedule.",
  NULL, NULL, "15,45 * * * * *");

/***
static MYSQL_SYSVAR_UINT(debug_mask, falcon_debug_mask,
  PLUGIN_VAR_RQCMDARG,
  "Falcon message type mask for logged messages.",
  NULL, NULL, 0, 0, INT_MAX, 0);
***/

// #define MYSQL_SYSVAR_UINT(name, varname, opt, comment, check, update, def, min, max, blk)

#define PARAMETER(name, text, min, deflt, max, flags, function) \
	static MYSQL_SYSVAR_UINT(name, falcon_##name, \
	PLUGIN_VAR_RQCMDARG | flags, text, function, NULL, deflt, min, max, 0);
#include "StorageParameters.h"
#undef PARAMETER

static MYSQL_SYSVAR_ULONGLONG(record_memory_max, falcon_record_memory_max,
  PLUGIN_VAR_RQCMDARG, // | PLUGIN_VAR_READONLY,
  "The maximum size of the record memory cache.",
  NULL, StorageInterface::updateRecordMemoryMax, LL(250)<<20, 0, (ulonglong) ~0, LL(1)<<20);

static MYSQL_SYSVAR_UINT(record_scavenge_threshold, falcon_record_scavenge_threshold,
  PLUGIN_VAR_OPCMDARG, // | PLUGIN_VAR_READONLY,
  "The percentage of falcon_record_memory_max that will cause the scavenger thread to start scavenging records from the record cache.",
  NULL, StorageInterface::updateRecordScavengeThreshold, 67, 10, 100, 1);

static MYSQL_SYSVAR_UINT(record_scavenge_floor, falcon_record_scavenge_floor,
  PLUGIN_VAR_OPCMDARG, // | PLUGIN_VAR_READONLY,
  "A percentage of falcon_record_memory_threshold that defines the amount of record data that will remain in the record cache after a scavenge run.",
  NULL, StorageInterface::updateRecordScavengeFloor, 50, 10, 90, 1);

static MYSQL_SYSVAR_ULONGLONG(initial_allocation, falcon_initial_allocation,
  PLUGIN_VAR_RQCMDARG, // | PLUGIN_VAR_READONLY,
  "Initial allocation (in bytes) of falcon user tablespace.",
  NULL, NULL, 0, 0, LL(4000000000), LL(1)<<20);

/***
static MYSQL_SYSVAR_UINT(allocation_extent, falcon_allocation_extent,
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
  "The percentage of the current size of falcon_user.fts to use as the size of the next extension to the file.",
  NULL, NULL, 10, 0, 100, 1);
***/

static MYSQL_SYSVAR_ULONGLONG(page_cache_size, falcon_page_cache_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The amount of memory to be used for the database page cache.",
  NULL, NULL, LL(4)<<20, LL(2)<<20, (ulonglong) ~0, LL(1)<<20);

static MYSQL_SYSVAR_UINT(page_size, falcon_page_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The page size used when creating a Falcon tablespace.",
  NULL, NULL, 4096, 1024, 32768, 1024);

static MYSQL_SYSVAR_UINT(serial_log_buffers, falcon_serial_log_buffers,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The number of buffers allocated for Falcon serial log.",
  NULL, NULL, 10, 10, 32768, 10);

static MYSQL_SYSVAR_UINT(index_chill_threshold, falcon_index_chill_threshold,
  PLUGIN_VAR_RQCMDARG,
  "Mbytes of pending index data that is 'frozen' to the Falcon serial log.",
  NULL, &updateIndexChillThreshold, 4, 1, 1024, 1);

static MYSQL_SYSVAR_UINT(record_chill_threshold, falcon_record_chill_threshold,
  PLUGIN_VAR_RQCMDARG,
  "Mbytes of pending record data that is 'frozen' to the Falcon serial log.",
  NULL, &updateRecordChillThreshold, 5, 1, 1024, 1);

static MYSQL_SYSVAR_UINT(max_transaction_backlog, falcon_max_transaction_backlog,
  PLUGIN_VAR_RQCMDARG,
  "Maximum number of backlogged transactions.",
  NULL, NULL, 150, 1, 1000000, 1);

static struct st_mysql_sys_var* falconVariables[]= {
#define PARAMETER(name, text, min, deflt, max, flags, function) MYSQL_SYSVAR(name),
#include "StorageParameters.h"
#undef PARAMETER
	MYSQL_SYSVAR(debug_server),
	MYSQL_SYSVAR(disable_fsync),
	MYSQL_SYSVAR(serial_log_dir),
	MYSQL_SYSVAR(checkpoint_schedule),
	MYSQL_SYSVAR(scavenge_schedule),
	//MYSQL_SYSVAR(debug_mask),
	MYSQL_SYSVAR(record_memory_max),
	MYSQL_SYSVAR(record_scavenge_floor),
	MYSQL_SYSVAR(record_scavenge_threshold),
	MYSQL_SYSVAR(initial_allocation),
	//MYSQL_SYSVAR(allocation_extent),
	MYSQL_SYSVAR(page_cache_size),
	MYSQL_SYSVAR(page_size),
	MYSQL_SYSVAR(serial_log_buffers),
	MYSQL_SYSVAR(index_chill_threshold),
	MYSQL_SYSVAR(record_chill_threshold),
	MYSQL_SYSVAR(max_transaction_backlog),
	NULL
};

static st_mysql_storage_engine falcon_storage_engine			=	{ MYSQL_HANDLERTON_INTERFACE_VERSION};
static st_mysql_information_schema falcon_system_memory_detail	=	{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema falcon_system_memory_summary =	{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema falcon_record_cache_detail	=	{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema falcon_record_cache_summary	=	{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema falcon_database_io			=	{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema falcon_transaction_info		=	{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema falcon_transaction_summary_info=	{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema falcon_sync_info				=	{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema falcon_serial_log_info		=	{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema falcon_tables				=	{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};

mysql_declare_plugin(falcon)
	{
	MYSQL_STORAGE_ENGINE_PLUGIN,
	&falcon_storage_engine,
	falcon_hton_name,
	"MySQL AB",
	"Falcon storage engine",
	PLUGIN_LICENSE_GPL,
	StorageInterface::falcon_init,				/* plugin init */
	StorageInterface::falcon_deinit,				/* plugin deinit */
	0x0100,										/* 1.0 */
	falconStatus,								/* status variables */
	falconVariables,							/* system variables */
	NULL										/* config options */
	},

	{
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&falcon_system_memory_detail,
	"FALCON_SYSTEM_MEMORY_DETAIL",
	"MySQL AB",
	"Falcon System Memory Detail.",
	PLUGIN_LICENSE_GPL,
	NfsPluginHandler::initSystemMemoryDetail,	/* plugin init */
	NfsPluginHandler::deinitSystemMemoryDetail,	/* plugin deinit */
	0x0005,
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options */
	},

	{
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&falcon_system_memory_summary,
	"FALCON_SYSTEM_MEMORY_SUMMARY",
	"MySQL AB",
	"Falcon System Memory Summary.",
	PLUGIN_LICENSE_GPL,
	NfsPluginHandler::initSystemMemorySummary,	/* plugin init */
	NfsPluginHandler::deinitSystemMemorySummary,/* plugin deinit */
	0x0005,
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options */
	},

	{
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&falcon_record_cache_detail,
	"FALCON_RECORD_CACHE_DETAIL",
	"MySQL AB",
	"Falcon Record Cache Detail.",
	PLUGIN_LICENSE_GPL,
	NfsPluginHandler::initRecordCacheDetail,	/* plugin init */
	NfsPluginHandler::deinitRecordCacheDetail,	/* plugin deinit */
	0x0005,
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options */
	},

	{
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&falcon_record_cache_summary,
	"FALCON_RECORD_CACHE_SUMMARY",
	"MySQL AB",
	"Falcon Record Cache Summary.",
	PLUGIN_LICENSE_GPL,
	NfsPluginHandler::initRecordCacheSummary,	/* plugin init */
	NfsPluginHandler::deinitRecordCacheSummary,	/* plugin deinit */
	0x0005,
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options   */
	},

	{
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&falcon_transaction_info,
	"FALCON_TRANSACTIONS",
	"MySQL AB",
	"Falcon Transactions.",
	PLUGIN_LICENSE_GPL,
	NfsPluginHandler::initTransactionInfo,		/* plugin init */
	NfsPluginHandler::deinitTransactionInfo,	/* plugin deinit */
	0x0005,
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options   */
	},

	{
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&falcon_transaction_summary_info,
	"FALCON_TRANSACTION_SUMMARY",
	"MySQL AB",
	"Falcon Transaction Summary.",
	PLUGIN_LICENSE_GPL,
	NfsPluginHandler::initTransactionSummaryInfo,		/* plugin init */
	NfsPluginHandler::deinitTransactionSummaryInfo,	/* plugin deinit */
	0x0005,
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options   */
	},

	{
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&falcon_sync_info,
	"FALCON_SYNCOBJECTS",
	"MySQL AB",
	"Falcon SyncObjects.",
	PLUGIN_LICENSE_GPL,
	NfsPluginHandler::initSyncInfo,				/* plugin init */
	NfsPluginHandler::deinitSyncInfo,			/* plugin deinit */
	0x0005,
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options   */
	},

	{
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&falcon_serial_log_info,
	"FALCON_SERIAL_LOG_INFO",
	"MySQL AB",
	"Falcon Serial Log Information.",
	PLUGIN_LICENSE_GPL,
	NfsPluginHandler::initSerialLogInfo,		/* plugin init */
	NfsPluginHandler::deinitSerialLogInfo,		/* plugin deinit */
	0x0005,
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options   */
	},

	{
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&falcon_database_io,
	"FALCON_DATABASE_IO",
	"MySQL AB",
	"Falcon Database IO.",
	PLUGIN_LICENSE_GPL,
	NfsPluginHandler::initDatabaseIO,			/* plugin init */
	NfsPluginHandler::deinitDatabaseIO,			/* plugin deinit */
	0x0005,
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options   */
	},

	{
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&falcon_tables,
	"FALCON_TABLES",
	"MySQL AB",
	"Falcon Tables.",
	PLUGIN_LICENSE_GPL,
	NfsPluginHandler::initTablesInfo,			/* plugin init */
	NfsPluginHandler::deinitTablesInfo,			/* plugin deinit */
	0x0005,
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options   */
	}

mysql_declare_plugin_end;
