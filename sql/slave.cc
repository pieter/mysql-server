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


#include "mysql_priv.h"
#include <mysql.h>
#include <myisam.h>
#include "mini_client.h"
#include "slave.h"
#include "sql_repl.h"
#include "repl_failsafe.h"
#include <thr_alarm.h>
#include <my_dir.h>
#include <assert.h>

bool use_slave_mask = 0;
MY_BITMAP slave_error_mask;

typedef bool (*CHECK_KILLED_FUNC)(THD*,void*);

volatile bool slave_sql_running = 0, slave_io_running = 0;
char* slave_load_tmpdir = 0;
MASTER_INFO main_mi;
MASTER_INFO* active_mi;
volatile int active_mi_in_use = 0;
HASH replicate_do_table, replicate_ignore_table;
DYNAMIC_ARRAY replicate_wild_do_table, replicate_wild_ignore_table;
bool do_table_inited = 0, ignore_table_inited = 0;
bool wild_do_table_inited = 0, wild_ignore_table_inited = 0;
bool table_rules_on = 0;
static TABLE* save_temporary_tables = 0;
/* TODO: fix variables to access ulonglong values and make it ulonglong */
ulong relay_log_space_limit = 0;

/*
  When slave thread exits, we need to remember the temporary tables so we
  can re-use them on slave start.

  TODO: move the vars below under MASTER_INFO
*/

int disconnect_slave_event_count = 0, abort_slave_event_count = 0;
static int events_till_disconnect = -1;
int events_till_abort = -1;
static int stuck_count = 0;

typedef enum { SLAVE_THD_IO, SLAVE_THD_SQL} SLAVE_THD_TYPE;

void skip_load_data_infile(NET* net);
static int process_io_rotate(MASTER_INFO* mi, Rotate_log_event* rev);
static int process_io_create_file(MASTER_INFO* mi, Create_file_log_event* cev);
static int queue_old_event(MASTER_INFO* mi, const char* buf,
			   uint event_len);
static bool wait_for_relay_log_space(RELAY_LOG_INFO* rli);
static inline bool io_slave_killed(THD* thd,MASTER_INFO* mi);
static inline bool sql_slave_killed(THD* thd,RELAY_LOG_INFO* rli);
static int count_relay_log_space(RELAY_LOG_INFO* rli);
static int init_slave_thread(THD* thd, SLAVE_THD_TYPE thd_type);
static int safe_connect(THD* thd, MYSQL* mysql, MASTER_INFO* mi);
static int safe_reconnect(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			  bool suppress_warnings);
static int connect_to_master(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			     bool reconnect, bool suppress_warnings);
static int safe_sleep(THD* thd, int sec, CHECK_KILLED_FUNC thread_killed,
		      void* thread_killed_arg);
static int request_table_dump(MYSQL* mysql, const char* db, const char* table);
static int create_table_from_dump(THD* thd, NET* net, const char* db,
				  const char* table_name);
static int check_master_version(MYSQL* mysql, MASTER_INFO* mi);
char* rewrite_db(char* db);


/*
  Get a bit mask for which threads are running so that we later can
  restart these threads
*/


void init_thread_mask(int* mask,MASTER_INFO* mi,bool inverse)
{
  bool set_io = mi->slave_running, set_sql = mi->rli.slave_running;
  if (inverse)
  {
    /*
      This makes me think of the Russian idiom "I am not I, and this is
      not my horse", which is used to deny reponsibility for
      one's actions. 
    */
    set_io = !set_io;
    set_sql = !set_sql;
  }
  register int tmp_mask=0;
  if (set_io)
    tmp_mask |= SLAVE_IO;
  if (set_sql)
    tmp_mask |= SLAVE_SQL;
  *mask = tmp_mask;
}


void lock_slave_threads(MASTER_INFO* mi)
{
  //TODO: see if we can do this without dual mutex
  pthread_mutex_lock(&mi->run_lock);
  pthread_mutex_lock(&mi->rli.run_lock);
}

void unlock_slave_threads(MASTER_INFO* mi)
{
  //TODO: see if we can do this without dual mutex
  pthread_mutex_unlock(&mi->rli.run_lock);
  pthread_mutex_unlock(&mi->run_lock);
}


int init_slave()
{
  DBUG_ENTER("init_slave");

  /*
    TODO: re-write this to interate through the list of files
    for multi-master
  */
  active_mi = &main_mi;

  /*
    If master_host is not specified, try to read it from the master_info file.
    If master_host is specified, create the master_info file if it doesn't
    exists.
  */
  if (init_master_info(active_mi,master_info_file,relay_log_info_file,
		       !master_host))
  {
    sql_print_error("Warning: failed to initialized master info");
    DBUG_RETURN(0);
  }

  /*
    make sure slave thread gets started if server_id is set,
    valid master.info is present, and master_host has not been specified
  */
  if (server_id && !master_host && active_mi->host[0])
    master_host= active_mi->host;

  if (master_host && !opt_skip_slave_start)
  {
    if (start_slave_threads(1 /* need mutex */,
			    0 /* no wait for start*/,
			    active_mi,
			    master_info_file,
			    relay_log_info_file,
			    SLAVE_IO | SLAVE_SQL))
      sql_print_error("Warning: Can't create threads to handle slave");
  }
  DBUG_RETURN(0);
}


static void free_table_ent(TABLE_RULE_ENT* e)
{
  my_free((gptr) e, MYF(0));
}

static byte* get_table_key(TABLE_RULE_ENT* e, uint* len,
			   my_bool not_used __attribute__((unused)))
{
  *len = e->key_len;
  return (byte*)e->db;
}


/*
  Open the given relay log

  SYNOPSIS
    init_relay_log_pos()
    rli			Relay information (will be initialized)
    log			Name of relay log file to read from. NULL = First log
    pos			Position in relay log file 
    need_data_lock	Set to 1 if this functions should do mutex locks
    errmsg		Store pointer to error message here

  DESCRIPTION
  - Close old open relay log files.
  - If we are using the same relay log as the running IO-thread, then set
    rli->cur_log to point to the same IO_CACHE entry.
  - If not, open the 'log' binary file.

  TODO
    - check proper initialization of master_log_name/master_log_pos
    - We may always want to delete all logs before 'log'.
      Currently if we are not calling this with 'log' as NULL or the first
      log we will never delete relay logs.
      If we want this we should not set skip_log_purge to 1.

  RETURN VALUES
    0	ok
    1	error.  errmsg is set to point to the error message
*/

int init_relay_log_pos(RELAY_LOG_INFO* rli,const char* log,
		       ulonglong pos, bool need_data_lock,
		       const char** errmsg)
{
  DBUG_ENTER("init_relay_log_pos");

  *errmsg=0;
  if (rli->log_pos_current)			// TODO: When can this happen ?
    DBUG_RETURN(0);
  pthread_mutex_t *log_lock=rli->relay_log.get_log_lock();
  pthread_mutex_lock(log_lock);
  if (need_data_lock)
    pthread_mutex_lock(&rli->data_lock);
  
  /* Close log file and free buffers if it's already open */
  if (rli->cur_log_fd >= 0)
  {
    end_io_cache(&rli->cache_buf);
    my_close(rli->cur_log_fd, MYF(MY_WME));
    rli->cur_log_fd = -1;
  }
  
  rli->relay_log_pos = pos;

  /*
    Test to see if the previous run was with the skip of purging
    If yes, we do not purge when we restart
  */
  if (rli->relay_log.find_log_pos(&rli->linfo,NullS))
  {
    *errmsg="Could not find first log during relay log initialization";
    goto err;
  }

  if (log)					// If not first log
  {
    if (strcmp(log, rli->linfo.log_file_name))
      rli->skip_log_purge=1;			// Different name; Don't purge
    if (rli->relay_log.find_log_pos(&rli->linfo, log))
    {
      *errmsg="Could not find target log during relay log initialization";
      goto err;
    }
  }
  strmake(rli->relay_log_name,rli->linfo.log_file_name,
	  sizeof(rli->relay_log_name)-1);
  if (rli->relay_log.is_active(rli->linfo.log_file_name))
  {
    /*
      The IO thread is using this log file.
      In this case, we will use the same IO_CACHE pointer to
      read data as the IO thread is using to write data.
    */
    if (my_b_tell((rli->cur_log=rli->relay_log.get_log_file())) == 0 &&
	check_binlog_magic(rli->cur_log,errmsg))
      goto err;
    rli->cur_log_old_open_count=rli->relay_log.get_open_count();
  }
  else
  {
    /*
      Open the relay log and set rli->cur_log to point at this one
    */
    if ((rli->cur_log_fd=open_binlog(&rli->cache_buf,
				     rli->linfo.log_file_name,errmsg)) < 0)
      goto err;
    rli->cur_log = &rli->cache_buf;
  }
  if (pos > BIN_LOG_HEADER_SIZE)
    my_b_seek(rli->cur_log,(off_t)pos);
  rli->log_pos_current=1;

err:
  pthread_cond_broadcast(&rli->data_cond);
  if (need_data_lock)
    pthread_mutex_unlock(&rli->data_lock);
  pthread_mutex_unlock(log_lock);
  DBUG_RETURN ((*errmsg) ? 1 : 0);
}


/* called from get_options() in mysqld.cc on start-up */

void init_slave_skip_errors(const char* arg)
{
  const char *p;
  my_bool last_was_digit = 0;
  if (bitmap_init(&slave_error_mask,MAX_SLAVE_ERROR,0))
  {
    fprintf(stderr, "Badly out of memory, please check your system status\n");
    exit(1);
  }
  use_slave_mask = 1;
  for (;isspace(*arg);++arg)
    /* empty */;
  if (!my_casecmp(arg,"all",3))
  {
    bitmap_set_all(&slave_error_mask);
    return;
  }
  for (p= arg ; *p; )
  {
    long err_code;
    if (!(p= str2int(p, 10, 0, LONG_MAX, &err_code)))
      break;
    if (err_code < MAX_SLAVE_ERROR)
       bitmap_set_bit(&slave_error_mask,(uint)err_code);
    while (!isdigit(*p) && *p)
      p++;
  }
}


/*
  We assume we have a run lock on rli and that both slave thread
  are not running
*/

int purge_relay_logs(RELAY_LOG_INFO* rli, THD *thd, bool just_reset,
		     const char** errmsg)
{
  int error=0;
  DBUG_ENTER("purge_relay_logs");
  if (!rli->inited)
    DBUG_RETURN(0); /* successfully do nothing */

  DBUG_ASSERT(rli->slave_running == 0);
  DBUG_ASSERT(rli->mi->slave_running == 0);

  rli->slave_skip_counter=0;
  pthread_mutex_lock(&rli->data_lock);
  rli->pending=0;
  rli->master_log_name[0]=0;
  rli->master_log_pos=0;			// 0 means uninitialized
  if (rli->relay_log.reset_logs(thd))
  {
    *errmsg = "Failed during log reset";
    error=1;
    goto err;
  }
  /* Save name of used relay log file */
  strmake(rli->relay_log_name, rli->relay_log.get_log_fname(),
	  sizeof(rli->relay_log_name)-1);
  // Just first log with magic number and nothing else
  rli->log_space_total= BIN_LOG_HEADER_SIZE;
  rli->relay_log_pos=   BIN_LOG_HEADER_SIZE;
  rli->relay_log.reset_bytes_written();
  rli->log_pos_current=0;
  if (!just_reset)
    error= init_relay_log_pos(rli, rli->relay_log_name, rli->relay_log_pos,
			      0 /* do not need data lock */, errmsg);

err:
#ifndef DBUG_OFF
  char buf[22];
#endif  
  DBUG_PRINT("info",("log_space_total: %s",llstr(rli->log_space_total,buf)));
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_RETURN(error);
}


int terminate_slave_threads(MASTER_INFO* mi,int thread_mask,bool skip_lock)
{
  if (!mi->inited)
    return 0; /* successfully do nothing */
  int error,force_all = (thread_mask & SLAVE_FORCE_ALL);
  pthread_mutex_t *sql_lock = &mi->rli.run_lock, *io_lock = &mi->run_lock;
  pthread_mutex_t *sql_cond_lock,*io_cond_lock;

  sql_cond_lock=sql_lock;
  io_cond_lock=io_lock;
  
  if (skip_lock)
  {
    sql_lock = io_lock = 0;
  }
  if ((thread_mask & (SLAVE_IO|SLAVE_FORCE_ALL)) && mi->slave_running)
  {
    mi->abort_slave=1;
    if ((error=terminate_slave_thread(mi->io_thd,io_lock,
				        io_cond_lock,
					&mi->stop_cond,
					&mi->slave_running)) &&
	!force_all)
      return error;
  }
  if ((thread_mask & (SLAVE_SQL|SLAVE_FORCE_ALL)) && mi->rli.slave_running)
  {
    DBUG_ASSERT(mi->rli.sql_thd != 0) ;
    mi->rli.abort_slave=1;
    if ((error=terminate_slave_thread(mi->rli.sql_thd,sql_lock,
				      sql_cond_lock,
				      &mi->rli.stop_cond,
				      &mi->rli.slave_running)) &&
	!force_all)
      return error;
  }
  return 0;
}


int terminate_slave_thread(THD* thd, pthread_mutex_t* term_lock,
			   pthread_mutex_t *cond_lock,
			   pthread_cond_t* term_cond,
			   volatile bool* slave_running)
{
  if (term_lock)
  {
    pthread_mutex_lock(term_lock);
    if (!*slave_running)
    {
      pthread_mutex_unlock(term_lock);
      return ER_SLAVE_NOT_RUNNING;
    }
  }
  DBUG_ASSERT(thd != 0);
  /*
    Is is criticate to test if the slave is running. Otherwise, we might
    be referening freed memory trying to kick it
  */
  THD_CHECK_SENTRY(thd);
  if (*slave_running)
  {
    KICK_SLAVE(thd);
  }
  while (*slave_running)
  {
    /*
      There is a small chance that slave thread might miss the first
      alarm. To protect againts it, resend the signal until it reacts
    */
    struct timespec abstime;
    set_timespec(abstime,2);
    pthread_cond_timedwait(term_cond, cond_lock, &abstime);
    if (*slave_running)
    {
      KICK_SLAVE(thd);
    }
  }
  if (term_lock)
    pthread_mutex_unlock(term_lock);
  return 0;
}


int start_slave_thread(pthread_handler h_func, pthread_mutex_t* start_lock,
		       pthread_mutex_t *cond_lock,
		       pthread_cond_t* start_cond,
		       volatile bool* slave_running,
		       MASTER_INFO* mi)
{
  pthread_t th;
  DBUG_ASSERT(mi->inited);
  if (start_lock)
    pthread_mutex_lock(start_lock);
  if (!server_id)
  {
    if (start_cond)
      pthread_cond_broadcast(start_cond);
    if (start_lock)
      pthread_mutex_unlock(start_lock);
    sql_print_error("Server id not set, will not start slave");
    return ER_BAD_SLAVE;
  }
  
  if (*slave_running)
  {
    if (start_cond)
      pthread_cond_broadcast(start_cond);
    if (start_lock)
      pthread_mutex_unlock(start_lock);
    return ER_SLAVE_MUST_STOP;
  }
  if (pthread_create(&th, &connection_attrib, h_func, (void*)mi))
  {
    if (start_lock)
      pthread_mutex_unlock(start_lock);
    return ER_SLAVE_THREAD;
  }
  if (start_cond && cond_lock)
  {
    THD* thd = current_thd;
    while (!*slave_running)
    {
      const char* old_msg = thd->enter_cond(start_cond,cond_lock,
					    "Waiting for slave thread to start");
      pthread_cond_wait(start_cond,cond_lock);
      thd->exit_cond(old_msg);
      /*
	TODO: in a very rare case of init_slave_thread failing, it is
	possible that we can get stuck here since slave_running will not
	be set. We need to change slave_running to int and have -1 as
	error code.
      */
      if (thd->killed)
      {
	pthread_mutex_unlock(cond_lock);
	return ER_SERVER_SHUTDOWN;
      }
    }
  }
  if (start_lock)
    pthread_mutex_unlock(start_lock);
  return 0;
}


/*
  SLAVE_FORCE_ALL is not implemented here on purpose since it does not make
  sense to do that for starting a slave - we always care if it actually
  started the threads that were not previously running
*/

int start_slave_threads(bool need_slave_mutex, bool wait_for_start,
			MASTER_INFO* mi, const char* master_info_fname,
			const char* slave_info_fname, int thread_mask)
{
  pthread_mutex_t *lock_io=0,*lock_sql=0,*lock_cond_io=0,*lock_cond_sql=0;
  pthread_cond_t* cond_io=0,*cond_sql=0;
  int error=0;
  DBUG_ENTER("start_slave_threads");
  
  if (need_slave_mutex)
  {
    lock_io = &mi->run_lock;
    lock_sql = &mi->rli.run_lock;
  }
  if (wait_for_start)
  {
    cond_io = &mi->start_cond;
    cond_sql = &mi->rli.start_cond;
    lock_cond_io = &mi->run_lock;
    lock_cond_sql = &mi->rli.run_lock;
  }

  if (thread_mask & SLAVE_IO)
    error=start_slave_thread(handle_slave_io,lock_io,lock_cond_io,
			     cond_io,&mi->slave_running,
			     mi);
  if (!error && (thread_mask & SLAVE_SQL))
    error=start_slave_thread(handle_slave_sql,lock_sql,lock_cond_sql,
			     cond_sql,
			     &mi->rli.slave_running,mi);
  DBUG_RETURN(error);
}


void init_table_rule_hash(HASH* h, bool* h_inited)
{
  hash_init(h, TABLE_RULE_HASH_SIZE,0,0,
	    (hash_get_key) get_table_key,
	    (void (*)(void*)) free_table_ent, 0);
  *h_inited = 1;
}

void init_table_rule_array(DYNAMIC_ARRAY* a, bool* a_inited)
{
  my_init_dynamic_array(a, sizeof(TABLE_RULE_ENT*), TABLE_RULE_ARR_SIZE,
		     TABLE_RULE_ARR_SIZE);
  *a_inited = 1;
}

static TABLE_RULE_ENT* find_wild(DYNAMIC_ARRAY *a, const char* key, int len)
{
  uint i;
  const char* key_end = key + len;
  
  for (i = 0; i < a->elements; i++)
    {
      TABLE_RULE_ENT* e ;
      get_dynamic(a, (gptr)&e, i);
      if (!wild_case_compare(key, key_end, (const char*)e->db,
			    (const char*)(e->db + e->key_len),'\\'))
	return e;
    }
  
  return 0;
}

int tables_ok(THD* thd, TABLE_LIST* tables)
{
  for (; tables; tables = tables->next)
  {
    if (!tables->updating) 
      continue;
    char hash_key[2*NAME_LEN+2];
    char* p;
    p = strmov(hash_key, tables->db ? tables->db : thd->db);
    *p++ = '.';
    uint len = strmov(p, tables->real_name) - hash_key ;
    if (do_table_inited) // if there are any do's
    {
      if (hash_search(&replicate_do_table, (byte*) hash_key, len))
	return 1;
    }
    if (ignore_table_inited) // if there are any ignores
    {
      if (hash_search(&replicate_ignore_table, (byte*) hash_key, len))
	return 0; 
    }
    if (wild_do_table_inited && find_wild(&replicate_wild_do_table,
					  hash_key, len))
      return 1;
    if (wild_ignore_table_inited && find_wild(&replicate_wild_ignore_table,
					      hash_key, len))
      return 0;
  }

  /*
    If no explicit rule found and there was a do list, do not replicate.
    If there was no do list, go ahead
  */
  return !do_table_inited && !wild_do_table_inited;
}


int add_table_rule(HASH* h, const char* table_spec)
{
  const char* dot = strchr(table_spec, '.');
  if (!dot) return 1;
  // len is always > 0 because we know the there exists a '.'
  uint len = (uint)strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if (!e) return 1;
  e->db = (char*)e + sizeof(TABLE_RULE_ENT);
  e->tbl_name = e->db + (dot - table_spec) + 1;
  e->key_len = len;
  memcpy(e->db, table_spec, len);
  (void)hash_insert(h, (byte*)e);
  return 0;
}

int add_wild_table_rule(DYNAMIC_ARRAY* a, const char* table_spec)
{
  const char* dot = strchr(table_spec, '.');
  if (!dot) return 1;
  uint len = (uint)strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if (!e) return 1;
  e->db = (char*)e + sizeof(TABLE_RULE_ENT);
  e->tbl_name = e->db + (dot - table_spec) + 1;
  e->key_len = len;
  memcpy(e->db, table_spec, len);
  insert_dynamic(a, (gptr)&e);
  return 0;
}

static void free_string_array(DYNAMIC_ARRAY *a)
{
  uint i;
  for (i = 0; i < a->elements; i++)
    {
      char* p;
      get_dynamic(a, (gptr) &p, i);
      my_free(p, MYF(MY_WME));
    }
  delete_dynamic(a);
}

static int end_slave_on_walk(MASTER_INFO* mi, gptr /*unused*/)
{
  end_master_info(mi);
  return 0;
}

void end_slave()
{
  /*
    TODO: replace the line below with
    list_walk(&master_list, (list_walk_action)end_slave_on_walk,0);
    once multi-master code is ready.
  */
  terminate_slave_threads(active_mi,SLAVE_FORCE_ALL);
  end_master_info(active_mi);
  if (do_table_inited)
    hash_free(&replicate_do_table);
  if (ignore_table_inited)
    hash_free(&replicate_ignore_table);
  if (wild_do_table_inited)
    free_string_array(&replicate_wild_do_table);
  if (wild_ignore_table_inited)
    free_string_array(&replicate_wild_ignore_table);
}


static bool io_slave_killed(THD* thd, MASTER_INFO* mi)
{
  DBUG_ASSERT(mi->io_thd == thd);
  DBUG_ASSERT(mi->slave_running == 1); // tracking buffer overrun
  return mi->abort_slave || abort_loop || thd->killed;
}


static bool sql_slave_killed(THD* thd, RELAY_LOG_INFO* rli)
{
  DBUG_ASSERT(rli->sql_thd == thd);
  DBUG_ASSERT(rli->slave_running == 1);// tracking buffer overrun
  return rli->abort_slave || abort_loop || thd->killed;
}


void slave_print_error(RELAY_LOG_INFO* rli, int err_code, const char* msg, ...)
{
  va_list args;
  va_start(args,msg);
  my_vsnprintf(rli->last_slave_error,
	       sizeof(rli->last_slave_error), msg, args);
  sql_print_error("Slave: %s, error_code=%d", rli->last_slave_error,
		  err_code);
  rli->last_slave_errno = err_code;
}


void skip_load_data_infile(NET* net)
{
  (void)my_net_write(net, "\xfb/dev/null", 10);
  (void)net_flush(net);
  (void)my_net_read(net);			// discard response
  send_ok(net);					// the master expects it
}


char* rewrite_db(char* db)
{
  if (replicate_rewrite_db.is_empty() || !db)
    return db;
  I_List_iterator<i_string_pair> it(replicate_rewrite_db);
  i_string_pair* tmp;

  while ((tmp=it++))
  {
    if (!strcmp(tmp->key, db))
      return tmp->val;
  }
  return db;
}


int db_ok(const char* db, I_List<i_string> &do_list,
	  I_List<i_string> &ignore_list )
{
  if (do_list.is_empty() && ignore_list.is_empty())
    return 1; // ok to replicate if the user puts no constraints

  /*
    If the user has specified restrictions on which databases to replicate
    and db was not selected, do not replicate.
  */
  if (!db)
    return 0;

  if (!do_list.is_empty()) // if the do's are not empty
  {
    I_List_iterator<i_string> it(do_list);
    i_string* tmp;

    while ((tmp=it++))
    {
      if (!strcmp(tmp->ptr, db))
	return 1; // match
    }
    return 0;
  }
  else // there are some elements in the don't, otherwise we cannot get here
  {
    I_List_iterator<i_string> it(ignore_list);
    i_string* tmp;

    while ((tmp=it++))
    {
      if (!strcmp(tmp->ptr, db))
	return 0; // match
    }
    return 1;
  }
}


static int init_strvar_from_file(char *var, int max_size, IO_CACHE *f,
				 const char *default_val)
{
  uint length;
  if ((length=my_b_gets(f,var, max_size)))
  {
    char* last_p = var + length -1;
    if (*last_p == '\n')
      *last_p = 0; // if we stopped on newline, kill it
    else
    {
      /*
	If we truncated a line or stopped on last char, remove all chars
	up to and including newline.
      */
      int c;
      while (((c=my_b_get(f)) != '\n' && c != my_b_EOF));
    }
    return 0;
  }
  else if (default_val)
  {
    strmake(var,  default_val, max_size-1);
    return 0;
  }
  return 1;
}


static int init_intvar_from_file(int* var, IO_CACHE* f, int default_val)
{
  char buf[32];
  
  if (my_b_gets(f, buf, sizeof(buf))) 
  {
    *var = atoi(buf);
    return 0;
  }
  else if (default_val)
  {
    *var = default_val;
    return 0;
  }
  return 1;
}


static int check_master_version(MYSQL* mysql, MASTER_INFO* mi)
{
  const char* errmsg= 0;
  
  switch (*mysql->server_version) {
  case '3':
    mi->old_format = 1;
    break;
  case '4':
  case '5':
    mi->old_format = 0;
    break;
  default:
    errmsg = "Master reported unrecognized MySQL version";
    break;
  }
err:
  if (errmsg)
  {
    sql_print_error(errmsg);
    return 1;
  }
  return 0;
}


static int create_table_from_dump(THD* thd, NET* net, const char* db,
				  const char* table_name)
{
  ulong packet_len = my_net_read(net); // read create table statement
  Vio* save_vio;
  HA_CHECK_OPT check_opt;
  TABLE_LIST tables;
  int error= 1;
  handler *file;
  uint save_options;
  
  if (packet_len == packet_error)
  {
    send_error(&thd->net, ER_MASTER_NET_READ);
    return 1;
  }
  if (net->read_pos[0] == 255) // error from master
  {
    net->read_pos[packet_len] = 0;
    net_printf(&thd->net, ER_MASTER, net->read_pos + 3);
    return 1;
  }
  thd->command = COM_TABLE_DUMP;
  thd->query = sql_alloc(packet_len + 1);
  if (!thd->query)
  {
    sql_print_error("create_table_from_dump: out of memory");
    net_printf(&thd->net, ER_GET_ERRNO, "Out of memory");
    return 1;
  }
  memcpy(thd->query, net->read_pos, packet_len);
  thd->query[packet_len] = 0;
  thd->current_tablenr = 0;
  thd->query_error = 0;
  thd->net.no_send_ok = 1;
  
  /* we do not want to log create table statement */
  save_options = thd->options;
  thd->options &= ~(ulong) OPTION_BIN_LOG;
  thd->proc_info = "Creating table from master dump";
  // save old db in case we are creating in a different database
  char* save_db = thd->db;
  thd->db = (char*)db;
  mysql_parse(thd, thd->query, packet_len); // run create table
  thd->db = save_db;		// leave things the way the were before
  thd->options = save_options;
  
  if (thd->query_error)
    goto err;			// mysql_parse took care of the error send

  bzero((char*) &tables,sizeof(tables));
  tables.db = (char*)db;
  tables.name = tables.real_name = (char*)table_name;
  tables.lock_type = TL_WRITE;
  thd->proc_info = "Opening master dump table";
  if (!open_ltable(thd, &tables, TL_WRITE))
  {
    send_error(&thd->net,0,0);			// Send error from open_ltable
    sql_print_error("create_table_from_dump: could not open created table");
    goto err;
  }
  
  file = tables.table->file;
  thd->proc_info = "Reading master dump table data";
  if (file->net_read_dump(net))
  {
    net_printf(&thd->net, ER_MASTER_NET_READ);
    sql_print_error("create_table_from_dump::failed in\
 handler::net_read_dump()");
    goto err;
  }

  check_opt.init();
  check_opt.flags|= T_VERY_SILENT | T_CALC_CHECKSUM | T_QUICK;
  thd->proc_info = "Rebuilding the index on master dump table";
  /*
    We do not want repair() to spam us with messages
    just send them to the error log, and report the failure in case of
    problems.
  */
  save_vio = thd->net.vio;
  thd->net.vio = 0;
  error=file->repair(thd,&check_opt) != 0;
  thd->net.vio = save_vio;
  if (error)
    net_printf(&thd->net, ER_INDEX_REBUILD,tables.table->real_name);

err:
  close_thread_tables(thd);
  thd->net.no_send_ok = 0;
  return error; 
}

int fetch_master_table(THD* thd, const char* db_name, const char* table_name,
		       MASTER_INFO* mi, MYSQL* mysql)
{
  int error = 1;
  int fetch_errno = 0;
  bool called_connected = (mysql != NULL);
  if (!called_connected && !(mysql = mc_mysql_init(NULL)))
  { 
    sql_print_error("fetch_master_table: Error in mysql_init()");
    fetch_errno = ER_GET_ERRNO;
    goto err;
  }

  if (!called_connected)
  {
    if (connect_to_master(thd, mysql, mi))
    {
      sql_print_error("Could not connect to master while fetching table\
 '%-64s.%-64s'", db_name, table_name);
      fetch_errno = ER_CONNECT_TO_MASTER;
      goto err;
    }
  }
  if (thd->killed)
    goto err;

  if (request_table_dump(mysql, db_name, table_name))
  {
    fetch_errno = ER_GET_ERRNO;
    sql_print_error("fetch_master_table: failed on table dump request ");
    goto err;
  }

  if (create_table_from_dump(thd, &mysql->net, db_name,
			    table_name))
  { 
    // create_table_from_dump will have sent the error alread
    sql_print_error("fetch_master_table: failed on create table ");
    goto err;
  }
  error = 0;
 err:
  if (mysql && !called_connected)
    mc_mysql_close(mysql);
  if (fetch_errno && thd->net.vio)
    send_error(&thd->net, fetch_errno, "Error in fetch_master_table");
  thd->net.no_send_ok = 0; // Clear up garbage after create_table_from_dump
  return error;
}


void end_master_info(MASTER_INFO* mi)
{
  DBUG_ENTER("end_master_info");

  if (!mi->inited)
    DBUG_VOID_RETURN;
  end_relay_log_info(&mi->rli);
  if (mi->fd >= 0)
  {
    end_io_cache(&mi->file);
    (void)my_close(mi->fd, MYF(MY_WME));
    mi->fd = -1;
  }
  mi->inited = 0;

  DBUG_VOID_RETURN;
}


int init_relay_log_info(RELAY_LOG_INFO* rli, const char* info_fname)
{
  char fname[FN_REFLEN+128];
  int info_fd;
  const char* msg = 0;
  int error = 0;
  DBUG_ENTER("init_relay_log_info");

  if (rli->inited)				// Set if this function called
    DBUG_RETURN(0);
  fn_format(fname, info_fname, mysql_data_home, "", 4+32);
  pthread_mutex_lock(&rli->data_lock);
  info_fd = rli->info_fd;
  rli->pending = 0;
  rli->cur_log_fd = -1;
  rli->slave_skip_counter=0;
  rli->log_pos_current=0;
  rli->abort_pos_wait=0;
  rli->skip_log_purge=0;
  rli->log_space_limit = relay_log_space_limit;
  rli->log_space_total = 0;

  // TODO: make this work with multi-master
  if (!opt_relay_logname)
  {
    char tmp[FN_REFLEN];
    /*
      TODO: The following should be using fn_format();  We just need to
      first change fn_format() to cut the file name if it's too long.
    */
    strmake(tmp,glob_hostname,FN_REFLEN-5);
    strmov(strcend(tmp,'.'),"-relay-bin");
    opt_relay_logname=my_strdup(tmp,MYF(MY_WME));
  }
  if (open_log(&rli->relay_log, glob_hostname, opt_relay_logname,
	       "-relay-bin", opt_relaylog_index_name,
	       LOG_BIN, 1 /* read_append cache */,
	       1 /* no auto events */))
    DBUG_RETURN(1);

  /* if file does not exist */
  if (access(fname,F_OK))
  {
    /*
      If someone removed the file from underneath our feet, just close
      the old descriptor and re-create the old file
    */
    if (info_fd >= 0)
      my_close(info_fd, MYF(MY_WME));
    if ((info_fd = my_open(fname, O_CREAT|O_RDWR|O_BINARY, MYF(MY_WME))) < 0 ||
	init_io_cache(&rli->info_file, info_fd, IO_SIZE*2, READ_CACHE, 0L,0,
		      MYF(MY_WME)))
    {
      msg= current_thd->net.last_error;
      goto err;
    }

    /* Init relay log with first entry in the relay index file */
    if (init_relay_log_pos(rli,NullS,BIN_LOG_HEADER_SIZE,0 /* no data lock */,
			   &msg))
      goto err;
    rli->master_log_pos = 0;			// uninitialized
    rli->info_fd = info_fd;
  }
  else // file exists
  {
    if (info_fd >= 0)
      reinit_io_cache(&rli->info_file, READ_CACHE, 0L,0,0);
    else if ((info_fd = my_open(fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0 ||
	     init_io_cache(&rli->info_file, info_fd,
			   IO_SIZE*2, READ_CACHE, 0L, 0, MYF(MY_WME)))
    {
      if (info_fd >= 0)
	my_close(info_fd, MYF(0));
      rli->info_fd= -1;
      rli->relay_log.close(1);
      pthread_mutex_unlock(&rli->data_lock);
      DBUG_RETURN(1);
    }
      
    rli->info_fd = info_fd;
    int relay_log_pos, master_log_pos;
    if (init_strvar_from_file(rli->relay_log_name,
			      sizeof(rli->relay_log_name), &rli->info_file,
			      "") ||
       init_intvar_from_file(&relay_log_pos,
			     &rli->info_file, BIN_LOG_HEADER_SIZE) ||
       init_strvar_from_file(rli->master_log_name,
			     sizeof(rli->master_log_name), &rli->info_file,
			     "") ||
       init_intvar_from_file(&master_log_pos, &rli->info_file, 0))
    {
      msg="Error reading slave log configuration";
      goto err;
    }
    rli->relay_log_pos=  relay_log_pos;
    rli->master_log_pos= master_log_pos;

    if (init_relay_log_pos(rli,
			   rli->relay_log_name,
			   rli->relay_log_pos,
			   0 /* no data lock*/,
			   &msg))
      goto err;
  }
  DBUG_ASSERT(rli->relay_log_pos >= BIN_LOG_HEADER_SIZE);
  DBUG_ASSERT(my_b_tell(rli->cur_log) == rli->relay_log_pos);
  /*
    Now change the cache from READ to WRITE - must do this
    before flush_relay_log_info
  */
  reinit_io_cache(&rli->info_file, WRITE_CACHE,0L,0,1);
  error= flush_relay_log_info(rli);
  if (count_relay_log_space(rli))
  {
    msg="Error counting relay log space";
    goto err;
  }
  rli->inited= 1;
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_RETURN(error);

err:
  sql_print_error(msg);
  end_io_cache(&rli->info_file);
  if (info_fd >= 0)
    my_close(info_fd, MYF(0));
  rli->info_fd= -1;
  rli->relay_log.close(1);
  pthread_mutex_unlock(&rli->data_lock);
  DBUG_RETURN(1);
}


static inline int add_relay_log(RELAY_LOG_INFO* rli,LOG_INFO* linfo)
{
  MY_STAT s;
  DBUG_ENTER("add_relay_log");
  if (!my_stat(linfo->log_file_name,&s,MYF(0)))
  {
    sql_print_error("log %s listed in the index, but failed to stat",
		    linfo->log_file_name);
    DBUG_RETURN(1);
  }
  rli->log_space_total += s.st_size;
#ifndef DBUG_OFF
  char buf[22];
  DBUG_PRINT("info",("log_space_total: %s", llstr(rli->log_space_total,buf)));
#endif  
  DBUG_RETURN(0);
}


static bool wait_for_relay_log_space(RELAY_LOG_INFO* rli)
{
  bool slave_killed=0;
  MASTER_INFO* mi = rli->mi;
  const char* save_proc_info;
  THD* thd = mi->io_thd;

  DBUG_ENTER("wait_for_relay_log_space");
  pthread_mutex_lock(&rli->log_space_lock);
  save_proc_info = thd->proc_info;
  thd->proc_info = "Waiting for relay log space to free";
  while (rli->log_space_limit < rli->log_space_total &&
	 !(slave_killed=io_slave_killed(thd,mi)))
  {
    pthread_cond_wait(&rli->log_space_cond, &rli->log_space_lock);
  }
  thd->proc_info = save_proc_info;
  pthread_mutex_unlock(&rli->log_space_lock);
  DBUG_RETURN(slave_killed);
}


static int count_relay_log_space(RELAY_LOG_INFO* rli)
{
  LOG_INFO linfo;
  DBUG_ENTER("count_relay_log_space");
  rli->log_space_total = 0;
  if (rli->relay_log.find_log_pos(&linfo, NullS))
  {
    sql_print_error("Could not find first log while counting relay log space");
    DBUG_RETURN(1);
  }
  do
  {
    if (add_relay_log(rli,&linfo))
      DBUG_RETURN(1);
  } while (!rli->relay_log.find_next_log(&linfo));
  DBUG_RETURN(0);
}


int init_master_info(MASTER_INFO* mi, const char* master_info_fname,
		     const char* slave_info_fname,
		     bool abort_if_no_master_info_file)
{
  int fd,error;
  char fname[FN_REFLEN+128];
  DBUG_ENTER("init_master_info");

  if (mi->inited)
    DBUG_RETURN(0);
  mi->mysql=0;
  mi->file_id=1;
  mi->ignore_stop_event=0;
  fn_format(fname, master_info_fname, mysql_data_home, "", 4+32);

  /*
    We need a mutex while we are changing master info parameters to
    keep other threads from reading bogus info
  */

  pthread_mutex_lock(&mi->data_lock);
  fd = mi->fd;
  
  if (access(fname,F_OK))
  {
    if (abort_if_no_master_info_file)
    {
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(0);
    }
    /*
      if someone removed the file from underneath our feet, just close
      the old descriptor and re-create the old file
    */
    if (fd >= 0)
      my_close(fd, MYF(MY_WME));
    if ((fd = my_open(fname, O_CREAT|O_RDWR|O_BINARY, MYF(MY_WME))) < 0 ||
	init_io_cache(&mi->file, fd, IO_SIZE*2, READ_CACHE, 0L,0,
		      MYF(MY_WME)))
      goto err;

    mi->master_log_name[0] = 0;
    mi->master_log_pos = BIN_LOG_HEADER_SIZE;		// skip magic number
    mi->fd = fd;
      
    if (master_host)
      strmake(mi->host, master_host, sizeof(mi->host) - 1);
    if (master_user)
      strmake(mi->user, master_user, sizeof(mi->user) - 1);
    if (master_password)
      strmake(mi->password, master_password, HASH_PASSWORD_LENGTH);
    mi->port = master_port;
    mi->connect_retry = master_connect_retry;
  }
  else // file exists
  {
    if (fd >= 0)
      reinit_io_cache(&mi->file, READ_CACHE, 0L,0,0);
    else if ((fd = my_open(fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0 ||
	     init_io_cache(&mi->file, fd, IO_SIZE*2, READ_CACHE, 0L,
			   0, MYF(MY_WME)))
      goto err;

    mi->fd = fd;
    int port, connect_retry, master_log_pos;

    if (init_strvar_from_file(mi->master_log_name,
			      sizeof(mi->master_log_name), &mi->file,
			      "") ||
	init_intvar_from_file(&master_log_pos, &mi->file, 4) ||
	init_strvar_from_file(mi->host, sizeof(mi->host), &mi->file,
			      master_host) ||
	init_strvar_from_file(mi->user, sizeof(mi->user), &mi->file,
			      master_user) || 
	init_strvar_from_file(mi->password, HASH_PASSWORD_LENGTH+1, &mi->file,
			      master_password) ||
	init_intvar_from_file(&port, &mi->file, master_port) ||
	init_intvar_from_file(&connect_retry, &mi->file,
			      master_connect_retry))
    {
      sql_print_error("Error reading master configuration");
      goto err;
    }
    /*
      This has to be handled here as init_intvar_from_file can't handle
      my_off_t types
    */
    mi->master_log_pos= (my_off_t) master_log_pos;
    mi->port= (uint) port;
    mi->connect_retry= (uint) connect_retry;
  }
  DBUG_PRINT("master_info",("log_file_name: %s  position: %ld",
			    mi->master_log_name,
			    (ulong) mi->master_log_pos));

  if (init_relay_log_info(&mi->rli, slave_info_fname))
    goto err;
  mi->rli.mi = mi;

  mi->inited = 1;
  // now change cache READ -> WRITE - must do this before flush_master_info
  reinit_io_cache(&mi->file, WRITE_CACHE,0L,0,1);
  error=test(flush_master_info(mi));
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(error);

err:
  if (fd >= 0)
  {
    my_close(fd, MYF(0));
    end_io_cache(&mi->file);
  }
  mi->fd= -1;
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(1);
}


int register_slave_on_master(MYSQL* mysql)
{
  String packet;
  char buf[4];

  if (!report_host)
    return 0;
  
  int4store(buf, server_id);
  packet.append(buf, 4);

  net_store_data(&packet, report_host); 
  if (report_user)
    net_store_data(&packet, report_user);
  else
    packet.append((char)0);
  
  if (report_password)
    net_store_data(&packet, report_user);
  else
    packet.append((char)0);

  int2store(buf, (uint16)report_port);
  packet.append(buf, 2);
  int4store(buf, rpl_recovery_rank);
  packet.append(buf, 4);
  int4store(buf, 0); /* tell the master will fill in master_id */
  packet.append(buf, 4);

  if (mc_simple_command(mysql, COM_REGISTER_SLAVE, (char*)packet.ptr(),
		       packet.length(), 0))
  {
    sql_print_error("Error on COM_REGISTER_SLAVE: '%s'",
		    mc_mysql_error(mysql));
    return 1;
  }

  return 0;
}

int show_master_info(THD* thd, MASTER_INFO* mi)
{
  // TODO: fix this for multi-master
  DBUG_ENTER("show_master_info");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Master_Host",
						     sizeof(mi->host)));
  field_list.push_back(new Item_empty_string("Master_User",
						     sizeof(mi->user)));
  field_list.push_back(new Item_empty_string("Master_Port", 6));
  field_list.push_back(new Item_empty_string("Connect_retry", 6));
  field_list.push_back(new Item_empty_string("Master_Log_File",
						     FN_REFLEN));
  field_list.push_back(new Item_empty_string("Read_Master_Log_Pos", 12));
  field_list.push_back(new Item_empty_string("Relay_Log_File",
						     FN_REFLEN));
  field_list.push_back(new Item_empty_string("Relay_Log_Pos", 12));
  field_list.push_back(new Item_empty_string("Relay_Master_Log_File",
						     FN_REFLEN));
  field_list.push_back(new Item_empty_string("Slave_IO_Running", 3));
  field_list.push_back(new Item_empty_string("Slave_SQL_Running", 3));
  field_list.push_back(new Item_empty_string("Replicate_do_db", 20));
  field_list.push_back(new Item_empty_string("Replicate_ignore_db", 20));
  field_list.push_back(new Item_empty_string("Last_errno", 4));
  field_list.push_back(new Item_empty_string("Last_error", 20));
  field_list.push_back(new Item_empty_string("Skip_counter", 12));
  field_list.push_back(new Item_empty_string("Exec_master_log_pos", 12));
  field_list.push_back(new Item_empty_string("Relay_log_space", 12));
  if (send_fields(thd, field_list, 1))
    DBUG_RETURN(-1);

  String* packet = &thd->packet;
  packet->length(0);
  
  pthread_mutex_lock(&mi->data_lock);
  pthread_mutex_lock(&mi->rli.data_lock);
  net_store_data(packet, mi->host);
  net_store_data(packet, mi->user);
  net_store_data(packet, (uint32) mi->port);
  net_store_data(packet, (uint32) mi->connect_retry);
  net_store_data(packet, mi->master_log_name);
  net_store_data(packet, (longlong) mi->master_log_pos);
  net_store_data(packet, mi->rli.relay_log_name +
		 dirname_length(mi->rli.relay_log_name));
  net_store_data(packet, (longlong) mi->rli.relay_log_pos);
  net_store_data(packet, mi->rli.master_log_name);
  net_store_data(packet, mi->slave_running ? "Yes":"No");
  net_store_data(packet, mi->rli.slave_running ? "Yes":"No");
  net_store_data(packet, &replicate_do_db);
  net_store_data(packet, &replicate_ignore_db);
  net_store_data(packet, (uint32)mi->rli.last_slave_errno);
  net_store_data(packet, mi->rli.last_slave_error);
  net_store_data(packet, mi->rli.slave_skip_counter);
  net_store_data(packet, (longlong) mi->rli.master_log_pos);
  net_store_data(packet, (longlong) mi->rli.log_space_total);
  pthread_mutex_unlock(&mi->rli.data_lock);
  pthread_mutex_unlock(&mi->data_lock);
  
  if (my_net_write(&thd->net, (char*)thd->packet.ptr(), packet->length()))
    DBUG_RETURN(-1);

  send_eof(&thd->net);
  DBUG_RETURN(0);
}


bool flush_master_info(MASTER_INFO* mi)
{
  IO_CACHE* file = &mi->file;
  char lbuf[22];
  DBUG_ENTER("flush_master_info");
  DBUG_PRINT("enter",("master_pos: %ld", (long) mi->master_log_pos));

  my_b_seek(file, 0L);
  my_b_printf(file, "%s\n%s\n%s\n%s\n%s\n%d\n%d\n%d\n",
	      mi->master_log_name, llstr(mi->master_log_pos, lbuf),
	      mi->host, mi->user,
	      mi->password, mi->port, mi->connect_retry
	      );
  flush_io_cache(file);
  DBUG_RETURN(0);
}


int st_relay_log_info::wait_for_pos(THD* thd, String* log_name,
				    ulonglong log_pos)
{
  if (!inited)
    return -1;
  bool pos_reached = 0;
  int event_count = 0;
  pthread_mutex_lock(&data_lock);
  abort_pos_wait=0; // abort only if master info  changes during wait

  while (!thd->killed || !abort_pos_wait)
  {
    int cmp_result;
    if (abort_pos_wait)
    {
      abort_pos_wait=0;
      pthread_mutex_unlock(&data_lock);
      return -1;
    }
    DBUG_ASSERT(*master_log_name || master_log_pos == 0);
    if (*master_log_name)
    {
      /*
	We should use dirname_length() here when we have a version of
	this that doesn't modify the argument */
      char *basename = strrchr(master_log_name, FN_LIBCHAR);
      if (basename)
	++basename;
      else
	basename = master_log_name;
      cmp_result =  strncmp(basename, log_name->ptr(),
			    log_name->length());
    }
    else
      cmp_result = 0;
      
    pos_reached = ((!cmp_result && master_log_pos >= log_pos) ||
		   cmp_result > 0);
    if (pos_reached || thd->killed)
      break;
    
    const char* msg = thd->enter_cond(&data_cond, &data_lock,
				      "Waiting for master update");
    pthread_cond_wait(&data_cond, &data_lock);
    thd->exit_cond(msg);
    event_count++;
  }
  pthread_mutex_unlock(&data_lock);
  return thd->killed ? -1 : event_count;
}


static int init_slave_thread(THD* thd, SLAVE_THD_TYPE thd_type)
{
  DBUG_ENTER("init_slave_thread");
  thd->system_thread = thd->bootstrap = 1;
  thd->client_capabilities = 0;
  my_net_init(&thd->net, 0);
  thd->net.read_timeout = slave_net_timeout;
  thd->master_access= ~0;
  thd->priv_user = 0;
  thd->slave_thread = 1;
  thd->options = (((opt_log_slave_updates) ? OPTION_BIN_LOG:0) | OPTION_AUTO_IS_NULL) ;
  thd->system_thread = 1;
  thd->client_capabilities = CLIENT_LOCAL_FILES;
  thd->real_id=pthread_self();
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id = thread_id++;
  pthread_mutex_unlock(&LOCK_thread_count);

  if (init_thr_lock() || thd->store_globals())
  {
    end_thread(thd,0);
    DBUG_RETURN(-1);
  }

  thd->mysys_var=my_thread_var;
  thd->dbug_thread_id=my_thread_id();
#if !defined(__WIN__) && !defined(OS2)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  if ((ulong) thd->variables.max_join_size == (ulong) HA_POS_ERROR)
    thd->options |= OPTION_BIG_SELECTS;

  if (thd_type == SLAVE_THD_SQL)
    thd->proc_info= "Waiting for the next event in slave queue";
  else
    thd->proc_info= "Waiting for master update";
  thd->version=refresh_version;
  thd->set_time();
  DBUG_RETURN(0);
}


static int safe_sleep(THD* thd, int sec, CHECK_KILLED_FUNC thread_killed,
		      void* thread_killed_arg)
{
  thr_alarm_t alarmed;
  thr_alarm_init(&alarmed);
  time_t start_time= time((time_t*) 0);
  time_t end_time= start_time+sec;
  ALARM  alarm_buff;

  while (start_time < end_time)
  {
    int nap_time = (int) (end_time - start_time);
    /*
      The only reason we are asking for alarm is so that
      we will be woken up in case of murder, so if we do not get killed,
      set the alarm so it goes off after we wake up naturally
    */
    thr_alarm(&alarmed, 2 * nap_time,&alarm_buff);
    sleep(nap_time);
    /*
      If we wake up before the alarm goes off, hit the button
      so it will not wake up the wife and kids :-)
    */
    if (thr_alarm_in_use(&alarmed))
      thr_end_alarm(&alarmed);
    
    if ((*thread_killed)(thd,thread_killed_arg))
      return 1;
    start_time=time((time_t*) 0);
  }
  return 0;
}

static int request_dump(MYSQL* mysql, MASTER_INFO* mi,
			bool *suppress_warnings)
{
  char buf[FN_REFLEN + 10];
  int len;
  int binlog_flags = 0; // for now
  char* logname = mi->master_log_name;
  // TODO if big log files: Change next to int8store()
  int4store(buf, (longlong) mi->master_log_pos);
  int2store(buf + 4, binlog_flags);
  int4store(buf + 6, server_id);
  len = (uint) strlen(logname);
  memcpy(buf + 10, logname,len);
  if (mc_simple_command(mysql, COM_BINLOG_DUMP, buf, len + 10, 1))
  {
    /*
      Something went wrong, so we will just reconnect and retry later
      in the future, we should do a better error analysis, but for
      now we just fill up the error log :-)
    */
    if (mc_mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
      *suppress_warnings= 1;			// Suppress reconnect warning
    else
      sql_print_error("Error on COM_BINLOG_DUMP: %s, will retry in %d secs",
		    mc_mysql_error(mysql), master_connect_retry);
    return 1;
  }

  return 0;
}


static int request_table_dump(MYSQL* mysql, const char* db, const char* table)
{
  char buf[1024];
  char * p = buf;
  uint table_len = (uint) strlen(table);
  uint db_len = (uint) strlen(db);
  if (table_len + db_len > sizeof(buf) - 2)
  {
    sql_print_error("request_table_dump: Buffer overrun");
    return 1;
  } 
  
  *p++ = db_len;
  memcpy(p, db, db_len);
  p += db_len;
  *p++ = table_len;
  memcpy(p, table, table_len);
  
  if (mc_simple_command(mysql, COM_TABLE_DUMP, buf, p - buf + table_len, 1))
  {
    sql_print_error("request_table_dump: Error sending the table dump \
command");
    return 1;
  }

  return 0;
}


/*
  read one event from the master
  
  SYNOPSIS
    read_event()
    mysql		MySQL connection
    mi			Master connection information
    suppress_warnings	TRUE when a normal net read timeout has caused us to
			try a reconnect.  We do not want to print anything to
			the error log in this case because this a anormal
			event in an idle server.

    RETURN VALUES
    'packet_error'	Error
    number		Length of packet

*/

static ulong read_event(MYSQL* mysql, MASTER_INFO *mi, bool* suppress_warnings)
{
  ulong len;

  *suppress_warnings= 0;
  /*
    my_real_read() will time us out
    We check if we were told to die, and if not, try reading again

    TODO:  Move 'events_till_disconnect' to the MASTER_INFO structure
  */
#ifndef DBUG_OFF
  if (disconnect_slave_event_count && !(events_till_disconnect--))
    return packet_error;      
#endif
  
  len = mc_net_safe_read(mysql);
  if (len == packet_error || (long) len < 1)
  {
    if (mc_mysql_errno(mysql) == ER_NET_READ_INTERRUPTED)
    {
      /*
	We are trying a normal reconnect after a read timeout;
	we suppress prints to .err file as long as the reconnect
	happens without problems
      */
      *suppress_warnings= TRUE;
    }
    else
      sql_print_error("Error reading packet from server: %s (\
server_errno=%d)",
		      mc_mysql_error(mysql), mc_mysql_errno(mysql));
    return packet_error;
  }

  if (len == 1)
  {
     sql_print_error("Slave: received 0 length packet from server, apparent\
 master shutdown: %s",
		     mc_mysql_error(mysql));
     return packet_error;
  }
  
  DBUG_PRINT("info",( "len=%u, net->read_pos[4] = %d\n",
		      len, mysql->net.read_pos[4]));
  return len - 1;   
}


int check_expected_error(THD* thd, RELAY_LOG_INFO* rli, int expected_error)
{
  switch (expected_error) {
  case ER_NET_READ_ERROR:
  case ER_NET_ERROR_ON_WRITE:  
  case ER_SERVER_SHUTDOWN:  
  case ER_NEW_ABORTING_CONNECTION:
    my_snprintf(rli->last_slave_error, sizeof(rli->last_slave_error), 
		"Slave: query '%s' partially completed on the master \
and was aborted. There is a chance that your master is inconsistent at this \
point. If you are sure that your master is ok, run this query manually on the\
 slave and then restart the slave with SET SQL_SLAVE_SKIP_COUNTER=1;\
 SLAVE START;", thd->query);
    rli->last_slave_errno = expected_error;
    sql_print_error("%s",rli->last_slave_error);
    return 1;
  default:
    return 0;
  }
}


static int exec_relay_log_event(THD* thd, RELAY_LOG_INFO* rli)
{
  DBUG_ASSERT(rli->sql_thd==thd);
  Log_event * ev = next_event(rli);
  DBUG_ASSERT(rli->sql_thd==thd);
  if (sql_slave_killed(thd,rli))
    return 1;
  if (ev)
  {
    int type_code = ev->get_type_code();
    int exec_res;
    pthread_mutex_lock(&rli->data_lock);

    /*
      Skip queries originating from this server or number of
      queries specified by the user in slave_skip_counter
      We can't however skip event's that has something to do with the
      log files themselves.
    */

    if (ev->server_id == (uint32) ::server_id ||
	(rli->slave_skip_counter && type_code != ROTATE_EVENT))
    {
      /* TODO: I/O thread should not even log events with the same server id */
      rli->inc_pos(ev->get_event_len(),
		   type_code != STOP_EVENT ? ev->log_pos : LL(0),
		   1/* skip lock*/);
      flush_relay_log_info(rli);

      /*
	Protect against common user error of setting the counter to 1
	instead of 2 while recovering from an failed auto-increment insert
      */
      if (rli->slave_skip_counter && 
	  !((type_code == INTVAR_EVENT || type_code == STOP_EVENT) &&
	    rli->slave_skip_counter == 1))
        --rli->slave_skip_counter;
      pthread_mutex_unlock(&rli->data_lock);
      delete ev;     
      return 0;					// avoid infinite update loops
    }
    pthread_mutex_unlock(&rli->data_lock);
  
    thd->server_id = ev->server_id; // use the original server id for logging
    thd->set_time();				// time the query
    if (!ev->when)
      ev->when = time(NULL);
    ev->thd = thd;
    thd->log_pos = ev->log_pos;
    exec_res = ev->exec_event(rli);
    DBUG_ASSERT(rli->sql_thd==thd);
    delete ev;
    return exec_res;
  }
  else
  {
    sql_print_error("\
Could not parse log event entry, check the master for binlog corruption\n\
This may also be a network problem, or just a bug in the master or slave code.\
");
    return 1;
  }
}


/* slave I/O thread */
pthread_handler_decl(handle_slave_io,arg)
{
#ifndef DBUG_OFF
slave_begin:  
#endif  
  THD *thd; // needs to be first for thread_stack
  MYSQL *mysql = NULL ;
  MASTER_INFO* mi = (MASTER_INFO*)arg; 
  char llbuff[22];
  uint retry_count= 0;
  ulonglong last_failed_pos = 0; // TODO: see if last_failed_pos is needed
  DBUG_ASSERT(mi->inited);
  
  pthread_mutex_lock(&mi->run_lock);
#ifndef DBUG_OFF  
  mi->events_till_abort = abort_slave_event_count;
#endif  
  
  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  thd= new THD; // note that contructor of THD uses DBUG_ !
  DBUG_ENTER("handle_slave_io");
  THD_CHECK_SENTRY(thd);

  pthread_detach_this_thread();
  if (init_slave_thread(thd, SLAVE_THD_IO))
  {
    pthread_cond_broadcast(&mi->start_cond);
    pthread_mutex_unlock(&mi->run_lock);
    sql_print_error("Failed during slave I/O thread initialization");
    goto err;
  }
  mi->io_thd = thd;
  thd->thread_stack = (char*)&thd; // remember where our stack is
  pthread_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  mi->slave_running = 1;
  mi->abort_slave = 0;
  pthread_mutex_unlock(&mi->run_lock);
  pthread_cond_broadcast(&mi->start_cond);
  
  DBUG_PRINT("master_info",("log_file_name: '%s'  position: %s",
			    mi->master_log_name,
			    llstr(mi->master_log_pos,llbuff)));
  
  if (!(mi->mysql = mysql = mc_mysql_init(NULL)))
  {
    sql_print_error("Slave I/O thread: error in mc_mysql_init()");
    goto err;
  }
  

  thd->proc_info = "connecting to master";
  // we can get killed during safe_connect
  if (!safe_connect(thd, mysql, mi))
    sql_print_error("Slave I/O thread: connected to master '%s@%s:%d',\
  replication started in log '%s' at position %s", mi->user,
		    mi->host, mi->port,
		    IO_RPL_LOG_NAME,
		    llstr(mi->master_log_pos,llbuff));
  else
  {
    sql_print_error("Slave I/O thread killed while connecting to master");
    goto err;
  }

connected:

  thd->slave_net = &mysql->net;
  thd->proc_info = "Checking master version";
  if (check_master_version(mysql, mi))
    goto err;
  if (!mi->old_format)
  {
    /*
      Register ourselves with the master.
      If fails, this is not fatal - we just print the error message and go
      on with life.
    */
    thd->proc_info = "Registering slave on master";
    if (register_slave_on_master(mysql) ||  update_slave_list(mysql))
      goto err;
  }
  
  while (!io_slave_killed(thd,mi))
  {
    bool suppress_warnings= 0;    
    thd->proc_info = "Requesting binlog dump";
    if (request_dump(mysql, mi, &suppress_warnings))
    {
      sql_print_error("Failed on request_dump()");
      if (io_slave_killed(thd,mi))
      {
	sql_print_error("Slave I/O thread killed while requesting master \
dump");
	goto err;
      }
	  
      thd->proc_info = "Waiiting to reconnect after a failed dump request";
      mc_end_server(mysql);
      /*
	First time retry immediately, assuming that we can recover
	right away - if first time fails, sleep between re-tries
	hopefuly the admin can fix the problem sometime
      */
      if (retry_count++)
      {
	if (retry_count > master_retry_count)
	  goto err;				// Don't retry forever
	safe_sleep(thd,mi->connect_retry,(CHECK_KILLED_FUNC)io_slave_killed,
		   (void*)mi);
      }
      if (io_slave_killed(thd,mi))
      {
	sql_print_error("Slave I/O thread killed while retrying master \
dump");
	goto err;
      }

      thd->proc_info = "Reconnecting after a failed dump request";
      if (!suppress_warnings)
	sql_print_error("Slave I/O thread: failed dump request, \
reconnecting to try again, log '%s' at postion %s", IO_RPL_LOG_NAME,
			llstr(mi->master_log_pos,llbuff));
      if (safe_reconnect(thd, mysql, mi, suppress_warnings) ||
	  io_slave_killed(thd,mi))
      {
	sql_print_error("Slave I/O thread killed during or \
after reconnect");
	goto err;
      }

      goto connected;
    }

    while (!io_slave_killed(thd,mi))
    {
      bool suppress_warnings= 0;    
      thd->proc_info = "Reading master update";
      ulong event_len = read_event(mysql, mi, &suppress_warnings);
      if (io_slave_killed(thd,mi))
      {
	if (global_system_variables.log_warnings)
	  sql_print_error("Slave I/O thread killed while reading event");
	goto err;
      }
	  	  
      if (event_len == packet_error)
      {
	if (mc_mysql_errno(mysql) == ER_NET_PACKET_TOO_LARGE)
	{
	  sql_print_error("\
Log entry on master is longer than max_allowed_packet (%ld) on \
slave. If the entry is correct, restart the server with a higher value of \
max_allowed_packet",
			  thd->variables.max_allowed_packet);
	  goto err;
	}

	thd->proc_info = "Waiting to reconnect after a failed read";
	mc_end_server(mysql);
	if (retry_count++)
	{
	  if (retry_count > master_retry_count)
	    goto err;				// Don't retry forever
	  safe_sleep(thd,mi->connect_retry,(CHECK_KILLED_FUNC)io_slave_killed,
		     (void*) mi);
	}	    
	if (io_slave_killed(thd,mi))
	{
	  if (global_system_variables.log_warnings)
	    sql_print_error("Slave I/O thread killed while waiting to \
reconnect after a failed read");
	  goto err;
	}
	thd->proc_info = "Reconnecting after a failed read";
	if (!suppress_warnings)
	  sql_print_error("Slave I/O thread: Failed reading log event, \
reconnecting to retry, log '%s' position %s", IO_RPL_LOG_NAME,
			  llstr(mi->master_log_pos, llbuff));
	if (safe_reconnect(thd, mysql, mi, suppress_warnings) ||
	    io_slave_killed(thd,mi))
	{
	  if (global_system_variables.log_warnings)
	    sql_print_error("Slave I/O thread killed during or after a \
reconnect done to recover from failed read");
	  goto err;
	}
	goto connected;
      } // if (event_len == packet_error)
	  
      retry_count=0;			// ok event, reset retry counter
      thd->proc_info = "Queueing event from master";
      if (queue_event(mi,(const char*)mysql->net.read_pos + 1,
		      event_len))
      {
	sql_print_error("Slave I/O thread could not queue event from master");
	goto err;
      }
      flush_master_info(mi);
      if (mi->rli.log_space_limit && mi->rli.log_space_limit <
	  mi->rli.log_space_total)
	if (wait_for_relay_log_space(&mi->rli))
	{
	  sql_print_error("Slave I/O thread aborted while waiting for relay \
log space");
	  goto err;
	}
      // TODO: check debugging abort code
#ifndef DBUG_OFF
      if (abort_slave_event_count && !--events_till_abort)
      {
	sql_print_error("Slave I/O thread: debugging abort");
	goto err;
      }
#endif
    } 
  }

  // error = 0;
err:
  // print the current replication position
  sql_print_error("Slave I/O thread exiting, read up to log '%s', position %s",
		  IO_RPL_LOG_NAME, llstr(mi->master_log_pos,llbuff));
  thd->query = thd->db = 0; // extra safety
  if (mysql)
  {
    mc_mysql_close(mysql);
    mi->mysql=0;
  }
  thd->proc_info = "Waiting for slave mutex on exit";
  pthread_mutex_lock(&mi->run_lock);
  mi->slave_running = 0;
  mi->io_thd = 0;
  // TODO: make rpl_status part of MASTER_INFO
  change_rpl_status(RPL_ACTIVE_SLAVE,RPL_IDLE_SLAVE);
  mi->abort_slave = 0; // TODO: check if this is needed
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because net.vio is 0
  pthread_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);
  my_thread_end();				// clean-up before broadcast
  pthread_cond_broadcast(&mi->stop_cond);	// tell the world we are done
  pthread_mutex_unlock(&mi->run_lock);
#ifndef DBUG_OFF
  if (abort_slave_event_count && !events_till_abort)
    goto slave_begin;
#endif  
  pthread_exit(0);
  DBUG_RETURN(0);				// Can't return anything here
}


/* slave SQL logic thread */

pthread_handler_decl(handle_slave_sql,arg)
{
#ifndef DBUG_OFF
slave_begin:  
#endif  
  THD *thd;			/* needs to be first for thread_stack */
  MYSQL *mysql = NULL ;
  bool retried_once = 0;
  ulonglong last_failed_pos = 0; // TODO: see if this can be removed
  char llbuff[22],llbuff1[22];
  RELAY_LOG_INFO* rli = &((MASTER_INFO*)arg)->rli; 
  const char* errmsg=0;
  DBUG_ASSERT(rli->inited);
  pthread_mutex_lock(&rli->run_lock);
  DBUG_ASSERT(!rli->slave_running);
#ifndef DBUG_OFF  
  rli->events_till_abort = abort_slave_event_count;
#endif  
  
  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  thd = new THD; // note that contructor of THD uses DBUG_ !
  DBUG_ENTER("handle_slave_sql");

  THD_CHECK_SENTRY(thd);
  pthread_detach_this_thread();
  if (init_slave_thread(thd, SLAVE_THD_SQL))
  {
    /*
      TODO: this is currently broken - slave start and change master
      will be stuck if we fail here
    */
    pthread_cond_broadcast(&rli->start_cond);
    pthread_mutex_unlock(&rli->run_lock);
    sql_print_error("Failed during slave thread initialization");
    goto err;
  }
  rli->sql_thd= thd;
  thd->temporary_tables = rli->save_temporary_tables; // restore temp tables
  thd->thread_stack = (char*)&thd; // remember where our stack is
  pthread_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  rli->slave_running = 1;
  rli->abort_slave = 0;
  pthread_mutex_unlock(&rli->run_lock);
  pthread_cond_broadcast(&rli->start_cond);
  // This should always be set to 0 when the slave thread is started
  rli->pending = 0;
  if (init_relay_log_pos(rli,
			 rli->relay_log_name,
			 rli->relay_log_pos,
			 1 /*need data lock*/, &errmsg))
  {
    sql_print_error("Error initializing relay log position: %s",
		    errmsg);
    goto err;
  }
  THD_CHECK_SENTRY(thd);
  DBUG_ASSERT(rli->relay_log_pos >= BIN_LOG_HEADER_SIZE);
  DBUG_ASSERT(my_b_tell(rli->cur_log) == rli->relay_log_pos);
  DBUG_ASSERT(rli->sql_thd == thd);

  DBUG_PRINT("master_info",("log_file_name: %s  position: %s",
			    rli->master_log_name,
			    llstr(rli->master_log_pos,llbuff)));
  if (global_system_variables.log_warnings)
    sql_print_error("Slave SQL thread initialized, starting replication in \
log '%s' at position %s, relay log '%s' position: %s", RPL_LOG_NAME,
		    llstr(rli->master_log_pos,llbuff),rli->relay_log_name,
		    llstr(rli->relay_log_pos,llbuff1));

  /* Read queries from the IO/THREAD until this thread is killed */

  while (!sql_slave_killed(thd,rli))
  {
    thd->proc_info = "Processing master log event"; 
    DBUG_ASSERT(rli->sql_thd == thd);
    THD_CHECK_SENTRY(thd);
    if (exec_relay_log_event(thd,rli))
    {
      // do not scare the user if SQL thread was simply killed or stopped
      if (!sql_slave_killed(thd,rli))
        sql_print_error("\
Error running query, slave SQL thread aborted. Fix the problem, and restart \
the slave SQL thread with \"SLAVE START\". We stopped at log \
'%s' position %s",
		      RPL_LOG_NAME, llstr(rli->master_log_pos, llbuff));
      goto err;
    }
  }

  /* Thread stopped. Print the current replication position to the log */
  sql_print_error("Slave SQL thread exiting, replication stopped in log \
 '%s' at position %s",
		  RPL_LOG_NAME, llstr(rli->master_log_pos,llbuff));

 err:
  thd->query = thd->db = 0; // extra safety
  thd->proc_info = "Waiting for slave mutex on exit";
  pthread_mutex_lock(&rli->run_lock);
  DBUG_ASSERT(rli->slave_running == 1); // tracking buffer overrun
  rli->slave_running = 0;
  rli->save_temporary_tables = thd->temporary_tables;

  /*
    TODO: see if we can do this conditionally in next_event() instead
    to avoid unneeded position re-init
  */
  rli->log_pos_current=0; 
  thd->temporary_tables = 0; // remove tempation from destructor to close them
  DBUG_ASSERT(thd->net.buff != 0);
  net_end(&thd->net); // destructor will not free it, because we are weird
  DBUG_ASSERT(rli->sql_thd == thd);
  THD_CHECK_SENTRY(thd);
  rli->sql_thd= 0;
  pthread_mutex_lock(&LOCK_thread_count);
  THD_CHECK_SENTRY(thd);
  delete thd;
  pthread_mutex_unlock(&LOCK_thread_count);
  my_thread_end(); // clean-up before broadcasting termination
  pthread_cond_broadcast(&rli->stop_cond);
  // tell the world we are done
  pthread_mutex_unlock(&rli->run_lock);
#ifndef DBUG_OFF // TODO: reconsider the code below
  if (abort_slave_event_count && !rli->events_till_abort)
    goto slave_begin;
#endif  
  pthread_exit(0);
  DBUG_RETURN(0);				// Can't return anything here
}

static int process_io_create_file(MASTER_INFO* mi, Create_file_log_event* cev)
{
  int error = 1;
  ulong num_bytes;
  bool cev_not_written;
  THD* thd;
  NET* net = &mi->mysql->net;
  DBUG_ENTER("process_io_create_file");

  if (unlikely(!cev->is_valid()))
    DBUG_RETURN(1);
  /*
    TODO: fix to honor table rules, not only db rules
  */
  if (!db_ok(cev->db, replicate_do_db, replicate_ignore_db))
  {
    skip_load_data_infile(net);
    DBUG_RETURN(0);
  }
  DBUG_ASSERT(cev->inited_from_old);
  thd = mi->io_thd;
  thd->file_id = cev->file_id = mi->file_id++;
  thd->server_id = cev->server_id;
  cev_not_written = 1;
  
  if (unlikely(net_request_file(net,cev->fname)))
  {
    sql_print_error("Slave I/O: failed requesting download of '%s'",
		    cev->fname);
    goto err;
  }

  /* this dummy block is so we could instantiate Append_block_log_event
     once and then modify it slightly instead of doing it multiple times
     in the loop
  */
  {
    Append_block_log_event aev(thd,0,0);
  
    for (;;)
    {
      if (unlikely((num_bytes=my_net_read(net)) == packet_error))
      {
	sql_print_error("Network read error downloading '%s' from master",
			cev->fname);
	goto err;
      }
      if (unlikely(!num_bytes)) /* eof */
      {
	send_ok(net); /* 3.23 master wants it */
	Execute_load_log_event xev(thd);
	xev.log_pos = mi->master_log_pos;
	if (unlikely(mi->rli.relay_log.append(&xev)))
	{
	  sql_print_error("Slave I/O: error writing Exec_load event to \
relay log");
	  goto err;
	}
	mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
	break;
      }
      if (unlikely(cev_not_written))
      {
	cev->block = (char*)net->read_pos;
	cev->block_len = num_bytes;
	cev->log_pos = mi->master_log_pos;
	if (unlikely(mi->rli.relay_log.append(cev)))
	{
	  sql_print_error("Slave I/O: error writing Create_file event to \
relay log");
	  goto err;
	}
	cev_not_written=0;
	mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
      }
      else
      {
	aev.block = (char*)net->read_pos;
	aev.block_len = num_bytes;
	aev.log_pos = mi->master_log_pos;
	if (unlikely(mi->rli.relay_log.append(&aev)))
	{
	  sql_print_error("Slave I/O: error writing Append_block event to \
relay log");
	  goto err;
	}
	mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total) ;
      }
    }
  }
  error=0;
err:
  DBUG_RETURN(error);
}

/*
  We assume we already locked mi->data_lock
*/

static int process_io_rotate(MASTER_INFO* mi, Rotate_log_event* rev)
{
  DBUG_ENTER("process_io_rotate");

  if (unlikely(!rev->is_valid()))
    DBUG_RETURN(1);
  DBUG_ASSERT(rev->ident_len < sizeof(mi->master_log_name));
  memcpy(mi->master_log_name,rev->new_log_ident,
	 rev->ident_len);
  mi->master_log_name[rev->ident_len] = 0;
  mi->master_log_pos = rev->pos;
  DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
#ifndef DBUG_OFF
  /*
    If we do not do this, we will be getting the first
    rotate event forever, so we need to not disconnect after one.
  */
  if (disconnect_slave_event_count)
    events_till_disconnect++;
#endif
  DBUG_RETURN(0);
}

/*
  TODO: verify the issue with stop events, see if we need them at all
  in the relay log
  TODO: test this code before release - it has to be tested on a separte
  setup with 3.23 master 
*/

static int queue_old_event(MASTER_INFO *mi, const char *buf,
			   ulong event_len)
{
  const char *errmsg = 0;
  bool inc_pos = 1;
  bool processed_stop_event = 0;
  char* tmp_buf = 0;
  DBUG_ENTER("queue_old_event");

  /* if we get Load event, we need to pass a non-reusable buffer
     to read_log_event, so we do a trick
  */
  if (buf[EVENT_TYPE_OFFSET] == LOAD_EVENT)
  {
    if (unlikely(!(tmp_buf=(char*)my_malloc(event_len+1,MYF(MY_WME)))))
    {
      sql_print_error("Slave I/O: out of memory for Load event");
      DBUG_RETURN(1);
    }
    memcpy(tmp_buf,buf,event_len);
    tmp_buf[event_len]=0; // Create_file constructor wants null-term buffer
    buf = (const char*)tmp_buf;
  }
  Log_event *ev = Log_event::read_log_event(buf,event_len, &errmsg,
					    1 /*old format*/ );
  if (unlikely(!ev))
  {
    sql_print_error("Read invalid event from master: '%s',\
 master could be corrupt but a more likely cause of this is a bug",
		    errmsg);
    my_free((char*) tmp_buf, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_RETURN(1);
  }
  pthread_mutex_lock(&mi->data_lock);
  ev->log_pos = mi->master_log_pos;
  switch (ev->get_type_code()) {
  case ROTATE_EVENT:
    if (unlikely(process_io_rotate(mi,(Rotate_log_event*)ev)))
    {
      delete ev;
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_ASSERT(!tmp_buf);      
      DBUG_RETURN(1);
    }
    mi->ignore_stop_event=1;
    inc_pos = 0;
    break;
  case STOP_EVENT:
    processed_stop_event=1;
    break;
  case CREATE_FILE_EVENT:
  {
    int error = process_io_create_file(mi,(Create_file_log_event*)ev);
    delete ev;
    mi->master_log_pos += event_len;
    DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
    pthread_mutex_unlock(&mi->data_lock);
    DBUG_ASSERT(tmp_buf);
    my_free((char*)tmp_buf, MYF(0));
    DBUG_RETURN(error);
  }
  default:
    mi->ignore_stop_event=0;
    break;
  }
  if (likely(!processed_stop_event || !mi->ignore_stop_event))
  {
    if (unlikely(mi->rli.relay_log.append(ev)))
    {
      delete ev;
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_ASSERT(!tmp_buf);
      DBUG_RETURN(1);
    }
    mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
  }
  delete ev;
  if (likely(inc_pos))
    mi->master_log_pos += event_len;
  DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
  if (unlikely(processed_stop_event))
    mi->ignore_stop_event=1;
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_ASSERT(!tmp_buf);
  DBUG_RETURN(0);
}

/*
  TODO: verify the issue with stop events, see if we need them at all
  in the relay log
*/

int queue_event(MASTER_INFO* mi,const char* buf, ulong event_len)
{
  int error=0;
  bool inc_pos = 1;
  bool processed_stop_event = 0;
  DBUG_ENTER("queue_event");

  if (mi->old_format)
    DBUG_RETURN(queue_old_event(mi,buf,event_len));

  pthread_mutex_lock(&mi->data_lock);
  
  /*
    TODO: figure out if other events in addition to Rotate
    require special processing
  */
  switch (buf[EVENT_TYPE_OFFSET]) {
  case STOP_EVENT:
    processed_stop_event=1;
    break;
  case ROTATE_EVENT:
  {
    Rotate_log_event rev(buf,event_len,0);
    if (unlikely(process_io_rotate(mi,&rev)))
      DBUG_RETURN(1);
    inc_pos=0;
    mi->ignore_stop_event=1;
    break;
  }
  default:
    mi->ignore_stop_event=0;
    break;
  }
  
  if (likely((!processed_stop_event || !mi->ignore_stop_event) &&
	     !(error = mi->rli.relay_log.appendv(buf,event_len,0))))
  {
    if (likely(inc_pos))
      mi->master_log_pos += event_len;
    DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
    mi->rli.relay_log.harvest_bytes_written(&mi->rli.log_space_total);
  }
  if (unlikely(processed_stop_event))
    mi->ignore_stop_event=1;
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(error);
}


void end_relay_log_info(RELAY_LOG_INFO* rli)
{
  DBUG_ENTER("end_relay_log_info");

  if (!rli->inited)
    DBUG_VOID_RETURN;
  if (rli->info_fd >= 0)
  {
    end_io_cache(&rli->info_file);
    (void) my_close(rli->info_fd, MYF(MY_WME));
    rli->info_fd = -1;
  }
  if (rli->cur_log_fd >= 0)
  {
    end_io_cache(&rli->cache_buf);
    (void)my_close(rli->cur_log_fd, MYF(MY_WME));
    rli->cur_log_fd = -1;
  }
  rli->inited = 0;
  rli->log_pos_current=0;
  rli->relay_log.close(1);
  DBUG_VOID_RETURN;
}

/* try to connect until successful or slave killed */
static int safe_connect(THD* thd, MYSQL* mysql, MASTER_INFO* mi)
{
  return connect_to_master(thd, mysql, mi, 0, 0);
}


/*
  Try to connect until successful or slave killed or we have retried
  master_retry_count times
*/

static int connect_to_master(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			     bool reconnect, bool suppress_warnings)
{
  int slave_was_killed;
  int last_errno= -2;				// impossible error
  ulong err_count=0;
  char llbuff[22];
  DBUG_ENTER("connect_to_master");

#ifndef DBUG_OFF
  events_till_disconnect = disconnect_slave_event_count;
#endif
  uint client_flag=0;
  if (opt_slave_compressed_protocol)
    client_flag=CLIENT_COMPRESS;		/* We will use compression */

  while (!(slave_was_killed = io_slave_killed(thd,mi)) &&
	 (reconnect ? mc_mysql_reconnect(mysql) != 0:
	  !mc_mysql_connect(mysql, mi->host, mi->user, mi->password, 0,
			    mi->port, 0, client_flag,
			    thd->variables.net_read_timeout)))
  {
    /* Don't repeat last error */
    if (mc_mysql_errno(mysql) != last_errno)
    {
      last_errno=mc_mysql_errno(mysql);
      suppress_warnings= 0;
      sql_print_error("Slave I/O thread: error connecting to master \
'%s@%s:%d': \
Error: '%s'  errno: %d  retry-time: %d",mi->user,mi->host,mi->port,
		      mc_mysql_error(mysql), last_errno,
		      mi->connect_retry);
    }
    /*
      By default we try forever. The reason is that failure will trigger
      master election, so if the user did not set master_retry_count we
      do not want to have election triggered on the first failure to
      connect
    */
    if (++err_count == master_retry_count)
    {
      slave_was_killed=1;
      if (reconnect)
        change_rpl_status(RPL_ACTIVE_SLAVE,RPL_LOST_SOLDIER);
      break;
    }
    safe_sleep(thd,mi->connect_retry,(CHECK_KILLED_FUNC)io_slave_killed,
	       (void*)mi);
  }

  if (!slave_was_killed)
  {
    if (reconnect)
    { 
      if (!suppress_warnings && global_system_variables.log_warnings)
	sql_print_error("Slave: connected to master '%s@%s:%d',\
replication resumed in log '%s' at position %s", mi->user,
			mi->host, mi->port,
			IO_RPL_LOG_NAME,
			llstr(mi->master_log_pos,llbuff));
    }
    else
    {
      change_rpl_status(RPL_IDLE_SLAVE,RPL_ACTIVE_SLAVE);
      mysql_log.write(thd, COM_CONNECT_OUT, "%s@%s:%d",
		      mi->user, mi->host, mi->port);
    }
#ifdef SIGNAL_WITH_VIO_CLOSE
    thd->set_active_vio(mysql->net.vio);
#endif      
  }
  DBUG_PRINT("exit",("slave_was_killed: %d", slave_was_killed));
  DBUG_RETURN(slave_was_killed);
}


/*
  Try to connect until successful or slave killed or we have retried
  master_retry_count times
*/

static int safe_reconnect(THD* thd, MYSQL* mysql, MASTER_INFO* mi,
			  bool suppress_warnings)
{
  return connect_to_master(thd, mysql, mi, 1, suppress_warnings);
}


/*
  Store the file and position where the execute-slave thread are in the
  relay log.

  SYNOPSIS
    flush_relay_log_info()
    rli			Relay log information

  NOTES
    - As this is only called by the slave thread, we don't need to
      have a lock on this.
    - If there is an active transaction, then we don't update the position
      in the relay log.  This is to ensure that we re-execute statements
      if we die in the middle of an transaction that was rolled back.
    - As a transaction never spans binary logs, we don't have to handle the
      case where we do a relay-log-rotation in the middle of the transaction.
      If this would not be the case, we would have to ensure that we
      don't delete the relay log file where the transaction started when
      we switch to a new relay log file.

  TODO
    - Change the log file information to a binary format to avoid calling
      longlong2str.

  RETURN VALUES
    0	ok
    1	write error
*/

bool flush_relay_log_info(RELAY_LOG_INFO* rli)
{
  bool error=0;
  IO_CACHE *file = &rli->info_file;
  char buff[FN_REFLEN*2+22*2+4], *pos;

  /* sql_thd is not set when calling from init_slave() */
  if ((rli->sql_thd && rli->sql_thd->options & OPTION_BEGIN))
    return 0;					// Wait for COMMIT

  my_b_seek(file, 0L);
  pos=strmov(buff, rli->relay_log_name);
  *pos++='\n';
  pos=longlong2str(rli->relay_log_pos, pos, 10);
  *pos++='\n';
  pos=strmov(pos, rli->master_log_name);
  *pos++='\n';
  pos=longlong2str(rli->master_log_pos, pos, 10);
  *pos='\n';
  if (my_b_write(file, buff, (ulong) (pos-buff)+1))
    error=1;
  if (flush_io_cache(file))
    error=1;
  if (flush_io_cache(rli->cur_log))		// QQ Why this call ?
    error=1;
  return error;
}


/*
  This function is called when we notice that the current "hot" log
  got rotated under our feet.
*/

static IO_CACHE *reopen_relay_log(RELAY_LOG_INFO *rli, const char **errmsg)
{
  DBUG_ASSERT(rli->cur_log != &rli->cache_buf);
  DBUG_ASSERT(rli->cur_log_fd == -1);
  DBUG_ENTER("reopen_relay_log");

  IO_CACHE *cur_log = rli->cur_log=&rli->cache_buf;
  if ((rli->cur_log_fd=open_binlog(cur_log,rli->relay_log_name,
				   errmsg)) <0)
    DBUG_RETURN(0);
  my_b_seek(cur_log,rli->relay_log_pos);
  DBUG_RETURN(cur_log);
}


Log_event* next_event(RELAY_LOG_INFO* rli)
{
  Log_event* ev;
  IO_CACHE* cur_log = rli->cur_log;
  pthread_mutex_t *log_lock = rli->relay_log.get_log_lock();
  const char* errmsg=0;
  THD* thd = rli->sql_thd;
  bool was_killed;
  DBUG_ENTER("next_event");
  DBUG_ASSERT(thd != 0);

  /*
    For most operations we need to protect rli members with data_lock,
    so we will hold it for the most of the loop below
    However, we will release it whenever it is worth the hassle, 
    and in the cases when we go into a pthread_cond_wait() with the
    non-data_lock mutex
  */
  pthread_mutex_lock(&rli->data_lock);
  
  while (!(was_killed=sql_slave_killed(thd,rli)))
  {
    /*
      We can have two kinds of log reading:
      hot_log:
        rli->cur_log points at the IO_CACHE of relay_log, which
        is actively being updated by the I/O thread. We need to be careful
        in this case and make sure that we are not looking at a stale log that
        has already been rotated. If it has been, we reopen the log.

      The other case is much simpler:
        We just have a read only log that nobody else will be updating.
    */
    bool hot_log;
    if ((hot_log = (cur_log != &rli->cache_buf)))
    {
      DBUG_ASSERT(rli->cur_log_fd == -1); // foreign descriptor
      pthread_mutex_lock(log_lock);

      /*
	Reading xxx_file_id is safe because the log will only
	be rotated when we hold relay_log.LOCK_log
      */
      if (rli->relay_log.get_open_count() != rli->cur_log_old_open_count)
      {
	// The master has switched to a new log file; Reopen the old log file
	cur_log=reopen_relay_log(rli, &errmsg);
	pthread_mutex_unlock(log_lock);
	if (!cur_log)				// No more log files
	  goto err;
	hot_log=0;				// Using old binary log
      }
    }
    DBUG_ASSERT(my_b_tell(cur_log) >= BIN_LOG_HEADER_SIZE);
    DBUG_ASSERT(my_b_tell(cur_log) == rli->relay_log_pos + rli->pending);
    /*
      Relay log is always in new format - if the master is 3.23, the
      I/O thread will convert the format for us
    */
    if ((ev=Log_event::read_log_event(cur_log,0,(bool)0 /* new format */)))
    {
      DBUG_ASSERT(thd==rli->sql_thd);
      if (hot_log)
	pthread_mutex_unlock(log_lock);
      pthread_mutex_unlock(&rli->data_lock);
      DBUG_RETURN(ev);
    }
    DBUG_ASSERT(thd==rli->sql_thd);
    if (opt_reckless_slave)			// For mysql-test
      cur_log->error = 0;
    if (cur_log->error < 0)
    {
      errmsg = "slave SQL thread aborted because of I/O error";
      if (hot_log)
	pthread_mutex_unlock(log_lock);
      goto err;
    }
    if (!cur_log->error) /* EOF */
    {
      /*
	On a hot log, EOF means that there are no more updates to
	process and we must block until I/O thread adds some and
	signals us to continue
      */
      if (hot_log)
      {
	DBUG_ASSERT(rli->relay_log.get_open_count() == rli->cur_log_old_open_count);
	/*
	  We can, and should release data_lock while we are waiting for
	  update. If we do not, show slave status will block
	*/
	pthread_mutex_unlock(&rli->data_lock);
	rli->relay_log.wait_for_update(rli->sql_thd);
	pthread_mutex_unlock(log_lock);
	
	// re-acquire data lock since we released it earlier
	pthread_mutex_lock(&rli->data_lock);
	continue;
      }
      /*
	If the log was not hot, we need to move to the next log in
	sequence. The next log could be hot or cold, we deal with both
	cases separately after doing some common initialization
      */
      end_io_cache(cur_log);
      DBUG_ASSERT(rli->cur_log_fd >= 0);
      my_close(rli->cur_log_fd, MYF(MY_WME));
      rli->cur_log_fd = -1;
	
      /*
	TODO: make skip_log_purge a start-up option. At this point this
	is not critical priority
      */
      if (!rli->skip_log_purge)
      {
	// purge_first_log will properly set up relay log coordinates in rli
	if (rli->relay_log.purge_first_log(rli))
	{
	  errmsg = "Error purging processed log";
	  goto err;
	}
      }
      else
      {
	/*
	  If hot_log is set, then we already have a lock on
	  LOCK_log.  If not, we have to get the lock.

	  According to Sasha, the only time this code will ever be executed
	  is if we are recovering from a bug.
	*/
	if (rli->relay_log.find_next_log(&rli->linfo, !hot_log))
	{
	  errmsg = "error switching to the next log";
	  goto err;
	}
	rli->relay_log_pos = BIN_LOG_HEADER_SIZE;
	rli->pending=0;
	strmake(rli->relay_log_name,rli->linfo.log_file_name,
		sizeof(rli->relay_log_name)-1);
	flush_relay_log_info(rli);
      }
	
      // next log is hot 
      if (rli->relay_log.is_active(rli->linfo.log_file_name))
      {
#ifdef EXTRA_DEBUG
	sql_print_error("next log '%s' is currently active",
			rli->linfo.log_file_name);
#endif	  
	rli->cur_log= cur_log= rli->relay_log.get_log_file();
	rli->cur_log_old_open_count= rli->relay_log.get_open_count();
	DBUG_ASSERT(rli->cur_log_fd == -1);
	  
	/*
	  Read pointer has to be at the start since we are the only
	  reader
	*/
	if (check_binlog_magic(cur_log,&errmsg))
	  goto err;
	continue;
      }
      /*
	if we get here, the log was not hot, so we will have to
	open it ourselves
      */
#ifdef EXTRA_DEBUG
      sql_print_error("next log '%s' is not active",
		      rli->linfo.log_file_name);
#endif	  
      // open_binlog() will check the magic header
      if ((rli->cur_log_fd=open_binlog(cur_log,rli->linfo.log_file_name,
				       &errmsg)) <0)
	goto err;
    }
    else
    {
      /*
	Read failed with a non-EOF error.
	TODO: come up with something better to handle this error
      */
      if (hot_log)
	pthread_mutex_unlock(log_lock);
      sql_print_error("Slave SQL thread: I/O error reading \
event(errno: %d  cur_log->error: %d)",
		      my_errno,cur_log->error);
      // set read position to the beginning of the event
      my_b_seek(cur_log,rli->relay_log_pos+rli->pending);
      /* otherwise, we have had a partial read */
      errmsg = "Aborting slave SQL thread because of partial event read";
      break;					// To end of function
    }
  }
  if (!errmsg && global_system_variables.log_warnings)
    errmsg = "slave SQL thread was killed";

err:
  pthread_mutex_unlock(&rli->data_lock);
  if (errmsg)
    sql_print_error("Error reading relay log event: %s", errmsg);
  DBUG_RETURN(0);
}


#ifdef __GNUC__
template class I_List_iterator<i_string>;
template class I_List_iterator<i_string_pair>;
#endif
