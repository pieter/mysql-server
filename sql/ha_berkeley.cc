/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/*
  TODO:
  - Not compressed keys should use cmp_fix_length_key
  - Don't automaticly pack all string keys (To do this we need to modify
    CREATE TABLE so that one can use the pack_keys argument per key).
  - An argument to pack_key that we don't want compression.
  - Interaction with LOCK TABLES (Monty will fix this)
  - DB_DBT_USERMEN should be used for fixed length tables
    We will need an updated Berkeley DB version for this.
  - Killing threads that has got a 'deadlock'
  - SHOW TABLE STATUS should give more information about the table.
  - Get a more accurate count of the number of rows.
  - Introduce hidden primary keys for tables without a primary key
  - We will need a manager thread that calls flush_logs, removes old
    logs and makes checkpoints at given intervals.
  - When not using UPDATE IGNORE, don't make a sub transaction but abort
    the main transaction on errors.
  - Handling of drop table during autocommit=0 ?
    (Should we just give an error in this case if there is a pending
    transaction ?)
  - When using ALTER TABLE IGNORE, we should not start an transaction, but do
    everything wthout transactions.

  Testing of:
  - ALTER TABLE
  - LOCK TABLES
  - CHAR keys
  - BLOBS
  - delete from t1;
*/


#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#ifdef HAVE_BERKELEY_DB
#include <m_ctype.h>
#include <myisampack.h>
#include <assert.h>
#include <hash.h>
#include "ha_berkeley.h"

#define HA_BERKELEY_ROWS_IN_TABLE 10000 /* to get optimization right */
#define HA_BERKELEY_RANGE_COUNT	  100

const char *ha_berkeley_ext=".db";
bool berkeley_skip=0;
u_int32_t berkeley_init_flags=0,berkeley_lock_type=DB_LOCK_DEFAULT;
ulong berkeley_cache_size;
char *berkeley_home, *berkeley_tmpdir, *berkeley_logdir;
long berkeley_lock_scan_time=0;
ulong berkeley_trans_retry=5;
pthread_mutex_t bdb_mutex;

static DB_ENV *db_env;
static HASH bdb_open_tables;

const char *berkeley_lock_names[] =
{ "DEFAULT", "OLDEST","RANDOM","YOUNGEST" };
u_int32_t berkeley_lock_types[]=
{ DB_LOCK_DEFAULT, DB_LOCK_OLDEST, DB_LOCK_RANDOM };
TYPELIB berkeley_lock_typelib= {array_elements(berkeley_lock_names),"",
				berkeley_lock_names};

static void berkeley_print_error(const char *db_errpfx, char *buffer);
static byte* bdb_get_key(BDB_SHARE *share,uint *length,
			 my_bool not_used __attribute__((unused)));
static BDB_SHARE *get_share(const char *table_name);
static void free_share(BDB_SHARE *share);


/* General functions */

bool berkeley_init(void)
{
  char buff[1024],*config[10], **conf_pos, *str_pos;
  conf_pos=config; str_pos=buff;
  DBUG_ENTER("berkeley_init");

  if (!berkeley_tmpdir)
    berkeley_tmpdir=mysql_tmpdir;
  if (!berkeley_home)
    berkeley_home=mysql_real_data_home;

  if (db_env_create(&db_env,0))
    DBUG_RETURN(1);
  db_env->set_errcall(db_env,berkeley_print_error);
  db_env->set_errpfx(db_env,"bdb");
  db_env->set_tmp_dir(db_env, berkeley_tmpdir);
  db_env->set_data_dir(db_env, mysql_data_home);
  if (berkeley_logdir)
    db_env->set_lg_dir(db_env, berkeley_logdir);

  if (opt_endinfo)
    db_env->set_verbose(db_env,
			DB_VERB_CHKPOINT | DB_VERB_DEADLOCK | DB_VERB_RECOVERY,
			1);
  
  db_env->set_cachesize(db_env, 0, berkeley_cache_size, 0);
  db_env->set_lk_detect(db_env, berkeley_lock_type);
  if (db_env->open(db_env,
		   berkeley_home,
		   berkeley_init_flags |  DB_INIT_LOCK | 
		   DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN |
		   DB_CREATE | DB_THREAD | DB_PRIVATE, 0666))
  {
    db_env->close(db_env,0);
    db_env=0;
  }
  (void) hash_init(&bdb_open_tables,32,0,0,
		   (hash_get_key) bdb_get_key,0,0);
  pthread_mutex_init(&bdb_mutex,NULL);
  DBUG_RETURN(db_env == 0);
}


bool berkeley_end(void)
{
  int error;
  DBUG_ENTER("berkeley_end");
  if (!db_env)
    return 1;
  error=db_env->close(db_env,0);		// Error is logged
  db_env=0;
  hash_free(&bdb_open_tables);
  pthread_mutex_destroy(&bdb_mutex);
  DBUG_RETURN(error != 0);
}

bool berkeley_flush_logs()
{
  int error;
  bool result=0;
  DBUG_ENTER("berkeley_flush_logs");
  if ((error=log_flush(db_env,0)))
  {
    my_error(ER_ERROR_DURING_FLUSH_LOGS,MYF(0),error);
    result=1;
  }
  if ((error=txn_checkpoint(db_env,0,0,0)))
  {
    my_error(ER_ERROR_DURING_CHECKPOINT,MYF(0),error);
    result=1;
  }
  DBUG_RETURN(result);
}


int berkeley_commit(THD *thd)
{
  DBUG_ENTER("berkeley_commit");
  DBUG_PRINT("trans",("ending transaction"));
  int error=txn_commit((DB_TXN*) thd->transaction.bdb_tid,0);
#ifndef DBUG_OFF
  if (error)
    DBUG_PRINT("error",("error: %d",error));
#endif
  thd->transaction.bdb_tid=0;
  DBUG_RETURN(error);
}

int berkeley_rollback(THD *thd)
{
  DBUG_ENTER("berkeley_rollback");
  DBUG_PRINT("trans",("aborting transaction"));
  int error=txn_abort((DB_TXN*) thd->transaction.bdb_tid);
  thd->transaction.bdb_tid=0;
  DBUG_RETURN(error);
}


static void berkeley_print_error(const char *db_errpfx, char *buffer)
{
  sql_print_error("%s:  %s",db_errpfx,buffer);
}



/*****************************************************************************
** Berkeley DB tables
*****************************************************************************/

const char **ha_berkeley::bas_ext() const
{ static const char *ext[]= { ha_berkeley_ext, NullS }; return ext; }


static int
berkeley_cmp_packed_key(const DBT *new_key, const DBT *saved_key)
{
  KEY *key=	      (KEY*) new_key->app_private;
  char *new_key_ptr=  (char*) new_key->data;
  char *saved_key_ptr=(char*) saved_key->data;
  KEY_PART_INFO *key_part= key->key_part, *end=key_part+key->key_parts;
  uint key_length=new_key->size;

  for ( ; key_part != end && (int) key_length > 0; key_part++)
  {
    int cmp;
    if (key_part->null_bit)
    {
      if (*new_key_ptr++ != *saved_key_ptr++)
	return ((int) new_key_ptr[-1] - (int) saved_key_ptr[-1]);
    }
    if ((cmp=key_part->field->pack_cmp(new_key_ptr,saved_key_ptr,
				       key_part->length)))
      return cmp;
    uint length=key_part->field->packed_col_length(new_key_ptr);
    new_key_ptr+=length;
    key_length-=length;
    saved_key_ptr+=key_part->field->packed_col_length(saved_key_ptr);
  }
  return key->handler.bdb_return_if_eq;
}


static int
berkeley_cmp_fix_length_key(const DBT *new_key, const DBT *saved_key)
{
  KEY *key=(KEY*) (new_key->app_private);
  char *new_key_ptr=  (char*) new_key->data;
  char *saved_key_ptr=(char*) saved_key->data;
  KEY_PART_INFO *key_part= key->key_part, *end=key_part+key->key_parts;
  uint key_length=new_key->size;

  for ( ; key_part != end && (int) key_length > 0 ; key_part++)
  {
    int cmp;
    if ((cmp=key_part->field->pack_cmp(new_key_ptr,saved_key_ptr,0)))
      return cmp;
    new_key_ptr+=key_part->length;
    key_length-= key_part->length;
    saved_key_ptr+=key_part->length;
  }
  return key->handler.bdb_return_if_eq;
}


int ha_berkeley::open(const char *name, int mode, int test_if_locked)
{
  char name_buff[FN_REFLEN];
  uint open_mode=(mode == O_RDONLY ? DB_RDONLY : 0) | DB_THREAD;
  int error;
  DBUG_ENTER("ha_berkeley::open");

  /* Need some extra memory in case of packed keys */
  uint max_key_length= table->max_key_length + MAX_REF_PARTS*2;
  if (!(alloc_ptr=
	my_multi_malloc(MYF(MY_WME),
			&key_file, table->keys*sizeof(*key_file),
			&key_type, table->keys*sizeof(u_int32_t),
			&key_buff,  max_key_length,
			&key_buff2, max_key_length,
			&primary_key_buff,
			table->key_info[table->primary_key].key_length,
			NullS)))
    DBUG_RETURN(1);
  if (!(rec_buff=my_malloc((alloced_rec_buff_length=table->reclength),
			   MYF(MY_WME))))
  {
    my_free(alloc_ptr,MYF(0));
    DBUG_RETURN(1);
  }

  /* Init table lock structure */
  if (!(share=get_share(name)))
  {
    my_free(rec_buff,MYF(0));
    my_free(alloc_ptr,MYF(0));
    DBUG_RETURN(1);
  }
  thr_lock_data_init(&share->lock,&lock,(void*) 0);

  if ((error=db_create(&file, db_env, 0)))
  {
    free_share(share);
    my_free(rec_buff,MYF(0));
    my_free(alloc_ptr,MYF(0));
    my_errno=error;
    DBUG_RETURN(1);
  }

  /* Open primary key */
  file->set_bt_compare(file, berkeley_cmp_packed_key);
  if ((error=(file->open(file, fn_format(name_buff,name,"", ha_berkeley_ext,
					 2 | 4),
			 "main", DB_BTREE, open_mode,0))))
  {
    free_share(share);
    my_free(rec_buff,MYF(0));
    my_free(alloc_ptr,MYF(0));
    my_errno=error;
    DBUG_RETURN(1);
  }

  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  transaction=0;
  cursor=0;

  fixed_length_row=!(table->db_create_options & HA_OPTION_PACK_RECORD);

  /* Open other keys */
  bzero((char*) key_file,sizeof(*key_file)*table->keys);
  key_used_on_scan=primary_key=table->primary_key;
  key_file[primary_key]=file;
  bzero((char*) &current_row,sizeof(current_row));

  DB **ptr=key_file;
  for (uint i=0, used_keys=0; i < table->keys ; i++, ptr++)
  {
    char part[7];
    key_type[i]=table->key_info[i].flags & HA_NOSAME ? DB_NOOVERWRITE : 0;
    if (i != primary_key)
    {
      if ((error=db_create(ptr, db_env, 0)))
      {
	close();
	my_errno=error;
	DBUG_RETURN(1);
      }
      sprintf(part,"key%02d",++used_keys);
      (*ptr)->set_bt_compare(*ptr, berkeley_cmp_packed_key);
      if (!(table->key_info[i].flags & HA_NOSAME))
	(*ptr)->set_flags(*ptr, DB_DUP);
      if ((error=((*ptr)->open(*ptr, name_buff, part, DB_BTREE,
			       open_mode, 0))))
      {
	close();
	my_errno=error;
	DBUG_RETURN(1);
      }
    }
  }
  DBUG_RETURN(0);
}


void ha_berkeley::initialize(void)
{
  /* Calculate pack_length of primary key */
  ref_length=0;
  KEY_PART_INFO *key_part= table->key_info[primary_key].key_part;
  KEY_PART_INFO *end=key_part+table->key_info[primary_key].key_parts;
  for ( ; key_part != end ; key_part++)
    ref_length+= key_part->field->max_packed_col_length(key_part->length);
  fixed_length_primary_key=
    (ref_length == table->key_info[primary_key].key_length);
}    

int ha_berkeley::close(void)
{
  int error,result=0;
  DBUG_ENTER("ha_berkeley::close");

  for (uint i=0; i < table->keys; i++)
  {
    if (key_file[i] && (error=key_file[i]->close(key_file[i],0)))
      result=error;
  }
  free_share(share);
  my_free(rec_buff,MYF(MY_ALLOW_ZERO_PTR));
  my_free(alloc_ptr,MYF(MY_ALLOW_ZERO_PTR));
  if (result)
    my_errno=result;
  DBUG_RETURN(result);
}


/* Reallocate buffer if needed */

bool ha_berkeley::fix_rec_buff_for_blob(ulong length)
{
  uint extra;
  if (! rec_buff || length > alloced_rec_buff_length)
  {
    byte *newptr;
    if (!(newptr=(byte*) my_realloc((gptr) rec_buff, length,
				    MYF(MY_ALLOW_ZERO_PTR))))
      return 1;
    rec_buff=newptr;
    alloced_rec_buff_length=length;
  }
  return 0;
}


/* Calculate max length needed for row */

ulong ha_berkeley::max_row_length(const byte *buf)
{
  ulong length=table->reclength + table->fields*2;
  for (Field_blob **ptr=table->blob_field ; *ptr ; ptr++)
    length+= (*ptr)->get_length(buf+(*ptr)->offset())+2;
  return length;
}


/*
  Pack a row for storage.  If the row is of fixed length, just store the
  row 'as is'.
  If not, we will generate a packed row suitable for storage.
  This will only fail if we don't have enough memory to pack the row, which;
  may only happen in rows with blobs,  as the default row length is
  pre-allocated.
*/

int ha_berkeley::pack_row(DBT *row, const byte *record)
{
  bzero((char*) row,sizeof(*row));
  if (fixed_length_row)
  {
    row->data=(void*) record;
    row->size=table->reclength;
    return 0;
  }
  if (table->blob_fields)
  {
    if (fix_rec_buff_for_blob(max_row_length(record)))
      return HA_ERR_OUT_OF_MEM;
  }

  /* Copy null bits */
  memcpy(rec_buff, record, table->null_bytes);
  byte *ptr=rec_buff + table->null_bytes;

  for (Field **field=table->field ; *field ; field++)
    ptr=(byte*) (*field)->pack((char*) ptr,record + (*field)->offset());
  row->data=rec_buff;
  row->size= (size_t) (ptr - rec_buff);
  return 0;
}


void ha_berkeley::unpack_row(char *record, DBT *row)
{
  if (fixed_length_row)
    memcpy(record,row->data,table->reclength);
  else
  {
    /* Copy null bits */
    const char *ptr= (const char*) row->data;
    memcpy(record, ptr, table->null_bytes);
    ptr+=table->null_bytes;
    for (Field **field=table->field ; *field ; field++)
      ptr= (*field)->unpack(record + (*field)->offset(), ptr);
  }
}


/*
  Create a packed key from from a row
  This will never fail as the key buffer is pre allocated.
*/

DBT *ha_berkeley::pack_key(DBT *key, uint keynr, char *buff,
			   const byte *record)
{
  KEY *key_info=table->key_info+keynr;
  KEY_PART_INFO *key_part=key_info->key_part;
  KEY_PART_INFO *end=key_part+key_info->key_parts;
  DBUG_ENTER("pack_key");

  bzero((char*) key,sizeof(*key));
  key->data=buff;
  key->app_private= key_info;

  for ( ; key_part != end ; key_part++)
  {
    if (key_part->null_bit)
    {
      /* Store 0 if the key part is a NULL part */
      if (record[key_part->null_offset] & key_part->null_bit)
      {
	*buff++ =0;
	key->flags|=DB_DBT_DUPOK;
	continue;
      }
      *buff++ = 1;				// Store NOT NULL marker
    }
    buff=key_part->field->pack(buff,record + key_part->offset,
			       key_part->length);
  }
  key->size= (buff  - (char*) key->data);
  DBUG_DUMP("key",(char*) key->data, key->size);
  DBUG_RETURN(key);
}


/*
  Create a packed key from from a MySQL unpacked key
*/

DBT *ha_berkeley::pack_key(DBT *key, uint keynr, char *buff,
			   const byte *key_ptr, uint key_length)
{
  KEY *key_info=table->key_info+keynr;
  KEY_PART_INFO *key_part=key_info->key_part;
  KEY_PART_INFO *end=key_part+key_info->key_parts;
  DBUG_ENTER("pack_key2");

  bzero((char*) key,sizeof(*key));
  key->data=buff;
  key->app_private= key_info;

  for (; key_part != end && (int) key_length > 0 ; key_part++)
  {
    uint offset=0;
    if (key_part->null_bit)
    {
      offset=1;
      if (!(*buff++ = (*key_ptr == 0)))		// Store 0 if NULL
      {
	key_length-= key_part->store_length;
	key_ptr+=   key_part->store_length;
	key->flags|=DB_DBT_DUPOK;
	continue;
      }
      key_ptr++;
    }	
    buff=key_part->field->keypack(buff,key_ptr+offset,key_part->length);
    key_ptr+=key_part->store_length;
    key_length-=key_part->store_length;
  }
  key->size= (buff  - (char*) key->data);
  DBUG_DUMP("key",(char*) key->data, key->size);
  DBUG_RETURN(key);
}


int ha_berkeley::write_row(byte * record)
{
  DBT row,prim_key,key;
  int error;
  DBUG_ENTER("write_row");

  statistic_increment(ha_write_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(record+table->time_stamp-1);
  if (table->next_number_field && record == table->record[0])
    update_auto_increment();
  if ((error=pack_row(&row, record)))
    DBUG_RETURN(error);

  if (table->keys == 1)
  {
    error=file->put(file, transaction, pack_key(&prim_key, primary_key,
						key_buff, record),
		    &row, key_type[primary_key]);
  }
  else
  {
    for (uint retry=0 ; retry < berkeley_trans_retry ; retry++)
    {
      uint keynr;
      DB_TXN *sub_trans;
      if ((error=txn_begin(db_env, transaction, &sub_trans, 0)))
	break;
      DBUG_PRINT("trans",("starting subtransaction"));
      if (!(error=file->put(file, sub_trans, pack_key(&prim_key, primary_key,
						      key_buff, record),
			    &row, key_type[primary_key])))
      {
	for (keynr=0 ; keynr < table->keys ; keynr++)
	{
	  if (keynr == primary_key)
	    continue;
	  if ((error=key_file[keynr]->put(key_file[keynr], sub_trans,
					  pack_key(&key, keynr, key_buff2,
						   record),
					  &prim_key, key_type[keynr])))
	  {
	    last_dup_key=keynr;
	    break;
	  }
	}
      }
      if (!error)
      {
	DBUG_PRINT("trans",("committing subtransaction"));
	error=txn_commit(sub_trans, 0);
      }
      else
      {
	/* Remove inserted row */
	int new_error;
	DBUG_PRINT("error",("Got error %d",error));
	DBUG_PRINT("trans",("aborting subtransaction"));
	if ((new_error=txn_abort(sub_trans)))
	{
	  error=new_error;			// This shouldn't happen
	  break;
	}
      }
      if (error != DB_LOCK_DEADLOCK)
	break;
    }
  }
  if (error == DB_KEYEXIST)
    error=HA_ERR_FOUND_DUPP_KEY;
  DBUG_RETURN(error);
}


/* Compare if a key in a row has changed */

int ha_berkeley::key_cmp(uint keynr, const byte * old_row,
			 const byte * new_row)
{
  KEY_PART_INFO *key_part=table->key_info[keynr].key_part;
  KEY_PART_INFO *end=key_part+table->key_info[keynr].key_parts;

  for ( ; key_part != end ; key_part++)
  {
    if (key_part->null_bit)
    {
      if ((old_row[key_part->null_offset] & key_part->null_bit) !=
	  (new_row[key_part->null_offset] & key_part->null_bit))
	return 1;
    }
    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH))
    {
      
      if (key_part->field->cmp_binary(old_row + key_part->offset,
				      new_row + key_part->offset,
				      (ulong) key_part->length))
	return 1;
    }
    else
    {
      if (memcmp(old_row+key_part->offset, new_row+key_part->offset,
		 key_part->length))
	return 1;
    }
  }
  return 0;
}


/*
  Update a row from one value to another.
*/

int ha_berkeley::update_primary_key(DB_TXN *trans, bool primary_key_changed,
				    const byte * old_row,
				    const byte * new_row, DBT *prim_key)
{
  DBT row, old_key;
  int error,new_error;
  DBUG_ENTER("update_primary_key");

  if (primary_key_changed)
  {
    // Primary key changed or we are updating a key that can have duplicates.
    // Delete the old row and add a new one
    pack_key(&old_key, primary_key, key_buff2, old_row);
    if ((error=remove_key(trans, primary_key, old_row, (DBT *) 0, &old_key)))
      DBUG_RETURN(error);			// This should always succeed
    if ((error=pack_row(&row, new_row)))
    {
      // Out of memory (this shouldn't happen!) 
      (void) file->put(file, trans, &old_key, &row,
		       key_type[primary_key]);
      DBUG_RETURN(error);
    }
    // Write new key
    if ((error=file->put(file, trans, prim_key, &row, key_type[primary_key])))
    {
      // Probably a duplicated key;  Return the error and let the caller
      // abort.
      last_dup_key=primary_key;
      DBUG_RETURN(error);
    }
  }
  else
  {
    // Primary key didn't change;  just update the row data
    if ((error=pack_row(&row, new_row)))
      DBUG_RETURN(error);
      error=file->put(file, trans, prim_key, &row, 0);
    if (error)
      DBUG_RETURN(error);				// Fatal error
  }
  DBUG_RETURN(0);
}



int ha_berkeley::update_row(const byte * old_row, byte * new_row)
{
  DBT row, prim_key, key, old_prim_key;
  int error;
  uint keynr;
  DB_TXN *sub_trans;
  bool primary_key_changed;
  DBUG_ENTER("update_row");

  statistic_increment(ha_update_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(new_row+table->time_stamp-1);
  pack_key(&prim_key, primary_key, key_buff, new_row);
  
  if ((primary_key_changed=key_cmp(primary_key, old_row, new_row)))
    pack_key(&old_prim_key, primary_key, primary_key_buff, old_row);
  else
    old_prim_key=prim_key;

  LINT_INIT(error);
  for (uint retry=0 ; retry < berkeley_trans_retry ; retry++)
  {
    if ((error=txn_begin(db_env, transaction, &sub_trans, 0)))
      break;
    DBUG_PRINT("trans",("starting subtransaction"));
    /* Start by updating the primary key */
    if (!(error=update_primary_key(sub_trans, primary_key_changed,
				   old_row, new_row, &prim_key)))
    {
      // Update all other keys
      for (uint keynr=0 ; keynr < table->keys ; keynr++)
      {
	if (keynr == primary_key)
	  continue;
	if (key_cmp(keynr, old_row, new_row) || primary_key_changed)
	{
	  if ((error=remove_key(sub_trans, keynr, old_row, (DBT*) 0,
				&old_prim_key)) ||
	      (error=key_file[keynr]->put(key_file[keynr], sub_trans,
					  pack_key(&key, keynr, key_buff2,
						   new_row),
					  &prim_key, key_type[keynr])))
	  {
	    last_dup_key=keynr;
	    break;
	  }
	}
      }
    }
    if (!error)
    {
      DBUG_PRINT("trans",("committing subtransaction"));
      error=txn_commit(sub_trans, 0);
      }
    else
    {
      /* Remove inserted row */
      int new_error;
      DBUG_PRINT("error",("Got error %d",error));
      DBUG_PRINT("trans",("aborting subtransaction"));
      if ((new_error=txn_abort(sub_trans)))
      {
	error=new_error;			// This shouldn't happen
	break;
      }
    }
    if (error != DB_LOCK_DEADLOCK)
      break;
  }
  if (error == DB_KEYEXIST)
    error=HA_ERR_FOUND_DUPP_KEY;
  DBUG_RETURN(error);
}


/*
  Delete one key
  This uses key_buff2, when keynr != primary key, so it's important that
  a function that calls this doesn't use this buffer for anything else.
  packed_record may be NULL if the key is unique
*/

int ha_berkeley::remove_key(DB_TXN *sub_trans, uint keynr, const byte *record,
			    DBT *packed_record,
			    DBT *prim_key)
{
  int error;
  DBT key;
  DBUG_ENTER("remove_key");
  DBUG_PRINT("enter",("index: %d",keynr));

  if ((table->key_info[keynr].flags & (HA_NOSAME | HA_NULL_PART_KEY)) ==
      HA_NOSAME)
  {						// Unique key
    dbug_assert(keynr == primary_key || prim_key->data != key_buff2);
    error=key_file[keynr]->del(key_file[keynr], sub_trans,
			       keynr == primary_key ?
			       prim_key :
			       pack_key(&key, keynr, key_buff2, record),
			       0);
  }
  else
  {
    /*
      To delete the not duplicated key, we need to open an cursor on the
      row to find the key to be delete and delete it.
      We will never come here with keynr = primary_key
    */
    dbug_assert(keynr != primary_key && prim_key->data != key_buff2);
    DBC *cursor;
    if (!(error=file->cursor(key_file[keynr], sub_trans, &cursor, 0)))
    {
      if (!(error=cursor->c_get(cursor,
			       (keynr == primary_key ? 
				prim_key :
				pack_key(&key, keynr, key_buff2, record)),
			       (keynr == primary_key ? 
				packed_record :  prim_key),
				DB_GET_BOTH)))
      {					// This shouldn't happen
	error=cursor->c_del(cursor,0);
      }
      int result=cursor->c_close(cursor);
      if (!error)
	error=result;
    }
  }
  DBUG_RETURN(error);
}


/* Delete all keys for new_record */

int ha_berkeley::remove_keys(DB_TXN *trans, const byte *record,
			     DBT *new_record, DBT *prim_key, key_map keys,
			     int result)
{
  for (uint keynr=0 ; keys  ;keynr++, keys>>=1)
  {
    if (keys & 1)
    {
      int new_error=remove_key(trans, keynr, record, new_record, prim_key);
      if (new_error)
      {
	result=new_error;			// Return last error
	if (trans)
	  break;				// Let rollback correct things
      }
    }
  }
  return result;
}


int ha_berkeley::delete_row(const byte * record)
{
  int error;
  DBT row, prim_key;
  key_map keys=table->keys_in_use;
  DBUG_ENTER("delete_row");
  statistic_increment(ha_delete_count,&LOCK_status);
    
  if ((error=pack_row(&row, record)))
    DBUG_RETURN((error));
  pack_key(&prim_key, primary_key, key_buff, record);
  for (uint retry=0 ; retry < berkeley_trans_retry ; retry++)
  {
    DB_TXN *sub_trans;
    if ((error=txn_begin(db_env, transaction, &sub_trans, 0)))
      break;
    DBUG_PRINT("trans",("starting sub transaction"));
    if (!error)
      error=remove_keys(sub_trans, record, &row, &prim_key, keys,0);
    if (!error)
    {
      DBUG_PRINT("trans",("ending sub transaction"));
      error=txn_commit(sub_trans, 0);
    }
    if (error)
    {
      /* retry */
      int new_error;
      DBUG_PRINT("error",("Got error %d",error));
      DBUG_PRINT("trans",("aborting subtransaction"));
      if ((new_error=txn_abort(sub_trans)))
      {
	error=new_error;			// This shouldn't happen
	break;
      }
    }
    if (error != DB_LOCK_DEADLOCK)
      break;
  }
  DBUG_RETURN(0);
}


int ha_berkeley::index_init(uint keynr)
{
  int error;
  DBUG_ENTER("index_init");
  active_index=keynr;
  dbug_assert(cursor == 0);
  if ((error=file->cursor(key_file[keynr], transaction, &cursor,
			  table->reginfo.lock_type > TL_WRITE_ALLOW_READ ?
			  0 : 0)))
    cursor=0;					// Safety
  bzero((char*) &last_key,sizeof(last_key));
  DBUG_RETURN(error);
}

int ha_berkeley::index_end()
{
  int error=0;
  DBUG_ENTER("index_end");
  if (cursor)
  {
    error=cursor->c_close(cursor);
    cursor=0;
  }
  DBUG_RETURN(error);
}


/* What to do after we have read a row based on an index */

int ha_berkeley::read_row(int error, char *buf, uint keynr, DBT *row,
			  bool read_next)
{
  DBUG_ENTER("read_row");
  if (error)
  {
    if (error == DB_NOTFOUND || error == DB_KEYEMPTY)
      error=read_next ? HA_ERR_END_OF_FILE : HA_ERR_KEY_NOT_FOUND;
    table->status=STATUS_NOT_FOUND;
    DBUG_RETURN(error);
  }

  if (keynr != primary_key)
  {
    DBT key;
    bzero((char*) &key,sizeof(key));
    key.data=key_buff2;
    key.size=row->size;
    key.app_private=table->key_info+primary_key;
    memcpy(key_buff2,row->data,row->size);
    /* Read the data into current_row */
    current_row.flags=DB_DBT_REALLOC;
    if ((error=file->get(file, transaction, &key, &current_row, 0)))
    {
      table->status=STATUS_NOT_FOUND;
      DBUG_RETURN(error == DB_NOTFOUND ? HA_ERR_CRASHED : error);
    }
    row= &current_row;
  }
  unpack_row(buf,row);
  table->status=0;
  DBUG_RETURN(0);
}


/* This is only used to read whole keys */

int ha_berkeley::index_read_idx(byte * buf, uint keynr, const byte * key,
				uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(ha_read_key_count,&LOCK_status);
  DBUG_ENTER("index_read_idx");
  current_row.flags=DB_DBT_REALLOC;
  DBUG_RETURN(read_row(file->get(key_file[keynr], transaction,
				 pack_key(&last_key, keynr, key_buff, key,
					  key_len),
				 &current_row,0),
		       buf, keynr, &current_row, 0));
}


int ha_berkeley::index_read(byte * buf, const byte * key,
			    uint key_len, enum ha_rkey_function find_flag)
{
  DBT row;
  int error;
  DBUG_ENTER("index_read");
  statistic_increment(ha_read_key_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  if (key_len == table->key_info[active_index].key_length)
  {
    error=read_row(cursor->c_get(cursor, pack_key(&last_key,
						  active_index,
						  key_buff,
						  key, key_len),
				 &row, DB_SET),
		   buf, active_index, &row, 0);
  }
  else
  {
    /* read of partial key */
    pack_key(&last_key, active_index, key_buff, key, key_len);
    /* Store for compare */
    memcpy(key_buff2, key_buff, last_key.size);
    ((KEY*) last_key.app_private)->handler.bdb_return_if_eq= -1;
    error=read_row(cursor->c_get(cursor, &last_key, &row, DB_SET_RANGE),
		   buf, active_index, &row, 0);
    ((KEY*) last_key.app_private)->handler.bdb_return_if_eq=0;
    if (!error && find_flag == HA_READ_KEY_EXACT)
    {
      /* Check that we didn't find a key that wasn't equal to the current
	 one */
      if (!error && ::key_cmp(table, key_buff2, active_index, key_len))
	error=HA_ERR_KEY_NOT_FOUND;
    }
  }
  DBUG_RETURN(error);
}


int ha_berkeley::index_next(byte * buf)
{
  DBT row;
  DBUG_ENTER("index_next");
  statistic_increment(ha_read_next_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT),
		       buf, active_index, &row ,1));
}

int ha_berkeley::index_next_same(byte * buf, const byte *key, uint keylen)
{
  DBT row;
  int error;
  DBUG_ENTER("index_next_same");
  statistic_increment(ha_read_next_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  if (keylen == table->key_info[active_index].key_length)
    error=read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT_DUP),
		   buf, active_index, &row,1);
  else
  {
    error=read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT),
		   buf, active_index, &row,1);
    if (!error && ::key_cmp(table, key, active_index, keylen))
      error=HA_ERR_END_OF_FILE;
  }
  DBUG_RETURN(error);
}


int ha_berkeley::index_prev(byte * buf)
{
  DBT row;
  DBUG_ENTER("index_prev");
  statistic_increment(ha_read_prev_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_PREV),
		       buf, active_index, &row,1));
}
  

int ha_berkeley::index_first(byte * buf)
{
  DBT row;
  DBUG_ENTER("index_first");
  statistic_increment(ha_read_first_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_FIRST),
		       buf, active_index, &row,0));
}

int ha_berkeley::index_last(byte * buf)
{
  DBT row;
  DBUG_ENTER("index_last");
  statistic_increment(ha_read_last_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_LAST),
		  buf, active_index, &row,0));
}

int ha_berkeley::rnd_init(bool scan)
{
  current_row.flags=DB_DBT_REALLOC;
  return index_init(primary_key);
}

int ha_berkeley::rnd_end()
{
  return index_end();
}

int ha_berkeley::rnd_next(byte *buf)
{
  DBT row;
  DBUG_ENTER("rnd_next");
  statistic_increment(ha_read_rnd_next_count,&LOCK_status);
  bzero((char*) &row,sizeof(row));
  DBUG_RETURN(read_row(cursor->c_get(cursor, &last_key, &row, DB_NEXT),
		       buf, active_index, &row, 1));
}


DBT *ha_berkeley::get_pos(DBT *to, byte *pos)
{
  bzero((char*) to,sizeof(*to));

  to->data=pos;
  to->app_private=table->key_info+primary_key;
  if (fixed_length_primary_key)
    to->size=ref_length;
  else
  {
    KEY_PART_INFO *key_part=table->key_info[primary_key].key_part;
    KEY_PART_INFO *end=key_part+table->key_info[primary_key].key_parts;

    for ( ; key_part != end ; key_part++)
      pos+=key_part->field->packed_col_length(pos);
    to->size= (uint) (pos- (byte*) to->data);
  }
  return to;
}


int ha_berkeley::rnd_pos(byte * buf, byte *pos)
{
  DBT db_pos;
  statistic_increment(ha_read_rnd_count,&LOCK_status);

  return read_row(file->get(file, transaction,
			    get_pos(&db_pos, pos),
			    &current_row, 0),
		  buf, active_index, &current_row,0);
}

void ha_berkeley::position(const byte *record)
{
  DBT key;
  pack_key(&key, primary_key, ref, record);
}


void ha_berkeley::info(uint flag)
{
  DBUG_ENTER("info");
  if (flag & HA_STATUS_VARIABLE)
  {
    records = HA_BERKELEY_ROWS_IN_TABLE; // Just to get optimisations right
    deleted = 0;
  }
  else if (flag & HA_STATUS_ERRKEY)
    errkey=last_dup_key;
  DBUG_VOID_RETURN;
}


int ha_berkeley::extra(enum ha_extra_function operation)
{
  return 0;
}

int ha_berkeley::reset(void)
{
  return 0;
}


/*
  As MySQL will execute an external lock for every new table it uses
  we can use this to start the transactions.
*/

int ha_berkeley::external_lock(THD *thd, int lock_type)
{
  int error=0;
  DBUG_ENTER("ha_berkeley::external_lock");
  if (lock_type != F_UNLCK)
  {
    if (!thd->transaction.bdb_lock_count++ && !thd->transaction.bdb_tid)
    {
      /* Found first lock, start transaction */
      DBUG_PRINT("trans",("starting transaction"));
      if ((error=txn_begin(db_env, 0,
			   (DB_TXN**) &thd->transaction.bdb_tid,
			   0)))
	thd->transaction.bdb_lock_count--;
    }
    transaction= (DB_TXN*) thd->transaction.bdb_tid;
  }
  else
  {
    lock.type=TL_UNLOCK;			// Unlocked
    if (current_row.flags & (DB_DBT_MALLOC | DB_DBT_REALLOC))
    {
      current_row.flags=0;
      if (current_row.data)
      {
	free(current_row.data);
	current_row.data=0;
      }
    }
    current_row.data=0;
    if (!--thd->transaction.bdb_lock_count)
    {
      if (thd->transaction.bdb_tid && (thd->options &
				       (OPTION_AUTO_COMMIT | OPTION_BEGIN)))
      {
	/* 
	   F_UNLOCK is done without a transaction commit / rollback. This
	   means that something went wrong.
	   We can in this case silenty abort the transaction.
	*/
	DBUG_PRINT("trans",("aborting transaction"));
	error=txn_abort((DB_TXN*) thd->transaction.bdb_tid);
	thd->transaction.bdb_tid=0;
      }
    }
  }
  DBUG_RETURN(error);
}  


THR_LOCK_DATA **ha_berkeley::store_lock(THD *thd, THR_LOCK_DATA **to,
					enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /* If we are not doing a LOCK TABLE, then allow multiple writers */
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
	 lock_type <= TL_WRITE) &&
	!thd->in_lock_tables)
      lock_type = TL_WRITE_ALLOW_WRITE;
    lock.type=lock_type;
  }
  *to++= &lock;
  return to;
}


static int create_sub_table(const char *table_name, const char *sub_name,
			    DBTYPE type, int flags)
{
  int error,error2;
  DB *file;
  DBUG_ENTER("create_sub_table");
  DBUG_PRINT("enter",("sub_name: %s",sub_name));

  if (!(error=db_create(&file, db_env, 0)))
  {
    file->set_flags(file, flags);
    error=(file->open(file, table_name, sub_name, type,
		      DB_THREAD | DB_CREATE, my_umask));
    if (error)
    {
      DBUG_PRINT("error",("Got error: %d when opening table '%s'",error,
			  table_name));
      (void) file->remove(file,table_name,NULL,0);
    }
    else
      (void) file->close(file,0);
  }
  else
  {
    DBUG_PRINT("error",("Got error: %d when creting table",error));
  }
  if (error)
    my_errno=error;
  DBUG_RETURN(error);
}


int ha_berkeley::create(const char *name, register TABLE *form,
			HA_CREATE_INFO *create_info)
{
  char name_buff[FN_REFLEN];
  char part[7];
  DBUG_ENTER("ha_berkeley::create");

  fn_format(name_buff,name,"", ha_berkeley_ext,2 | 4);

  /* Create the main table that will hold the real rows */
  if (create_sub_table(name_buff,"main",DB_BTREE,0))
    DBUG_RETURN(1);

  /* Create the keys */
  for (uint i=1; i < form->keys; i++)
  {
    sprintf(part,"key%02d",i);
    if (create_sub_table(name_buff, part, DB_BTREE,
			 (table->key_info[i].flags & HA_NOSAME) ? 0 :
			 DB_DUP))
      DBUG_RETURN(1);
  }

  /* Create the status block to save information from last status command */
  /* Is DB_BTREE the best option here ? (QUEUE can't be used in sub tables) */
  if (create_sub_table(name_buff,"status",DB_BTREE,0))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


int ha_berkeley::delete_table(const char *name)
{
  int error;
  char name_buff[FN_REFLEN];
  if ((error=db_create(&file, db_env, 0)))
  {
    my_errno=error;
    file=0;
    return 1;
  }
  error=file->remove(file,fn_format(name_buff,name,"",ha_berkeley_ext,2 | 4),
		     NULL,0);
  file=0;					// Safety
  return error;
}

/*
  How many seeks it will take to read through the table
  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/

double ha_berkeley::scan_time()
{
  return records/3;
 }

ha_rows ha_berkeley::records_in_range(int keynr,
				      const byte *start_key,uint start_key_len,
				      enum ha_rkey_function start_search_flag,
				      const byte *end_key,uint end_key_len,
				      enum ha_rkey_function end_search_flag)
{
  DBT key;
  DB_KEY_RANGE start_range, end_range;
  double start_pos,end_pos,rows;
  DBUG_ENTER("records_in_range");
  if ((start_key && file->key_range(file,transaction,
				    pack_key(&key, keynr, key_buff, start_key,
					     start_key_len),
				    &start_range,0)) ||
      (end_key && file->key_range(file,transaction,
				  pack_key(&key, keynr, key_buff, end_key,
					   end_key_len),
				  &end_range,0)))
    DBUG_RETURN(HA_BERKELEY_RANGE_COUNT); // Better than returning an error

  if (!start_key)
    start_pos=0.0;
  else if (start_search_flag == HA_READ_KEY_EXACT)
    start_pos=start_range.less;
  else
    start_pos=start_range.less+start_range.equal;

  if (!end_key)
    end_pos=1.0;
  else if (end_search_flag == HA_READ_BEFORE_KEY)
    end_pos=end_range.less;
  else
    end_pos=end_range.less+end_range.equal;
  rows=(end_pos-start_pos)*records;
  DBUG_PRINT("exit",("rows: %g",rows));
  DBUG_RETURN(rows <= 1.0 ? (ha_rows) 1 : (ha_rows) rows);
}

/****************************************************************************
 Handling the shared BDB_SHARE structure that is needed to provide table
 locking.
****************************************************************************/

static byte* bdb_get_key(BDB_SHARE *share,uint *length,
			 my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (byte*) share->table_name;
}

static BDB_SHARE *get_share(const char *table_name)
{
  BDB_SHARE *share;
  pthread_mutex_lock(&bdb_mutex);
  uint length=(uint) strlen(table_name);
  if (!(share=(BDB_SHARE*) hash_search(&bdb_open_tables, table_name, length)))
  {
    if ((share=(BDB_SHARE *) my_malloc(sizeof(*share)+length+1,
				       MYF(MY_WME | MY_ZEROFILL))))
    {
      // pthread_mutex_init(&share->mutex);
      // pthread_cond_init(&share->cond);
      share->table_name_length=length;
      share->table_name=(char*) (share+1);
      strmov(share->table_name,table_name);
      if (hash_insert(&bdb_open_tables, (char*) share))
      {
	pthread_mutex_unlock(&bdb_mutex);
	my_free((gptr) share,0);
	return 0;
      }
      thr_lock_init(&share->lock);
    }
  }
  share->use_count++;
  pthread_mutex_unlock(&bdb_mutex);
  return share;
}

static void free_share(BDB_SHARE *share)
{
  pthread_mutex_lock(&bdb_mutex);
  if (!--share->use_count)
  {
    hash_delete(&bdb_open_tables, (gptr) share);
    thr_lock_delete(&share->lock);
  //  pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&bdb_mutex);
}

#endif /* HAVE_BERKELEY_DB */
