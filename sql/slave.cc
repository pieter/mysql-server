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
#include "mini_client.h"
#include <thr_alarm.h>
#include <my_dir.h>

pthread_handler_decl(handle_slave,arg);
extern bool volatile abort_loop, abort_slave;

// the master variables are defaults read from my.cnf or command line
extern uint master_port, master_connect_retry;
extern my_string master_user, master_password, master_host,
  master_info_file;

extern I_List<i_string> replicate_do_db, replicate_ignore_db;
extern I_List<i_string_pair> replicate_rewrite_db;
extern I_List<THD> threads;
bool slave_running = 0;
pthread_t slave_real_id;
MASTER_INFO glob_mi;


extern bool opt_log_slave_updates ;

static inline void skip_load_data_infile(NET* net);
static inline bool slave_killed(THD* thd);
static int init_slave_thread(THD* thd);
int init_master_info(MASTER_INFO* mi);
static void safe_connect(THD* thd, MYSQL* mysql, MASTER_INFO* mi);
static void safe_reconnect(THD* thd, MYSQL* mysql, MASTER_INFO* mi);
static int safe_sleep(THD* thd, int sec);
static int request_table_dump(MYSQL* mysql, char* db, char* table);
static int create_table_from_dump(THD* thd, NET* net, const char* db,
				  const char* table_name);
static inline char* rewrite_db(char* db);

static inline bool slave_killed(THD* thd)
{
  return abort_slave || abort_loop || thd->killed;
}

static inline void skip_load_data_infile(NET* net)
{
  (void)my_net_write(net, "\xfb/dev/null", 10);
  (void)net_flush(net);
  (void)my_net_read(net); // discard response
  send_ok(net); // the master expects it
}

static inline char* rewrite_db(char* db)
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

  if(!db)
    return 0; // if the user has specified restrictions on which databases to replicate
  // and db was not selected, do not replicate

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

  // impossible
  return 0;
}

static void init_strvar_from_file(char* var, int max_size, FILE* f,
			       char* default_val)
{

  if(fgets(var, max_size, f)) 
    {
      char* last_p = strend(var) - 1;
      if(*last_p == '\n') *last_p = 0; // if we stopped on newline, kill it
      else
	while( (fgetc(f) != '\n' && !feof(f)));
      // if we truncated a line or stopped on last char, remove all chars
      // up to and including newline
    }
  else if(default_val)
   strmake(var,  default_val, max_size);
}

static void init_intvar_from_file(int* var, FILE* f,
			       int default_val)
{
  char buf[32];
  
  if(fgets(buf, sizeof(buf), f)) 
    {
      *var = atoi(buf);
    }
  else if(default_val)
   *var = default_val;
}


static int create_table_from_dump(THD* thd, NET* net, const char* db,
				  const char* table_name)
{
  uint packet_len = my_net_read(net); // read create table statement
  TABLE_LIST tables;
  int error = 0;
  
  if(packet_len == packet_error)
    {
      send_error(&thd->net, ER_MASTER_NET_READ);
      return 1;
    }
  if(net->read_pos[0] == 255) // error from master
    {
      net->read_pos[packet_len] = 0;
      net_printf(&thd->net, ER_MASTER, net->read_pos + 3);
      return 1;
    }
  thd->command = COM_TABLE_DUMP;
  thd->query = sql_alloc(packet_len + 1);
  if(!thd->query)
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
  char* save_db = thd->db;
  thd->db = thd->last_nx_db; // in case we are creating in a different
  // database
  mysql_parse(thd, thd->query, packet_len); // run create table
  thd->db = save_db; // leave things the way the were before
  
  if(thd->query_error)
    {
      close_thread_tables(thd); // mysql_parse takes care of the error send
      return 1;
    }

  bzero((char*) &tables,sizeof(tables));
  tables.db = (char*)db;
  tables.name = tables.real_name = (char*)table_name;
  tables.lock_type = TL_WRITE;
  thd->proc_info = "Opening master dump table";
  if(!open_ltable(thd, &tables, TL_WRITE))
    {
      // open tables will send the error
      sql_print_error("create_table_from_dump: could not open created table");
      close_thread_tables(thd);
      return 1;
    }
  
  handler *file = tables.table->file;
  thd->proc_info = "Reading master dump table data";
  if(file->net_read_dump(net))
    {
      net_printf(&thd->net, ER_MASTER_NET_READ);
      sql_print_error("create_table_from_dump::failed in\
 handler::net_read_dump()");
      close_thread_tables(thd);
      return 1;
    }

  HA_CHECK_OPT check_opt;
  check_opt.init();
  check_opt.quick = 1;
  thd->proc_info = "rebuilding the index on master dump table";
  Vio* save_vio = thd->net.vio;
  thd->net.vio = 0; // we do not want repair() to spam us with messages
  // just send them to the error log, and report the failure in case of
  // problems
  if(file->repair(thd,&check_opt ))
    {
      net_printf(&thd->net, ER_INDEX_REBUILD,tables.table->real_name );
      error = 1;
    }
  thd->net.vio = save_vio;
  close_thread_tables(thd);
  
  thd->net.no_send_ok = 0;
  return error; 
}

int fetch_nx_table(THD* thd, MASTER_INFO* mi)
{
  MYSQL* mysql = mc_mysql_init(NULL);
  int error = 1;
  int nx_errno = 0;
  if(!mysql)
    {
      sql_print_error("fetch_nx_table: Error in mysql_init()");
      nx_errno = ER_GET_ERRNO;
      goto err;
    }

  safe_connect(thd, mysql, mi);
  if(slave_killed(thd))
    goto err;

  if(request_table_dump(mysql, thd->last_nx_db, thd->last_nx_table))
    {
      nx_errno = ER_GET_ERRNO;
      sql_print_error("fetch_nx_table: failed on table dump request ");
      goto err;
    }

  if(create_table_from_dump(thd, &mysql->net, thd->last_nx_db,
			    thd->last_nx_table))
    {
      // create_table_from_dump will have sent the error alread
      sql_print_error("fetch_nx_table: failed on create table ");
      goto err;
    }
  
  error = 0;
 err:
  if(mysql)
    {
     mc_mysql_close(mysql);
     mysql = 0;
    }
  if(nx_errno && thd->net.vio)
    send_error(&thd->net, nx_errno, "Error in fetch_nx_table");
  
  return error;
}

int init_master_info(MASTER_INFO* mi)
{
  FILE* file;
  MY_STAT stat_area;
  char fname[FN_REFLEN+128];
  fn_format(fname, master_info_file, mysql_data_home, "", 4+16+32);
  

  // we need a mutex while we are changing master info parameters to
  // keep other threads from reading bogus info

  pthread_mutex_lock(&mi->lock);
  
  
  if(!my_stat(fname, &stat_area, MYF(0))) // we do not want any messages
    // if the file does not exist
    {
      file = my_fopen(fname, O_CREAT|O_RDWR|O_BINARY, MYF(MY_WME));
      if(!file)
	return 1;
      mi->log_file_name[0] = 0;
      mi->pos = 4; // skip magic number
      mi->file = file;
      
      if(master_host)
        strmake(mi->host, master_host, sizeof(mi->host) - 1);
      if(master_user)
        strmake(mi->user, master_user, sizeof(mi->user) - 1);
      if(master_password)
        strmake(mi->password, master_password, sizeof(mi->password) - 1);
      mi->port = master_port;
      mi->connect_retry = master_connect_retry;
      
      if(flush_master_info(mi))
	return 1;
    }
  else
    {
      file = my_fopen(fname, O_RDWR|O_BINARY, MYF(MY_WME));
      if(!file)
	return 1;
      
      if(!fgets(mi->log_file_name, sizeof(mi->log_file_name), file))
	{
	  sql_print_error("Error reading log file name from master info file ");
	  return 1;
	}

      *(strend(mi->log_file_name) - 1) = 0; // kill \n
      char buf[FN_REFLEN];
      if(!fgets(buf, sizeof(buf), file))
	{
	  sql_print_error("Error reading log file position from master info file");
	  return 1;
	}

      mi->pos = atoi(buf);
      mi->file = file;
      init_strvar_from_file(mi->host, sizeof(mi->host), file, master_host);
      init_strvar_from_file(mi->user, sizeof(mi->user), file, master_user); 
      init_strvar_from_file(mi->password, sizeof(mi->password), file,
			 master_password);
      
      init_intvar_from_file((int*)&mi->port, file, master_port);	
      init_intvar_from_file((int*)&mi->connect_retry, file,
			    master_connect_retry);
      
    }
  
  mi->inited = 1;
  pthread_mutex_unlock(&mi->lock);
  
  return 0;
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
  net_store_data(packet, (longlong)glob_mi.pos);
  pthread_mutex_unlock(&glob_mi.lock);
  pthread_mutex_lock(&LOCK_slave);
  net_store_data(packet, slave_running ? "Yes":"No");
  pthread_mutex_unlock(&LOCK_slave);
  net_store_data(packet, &replicate_do_db);
  net_store_data(packet, &replicate_ignore_db);
  
  if(my_net_write(&thd->net, (char*)thd->packet.ptr(), packet->length()))
    DBUG_RETURN(-1);

  send_eof(&thd->net);
  DBUG_RETURN(0);
}

int flush_master_info(MASTER_INFO* mi)
{
  FILE* file = mi->file;
  char lbuf[22];
  
  if(my_fseek(file, 0L, MY_SEEK_SET, MYF(MY_WME)) == MY_FILEPOS_ERROR ||
     fprintf(file, "%s\n%s\n%s\n%s\n%s\n%d\n%d\n",
        mi->log_file_name, llstr(mi->pos, lbuf), mi->host, mi->user, mi->password,
	     mi->port, mi->connect_retry) < 0 ||
     fflush(file))
    {
      sql_print_error("Write error flushing master_info: %d", errno);
      return 1;
    }

  return 0;
}


static int init_slave_thread(THD* thd)
{
  DBUG_ENTER("init_slave_thread");
  thd->system_thread = thd->bootstrap = 1;
  thd->client_capabilities = 0;
  my_net_init(&thd->net, 0);
  thd->max_packet_length=thd->net.max_packet;
  thd->master_access= ~0;
  thd->priv_user = 0;
  thd->options = (((opt_log_slave_updates) ? OPTION_BIN_LOG:0)
    | OPTION_AUTO_COMMIT | OPTION_AUTO_IS_NULL) ;
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
#ifndef __WIN__
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
    thr_alarm(&alarmed, 2 * nap_time,&alarm_buff); // the only reason we are asking for alarm is so that
    // we will be woken up in case of murder, so if we do not get killed, set the alarm
    // so it goes off after we wake up naturally
    sleep(nap_time);
    if (thr_alarm_in_use(&alarmed)) // if we wake up before the alarm goes off, hit the button
      thr_end_alarm(&alarmed);     // so it will not wake up the wife and kids :-)
    
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
  if(mc_simple_command(mysql, COM_BINLOG_DUMP, buf, len + 10, 1))
	// something went wrong, so we will just reconnect and retry later
	// in the future, we should do a better error analysis, but for
	// now we just fill up the error log :-)
    {
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
  
  if(mc_simple_command(mysql, COM_TABLE_DUMP, buf, p - buf + table_len, 1))
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
  int read_errno = EINTR; // for convinience lets think we start by
  // being in the interrupted state :-)
  // my_real_read() will time us out
  // we check if we were told to die, and if not, try reading again
  while (!abort_loop && !abort_slave && len == packet_error && read_errno == EINTR )
  {
    len = mc_net_safe_read(mysql);
    read_errno = errno;
  }
  if(abort_loop || abort_slave)
    return packet_error;
  if (len == packet_error || (int) len < 1)
  {
    sql_print_error("Error reading packet from server: %s (read_errno %d,\
server_errno=%d)",
		    mc_mysql_error(mysql), read_errno, mc_mysql_errno(mysql));
    return packet_error;
  }

  if(len == 1)
    {
     sql_print_error("Received 0 length packet from server, looks like master shutdown: %s (%d)",
		    mc_mysql_error(mysql), read_errno);
     return packet_error;
    }
  
  DBUG_PRINT("info",( "len=%u, net->read_pos[4] = %d\n",
		      len, mysql->net.read_pos[4]));

  return len - 1;   
}


static int exec_event(THD* thd, NET* net, MASTER_INFO* mi, int event_len)
{
  Log_event * ev = Log_event::read_log_event((const char*)net->read_pos + 1,
					     event_len);
  
  if (ev)
  {
    int type_code = ev->get_type_code();
    if(ev->server_id == ::server_id)
      {
	if(type_code == LOAD_EVENT)
	  skip_load_data_infile(net);
	
	mi->inc_pos(event_len);
	flush_master_info(mi);
	delete ev;     
	return 0; // avoid infinite update loops
      }
  
    thd->server_id = ev->server_id; // use the original server id for logging
    thd->set_time(); // time the query
    ev->when = time(NULL);
    
    switch(type_code)
    {
    case QUERY_EVENT:
    {
      Query_log_event* qev = (Query_log_event*)ev;
      int q_len = qev->q_len;
      init_sql_alloc(&thd->mem_root, 8192,0);
      thd->db = rewrite_db((char*)qev->db);
      if(db_ok(thd->db, replicate_do_db, replicate_ignore_db))
      {
	thd->query = (char*)qev->query;
	thd->set_time((time_t)qev->when);
	thd->current_tablenr = 0;
	VOID(pthread_mutex_lock(&LOCK_thread_count));
	thd->query_id = query_id++;
	VOID(pthread_mutex_unlock(&LOCK_thread_count));
	thd->last_nx_table = thd->last_nx_db = 0;
	thd->query_error = 0; // clear error
	thd->net.last_errno = 0;
	thd->net.last_error[0] = 0;
	mysql_parse(thd, thd->query, q_len);
	int expected_error,actual_error;
	if((expected_error = qev->error_code) !=
	   (actual_error = thd->net.last_errno) && expected_error)
	  {
	    sql_print_error("Slave: did not get the expected error\
 running query from master - expected: '%s', got '%s'",
			    ER(expected_error),
			    actual_error ? ER(actual_error):"no error"
			    );
	    thd->query_error = 1;
	  }
	else if(expected_error == actual_error)
	  thd->query_error = 0;
      }
      thd->db = 0;// prevent db from being freed
      thd->query = 0; // just to be sure
      thd->convert_set = 0; // assume no convert for next query
      // unless set explictly
      close_thread_tables(thd);
      free_root(&thd->mem_root,0);
      delete ev;
      
      if (thd->query_error)
      {
	sql_print_error("Slave:  error running query '%s' ",
			qev->query);
	return 1;
      }
	    
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

	enum enum_duplicates handle_dup = DUP_IGNORE;
	if(lev->sql_ex.opt_flags && REPLACE_FLAG)
	  handle_dup = DUP_REPLACE;
	sql_exchange ex((char*)lev->fname, lev->sql_ex.opt_flags &&
			DUMPFILE_FLAG );
	String field_term(&lev->sql_ex.field_term, 1),
	  enclosed(&lev->sql_ex.enclosed, 1), line_term(&lev->sql_ex.line_term,1),
	  escaped(&lev->sql_ex.escaped, 1), line_start(&lev->sql_ex.line_start, 1);
	    
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
	    
	TABLE_LIST tables;
	bzero((char*) &tables,sizeof(tables));
	tables.db = thd->db;
	tables.name = tables.real_name = (char*)lev->table_name;
	tables.lock_type = TL_WRITE;
	    
	if (open_tables(thd, &tables))
	{
	  sql_print_error("Slave:  error opening table %s ",
			  tables.name);
	  delete ev;
	  return 1;
	}

	List<Item> fields;
	lev->set_fields(fields);
	thd->net.vio = net->vio;
	// mysql_load will use thd->net to read the file
	thd->net.pkt_nr = net->pkt_nr;
	// make sure the client does get confused
	// about the packet sequence
	if(mysql_load(thd, &ex, &tables, fields, handle_dup, 1,
		      TL_WRITE))
	  thd->query_error = 1;
	net->pkt_nr = thd->net.pkt_nr;
      }
      else // we will just ask the master to send us /dev/null if we do not want to
	// load the data :-)
      {
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
		
	sql_print_error("Slave:  error '%s' running load data infile ",
			ER(sql_error));
	delete ev;
	return 1;
      }
      delete ev;
	    
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
      break;
                  
    case STOP_EVENT:
      mi->inc_pos(event_len);
      flush_master_info(mi);
      break;
    case ROTATE_EVENT:
    {
      Rotate_log_event* rev = (Rotate_log_event*)ev;
      int ident_len = rev->ident_len;
      memcpy(mi->log_file_name, rev->new_log_ident,ident_len );
      mi->log_file_name[ident_len] = 0;
      mi->pos = 4; // skip magic number
      flush_master_info(mi);
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
      break;
    }
    }
                  
  }
  else
  {
    sql_print_error("Could not parse log event entry, check the master for binlog corruption\n\
 This may also be a network problem, or just a bug in the master or slave code");
    return 1;
  }
  return 0;	  
}
      
// slave thread

pthread_handler_decl(handle_slave,arg __attribute__((unused)))
{
  THD *thd;; // needs to be first for thread_stack
  MYSQL *mysql = NULL ;

  if(!server_id)
    {
     sql_print_error("Server id not set, will not start slave");
     pthread_exit((void*)1);
    }
  
  pthread_mutex_lock(&LOCK_slave);
  if(slave_running)
    {
      pthread_mutex_unlock(&LOCK_slave);
      pthread_exit((void*)1);  // safety just in case
    }
  slave_running = 1;
  abort_slave = 0;
  pthread_mutex_unlock(&LOCK_slave);
  
  int error = 1;
  
  my_thread_init(); // needs to be up here, otherwise we get a coredump
  // trying to use DBUG_ stuff
  thd = new THD; // note that contructor of THD uses DBUG_ !
  thd->set_time();
  DBUG_ENTER("handle_slave");

  pthread_detach_this_thread();
  if(init_slave_thread(thd) || init_master_info(&glob_mi))
    goto err;
  thd->thread_stack = (char*)&thd; // remember where our stack is

  threads.append(thd);
  
  DBUG_PRINT("info",("master info: log_file_name=%s, position=%d",
		     glob_mi.log_file_name, glob_mi.pos));

  mysql = mc_mysql_init(NULL);
  if(!mysql)
    {
      sql_print_error("Slave thread: error in mc_mysql_init()");
      goto err;
    }
  
  thd->proc_info = "connecting to master";
  safe_connect(thd, mysql, &glob_mi);
  
  while(!slave_killed(thd))
    {
      thd->proc_info = "requesting binlog dump";
      if(request_dump(mysql, &glob_mi))
	{
	  sql_print_error("Failed on request_dump()");
	  if(slave_killed(thd))
           goto err;
	  
	  thd->proc_info = "waiting to reconnect after a failed dump request";
	  safe_sleep(thd, glob_mi.connect_retry);
	  if(slave_killed(thd))
	      goto err;

	  thd->proc_info = "reconnecting after a failed dump request";

	  safe_reconnect(thd, mysql, &glob_mi);
	  if(slave_killed(thd))
	      goto err;

	  continue;
	}


      while(!slave_killed(thd))
	{
          bool reset = 0;
	  thd->proc_info = "reading master update";
	  uint event_len = read_event(mysql, &glob_mi);
	  if(slave_killed(thd))
	    goto err;
	  
	  if (event_len == packet_error)
	  {
	    thd->proc_info = "waiting to reconnect after a failed read";
	    safe_sleep(thd, glob_mi.connect_retry);
	    if(slave_killed(thd))
	      goto err;
	    thd->proc_info = "reconnecting after a failed read";
	    safe_reconnect(thd, mysql, &glob_mi);
	    if(slave_killed(thd))
	      goto err;
	    reset = 1;
	  }
	  
	  if(reset)
	    break;
	  
	  thd->proc_info = "processing master log event"; 
	  if(exec_event(thd, &mysql->net, &glob_mi, event_len))
	    {
	      sql_print_error("Error running query, slave aborted. Fix the problem, and re-start\
 the slave thread with mysqladmin start-slave");
	      goto err;
	      // there was an error running the query
	      // abort the slave thread, when the problem is fixed, the user
	      // should restart the slave with mysqladmin start-slave
	    }

	}
    }

  error = 0;
 err:
  thd->query = thd->db = 0; // extra safety
  if(mysql)
    {
      mc_mysql_close(mysql);
      mysql = 0;
    }
  thd->proc_info = "waiting for slave mutex on exit";
  pthread_mutex_lock(&LOCK_slave);
  slave_running = 0;
  abort_slave = 0;
  pthread_cond_broadcast(&COND_slave_stopped); // tell the world we are done
  pthread_mutex_unlock(&LOCK_slave);
  delete thd;
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);				// Can't return anything here
}

static void safe_connect(THD* thd, MYSQL* mysql, MASTER_INFO* mi)
  // will try to connect until successful
{
  while(!slave_killed(thd) &&
	!mc_mysql_connect(mysql, mi->host, mi->user, mi->password, 0,
			  mi->port, 0, 0))
  {
    sql_print_error(
		    "Slave thread: error connecting to master:%s, retry in %d sec",
		    mc_mysql_error(mysql), mi->connect_retry);
    safe_sleep(thd, mi->connect_retry);
  }
  
  mysql_log.write(thd, COM_CONNECT_OUT, "%s@%s:%d",
		  mi->user, mi->host, mi->port);
  
}

// will try to connect until successful

static void safe_reconnect(THD* thd, MYSQL* mysql, MASTER_INFO* mi)
{
  while(!slave_killed(thd) && mc_mysql_reconnect(mysql))
  {
    sql_print_error(
		    "Slave thread: error connecting to master:%s, retry in %d sec",
		    mc_mysql_error(mysql), mi->connect_retry);
    safe_sleep(thd, mi->connect_retry);
  }
  
}

#ifdef __GNUC__
template class I_List_iterator<i_string>;
template class I_List_iterator<i_string_pair>;
#endif

