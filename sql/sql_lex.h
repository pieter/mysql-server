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


/* YACC and LEX Definitions */

/* These may not be declared yet */
class Table_ident;
class sql_exchange;
class LEX_COLUMN;

// The following hack is neaded because mysql_yacc.cc does not define
// YYSTYPE before including this file

#ifdef MYSQL_YACC
#define LEX_YYSTYPE void *
#else
#include "lex_symbol.h"
#include "sql_yacc.h"
#define LEX_YYSTYPE YYSTYPE *
#endif

enum enum_sql_command {
  SQLCOM_SELECT, SQLCOM_CREATE_TABLE, SQLCOM_CREATE_INDEX, SQLCOM_ALTER_TABLE,
  SQLCOM_UPDATE, SQLCOM_INSERT, SQLCOM_INSERT_SELECT,
  SQLCOM_DELETE, SQLCOM_TRUNCATE, SQLCOM_DROP_TABLE, SQLCOM_DROP_INDEX,

  SQLCOM_SHOW_DATABASES, SQLCOM_SHOW_TABLES, SQLCOM_SHOW_FIELDS,
  SQLCOM_SHOW_KEYS, SQLCOM_SHOW_VARIABLES, SQLCOM_SHOW_LOGS, SQLCOM_SHOW_STATUS,
  SQLCOM_SHOW_PROCESSLIST, SQLCOM_SHOW_MASTER_STAT, SQLCOM_SHOW_SLAVE_STAT,
  SQLCOM_SHOW_GRANTS, SQLCOM_SHOW_CREATE,

  SQLCOM_LOAD,SQLCOM_SET_OPTION,SQLCOM_LOCK_TABLES,SQLCOM_UNLOCK_TABLES,
  SQLCOM_GRANT, SQLCOM_CHANGE_DB, SQLCOM_CREATE_DB, SQLCOM_DROP_DB,
  SQLCOM_REPAIR, SQLCOM_REPLACE, SQLCOM_REPLACE_SELECT, 
  SQLCOM_CREATE_FUNCTION, SQLCOM_DROP_FUNCTION,
  SQLCOM_REVOKE,SQLCOM_OPTIMIZE, SQLCOM_CHECK,
  SQLCOM_FLUSH, SQLCOM_KILL,  SQLCOM_ANALYZE,
  SQLCOM_ROLLBACK, SQLCOM_COMMIT, SQLCOM_SLAVE_START, SQLCOM_SLAVE_STOP,
  SQLCOM_BEGIN, SQLCOM_LOAD_MASTER_TABLE, SQLCOM_CHANGE_MASTER,
  SQLCOM_RENAME_TABLE, SQLCOM_BACKUP_TABLE, SQLCOM_RESTORE_TABLE,
  SQLCOM_RESET, SQLCOM_PURGE, SQLCOM_SHOW_BINLOGS,
  SQLCOM_SHOW_OPEN_TABLES, SQLCOM_DO, SQLCOM_EMPTY_QUERY,
  SQLCOM_END
};

enum lex_states { STATE_START, STATE_CHAR, STATE_IDENT,
		  STATE_IDENT_SEP,
		  STATE_IDENT_START,
		  STATE_FOUND_IDENT,
		  STATE_SIGNED_NUMBER,
		  STATE_REAL,
		  STATE_CMP_OP,
		  STATE_LONG_CMP_OP,
		  STATE_STRING,
		  STATE_COMMENT,
		  STATE_END,
		  STATE_OPERATOR_OR_IDENT,
		  STATE_NUMBER_IDENT,
		  STATE_INT_OR_REAL,
		  STATE_REAL_OR_POINT,
		  STATE_BOOL,
		  STATE_EOL,
		  STATE_ESCAPE,
		  STATE_LONG_COMMENT,
		  STATE_END_LONG_COMMENT,
		  STATE_COLON,
		  STATE_SET_VAR,
		  STATE_USER_END,
		  STATE_HOSTNAME,
		  STATE_SKIP,
		  STATE_USER_VARIABLE_DELIMITER
};

typedef List<Item> List_item;

typedef struct st_lex_master_info
{
  char* host, *user, *password,*log_file_name;
  uint port, connect_retry;
  ulonglong pos;
} LEX_MASTER_INFO;

/* The state of the lex parsing. This is saved in the THD struct */

typedef struct st_lex {
  uint	 yylineno,yytoklen;			/* Simulate lex */
  LEX_YYSTYPE yylval;
  uchar *ptr,*tok_start,*tok_end,*end_of_query;
  char *length,*dec,*change,*name;
  char *db,*db1,*table1,*db2,*table2;		/* For outer join using .. */
  char *backup_dir;				/* For RESTORE/BACKUP */
  char* to_log;                                 /* For PURGE MASTER LOGS TO */
  String *wild;
  sql_exchange *exchange;
  ha_rows select_limit,offset_limit;

  List<List_item>     expr_list;
  List<List_item>     when_list;
  List<List_item>     many_values;
  List<key_part_spec> col_list;
  List<Alter_drop>    drop_list;
  List<Alter_column>  alter_list;
  List<String>	      interval_list,use_index,*use_index_ptr,
		      ignore_index, *ignore_index_ptr;
  List<st_lex_user>   users_list;
  List<LEX_COLUMN>    columns;
  List<Key>	      key_list;
  List<create_field>  create_list;
  List<Item>	      item_list,*insert_list,field_list,value_list;
  List<Item_func_match> ftfunc_list;
  SQL_LIST	      order_list,table_list,group_list,proc_list;
  TYPELIB	      *interval;
  create_field	      *last_field;

  Item *where,*having,*default_value;
  CONVERT *convert_set;
  LEX_USER *grant_user;
  gptr yacc_yyss,yacc_yyvs;
  udf_func udf;
  HA_CHECK_OPT   check_opt;			// check/repair options
  HA_CREATE_INFO create_info;
  LEX_MASTER_INFO mi;				// used by CHANGE MASTER
  ulong thread_id,type;
  ulong options;
  ulong gemini_spin_retries;
  enum_sql_command sql_command;
  enum lex_states next_state;
  enum enum_duplicates duplicates;
  enum enum_tx_isolation tx_isolation;
  uint in_sum_expr,grant,grant_tot_col,which_columns;
  thr_lock_type lock_option;
  bool	create_refs,drop_primary,drop_if_exists,local_file;
  bool  in_comment,ignore_space,verbose;

} LEX;


void lex_init(void);
void lex_free(void);
LEX *lex_start(THD *thd, uchar *buf,uint length);
void lex_end(LEX *lex);

extern pthread_key(LEX*,THR_LEX);

extern LEX_STRING tmp_table_alias;

#define current_lex (&current_thd->lex)
