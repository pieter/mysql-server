/* Copyright (C) 2000-2003 MySQL AB

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
  Handling of MySQL SQL variables

  To add a new variable, one has to do the following:

  - If the variable is thread specific, add it to 'system_variables' struct.
    If not, add it to mysqld.cc and an declaration in 'mysql_priv.h'
  - Don't forget to initialize new fields in global_system_variables and
     max_system_variables!
  - Use one of the 'sys_var... classes from set_var.h or write a specific
    one for the variable type.
  - Define it in the 'variable definition list' in this file.
  - If the variable should be changeable or one should be able to access it
    with @@variable_name, it should be added to the 'list of all variables'
    list in this file.
  - If the variable should be changed from the command line, add a definition
    of it in the my_option structure list in mysqld.dcc
  - If the variable should show up in 'show variables' add it to the
    init_vars[] struct in this file

  TODO:
    - Add full support for the variable character_set (for 4.1)

    - When updating myisam_delay_key_write, we should do a 'flush tables'
      of all MyISAM tables to ensure that they are reopen with the
      new attribute.
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <mysql.h>
#include "slave.h"
#include "sql_acl.h"
#include <my_getopt.h>
#include <thr_alarm.h>
#include <myisam.h>
#ifdef HAVE_BERKELEY_DB
#include "ha_berkeley.h"
#endif
#ifdef HAVE_INNOBASE_DB
#include "ha_innodb.h"
#endif

static HASH system_variable_hash;
const char *bool_type_names[]= { "OFF", "ON", NullS };
TYPELIB bool_typelib=
{
  array_elements(bool_type_names)-1, "", bool_type_names
};

const char *delay_key_write_type_names[]= { "OFF", "ON", "ALL", NullS };
TYPELIB delay_key_write_typelib=
{
  array_elements(delay_key_write_type_names)-1, "", delay_key_write_type_names
};

static bool sys_check_charset(THD *thd, set_var *var);
static bool sys_update_charset(THD *thd, set_var *var);
static void sys_set_default_charset(THD *thd, enum_var_type type);
static bool set_option_bit(THD *thd, set_var *var);
static bool set_option_autocommit(THD *thd, set_var *var);
static bool set_log_update(THD *thd, set_var *var);
static void fix_low_priority_updates(THD *thd, enum_var_type type);
static void fix_tx_isolation(THD *thd, enum_var_type type);
static void fix_net_read_timeout(THD *thd, enum_var_type type);
static void fix_net_write_timeout(THD *thd, enum_var_type type);
static void fix_net_retry_count(THD *thd, enum_var_type type);
static void fix_max_join_size(THD *thd, enum_var_type type);
static void fix_query_cache_size(THD *thd, enum_var_type type);
static void fix_query_cache_min_res_unit(THD *thd, enum_var_type type);
static void fix_myisam_max_extra_sort_file_size(THD *thd, enum_var_type type);
static void fix_myisam_max_sort_file_size(THD *thd, enum_var_type type);
static void fix_max_binlog_size(THD *thd, enum_var_type type);
static void fix_max_relay_log_size(THD *thd, enum_var_type type);
static void fix_max_connections(THD *thd, enum_var_type type);
static KEY_CACHE *create_key_cache(const char *name, uint length);
void fix_sql_mode_var(THD *thd, enum_var_type type);
static byte *get_error_count(THD *thd);
static byte *get_warning_count(THD *thd);

/*
  Variable definition list

  These are variables that can be set from the command line, in
  alphabetic order
*/

sys_var_long_ptr	sys_binlog_cache_size("binlog_cache_size",
					      &binlog_cache_size);
sys_var_thd_ulong	sys_bulk_insert_buff_size("bulk_insert_buffer_size",
						  &SV::bulk_insert_buff_size);
sys_var_character_set_server	sys_character_set_server("character_set_server");
sys_var_str			sys_charset_system("character_set_system",
				    sys_check_charset,
				    sys_update_charset,
				    sys_set_default_charset);
sys_var_character_set_database	sys_character_set_database("character_set_database");
sys_var_character_set_client  sys_character_set_client("character_set_client");
sys_var_character_set_connection  sys_character_set_connection("character_set_connection");
sys_var_character_set_results sys_character_set_results("character_set_results");
sys_var_collation_connection sys_collation_connection("collation_connection");
sys_var_collation_database sys_collation_database("collation_database");
sys_var_collation_server sys_collation_server("collation_server");
sys_var_bool_ptr	sys_concurrent_insert("concurrent_insert",
					      &myisam_concurrent_insert);
sys_var_long_ptr	sys_connect_timeout("connect_timeout",
					    &connect_timeout);
sys_var_enum		sys_delay_key_write("delay_key_write",
					    &delay_key_write_options,
					    &delay_key_write_typelib,
					    fix_delay_key_write);
sys_var_long_ptr	sys_delayed_insert_limit("delayed_insert_limit",
						 &delayed_insert_limit);
sys_var_long_ptr	sys_delayed_insert_timeout("delayed_insert_timeout",
						   &delayed_insert_timeout);
sys_var_long_ptr	sys_delayed_queue_size("delayed_queue_size",
					       &delayed_queue_size);
sys_var_long_ptr	sys_expire_logs_days("expire_logs_days",
					     &expire_logs_days);
sys_var_bool_ptr	sys_flush("flush", &myisam_flush);
sys_var_long_ptr	sys_flush_time("flush_time", &flush_time);
sys_var_thd_ulong	sys_interactive_timeout("interactive_timeout",
						&SV::net_interactive_timeout);
sys_var_thd_ulong	sys_join_buffer_size("join_buffer_size",
					     &SV::join_buff_size);
sys_var_key_buffer_size	sys_key_buffer_size("key_buffer_size");
sys_var_bool_ptr	sys_local_infile("local_infile",
					 &opt_local_infile);
sys_var_thd_bool	sys_log_warnings("log_warnings", &SV::log_warnings);
sys_var_thd_ulong	sys_long_query_time("long_query_time",
					     &SV::long_query_time);
sys_var_thd_bool	sys_low_priority_updates("low_priority_updates",
						 &SV::low_priority_updates,
						 fix_low_priority_updates);
#ifndef TO_BE_DELETED	/* Alias for the low_priority_updates */
sys_var_thd_bool	sys_sql_low_priority_updates("sql_low_priority_updates",
						     &SV::low_priority_updates,
						     fix_low_priority_updates);
#endif
sys_var_thd_ulong	sys_max_allowed_packet("max_allowed_packet",
					       &SV::max_allowed_packet);
sys_var_long_ptr	sys_max_binlog_cache_size("max_binlog_cache_size",
						  &max_binlog_cache_size);
sys_var_long_ptr	sys_max_binlog_size("max_binlog_size",
					    &max_binlog_size,
                                            fix_max_binlog_size);
sys_var_long_ptr	sys_max_connections("max_connections",
					    &max_connections,
                                            fix_max_connections);
sys_var_long_ptr	sys_max_connect_errors("max_connect_errors",
					       &max_connect_errors);
sys_var_long_ptr	sys_max_delayed_threads("max_delayed_threads",
						&max_insert_delayed_threads,
						fix_max_connections);
sys_var_thd_ulong	sys_max_error_count("max_error_count",
					    &SV::max_error_count);
sys_var_thd_ulong	sys_max_heap_table_size("max_heap_table_size",
						&SV::max_heap_table_size);
/* 
   sys_pseudo_thread_id has its own class (instead of sys_var_thd_ulong) because
   we want a check() function.
*/
sys_var_pseudo_thread_id sys_pseudo_thread_id("pseudo_thread_id",
					     &SV::pseudo_thread_id);
sys_var_thd_ha_rows	sys_max_join_size("max_join_size",
					  &SV::max_join_size,
					  fix_max_join_size);
sys_var_thd_ulong	sys_max_seeks_for_key("max_seeks_for_key",
					      &SV::max_seeks_for_key);
sys_var_thd_ulong   sys_max_length_for_sort_data("max_length_for_sort_data",
                        &SV::max_length_for_sort_data);
#ifndef TO_BE_DELETED	/* Alias for max_join_size */
sys_var_thd_ha_rows	sys_sql_max_join_size("sql_max_join_size",
					      &SV::max_join_size,
					      fix_max_join_size);
#endif
sys_var_thd_ulong	sys_max_prep_stmt_count("max_prepared_statements",
						&SV::max_prep_stmt_count);
sys_var_long_ptr	sys_max_relay_log_size("max_relay_log_size",
                                               &max_relay_log_size,
                                               fix_max_relay_log_size);
sys_var_thd_ulong	sys_max_sort_length("max_sort_length",
					    &SV::max_sort_length);
sys_var_long_ptr	sys_max_user_connections("max_user_connections",
						 &max_user_connections);
sys_var_thd_ulong	sys_max_tmp_tables("max_tmp_tables",
					   &SV::max_tmp_tables);
sys_var_long_ptr	sys_max_write_lock_count("max_write_lock_count",
						 &max_write_lock_count);
sys_var_thd_ulonglong	sys_myisam_max_extra_sort_file_size("myisam_max_extra_sort_file_size", &SV::myisam_max_extra_sort_file_size, fix_myisam_max_extra_sort_file_size, 1);
sys_var_thd_ulonglong	sys_myisam_max_sort_file_size("myisam_max_sort_file_size", &SV::myisam_max_sort_file_size, fix_myisam_max_sort_file_size, 1);
sys_var_thd_ulong       sys_myisam_repair_threads("myisam_repair_threads", &SV::myisam_repair_threads);
sys_var_thd_ulong	sys_myisam_sort_buffer_size("myisam_sort_buffer_size", &SV::myisam_sort_buff_size);
sys_var_thd_ulong	sys_net_buffer_length("net_buffer_length",
					      &SV::net_buffer_length);
sys_var_thd_ulong	sys_net_read_timeout("net_read_timeout",
					     &SV::net_read_timeout,
					     fix_net_read_timeout);
sys_var_thd_ulong	sys_net_write_timeout("net_write_timeout",
					      &SV::net_write_timeout,
					      fix_net_write_timeout);
sys_var_thd_ulong	sys_net_retry_count("net_retry_count",
					    &SV::net_retry_count,
					    fix_net_retry_count);
sys_var_thd_bool	sys_new_mode("new", &SV::new_mode);
sys_var_thd_bool	sys_old_passwords("old_passwords", &SV::old_passwords);
sys_var_thd_ulong       sys_preload_buff_size("preload_buffer_size",
                                              &SV::preload_buff_size);
sys_var_thd_ulong	sys_read_buff_size("read_buffer_size",
					   &SV::read_buff_size);
sys_var_bool_ptr	sys_readonly("read_only", &opt_readonly);
sys_var_thd_ulong	sys_read_rnd_buff_size("read_rnd_buffer_size",
					       &SV::read_rnd_buff_size);
#ifdef HAVE_REPLICATION
sys_var_bool_ptr	sys_relay_log_purge("relay_log_purge",
                                            &relay_log_purge);
#endif
sys_var_long_ptr	sys_rpl_recovery_rank("rpl_recovery_rank",
					      &rpl_recovery_rank);
sys_var_long_ptr	sys_query_cache_size("query_cache_size",
					     &query_cache_size,
					     fix_query_cache_size);

sys_var_thd_ulong	sys_range_alloc_block_size("range_alloc_block_size",
						   &SV::range_alloc_block_size);
sys_var_thd_ulong	sys_query_alloc_block_size("query_alloc_block_size",
						   &SV::query_alloc_block_size);
sys_var_thd_ulong	sys_query_prealloc_size("query_prealloc_size",
						&SV::query_prealloc_size);
sys_var_thd_ulong	sys_trans_alloc_block_size("transaction_alloc_block_size",
						   &SV::trans_alloc_block_size);
sys_var_thd_ulong	sys_trans_prealloc_size("transaction_prealloc_size",
						&SV::trans_prealloc_size);

#ifdef HAVE_QUERY_CACHE
sys_var_long_ptr	sys_query_cache_limit("query_cache_limit",
					      &query_cache.query_cache_limit);
sys_var_long_ptr        sys_query_cache_min_res_unit("query_cache_min_res_unit",
						     &query_cache_min_res_unit,
						     fix_query_cache_min_res_unit);
sys_var_thd_enum	sys_query_cache_type("query_cache_type",
					     &SV::query_cache_type,
					     &query_cache_type_typelib);
#endif /* HAVE_QUERY_CACHE */
sys_var_bool_ptr	sys_secure_auth("secure_auth", &opt_secure_auth);
sys_var_long_ptr	sys_server_id("server_id",&server_id);
sys_var_bool_ptr	sys_slave_compressed_protocol("slave_compressed_protocol",
						      &opt_slave_compressed_protocol);
#ifdef HAVE_REPLICATION
sys_var_long_ptr	sys_slave_net_timeout("slave_net_timeout",
					      &slave_net_timeout);
#endif
sys_var_long_ptr	sys_slow_launch_time("slow_launch_time",
					     &slow_launch_time);
sys_var_thd_ulong	sys_sort_buffer("sort_buffer_size",
					&SV::sortbuff_size);
sys_var_thd_sql_mode    sys_sql_mode("sql_mode",
                                     &SV::sql_mode);
sys_var_thd_enum        sys_table_type("table_type", &SV::table_type,
                                       &ha_table_typelib);
sys_var_long_ptr	sys_table_cache_size("table_cache",
					     &table_cache_size);
sys_var_long_ptr	sys_thread_cache_size("thread_cache_size",
					      &thread_cache_size);
sys_var_thd_enum	sys_tx_isolation("tx_isolation",
					 &SV::tx_isolation,
					 &tx_isolation_typelib,
					 fix_tx_isolation);
sys_var_thd_ulong	sys_tmp_table_size("tmp_table_size",
					   &SV::tmp_table_size);
sys_var_thd_ulong	sys_net_wait_timeout("wait_timeout",
					     &SV::net_wait_timeout);

#ifdef HAVE_INNOBASE_DB
sys_var_long_ptr        sys_innodb_max_dirty_pages_pct("innodb_max_dirty_pages_pct",
                                                        &srv_max_buf_pool_modified_pct);
#endif 					     

/* Time/date/datetime formats */

sys_var_thd_date_time_format sys_time_format("time_format",
					     &SV::time_format,
					     TIMESTAMP_TIME);
sys_var_thd_date_time_format sys_date_format("date_format",
					     &SV::date_format,
					     TIMESTAMP_DATE);
sys_var_thd_date_time_format sys_datetime_format("datetime_format",
						 &SV::datetime_format,
						 TIMESTAMP_DATETIME);

/* Variables that are bits in THD */

static sys_var_thd_bit	sys_autocommit("autocommit",
				       set_option_autocommit,
				       OPTION_NOT_AUTOCOMMIT,
				       1);
static sys_var_thd_bit	sys_big_tables("big_tables",
				       set_option_bit,
				       OPTION_BIG_TABLES);
#ifndef TO_BE_DELETED	/* Alias for big_tables */
static sys_var_thd_bit	sys_sql_big_tables("sql_big_tables",
					   set_option_bit,
					   OPTION_BIG_TABLES);
#endif
static sys_var_thd_bit	sys_big_selects("sql_big_selects",
					set_option_bit,
					OPTION_BIG_SELECTS);
static sys_var_thd_bit	sys_log_off("sql_log_off",
				    set_option_bit,
				    OPTION_LOG_OFF);
static sys_var_thd_bit	sys_log_update("sql_log_update",
				       set_log_update,
				       OPTION_UPDATE_LOG);
static sys_var_thd_bit	sys_log_binlog("sql_log_bin",
					set_log_update,
					OPTION_BIN_LOG);
static sys_var_thd_bit	sys_sql_warnings("sql_warnings",
					 set_option_bit,
					 OPTION_WARNINGS);
static sys_var_thd_bit	sys_auto_is_null("sql_auto_is_null",
					 set_option_bit,
					 OPTION_AUTO_IS_NULL);
static sys_var_thd_bit	sys_safe_updates("sql_safe_updates",
					 set_option_bit,
					 OPTION_SAFE_UPDATES);
static sys_var_thd_bit	sys_buffer_results("sql_buffer_result",
					   set_option_bit,
					   OPTION_BUFFER_RESULT);
static sys_var_thd_bit	sys_quote_show_create("sql_quote_show_create",
					      set_option_bit,
					      OPTION_QUOTE_SHOW_CREATE);
static sys_var_thd_bit	sys_foreign_key_checks("foreign_key_checks",
					       set_option_bit,
					       OPTION_NO_FOREIGN_KEY_CHECKS,
					       1);
static sys_var_thd_bit	sys_unique_checks("unique_checks",
					  set_option_bit,
					  OPTION_RELAXED_UNIQUE_CHECKS,
					  1);

/* Local state variables */

static sys_var_thd_ha_rows	sys_select_limit("sql_select_limit",
						 &SV::select_limit);
static sys_var_timestamp	sys_timestamp("timestamp");
static sys_var_last_insert_id	sys_last_insert_id("last_insert_id");
static sys_var_last_insert_id	sys_identity("identity");
static sys_var_insert_id	sys_insert_id("insert_id");
static sys_var_readonly		sys_error_count("error_count",
						OPT_SESSION,
						SHOW_LONG,
						get_error_count);
static sys_var_readonly		sys_warning_count("warning_count",
						  OPT_SESSION,
						  SHOW_LONG,
						  get_warning_count);

/* alias for last_insert_id() to be compatible with Sybase */
#ifdef HAVE_REPLICATION
static sys_var_slave_skip_counter sys_slave_skip_counter("sql_slave_skip_counter");
#endif
static sys_var_rand_seed1	sys_rand_seed1("rand_seed1");
static sys_var_rand_seed2	sys_rand_seed2("rand_seed2");

static sys_var_thd_ulong        sys_default_week_format("default_week_format",
					                &SV::default_week_format);

sys_var_thd_ulong               sys_group_concat_max_len("group_concat_max_len",
                                                         &SV::group_concat_max_len);

/*
  List of all variables for initialisation and storage in hash
  This is sorted in alphabetical order to make it easy to add new variables

  If the variable is not in this list, it can't be changed with
  SET variable_name=
*/

sys_var *sys_variables[]=
{
  &sys_auto_is_null,
  &sys_autocommit,
  &sys_big_tables,
  &sys_big_selects,
  &sys_binlog_cache_size,
  &sys_buffer_results,
  &sys_bulk_insert_buff_size,
  &sys_character_set_server,
  &sys_character_set_database,
  &sys_character_set_client,
  &sys_character_set_connection,
  &sys_character_set_results,
  &sys_collation_connection,
  &sys_collation_database,
  &sys_collation_server,
  &sys_concurrent_insert,
  &sys_connect_timeout,
  &sys_date_format,
  &sys_datetime_format,
  &sys_default_week_format,
  &sys_delay_key_write,
  &sys_delayed_insert_limit,
  &sys_delayed_insert_timeout,
  &sys_delayed_queue_size,
  &sys_error_count,
  &sys_expire_logs_days,
  &sys_flush,
  &sys_flush_time,
  &sys_foreign_key_checks,
  &sys_group_concat_max_len,
  &sys_identity,
  &sys_insert_id,
  &sys_interactive_timeout,
  &sys_join_buffer_size,
  &sys_key_buffer_size,
  &sys_last_insert_id,
  &sys_local_infile,
  &sys_log_binlog,
  &sys_log_off,
  &sys_log_update,
  &sys_log_warnings,
  &sys_long_query_time,
  &sys_low_priority_updates,
  &sys_max_allowed_packet,
  &sys_max_binlog_cache_size,
  &sys_max_binlog_size,
  &sys_max_connect_errors,
  &sys_max_connections,
  &sys_max_delayed_threads,
  &sys_max_error_count,
  &sys_max_heap_table_size,
  &sys_max_join_size,
  &sys_max_length_for_sort_data,
  &sys_max_prep_stmt_count,
  &sys_max_relay_log_size,
  &sys_max_seeks_for_key,
  &sys_max_sort_length,
  &sys_max_tmp_tables,
  &sys_max_user_connections,
  &sys_max_write_lock_count,
  &sys_myisam_max_extra_sort_file_size,
  &sys_myisam_max_sort_file_size,
  &sys_myisam_repair_threads,
  &sys_myisam_sort_buffer_size,
  &sys_net_buffer_length,
  &sys_net_read_timeout,
  &sys_net_retry_count,
  &sys_net_wait_timeout,
  &sys_net_write_timeout,
  &sys_new_mode,
  &sys_old_passwords,
  &sys_preload_buff_size,
  &sys_pseudo_thread_id,
  &sys_query_alloc_block_size,
  &sys_query_cache_size,
  &sys_query_prealloc_size,
#ifdef HAVE_QUERY_CACHE
  &sys_query_cache_limit,
  &sys_query_cache_min_res_unit,
  &sys_query_cache_type,
#endif /* HAVE_QUERY_CACHE */
  &sys_quote_show_create,
  &sys_rand_seed1,
  &sys_rand_seed2,
  &sys_range_alloc_block_size,
  &sys_readonly,
  &sys_read_buff_size,
  &sys_read_rnd_buff_size,
#ifdef HAVE_REPLICATION
  &sys_relay_log_purge,
#endif
  &sys_rpl_recovery_rank,
  &sys_safe_updates,
  &sys_secure_auth,
  &sys_select_limit,
  &sys_server_id,
#ifdef HAVE_REPLICATION
  &sys_slave_compressed_protocol,
  &sys_slave_net_timeout,
  &sys_slave_skip_counter,
#endif
  &sys_slow_launch_time,
  &sys_sort_buffer,
  &sys_sql_big_tables,
  &sys_sql_low_priority_updates,
  &sys_sql_max_join_size,
  &sys_sql_mode,
  &sys_sql_warnings,
  &sys_table_cache_size,
  &sys_table_type,
  &sys_thread_cache_size,
  &sys_time_format,
  &sys_timestamp,
  &sys_tmp_table_size,
  &sys_trans_alloc_block_size,
  &sys_trans_prealloc_size,
  &sys_tx_isolation,
#ifdef HAVE_INNOBASE_DB
  &sys_innodb_max_dirty_pages_pct,
#endif    
  &sys_unique_checks,
  &sys_warning_count
};


/*
  Variables shown by SHOW variables in alphabetical order
*/

struct show_var_st init_vars[]= {
  {"back_log",                (char*) &back_log,                    SHOW_LONG},
  {"basedir",                 mysql_home,                           SHOW_CHAR},
#ifdef HAVE_BERKELEY_DB
  {"bdb_cache_size",          (char*) &berkeley_cache_size,         SHOW_LONG},
  {"bdb_log_buffer_size",     (char*) &berkeley_log_buffer_size,    SHOW_LONG},
  {"bdb_home",                (char*) &berkeley_home,               SHOW_CHAR_PTR},
  {"bdb_max_lock",            (char*) &berkeley_max_lock,	    SHOW_LONG},
  {"bdb_logdir",              (char*) &berkeley_logdir,             SHOW_CHAR_PTR},
  {"bdb_shared_data",	      (char*) &berkeley_shared_data,	    SHOW_BOOL},
  {"bdb_tmpdir",              (char*) &berkeley_tmpdir,             SHOW_CHAR_PTR},
  {"bdb_version",             (char*) DB_VERSION_STRING,            SHOW_CHAR},
#endif
  {sys_binlog_cache_size.name,(char*) &sys_binlog_cache_size,	    SHOW_SYS},
  {sys_bulk_insert_buff_size.name,(char*) &sys_bulk_insert_buff_size,SHOW_SYS},
  {sys_character_set_server.name, (char*) &sys_character_set_server,SHOW_SYS},
  {sys_charset_system.name,   (char*) &sys_charset_system,          SHOW_SYS},
  {sys_character_set_database.name, (char*) &sys_character_set_database,SHOW_SYS},
  {sys_character_set_client.name,(char*) &sys_character_set_client, SHOW_SYS},
  {sys_character_set_connection.name,(char*) &sys_character_set_connection,SHOW_SYS},
  {"character-sets-dir",      mysql_charsets_dir,                   SHOW_CHAR},
  {sys_character_set_results.name,(char*) &sys_character_set_results, SHOW_SYS},
  {sys_collation_connection.name,(char*) &sys_collation_connection, SHOW_SYS},
  {sys_collation_database.name,(char*) &sys_collation_database,     SHOW_SYS},
  {sys_collation_server.name,(char*) &sys_collation_server,         SHOW_SYS},
  {sys_concurrent_insert.name,(char*) &sys_concurrent_insert,       SHOW_SYS},
  {sys_connect_timeout.name,  (char*) &sys_connect_timeout,         SHOW_SYS},
  {"datadir",                 mysql_real_data_home,                 SHOW_CHAR},
  {sys_date_format.name,      (char*) &sys_date_format,		    SHOW_SYS},
  {sys_datetime_format.name,  (char*) &sys_datetime_format,	    SHOW_SYS},
  {sys_default_week_format.name, (char*) &sys_default_week_format,  SHOW_SYS},
  {sys_delay_key_write.name,  (char*) &sys_delay_key_write,         SHOW_SYS},
  {sys_delayed_insert_limit.name, (char*) &sys_delayed_insert_limit,SHOW_SYS},
  {sys_delayed_insert_timeout.name, (char*) &sys_delayed_insert_timeout, SHOW_SYS},
  {sys_delayed_queue_size.name,(char*) &sys_delayed_queue_size,     SHOW_SYS},
  {sys_expire_logs_days.name, (char*) &sys_expire_logs_days,        SHOW_SYS},
  {sys_flush.name,             (char*) &sys_flush,                  SHOW_SYS},
  {sys_flush_time.name,        (char*) &sys_flush_time,             SHOW_SYS},
  {"ft_boolean_syntax",       (char*) ft_boolean_syntax,	    SHOW_CHAR},
  {"ft_min_word_len",         (char*) &ft_min_word_len,             SHOW_LONG},
  {"ft_max_word_len",         (char*) &ft_max_word_len,             SHOW_LONG},
  {"ft_max_word_len_for_sort",(char*) &ft_max_word_len_for_sort,    SHOW_LONG},
  {"ft_stopword_file",        (char*) &ft_stopword_file,            SHOW_CHAR_PTR},
  {"have_bdb",		      (char*) &have_berkeley_db,	    SHOW_HAVE},
  {"have_crypt",	      (char*) &have_crypt,		    SHOW_HAVE},
  {"have_compress",	      (char*) &have_compress,		    SHOW_HAVE},
  {"have_innodb",	      (char*) &have_innodb,		    SHOW_HAVE},
  {"have_isam",	      	      (char*) &have_isam,		    SHOW_HAVE},
  {"have_raid",		      (char*) &have_raid,		    SHOW_HAVE},
  {"have_symlink",            (char*) &have_symlink,         	    SHOW_HAVE},
  {"have_openssl",	      (char*) &have_openssl,		    SHOW_HAVE},
  {"have_query_cache",        (char*) &have_query_cache,            SHOW_HAVE},
  {"init_file",               (char*) &opt_init_file,               SHOW_CHAR_PTR},
#ifdef HAVE_INNOBASE_DB
  {"innodb_additional_mem_pool_size", (char*) &innobase_additional_mem_pool_size, SHOW_LONG },
  {"innodb_buffer_pool_size", (char*) &innobase_buffer_pool_size, SHOW_LONG },
  {"innodb_buffer_pool_awe_mem_mb", (char*) &innobase_buffer_pool_awe_mem_mb, SHOW_LONG },
  {"innodb_data_file_path", (char*) &innobase_data_file_path,	    SHOW_CHAR_PTR},
  {"innodb_data_home_dir",  (char*) &innobase_data_home_dir,	    SHOW_CHAR_PTR},
  {"innodb_file_io_threads", (char*) &innobase_file_io_threads, SHOW_LONG },
  {"innodb_open_files", (char*) &innobase_open_files, SHOW_LONG },
  {"innodb_force_recovery", (char*) &innobase_force_recovery, SHOW_LONG },
  {"innodb_thread_concurrency", (char*) &innobase_thread_concurrency, SHOW_LONG },
  {"innodb_flush_log_at_trx_commit", (char*) &innobase_flush_log_at_trx_commit, SHOW_INT},
  {"innodb_fast_shutdown", (char*) &innobase_fast_shutdown, SHOW_MY_BOOL},
  {"innodb_file_per_table", (char*) &innobase_file_per_table, SHOW_MY_BOOL},
  {"innodb_flush_method",    (char*) &innobase_unix_file_flush_method, SHOW_CHAR_PTR},
  {"innodb_lock_wait_timeout", (char*) &innobase_lock_wait_timeout, SHOW_LONG },
  {"innodb_log_arch_dir",   (char*) &innobase_log_arch_dir, 	    SHOW_CHAR_PTR},
  {"innodb_log_archive",    (char*) &innobase_log_archive, 	    SHOW_MY_BOOL},
  {"innodb_log_buffer_size", (char*) &innobase_log_buffer_size, SHOW_LONG },
  {"innodb_log_file_size", (char*) &innobase_log_file_size, SHOW_LONG},
  {"innodb_log_files_in_group", (char*) &innobase_log_files_in_group,	SHOW_LONG},
  {"innodb_log_group_home_dir", (char*) &innobase_log_group_home_dir, SHOW_CHAR_PTR},
  {"innodb_mirrored_log_groups", (char*) &innobase_mirrored_log_groups, SHOW_LONG},
  {sys_innodb_max_dirty_pages_pct.name, (char*) &sys_innodb_max_dirty_pages_pct, SHOW_SYS},
#endif
  {sys_interactive_timeout.name,(char*) &sys_interactive_timeout,   SHOW_SYS},
  {sys_join_buffer_size.name,   (char*) &sys_join_buffer_size,	    SHOW_SYS},
  {sys_key_buffer_size.name,	(char*) &sys_key_buffer_size,	    SHOW_SYS},
  {"language",                language,                             SHOW_CHAR},
  {"large_files_support",     (char*) &opt_large_files,             SHOW_BOOL},	
  {sys_local_infile.name,     (char*) &sys_local_infile,	    SHOW_SYS},
#ifdef HAVE_MLOCKALL
  {"locked_in_memory",	      (char*) &locked_in_memory,	    SHOW_BOOL},
#endif
  {"log",                     (char*) &opt_log,                     SHOW_BOOL},
  {"log_update",              (char*) &opt_update_log,              SHOW_BOOL},
  {"log_bin",                 (char*) &opt_bin_log,                 SHOW_BOOL},
#ifdef HAVE_REPLICATION
  {"log_slave_updates",       (char*) &opt_log_slave_updates,       SHOW_MY_BOOL},
#endif
  {"log_slow_queries",        (char*) &opt_slow_log,                SHOW_BOOL},
  {sys_log_warnings.name,     (char*) &sys_log_warnings,	    SHOW_SYS},
  {sys_long_query_time.name,  (char*) &sys_long_query_time, 	    SHOW_SYS},
  {sys_low_priority_updates.name, (char*) &sys_low_priority_updates, SHOW_SYS},
  {"lower_case_table_names",  (char*) &lower_case_table_names,      SHOW_MY_BOOL},
  {sys_max_allowed_packet.name,(char*) &sys_max_allowed_packet,	    SHOW_SYS},
  {sys_max_binlog_cache_size.name,(char*) &sys_max_binlog_cache_size, SHOW_SYS},
  {sys_max_binlog_size.name,    (char*) &sys_max_binlog_size,	    SHOW_SYS},
  {sys_max_connections.name,    (char*) &sys_max_connections,	    SHOW_SYS},
  {sys_max_connect_errors.name, (char*) &sys_max_connect_errors,    SHOW_SYS},
  {sys_max_error_count.name,	(char*) &sys_max_error_count,	    SHOW_SYS},
  {sys_max_delayed_threads.name,(char*) &sys_max_delayed_threads,   SHOW_SYS},
  {sys_max_heap_table_size.name,(char*) &sys_max_heap_table_size,   SHOW_SYS},
  {sys_max_join_size.name,	(char*) &sys_max_join_size,	    SHOW_SYS},
  {sys_max_relay_log_size.name, (char*) &sys_max_relay_log_size,    SHOW_SYS},
  {sys_max_seeks_for_key.name,  (char*) &sys_max_seeks_for_key,	    SHOW_SYS},
  {sys_max_length_for_sort_data.name, (char*) &sys_max_length_for_sort_data,
   SHOW_SYS},
  {sys_max_prep_stmt_count.name,(char*) &sys_max_prep_stmt_count,   SHOW_SYS},
  {sys_max_sort_length.name,	(char*) &sys_max_sort_length,	    SHOW_SYS},
  {sys_max_user_connections.name,(char*) &sys_max_user_connections, SHOW_SYS},
  {sys_max_tmp_tables.name,	(char*) &sys_max_tmp_tables,	    SHOW_SYS},
  {sys_max_write_lock_count.name, (char*) &sys_max_write_lock_count,SHOW_SYS},
  {sys_myisam_max_extra_sort_file_size.name,
   (char*) &sys_myisam_max_extra_sort_file_size,
   SHOW_SYS},
  {sys_myisam_max_sort_file_size.name, (char*) &sys_myisam_max_sort_file_size,
   SHOW_SYS},
  {sys_myisam_repair_threads.name, (char*) &sys_myisam_repair_threads,
   SHOW_SYS},
  {"myisam_recover_options",  (char*) &myisam_recover_options_str,  SHOW_CHAR_PTR},
  {sys_myisam_sort_buffer_size.name, (char*) &sys_myisam_sort_buffer_size, SHOW_SYS},
#ifdef __NT__
  {"named_pipe",	      (char*) &opt_enable_named_pipe,       SHOW_MY_BOOL},
#endif
  {sys_net_buffer_length.name,(char*) &sys_net_buffer_length,       SHOW_SYS},
  {sys_net_read_timeout.name, (char*) &sys_net_read_timeout,        SHOW_SYS},
  {sys_net_retry_count.name,  (char*) &sys_net_retry_count,	    SHOW_SYS},
  {sys_net_write_timeout.name,(char*) &sys_net_write_timeout,       SHOW_SYS},
  {sys_new_mode.name,         (char*) &sys_new_mode,                SHOW_SYS},
  {sys_old_passwords.name,    (char*) &sys_old_passwords,           SHOW_SYS},
  {"open_files_limit",	      (char*) &open_files_limit,	    SHOW_LONG},
  {"pid_file",                (char*) pidfile_name,                 SHOW_CHAR},
  {"log_error",               (char*) log_error_file,               SHOW_CHAR},
  {"port",                    (char*) &mysqld_port,                  SHOW_INT},
  {"protocol_version",        (char*) &protocol_version,            SHOW_INT},
  {sys_preload_buff_size.name, (char*) &sys_preload_buff_size,      SHOW_SYS},
  {sys_pseudo_thread_id.name, (char*) &sys_pseudo_thread_id,        SHOW_SYS},
  {sys_query_alloc_block_size.name, (char*) &sys_query_alloc_block_size,
   SHOW_SYS},
#ifdef HAVE_QUERY_CACHE
  {sys_query_cache_limit.name,(char*) &sys_query_cache_limit,	    SHOW_SYS},
  {sys_query_cache_min_res_unit.name, (char*) &sys_query_cache_min_res_unit,
   SHOW_SYS},
  {sys_query_cache_size.name, (char*) &sys_query_cache_size,	    SHOW_SYS},
  {sys_query_cache_type.name, (char*) &sys_query_cache_type,        SHOW_SYS},
  {"secure_auth",             (char*) &sys_secure_auth,             SHOW_SYS},
#endif /* HAVE_QUERY_CACHE */
  {sys_query_prealloc_size.name, (char*) &sys_query_prealloc_size,  SHOW_SYS},
  {sys_range_alloc_block_size.name, (char*) &sys_range_alloc_block_size,
   SHOW_SYS},
  {sys_read_buff_size.name,   (char*) &sys_read_buff_size,	    SHOW_SYS},
  {sys_readonly.name,         (char*) &sys_readonly,                SHOW_SYS},
  {sys_read_rnd_buff_size.name,(char*) &sys_read_rnd_buff_size,	    SHOW_SYS},
#ifdef HAVE_REPLICATION
  {sys_relay_log_purge.name,  (char*) &sys_relay_log_purge,         SHOW_SYS},
#endif
  {sys_rpl_recovery_rank.name,(char*) &sys_rpl_recovery_rank,       SHOW_SYS},
#ifdef HAVE_SMEM
  {"shared_memory",           (char*) &opt_enable_shared_memory,    SHOW_MY_BOOL},
  {"shared_memory_base_name", (char*) &shared_memory_base_name,     SHOW_CHAR_PTR},
#endif
  {sys_server_id.name,	      (char*) &sys_server_id,		    SHOW_SYS},
#ifdef HAVE_REPLICATION
  {sys_slave_net_timeout.name,(char*) &sys_slave_net_timeout,	    SHOW_SYS},
#endif
  {sys_readonly.name,         (char*) &sys_readonly,                SHOW_SYS},
  {"skip_external_locking",   (char*) &my_disable_locking,          SHOW_MY_BOOL},
  {"skip_networking",         (char*) &opt_disable_networking,      SHOW_BOOL},
  {"skip_show_database",      (char*) &opt_skip_show_db,            SHOW_BOOL},
  {sys_slow_launch_time.name, (char*) &sys_slow_launch_time,        SHOW_SYS},
#ifdef HAVE_SYS_UN_H
  {"socket",                  (char*) &mysqld_unix_port,             SHOW_CHAR_PTR},
#endif
  {sys_sort_buffer.name,      (char*) &sys_sort_buffer, 	    SHOW_SYS},
  {sys_sql_mode.name,         (char*) &sys_sql_mode,                SHOW_SYS},
  {"table_cache",             (char*) &table_cache_size,            SHOW_LONG},
  {sys_table_type.name,	      (char*) &sys_table_type,	            SHOW_SYS},
  {sys_thread_cache_size.name,(char*) &sys_thread_cache_size,       SHOW_SYS},
#ifdef HAVE_THR_SETCONCURRENCY
  {"thread_concurrency",      (char*) &concurrency,                 SHOW_LONG},
#endif
  {"thread_stack",            (char*) &thread_stack,                SHOW_LONG},
  {sys_tx_isolation.name,     (char*) &sys_tx_isolation,	    SHOW_SYS},
  {sys_time_format.name,      (char*) &sys_time_format,		    SHOW_SYS},
#ifdef HAVE_TZNAME
  {"timezone",                time_zone,                            SHOW_CHAR},
#endif
  {sys_tmp_table_size.name,   (char*) &sys_tmp_table_size,	    SHOW_SYS},
  {"tmpdir",                  (char*) &opt_mysql_tmpdir,            SHOW_CHAR_PTR},
  {sys_trans_alloc_block_size.name, (char*) &sys_trans_alloc_block_size,
   SHOW_SYS},
  {sys_trans_prealloc_size.name, (char*) &sys_trans_prealloc_size,  SHOW_SYS},
  {"version",                 server_version,                       SHOW_CHAR},
  {sys_net_wait_timeout.name, (char*) &sys_net_wait_timeout,	    SHOW_SYS},
  {NullS, NullS, SHOW_LONG}
};


bool sys_var::check(THD *thd, set_var *var)
{
  var->save_result.ulonglong_value= var->value->val_int();
  return 0;
}

/*
  Functions to check and update variables
*/

/*
  The following 3 functions need to be changed in 4.1 when we allow
  one to change character sets
*/

static bool sys_check_charset(THD *thd, set_var *var)
{
  return 0;
}


static bool sys_update_charset(THD *thd, set_var *var)
{
  return 0;
}


static void sys_set_default_charset(THD *thd, enum_var_type type)
{
}


/*
  If one sets the LOW_PRIORIY UPDATES flag, we also must change the
  used lock type
*/

static void fix_low_priority_updates(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    thd->update_lock_default= (thd->variables.low_priority_updates ?
			       TL_WRITE_LOW_PRIORITY : TL_WRITE);
}


static void
fix_myisam_max_extra_sort_file_size(THD *thd, enum_var_type type)
{
  myisam_max_extra_temp_length=
    (my_off_t) global_system_variables.myisam_max_extra_sort_file_size;
}


static void
fix_myisam_max_sort_file_size(THD *thd, enum_var_type type)
{
  myisam_max_temp_length=
    (my_off_t) global_system_variables.myisam_max_sort_file_size;
}

/*
  Set the OPTION_BIG_SELECTS flag if max_join_size == HA_POS_ERROR
*/

static void fix_max_join_size(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
  {
    if (thd->variables.max_join_size == HA_POS_ERROR)
      thd->options|= OPTION_BIG_SELECTS;
    else
      thd->options&= ~OPTION_BIG_SELECTS;
  }
}


/*
  If one doesn't use the SESSION modifier, the isolation level
  is only active for the next command
*/

static void fix_tx_isolation(THD *thd, enum_var_type type)
{
  if (type == OPT_SESSION)
    thd->session_tx_isolation= ((enum_tx_isolation)
				thd->variables.tx_isolation);
}


/*
  If we are changing the thread variable, we have to copy it to NET too
*/

#ifdef HAVE_REPLICATION
static void fix_net_read_timeout(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    thd->net.read_timeout=thd->variables.net_read_timeout;
}


static void fix_net_write_timeout(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    thd->net.write_timeout=thd->variables.net_write_timeout;
}

static void fix_net_retry_count(THD *thd, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    thd->net.retry_count=thd->variables.net_retry_count;
}
#else /* HAVE_REPLICATION */
static void fix_net_read_timeout(THD *thd __attribute__(unused),
				 enum_var_type type __attribute__(unused))
{}
static void fix_net_write_timeout(THD *thd __attribute__(unused),
				  enum_var_type type __attribute__(unused))
{}
static void fix_net_retry_count(THD *thd __attribute__(unused),
				enum_var_type type __attribute__(unused))
{}
#endif /* HAVE_REPLICATION */


static void fix_query_cache_size(THD *thd, enum_var_type type)
{
#ifdef HAVE_QUERY_CACHE
  query_cache.resize(query_cache_size);
#endif
}


#ifdef HAVE_QUERY_CACHE
static void fix_query_cache_min_res_unit(THD *thd, enum_var_type type)
{
  query_cache_min_res_unit= 
    query_cache.set_min_res_unit(query_cache_min_res_unit);
}
#endif


extern void fix_delay_key_write(THD *thd, enum_var_type type)
{
  switch ((enum_delay_key_write) delay_key_write_options) {
  case DELAY_KEY_WRITE_NONE:
    myisam_delay_key_write=0;
    break;
  case DELAY_KEY_WRITE_ON:
    myisam_delay_key_write=1;
    break;
  case DELAY_KEY_WRITE_ALL:
    myisam_delay_key_write=1;
    ha_open_options|= HA_OPEN_DELAY_KEY_WRITE;
    break;
  }
}

static void fix_max_binlog_size(THD *thd, enum_var_type type)
{
  DBUG_ENTER("fix_max_binlog_size");
  DBUG_PRINT("info",("max_binlog_size=%lu max_relay_log_size=%lu",
                     max_binlog_size, max_relay_log_size));
  mysql_bin_log.set_max_size(max_binlog_size);
#ifdef HAVE_REPLICATION
  if (!max_relay_log_size)
    active_mi->rli.relay_log.set_max_size(max_binlog_size);
#endif
  DBUG_VOID_RETURN;
}

static void fix_max_relay_log_size(THD *thd, enum_var_type type)
{
  DBUG_ENTER("fix_max_relay_log_size");
  DBUG_PRINT("info",("max_binlog_size=%lu max_relay_log_size=%lu",
                     max_binlog_size, max_relay_log_size));
#ifdef HAVE_REPLICATION
  active_mi->rli.relay_log.set_max_size(max_relay_log_size ?
                                        max_relay_log_size: max_binlog_size);
#endif
  DBUG_VOID_RETURN;
}


static void fix_max_connections(THD *thd, enum_var_type type)
{
  resize_thr_alarm(max_connections + max_insert_delayed_threads + 10);
}

bool sys_var_long_ptr::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (option_limits)
    *value= (ulong) getopt_ull_limit_value(tmp, option_limits);
  else
    *value= (ulong) tmp;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return 0;
}


void sys_var_long_ptr::set_default(THD *thd, enum_var_type type)
{
  *value= (ulong) option_limits->def_value;
}


bool sys_var_ulonglong_ptr::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (option_limits)
    *value= (ulonglong) getopt_ull_limit_value(tmp, option_limits);
  else
    *value= (ulonglong) tmp;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return 0;
}


void sys_var_ulonglong_ptr::set_default(THD *thd, enum_var_type type)
{
  pthread_mutex_lock(&LOCK_global_system_variables);
  *value= (ulonglong) option_limits->def_value;
  pthread_mutex_unlock(&LOCK_global_system_variables);
}


bool sys_var_bool_ptr::update(THD *thd, set_var *var)
{
  *value= (my_bool) var->save_result.ulong_value;
  return 0;
}


void sys_var_bool_ptr::set_default(THD *thd, enum_var_type type)
{
  *value= (my_bool) option_limits->def_value;
}


bool sys_var_enum::update(THD *thd, set_var *var)
{
  *value= (uint) var->save_result.ulong_value;
  return 0;
}


byte *sys_var_enum::value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
{
  return (byte*) enum_names->type_names[*value];
}


bool sys_var_thd_ulong::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;

  /* Don't use bigger value than given with --maximum-variable-name=.. */
  if ((ulong) tmp > max_system_variables.*offset)
    tmp= max_system_variables.*offset;

  if (option_limits)
    tmp= (ulong) getopt_ull_limit_value(tmp, option_limits);
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= (ulong) tmp;
  else
    thd->variables.*offset= (ulong) tmp;
  return 0;
}


void sys_var_thd_ulong::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    /* We will not come here if option_limits is not set */
    global_system_variables.*offset= (ulong) option_limits->def_value;
  }
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_ulong::value_ptr(THD *thd, enum_var_type type,
				   LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}


bool sys_var_thd_ha_rows::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;

  /* Don't use bigger value than given with --maximum-variable-name=.. */
  if ((ha_rows) tmp > max_system_variables.*offset)
    tmp= max_system_variables.*offset;

  if (option_limits)
    tmp= (ha_rows) getopt_ull_limit_value(tmp, option_limits);
  if (var->type == OPT_GLOBAL)
  {
    /* Lock is needed to make things safe on 32 bit systems */
    pthread_mutex_lock(&LOCK_global_system_variables);    
    global_system_variables.*offset= (ha_rows) tmp;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= (ha_rows) tmp;
  return 0;
}


void sys_var_thd_ha_rows::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    /* We will not come here if option_limits is not set */
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= (ha_rows) option_limits->def_value;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_ha_rows::value_ptr(THD *thd, enum_var_type type,
				     LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}

bool sys_var_thd_ulonglong::update(THD *thd,  set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;

  if ((ulonglong) tmp > max_system_variables.*offset)
    tmp= max_system_variables.*offset;

  if (option_limits)
    tmp= (ulong) getopt_ull_limit_value(tmp, option_limits);
  if (var->type == OPT_GLOBAL)
  {
    /* Lock is needed to make things safe on 32 bit systems */
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= (ulonglong) tmp;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= (ulonglong) tmp;
  return 0;
}


void sys_var_thd_ulonglong::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= (ulonglong) option_limits->def_value;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_ulonglong::value_ptr(THD *thd, enum_var_type type,
				       LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}


bool sys_var_thd_bool::update(THD *thd,  set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= (my_bool) var->save_result.ulong_value;
  else
    thd->variables.*offset= (my_bool) var->save_result.ulong_value;
  return 0;
}


void sys_var_thd_bool::set_default(THD *thd,  enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= (my_bool) option_limits->def_value;
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_bool::value_ptr(THD *thd, enum_var_type type,
				  LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
    return (byte*) &(global_system_variables.*offset);
  return (byte*) &(thd->variables.*offset);
}


bool sys_var::check_enum(THD *thd, set_var *var, TYPELIB *enum_names)
{
  char buff[80];
  const char *value;
  String str(buff, sizeof(buff), system_charset_info), *res;

  if (var->value->result_type() == STRING_RESULT)
  {
    if (!(res=var->value->val_str(&str)) ||
	((long) (var->save_result.ulong_value=
		 (ulong) find_type(enum_names, res->ptr(),
				   res->length(),1)-1)) < 0)
    {
      value= res ? res->c_ptr() : "NULL";
      goto err;
    }
  }
  else
  {
    ulonglong tmp=var->value->val_int();
    if (tmp >= enum_names->count)
    {
      llstr(tmp,buff);
      value=buff;				// Wrong value is here
      goto err;
    }
    var->save_result.ulong_value= (ulong) tmp;	// Save for update
  }
  return 0;

err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, value);
  return 1;
}


bool sys_var::check_set(THD *thd, set_var *var, TYPELIB *enum_names)
{
  bool not_used;
  char buff[80], *error= 0;
  uint error_len= 0;
  String str(buff, sizeof(buff), system_charset_info), *res;

  if (var->value->result_type() == STRING_RESULT)
  {
    if (!(res= var->value->val_str(&str)))
      goto err;
    var->save_result.ulong_value= ((ulong)
				   find_set(enum_names, res->c_ptr(),
					    res->length(), &error, &error_len,
					    &not_used));
    if (error_len)
    {
      strmake(buff, error, min(sizeof(buff), error_len));
      goto err;
    }
  }
  else
  {
    ulonglong tmp= var->value->val_int();
    if (tmp >= enum_names->count)
    {
      llstr(tmp, buff);
      goto err;
    }
    var->save_result.ulong_value= (ulong) tmp;  // Save for update
  }
  return 0;

err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, buff);
  return 1;
}


/*
  Return an Item for a variable.  Used with @@[global.]variable_name

  If type is not given, return local value if exists, else global

  We have to use netprintf() instead of my_error() here as this is
  called on the parsing stage.

  TODO:
    With prepared statements/stored procedures this has to be fixed
    to create an item that gets the current value at fix_fields() stage.
*/

Item *sys_var::item(THD *thd, enum_var_type var_type, LEX_STRING *base)
{
  if (check_type(var_type))
  {
    if (var_type != OPT_DEFAULT)
    {
      net_printf(thd,
		 var_type == OPT_GLOBAL ? ER_LOCAL_VARIABLE :
		 ER_GLOBAL_VARIABLE, name);
      return 0;
    }
    /* As there was no local variable, return the global value */
    var_type= OPT_GLOBAL;
  }
  switch (type()) {
  case SHOW_LONG:
    return new Item_uint((int32) *(ulong*) value_ptr(thd, var_type, base));
  case SHOW_LONGLONG:
  {
    longlong value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(longlong*) value_ptr(thd, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_int(value);
  }
  case SHOW_HA_ROWS:
  {
    ha_rows value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(ha_rows*) value_ptr(thd, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_int((longlong) value);
  }
  case SHOW_MY_BOOL:
    return new Item_int((int32) *(my_bool*) value_ptr(thd, var_type, base),1);
  case SHOW_CHAR:
  {
    Item_string *tmp;
    pthread_mutex_lock(&LOCK_global_system_variables);
    char *str= (char*) value_ptr(thd, var_type, base);
    tmp= new Item_string(str, strlen(str), system_charset_info);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return tmp;
  }
  default:
    net_printf(thd, ER_VAR_CANT_BE_READ, name);
  }
  return 0;
}


bool sys_var_thd_enum::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= var->save_result.ulong_value;
  else
    thd->variables.*offset= var->save_result.ulong_value;
  return 0;
}


void sys_var_thd_enum::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= (ulong) option_limits->def_value;
  else
    thd->variables.*offset= global_system_variables.*offset;
}


byte *sys_var_thd_enum::value_ptr(THD *thd, enum_var_type type,
				  LEX_STRING *base)
{
  ulong tmp= ((type == OPT_GLOBAL) ?
	      global_system_variables.*offset :
	      thd->variables.*offset);
  return (byte*) enum_names->type_names[tmp];
}


bool sys_var_thd_bit::update(THD *thd, set_var *var)
{
  int res= (*update_func)(thd, var);
  thd->lex.select_lex.options=thd->options;
  return res;
}


byte *sys_var_thd_bit::value_ptr(THD *thd, enum_var_type type,
				 LEX_STRING *base)
{
  /*
    If reverse is 0 (default) return 1 if bit is set.
    If reverse is 1, return 0 if bit is set
  */
  thd->sys_var_tmp.my_bool_value= ((thd->options & bit_flag) ?
				   !reverse : reverse);
  return (byte*) &thd->sys_var_tmp.my_bool_value;
}


/* Update a date_time format variable based on given value */

void sys_var_thd_date_time_format::update2(THD *thd, enum_var_type type,
					   DATE_TIME_FORMAT *new_value)
{
  DATE_TIME_FORMAT *old;
  DBUG_ENTER("sys_var_date_time_format::update2");
  DBUG_DUMP("positions",(char*) new_value->positions,
	    sizeof(new_value->positions));

  if (type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    old= (global_system_variables.*offset);
    (global_system_variables.*offset)= new_value;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
  {
    old= (thd->variables.*offset);
    (thd->variables.*offset)= new_value;
  }
  my_free((char*) old, MYF(MY_ALLOW_ZERO_PTR));
  DBUG_VOID_RETURN;
}


bool sys_var_thd_date_time_format::update(THD *thd, set_var *var)
{
  DATE_TIME_FORMAT *new_value;
  /* We must make a copy of the last value to get it into normal memory */
  new_value= date_time_format_copy((THD*) 0,
				   var->save_result.date_time_format);
  if (!new_value)
    return 1;					// Out of memory
  update2(thd, var->type, new_value);		// Can't fail
  return 0;
}


bool sys_var_thd_date_time_format::check(THD *thd, set_var *var)
{
  char buff[80];
  String str(buff,sizeof(buff), system_charset_info), *res;
  DATE_TIME_FORMAT *format;

  if (!(res=var->value->val_str(&str)))
    res= &my_empty_string;

  if (!(format= date_time_format_make(date_time_type,
				      res->ptr(), res->length())))
  {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, res->c_ptr());
    return 1;
  }
  
  /*
    We must copy result to thread space to not get a memory leak if
    update is aborted
  */
  var->save_result.date_time_format= date_time_format_copy(thd, format);
  my_free((char*) format, MYF(0));
  return var->save_result.date_time_format == 0;
}


void sys_var_thd_date_time_format::set_default(THD *thd, enum_var_type type)
{
  DATE_TIME_FORMAT *res= 0;

  if (type == OPT_GLOBAL)
  {
    const char *format;
    if ((format= opt_date_time_formats[date_time_type]))
      res= date_time_format_make(date_time_type, format, strlen(format));
  }
  else
  {
    /* Make copy with malloc */
    res= date_time_format_copy((THD *) 0, global_system_variables.*offset);
  }

  if (res)					// Should always be true
    update2(thd, type, res);
}


byte *sys_var_thd_date_time_format::value_ptr(THD *thd, enum_var_type type,
					      LEX_STRING *base)
{
  if (type == OPT_GLOBAL)
  {
    char *res;
    /*
      We do a copy here just to be sure things will work even if someone
      is modifying the original string while the copy is accessed
      (Can't happen now in SQL SHOW, but this is a good safety for the future)
    */
    res= thd->strmake((global_system_variables.*offset)->format.str,
		      (global_system_variables.*offset)->format.length);
    return (byte*) res;
  }
  return (byte*) (thd->variables.*offset)->format.str;
}


typedef struct old_names_map_st
{
  const char *old_name;
  const char *new_name;
} my_old_conv;

static my_old_conv old_conv[]= 
{
  {	"cp1251_koi8"		,	"cp1251"	},
  {	"cp1250_latin2"		,	"cp1250"	},
  {	"kam_latin2"		,	"keybcs2"	},
  {	"mac_latin2"		,	"MacRoman"	},
  {	"macce_latin2"		,	"MacCE"		},
  {	"pc2_latin2"		,	"pclatin2"	},
  {	"vga_latin2"		,	"pclatin1"	},
  {	"koi8_cp1251"		,	"koi8r"		},
  {	"win1251ukr_koi8_ukr"	,	"win1251ukr"	},
  {	"koi8_ukr_win1251ukr"	,	"koi8u"		},
  {	NULL			,	NULL		}
};

CHARSET_INFO *get_old_charset_by_name(const char *name)
{
  my_old_conv *conv;
 
  for (conv= old_conv; conv->old_name; conv++)
  {
    if (!my_strcasecmp(&my_charset_latin1, name, conv->old_name))
      return get_charset_by_csname(conv->new_name, MY_CS_PRIMARY, MYF(0));
  }
  return NULL;
}


bool sys_var_collation::check(THD *thd, set_var *var)
{
  CHARSET_INFO *tmp;
  char buff[80];
  String str(buff,sizeof(buff), system_charset_info), *res;

  if (!(res=var->value->val_str(&str)))
    res= &my_empty_string;

  if (!(tmp=get_charset_by_name(res->c_ptr(),MYF(0))))
  {
    my_error(ER_UNKNOWN_COLLATION, MYF(0), res->c_ptr());
    return 1;
  }
  var->save_result.charset= tmp;	// Save for update
  return 0;
}


bool sys_var_character_set::check(THD *thd, set_var *var)
{
  CHARSET_INFO *tmp;
  char buff[80];
  String str(buff,sizeof(buff), system_charset_info), *res;

  if (!(res=var->value->val_str(&str)))
  { 
    if (!nullable)
    {
      my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), "NULL");
      return 1;
    }
    tmp= NULL;
  }
  else if (!(tmp=get_charset_by_csname(res->c_ptr(),MY_CS_PRIMARY,MYF(0))) &&
	   !(tmp=get_old_charset_by_name(res->c_ptr())))
  {
    my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), res->c_ptr());
    return 1;
  }
  var->save_result.charset= tmp;	// Save for update
  return 0;
}


bool sys_var_character_set::update(THD *thd, set_var *var)
{
  ci_ptr(thd,var->type)[0]= var->save_result.charset;
  thd->update_charset();
  return 0;
}


byte *sys_var_character_set::value_ptr(THD *thd, enum_var_type type,
				       LEX_STRING *base)
{
  CHARSET_INFO *cs= ci_ptr(thd,type)[0];
  return cs ? (byte*) cs->csname : (byte*) "NULL";
}


CHARSET_INFO ** sys_var_character_set_connection::ci_ptr(THD *thd,
							 enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &global_system_variables.collation_connection;
  else
    return &thd->variables.collation_connection;
}


void sys_var_character_set_connection::set_default(THD *thd,
						   enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.collation_connection= default_charset_info;
 else
 {
   thd->variables.collation_connection= global_system_variables.collation_connection;
   thd->update_charset();
 }
}


CHARSET_INFO ** sys_var_character_set_client::ci_ptr(THD *thd,
						     enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &global_system_variables.character_set_client;
  else
    return &thd->variables.character_set_client;
}


void sys_var_character_set_client::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.character_set_client= default_charset_info;
 else
 {
   thd->variables.character_set_client= (global_system_variables.
					 character_set_client);
   thd->update_charset();
 }
}


CHARSET_INFO **
sys_var_character_set_results::ci_ptr(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &global_system_variables.character_set_results;
  else
    return &thd->variables.character_set_results;
}


void sys_var_character_set_results::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.character_set_results= default_charset_info;
 else
 {
   thd->variables.character_set_results= (global_system_variables.
					  character_set_results);
   thd->update_charset();
 }
}


CHARSET_INFO **
sys_var_character_set_server::ci_ptr(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &global_system_variables.collation_server;
  else
    return &thd->variables.collation_server;
}


void sys_var_character_set_server::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.collation_server= default_charset_info;
 else
 {
   thd->variables.collation_server= global_system_variables.collation_server;
   thd->update_charset();
 }
}


CHARSET_INFO ** sys_var_character_set_database::ci_ptr(THD *thd,
						       enum_var_type type)
{
  if (type == OPT_GLOBAL)
    return &global_system_variables.collation_database;
  else
    return &thd->variables.collation_database;
}


void sys_var_character_set_database::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
    global_system_variables.collation_database= default_charset_info;
  else
  {
    thd->variables.collation_database= thd->db_charset;
    thd->update_charset();
  }
}


bool sys_var_collation_connection::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.collation_connection= var->save_result.charset;
  else
  {
    thd->variables.collation_connection= var->save_result.charset;
    thd->update_charset();
  }
  return 0;
}


byte *sys_var_collation_connection::value_ptr(THD *thd, enum_var_type type,
					      LEX_STRING *base)
{
  CHARSET_INFO *cs= ((type == OPT_GLOBAL) ?
		  global_system_variables.collation_connection :
		  thd->variables.collation_connection);
  return cs ? (byte*) cs->name : (byte*) "NULL";
}


void sys_var_collation_connection::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.collation_connection= default_charset_info;
 else
 {
   thd->variables.collation_connection= (global_system_variables.
					 collation_connection);
   thd->update_charset();
 }
}

bool sys_var_collation_database::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.collation_database= var->save_result.charset;
  else
  {
    thd->variables.collation_database= var->save_result.charset;
    thd->update_charset();
  }
  return 0;
}


byte *sys_var_collation_database::value_ptr(THD *thd, enum_var_type type,
					      LEX_STRING *base)
{
  CHARSET_INFO *cs= ((type == OPT_GLOBAL) ?
		  global_system_variables.collation_database :
		  thd->variables.collation_database);
  return cs ? (byte*) cs->name : (byte*) "NULL";
}


void sys_var_collation_database::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.collation_database= default_charset_info;
 else
 {
   thd->variables.collation_database= (global_system_variables.
					 collation_database);
   thd->update_charset();
 }
}


bool sys_var_collation_server::update(THD *thd, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.collation_server= var->save_result.charset;
  else
  {
    thd->variables.collation_server= var->save_result.charset;
    thd->update_charset();
  }
  return 0;
}


byte *sys_var_collation_server::value_ptr(THD *thd, enum_var_type type,
					      LEX_STRING *base)
{
  CHARSET_INFO *cs= ((type == OPT_GLOBAL) ?
		  global_system_variables.collation_server :
		  thd->variables.collation_server);
  return cs ? (byte*) cs->name : (byte*) "NULL";
}


void sys_var_collation_server::set_default(THD *thd, enum_var_type type)
{
 if (type == OPT_GLOBAL)
   global_system_variables.collation_server= default_charset_info;
 else
 {
   thd->variables.collation_server= (global_system_variables.
					 collation_server);
   thd->update_charset();
 }
}


bool sys_var_key_buffer_size::update(THD *thd, set_var *var)
{
  ulonglong tmp= var->save_result.ulonglong_value;

  NAMED_LIST *list;
  LEX_STRING *base_name= &var->base;

  if (!base_name->length)
  {
    /* We are using SET KEY_BUFFER_SIZE=# */
    base_name->str= (char*) "default";
    base_name->length= 7;
  }
  KEY_CACHE *key_cache= (KEY_CACHE*) find_named(&key_caches, base_name->str,
						base_name->length, &list);
  if (!key_cache)
  {
    if (!tmp)					// Tried to delete cache
      return 0;					// Ok, nothing to do
    if (!(key_cache= create_key_cache(base_name->str,
				      base_name->length)))
      return 1;
  }
  if (!tmp)					// Zero size means delete
  {
    /* Don't delete the default key cache */
    if (base_name->length != 7 || memcmp(base_name->str, "default", 7))
    {
      /*
	QQ: Here we should move tables that is using the found key cache
	to the default key cache
      */
      delete list;
      my_free((char*) key_cache, MYF(0));
      return 0;
    }
  }

  key_cache->size= (ulonglong) getopt_ull_limit_value(tmp, option_limits);

  /* QQ:  Needs to be updated when we have multiple key caches */
  keybuff_size= key_cache->size;
  ha_resize_key_cache();
  return 0;
}


static ulonglong zero=0;

byte *sys_var_key_buffer_size::value_ptr(THD *thd, enum_var_type type,
					 LEX_STRING *base)
{
  const char *name;
  uint length;
  KEY_CACHE *key_cache;
  NAMED_LIST *not_used;

  if (!base->str)
  {
    name= "default";
    length= 7;
  }
  else
  {
    name=   base->str;
    length= base->length;
  }
  key_cache= (KEY_CACHE*) find_named(&key_caches, name, length, &not_used);
  if (!key_cache)
    return (byte*) &zero;
  return (byte*) &key_cache->size;
}
  


/*****************************************************************************
  Functions to handle SET NAMES and SET CHARACTER SET
*****************************************************************************/

int set_var_collation_client::check(THD *thd)
{
  return 0;
}

int set_var_collation_client::update(THD *thd)
{
  thd->variables.character_set_client= character_set_client;
  thd->variables.character_set_results= character_set_results;
  thd->variables.collation_connection= collation_connection;
  thd->update_charset();
  thd->protocol_simple.init(thd);
  thd->protocol_prep.init(thd);
  return 0;
}

/****************************************************************************/

bool sys_var_timestamp::update(THD *thd,  set_var *var)
{
  thd->set_time((time_t) var->save_result.ulonglong_value);
  return 0;
}


void sys_var_timestamp::set_default(THD *thd, enum_var_type type)
{
  thd->user_time=0;
}


byte *sys_var_timestamp::value_ptr(THD *thd, enum_var_type type,
				   LEX_STRING *base)
{
  thd->sys_var_tmp.long_value= (long) thd->start_time;
  return (byte*) &thd->sys_var_tmp.long_value;
}


bool sys_var_last_insert_id::update(THD *thd, set_var *var)
{
  thd->insert_id(var->save_result.ulonglong_value);
  return 0;
}


byte *sys_var_last_insert_id::value_ptr(THD *thd, enum_var_type type,
					LEX_STRING *base)
{
  thd->sys_var_tmp.long_value= (long) thd->insert_id();
  return (byte*) &thd->last_insert_id;
}


bool sys_var_insert_id::update(THD *thd, set_var *var)
{
  thd->next_insert_id= var->save_result.ulonglong_value;
  return 0;
}


byte *sys_var_insert_id::value_ptr(THD *thd, enum_var_type type,
				   LEX_STRING *base)
{
  return (byte*) &thd->current_insert_id;
}

bool sys_var_pseudo_thread_id::check(THD *thd, set_var *var)
{
  var->save_result.ulonglong_value= var->value->val_int();
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (thd->master_access & SUPER_ACL)
    return 0;
  else
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
    return 1;
  }
#else
  return 0;
#endif
}


#ifdef HAVE_REPLICATION
bool sys_var_slave_skip_counter::check(THD *thd, set_var *var)
{
  int result= 0;
  LOCK_ACTIVE_MI;
  pthread_mutex_lock(&active_mi->rli.run_lock);
  if (active_mi->rli.slave_running)
  {
    my_error(ER_SLAVE_MUST_STOP, MYF(0));
    result=1;
  }
  pthread_mutex_unlock(&active_mi->rli.run_lock);
  UNLOCK_ACTIVE_MI;
  var->save_result.ulong_value= (ulong) var->value->val_int();
  return result;
}


bool sys_var_slave_skip_counter::update(THD *thd, set_var *var)
{
  LOCK_ACTIVE_MI;
  pthread_mutex_lock(&active_mi->rli.run_lock);
  /*
    The following test should normally never be true as we test this
    in the check function;  To be safe against multiple
    SQL_SLAVE_SKIP_COUNTER request, we do the check anyway
  */
  if (!active_mi->rli.slave_running)
  {
    pthread_mutex_lock(&active_mi->rli.data_lock);
    active_mi->rli.slave_skip_counter= var->save_result.ulong_value;
    pthread_mutex_unlock(&active_mi->rli.data_lock);
  }
  pthread_mutex_unlock(&active_mi->rli.run_lock);
  UNLOCK_ACTIVE_MI;
  return 0;
}
#endif /* HAVE_REPLICATION */

bool sys_var_rand_seed1::update(THD *thd, set_var *var)
{
  thd->rand.seed1= (ulong) var->save_result.ulonglong_value;
  return 0;
}

bool sys_var_rand_seed2::update(THD *thd, set_var *var)
{
  thd->rand.seed2= (ulong) var->save_result.ulonglong_value;
  return 0;
}


/*
  Functions to update thd->options bits
*/

static bool set_option_bit(THD *thd, set_var *var)
{
  sys_var_thd_bit *sys_var= ((sys_var_thd_bit*) var->var);
  if ((var->save_result.ulong_value != 0) == sys_var->reverse)
    thd->options&= ~sys_var->bit_flag;
  else
    thd->options|= sys_var->bit_flag;
  return 0;
}


static bool set_option_autocommit(THD *thd, set_var *var)
{
  /* The test is negative as the flag we use is NOT autocommit */

  ulong org_options=thd->options;

  if (var->save_result.ulong_value != 0)
    thd->options&= ~((sys_var_thd_bit*) var->var)->bit_flag;
  else
    thd->options|= ((sys_var_thd_bit*) var->var)->bit_flag;

  if ((org_options ^ thd->options) & OPTION_NOT_AUTOCOMMIT)
  {
    if ((org_options & OPTION_NOT_AUTOCOMMIT))
    {
      /* We changed to auto_commit mode */
      thd->options&= ~(ulong) (OPTION_BEGIN | OPTION_STATUS_NO_TRANS_UPDATE);
      thd->server_status|= SERVER_STATUS_AUTOCOMMIT;
      if (ha_commit(thd))
	return 1;
    }
    else
    {
      thd->options&= ~(ulong) (OPTION_STATUS_NO_TRANS_UPDATE);
      thd->server_status&= ~SERVER_STATUS_AUTOCOMMIT;
    }
  }
  return 0;
}


static bool set_log_update(THD *thd, set_var *var)
{
  if (opt_sql_bin_update)
    ((sys_var_thd_bit*) var->var)->bit_flag|= (OPTION_BIN_LOG |
					       OPTION_UPDATE_LOG);
  set_option_bit(thd, var);
  return 0;
}

static byte *get_warning_count(THD *thd)
{
  thd->sys_var_tmp.long_value=
    (thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_NOTE] +
     thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_WARN]);
  return (byte*) &thd->sys_var_tmp.long_value;
}

static byte *get_error_count(THD *thd)
{
  thd->sys_var_tmp.long_value= 
    thd->warn_count[(uint) MYSQL_ERROR::WARN_LEVEL_ERROR];
  return (byte*) &thd->sys_var_tmp.long_value;
}


/****************************************************************************
  Main handling of variables:
  - Initialisation
  - Searching during parsing
  - Update loop
****************************************************************************/

/*
  Find variable name in option my_getopt structure used for command line args

  SYNOPSIS
    find_option()
    opt		option structure array to search in
    name	variable name

  RETURN VALUES
    0		Error
    ptr		pointer to option structure
*/

static struct my_option *find_option(struct my_option *opt, const char *name) 
{
  uint length=strlen(name);
  for (; opt->name; opt++)
  {
    if (!getopt_compare_strings(opt->name, name, length) &&
	!opt->name[length])
    {
      /*
	Only accept the option if one can set values through it.
	If not, there is no default value or limits in the option.
      */
      return (opt->value) ? opt : 0;
    }
  }
  return 0;
}


/*
  Return variable name and length for hashing of variables
*/

static byte *get_sys_var_length(const sys_var *var, uint *length,
				my_bool first)
{
  *length= var->name_length;
  return (byte*) var->name;
}


/*
  Initialises sys variables and put them in system_variable_hash
*/


void set_var_init()
{
  hash_init(&system_variable_hash, system_charset_info,
	    array_elements(sys_variables),0,0,
	    (hash_get_key) get_sys_var_length,0,0);
  sys_var **var, **end;
  for (var= sys_variables, end= sys_variables+array_elements(sys_variables) ;
       var < end;
       var++)
  {
    (*var)->name_length= strlen((*var)->name);
    (*var)->option_limits= find_option(my_long_options, (*var)->name);
    my_hash_insert(&system_variable_hash, (byte*) *var);
  }
  /*
    Special cases
    Needed because MySQL can't find the limits for a variable it it has
    a different name than the command line option.
    As these variables are deprecated, this code will disappear soon...
  */
  sys_sql_max_join_size.option_limits= sys_max_join_size.option_limits;
}


void set_var_free()
{
  hash_free(&system_variable_hash);
}


/*
  Find a user set-table variable

  SYNOPSIS
    find_sys_var()
    str		Name of system variable to find
    length	Length of variable.  zero means that we should use strlen()
		on the variable

  NOTE
    We have to use net_printf() as this is called during the parsing stage

  RETURN VALUES
    pointer	pointer to variable definitions
    0		Unknown variable (error message is given)
*/

sys_var *find_sys_var(const char *str, uint length)
{
  sys_var *var= (sys_var*) hash_search(&system_variable_hash,
				       (byte*) str,
				       length ? length :
				       strlen(str));
  if (!var)
    net_printf(current_thd, ER_UNKNOWN_SYSTEM_VARIABLE, (char*) str);
  return var;
}


/*
  Execute update of all variables

  SYNOPSIS

  sql_set
    THD		Thread id
    set_var	List of variables to update

  DESCRIPTION
    First run a check of all variables that all updates will go ok.
    If yes, then execute all updates, returning an error if any one failed.

    This should ensure that in all normal cases none all or variables are
    updated

    RETURN VALUE
    0	ok
    1	ERROR, message sent (normally no variables was updated)
    -1  ERROR, message not sent
*/

int sql_set_variables(THD *thd, List<set_var_base> *var_list)
{
  int error= 0;
  List_iterator_fast<set_var_base> it(*var_list);
  DBUG_ENTER("sql_set_variables");

  set_var_base *var;
  while ((var=it++))
  {
    if ((error=var->check(thd)))
      DBUG_RETURN(error);
  }
  if (thd->net.report_error)
    DBUG_RETURN(1);
  it.rewind();
  while ((var=it++))
    error|= var->update(thd);			// Returns 0, -1 or 1
  DBUG_RETURN(error);
}


/*****************************************************************************
  Functions to handle SET mysql_internal_variable=const_expr
*****************************************************************************/

int set_var::check(THD *thd)
{
  if (var->check_type(type))
  {
    my_error(type == OPT_GLOBAL ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE,
	     MYF(0),
	     var->name);
    return -1;
  }
  if ((type == OPT_GLOBAL && check_global_access(thd, SUPER_ACL)))
    return 1;
  /* value is a NULL pointer if we are using SET ... = DEFAULT */
  if (!value)
  {
    if (var->check_default(type))
    {
      my_error(ER_NO_DEFAULT, MYF(0), var->name);
      return -1;
    }
    return 0;
  }

  if (value->fix_fields(thd, 0, &value) || value->check_cols(1))
    return -1;
  if (var->check_update_type(value->result_type()))
  {
    my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), var->name);
    return -1;
  }
  return var->check(thd, this) ? -1 : 0;
}


int set_var::update(THD *thd)
{
  if (!value)
    var->set_default(thd, type);
  else if (var->update(thd, this))
    return -1;				// should never happen
  if (var->after_update)
    (*var->after_update)(thd, type);
  return 0;
}


/*****************************************************************************
  Functions to handle SET @user_variable=const_expr
*****************************************************************************/

int set_var_user::check(THD *thd)
{
  return (user_var_item->fix_fields(thd,0, (Item**) 0) ||
	  user_var_item->check()) ? -1 : 0;
}


int set_var_user::update(THD *thd)
{
  if (user_var_item->update())
  {
    /* Give an error if it's not given already */
    my_error(ER_SET_CONSTANTS_ONLY, MYF(0));
    return -1;
  }
  return 0;
}


/*****************************************************************************
  Functions to handle SET PASSWORD
*****************************************************************************/

int set_var_password::check(THD *thd)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!user->host.str)
    user->host.str= (char*) thd->host_or_ip;
  /* Returns 1 as the function sends error to client */
  return check_change_password(thd, user->host.str, user->user.str) ? 1 : 0;
#else 
  return 0;
#endif
}

int set_var_password::update(THD *thd)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /* Returns 1 as the function sends error to client */
  return (change_password(thd, user->host.str, user->user.str, password) ?
	  1 : 0);
#else
  return 0;
#endif
}

/****************************************************************************
 Functions to handle sql_mode
****************************************************************************/

byte *sys_var_thd_sql_mode::value_ptr(THD *thd, enum_var_type type,
				      LEX_STRING *base)
{
  ulong val;
  char buff[256];
  String tmp(buff, sizeof(buff), &my_charset_latin1);
  my_bool found= 0;

  tmp.length(0);
  val= ((type == OPT_GLOBAL) ? global_system_variables.*offset :
        thd->variables.*offset);
  for (uint i= 0; val; val>>= 1, i++)
  {
    if (val & 1)
    {
      tmp.append(enum_names->type_names[i]);
      tmp.append(',');
    }
  }
  if (tmp.length())
    tmp.length(tmp.length() - 1);
  return (byte*) thd->strmake(tmp.ptr(), tmp.length());
}


void sys_var_thd_sql_mode::set_default(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= 0;
  else
    thd->variables.*offset= global_system_variables.*offset;
}

void fix_sql_mode_var(THD *thd, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.sql_mode=
      fix_sql_mode(global_system_variables.sql_mode);
  else
    thd->variables.sql_mode= fix_sql_mode(thd->variables.sql_mode);
}

/* Map database specific bits to function bits */

ulong fix_sql_mode(ulong sql_mode)
{
  /*
    Note that we dont set 
    MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS | MODE_NO_FIELD_OPTIONS
    to allow one to get full use of MySQL in this mode.
  */

  if (sql_mode & MODE_ANSI)
    sql_mode|= (MODE_REAL_AS_FLOAT | MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE | MODE_ONLY_FULL_GROUP_BY);
  if (sql_mode & MODE_ORACLE)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS);
  if (sql_mode & MODE_MSSQL)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS);
  if (sql_mode & MODE_MSSQL)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS);
  if (sql_mode & MODE_POSTGRESQL)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS);
  if (sql_mode & MODE_DB2)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS);
  if (sql_mode & MODE_DB2)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS);
  if (sql_mode & MODE_MAXDB)
    sql_mode|= (MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
		MODE_IGNORE_SPACE |
		MODE_NO_KEY_OPTIONS | MODE_NO_TABLE_OPTIONS |
		MODE_NO_FIELD_OPTIONS);
  if (sql_mode & MODE_MYSQL40)
    sql_mode|= MODE_NO_FIELD_OPTIONS;
  if (sql_mode & MODE_MYSQL323)
    sql_mode|= MODE_NO_FIELD_OPTIONS;
  return sql_mode;
}


/****************************************************************************
  Named list handling
****************************************************************************/

gptr find_named(I_List<NAMED_LIST> *list, const char *name, uint length,
		NAMED_LIST **found)
{
  I_List_iterator<NAMED_LIST> it(*list);
  NAMED_LIST *element;
  while ((element= it++))
  {
    if (element->cmp(name, length))
    {
      *found= element;
      return element->data;
    }
  }
  return 0;
}


void delete_elements(I_List<NAMED_LIST> *list, void (*free_element)(gptr))
{
  NAMED_LIST *element;
  DBUG_ENTER("delete_elements");
  while ((element= list->get()))
  {
    (*free_element)(element->data);
    delete element;
  }
  DBUG_VOID_RETURN;
}


/* Key cache functions */

static KEY_CACHE *create_key_cache(const char *name, uint length)
{
  KEY_CACHE *key_cache;
  DBUG_PRINT("info",("Creating key cache: %.*s  length: %d", length, name,
		     length));
  if ((key_cache= (KEY_CACHE*) my_malloc(sizeof(KEY_CACHE),
					 MYF(MY_ZEROFILL | MY_WME))))
  {
    if (!new NAMED_LIST(&key_caches, name, length, (gptr) key_cache))
    {
      my_free((char*) key_cache, MYF(0));
      key_cache= 0;
    }
  }
  return key_cache;
}


KEY_CACHE *get_or_create_key_cache(const char *name, uint length)
{
  NAMED_LIST *not_used;
  KEY_CACHE *key_cache= (KEY_CACHE*) find_named(&key_caches, name,
						length, &not_used);
  if (!key_cache)
    key_cache= create_key_cache(name, length);
  return key_cache;
}


void free_key_cache(gptr key_cache)
{
  my_free(key_cache, MYF(0));
}


/****************************************************************************
  Used templates
****************************************************************************/

#ifdef __GNUC__
template class List<set_var_base>;
template class List_iterator_fast<set_var_base>;
template class I_List_iterator<NAMED_LIST>;
#endif
