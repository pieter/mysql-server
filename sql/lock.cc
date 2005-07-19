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


/* locking functions for mysql */
/*
  Because of the new concurrent inserts, we must first get external locks
  before getting internal locks.  If we do it in the other order, the status
  information is not up to date when called from the lock handler.

  GENERAL DESCRIPTION OF LOCKING

  When not using LOCK TABLES:

  - For each SQL statement mysql_lock_tables() is called for all involved
    tables.
    - mysql_lock_tables() will call
      table_handler->external_lock(thd,locktype) for each table.
      This is followed by a call to thr_multi_lock() for all tables.

  - When statement is done, we call mysql_unlock_tables().
    This will call thr_multi_unlock() followed by
    table_handler->external_lock(thd, F_UNLCK) for each table.

  - Note that mysql_unlock_tables() may be called several times as
    MySQL in some cases can free some tables earlier than others.

  - The above is true both for normal and temporary tables.

  - Temporary non transactional tables are never passed to thr_multi_lock()
    and we never call external_lock(thd, F_UNLOCK) on these.

  When using LOCK TABLES:

  - LOCK TABLE will call mysql_lock_tables() for all tables.
    mysql_lock_tables() will call
    table_handler->external_lock(thd,locktype) for each table.
    This is followed by a call to thr_multi_lock() for all tables.

  - For each statement, we will call table_handler->start_stmt(THD)
    to inform the table handler that we are using the table.

    The tables used can only be tables used in LOCK TABLES or a
    temporary table.

  - When statement is done, we will call ha_commit_stmt(thd);

  - When calling UNLOCK TABLES we call mysql_unlock_tables() for all
    tables used in LOCK TABLES

TODO:
  Change to use my_malloc() ONLY when using LOCK TABLES command or when
  we are forced to use mysql_lock_merge.
*/

#include "mysql_priv.h"
#include <hash.h>
#include "ha_myisammrg.h"
#ifndef MASTER
#include "../srclib/myisammrg/myrg_def.h"
#else
#include "../storage/myisammrg/myrg_def.h"
#endif

static MYSQL_LOCK *get_lock_data(THD *thd, TABLE **table,uint count,
				 bool unlock, TABLE **write_locked);
static int lock_external(THD *thd, TABLE **table,uint count);
static int unlock_external(THD *thd, TABLE **table,uint count);
static void print_lock_error(int error, const char *);


/*
  Lock tables.

  SYNOPSIS
    mysql_lock_tables()
    thd                         The current thread.
    tables                      An array of pointers to the tables to lock.
    count                       The number of tables to lock.
    flags                       Options:
      MYSQL_LOCK_IGNORE_GLOBAL_READ_LOCK      Ignore a global read lock
      MYSQL_LOCK_IGNORE_FLUSH                 Ignore a flush tables.

  RETURN
    A lock structure pointer on success.
    NULL on error.
*/

MYSQL_LOCK *mysql_lock_tables(THD *thd, TABLE **tables, uint count, uint flags)
{
  MYSQL_LOCK *sql_lock;
  TABLE *write_lock_used;
  int rc;
  /* Map the return value of thr_lock to an error from errmsg.txt */
  const static int thr_lock_errno_to_mysql[]=
  { 0, 1, ER_LOCK_WAIT_TIMEOUT, ER_LOCK_DEADLOCK };
  DBUG_ENTER("mysql_lock_tables");

  for (;;)
  {
    if (!(sql_lock = get_lock_data(thd,tables,count, 0,&write_lock_used)))
      break;

    if (global_read_lock && write_lock_used &&
        ! (flags & MYSQL_LOCK_IGNORE_GLOBAL_READ_LOCK))
    {
      /*
	Someone has issued LOCK ALL TABLES FOR READ and we want a write lock
	Wait until the lock is gone
      */
      if (wait_if_global_read_lock(thd, 1, 1))
      {
	my_free((gptr) sql_lock,MYF(0));
	sql_lock=0;
	break;
      }	
      if (thd->version != refresh_version)
      {
	my_free((gptr) sql_lock,MYF(0));
	goto retry;
      }
    }

    thd->proc_info="System lock";
    if (lock_external(thd, tables, count))
    {
      my_free((gptr) sql_lock,MYF(0));
      sql_lock=0;
      break;
    }
    thd->proc_info="Table lock";
    thd->locked=1;
    rc= thr_lock_errno_to_mysql[(int) thr_multi_lock(sql_lock->locks,
                                                     sql_lock->lock_count,
                                                     thd->lock_id)];
    if (rc > 1)                                 /* a timeout or a deadlock */
    {
      my_error(rc, MYF(0));
      my_free((gptr) sql_lock,MYF(0));
      sql_lock= 0;
      break;
    }
    else if (rc == 1)                           /* aborted */
    {
      thd->some_tables_deleted=1;		// Try again
      sql_lock->lock_count= 0;                  // Locks are already freed
    }
    else if (!thd->some_tables_deleted || (flags & MYSQL_LOCK_IGNORE_FLUSH))
    {
      thd->locked=0;
      break;
    }
    else if (!thd->open_tables)
    {
      // Only using temporary tables, no need to unlock
      thd->some_tables_deleted=0;
      thd->locked=0;
      break;
    }
    thd->proc_info=0;

    /* some table was altered or deleted. reopen tables marked deleted */
    mysql_unlock_tables(thd,sql_lock);
    thd->locked=0;
retry:
    sql_lock=0;
    if (wait_for_tables(thd))
      break;					// Couldn't open tables
  }
  thd->proc_info=0;
  if (thd->killed)
  {
    thd->send_kill_message();
    if (sql_lock)
    {
      mysql_unlock_tables(thd,sql_lock);
      sql_lock=0;
    }
  }

  thd->lock_time();
  DBUG_RETURN (sql_lock);
}


static int lock_external(THD *thd, TABLE **tables, uint count)
{
  reg1 uint i;
  int lock_type,error;
  DBUG_ENTER("lock_external");

  for (i=1 ; i <= count ; i++, tables++)
  {
    DBUG_ASSERT((*tables)->reginfo.lock_type >= TL_READ);
    lock_type=F_WRLCK;				/* Lock exclusive */
    if ((*tables)->db_stat & HA_READ_ONLY ||
	((*tables)->reginfo.lock_type >= TL_READ &&
	 (*tables)->reginfo.lock_type <= TL_READ_NO_INSERT))
      lock_type=F_RDLCK;
    if ((error=(*tables)->file->external_lock(thd,lock_type)))
    {
      print_lock_error(error, (*tables)->file->table_type());
      for (; i-- ; tables--)
      {
	(*tables)->file->external_lock(thd, F_UNLCK);
	(*tables)->current_lock=F_UNLCK;
      }
      DBUG_RETURN(error);
    }
    else
    {
      (*tables)->db_stat &= ~ HA_BLOCK_LOCK;
      (*tables)->current_lock= lock_type;
    }
  }
  DBUG_RETURN(0);
}


void mysql_unlock_tables(THD *thd, MYSQL_LOCK *sql_lock)
{
  DBUG_ENTER("mysql_unlock_tables");
  if (sql_lock->lock_count)
    thr_multi_unlock(sql_lock->locks,sql_lock->lock_count);
  if (sql_lock->table_count)
    VOID(unlock_external(thd,sql_lock->table,sql_lock->table_count));
  my_free((gptr) sql_lock,MYF(0));
  DBUG_VOID_RETURN;
}

/*
  Unlock some of the tables locked by mysql_lock_tables
  This will work even if get_lock_data fails (next unlock will free all)
  */

void mysql_unlock_some_tables(THD *thd, TABLE **table,uint count)
{
  MYSQL_LOCK *sql_lock;
  TABLE *write_lock_used;
  if ((sql_lock = get_lock_data(thd, table, count, 1, &write_lock_used)))
    mysql_unlock_tables(thd, sql_lock);
}


/*
** unlock all tables locked for read.
*/

void mysql_unlock_read_tables(THD *thd, MYSQL_LOCK *sql_lock)
{
  uint i,found;
  DBUG_ENTER("mysql_unlock_read_tables");

  /* Move all write locks first */
  THR_LOCK_DATA **lock=sql_lock->locks;
  for (i=found=0 ; i < sql_lock->lock_count ; i++)
  {
    if (sql_lock->locks[i]->type >= TL_WRITE_ALLOW_READ)
    {
      swap_variables(THR_LOCK_DATA *, *lock, sql_lock->locks[i]);
      lock++;
      found++;
    }
  }
  /* unlock the read locked tables */
  if (i != found)
  {
    thr_multi_unlock(lock,i-found);
    sql_lock->lock_count= found;
  }

  /* Then do the same for the external locks */
  /* Move all write locked tables first */
  TABLE **table=sql_lock->table;
  for (i=found=0 ; i < sql_lock->table_count ; i++)
  {
    if ((uint) sql_lock->table[i]->reginfo.lock_type >= TL_WRITE_ALLOW_READ)
    {
      swap_variables(TABLE *, *table, sql_lock->table[i]);
      table++;
      found++;
    }
  }
  /* Unlock all read locked tables */
  if (i != found)
  {
    VOID(unlock_external(thd,table,i-found));
    sql_lock->table_count=found;
  }
  DBUG_VOID_RETURN;
}



void mysql_lock_remove(THD *thd, MYSQL_LOCK *locked,TABLE *table)
{
  mysql_unlock_some_tables(thd, &table,1);
  if (locked)
  {
    reg1 uint i;
    for (i=0; i < locked->table_count; i++)
    {
      if (locked->table[i] == table)
      {
	locked->table_count--;
	bmove((char*) (locked->table+i),
	      (char*) (locked->table+i+1),
	      (locked->table_count-i)* sizeof(TABLE*));
	break;
      }
    }
    THR_LOCK_DATA **prev=locked->locks;
    for (i=0 ; i < locked->lock_count ; i++)
    {
      if (locked->locks[i]->type != TL_UNLOCK)
	*prev++ = locked->locks[i];
    }
    locked->lock_count=(uint) (prev - locked->locks);
  }
}

/* abort all other threads waiting to get lock in table */

void mysql_lock_abort(THD *thd, TABLE *table)
{
  MYSQL_LOCK *locked;
  TABLE *write_lock_used;
  if ((locked = get_lock_data(thd,&table,1,1,&write_lock_used)))
  {
    for (uint i=0; i < locked->lock_count; i++)
      thr_abort_locks(locked->locks[i]->lock);
    my_free((gptr) locked,MYF(0));
  }
}


/* Abort one thread / table combination */

void mysql_lock_abort_for_thread(THD *thd, TABLE *table)
{
  MYSQL_LOCK *locked;
  TABLE *write_lock_used;
  DBUG_ENTER("mysql_lock_abort_for_thread");

  if ((locked = get_lock_data(thd,&table,1,1,&write_lock_used)))
  {
    for (uint i=0; i < locked->lock_count; i++)
      thr_abort_locks_for_thread(locked->locks[i]->lock,
				 table->in_use->real_id);
    my_free((gptr) locked,MYF(0));
  }
  DBUG_VOID_RETURN;
}


MYSQL_LOCK *mysql_lock_merge(MYSQL_LOCK *a,MYSQL_LOCK *b)
{
  MYSQL_LOCK *sql_lock;
  DBUG_ENTER("mysql_lock_merge");
  if (!(sql_lock= (MYSQL_LOCK*)
	my_malloc(sizeof(*sql_lock)+
		  sizeof(THR_LOCK_DATA*)*(a->lock_count+b->lock_count)+
		  sizeof(TABLE*)*(a->table_count+b->table_count),MYF(MY_WME))))
    DBUG_RETURN(0);				// Fatal error
  sql_lock->lock_count=a->lock_count+b->lock_count;
  sql_lock->table_count=a->table_count+b->table_count;
  sql_lock->locks=(THR_LOCK_DATA**) (sql_lock+1);
  sql_lock->table=(TABLE**) (sql_lock->locks+sql_lock->lock_count);
  memcpy(sql_lock->locks,a->locks,a->lock_count*sizeof(*a->locks));
  memcpy(sql_lock->locks+a->lock_count,b->locks,
	 b->lock_count*sizeof(*b->locks));
  memcpy(sql_lock->table,a->table,a->table_count*sizeof(*a->table));
  memcpy(sql_lock->table+a->table_count,b->table,
	 b->table_count*sizeof(*b->table));
  my_free((gptr) a,MYF(0));
  my_free((gptr) b,MYF(0));
  DBUG_RETURN(sql_lock);
}


	/* unlock a set of external */

static int unlock_external(THD *thd, TABLE **table,uint count)
{
  int error,error_code;
  DBUG_ENTER("unlock_external");

  error_code=0;
  do
  {
    if ((*table)->current_lock != F_UNLCK)
    {
      (*table)->current_lock = F_UNLCK;
      if ((error=(*table)->file->external_lock(thd, F_UNLCK)))
      {
	error_code=error;
	print_lock_error(error_code, (*table)->file->table_type());
      }
    }
    table++;
  } while (--count);
  DBUG_RETURN(error_code);
}


/*
** Get lock structures from table structs and initialize locks
*/


static MYSQL_LOCK *get_lock_data(THD *thd, TABLE **table_ptr, uint count,
				 bool get_old_locks, TABLE **write_lock_used)
{
  uint i,tables,lock_count;
  MYSQL_LOCK *sql_lock;
  THR_LOCK_DATA **locks;
  TABLE **to;

  *write_lock_used=0;
  for (i=tables=lock_count=0 ; i < count ; i++)
  {
    if (table_ptr[i]->s->tmp_table != TMP_TABLE)
    {
      tables+=table_ptr[i]->file->lock_count();
      lock_count++;
    }
    /*
      To be able to open and lock for reading system tables like 'mysql.proc',
      when we already have some tables opened and locked, and avoid deadlocks
      we have to disallow write-locking of these tables with any other tables.
    */
    if (table_ptr[i]->s->system_table &&
        table_ptr[i]->reginfo.lock_type >= TL_WRITE_ALLOW_WRITE &&
        count != 1)
    {
      my_error(ER_WRONG_LOCK_OF_SYSTEM_TABLE, MYF(0), table_ptr[i]->s->db,
               table_ptr[i]->s->table_name);
      return 0;
    }
  }

  if (!(sql_lock= (MYSQL_LOCK*)
	my_malloc(sizeof(*sql_lock)+
		  sizeof(THR_LOCK_DATA*)*tables+sizeof(table_ptr)*lock_count,
		  MYF(0))))
    return 0;
  locks=sql_lock->locks=(THR_LOCK_DATA**) (sql_lock+1);
  to=sql_lock->table=(TABLE**) (locks+tables);
  sql_lock->table_count=lock_count;
  sql_lock->lock_count=tables;

  for (i=0 ; i < count ; i++)
  {
    TABLE *table;
    if ((table=table_ptr[i])->s->tmp_table == TMP_TABLE)
      continue;
    *to++=table;
    enum thr_lock_type lock_type= table->reginfo.lock_type;
    if (lock_type >= TL_WRITE_ALLOW_WRITE)
    {
      *write_lock_used=table;
      if (table->db_stat & HA_READ_ONLY)
      {
	my_error(ER_OPEN_AS_READONLY, MYF(0), table->alias);
	my_free((gptr) sql_lock,MYF(0));
	return 0;
      }
    }
    THR_LOCK_DATA **org_locks = locks;
    locks=table->file->store_lock(thd, locks, get_old_locks ? TL_IGNORE :
				  lock_type);
    if (locks)
      for ( ; org_locks != locks ; org_locks++)
	(*org_locks)->debug_print_param= (void *) table;
  }
  return sql_lock;
}


/*****************************************************************************
  Lock table based on the name.
  This is used when we need total access to a closed, not open table
*****************************************************************************/

/*
  Lock and wait for the named lock.

  SYNOPSIS
    lock_and_wait_for_table_name()
    thd			Thread handler
    table_list		Lock first table in this list


  NOTES
    Works together with global read lock.

  RETURN
    0	ok
    1	error
*/

int lock_and_wait_for_table_name(THD *thd, TABLE_LIST *table_list)
{
  int lock_retcode;
  int error= -1;
  DBUG_ENTER("lock_and_wait_for_table_name");

  if (wait_if_global_read_lock(thd, 0, 1))
    DBUG_RETURN(1);
  VOID(pthread_mutex_lock(&LOCK_open));
  if ((lock_retcode = lock_table_name(thd, table_list)) < 0)
    goto end;
  if (lock_retcode && wait_for_locked_table_names(thd, table_list))
  {
    unlock_table_name(thd, table_list);
    goto end;
  }
  error=0;

end:
  pthread_mutex_unlock(&LOCK_open);
  start_waiting_global_read_lock(thd);
  DBUG_RETURN(error);
}


/*
  Put a not open table with an old refresh version in the table cache.

  SYNPOSIS
    lock_table_name()
    thd			Thread handler
    table_list		Lock first table in this list

  WARNING
    If you are going to update the table, you should use
    lock_and_wait_for_table_name instead of this function as this works
    together with 'FLUSH TABLES WITH READ LOCK'

  NOTES
    This will force any other threads that uses the table to release it
    as soon as possible.

  REQUIREMENTS
    One must have a lock on LOCK_open !

  RETURN:
    < 0 error
    == 0 table locked
    > 0  table locked, but someone is using it
*/

int lock_table_name(THD *thd, TABLE_LIST *table_list)
{
  TABLE *table;
  char  key[MAX_DBKEY_LENGTH];
  char *db= table_list->db;
  uint  key_length;
  DBUG_ENTER("lock_table_name");
  DBUG_PRINT("enter",("db: %s  name: %s", db, table_list->table_name));

  safe_mutex_assert_owner(&LOCK_open);

  key_length=(uint) (strmov(strmov(key,db)+1,table_list->table_name)
		     -key)+ 1;


  /* Only insert the table if we haven't insert it already */
  for (table=(TABLE*) hash_search(&open_cache,(byte*) key,key_length) ;
       table ;
       table = (TABLE*) hash_next(&open_cache,(byte*) key,key_length))
    if (table->in_use == thd)
      DBUG_RETURN(0);

  /*
    Create a table entry with the right key and with an old refresh version
    Note that we must use my_malloc() here as this is freed by the table
    cache
  */
  if (!(table= (TABLE*) my_malloc(sizeof(*table)+key_length,
				  MYF(MY_WME | MY_ZEROFILL))))
    DBUG_RETURN(-1);
  table->s= &table->share_not_to_be_used;
  memcpy((table->s->table_cache_key= (char*) (table+1)), key, key_length);
  table->s->key_length=key_length;
  table->in_use=thd;
  table->locked_by_name=1;
  table_list->table=table;

  if (my_hash_insert(&open_cache, (byte*) table))
  {
    my_free((gptr) table,MYF(0));
    DBUG_RETURN(-1);
  }
  if (remove_table_from_cache(thd, db, table_list->table_name, 0))
    DBUG_RETURN(1);					// Table is in use
  DBUG_RETURN(0);
}


void unlock_table_name(THD *thd, TABLE_LIST *table_list)
{
  if (table_list->table)
  {
    hash_delete(&open_cache, (byte*) table_list->table);
    (void) pthread_cond_broadcast(&COND_refresh);
  }
}


static bool locked_named_table(THD *thd, TABLE_LIST *table_list)
{
  for (; table_list ; table_list=table_list->next_local)
  {
    if (table_list->table && table_is_used(table_list->table,0))
      return 1;
  }
  return 0;					// All tables are locked
}


bool wait_for_locked_table_names(THD *thd, TABLE_LIST *table_list)
{
  bool result=0;
  DBUG_ENTER("wait_for_locked_table_names");
  safe_mutex_assert_owner(&LOCK_open);

  while (locked_named_table(thd,table_list))
  {
    if (thd->killed)
    {
      result=1;
      break;
    }
    wait_for_refresh(thd);
    pthread_mutex_lock(&LOCK_open);
  }
  DBUG_RETURN(result);
}


/*
  Lock all tables in list with a name lock

  SYNOPSIS
    lock_table_names()
    thd			Thread handle
    table_list		Names of tables to lock

  NOTES
    If you are just locking one table, you should use
    lock_and_wait_for_table_name().

  REQUIREMENTS
    One must have a lock on LOCK_open when calling this

  RETURN
    0	ok
    1	Fatal error (end of memory ?)
*/

bool lock_table_names(THD *thd, TABLE_LIST *table_list)
{
  bool got_all_locks=1;
  TABLE_LIST *lock_table;

  for (lock_table= table_list; lock_table; lock_table= lock_table->next_local)
  {
    int got_lock;
    if ((got_lock=lock_table_name(thd,lock_table)) < 0)
      goto end;					// Fatal error
    if (got_lock)
      got_all_locks=0;				// Someone is using table
  }

  /* If some table was in use, wait until we got the lock */
  if (!got_all_locks && wait_for_locked_table_names(thd, table_list))
    goto end;
  return 0;

end:
  unlock_table_names(thd, table_list, lock_table);
  return 1;
}


/*
  Unlock all tables in list with a name lock

  SYNOPSIS
    unlock_table_names()
    thd			Thread handle
    table_list		Names of tables to unlock
    last_table		Don't unlock any tables after this one.
			(default 0, which will unlock all tables)

  NOTES
    One must have a lock on LOCK_open when calling this
    This function will send a COND_refresh signal to inform other threads
    that the name locks are removed

  RETURN
    0	ok
    1	Fatal error (end of memory ?)
*/

void unlock_table_names(THD *thd, TABLE_LIST *table_list,
			TABLE_LIST *last_table)
{
  for (TABLE_LIST *table= table_list;
       table != last_table;
       table= table->next_local)
    unlock_table_name(thd,table);
  pthread_cond_broadcast(&COND_refresh);
}


static void print_lock_error(int error, const char *table)
{
  int textno;
  DBUG_ENTER("print_lock_error");

  switch (error) {
  case HA_ERR_LOCK_WAIT_TIMEOUT:
    textno=ER_LOCK_WAIT_TIMEOUT;
    break;
  case HA_ERR_READ_ONLY_TRANSACTION:
    textno=ER_READ_ONLY_TRANSACTION;
    break;
  case HA_ERR_LOCK_DEADLOCK:
    textno=ER_LOCK_DEADLOCK;
    break;
  case HA_ERR_WRONG_COMMAND:
    textno=ER_ILLEGAL_HA;
    break;
  default:
    textno=ER_CANT_LOCK;
    break;
  }

  if ( textno == ER_ILLEGAL_HA )
    my_error(textno, MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG), table);
  else
    my_error(textno, MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG), error);

  DBUG_VOID_RETURN;
}


/****************************************************************************
  Handling of global read locks

  Taking the global read lock is TWO steps (2nd step is optional; without
  it, COMMIT of existing transactions will be allowed):
  lock_global_read_lock() THEN make_global_read_lock_block_commit().

  The global locks are handled through the global variables:
  global_read_lock
    count of threads which have the global read lock (i.e. have completed at
    least the first step above)
  global_read_lock_blocks_commit
    count of threads which have the global read lock and block
    commits (i.e. are in or have completed the second step above)
  waiting_for_read_lock
    count of threads which want to take a global read lock but cannot
  protect_against_global_read_lock
    count of threads which have set protection against global read lock.

  access to them is protected with a mutex LOCK_global_read_lock

  (XXX: one should never take LOCK_open if LOCK_global_read_lock is taken,
  otherwise a deadlock may occur - see mysql_rm_table. Other mutexes could
  be a problem too - grep the code for global_read_lock if you want to use
  any other mutex here)

  How blocking of threads by global read lock is achieved: that's
  advisory. Any piece of code which should be blocked by global read lock must
  be designed like this:
  - call to wait_if_global_read_lock(). When this returns 0, no global read
  lock is owned; if argument abort_on_refresh was 0, none can be obtained.
  - job
  - if abort_on_refresh was 0, call to start_waiting_global_read_lock() to
  allow other threads to get the global read lock. I.e. removal of the
  protection.
  (Note: it's a bit like an implementation of rwlock).

  [ I am sorry to mention some SQL syntaxes below I know I shouldn't but found
  no better descriptive way ]

  Why does FLUSH TABLES WITH READ LOCK need to block COMMIT: because it's used
  to read a non-moving SHOW MASTER STATUS, and a COMMIT writes to the binary
  log.

  Why getting the global read lock is two steps and not one. Because FLUSH
  TABLES WITH READ LOCK needs to insert one other step between the two:
  flushing tables. So the order is
  1) lock_global_read_lock() (prevents any new table write locks, i.e. stalls
  all new updates)
  2) close_cached_tables() (the FLUSH TABLES), which will wait for tables
  currently opened and being updated to close (so it's possible that there is
  a moment where all new updates of server are stalled *and* FLUSH TABLES WITH
  READ LOCK is, too).
  3) make_global_read_lock_block_commit().
  If we have merged 1) and 3) into 1), we would have had this deadlock:
  imagine thread 1 and 2, in non-autocommit mode, thread 3, and an InnoDB
  table t.
  thd1: SELECT * FROM t FOR UPDATE;
  thd2: UPDATE t SET a=1; # blocked by row-level locks of thd1
  thd3: FLUSH TABLES WITH READ LOCK; # blocked in close_cached_tables() by the
  table instance of thd2
  thd1: COMMIT; # blocked by thd3.
  thd1 blocks thd2 which blocks thd3 which blocks thd1: deadlock.

  Note that we need to support that one thread does
  FLUSH TABLES WITH READ LOCK; and then COMMIT;
  (that's what innobackup does, for some good reason).
  So in this exceptional case the COMMIT should not be blocked by the FLUSH
  TABLES WITH READ LOCK.

  TODO in MySQL 5.x: make_global_read_lock_block_commit() should be
  killable. Normally CPU does not spend a long time in this function (COMMITs
  are quite fast), but it would still be nice.

****************************************************************************/

volatile uint global_read_lock=0;
volatile uint global_read_lock_blocks_commit=0;
static volatile uint protect_against_global_read_lock=0;
static volatile uint waiting_for_read_lock=0;

#define GOT_GLOBAL_READ_LOCK               1
#define MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT 2

bool lock_global_read_lock(THD *thd)
{
  DBUG_ENTER("lock_global_read_lock");

  if (!thd->global_read_lock)
  {
    (void) pthread_mutex_lock(&LOCK_global_read_lock);
    const char *old_message=thd->enter_cond(&COND_refresh, &LOCK_global_read_lock,
					    "Waiting to get readlock");
    DBUG_PRINT("info",
	       ("waiting_for: %d  protect_against: %d",
		waiting_for_read_lock, protect_against_global_read_lock));

    waiting_for_read_lock++;
    while (protect_against_global_read_lock && !thd->killed)
      pthread_cond_wait(&COND_refresh, &LOCK_global_read_lock);
    waiting_for_read_lock--;
    if (thd->killed)
    {
      thd->exit_cond(old_message);
      DBUG_RETURN(1);
    }
    thd->global_read_lock= GOT_GLOBAL_READ_LOCK;
    global_read_lock++;
    thd->exit_cond(old_message); // this unlocks LOCK_global_read_lock
  }
  /*
    We DON'T set global_read_lock_blocks_commit now, it will be set after
    tables are flushed (as the present function serves for FLUSH TABLES WITH
    READ LOCK only). Doing things in this order is necessary to avoid
    deadlocks (we must allow COMMIT until all tables are closed; we should not
    forbid it before, or we can have a 3-thread deadlock if 2 do SELECT FOR
    UPDATE and one does FLUSH TABLES WITH READ LOCK).
  */
  DBUG_RETURN(0);
}

void unlock_global_read_lock(THD *thd)
{
  uint tmp;
  pthread_mutex_lock(&LOCK_global_read_lock);
  tmp= --global_read_lock;
  if (thd->global_read_lock == MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT)
    --global_read_lock_blocks_commit;
  pthread_mutex_unlock(&LOCK_global_read_lock);
  /* Send the signal outside the mutex to avoid a context switch */
  if (!tmp)
    pthread_cond_broadcast(&COND_refresh);
  thd->global_read_lock= 0;
}

#define must_wait (global_read_lock &&                             \
                   (is_not_commit ||                               \
                    global_read_lock_blocks_commit))

bool wait_if_global_read_lock(THD *thd, bool abort_on_refresh,
                              bool is_not_commit)
{
  const char *old_message;
  bool result= 0, need_exit_cond;
  DBUG_ENTER("wait_if_global_read_lock");

  LINT_INIT(old_message);
  (void) pthread_mutex_lock(&LOCK_global_read_lock);
  if ((need_exit_cond= must_wait))
  {
    if (thd->global_read_lock)		// This thread had the read locks
    {
      if (is_not_commit)
        my_message(ER_CANT_UPDATE_WITH_READLOCK,
                   ER(ER_CANT_UPDATE_WITH_READLOCK), MYF(0));
      (void) pthread_mutex_unlock(&LOCK_global_read_lock);
      /*
        We allow FLUSHer to COMMIT; we assume FLUSHer knows what it does.
        This allowance is needed to not break existing versions of innobackup
        which do a BEGIN; INSERT; FLUSH TABLES WITH READ LOCK; COMMIT.
      */
      DBUG_RETURN(is_not_commit);
    }
    old_message=thd->enter_cond(&COND_refresh, &LOCK_global_read_lock,
				"Waiting for release of readlock");
    while (must_wait && ! thd->killed &&
	   (!abort_on_refresh || thd->version == refresh_version))
      (void) pthread_cond_wait(&COND_refresh,&LOCK_global_read_lock);
    if (thd->killed)
      result=1;
  }
  if (!abort_on_refresh && !result)
    protect_against_global_read_lock++;
  /*
    The following is only true in case of a global read locks (which is rare)
    and if old_message is set
  */
  if (unlikely(need_exit_cond))
    thd->exit_cond(old_message); // this unlocks LOCK_global_read_lock
  else
    pthread_mutex_unlock(&LOCK_global_read_lock);
  DBUG_RETURN(result);
}


void start_waiting_global_read_lock(THD *thd)
{
  bool tmp;
  DBUG_ENTER("start_waiting_global_read_lock");
  if (unlikely(thd->global_read_lock))
    DBUG_VOID_RETURN;
  (void) pthread_mutex_lock(&LOCK_global_read_lock);
  tmp= (!--protect_against_global_read_lock &&
        (waiting_for_read_lock || global_read_lock_blocks_commit));
  (void) pthread_mutex_unlock(&LOCK_global_read_lock);
  if (tmp)
    pthread_cond_broadcast(&COND_refresh);
  DBUG_VOID_RETURN;
}


bool make_global_read_lock_block_commit(THD *thd)
{
  bool error;
  const char *old_message;
  DBUG_ENTER("make_global_read_lock_block_commit");
  /*
    If we didn't succeed lock_global_read_lock(), or if we already suceeded
    make_global_read_lock_block_commit(), do nothing.
  */
  if (thd->global_read_lock != GOT_GLOBAL_READ_LOCK)
    DBUG_RETURN(1);
  pthread_mutex_lock(&LOCK_global_read_lock);
  /* increment this BEFORE waiting on cond (otherwise race cond) */
  global_read_lock_blocks_commit++;
  /* For testing we set up some blocking, to see if we can be killed */
  DBUG_EXECUTE_IF("make_global_read_lock_block_commit_loop",
                  protect_against_global_read_lock++;);
  old_message= thd->enter_cond(&COND_refresh, &LOCK_global_read_lock,
                               "Waiting for all running commits to finish");
  while (protect_against_global_read_lock && !thd->killed)
    pthread_cond_wait(&COND_refresh, &LOCK_global_read_lock);
  DBUG_EXECUTE_IF("make_global_read_lock_block_commit_loop",
                  protect_against_global_read_lock--;);
  if (error= thd->killed)
    global_read_lock_blocks_commit--; // undo what we did
  else
    thd->global_read_lock= MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT;
  thd->exit_cond(old_message); // this unlocks LOCK_global_read_lock
  DBUG_RETURN(error);
}


