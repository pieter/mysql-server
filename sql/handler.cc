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


/* Handler-calling-functions */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "ha_heap.h"
#include "ha_myisam.h"
#include "ha_myisammrg.h"
#ifndef NO_ISAM
#include "ha_isam.h"
#include "ha_isammrg.h"
#endif
#ifdef HAVE_BERKELEY_DB
#include "ha_berkeley.h"
#endif
#ifdef HAVE_INNOBASE_DB
#include "ha_innobase.h"
#endif
#ifdef HAVE_GEMINI_DB
#include "ha_gemini.h"
#endif
#include <myisampack.h>
#include <errno.h>

	/* static functions defined in this file */

static int NEAR_F delete_file(const char *name,const char *ext,int extflag);

ulong ha_read_count, ha_write_count, ha_delete_count, ha_update_count,
      ha_read_key_count, ha_read_next_count, ha_read_prev_count,
      ha_read_first_count, ha_read_last_count,
      ha_read_rnd_count, ha_read_rnd_next_count;

const char *ha_table_type[] = {
  "", "DIAB_ISAM","HASH","MISAM","PISAM","RMS_ISAM","HEAP", "ISAM",
  "MRG_ISAM","MYISAM", "MRG_MYISAM", "BDB", "INNODB", "GEMINI", "?", "?",NullS
};

TYPELIB ha_table_typelib= {array_elements(ha_table_type)-4,"",
			   ha_table_type+1};

const char *ha_row_type[] = {
  "", "FIXED", "DYNAMIC", "COMPRESSED","?","?","?"
};

const char *tx_isolation_names[] =
{ "READ-UNCOMMITTED", "READ-COMMITTED", "REPEATABLE-READ", "SERIALIZABLE",
  NullS};
TYPELIB tx_isolation_typelib= {array_elements(tx_isolation_names)-1,"",
			       tx_isolation_names};

	/* Use other database handler if databasehandler is not incompiled */

enum db_type ha_checktype(enum db_type database_type)
{
  switch (database_type) {
#ifdef HAVE_BERKELEY_DB
  case DB_TYPE_BERKELEY_DB:
    return(berkeley_skip ? DB_TYPE_MYISAM : database_type);
#endif
#ifdef HAVE_INNOBASE_DB
  case DB_TYPE_INNOBASE:
    return(innodb_skip ? DB_TYPE_MYISAM : database_type);
#endif
#ifdef HAVE_GEMINI_DB
  case DB_TYPE_GEMINI:
    return(gemini_skip ? DB_TYPE_MYISAM : database_type);
#endif
#ifndef NO_HASH
  case DB_TYPE_HASH:
#endif
#ifndef NO_MERGE
  case DB_TYPE_MRG_ISAM:
#endif
#ifndef NO_ISAM
  case DB_TYPE_ISAM:
#endif
  case DB_TYPE_HEAP:
  case DB_TYPE_MYISAM:
  case DB_TYPE_MRG_MYISAM:
    return (database_type);			/* Database exists on system */
  default:
    break;
  }
  return(DB_TYPE_MYISAM);			/* Use this as default */
} /* ha_checktype */


handler *get_new_handler(TABLE *table, enum db_type db_type)
{
  switch (db_type) {
#ifndef NO_HASH
  return new ha_hash(table);
#endif
#ifndef NO_MERGE
  case DB_TYPE_MRG_ISAM:
    return new ha_isammrg(table);
#endif
#ifndef NO_ISAM
  case DB_TYPE_ISAM:
    return new ha_isam(table);
#endif
#ifdef HAVE_BERKELEY_DB
  case DB_TYPE_BERKELEY_DB:
    return new ha_berkeley(table);
#endif
#ifdef HAVE_INNOBASE_DB
  case DB_TYPE_INNOBASE:
    return new ha_innobase(table);
#endif
#ifdef HAVE_GEMINI_DB
  case DB_TYPE_GEMINI:
    return new ha_gemini(table);
#endif
  case DB_TYPE_HEAP:
    return new ha_heap(table);
  case DB_TYPE_MYISAM:
  default:					// should never happen
    return new ha_myisam(table);
  case DB_TYPE_MRG_MYISAM:
    return new ha_myisammrg(table);
  }
}

int ha_init()
{
#ifdef HAVE_BERKELEY_DB
  if (!berkeley_skip)
  {
    int error;
    if ((error=berkeley_init()))
      return error;
    if (!berkeley_skip)				// If we couldn't use handler
      opt_using_transactions=1;
    else
      have_berkeley_db=SHOW_OPTION_DISABLED;
  }
#endif
#ifdef HAVE_INNOBASE_DB
  if (!innodb_skip)
  {
    if (innobase_init())
      return -1;
    if (!innodb_skip)				// If we couldn't use handler
      opt_using_transactions=1;
    else
      have_innodb=SHOW_OPTION_DISABLED;
  }
#endif
#ifdef HAVE_GEMINI_DB
  if (!gemini_skip)
  {
    if (gemini_init())
      return -1;
    if (!gemini_skip)				// If we couldn't use handler
      opt_using_transactions=1;
    else
      have_gemini=SHOW_OPTION_DISABLED;
  }
#endif
  return 0;
}

	/* close, flush or restart databases */
	/* Ignore this for other databases than ours */

int ha_panic(enum ha_panic_function flag)
{
  int error=0;
#ifndef NO_MERGE
  error|=mrg_panic(flag);
#endif
#ifndef NO_HASH
  error|=h_panic(flag);			/* fix hash */
#endif
  error|=heap_panic(flag);
  error|=nisam_panic(flag);
  error|=mi_panic(flag);
  error|=myrg_panic(flag);
#ifdef HAVE_BERKELEY_DB
  if (!berkeley_skip)
    error|=berkeley_end();
#endif
#ifdef HAVE_INNOBASE_DB
  if (!innodb_skip)
    error|=innobase_end();
#endif
#ifdef HAVE_GEMINI_DB
  if (!gemini_skip)
    error|=gemini_end();
#endif
  return error;
} /* ha_panic */

void ha_drop_database(char* path)
{
#ifdef HAVE_INNOBASE_DB
  if (!innodb_skip)
    innobase_drop_database(path);
#endif
}

void ha_close_connection(THD* thd)
{
#ifdef HAVE_INNOBASE_DB
  if (!innodb_skip)
    innobase_close_connection(thd);
#endif
#ifdef HAVE_GEMINI_DB
  if (!gemini_skip && thd->gemini.context)
  {
    gemini_disconnect(thd);
  }
#endif /* HAVE_GEMINI_DB */
}

/*
  This is used to commit or rollback a single statement depending
  on the value of error
*/

int ha_autocommit_or_rollback(THD *thd, int error)
{
  DBUG_ENTER("ha_autocommit_or_rollback");
#ifdef USING_TRANSACTIONS
  if (opt_using_transactions)
  {
    if (!error)
    {
      if (ha_commit_stmt(thd))
	error=1;
    }
    else
      (void) ha_rollback_stmt(thd);
    thd->tx_isolation=thd->session_tx_isolation;
  }
#endif
  DBUG_RETURN(error);
}

/*
  This function is called when MySQL writes the log segment of a
  transaction to the binlog. It is called when the LOCK_log mutex is
  reserved. Here we communicate to transactional table handlers whta
  binlog position corresponds to the current transaction. The handler
  can store it and in recovery print to the user, so that the user
  knows from what position in the binlog to start possible
  roll-forward, for example, if the crashed server was a slave in
  replication. This function also calls the commit of the table
  handler, because the order of trasnactions in the log of the table
  handler must be the same as in the binlog.

  arguments:
  log_file_name: latest binlog file name
  end_offset:	 the offset in the binlog file up to which we wrote
*/

int ha_report_binlog_offset_and_commit(THD *thd,
				       char *log_file_name,
				       my_off_t end_offset)
{
  int  error= 0;
#ifdef HAVE_INNOBASE_DB
  THD_TRANS *trans;
  trans = &thd->transaction.all;
  if (trans->innobase_tid)
  {
    if ((error=innobase_report_binlog_offset_and_commit(thd,
							trans->innobase_tid,
							log_file_name,
							end_offset)))
    {
      my_error(ER_ERROR_DURING_COMMIT, MYF(0), error);
      error=1;
    }
    trans->innodb_active_trans=0;
  }
#endif
  return error;
}


int ha_commit_trans(THD *thd, THD_TRANS* trans)
{
  int error=0;
  DBUG_ENTER("ha_commit");
#ifdef USING_TRANSACTIONS
  if (opt_using_transactions)
  {
    /* Update the binary log if we have cached some queries */
    if (trans == &thd->transaction.all && mysql_bin_log.is_open() &&
	my_b_tell(&thd->transaction.trans_log))
    {
      mysql_bin_log.write(thd, &thd->transaction.trans_log);
      reinit_io_cache(&thd->transaction.trans_log,
		      WRITE_CACHE, (my_off_t) 0, 0, 1);
      thd->transaction.trans_log.end_of_file= max_binlog_cache_size;
    }
#ifdef HAVE_BERKELEY_DB
    if (trans->bdb_tid)
    {
      if ((error=berkeley_commit(thd,trans->bdb_tid)))
      {
	my_error(ER_ERROR_DURING_COMMIT, MYF(0), error);
	error=1;
      }
      trans->bdb_tid=0;
    }
#endif
#ifdef HAVE_INNOBASE_DB
    if (trans->innobase_tid)
    {
      if ((error=innobase_commit(thd,trans->innobase_tid)))
      {
	my_error(ER_ERROR_DURING_COMMIT, MYF(0), error);
	error=1;
      }
      trans->innodb_active_trans=0;
    }
#endif
#ifdef HAVE_GEMINI_DB 
    /* Commit the transaction in behalf of the commit statement
       or if we're in auto-commit mode               */
    if((trans == &thd->transaction.all) || 
        (!(thd->options & (OPTION_NOT_AUTO_COMMIT | OPTION_BEGIN))))
    {
      error=gemini_commit(thd);
      if (error)
      {
	my_error(ER_ERROR_DURING_COMMIT, MYF(0), error);
	error=1;
      }
    }
#endif
    if (error && trans == &thd->transaction.all && mysql_bin_log.is_open())
      sql_print_error("Error: Got error during commit;  Binlog is not up to date!");
    thd->tx_isolation=thd->session_tx_isolation;
  }
#endif // using transactions
  DBUG_RETURN(error);
}


int ha_rollback_trans(THD *thd, THD_TRANS *trans)
{
  int error=0;
  DBUG_ENTER("ha_rollback");
#ifdef USING_TRANSACTIONS
  if (opt_using_transactions)
  {
#ifdef HAVE_BERKELEY_DB
    if (trans->bdb_tid)
    {
      if ((error=berkeley_rollback(thd, trans->bdb_tid)))
      {
	my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), error);
	error=1;
      }
      trans->bdb_tid=0;
    }
#endif
#ifdef HAVE_INNOBASE_DB
    if (trans->innobase_tid)
    {
      if ((error=innobase_rollback(thd, trans->innobase_tid)))
      {
	my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), error);
	error=1;
      }
      trans->innodb_active_trans=0;
    }
#endif
#ifdef HAVE_GEMINI_DB
    if((trans == &thd->transaction.stmt) &&
       (thd->options & (OPTION_NOT_AUTO_COMMIT | OPTION_BEGIN)))
      error = gemini_rollback_to_savepoint(thd);
    else
      error=gemini_rollback(thd);
    if (error)
    {
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), error);
      error=1;
    }
#endif
    if (trans == &thd->transaction.all)
      reinit_io_cache(&thd->transaction.trans_log,
		      WRITE_CACHE, (my_off_t) 0, 0, 1);
    thd->transaction.trans_log.end_of_file= max_binlog_cache_size;
    thd->tx_isolation=thd->session_tx_isolation;
  }
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}

void ha_set_spin_retries(uint retries)
{
#ifdef HAVE_GEMINI_DB
  if (!gemini_skip)
  {
    gemini_set_option_long(GEM_OPTID_SPIN_RETRIES, retries);
  }
#endif /* HAVE_GEMINI_DB */
}


bool ha_flush_logs()
{
  bool result=0;
#ifdef HAVE_BERKELEY_DB
  if (!berkeley_skip && berkeley_flush_logs())
    result=1;
#endif
#ifdef HAVE_INNOBASE_DB
  if (!innodb_skip && innobase_flush_logs())
    result=1;
#endif
  return result;
}

/*
  This should return ENOENT if the file doesn't exists.
  The .frm file will be deleted only if we return 0 or ENOENT
*/

int ha_delete_table(enum db_type table_type, const char *path)
{
  handler *file=get_new_handler((TABLE*) 0, table_type);
  if (!file)
    return ENOENT;
  int error=file->delete_table(path);
  delete file;
  return error;
}
    
void ha_store_ptr(byte *buff, uint pack_length, my_off_t pos)
{
  switch (pack_length) {
#if SIZEOF_OFF_T > 4
  case 8: mi_int8store(buff,pos); break;
  case 7: mi_int7store(buff,pos); break;
  case 6: mi_int6store(buff,pos); break;
  case 5: mi_int5store(buff,pos); break;
#endif
  case 4: mi_int4store(buff,pos); break;
  case 3: mi_int3store(buff,pos); break;
  case 2: mi_int2store(buff,(uint) pos); break;
  case 1: buff[0]= (uchar) pos; break;
  }
  return;
}

my_off_t ha_get_ptr(byte *ptr, uint pack_length)
{
  my_off_t pos;
  switch (pack_length) {
#if SIZEOF_OFF_T > 4
  case 8:
    pos= (my_off_t) mi_uint8korr(ptr);
    break;
  case 7:
    pos= (my_off_t) mi_uint7korr(ptr);
    break;
  case 6:
    pos= (my_off_t) mi_uint6korr(ptr);
    break;
  case 5:
    pos= (my_off_t) mi_uint5korr(ptr);
    break;
#endif
  case 4:
    pos= (my_off_t) mi_uint4korr(ptr);
    break;
  case 3:
    pos= (my_off_t) mi_uint3korr(ptr);
    break;
  case 2:
    pos= (my_off_t) mi_uint2korr(ptr);
    break;
  case 1:
    pos= (my_off_t) mi_uint2korr(ptr);
    break;
  default:
    pos=0;					// Impossible
    break;
  }
 return pos;
}

/****************************************************************************
** General handler functions
****************************************************************************/

	/* Open database-handler. Try O_RDONLY if can't open as O_RDWR */
	/* Don't wait for locks if not HA_OPEN_WAIT_IF_LOCKED is set */

int handler::ha_open(const char *name, int mode, int test_if_locked)
{
  int error;
  DBUG_ENTER("handler::open");
  DBUG_PRINT("enter",("name: %s  db_type: %d  db_stat: %d  mode: %d  lock_test: %d",
		      name, table->db_type, table->db_stat, mode, test_if_locked));

  if ((error=open(name,mode,test_if_locked)))
  {
    if ((error == EACCES || error == EROFS) && mode == O_RDWR &&
	(table->db_stat & HA_TRY_READ_ONLY))
    {
      table->db_stat|=HA_READ_ONLY;
      error=open(name,O_RDONLY,test_if_locked);
    }
  }
  if (error)
  {
    my_errno=error;			/* Safeguard */
    DBUG_PRINT("error",("error: %d  errno: %d",error,errno));
  }
  else
  {
    if (table->db_options_in_use & HA_OPTION_READ_ONLY_DATA)
      table->db_stat|=HA_READ_ONLY;
  }
  if (!error)
  {
    if (!alloc_root_inited(&table->mem_root))	// If temporary table
      ref=(byte*) sql_alloc(ALIGN_SIZE(ref_length)*2);
    else
      ref=(byte*) alloc_root(&table->mem_root, ALIGN_SIZE(ref_length)*2);
    if (!ref)
    {
      close();
      error=HA_ERR_OUT_OF_MEM;
    }
    else
      dupp_ref=ref+ALIGN_SIZE(ref_length);
  }
  DBUG_RETURN(error);
}

int handler::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  return HA_ADMIN_NOT_IMPLEMENTED;
}

int handler::backup(THD* thd, HA_CHECK_OPT* check_opt)
{
  return HA_ADMIN_NOT_IMPLEMENTED;
}

int handler::restore(THD* thd, HA_CHECK_OPT* check_opt)
{
  return HA_ADMIN_NOT_IMPLEMENTED;
}

int handler::repair(THD* thd, HA_CHECK_OPT* check_opt)
{
  return HA_ADMIN_NOT_IMPLEMENTED;
}

int handler::optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  return HA_ADMIN_NOT_IMPLEMENTED;
}

int handler::analyze(THD* thd, HA_CHECK_OPT* check_opt)
{
  return HA_ADMIN_NOT_IMPLEMENTED;
}

	/* Read first row from a table */

int handler::rnd_first(byte * buf)
{
  register int error;
  DBUG_ENTER("handler::rnd_first");

  statistic_increment(ha_read_first_count,&LOCK_status);
  (void) rnd_init();
  while ((error= rnd_next(buf)) == HA_ERR_RECORD_DELETED) ;
  (void) rnd_end();
  DBUG_RETURN(error);
}


/*
  The following function is only needed for tables that may be temporary tables
  during joins
*/

int handler::restart_rnd_next(byte *buf, byte *pos)
{
  return HA_ERR_WRONG_COMMAND;
}


	/* Set a timestamp in record */

void handler::update_timestamp(byte *record)
{
  long skr= (long) current_thd->query_start();
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(record,skr);
  }
  else
#endif
  longstore(record,skr);
  return;
}

	/* Updates field with field_type NEXT_NUMBER according to following:
	** if field = 0 change field to the next free key in database.
	*/

void handler::update_auto_increment()
{
  longlong nr;
  THD *thd;
  DBUG_ENTER("update_auto_increment");
  if (table->next_number_field->val_int() != 0)
  {
    auto_increment_column_changed=0;
    DBUG_VOID_RETURN;
  }
  thd=current_thd;
  if ((nr=thd->next_insert_id))
    thd->next_insert_id=0;			// Clear after use
  else
    nr=get_auto_increment();
  thd->insert_id((ulonglong) nr);
  table->next_number_field->store(nr);
  auto_increment_column_changed=1;
  DBUG_VOID_RETURN;
}


longlong handler::get_auto_increment()
{
  longlong nr;
  int error;
  (void) extra(HA_EXTRA_KEYREAD);
  index_init(table->next_number_index);
  error=index_last(table->record[1]);
  if (error)
    nr=1;
  else
    nr=(longlong) table->next_number_field->
      val_int_offset(table->rec_buff_length)+1;
  (void) extra(HA_EXTRA_NO_KEYREAD);
  index_end();
  return nr;
}

	/* Print error that we got from handler function */

void handler::print_error(int error, myf errflag)
{
  DBUG_ENTER("print_error");
  DBUG_PRINT("enter",("error: %d",error));

  int textno=ER_GET_ERRNO;
  switch (error) {
  case EAGAIN:
    textno=ER_FILE_USED;
    break;
  case ENOENT:
    textno=ER_FILE_NOT_FOUND;
    break;
  case HA_ERR_KEY_NOT_FOUND:
  case HA_ERR_NO_ACTIVE_RECORD:
  case HA_ERR_END_OF_FILE:
    textno=ER_KEY_NOT_FOUND;
    break;
  case HA_ERR_WRONG_TABLE_DEF:
    textno=ER_WRONG_MRG_TABLE;
    break;
  case HA_ERR_FOUND_DUPP_KEY:
  {
    uint key_nr=get_dup_key(error);
    if ((int) key_nr >= 0)
    {
      /* Write the dupplicated key in the error message */
      char key[MAX_KEY_LENGTH];
      String str(key,sizeof(key));
      key_unpack(&str,table,(uint) key_nr);
      uint max_length=MYSQL_ERRMSG_SIZE-(uint) strlen(ER(ER_DUP_ENTRY));
      if (str.length() >= max_length)
      {
	str.length(max_length-4);
	str.append("...");
      }
      my_error(ER_DUP_ENTRY,MYF(0),str.c_ptr(),key_nr+1);
      DBUG_VOID_RETURN;
    }
    textno=ER_DUP_KEY;
    break;
  }
  case HA_ERR_FOUND_DUPP_UNIQUE:
    textno=ER_DUP_UNIQUE;
    break;
  case HA_ERR_RECORD_CHANGED:
    textno=ER_CHECKREAD;
    break;
  case HA_ERR_CRASHED:
    textno=ER_NOT_KEYFILE;
    break;
  case HA_ERR_CRASHED_ON_USAGE:
    textno=ER_CRASHED_ON_USAGE;
    break;
  case HA_ERR_CRASHED_ON_REPAIR:
    textno=ER_CRASHED_ON_REPAIR;
    break;
  case HA_ERR_OUT_OF_MEM:
    my_error(ER_OUT_OF_RESOURCES,errflag);
    DBUG_VOID_RETURN;
  case HA_ERR_WRONG_COMMAND:
    textno=ER_ILLEGAL_HA;
    break;
  case HA_ERR_OLD_FILE:
    textno=ER_OLD_KEYFILE;
    break;
  case HA_ERR_UNSUPPORTED:
    textno=ER_UNSUPPORTED_EXTENSION;
    break;
  case HA_ERR_RECORD_FILE_FULL:
    textno=ER_RECORD_FILE_FULL;
    break;
  case HA_ERR_LOCK_WAIT_TIMEOUT:
    textno=ER_LOCK_WAIT_TIMEOUT;
    break;
  case HA_ERR_LOCK_TABLE_FULL:
    textno=ER_LOCK_TABLE_FULL;
    break;
  case HA_ERR_LOCK_DEADLOCK:
    textno=ER_LOCK_DEADLOCK;
    break;
  case HA_ERR_READ_ONLY_TRANSACTION:
    textno=ER_READ_ONLY_TRANSACTION;
    break;
  case HA_ERR_CANNOT_ADD_FOREIGN:
    textno=ER_CANNOT_ADD_FOREIGN;
    break;
  case HA_ERR_ROW_IS_REFERENCED:
    textno=ER_ROW_IS_REFERENCED;
    break;
  case HA_ERR_NO_REFERENCED_ROW:
    textno=ER_NO_REFERENCED_ROW;
    break;
  default:
    {
      my_error(ER_GET_ERRNO,errflag,error);
      DBUG_VOID_RETURN;
    }
  }
  my_error(textno,errflag,table->table_name,error);
  DBUG_VOID_RETURN;
}

	/* Return key if error because of duplicated keys */

uint handler::get_dup_key(int error)
{
  DBUG_ENTER("get_dup_key");
  table->file->errkey  = (uint) -1;
  if (error == HA_ERR_FOUND_DUPP_KEY || error == HA_ERR_FOUND_DUPP_UNIQUE)
    info(HA_STATUS_ERRKEY | HA_STATUS_NO_LOCK);
  DBUG_RETURN(table->file->errkey);
}

int handler::delete_table(const char *name)
{
  int error=0;
  for (const char **ext=bas_ext(); *ext ; ext++)
  {
    if (delete_file(name,*ext,2))
    {
      if ((error=errno) != ENOENT)
	break;
    }
  }
  return error;
}


int handler::rename_table(const char * from, const char * to)
{
  DBUG_ENTER("handler::rename_table");
  for (const char **ext=bas_ext(); *ext ; ext++)
  {
    if (rename_file_ext(from,to,*ext))
      DBUG_RETURN(my_errno);
  }
  DBUG_RETURN(0);
}

int ha_commit_rename(THD *thd)
{
  int error=0;
#ifdef HAVE_GEMINI_DB
    /* Gemini needs to commit the rename; otherwise a rollback will change
    ** the table names back internally but the physical files will still
    ** have the new names.
    */
    if (ha_commit_stmt(thd))
      error= -1;
    if (ha_commit(thd))
      error= -1;
#endif
    return error;
}

/* Tell the handler to turn on or off logging to the handler's
   recovery log
*/
int ha_recovery_logging(THD *thd, bool on)
{
  int error=0;

  DBUG_ENTER("ha_recovery_logging");
#ifdef USING_TRANSACTIONS
  if (opt_using_transactions)
  {
#ifdef HAVE_GEMINI_DB
    error = gemini_recovery_logging(thd, on);
#endif
  }
#endif
  DBUG_RETURN(error);
}

int handler::index_next_same(byte *buf, const byte *key, uint keylen)
{
  int error;
  if (!(error=index_next(buf)))
  {
    if (key_cmp(table, key, active_index, keylen))
    {
      table->status=STATUS_NOT_FOUND;
      error=HA_ERR_END_OF_FILE;
    }
  }
  return error;
}


/*
  The following is only needed if we would like to use the database
  for internal temporary tables
*/

int handler::delete_all_rows()
{
  return (my_errno=HA_ERR_WRONG_COMMAND);
}

/****************************************************************************
** Some general functions that isn't in the handler class
****************************************************************************/

	/* Initiates table-file and calls apropriate database-creator */
	/* Returns 1 if something got wrong */

int ha_create_table(const char *name, HA_CREATE_INFO *create_info,
		    bool update_create_info)

{
  int error;
  TABLE table;
  DBUG_ENTER("ha_create_table");

  if (openfrm(name,"",0,(uint) READ_ALL, 0, &table))
    DBUG_RETURN(1);
  if (update_create_info)
  {
    update_create_info_from_table(create_info, &table);
    if (table.file->option_flag() & HA_DROP_BEFORE_CREATE)
      table.file->delete_table(name);		// Needed for BDB tables
  }
  error=table.file->create(name,&table,create_info);
  VOID(closefrm(&table));
  if (error) {
    if (table.db_type == DB_TYPE_INNOBASE) {
      /* Creation of InnoDB table cannot fail because of an OS error:
	 put error as the number */
      my_error(ER_CANT_CREATE_TABLE,MYF(ME_BELL+ME_WAITTANG),name,error);
    } else {
      my_error(ER_CANT_CREATE_TABLE,MYF(ME_BELL+ME_WAITTANG),name,my_errno);
    }
  }
  DBUG_RETURN(error != 0);
}

	/* Use key cacheing on all databases */

void ha_key_cache(void)
{
  if (keybuff_size)
    (void) init_key_cache(keybuff_size,0);
} /* ha_key_cache */


static int NEAR_F delete_file(const char *name,const char *ext,int extflag)
{
  char buff[FN_REFLEN];
  VOID(fn_format(buff,name,"",ext,extflag | 4));
  return(my_delete(buff,MYF(MY_WME)));
}
