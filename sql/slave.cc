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
#include <thr_alarm.h>
#include <my_dir.h>

#define RPL_LOG_NAME (glob_mi.log_file_name[0] ? glob_mi.log_file_name :\
 "FIRST")

volatile bool slave_running = 0;
pthread_t slave_real_id;
MASTER_INFO glob_mi;
MY_BITMAP slave_error_mask;
bool use_slave_mask = 0;
HASH replicate_do_table, replicate_ignore_table;
DYNAMIC_ARRAY replicate_wild_do_table, replicate_wild_ignore_table;
bool do_table_inited = 0, ignore_table_inited = 0;
bool wild_do_table_inited = 0, wild_ignore_table_inited = 0;
bool table_rules_on = 0;
uint32 slave_skip_counter = 0; 
static TABLE* save_temporary_tables = 0;
THD* slave_thd = 0;
// when slave thread exits, we need to remember the temporary tables so we
// can re-use them on slave start

static int last_slave_errno = 0;
static char last_slave_error[1024] = "";
#ifndef DBUG_OFF
int disconnect_slave_event_count = 0, abort_slave_event_count = 0;
static int events_till_disconnect = -1, events_till_abort = -1;
static int stuck_count = 0;
#endif


inline void skip_load_data_infile(NET* net);
inline bool slave_killed(THD* thd);
static int init_slave_thread(THD* thd);
static int safe_connect(THD* thd, MYSQL* mysql, MASTER_INFO* mi);
static int safe_reconnect(THD* thd, MYSQL* mysql, MASTER_INFO* mi);
static int safe_sleep(THD* thd, int sec);
static int request_table_dump(MYSQL* mysql, char* db, char* table);
static int create_table_from_dump(THD* thd, NET* net, const char* db,
				  const char* table_name);
inline char* rewrite_db(char* db);
static int check_expected_error(THD* thd, int expected_error);

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

/* called from get_options() in mysqld.cc on start-up */
void init_slave_skip_errors(char* arg)
{
  char* p;
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

void init_table_rule_hash(HASH* h, bool* h_inited)
{
  hash_init(h, TABLE_RULE_HASH_SIZE,0,0,
	    (hash_get_key) get_table_key,
	    (void (*)(void*)) free_table_ent, 0);
  *h_inited = 1;
}

void init_table_rule_array(DYNAMIC_ARRAY* a, bool* a_inited)
{
  init_dynamic_array(a, sizeof(TABLE_RULE_ENT*), TABLE_RULE_ARR_SIZE,
		     TABLE_RULE_ARR_SIZE);
  *a_inited = 1;
}

static TABLE_RULE_ENT* find_wild(DYNAMIC_ARRAY *a, const char* key, int len)
{
  uint i;
  const char* key_end = key + len;
  
  for(i = 0; i < a->elements; i++)
    {
      TABLE_RULE_ENT* e ;
      get_dynamic(a, (gptr)&e, i);
      if(!wild_case_compare(key, key_end, (const char*)e->db,
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
    if (ignore_table_inited) // if there are any do's
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

  // if no explicit rule found
  // and there was a do list, do not replicate. If there was
  // no do list, go ahead
  return !do_table_inited && !wild_do_table_inited;
}


int add_table_rule(HASH* h, const char* table_spec)
{
  const char* dot = strchr(table_spec, '.');
  if(!dot) return 1;
  // len is always > 0 because we know the there exists a '.'
  uint len = (uint)strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if(!e) return 1;
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
  if(!dot) return 1;
  uint len = (uint)strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if(!e) return 1;
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
  for(i = 0; i < a->elements; i++)
    {
      char* p;
      get_dynamic(a, (gptr) &p, i);
      my_free(p, MYF(MY_WME));
    }
  delete_dynamic(a);
}

void end_slave()
{
  pthread_mutex_lock(&LOCK_slave);
  if (slave_running)
  {
    abort_slave = 1;
    thr_alarm_kill(slave_real_id);
#ifdef SIGNAL_WITH_VIO_CLOSE
    slave_thd->close_active_vio();
#endif    
    while (slave_running)
      pthread_cond_wait(&COND_slave_stopped, &LOCK_slave);
  }
  pthread_mutex_unlock(&LOCK_slave);
  
  end_master_info(&glob_mi);
  if(do_table_inited)
    hash_free(&replicate_do_table);
  if(ignore_table_inited)
    hash_free(&replicate_ignore_table);
  if(wild_do_table_inited)
    free_string_array(&replicate_wild_do_table);
  if(wild_ignore_table_inited)
    free_string_array(&replicate_wild_ignore_table);
}

inline bool slave_killed(THD* thd)
{
  return abort_slave || abort_loop || thd->killed;
}

inline void skip_load_data_infile(NET* net)
{
  (void)my_net_write(net, "\xfb/dev/null", 10);
  (void)net_flush(net);
  (void)my_net_read(net); // discard response
  send_ok(net); // the master expects it
}

inline char* rewrite_db(char* db)
{
  if(replicate_rewrite_db.is_empty() || !db) return db;
  I_List_iterator<i_string_pair> it(replicate_rewrite_db);
  i_string_pair* tmp;

  while((tmp=it++))
    {
      if(!strcmp(tmp->key, db))
	return tmp->val;
    }

  return db;
}

int db_ok(const char* db, I_List<i_string> &do_list,
	  I_List<i_string> &ignore_list )
{
  if(do_list.is_empty() && ignore_list.is_empty())
    return 1; // ok to replicate if the user puts no constraints

  // if the user has specified restrictions on which databases to replicate
  // and db was not selected, do not replicate
  if(!db)
    return 0;

  if(!do_list.is_empty()) // if the do's are not empty
    {
      I_List_iterator<i_string> it(do_list);
      i_string* tmp;

      while((tmp=it++))
	{
	  if(!strcmp(tmp->ptr, db))
	    return 1; // match
	}
      return 0;
    }
  else // there are some elements in the don't, otherwise we cannot get here
    {
      I_List_iterator<i_string> it(ignore_list);
      i_string* tmp;

      while((tmp=it++))
	{
	  if(!strcmp(tmp->ptr, db))
	    return 0; // match
	}
      
      return 1;
    }
}

static int init_strvar_from_file(char* var, int max_size, IO_CACHE* f,
			       char* default_val)
{
  uint length;
  if ((length=my_b_gets(f,var, max_size)))
  {
    char* last_p = var + length -1;
    if (*last_p == '\n')
      *last_p = 0; // if we stopped on newline, kill it
    else
    {
      // if we truncated a line or stopped on last char, remove all chars
      // up to and including newline
      int c;
      while( ((c=my_b_get(f)) != '\n' && c != my_b_EOF));
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
  else if(default_val)
  {
    *var = default_val;
    return 0;
  }
  return 1;
}


static int create_table_from_dump(THD* thd, NET* net, const char* db,
				  const char* table_name)
{
  uint packet_len = my_net_read(net); // read create table statement
  Vio* save_vio;
  HA_CHECK_OPT check_opt;
  TABLE_LIST tables;
  int error= 1;
  handler *file;
  
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
  thd->proc_info = "Creating table from master dump";
  // save old db in case we are creating in a different database
  char* save_db = thd->db;
  thd->db = thd->last_nx_db;
  mysql_parse(thd, thd->query, packet_len); // run create table
  thd->db = save_db;		// leave things the way the were before
  
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
  check_opt.flags|= T_VERY_SILENT | T_CALC_CHECKSUM;
  check_opt.quick = 1;
  thd->proc_info = "Rebuilding the index on master dump table";
  // we do not want repair() to spam us with messages
  // just send them to the error log, and report the failure in case of
  // problems
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

int fetch_nx_table(THD* thd, MASTER_INFO* mi)
{
  MYSQL* mysql = mc_mysql_init(NULL);
  int error = 1;
  int nx_errno = 0;
  if (!mysql)
  {
    sql_print_error("fetch_nx_table: Error in mysql_init()");
    nx_errno = ER_GET_ERRNO;
    goto err;
  }

  safe_connect(thd, mysql, mi);
  if (slave_killed(thd))
    goto err;

  if (request_table_dump(mysql, thd->last_nx_db, thd->last_nx_table))
  {
    nx_errno = ER_GET_ERRNO;
    sql_print_error("fetch_nx_table: failed on table dump request ");
    goto err;
  }

  if (create_table_from_dump(thd, &mysql->net, thd->last_nx_db,
			     thd->last_nx_table))
  {
    // create_table_from_dump will have sent the error alread
    sql_print_error("fetch_nx_table: failed on create table ");
    goto err;
  }
  
  error = 0;

 err:
  if (mysql)
    mc_mysql_close(mysql);
  if (nx_errno && thd->net.vio)
    send_error(&thd->net, nx_errno, "Error in fetch_nx_table");
  thd->net.no_send_ok = 0; // Clear up garbage after create_table_from_dump
  return error;
}

void end_master_info(MASTER_INFO* mi)
{
  if(mi->fd >= 0)
    {
      end_io_cache(&mi->file);
      (void)my_close(mi->fd, MYF(MY_WME));
      mi->fd = -1;
    }
  mi->inited = 0;
}

int init_master_info(MASTER_INFO* mi)
{
  if (mi->inited)
    return 0;
  int fd,length,error;
  MY_STAT stat_area;
  char fname[FN_REFLEN+128];
  const char *msg;
  fn_format(fname, master_info_file, mysql_data_home, "", 4+16+32);

  // we need a mutex while we are changing master info parameters to
  // keep other threads from reading bogus info

  pthread_mutex_lock(&mi->lock);
  mi->pending = 0;
  fd = mi->fd;
  
  // we do not want any messages if the file does not exist
  if (!my_stat(fname, &stat_area, MYF(0)))
  {
    // if someone removed the file from underneath our feet, just close
    // the old descriptor and re-create the old file
    if (fd >= 0)
      my_close(fd, MYF(MY_WME));
    if ((fd = my_open(fname, O_CREAT|O_RDWR|O_BINARY, MYF(MY_WME))) < 0
	|| init_io_cache(&mi->file, fd, IO_SIZE*2, READ_CACHE, 0L,0,
			 MYF(MY_WME)))
    {
      if(fd >= 0)
	my_close(fd, MYF(0));
      pthread_mutex_unlock(&mi->lock);
      return 1;
    }
    mi->log_file_name[0] = 0;
    mi->pos = 4; // skip magic number
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
    if(fd >= 0)
      reinit_io_cache(&mi->file, READ_CACHE, 0L,0,0);
    else if((fd = my_open(fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0
	    || init_io_cache(&mi->file, fd, IO_SIZE*2, READ_CACHE, 0L,
			     0, MYF(MY_WME)))
    {
      if(fd >= 0)
	my_close(fd, MYF(0));
      pthread_mutex_unlock(&mi->lock);
      return 1;
    }
      
    if ((length=my_b_gets(&mi->file, mi->log_file_name,
			   sizeof(mi->log_file_name))) < 1)
    {
      msg="Error reading log file name from master info file ";
      goto error;
    }

    mi->log_file_name[length-1]= 0; // kill \n
    /* Reuse fname buffer */
    if(!my_b_gets(&mi->file, fname, sizeof(fname)))
    {
      msg="Error reading log file position from master info file";
      goto error;
    }
    mi->pos = strtoull(fname,(char**) 0, 10);

    mi->fd = fd;
    if(init_strvar_from_file(mi->host, sizeof(mi->host), &mi->file,
			     master_host) ||
       init_strvar_from_file(mi->user, sizeof(mi->user), &mi->file,
			     master_user) || 
       init_strvar_from_file(mi->password, HASH_PASSWORD_LENGTH+1, &mi->file,
			     master_password) ||
       init_intvar_from_file((int*)&mi->port, &mi->file, master_port) ||
       init_intvar_from_file((int*)&mi->connect_retry, &mi->file,
			     master_connect_retry))
    {
      msg="Error reading master configuration";
      goto error;
    }
  }
  
  mi->inited = 1;
  // now change the cache from READ to WRITE - must do this
  // before flush_master_info
  reinit_io_cache(&mi->file, WRITE_CACHE, 0L,0,1);
  error=test(flush_master_info(mi));
  pthread_mutex_unlock(&mi->lock);
  return error;

error:
  sql_print_error(msg);
  end_io_cache(&mi->file);
  my_close(fd, MYF(0));
  pthread_mutex_unlock(&mi->lock);
  return 1;
}

int show_master_info(THD* thd)
{
  DBUG_ENTER("show_master_info");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Master_Host",
						     sizeof(glob_mi.host)));
  field_list.push_back(new Item_empty_string("Master_User",
						     sizeof(glob_mi.user)));
  field_list.push_back(new Item_empty_string("Master_Port", 6));
  field_list.push_back(new Item_empty_string("Connect_retry", 6));
  field_list.push_back( new Item_empty_string("Log_File",
						     FN_REFLEN));
  field_list.push_back(new Item_empty_string("Pos", 12));
  field_list.push_back(new Item_empty_string("Slave_Running", 3));
  field_list.push_back(new Item_empty_string("Replicate_do_db", 20));
  field_list.push_back(new Item_empty_string("Replicate_ignore_db", 20));
  field_list.push_back(new Item_empty_string("Last_errno", 4));
  field_list.push_back(new Item_empty_string("Last_error", 20));
  field_list.push_back(new Item_empty_string("Skip_counter", 12));
  if(send_fields(thd, field_list, 1))
    DBUG_RETURN(-1);

  String* packet = &thd->packet;
  packet->length(0);
  
  pthread_mutex_lock(&glob_mi.lock);
  net_store_data(packet, glob_mi.host);
  net_store_data(packet, glob_mi.user);
  net_store_data(packet, (uint32) glob_mi.port);
  net_store_data(packet, (uint32) glob_mi.connect_retry);
  net_store_data(packet, glob_mi.log_file_name);
  net_store_data(packet, (uint32) glob_mi.pos);	// QQ: Should be fixed
  pthread_mutex_unlock(&glob_mi.lock);
  pthread_mutex_lock(&LOCK_slave);
  net_store_data(packet, slave_running ? "Yes":"No");
  pthread_mutex_unlock(&LOCK_slave);
  net_store_data(packet, &replicate_do_db);
  net_store_data(packet, &replicate_ignore_db);
  net_store_data(packet, (uint32)last_slave_errno);
  net_store_data(packet, last_slave_error);
  net_store_data(packet, slave_skip_counter);
  
  if (my_net_write(&thd->net, (char*)thd->packet.ptr(), packet->length()))
    DBUG_RETURN(-1);

  send_eof(&thd->net);
  DBUG_RETURN(0);
}

int flush_master_info(MASTER_INFO* mi)
{
  IO_CACHE* file = &mi->file;
  char lbuf[22];
  
  my_b_seek(file, 0L);
  my_b_printf(file, "%s\n%s\n%s\n%s\n%s\n%d\n%d\n",
	      mi->log_file_name, llstr(mi->pos, lbuf), mi->host, mi->user,
	      mi->password, mi->port, mi->connect_retry);
  flush_io_cache(file);
  return 0;
}

int st_master_info::wait_for_pos(THD* thd, String* log_name, ulonglong log_pos)
{
  if (!inited) return -1;
  bool pos_reached;
  int event_count = 0;
  pthread_mutex_lock(&lock);
  while(!thd->killed)
  {
    int cmp_result;
    if (*log_file_name)
    {
      /*
	We should use dirname_length() here when we have a version of
	this that doesn't modify the argument */
      char *basename = strrchr(log_file_name, FN_LIBCHAR);
      if (basename)
	++basename;
      else
	basename = log_file_name;
      cmp_result =  strncmp(basename, log_name->ptr(),
			    log_name->length());
    }
    else
      cmp_result = 0;
      
    pos_reached = ((!cmp_result && pos >= log_pos) || cmp_result > 0);
    if (pos_reached || thd->killed)
      break;
    
    const char* msg = thd->enter_cond(&cond, &lock,
				      "Waiting for master update");
    pthread_cond_wait(&cond, &lock);
    thd->exit_cond(msg);
    event_count++;
  }
  pthread_mutex_unlock(&lock);
  return thd->killed ? -1 : event_count;
}


static int init_slave_thread(THD* thd)
{
  DBUG_ENTER("init_slave_thread");
  thd->system_thread = thd->bootstrap = 1;
  thd->client_capabilities = 0;
  my_net_init(&thd->net, 0);
  thd->net.timeout = slave_net_timeout;
  thd->max_packet_length=thd->net.max_packet;
  thd->master_access= ~0;
  thd->priv_user = 0;
  thd->slave_thread = 1;
  thd->options = (((opt_log_slave_updates) ? OPTION_BIN_LOG:0) | OPTION_AUTO_IS_NULL) ;
  thd->system_thread = 1;
  thd->client_capabilities = CLIENT_LOCAL_FILES;
  slave_real_id=thd->real_id=pthread_self();
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id = thread_id++;
  pthread_mutex_unlock(&LOCK_thread_count);

  if (init_thr_lock() ||
      my_pthread_setspecific_ptr(THR_THD,  thd) ||
      my_pthread_setspecific_ptr(THR_MALLOC, &thd->mem_root) ||
      my_pthread_setspecific_ptr(THR_NET,  &thd->net))
  {
    close_connection(&thd->net,ER_OUT_OF_RESOURCES); // is this needed?
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

  thd->mem_root.free=thd->mem_root.used=0;	// Probably not needed
  if (thd->max_join_size == (ulong) ~0L)
    thd->options |= OPTION_BIG_SELECTS;

  thd->proc_info="Waiting for master update";
  thd->version=refresh_version;
  thd->set_time();

  DBUG_RETURN(0);
}

static int safe_sleep(THD* thd, int sec)
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
      the only reason we are asking for alarm is so that
      we will be woken up in case of murder, so if we do not get killed,
      set the alarm so it goes off after we wake up naturally
    */
    thr_alarm(&alarmed, 2 * nap_time,&alarm_buff);
    sleep(nap_time);
    // if we wake up before the alarm goes off, hit the button
    // so it will not wake up the wife and kids :-)
    if (thr_alarm_in_use(&alarmed))
      thr_end_alarm(&alarmed);
    
    if (slave_killed(thd))
      return 1;
    start_time=time((time_t*) 0);
  }
  return 0;
}


static int request_dump(MYSQL* mysql, MASTER_INFO* mi)
{
  char buf[FN_REFLEN + 10];
  int len;
  int binlog_flags = 0; // for now
  char* logname = mi->log_file_name;
  int4store(buf, mi->pos);
  int2store(buf + 4, binlog_flags);
  int4store(buf + 6, server_id);
  len = (uint) strlen(logname);
  memcpy(buf + 10, logname,len);
  if (mc_simple_command(mysql, COM_BINLOG_DUMP, buf, len + 10, 1))
  {
    // something went wrong, so we will just reconnect and retry later
    // in the future, we should do a better error analysis, but for
    // now we just fill up the error log :-)
    sql_print_error("Error on COM_BINLOG_DUMP: %s, will retry in %d secs",
		    mc_mysql_error(mysql), master_connect_retry);
    return 1;
  }

  return 0;
}

static int request_table_dump(MYSQL* mysql, char* db, char* table)
{
  char buf[1024];
  char * p = buf;
  uint table_len = (uint) strlen(table);
  uint db_len = (uint) strlen(db);
  if(table_len + db_len > sizeof(buf) - 2)
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


static uint read_event(MYSQL* mysql, MASTER_INFO *mi)
{
  uint len = packet_error;
  // for convinience lets think we start by
  // being in the interrupted state :-)
  int read_errno = EINTR;

  // my_real_read() will time us out
  // we check if we were told to die, and if not, try reading again
#ifndef DBUG_OFF
  if (disconnect_slave_event_count && !(events_till_disconnect--))
    return packet_error;      
#endif
  
  while (!abort_loop && !abort_slave && len == packet_error &&
	 read_errno == EINTR )
  {
    len = mc_net_safe_read(mysql);
    read_errno = errno;
  }
  if (abort_loop || abort_slave)
    return packet_error;
  if (len == packet_error || (int) len < 1)
  {
    sql_print_error("Error reading packet from server: %s (read_errno %d,\
server_errno=%d)",
		    mc_mysql_error(mysql), read_errno, mc_mysql_errno(mysql));
    return packet_error;
  }

  if (len == 1)
  {
     sql_print_error("Slave: received 0 length packet from server, apparent\
 master shutdown: %s (%d)",
		     mc_mysql_error(mysql), read_errno);
     return packet_error;
  }
  
  DBUG_PRINT("info",( "len=%u, net->read_pos[4] = %d\n",
		      len, mysql->net.read_pos[4]));
  return len - 1;   
}

static int check_expected_error(THD* thd, int expected_error)
{
  switch(expected_error)
    {
    case ER_NET_READ_ERROR:
    case ER_NET_ERROR_ON_WRITE:  
    case ER_SERVER_SHUTDOWN:  
    case ER_NEW_ABORTING_CONNECTION:
      my_snprintf(last_slave_error, sizeof(last_slave_error), 
		 "Slave: query '%s' partially completed on the master \
and was aborted. There is a chance that your master is inconsistent at this \
point. If you are sure that your master is ok, run this query manually on the\
 slave and then restart the slave with SET SQL_SLAVE_SKIP_COUNTER=1;\
 SLAVE START;", thd->query);
      last_slave_errno = expected_error;
      sql_print_error("%s",last_slave_error);
      return 1;
    default:
      return 0;
    }
}

inline int ignored_error_code(int err_code)
{
  return use_slave_mask && bitmap_is_set(&slave_error_mask, err_code);
}

static int exec_event(THD* thd, NET* net, MASTER_INFO* mi, int event_len)
{
  Log_event * ev = Log_event::read_log_event((const char*)net->read_pos + 1,
					     event_len);
  char llbuff[22];
  
  if (ev)
  {
    int type_code = ev->get_type_code();
    if (ev->server_id == ::server_id || slave_skip_counter)
    {
      if(type_code == LOAD_EVENT)
	skip_load_data_infile(net);
	
      mi->inc_pos(event_len);
      flush_master_info(mi);
      if(slave_skip_counter && /* protect against common user error of
				  setting the counter to 1 instead of 2
			          while recovering from an failed
			          auto-increment insert */
	 	 !(type_code == INTVAR_EVENT &&
				 slave_skip_counter == 1))
        --slave_skip_counter;
      delete ev;     
      return 0;					// avoid infinite update loops
    }
  
    thd->server_id = ev->server_id; // use the original server id for logging
    thd->set_time();				// time the query
    if(!ev->when)
      ev->when = time(NULL);
    
    switch(type_code) {
    case QUERY_EVENT:
    {
      Query_log_event* qev = (Query_log_event*)ev;
      int q_len = qev->q_len;
      int expected_error,actual_error = 0;
      init_sql_alloc(&thd->mem_root, 8192,0);
      thd->db = rewrite_db((char*)qev->db);
      if (db_ok(thd->db, replicate_do_db, replicate_ignore_db))
      {
	thd->query = (char*)qev->query;
	thd->set_time((time_t)qev->when);
	thd->current_tablenr = 0;
	VOID(pthread_mutex_lock(&LOCK_thread_count));
	thd->query_id = query_id++;
	VOID(pthread_mutex_unlock(&LOCK_thread_count));
	thd->last_nx_table = thd->last_nx_db = 0;
	thd->query_error = 0;			// clear error
	thd->net.last_errno = 0;
	thd->net.last_error[0] = 0;
	thd->slave_proxy_id = qev->thread_id;	// for temp tables
	
	// sanity check to make sure the master did not get a really bad
	// error on the query
	if (ignored_error_code((expected_error=qev->error_code)) ||
	    !check_expected_error(thd, expected_error))
	{
	  mysql_parse(thd, thd->query, q_len);
	  if (expected_error !=
	      (actual_error = thd->net.last_errno) && expected_error &&
	      !ignored_error_code(actual_error))
	  {
	    const char* errmsg = "Slave: did not get the expected error\
 running query from master - expected: '%s' (%d), got '%s' (%d)"; 
	    sql_print_error(errmsg, ER_SAFE(expected_error),
			    expected_error,
			    actual_error ? thd->net.last_error:"no error",
			    actual_error);
	    thd->query_error = 1;
	  }
	  else if (expected_error == actual_error ||
		   ignored_error_code(actual_error))
	  {
	    thd->query_error = 0;
	    *last_slave_error = 0;
	    last_slave_errno = 0;
	  }
	}
	else
	{
	  // master could be inconsistent, abort and tell DBA to check/fix it
	  thd->db = thd->query = 0;
	  thd->convert_set = 0;
	  close_thread_tables(thd);
	  free_root(&thd->mem_root,0);
	  delete ev;
	  return 1;
	}
      }
      thd->db = 0;				// prevent db from being freed
      thd->query = 0;				// just to be sure
      // assume no convert for next query unless set explictly
      thd->convert_set = 0;
      close_thread_tables(thd);
      
      if (thd->query_error || thd->fatal_error)
      {
	sql_print_error("Slave:  error running query '%s' ",
			qev->query);
	last_slave_errno = actual_error ? actual_error : -1;
	my_snprintf(last_slave_error, sizeof(last_slave_error),
		    "error '%s' on query '%s'",
		    actual_error ? thd->net.last_error :
		    "unexpected success or fatal error",
		    qev->query
		    );
        free_root(&thd->mem_root,0);
	delete ev;
	return 1;
      }
      free_root(&thd->mem_root,0);
      delete ev;

      mi->inc_pos(event_len);
      flush_master_info(mi);
      break;
    }
	  
    case LOAD_EVENT:
    {
      Load_log_event* lev = (Load_log_event*)ev;
      init_sql_alloc(&thd->mem_root, 8192,0);
      thd->db = rewrite_db((char*)lev->db);
      thd->query = 0;
      thd->query_error = 0;
	    
      if(db_ok(thd->db, replicate_do_db, replicate_ignore_db))
      {
	thd->set_time((time_t)lev->when);
	thd->current_tablenr = 0;
	VOID(pthread_mutex_lock(&LOCK_thread_count));
	thd->query_id = query_id++;
	VOID(pthread_mutex_unlock(&LOCK_thread_count));

	TABLE_LIST tables;
	bzero((char*) &tables,sizeof(tables));
	tables.db = thd->db;
	tables.name = tables.real_name = (char*)lev->table_name;
	tables.lock_type = TL_WRITE;
	// the table will be opened in mysql_load    
        if(table_rules_on && !tables_ok(thd, &tables))
	{
	  skip_load_data_infile(net);
	}
	else
	{
	  enum enum_duplicates handle_dup = DUP_IGNORE;
	  if(lev->sql_ex.opt_flags && REPLACE_FLAG)
	    handle_dup = DUP_REPLACE;
	  sql_exchange ex((char*)lev->fname, lev->sql_ex.opt_flags &&
			  DUMPFILE_FLAG );
	  String field_term(&lev->sql_ex.field_term, 1),
	    enclosed(&lev->sql_ex.enclosed, 1),
	    line_term(&lev->sql_ex.line_term,1),
	    escaped(&lev->sql_ex.escaped, 1),
	    line_start(&lev->sql_ex.line_start, 1);
	    
	  ex.field_term = &field_term;
	  if(lev->sql_ex.empty_flags & FIELD_TERM_EMPTY)
	    ex.field_term->length(0);
	    
	  ex.enclosed = &enclosed;
	  if(lev->sql_ex.empty_flags & ENCLOSED_EMPTY)
	    ex.enclosed->length(0);

	  ex.line_term = &line_term;
	  if(lev->sql_ex.empty_flags & LINE_TERM_EMPTY)
	    ex.line_term->length(0);

	  ex.line_start = &line_start;
	  if(lev->sql_ex.empty_flags & LINE_START_EMPTY)
	    ex.line_start->length(0);

	  ex.escaped = &escaped;
	  if(lev->sql_ex.empty_flags & ESCAPED_EMPTY)
	    ex.escaped->length(0);

	  ex.opt_enclosed = (lev->sql_ex.opt_flags & OPT_ENCLOSED_FLAG);
	  if(lev->sql_ex.empty_flags & FIELD_TERM_EMPTY)
	    ex.field_term->length(0);
	    
	  ex.skip_lines = lev->skip_lines;
	    

	  List<Item> fields;
	  lev->set_fields(fields);
	  thd->slave_proxy_id = thd->thread_id;
	  thd->net.vio = net->vio;
	  // mysql_load will use thd->net to read the file
	  thd->net.pkt_nr = net->pkt_nr;
	  // make sure the client does not get confused
	  // about the packet sequence
	  if(mysql_load(thd, &ex, &tables, fields, handle_dup, 1,
			TL_WRITE))
	    thd->query_error = 1;
	  if(thd->cuted_fields)
	    sql_print_error("Slave: load data infile at position %s in log \
'%s' produced %d warning(s)", llstr(glob_mi.pos,llbuff), RPL_LOG_NAME,
			    thd->cuted_fields );
	  net->pkt_nr = thd->net.pkt_nr;
	}
      }
      else
      {
	// we will just ask the master to send us /dev/null if we do not
	// want to load the data :-)
	skip_load_data_infile(net);
      }
	    
      thd->net.vio = 0; 
      thd->db = 0;// prevent db from being freed
      close_thread_tables(thd);
      if(thd->query_error)
      {
	int sql_error = thd->net.last_errno;
	if(!sql_error)
	  sql_error = ER_UNKNOWN_ERROR;
		
	sql_print_error("Slave: Error '%s' running load data infile ",
			ER(sql_error));
	delete ev;
        free_root(&thd->mem_root,0);
	return 1;
      }
      
      delete ev;
      free_root(&thd->mem_root,0);
	    
      if(thd->fatal_error)
      {
	sql_print_error("Slave: Fatal error running query '%s' ",
			thd->query);
	return 1;
      }

      mi->inc_pos(event_len);
      flush_master_info(mi);
      break;
    }

    case START_EVENT:
      mi->inc_pos(event_len);
      flush_master_info(mi);
      delete ev;
      break;
                  
    case STOP_EVENT:
      if(mi->pos > 4) // stop event should be ignored after rotate event
	{
          close_temporary_tables(thd);
          mi->inc_pos(event_len);
          flush_master_info(mi);
	}
      delete ev;
      break;
    case ROTATE_EVENT:
    {
      Rotate_log_event* rev = (Rotate_log_event*)ev;
      int ident_len = rev->ident_len;
      pthread_mutex_lock(&mi->lock);
      memcpy(mi->log_file_name, rev->new_log_ident,ident_len );
      mi->log_file_name[ident_len] = 0;
      mi->pos = 4; // skip magic number
      pthread_cond_broadcast(&mi->cond);
      pthread_mutex_unlock(&mi->lock);
      flush_master_info(mi);
#ifndef DBUG_OFF
      if(abort_slave_event_count)
	++events_till_abort;
#endif      
      delete ev;
      break;
    }

    case INTVAR_EVENT:
    {
      Intvar_log_event* iev = (Intvar_log_event*)ev;
      switch(iev->type)
      {
      case LAST_INSERT_ID_EVENT:
	thd->last_insert_id_used = 1;
	thd->last_insert_id = iev->val;
	break;
      case INSERT_ID_EVENT:
	thd->next_insert_id = iev->val;
	break;
		
      }
      mi->inc_pending(event_len);
      delete ev;
      break;
    }
    }
  }
  else
  {
    sql_print_error("\
Could not parse log event entry, check the master for binlog corruption\n\
This may also be a network problem, or just a bug in the master or slave code.\
");
    return 1;
  }
  return 0;	  
}
      
// slave thread

pthread_handler_decl(handle_slave,arg __attribute__((unused)))
{
#ifndef DBUG_OFF
 slave_begin:  
#endif  
  THD *thd; // needs to be first for thread_stack
  MYSQL *mysql = NULL ;
  char llbuff[22];

  pthread_mutex_lock(&LOCK_slave);
  if (!server_id)
  {
    pthread_cond_broadcast(&COND_slave_start);
    pthread_mutex_unlock(&LOCK_slave);
    sql_print_error("Server id not set, will not start slave");
    pthread_exit((void*)1);
  }
  
  if(slave_running)
    {
      pthread_cond_broadcast(&COND_slave_start);
      pthread_mutex_unlock(&LOCK_slave);
      pthread_exit((void*)1);  // safety just in case
    }
  slave_running = 1;
  abort_slave = 0;
#ifndef DBUG_OFF  
  events_till_abort = abort_slave_event_count;
#endif  
  pthread_cond_broadcast(&COND_slave_start);
  pthread_mutex_unlock(&LOCK_slave);
  
  // int error = 1;
  bool retried_once = 0;
  ulonglong last_failed_pos = 0;
  
  // needs to call my_thread_init(), otherwise we get a coredump in DBUG_ stuff
  my_thread_init();
  slave_thd = thd = new THD; // note that contructor of THD uses DBUG_ !
  thd->set_time();
  DBUG_ENTER("handle_slave");

  pthread_detach_this_thread();
  if (init_slave_thread(thd) || init_master_info(&glob_mi))
    {
      sql_print_error("Failed during slave thread initialization");
      goto err;
    }
  thd->thread_stack = (char*)&thd; // remember where our stack is
  thd->temporary_tables = save_temporary_tables; // restore temp tables
  threads.append(thd);
  glob_mi.pending = 0;  //this should always be set to 0 when the slave thread
  // is started
  
  DBUG_PRINT("info",("master info: log_file_name=%s, position=%s",
		     glob_mi.log_file_name, llstr(glob_mi.pos,llbuff)));

  
  if (!(mysql = mc_mysql_init(NULL)))
  {
    sql_print_error("Slave thread: error in mc_mysql_init()");
    goto err;
  }
  
  thd->proc_info = "connecting to master";
#ifndef DBUG_OFF  
  sql_print_error("Slave thread initialized");
#endif
  // we can get killed during safe_connect
  if (!safe_connect(thd, mysql, &glob_mi))
   sql_print_error("Slave: connected to master '%s@%s:%d',\
  replication started in log '%s' at position %s", glob_mi.user,
		   glob_mi.host, glob_mi.port,
		   RPL_LOG_NAME,
		   llstr(glob_mi.pos,llbuff));
  else
  {
    sql_print_error("Slave thread killed while connecting to master");
    goto err;
  }
  
connected:
  
  while (!slave_killed(thd))
  {
      thd->proc_info = "Requesting binlog dump";
      if(request_dump(mysql, &glob_mi))
	{
	  sql_print_error("Failed on request_dump()");
	  if(slave_killed(thd))
	    {
	      sql_print_error("Slave thread killed while requesting master \
dump");
              goto err;
	    }
	  
	  thd->proc_info = "Waiiting to reconnect after a failed dump request";
	  if(mysql->net.vio)
	    vio_close(mysql->net.vio);
	  // first time retry immediately, assuming that we can recover
	  // right away - if first time fails, sleep between re-tries
	  // hopefuly the admin can fix the problem sometime
	  if(retried_once)
	    safe_sleep(thd, glob_mi.connect_retry);
	  else
	    retried_once = 1;
	  
	  if(slave_killed(thd))
	    {
	      sql_print_error("Slave thread killed while retrying master \
dump");
	      goto err;
	    }

	  thd->proc_info = "Reconnecting after a failed dump request";
	  last_failed_pos=glob_mi.pos;
          sql_print_error("Slave: failed dump request, reconnecting to \
try again, log '%s' at postion %s", RPL_LOG_NAME,
			  llstr(last_failed_pos,llbuff));
	  if(safe_reconnect(thd, mysql, &glob_mi) || slave_killed(thd))
	    {
	      sql_print_error("Slave thread killed during or after reconnect");
	      goto err;
	    }

	  goto connected;
	}

      while(!slave_killed(thd))
	{
	  thd->proc_info = "Reading master update";
	  uint event_len = read_event(mysql, &glob_mi);
	  if(slave_killed(thd))
	    {
	      sql_print_error("Slave thread killed while reading event");
	      goto err;
	    }
	  	  
	  if (event_len == packet_error)
	  {
	    if(mc_mysql_errno(mysql) == ER_NET_PACKET_TOO_LARGE)
	      {
		sql_print_error("Log entry on master is longer than \
max_allowed_packet on slave. Slave thread will be aborted. If the entry is \
really supposed to be that long, restart the server with a higher value of \
max_allowed_packet. The current value is %ld", max_allowed_packet);
		goto err;
	      }
	    
	    thd->proc_info = "Waiting to reconnect after a failed read";
	    if(mysql->net.vio)
 	      vio_close(mysql->net.vio);
	    if(retried_once) // punish repeat offender with sleep
	      safe_sleep(thd, glob_mi.connect_retry);
	    else
	      retried_once = 1; 
	    
	    if(slave_killed(thd))
	      {
		sql_print_error("Slave thread killed while waiting to \
reconnect after a failed read");
	        goto err;
	      }
	    thd->proc_info = "Reconnecting after a failed read";
	    last_failed_pos= glob_mi.pos;
	    sql_print_error("Slave: Failed reading log event, \
reconnecting to retry, log '%s' position %s", RPL_LOG_NAME,
			    llstr(last_failed_pos, llbuff));
	    if(safe_reconnect(thd, mysql, &glob_mi) || slave_killed(thd))
	      {
		sql_print_error("Slave thread killed during or after a \
reconnect done to recover from failed read");
	        goto err;
	      }
	    
	    goto connected;
	  } // if(event_len == packet_error)
	  
	  thd->proc_info = "Processing master log event"; 
	  if(exec_event(thd, &mysql->net, &glob_mi, event_len))
	    {
	      sql_print_error("\
Error running query, slave aborted. Fix the problem, and re-start \
the slave thread with \"mysqladmin start-slave\". We stopped at log \
'%s' position %s",
			      RPL_LOG_NAME, llstr(glob_mi.pos, llbuff));
	      goto err;
	      // there was an error running the query
	      // abort the slave thread, when the problem is fixed, the user
	      // should restart the slave with mysqladmin start-slave
	    }
#ifndef DBUG_OFF
	  if(abort_slave_event_count && !--events_till_abort)
	    {
	      sql_print_error("Slave: debugging abort");
	      goto err;
	    }
#endif	  
	  
	  // successful exec with offset advance,
	  // the slave repents and his sins are forgiven!
	  if(glob_mi.pos > last_failed_pos)
	    {
	     retried_once = 0;
#ifndef DBUG_OFF
	     stuck_count = 0;
#endif
	    }
#ifndef DBUG_OFF
	  else
	    {
	      // show a little mercy, allow slave to read one more event
	      // before cutting him off - otherwise he gets stuck
	      // on Intvar events, since they do not advance the offset
	      // immediately
	      if (++stuck_count > 2)
	        events_till_disconnect++;
	    }
#endif	  
	} // while(!slave_killed(thd)) - read/exec loop
  } // while(!slave_killed(thd)) - slave loop

  // error = 0;
 err:
  // print the current replication position 
  sql_print_error("Slave thread exiting, replication stopped in log '%s' at \
position %s",
		  RPL_LOG_NAME, llstr(glob_mi.pos,llbuff));
  thd->query = thd->db = 0; // extra safety
  if(mysql)
      mc_mysql_close(mysql);
  thd->proc_info = "Waiting for slave mutex on exit";
  pthread_mutex_lock(&LOCK_slave);
  slave_running = 0;
  abort_slave = 0;
  save_temporary_tables = thd->temporary_tables;
  thd->temporary_tables = 0; // remove tempation from destructor to close them
  pthread_cond_broadcast(&COND_slave_stopped); // tell the world we are done
  pthread_mutex_unlock(&LOCK_slave);
  net_end(&thd->net); // destructor will not free it, because we are weird
  slave_thd = 0;
  delete thd;
  my_thread_end();
#ifndef DBUG_OFF
  if(abort_slave_event_count && !events_till_abort)
    goto slave_begin;
#endif  
  pthread_exit(0);
  DBUG_RETURN(0);				// Can't return anything here
}


/* try to connect until successful or slave killed */

static int safe_connect(THD* thd, MYSQL* mysql, MASTER_INFO* mi)
{
  int slave_was_killed;
#ifndef DBUG_OFF
  events_till_disconnect = disconnect_slave_event_count;
#endif  
  while(!(slave_was_killed = slave_killed(thd)) &&
	!mc_mysql_connect(mysql, mi->host, mi->user, mi->password, 0,
			  mi->port, 0, 0))
  {
    sql_print_error("Slave thread: error connecting to master: %s (%d),\
 retry in %d sec", mc_mysql_error(mysql), errno, mi->connect_retry);
    safe_sleep(thd, mi->connect_retry);
  }
  
  if(!slave_was_killed)
    {
      mysql_log.write(thd, COM_CONNECT_OUT, "%s@%s:%d",
		  mi->user, mi->host, mi->port);
#ifdef SIGNAL_WITH_VIO_CLOSE
      thd->set_active_vio(mysql->net.vio);
#endif      
    }
  
  return slave_was_killed;
}

/*
  Try to connect until successful or slave killed or we have retried
  master_retry_count times
*/

static int safe_reconnect(THD* thd, MYSQL* mysql, MASTER_INFO* mi)
{
  int slave_was_killed;
  int last_errno= -2;				// impossible error
  ulong err_count=0;
  char llbuff[22];

  /*
    If we lost connection after reading a state set event
    we will be re-reading it, so pending needs to be cleared
  */
  mi->pending = 0;
#ifndef DBUG_OFF
  events_till_disconnect = disconnect_slave_event_count;
#endif
  while (!(slave_was_killed = slave_killed(thd)) && mc_mysql_reconnect(mysql))
  {
    /* Don't repeat last error */
    if (mc_mysql_errno(mysql) != last_errno)
    {
      sql_print_error("Slave thread: error re-connecting to master: \
%s, last_errno=%d, retry in %d sec",
		      mc_mysql_error(mysql), last_errno=mc_mysql_errno(mysql),
		      mi->connect_retry);
    }
    safe_sleep(thd, mi->connect_retry);
    /* if master_retry_count is not set, keep trying until success */
    if (master_retry_count && err_count++ == master_retry_count)
    {
      slave_was_killed=1;
      break;
    }
  }

  if (!slave_was_killed)
  {
    sql_print_error("Slave: reconnected to master '%s@%s:%d',\
replication resumed in log '%s' at position %s", glob_mi.user,
		    glob_mi.host, glob_mi.port,
		    RPL_LOG_NAME,
		    llstr(glob_mi.pos,llbuff));
#ifdef SIGNAL_WITH_VIO_CLOSE
    thd->set_active_vio(mysql->net.vio);
#endif      
  }

  return slave_was_killed;
}

#ifdef __GNUC__
template class I_List_iterator<i_string>;
template class I_List_iterator<i_string_pair>;
#endif
