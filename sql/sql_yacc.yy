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

/* sql_yacc.yy */

%{
/* thd is passed as an arg to yyparse(), and subsequently to yylex().
** The type will be void*, so it must be  cast to (THD*) when used.
** Use the YYTHD macro for this.
*/
#define YYPARSE_PARAM yythd
#define YYLEX_PARAM yythd
#define YYTHD ((THD *)yythd)

#define MYSQL_YACC
#define YYINITDEPTH 100
#define YYMAXDEPTH 3200				/* Because of 64K stack */
#define Lex (&(YYTHD->lex))
#define Select Lex->current_select
#include "mysql_priv.h"
#include "slave.h"
#include "sql_acl.h"
#include "lex_symbol.h"
#include "item_create.h"
#include <myisam.h>
#include <myisammrg.h>

extern void yyerror(const char*);
int yylex(void *yylval, void *yythd);

#define yyoverflow(A,B,C,D,E,F) if (my_yyoverflow((B),(D),(int*) (F))) { yyerror((char*) (A)); return 2; }

inline Item *or_or_concat(THD *thd, Item* A, Item* B)
{
  return (thd->variables.sql_mode & MODE_PIPES_AS_CONCAT ?
          (Item*) new Item_func_concat(A,B) : (Item*) new Item_cond_or(A,B));
}

%}
%union {
  int  num;
  ulong ulong_num;
  ulonglong ulonglong_number;
  LEX_STRING lex_str;
  LEX_STRING *lex_str_ptr;
  LEX_SYMBOL symbol;
  Table_ident *table;
  char *simple_string;
  Item *item;
  List<Item> *item_list;
  List<String> *string_list;
  String *string;
  key_part_spec *key_part;
  TABLE_LIST *table_list;
  udf_func *udf;
  LEX_USER *lex_user;
  sys_var *variable;
  Key::Keytype key_type;
  enum ha_key_alg  key_alg;
  enum db_type db_type;
  enum row_type row_type;
  enum ha_rkey_function ha_rkey_mode;
  enum enum_tx_isolation tx_isolation;
  enum Item_cast cast_type;
  enum Item_udftype udf_type;
  CHARSET_INFO *charset;
  thr_lock_type lock_type;
  interval_type interval;
  st_select_lex *select_lex;
  chooser_compare_func_creator boolfunc2creator;
}

%{
bool my_yyoverflow(short **a, YYSTYPE **b,int *yystacksize);
%}

%pure_parser					/* We have threads */

%token	END_OF_INPUT

%token CLOSE_SYM
%token HANDLER_SYM
%token LAST_SYM
%token NEXT_SYM
%token PREV_SYM

%token	DIV_SYM
%token	EQ
%token	EQUAL_SYM
%token	SOUNDS_SYM
%token	GE
%token	GT_SYM
%token	LE
%token	LT
%token	NE
%token	IS
%token	MOD_SYM
%token	SHIFT_LEFT
%token	SHIFT_RIGHT
%token  SET_VAR

%token	ABORT_SYM
%token	ADD
%token	AFTER_SYM
%token	ALTER
%token	ANALYZE_SYM
%token	ANY_SYM
%token	AVG_SYM
%token	BEGIN_SYM
%token	BINLOG_SYM
%token	CHANGE
%token	CLIENT_SYM
%token	COMMENT_SYM
%token	COMMIT_SYM
%token	COUNT_SYM
%token	CREATE
%token	CROSS
%token  CUBE_SYM
%token	DELETE_SYM
%token	DUAL_SYM
%token	DO_SYM
%token	DROP
%token	EVENTS_SYM
%token	EXECUTE_SYM
%token	FLUSH_SYM
%token  HELP_SYM
%token	INSERT
%token	RELAY_THREAD
%token	KILL_SYM
%token	LOAD
%token	LOCKS_SYM
%token	LOCK_SYM
%token	MASTER_SYM
%token	MAX_SYM
%token	MIN_SYM
%token	NONE_SYM
%token	OPTIMIZE
%token	PURGE
%token	REPAIR
%token	REPLICATION
%token	RESET_SYM
%token	ROLLBACK_SYM
%token  ROLLUP_SYM
%token	SELECT_SYM
%token	SHOW
%token	SLAVE
%token	SQL_THREAD
%token	START_SYM
%token	STD_SYM
%token  VARIANCE_SYM
%token	STOP_SYM
%token	SUM_SYM
%token	SUPER_SYM
%token	TRUNCATE_SYM
%token	UNLOCK_SYM
%token	UPDATE_SYM

%token	ACTION
%token	AGGREGATE_SYM
%token	ALL
%token	AND
%token	AS
%token	ASC
%token	AUTO_INC
%token	AVG_ROW_LENGTH
%token	BACKUP_SYM
%token	BERKELEY_DB_SYM
%token	BINARY
%token	BIT_SYM
%token	BOOL_SYM
%token	BOOLEAN_SYM
%token	BOTH
%token	BTREE_SYM
%token	BY
%token	BYTE_SYM
%token	CACHE_SYM
%token	CASCADE
%token	CAST_SYM
%token	CHARSET
%token	CHECKSUM_SYM
%token	CHECK_SYM
%token	COMMITTED_SYM
%token	COLLATE_SYM
%token	COLLATION_SYM
%token	COLUMNS
%token	COLUMN_SYM
%token	CONCURRENT
%token	CONSTRAINT
%token	CONVERT_SYM
%token	DATABASES
%token	DATA_SYM
%token	DEFAULT
%token	DELAYED_SYM
%token	DELAY_KEY_WRITE_SYM
%token	DESC
%token	DESCRIBE
%token	DES_KEY_FILE
%token	DISABLE_SYM
%token	DISTINCT
%token  DUPLICATE_SYM
%token	DYNAMIC_SYM
%token	ENABLE_SYM
%token	ENCLOSED
%token	ESCAPED
%token	DIRECTORY_SYM
%token	ESCAPE_SYM
%token	EXISTS
%token	EXTENDED_SYM
%token	FALSE_SYM
%token	FILE_SYM
%token	FIRST_SYM
%token	FIXED_SYM
%token	FLOAT_NUM
%token	FORCE_SYM
%token	FOREIGN
%token	FROM
%token	FULL
%token	FULLTEXT_SYM
%token	GLOBAL_SYM
%token	GRANT
%token	GRANTS
%token	GREATEST_SYM
%token	GROUP
%token	HAVING
%token	HASH_SYM
%token	HEAP_SYM
%token	HEX_NUM
%token	HIGH_PRIORITY
%token	HOSTS_SYM
%token	IDENT
%token	IGNORE_SYM
%token	INDEX
%token	INDEXES
%token	INFILE
%token	INNER_SYM
%token	INNOBASE_SYM
%token	INTO
%token	IN_SYM
%token	ISOLATION
%token	ISAM_SYM
%token	JOIN_SYM
%token	KEYS
%token	KEY_SYM
%token	LEADING
%token	LEAST_SYM
%token	LEVEL_SYM
%token	LEX_HOSTNAME
%token	LIKE
%token	LINES
%token	LOCAL_SYM
%token	LOG_SYM
%token	LOGS_SYM
%token	LONG_NUM
%token	LONG_SYM
%token	LOW_PRIORITY
%token	MASTER_HOST_SYM
%token	MASTER_USER_SYM
%token	MASTER_LOG_FILE_SYM
%token	MASTER_LOG_POS_SYM
%token	MASTER_PASSWORD_SYM
%token	MASTER_PORT_SYM
%token	MASTER_CONNECT_RETRY_SYM
%token	MASTER_SERVER_ID_SYM
%token	RELAY_LOG_FILE_SYM
%token	RELAY_LOG_POS_SYM
%token	MATCH
%token	MAX_ROWS
%token	MAX_CONNECTIONS_PER_HOUR
%token	MAX_QUERIES_PER_HOUR
%token	MAX_UPDATES_PER_HOUR
%token	MEDIUM_SYM
%token	MERGE_SYM
%token	MEMORY_SYM
%token	MIN_ROWS
%token	MYISAM_SYM
%token	NAMES_SYM
%token	NATIONAL_SYM
%token	NATURAL
%token	NEW_SYM
%token	NCHAR_SYM
%token	NCHAR_STRING
%token	NOT
%token	NO_SYM
%token	NULL_SYM
%token	NUM
%token	OFFSET_SYM
%token	ON
%token	OPEN_SYM
%token	OPTION
%token	OPTIONALLY
%token	OR
%token	OR_OR_CONCAT
%token	ORDER_SYM
%token	OUTER
%token	OUTFILE
%token	DUMPFILE
%token	PACK_KEYS_SYM
%token	PARTIAL
%token	PRIMARY_SYM
%token	PRIVILEGES
%token	PROCESS
%token	PROCESSLIST_SYM
%token	QUERY_SYM
%token	RAID_0_SYM
%token	RAID_STRIPED_SYM
%token	RAID_TYPE
%token	RAID_CHUNKS
%token	RAID_CHUNKSIZE
%token	READ_SYM
%token	REAL_NUM
%token	REFERENCES
%token	REGEXP
%token	RELOAD
%token	RENAME
%token	REPEATABLE_SYM
%token	REQUIRE_SYM
%token	RESOURCES
%token	RESTORE_SYM
%token	RESTRICT
%token	REVOKE
%token	ROWS_SYM
%token	ROW_FORMAT_SYM
%token	ROW_SYM
%token	RTREE_SYM
%token	SET
%token  SEPARATOR_SYM
%token	SERIAL_SYM
%token	SERIALIZABLE_SYM
%token	SESSION_SYM
%token	SIMPLE_SYM
%token	SHUTDOWN
%token	SPATIAL_SYM
%token  SSL_SYM
%token	STARTING
%token	STATUS_SYM
%token	STRAIGHT_JOIN
%token	SUBJECT_SYM
%token	TABLES
%token	TABLE_SYM
%token	TEMPORARY
%token	TERMINATED
%token	TEXT_STRING
%token	TO_SYM
%token	TRAILING
%token	TRANSACTION_SYM
%token	TRUE_SYM
%token	TYPE_SYM
%token  TYPES_SYM
%token	FUNC_ARG0
%token	FUNC_ARG1
%token	FUNC_ARG2
%token	FUNC_ARG3
%token	UDF_RETURNS_SYM
%token	UDF_SONAME_SYM
%token	UDF_SYM
%token	UNCOMMITTED_SYM
%token	UNDERSCORE_CHARSET
%token	UNICODE_SYM
%token	UNION_SYM
%token	UNIQUE_SYM
%token	USAGE
%token	USE_FRM
%token	USE_SYM
%token	USING
%token	VALUE_SYM
%token	VALUES
%token	VARIABLES
%token	WHERE
%token	WITH
%token	WRITE_SYM
%token  NO_WRITE_TO_BINLOG
%token	X509_SYM
%token	XOR
%token	COMPRESSED_SYM

%token  ERRORS
%token  WARNINGS

%token	ASCII_SYM
%token	BIGINT
%token	BLOB_SYM
%token	CHAR_SYM
%token	CHANGED
%token	COALESCE
%token	DATETIME
%token	DATE_SYM
%token	DECIMAL_SYM
%token	DOUBLE_SYM
%token	ENUM
%token	FAST_SYM
%token	FLOAT_SYM
%token  GEOMETRY_SYM
%token	INT_SYM
%token	LIMIT
%token	LONGBLOB
%token	LONGTEXT
%token	MEDIUMBLOB
%token	MEDIUMINT
%token	MEDIUMTEXT
%token	NUMERIC_SYM
%token	PRECISION
%token	QUICK
%token	REAL
%token	SIGNED_SYM
%token	SMALLINT
%token	STRING_SYM
%token	TEXT_SYM
%token	TIMESTAMP
%token	TIME_SYM
%token	TINYBLOB
%token	TINYINT
%token	TINYTEXT
%token	ULONGLONG_NUM
%token	UNSIGNED
%token	VARBINARY
%token	VARCHAR
%token	VARYING
%token	ZEROFILL

%token	AGAINST
%token	ATAN
%token	BETWEEN_SYM
%token	BIT_AND
%token	BIT_OR
%token	CASE_SYM
%token	CONCAT
%token	CONCAT_WS
%token	CURDATE
%token	CURTIME
%token	DATABASE
%token	DATE_ADD_INTERVAL
%token	DATE_SUB_INTERVAL
%token	DAY_HOUR_SYM
%token	DAY_MINUTE_SYM
%token	DAY_SECOND_SYM
%token	DAY_SYM
%token	DECODE_SYM
%token	DES_ENCRYPT_SYM
%token	DES_DECRYPT_SYM
%token	ELSE
%token	ELT_FUNC
%token	ENCODE_SYM
%token	ENCRYPT
%token	EXPORT_SET
%token	EXTRACT_SYM
%token	FIELD_FUNC
%token	FORMAT_SYM
%token	FOR_SYM
%token	FROM_UNIXTIME
%token	GEOMCOLLFROMTEXT
%token	GEOMFROMTEXT
%token	GEOMFROMWKB
%token  GEOMETRYCOLLECTION
%token  GROUP_CONCAT_SYM
%token	GROUP_UNIQUE_USERS
%token	HOUR_MINUTE_SYM
%token	HOUR_SECOND_SYM
%token	HOUR_SYM
%token	IDENTIFIED_SYM
%token	IF
%token	INSERT_METHOD
%token	INTERVAL_SYM
%token	LAST_INSERT_ID
%token	LEFT
%token	LINEFROMTEXT
%token  LINESTRING
%token	LOCATE
%token	MAKE_SET_SYM
%token	MASTER_POS_WAIT
%token	MINUTE_SECOND_SYM
%token	MINUTE_SYM
%token	MODE_SYM
%token	MODIFY_SYM
%token	MONTH_SYM
%token	MLINEFROMTEXT
%token	MPOINTFROMTEXT
%token	MPOLYFROMTEXT
%token  MULTILINESTRING
%token  MULTIPOINT
%token  MULTIPOLYGON
%token	NOW_SYM
%token	PASSWORD
%token	POINTFROMTEXT
%token	POINT_SYM
%token	POLYFROMTEXT
%token  POLYGON
%token	POSITION_SYM
%token	PROCEDURE
%token	RAND
%token	REPLACE
%token	RIGHT
%token	ROUND
%token	SECOND_SYM
%token	SHARE_SYM
%token	SUBSTRING
%token	SUBSTRING_INDEX
%token	TRIM
%token	UDA_CHAR_SUM
%token	UDA_FLOAT_SUM
%token	UDA_INT_SUM
%token	UDF_CHAR_FUNC
%token	UDF_FLOAT_FUNC
%token	UDF_INT_FUNC
%token	UNIQUE_USERS
%token	UNIX_TIMESTAMP
%token	USER
%token	WEEK_SYM
%token	WHEN_SYM
%token	WORK_SYM
%token	YEAR_MONTH_SYM
%token	YEAR_SYM
%token	YEARWEEK
%token	BENCHMARK_SYM
%token	END
%token	THEN_SYM

%token	SQL_BIG_RESULT
%token	SQL_CACHE_SYM
%token	SQL_CALC_FOUND_ROWS
%token	SQL_NO_CACHE_SYM
%token	SQL_SMALL_RESULT
%token  SQL_BUFFER_RESULT

%token  ISSUER_SYM
%token  SUBJECT_SYM
%token  CIPHER_SYM

%token  HELP
%token  BEFORE_SYM
%left   SET_VAR
%left	OR_OR_CONCAT OR
%left	AND
%left	BETWEEN_SYM CASE_SYM WHEN_SYM THEN_SYM ELSE
%left	EQ EQUAL_SYM GE GT_SYM LE LT NE IS LIKE REGEXP IN_SYM
%left	'|'
%left	'&'
%left	SHIFT_LEFT SHIFT_RIGHT
%left	'-' '+'
%left	'*' '/' '%' DIV_SYM MOD_SYM
%left	NEG '~'
%left   XOR
%left   '^'
%right	NOT
%right	BINARY COLLATE_SYM
/* These don't actually affect the way the query is really evaluated, but
   they silence a few warnings for shift/reduce conflicts. */
%left	','
%left	STRAIGHT_JOIN JOIN_SYM
%nonassoc	CROSS INNER_SYM NATURAL LEFT RIGHT

%type <lex_str>
	IDENT TEXT_STRING REAL_NUM FLOAT_NUM NUM LONG_NUM HEX_NUM LEX_HOSTNAME
	ULONGLONG_NUM field_ident select_alias ident ident_or_text
        UNDERSCORE_CHARSET IDENT_sys TEXT_STRING_sys TEXT_STRING_literal
	NCHAR_STRING

%type <lex_str_ptr>
	opt_table_alias

%type <table>
	table_ident references

%type <simple_string>
	remember_name remember_end opt_ident opt_db text_or_password
	opt_escape

%type <string>
	text_string opt_gconcat_separator

%type <num>
	type int_type real_type order_dir opt_field_spec lock_option
	udf_type if_exists opt_local opt_table_options table_options
	table_option opt_if_not_exists opt_no_write_to_binlog opt_var_type opt_var_ident_type
	delete_option opt_temporary all_or_any opt_distinct

%type <ulong_num>
	ULONG_NUM raid_types merge_insert_types

%type <ulonglong_number>
	ulonglong_num

%type <lock_type>
	replace_lock_option opt_low_priority insert_lock_option load_data_lock

%type <item>
	literal text_literal insert_ident order_ident
	simple_ident select_item2 expr opt_expr opt_else sum_expr in_sum_expr
	table_wild opt_pad no_in_expr expr_expr simple_expr no_and_expr
	using_list expr_or_default set_expr_or_default interval_expr
	param_marker singlerow_subselect singlerow_subselect_init
	exists_subselect exists_subselect_init

%type <item_list>
	expr_list udf_expr_list when_list ident_list ident_list_arg

%type <key_type>
	key_type opt_unique_or_fulltext

%type <key_alg>
	key_alg opt_btree_or_rtree

%type <string_list>
	key_usage_list

%type <key_part>
	key_part

%type <table_list>
	join_table_list  join_table

%type <udf>
	UDF_CHAR_FUNC UDF_FLOAT_FUNC UDF_INT_FUNC
	UDA_CHAR_SUM UDA_FLOAT_SUM UDA_INT_SUM

%type <interval> interval

%type <db_type> table_types

%type <row_type> row_types

%type <tx_isolation> isolation_types

%type <ha_rkey_mode> handler_rkey_mode

%type <cast_type> cast_type

%type <udf_type> udf_func_type

%type <symbol> FUNC_ARG0 FUNC_ARG1 FUNC_ARG2 FUNC_ARG3 keyword

%type <lex_user> user grant_user

%type <charset>
	opt_collate
	charset_name
	charset_name_or_default
	old_or_new_charset_name
	old_or_new_charset_name_or_default
	collation_name
	collation_name_or_default

%type <variable> internal_variable_name

%type <select_lex> in_subselect in_subselect_init

%type <boolfunc2creator> comp_op

%type <NONE>
	query verb_clause create change select do drop insert replace insert2
	insert_values update delete truncate rename
	show describe load alter optimize flush
	reset purge begin commit rollback slave master_def master_defs
	repair restore backup analyze check start
	field_list field_list_item field_spec kill column_def key_def
	select_item_list select_item values_list no_braces
	opt_limit_clause delete_limit_clause fields opt_values values
	procedure_list procedure_list2 procedure_item
	when_list2 expr_list2  handler
	opt_precision opt_ignore opt_column opt_restrict
	grant revoke set lock unlock string_list field_options field_option
	field_opt_list opt_binary table_lock_list table_lock
	ref_list opt_on_delete opt_on_delete_list opt_on_delete_item use
	opt_delete_options opt_delete_option varchar nchar nvarchar
	opt_outer table_list table_name opt_option opt_place
	opt_attribute opt_attribute_list attribute column_list column_list_id
	opt_column_list grant_privileges opt_table user_list grant_option
	grant_privilege grant_privilege_list
	flush_options flush_option
	equal optional_braces opt_key_definition key_usage_list2
	opt_mi_check_type opt_to mi_check_types normal_join
	table_to_table_list table_to_table opt_table_list opt_as
	handler_rkey_function handler_read_or_scan
	single_multi table_wild_list table_wild_one opt_wild
	union_clause union_list union_option
	precision subselect_start opt_and charset
	subselect_end select_var_list select_var_list_init help opt_len
END_OF_INPUT

%type <NONE>
	'-' '+' '*' '/' '%' '(' ')'
	',' '!' '{' '}' '&' '|' AND OR OR_OR_CONCAT BETWEEN_SYM CASE_SYM
	THEN_SYM WHEN_SYM DIV_SYM MOD_SYM
%%


query:
	END_OF_INPUT
	{
	   THD *thd= YYTHD;
	   if (!thd->bootstrap &&
	      (!(thd->lex.select_lex.options & OPTION_FOUND_COMMENT)))
	   {
	     send_error(thd,ER_EMPTY_QUERY);
	     YYABORT;
	   }
	   else
	   {
	     thd->lex.sql_command = SQLCOM_EMPTY_QUERY;
	   }
	}
	| verb_clause END_OF_INPUT {};

verb_clause:
	  alter
	| analyze
	| backup
	| begin
	| change
	| check
	| commit
	| create
	| delete
	| describe
	| do
	| drop
	| grant
	| insert
	| flush
	| load
	| lock
	| kill
	| optimize
	| purge
	| rename
        | repair
	| replace
	| reset
	| restore
	| revoke
	| rollback
	| select
	| set
	| slave
	| start
	| show
	| truncate
	| handler
	| unlock
	| update
	| use
	| help;

/* help */

help:
       HELP_SYM ident_or_text
       {
	  LEX *lex= Lex;
	  lex->sql_command= SQLCOM_HELP;
	  lex->help_arg= $2.str;
       };

/* change master */

change:
       CHANGE MASTER_SYM TO_SYM
        {
	  LEX *lex = Lex;
	  lex->sql_command = SQLCOM_CHANGE_MASTER;
	  bzero((char*) &lex->mi, sizeof(lex->mi));
        }
       master_defs
	{}
       ;

master_defs:
       master_def
       | master_defs ',' master_def;

master_def:
       MASTER_HOST_SYM EQ TEXT_STRING_sys
       {
	 Lex->mi.host = $3.str;
       }
       |
       MASTER_USER_SYM EQ TEXT_STRING_sys
       {
	 Lex->mi.user = $3.str;
       }
       |
       MASTER_PASSWORD_SYM EQ TEXT_STRING_sys
       {
	 Lex->mi.password = $3.str;
       }
       |
       MASTER_LOG_FILE_SYM EQ TEXT_STRING_sys
       {
	 Lex->mi.log_file_name = $3.str;
       }
       |
       MASTER_PORT_SYM EQ ULONG_NUM
       {
	 Lex->mi.port = $3;
       }
       |
       MASTER_LOG_POS_SYM EQ ulonglong_num
       {
	 Lex->mi.pos = $3;
       }
       |
       MASTER_CONNECT_RETRY_SYM EQ ULONG_NUM
       {
	 Lex->mi.connect_retry = $3;
       }
       |
       RELAY_LOG_FILE_SYM EQ TEXT_STRING_sys
       {
	 Lex->mi.relay_log_name = $3.str;
       }
       |
       RELAY_LOG_POS_SYM EQ ULONG_NUM
       {
	 Lex->mi.relay_log_pos = $3;
       }
       ;


/* create a table */

create:
	CREATE opt_table_options TABLE_SYM opt_if_not_exists table_ident
	{
	  THD *thd= YYTHD;
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_CREATE_TABLE;
	  if (!lex->select_lex.add_table_to_list(thd,$5,
						 ($2 &
						  HA_LEX_CREATE_TMP_TABLE ?
						  &tmp_table_alias :
						  (LEX_STRING*) 0),
						 TL_OPTION_UPDATING,
						 ((using_update_log)?
						  TL_READ_NO_INSERT:
						  TL_READ)))
	    YYABORT;
	  lex->create_list.empty();
	  lex->key_list.empty();
	  lex->col_list.empty();
	  lex->change=NullS;
	  bzero((char*) &lex->create_info,sizeof(lex->create_info));
	  lex->create_info.options=$2 | $4;
	  lex->create_info.db_type= (enum db_type) lex->thd->variables.table_type;
	  lex->create_info.table_charset= thd->db_charset;
	  lex->name=0;
	}
	create2
	{}
	| CREATE opt_unique_or_fulltext INDEX ident key_alg ON table_ident
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_CREATE_INDEX;
	    if (!lex->current_select->add_table_to_list(lex->thd, $7, NULL,
							TL_OPTION_UPDATING))
	      YYABORT;
	    lex->create_list.empty();
	    lex->key_list.empty();
	    lex->col_list.empty();
	    lex->change=NullS;
	  }
	   '(' key_list ')'
	  {
	    LEX *lex=Lex;

	    lex->key_list.push_back(new Key($2,$4.str, $5, lex->col_list));
	    lex->col_list.empty();
	  }
	| CREATE DATABASE opt_if_not_exists ident 
	  { Lex->create_info.table_charset=NULL; }
	  opt_create_database_options
	  {
	    LEX *lex=Lex;
	    lex->sql_command=SQLCOM_CREATE_DB;
	    lex->name=$4.str;
            lex->create_info.options=$3;
	  }
	| CREATE udf_func_type UDF_SYM IDENT_sys
	  {
	    LEX *lex=Lex;
	    lex->sql_command = SQLCOM_CREATE_FUNCTION;
	    lex->udf.name = $4;
	    lex->udf.type= $2;
	  }
	  UDF_RETURNS_SYM udf_type UDF_SONAME_SYM TEXT_STRING_sys
	  {
	    LEX *lex=Lex;
	    lex->udf.returns=(Item_result) $7;
	    lex->udf.dl=$9.str;
	  }
          ;

create2:
	'(' field_list ')' opt_create_table_options create3 {}
	| opt_create_table_options create3 {}
	| LIKE table_ident
      	  {
      	    LEX *lex=Lex;
     	    if (!(lex->name= (char *)$2))
              YYABORT;
    	  }
	| '(' LIKE table_ident ')'
      	  {
      	    LEX *lex=Lex;
     	    if (!(lex->name= (char *)$3))
              YYABORT;
    	  }
          ;

create3:
	/* empty */ {}
	| opt_duplicate opt_as SELECT_SYM
          {
	    LEX *lex=Lex;
	    lex->lock_option= (using_update_log) ? TL_READ_NO_INSERT : TL_READ;
	    mysql_init_select(lex);
	    lex->current_select->parsing_place= SELECT_LEX_NODE::SELECT_LIST;
          }
          select_options select_item_list
	  {
	    Select->parsing_place= SELECT_LEX_NODE::NO_MATTER;
	  }
	  opt_select_from union_clause {}
          ;

opt_as:
	/* empty */ {}
	| AS	    {};

opt_create_database_options:
	/* empty */			{}
	| create_database_options	{};

create_database_options:
	create_database_option					{}
	| create_database_options create_database_option	{};

create_database_option:
	  COLLATE_SYM collation_name_or_default	
	  { Lex->create_info.table_charset=$2; }
	| opt_default charset charset_name_or_default
	  { Lex->create_info.table_charset=$3; }
	;

opt_table_options:
	/* empty */	 { $$= 0; }
	| table_options  { $$= $1;};

table_options:
	table_option	{ $$=$1; }
	| table_option table_options { $$= $1 | $2; };

table_option:
	TEMPORARY	{ $$=HA_LEX_CREATE_TMP_TABLE; };

opt_if_not_exists:
	/* empty */	 { $$= 0; }
	| IF NOT EXISTS	 { $$=HA_LEX_CREATE_IF_NOT_EXISTS; };

opt_create_table_options:
	/* empty */
	| create_table_options;

create_table_options_space_separated:
	create_table_option
	| create_table_option create_table_options_space_separated;

create_table_options:
	create_table_option
	| create_table_option     create_table_options
	| create_table_option ',' create_table_options;

create_table_option:
	TYPE_SYM opt_equal table_types          { Lex->create_info.db_type= $3; }
	| MAX_ROWS opt_equal ulonglong_num	{ Lex->create_info.max_rows= $3; Lex->create_info.used_fields|= HA_CREATE_USED_MAX_ROWS;}
	| MIN_ROWS opt_equal ulonglong_num	{ Lex->create_info.min_rows= $3; Lex->create_info.used_fields|= HA_CREATE_USED_MIN_ROWS;}
	| AVG_ROW_LENGTH opt_equal ULONG_NUM	{ Lex->create_info.avg_row_length=$3; Lex->create_info.used_fields|= HA_CREATE_USED_AVG_ROW_LENGTH;}
	| PASSWORD opt_equal TEXT_STRING_sys	{ Lex->create_info.password=$3.str; }
	| COMMENT_SYM opt_equal TEXT_STRING_sys	{ Lex->create_info.comment=$3.str; }
	| AUTO_INC opt_equal ulonglong_num	{ Lex->create_info.auto_increment_value=$3; Lex->create_info.used_fields|= HA_CREATE_USED_AUTO;}
	| PACK_KEYS_SYM opt_equal ULONG_NUM	{ Lex->create_info.table_options|= $3 ? HA_OPTION_PACK_KEYS : HA_OPTION_NO_PACK_KEYS; Lex->create_info.used_fields|= HA_CREATE_USED_PACK_KEYS;}
	| PACK_KEYS_SYM opt_equal DEFAULT	{ Lex->create_info.table_options&= ~(HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS); Lex->create_info.used_fields|= HA_CREATE_USED_PACK_KEYS;}
	| CHECKSUM_SYM opt_equal ULONG_NUM	{ Lex->create_info.table_options|= $3 ? HA_OPTION_CHECKSUM : HA_OPTION_NO_CHECKSUM; }
	| DELAY_KEY_WRITE_SYM opt_equal ULONG_NUM { Lex->create_info.table_options|= $3 ? HA_OPTION_DELAY_KEY_WRITE : HA_OPTION_NO_DELAY_KEY_WRITE; }
	| ROW_FORMAT_SYM opt_equal row_types	{ Lex->create_info.row_type= $3; }
	| RAID_TYPE opt_equal raid_types	{ Lex->create_info.raid_type= $3; Lex->create_info.used_fields|= HA_CREATE_USED_RAID;}
	| RAID_CHUNKS opt_equal ULONG_NUM	{ Lex->create_info.raid_chunks= $3; Lex->create_info.used_fields|= HA_CREATE_USED_RAID;}
	| RAID_CHUNKSIZE opt_equal ULONG_NUM	{ Lex->create_info.raid_chunksize= $3*RAID_BLOCK_SIZE; Lex->create_info.used_fields|= HA_CREATE_USED_RAID;}
	| UNION_SYM opt_equal '(' table_list ')'
	  {
	    /* Move the union list to the merge_list */
	    LEX *lex=Lex;
	    TABLE_LIST *table_list= lex->select_lex.get_table_list();
	    lex->create_info.merge_list= lex->select_lex.table_list;
	    lex->create_info.merge_list.elements--;
	    lex->create_info.merge_list.first= (byte*) (table_list->next);
	    lex->select_lex.table_list.elements=1;
	    lex->select_lex.table_list.next= (byte**) &(table_list->next);
	    table_list->next=0;
	    lex->create_info.used_fields|= HA_CREATE_USED_UNION;
	  }
	| opt_default charset opt_equal charset_name_or_default
	  {
	    Lex->create_info.table_charset= $4;
	    Lex->create_info.used_fields|= HA_CREATE_USED_CHARSET;
	  }
	| COLLATE_SYM opt_equal collation_name_or_default
	  {
	    Lex->create_info.table_charset= $3;
	    Lex->create_info.used_fields|= HA_CREATE_USED_CHARSET;
	  }
	| INSERT_METHOD opt_equal merge_insert_types   { Lex->create_info.merge_insert_method= $3; Lex->create_info.used_fields|= HA_CREATE_USED_INSERT_METHOD;}
	| DATA_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys
	  { Lex->create_info.data_file_name= $4.str; }
	| INDEX DIRECTORY_SYM opt_equal TEXT_STRING_sys { Lex->create_info.index_file_name= $4.str; };

table_types:
	ISAM_SYM	{ $$= DB_TYPE_ISAM; }
	| MYISAM_SYM	{ $$= DB_TYPE_MYISAM; }
	| MERGE_SYM	{ $$= DB_TYPE_MRG_MYISAM; }
	| HEAP_SYM	{ $$= DB_TYPE_HEAP; }
	| MEMORY_SYM	{ $$= DB_TYPE_HEAP; }
	| BERKELEY_DB_SYM { $$= DB_TYPE_BERKELEY_DB; }
	| INNOBASE_SYM  { $$= DB_TYPE_INNODB; };

row_types:
	DEFAULT		{ $$= ROW_TYPE_DEFAULT; }
	| FIXED_SYM	{ $$= ROW_TYPE_FIXED; }
	| DYNAMIC_SYM	{ $$= ROW_TYPE_DYNAMIC; }
	| COMPRESSED_SYM { $$= ROW_TYPE_COMPRESSED; };

raid_types:
	RAID_STRIPED_SYM { $$= RAID_TYPE_0; }
	| RAID_0_SYM	 { $$= RAID_TYPE_0; }
	| ULONG_NUM	 { $$=$1;};

merge_insert_types:
       NO_SYM            { $$= MERGE_INSERT_DISABLED; }
       | FIRST_SYM       { $$= MERGE_INSERT_TO_FIRST; }
       | LAST_SYM        { $$= MERGE_INSERT_TO_LAST; };

opt_select_from:
	opt_limit_clause {}
	| FROM DUAL_SYM {}
	| select_from select_lock_type;

udf_func_type:
	/* empty */ 	{ $$ = UDFTYPE_FUNCTION; }
	| AGGREGATE_SYM { $$ = UDFTYPE_AGGREGATE; };

udf_type:
	STRING_SYM {$$ = (int) STRING_RESULT; }
	| REAL {$$ = (int) REAL_RESULT; }
	| INT_SYM {$$ = (int) INT_RESULT; };

field_list:
	  field_list_item
	| field_list ',' field_list_item;


field_list_item:
	   column_def
         | key_def
         ;

column_def:
	  field_spec check_constraint
	| field_spec references
	  {
	    Lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	;

key_def:
	key_type opt_ident key_alg '(' key_list ')'
	  {
	    LEX *lex=Lex;
	    lex->key_list.push_back(new Key($1,$2, $3, lex->col_list));
	    lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	| opt_constraint FOREIGN KEY_SYM opt_ident '(' key_list ')' references
	  {
	    LEX *lex=Lex;
	    lex->key_list.push_back(new foreign_key($4, lex->col_list,
				    $8,
				    lex->ref_list,
				    lex->fk_delete_opt,
				    lex->fk_update_opt,
				    lex->fk_match_option));
	    lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	| opt_constraint check_constraint
	  {
	    Lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	;

check_constraint:
	/* empty */
	| CHECK_SYM expr
	;

opt_constraint:
	/* empty */
	| CONSTRAINT opt_ident;

field_spec:
	field_ident
	 {
	   LEX *lex=Lex;
	   lex->length=lex->dec=0; lex->type=0; lex->interval=0;
	   lex->default_value=lex->comment=0;
	   lex->charset=NULL;
	 }
	type opt_attribute
	{
	  LEX *lex=Lex;
	  if (add_field_to_list(lex->thd, $1.str,
				(enum enum_field_types) $3,
				lex->length,lex->dec,lex->type,
				lex->default_value, lex->comment,
				lex->change,lex->interval,lex->charset,
				lex->uint_geom_type))
	    YYABORT;
	};

type:
	int_type opt_len field_options	{ $$=$1; }
	| real_type opt_precision field_options { $$=$1; }
	| FLOAT_SYM float_options field_options { $$=FIELD_TYPE_FLOAT; }
	| BIT_SYM opt_len		{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_TINY; }
	| BOOL_SYM			{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_TINY; }
	| BOOLEAN_SYM			{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_TINY; }
	| char '(' NUM ')' opt_binary	{ Lex->length=$3.str;
					  $$=FIELD_TYPE_STRING; }
	| char opt_binary		{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_STRING; }
	| nchar '(' NUM ')'		{ Lex->length=$3.str;
					  $$=FIELD_TYPE_STRING; 
					  Lex->charset=national_charset_info; }
	| nchar				{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_STRING; 
					  Lex->charset=national_charset_info; }
	| BINARY '(' NUM ')'		{ Lex->length=$3.str;
					  Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_STRING; }
	| varchar '(' NUM ')' opt_binary { Lex->length=$3.str;
					  $$=FIELD_TYPE_VAR_STRING; }
	| nvarchar '(' NUM ')'		{ Lex->length=$3.str;
					  $$=FIELD_TYPE_VAR_STRING;
					  Lex->charset=national_charset_info; }
	| VARBINARY '(' NUM ')' 	{ Lex->length=$3.str;
					  Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_VAR_STRING; }
	| YEAR_SYM opt_len field_options { $$=FIELD_TYPE_YEAR; }
	| DATE_SYM			{ $$=FIELD_TYPE_DATE; }
	| TIME_SYM			{ $$=FIELD_TYPE_TIME; }
	| TIMESTAMP
	  {
	    if (YYTHD->variables.sql_mode & MODE_SAPDB)
	      $$=FIELD_TYPE_DATETIME;
	    else
	      $$=FIELD_TYPE_TIMESTAMP;
	   }
	| TIMESTAMP '(' NUM ')'		{ Lex->length=$3.str;
					  $$=FIELD_TYPE_TIMESTAMP; }
	| DATETIME			{ $$=FIELD_TYPE_DATETIME; }
	| TINYBLOB			{ Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_TINY_BLOB; }
	| BLOB_SYM opt_len		{ Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_BLOB; }
	| GEOMETRY_SYM			{ Lex->charset=&my_charset_bin;
					  Lex->uint_geom_type= (uint) Field::GEOM_GEOMETRY;
					  $$=FIELD_TYPE_GEOMETRY; }
	| GEOMETRYCOLLECTION		{ Lex->charset=&my_charset_bin;
					  Lex->uint_geom_type= (uint) Field::GEOM_GEOMETRYCOLLECTION;
					  $$=FIELD_TYPE_GEOMETRY; }
	| POINT_SYM			{ Lex->charset=&my_charset_bin;
					  Lex->uint_geom_type= (uint) Field::GEOM_POINT;
					  $$=FIELD_TYPE_GEOMETRY; }
	| MULTIPOINT			{ Lex->charset=&my_charset_bin;
					  Lex->uint_geom_type= (uint) Field::GEOM_MULTIPOINT;
					  $$=FIELD_TYPE_GEOMETRY; }
	| LINESTRING			{ Lex->charset=&my_charset_bin;
					  Lex->uint_geom_type= (uint) Field::GEOM_LINESTRING;
					  $$=FIELD_TYPE_GEOMETRY; }
	| MULTILINESTRING		{ Lex->charset=&my_charset_bin;
					  Lex->uint_geom_type= (uint) Field::GEOM_MULTILINESTRING;
					  $$=FIELD_TYPE_GEOMETRY; }
	| POLYGON			{ Lex->charset=&my_charset_bin;
					  Lex->uint_geom_type= (uint) Field::GEOM_POLYGON;
					  $$=FIELD_TYPE_GEOMETRY; }
	| MULTIPOLYGON			{ Lex->charset=&my_charset_bin;
					  Lex->uint_geom_type= (uint) Field::GEOM_MULTIPOLYGON;
					  $$=FIELD_TYPE_GEOMETRY; }
	| MEDIUMBLOB			{ Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_MEDIUM_BLOB; }
	| LONGBLOB			{ Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_LONG_BLOB; }
	| LONG_SYM VARBINARY		{ Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_MEDIUM_BLOB; }
	| LONG_SYM varchar opt_binary	{ $$=FIELD_TYPE_MEDIUM_BLOB; }
	| TINYTEXT opt_binary		{ $$=FIELD_TYPE_TINY_BLOB; }
	| TEXT_SYM opt_len opt_binary	{ $$=FIELD_TYPE_BLOB; }
	| MEDIUMTEXT opt_binary		{ $$=FIELD_TYPE_MEDIUM_BLOB; }
	| LONGTEXT opt_binary		{ $$=FIELD_TYPE_LONG_BLOB; }
	| DECIMAL_SYM float_options field_options
					{ $$=FIELD_TYPE_DECIMAL;}
	| NUMERIC_SYM float_options field_options
					{ $$=FIELD_TYPE_DECIMAL;}
	| FIXED_SYM float_options field_options
					{ $$=FIELD_TYPE_DECIMAL;}
	| ENUM {Lex->interval_list.empty();} '(' string_list ')' opt_binary
	  {
	    LEX *lex=Lex;
	    lex->interval=typelib(lex->interval_list);
	    $$=FIELD_TYPE_ENUM;
	  }
	| SET { Lex->interval_list.empty();} '(' string_list ')' opt_binary
	  {
	    LEX *lex=Lex;
	    lex->interval=typelib(lex->interval_list);
	    $$=FIELD_TYPE_SET;
	  }
	| LONG_SYM opt_binary		{ $$=FIELD_TYPE_MEDIUM_BLOB; }
	| SERIAL_SYM
	  {
	    $$=FIELD_TYPE_LONGLONG;
	    Lex->type|= (AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNSIGNED_FLAG |
		         UNIQUE_FLAG);
	  }
	;

char:
	CHAR_SYM {}
	;

nchar:
	NCHAR_SYM {}
	| NATIONAL_SYM CHAR_SYM {}
	;

varchar:
	char VARYING {}
	| VARCHAR {}
	;

nvarchar:
	NATIONAL_SYM VARCHAR {}
	| NCHAR_SYM VARCHAR {}
	| NATIONAL_SYM CHAR_SYM VARYING {}
	| NCHAR_SYM VARYING {}
	;

int_type:
	INT_SYM		{ $$=FIELD_TYPE_LONG; }
	| TINYINT	{ $$=FIELD_TYPE_TINY; }
	| SMALLINT	{ $$=FIELD_TYPE_SHORT; }
	| MEDIUMINT	{ $$=FIELD_TYPE_INT24; }
	| BIGINT	{ $$=FIELD_TYPE_LONGLONG; };

real_type:
	REAL		{ $$= YYTHD->variables.sql_mode & MODE_REAL_AS_FLOAT ?
			      FIELD_TYPE_FLOAT : FIELD_TYPE_DOUBLE; }
	| DOUBLE_SYM	{ $$=FIELD_TYPE_DOUBLE; }
	| DOUBLE_SYM PRECISION { $$=FIELD_TYPE_DOUBLE; };


float_options:
	/* empty */		{}
	| '(' NUM ')'		{ Lex->length=$2.str; }
	| precision		{};

precision:
	'(' NUM ',' NUM ')'
	{
	  LEX *lex=Lex;
	  lex->length=$2.str; lex->dec=$4.str;
	};

field_options:
	/* empty */		{}
	| field_opt_list	{};

field_opt_list:
	field_opt_list field_option {}
	| field_option {};

field_option:
	SIGNED_SYM	{}
	| UNSIGNED	{ Lex->type|= UNSIGNED_FLAG;}
	| ZEROFILL	{ Lex->type|= UNSIGNED_FLAG | ZEROFILL_FLAG; };

opt_len:
	/* empty */	{ Lex->length=(char*) 0; } /* use default length */
	| '(' NUM ')'	{ Lex->length= $2.str; };

opt_precision:
	/* empty */	{}
	| precision	{};

opt_attribute:
	/* empty */ {}
	| opt_attribute_list {};

opt_attribute_list:
	opt_attribute_list attribute {}
	| attribute;

attribute:
	NULL_SYM	  { Lex->type&= ~ NOT_NULL_FLAG; }
	| NOT NULL_SYM	  { Lex->type|= NOT_NULL_FLAG; }
	| DEFAULT literal { Lex->default_value=$2; }
	| AUTO_INC	  { Lex->type|= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG; }
	| SERIAL_SYM DEFAULT VALUE_SYM
	  { Lex->type|= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNIQUE_FLAG; }
	| opt_primary KEY_SYM { Lex->type|= PRI_KEY_FLAG | NOT_NULL_FLAG; }
	| UNIQUE_SYM	  { Lex->type|= UNIQUE_FLAG; }
	| UNIQUE_SYM KEY_SYM { Lex->type|= UNIQUE_KEY_FLAG; }
	| COMMENT_SYM text_literal { Lex->comment= $2; }
	| COLLATE_SYM collation_name 
	  { 
	    if (Lex->charset && !my_charset_same(Lex->charset,$2))
	    {
	      net_printf(YYTHD,ER_COLLATION_CHARSET_MISMATCH,
			 $2->name,Lex->charset->csname);
	      YYABORT;
	    }
	    else
	    {
	      Lex->charset=$2;
	    }
	  }
	;

charset:
	CHAR_SYM SET	{}
	| CHARSET	{}
	;

charset_name:
	ident_or_text
	{
	  if (!($$=get_charset_by_csname($1.str,MY_CS_PRIMARY,MYF(0))))
	  {
	    net_printf(YYTHD,ER_UNKNOWN_CHARACTER_SET,$1.str);
	    YYABORT;
	  }
	}
	| BINARY { $$= &my_charset_bin; }
	;

charset_name_or_default:
	charset_name { $$=$1;   }
	| DEFAULT    { $$=NULL; } ;


old_or_new_charset_name:
	ident_or_text
	{
	  if (!($$=get_charset_by_csname($1.str,MY_CS_PRIMARY,MYF(0))) &&
	      !($$=get_old_charset_by_name($1.str)))
	  {
	    net_printf(YYTHD,ER_UNKNOWN_CHARACTER_SET,$1.str);
	    YYABORT;
	  }
	}
	| BINARY { $$= &my_charset_bin; }
	;

old_or_new_charset_name_or_default:
	old_or_new_charset_name { $$=$1;   }
	| DEFAULT    { $$=NULL; } ;

collation_name:
	ident_or_text
	{
	  if (!($$=get_charset_by_name($1.str,MYF(0))))
	  {
	    net_printf(YYTHD,ER_UNKNOWN_CHARACTER_SET,$1.str);
	    YYABORT;
	  }
	};

opt_collate:
	/* empty */	{ $$=NULL; }
	| COLLATE_SYM collation_name_or_default { $$=$2; }
	;

collation_name_or_default:
	collation_name { $$=$1;   }
	| DEFAULT    { $$=NULL; } ;

opt_default:
	/* empty */	{}
	| DEFAULT	{};

opt_binary:
	/* empty */			{ Lex->charset=NULL; }
	| ASCII_SYM			{ Lex->charset=&my_charset_latin1; }
	| BYTE_SYM			{ Lex->charset=&my_charset_bin; }
	| BINARY			{ Lex->charset=&my_charset_bin; }
	| UNICODE_SYM
	{
	  if (!(Lex->charset=get_charset_by_name("ucs2",MYF(0))))
	  {
	    net_printf(YYTHD,ER_UNKNOWN_CHARACTER_SET,"ucs2");
	    YYABORT;
	  }
	}
	| charset charset_name	{ Lex->charset=$2; } ;

opt_primary:
	/* empty */
	| PRIMARY_SYM
	;

references:
	REFERENCES table_ident
	{
	  LEX *lex=Lex;
	  lex->fk_delete_opt= lex->fk_update_opt= lex->fk_match_option= 0;
	  lex->ref_list.empty();
	}
	opt_ref_list
	{
	  $$=$2;
	};

opt_ref_list:
	/* empty */ opt_on_delete {}
	| '(' ref_list ')' opt_on_delete {};

ref_list:
	ref_list ',' ident	{ Lex->ref_list.push_back(new key_part_spec($3.str)); }
	| ident			{ Lex->ref_list.push_back(new key_part_spec($1.str)); };


opt_on_delete:
	/* empty */ {}
	| opt_on_delete_list {};

opt_on_delete_list:
	opt_on_delete_list opt_on_delete_item {}
	| opt_on_delete_item {};

opt_on_delete_item:
	ON DELETE_SYM delete_option   { Lex->fk_delete_opt= $3; }
	| ON UPDATE_SYM delete_option { Lex->fk_update_opt= $3; }
	| MATCH FULL	{ Lex->fk_match_option= foreign_key::FK_MATCH_FULL; }
	| MATCH PARTIAL { Lex->fk_match_option= foreign_key::FK_MATCH_PARTIAL; }
	| MATCH SIMPLE_SYM { Lex->fk_match_option= foreign_key::FK_MATCH_SIMPLE; };

delete_option:
	RESTRICT	 { $$= (int) foreign_key::FK_OPTION_RESTRICT; }
	| CASCADE	 { $$= (int) foreign_key::FK_OPTION_CASCADE; }
	| SET NULL_SYM   { $$= (int) foreign_key::FK_OPTION_SET_NULL; }
	| NO_SYM ACTION  { $$= (int) foreign_key::FK_OPTION_NO_ACTION; }
	| SET DEFAULT    { $$= (int) foreign_key::FK_OPTION_DEFAULT;  };

key_type:
	opt_constraint PRIMARY_SYM KEY_SYM  { $$= Key::PRIMARY; }
	| key_or_index			    { $$= Key::MULTIPLE; }
	| FULLTEXT_SYM			    { $$= Key::FULLTEXT; }
	| FULLTEXT_SYM key_or_index	    { $$= Key::FULLTEXT; }
	| SPATIAL_SYM			    { $$= Key::SPATIAL; }
	| SPATIAL_SYM key_or_index	    { $$= Key::SPATIAL; }
	| opt_constraint UNIQUE_SYM	    { $$= Key::UNIQUE; }
	| opt_constraint UNIQUE_SYM key_or_index { $$= Key::UNIQUE; };

key_or_index:
	KEY_SYM {}
	| INDEX {};

keys_or_index:
	KEYS {}
	| INDEX {}
	| INDEXES {};

opt_unique_or_fulltext:
	/* empty */	{ $$= Key::MULTIPLE; }
	| UNIQUE_SYM	{ $$= Key::UNIQUE; }
	| SPATIAL_SYM	{ $$= Key::SPATIAL; };

key_alg:
	/* empty */		   { $$= HA_KEY_ALG_UNDEF; }
	| USING opt_btree_or_rtree { $$= $2; }
	| TYPE_SYM opt_btree_or_rtree  { $$= $2; };

opt_btree_or_rtree:
	BTREE_SYM	{ $$= HA_KEY_ALG_BTREE; }
	| RTREE_SYM	{ $$= HA_KEY_ALG_RTREE; }
	| HASH_SYM	{ $$= HA_KEY_ALG_HASH; };

key_list:
	key_list ',' key_part order_dir { Lex->col_list.push_back($3); }
	| key_part order_dir		{ Lex->col_list.push_back($1); };

key_part:
	ident			{ $$=new key_part_spec($1.str); }
	| ident '(' NUM ')'	{ $$=new key_part_spec($1.str,(uint) atoi($3.str)); };

opt_ident:
	/* empty */	{ $$=(char*) 0; }	/* Defaultlength */
	| field_ident	{ $$=$1.str; };

string_list:
	text_string			{ Lex->interval_list.push_back($1); }
	| string_list ',' text_string	{ Lex->interval_list.push_back($3); };

/*
** Alter table
*/

alter:
	ALTER opt_ignore TABLE_SYM table_ident
	{
	  THD *thd= YYTHD;
	  LEX *lex=&thd->lex;
	  lex->sql_command = SQLCOM_ALTER_TABLE;
	  lex->name=0;
	  if (!lex->select_lex.add_table_to_list(thd, $4, NULL,
						 TL_OPTION_UPDATING))
	    YYABORT;
	  lex->drop_primary=0;
	  lex->create_list.empty();
	  lex->key_list.empty();
	  lex->col_list.empty();
	  lex->drop_list.empty();
	  lex->alter_list.empty();
          lex->select_lex.init_order();
	  lex->select_lex.db=lex->name=0;
	  bzero((char*) &lex->create_info,sizeof(lex->create_info));
	  lex->create_info.db_type= DB_TYPE_DEFAULT;
	  lex->create_info.table_charset= thd->db_charset;
	  lex->create_info.row_type= ROW_TYPE_NOT_USED;
          lex->alter_keys_onoff=LEAVE_AS_IS;
          lex->simple_alter=1;
	}
	alter_list
	{}
	| ALTER DATABASE ident opt_create_database_options
	  {
	    LEX *lex=Lex;
	    lex->sql_command=SQLCOM_ALTER_DB;
	    lex->name=$3.str;
	  };


alter_list:
        | alter_list_item
	| alter_list ',' alter_list_item;

add_column:
	ADD opt_column { Lex->change=0; };

alter_list_item:
	add_column column_def opt_place { Lex->simple_alter=0; }
	| ADD key_def { Lex->simple_alter=0; }
	| add_column '(' field_list ')'      { Lex->simple_alter=0; }
	| CHANGE opt_column field_ident
	  {
	     LEX *lex=Lex;
	     lex->change= $3.str; lex->simple_alter=0;
	  }
          field_spec opt_place
        | MODIFY_SYM opt_column field_ident
          {
            LEX *lex=Lex;
            lex->length=lex->dec=0; lex->type=0; lex->interval=0;
            lex->default_value=lex->comment=0;
            lex->simple_alter=0;
          }
          type opt_attribute
          {
            LEX *lex=Lex;
            if (add_field_to_list(lex->thd,$3.str,
                                  (enum enum_field_types) $5,
                                  lex->length,lex->dec,lex->type,
                                  lex->default_value, lex->comment,
				  $3.str, lex->interval, lex->charset,
				  lex->uint_geom_type))
	       YYABORT;
          }
          opt_place
	| DROP opt_column field_ident opt_restrict
	  {
	    LEX *lex=Lex;
	    lex->drop_list.push_back(new Alter_drop(Alter_drop::COLUMN,
					    $3.str)); lex->simple_alter=0;
	  }
	| DROP PRIMARY_SYM KEY_SYM
	  {
	    LEX *lex=Lex;
	    lex->drop_primary=1; lex->simple_alter=0;
	  }
	| DROP FOREIGN KEY_SYM opt_ident { Lex->simple_alter=0; }
	| DROP key_or_index field_ident
	  {
	    LEX *lex=Lex;
	    lex->drop_list.push_back(new Alter_drop(Alter_drop::KEY,
						    $3.str));
	    lex->simple_alter=0;
	  }
	| DISABLE_SYM KEYS { Lex->alter_keys_onoff=DISABLE; }
	| ENABLE_SYM KEYS  { Lex->alter_keys_onoff=ENABLE; }
	| ALTER opt_column field_ident SET DEFAULT literal
	  {
	    LEX *lex=Lex;
	    lex->alter_list.push_back(new Alter_column($3.str,$6));
	    lex->simple_alter=0;
	  }
	| ALTER opt_column field_ident DROP DEFAULT
	  {
	    LEX *lex=Lex;
	    lex->alter_list.push_back(new Alter_column($3.str,(Item*) 0));
	    lex->simple_alter=0;
	  }
	| RENAME opt_to table_ident
	  {
	    LEX *lex=Lex;
	    lex->select_lex.db=$3->db.str;
	    lex->name= $3->table.str;
	  }
        | create_table_options_space_separated { Lex->simple_alter=0; }
	| order_clause         { Lex->simple_alter=0; };

opt_column:
	/* empty */	{}
	| COLUMN_SYM	{};

opt_ignore:
	/* empty */	{ Lex->duplicates=DUP_ERROR; }
	| IGNORE_SYM	{ Lex->duplicates=DUP_IGNORE; };

opt_restrict:
	/* empty */	{}
	| RESTRICT	{}
	| CASCADE	{};

opt_place:
	/* empty */	{}
	| AFTER_SYM ident { store_position_for_column($2.str); }
	| FIRST_SYM	  { store_position_for_column(first_keyword); };

opt_to:
	/* empty */	{}
	| TO_SYM	{}
	| EQ		{}
	| AS		{};

/*
  The first two deprecate the last two--delete the last two for 4.1 release
*/

slave:
	START_SYM SLAVE slave_thread_opts
        {
	  LEX *lex=Lex;
          lex->sql_command = SQLCOM_SLAVE_START;
	  lex->type = 0;
        }
        | STOP_SYM SLAVE slave_thread_opts
          {
	    LEX *lex=Lex;
            lex->sql_command = SQLCOM_SLAVE_STOP;
	    lex->type = 0;
          }
	;

start:
	START_SYM TRANSACTION_SYM { Lex->sql_command = SQLCOM_BEGIN;}
	{}
	;

slave_thread_opts:
	{ Lex->slave_thd_opt= 0; }
	slave_thread_opt_list
	;

slave_thread_opt_list:
	slave_thread_opt
	| slave_thread_opt_list ',' slave_thread_opt
	;

slave_thread_opt:
	/*empty*/	{}
	| SQL_THREAD	{ Lex->slave_thd_opt|=SLAVE_SQL; }
	| RELAY_THREAD 	{ Lex->slave_thd_opt|=SLAVE_IO; }
	;

restore:
	RESTORE_SYM table_or_tables
	{
	   Lex->sql_command = SQLCOM_RESTORE_TABLE;
	}
	table_list FROM TEXT_STRING_sys
        {
	  Lex->backup_dir = $6.str;
        };

backup:
	BACKUP_SYM table_or_tables
	{
	   Lex->sql_command = SQLCOM_BACKUP_TABLE;
	}
	table_list TO_SYM TEXT_STRING_sys
        {
	  Lex->backup_dir = $6.str;
        };

repair:
	REPAIR opt_no_write_to_binlog table_or_tables
	{
	   LEX *lex=Lex;
	   lex->sql_command = SQLCOM_REPAIR;
           lex->no_write_to_binlog= $2;
	   lex->check_opt.init();
	}
	table_list opt_mi_repair_type
	{}
	;

opt_mi_repair_type:
	/* empty */ { Lex->check_opt.flags = T_MEDIUM; }
	| mi_repair_types {};

mi_repair_types:
	mi_repair_type {}
	| mi_repair_type mi_repair_types {};

mi_repair_type:
	QUICK          { Lex->check_opt.flags|= T_QUICK; }
	| EXTENDED_SYM { Lex->check_opt.flags|= T_EXTEND; }
        | USE_FRM      { Lex->check_opt.sql_flags|= TT_USEFRM; };

analyze:
	ANALYZE_SYM opt_no_write_to_binlog table_or_tables
	{
	   LEX *lex=Lex;
	   lex->sql_command = SQLCOM_ANALYZE;
           lex->no_write_to_binlog= $2;
	   lex->check_opt.init();
	}
	table_list opt_mi_check_type
	{}
	;

check:
	CHECK_SYM table_or_tables
	{
	   LEX *lex=Lex;
	   lex->sql_command = SQLCOM_CHECK;
	   lex->check_opt.init();
	}
	table_list opt_mi_check_type
	{}
	;

opt_mi_check_type:
	/* empty */ { Lex->check_opt.flags = T_MEDIUM; }
	| mi_check_types {};

mi_check_types:
	mi_check_type {}
	| mi_check_type mi_check_types {};

mi_check_type:
	QUICK      { Lex->check_opt.flags|= T_QUICK; }
	| FAST_SYM { Lex->check_opt.flags|= T_FAST; }
	| MEDIUM_SYM { Lex->check_opt.flags|= T_MEDIUM; }
	| EXTENDED_SYM { Lex->check_opt.flags|= T_EXTEND; }
	| CHANGED  { Lex->check_opt.flags|= T_CHECK_ONLY_CHANGED; };

optimize:
	OPTIMIZE opt_no_write_to_binlog table_or_tables
	{
	   LEX *lex=Lex;
	   lex->sql_command = SQLCOM_OPTIMIZE;
           lex->no_write_to_binlog= $2;        
	   lex->check_opt.init();
	}
	table_list opt_mi_check_type
	{}
	;

opt_no_write_to_binlog:
	/* empty */        { $$= 0; }
	| NO_WRITE_TO_BINLOG  { $$= 1; }
	;

rename:
	RENAME table_or_tables
	{
	   Lex->sql_command=SQLCOM_RENAME_TABLE;
	}
	table_to_table_list
	{}
	;

table_to_table_list:
	table_to_table
	| table_to_table_list ',' table_to_table;

table_to_table:
	table_ident TO_SYM table_ident
	{
	  LEX *lex=Lex;	  
	  SELECT_LEX_NODE *sl= lex->current_select;
	  if (!sl->add_table_to_list(lex->thd, $1,NULL,TL_OPTION_UPDATING,
				     TL_IGNORE) ||
	      !sl->add_table_to_list(lex->thd, $3,NULL,TL_OPTION_UPDATING,
				     TL_IGNORE))
	    YYABORT;
	};

/*
  Select : retrieve data from table
*/


select:
	select_init { Lex->sql_command=SQLCOM_SELECT; };

/* Need select_init2 for subselects. */
select_init:
	SELECT_SYM select_init2
	|
	'(' SELECT_SYM select_part2 ')'
	  {
	    LEX *lex= Lex;
            SELECT_LEX * sel= lex->current_select->select_lex();
	    if (sel->set_braces(1))
	    {
	      send_error(lex->thd, ER_SYNTAX_ERROR);
	      YYABORT;
	    }
	  if (sel->linkage == UNION_TYPE &&
	      !sel->master_unit()->first_select()->braces)
	  {
	    send_error(lex->thd, ER_SYNTAX_ERROR);
	    YYABORT;
	  }
            /* select in braces, can't contain global parameters */
            sel->master_unit()->global_parameters=
               sel->master_unit();
          } union_opt;

select_init2:
	select_part2
        {
	  LEX *lex= Lex;
          SELECT_LEX * sel= lex->current_select->select_lex();
          if (lex->current_select->set_braces(0))
	  {
	    send_error(lex->thd, ER_SYNTAX_ERROR);
	    YYABORT;
	  }
	  if (sel->linkage == UNION_TYPE &&
	      sel->master_unit()->first_select()->braces)
	  {
	    send_error(lex->thd, ER_SYNTAX_ERROR);
	    YYABORT;
	  }
	}
	union_clause
	;

select_part2:
	{
	  LEX *lex=Lex;
	  SELECT_LEX * sel= lex->current_select->select_lex();
	  if (lex->current_select == &lex->select_lex)
	    lex->lock_option= TL_READ; /* Only for global SELECT */
	  if (sel->linkage != UNION_TYPE)
	    mysql_init_select(lex);
	  lex->current_select->parsing_place= SELECT_LEX_NODE::SELECT_LIST;
	}
	select_options select_item_list
	{
	  Select->parsing_place= SELECT_LEX_NODE::NO_MATTER;
	}
	select_into select_lock_type;

select_into:
	opt_limit_clause {}
	| FROM DUAL_SYM /* oracle compatibility: oracle always requires FROM
                           clause, and DUAL is system table without fields.
                           Is "SELECT 1 FROM DUAL" any better than
                           "SELECT 1" ? Hmmm :) */
        | into
	| select_from
	| into select_from
	| select_from into;

select_from:
	FROM join_table_list where_clause group_clause having_clause opt_order_clause opt_limit_clause procedure_clause;

select_options:
	/* empty*/
	| select_option_list;

select_option_list:
	select_option_list select_option
	| select_option;

select_option:
	STRAIGHT_JOIN { Select->options|= SELECT_STRAIGHT_JOIN; }
	| HIGH_PRIORITY
	  {
	    if (check_simple_select())
	      YYABORT;
	    Lex->lock_option= TL_READ_HIGH_PRIORITY;
	  }
	| DISTINCT	{ Select->options|= SELECT_DISTINCT; }
	| SQL_SMALL_RESULT { Select->options|= SELECT_SMALL_RESULT; }
	| SQL_BIG_RESULT { Select->options|= SELECT_BIG_RESULT; }
	| SQL_BUFFER_RESULT
	  {
	    if (check_simple_select())
	      YYABORT;
	    Select->options|= OPTION_BUFFER_RESULT;
	  }
	| SQL_CALC_FOUND_ROWS
	  {
	    if (check_simple_select())
	      YYABORT;
	    Select->options|= OPTION_FOUND_ROWS;
	  }
	| SQL_NO_CACHE_SYM { Lex->uncacheable(); }
	| SQL_CACHE_SYM    { Select->options|= OPTION_TO_QUERY_CACHE; }
	| ALL		{}
	;

select_lock_type:
	/* empty */
	| FOR_SYM UPDATE_SYM
	  {
	    LEX *lex=Lex;
	    lex->current_select->set_lock_for_tables(TL_WRITE);
	    lex->safe_to_cache_query=0;
	  }
	| LOCK_SYM IN_SYM SHARE_SYM MODE_SYM
	  {
	    LEX *lex=Lex;
	    lex->current_select->
	      set_lock_for_tables(TL_READ_WITH_SHARED_LOCKS);
	    lex->safe_to_cache_query=0;
	  }
	;

select_item_list:
	  select_item_list ',' select_item
	| select_item
	| '*'
	  {
	    THD *thd= YYTHD;
	    if (add_item_to_list(thd, new Item_field(NULL, NULL, "*")))
	      YYABORT;
	    (thd->lex.current_select->select_lex()->with_wild)++;
	  };


select_item:
	  remember_name select_item2 remember_end select_alias
	  {
	    if (add_item_to_list(YYTHD, $2))
	      YYABORT;
	    if ($4.str)
	      $2->set_name($4.str,$4.length,system_charset_info);
	    else if (!$2->name)
	      $2->set_name($1,(uint) ($3 - $1), YYTHD->charset());
	  };

remember_name:
	{ $$=(char*) Lex->tok_start; };

remember_end:
	{ $$=(char*) Lex->tok_end; };

select_item2:
	table_wild	{ $$=$1; } /* table.* */
	| expr		{ $$=$1; };

select_alias:
	/* empty */		{ $$.str=0;}
	| AS ident		{ $$=$2; }
	| AS TEXT_STRING_sys	{ $$=$2; }
	| ident			{ $$=$1; }
	| TEXT_STRING_sys	{ $$=$1; }
	;

optional_braces:
	/* empty */ {}
	| '(' ')' {};

/* all possible expressions */
expr:	
	expr_expr	{ $$= $1; }
	| simple_expr	{ $$= $1; }
	;

comp_op:  EQ		{ $$ = &comp_eq_creator; }
	| GE		{ $$ = &comp_ge_creator; }
	| GT_SYM	{ $$ = &comp_gt_creator; }
	| LE		{ $$ = &comp_le_creator; }
	| LT		{ $$ = &comp_lt_creator; }
	| NE		{ $$ = &comp_ne_creator; }
	;

all_or_any: ALL     { $$ = 1; }
        |   ANY_SYM { $$ = 0; }
        ;

/* expressions that begin with 'expr' */
expr_expr:
	 expr IN_SYM '(' expr_list ')'
	  { $$= new Item_func_in($1,*$4); }
	| expr NOT IN_SYM '(' expr_list ')'
	  { $$= new Item_func_not(new Item_func_in($1,*$5)); }
        | expr IN_SYM in_subselect
          { $$= new Item_in_subselect(YYTHD, $1, $3); }
	| expr NOT IN_SYM in_subselect
          {
            $$= new Item_func_not(new Item_in_subselect(YYTHD, $1, $4));
          }
	| expr BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_between($1,$3,$5); }
	| expr NOT BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_not(new Item_func_between($1,$4,$6)); }
	| expr OR_OR_CONCAT expr { $$= or_or_concat(YYTHD, $1,$3); }
	| expr OR expr		{ $$= new Item_cond_or($1,$3); }
        | expr XOR expr		{ $$= new Item_cond_xor($1,$3); }
	| expr AND expr		{ $$= new Item_cond_and($1,$3); }
	| expr SOUNDS_SYM LIKE expr { $$= Item_bool_func2::eq_creator(new Item_func_soundex($1), new Item_func_soundex($4));}
	| expr LIKE simple_expr opt_escape { $$= new Item_func_like($1,$3,$4); }
	| expr NOT LIKE simple_expr opt_escape	{ $$= new Item_func_not(new Item_func_like($1,$4,$5));}
	| expr REGEXP expr { $$= new Item_func_regex($1,$3); }
	| expr NOT REGEXP expr { $$= new Item_func_not(new Item_func_regex($1,$4)); }
	| expr IS NULL_SYM	{ $$= new Item_func_isnull($1); }
	| expr IS NOT NULL_SYM { $$= new Item_func_isnotnull($1); }
	| expr EQUAL_SYM expr	{ $$= new Item_func_equal($1,$3); }
	| expr comp_op expr %prec EQ	{ $$= (*((*$2)(0)))($1,$3); }
	| expr comp_op all_or_any in_subselect %prec EQ
	{
	  Item_allany_subselect *it=
	    new Item_allany_subselect(YYTHD, $1, (*$2)($3), $4);
	  if ($3)
	    $$ = new Item_func_not(it);	/* ALL */
	  else
	    $$ = it;			/* ANY/SOME */
	}
	| expr SHIFT_LEFT expr	{ $$= new Item_func_shift_left($1,$3); }
	| expr SHIFT_RIGHT expr { $$= new Item_func_shift_right($1,$3); }
	| expr '+' expr		{ $$= new Item_func_plus($1,$3); }
	| expr '-' expr		{ $$= new Item_func_minus($1,$3); }
	| expr '*' expr		{ $$= new Item_func_mul($1,$3); }
	| expr '/' expr		{ $$= new Item_func_div($1,$3); }
	| expr DIV_SYM expr	{ $$= new Item_func_int_div($1,$3); }
	| expr MOD_SYM expr	{ $$= new Item_func_mod($1,$3); }
	| expr '|' expr		{ $$= new Item_func_bit_or($1,$3); }
        | expr '^' expr		{ $$= new Item_func_bit_xor($1,$3); }
	| expr '&' expr		{ $$= new Item_func_bit_and($1,$3); }
	| expr '%' expr		{ $$= new Item_func_mod($1,$3); }
	| expr '+' interval_expr interval
	  { $$= new Item_date_add_interval($1,$3,$4,0); }
	| expr '-' interval_expr interval
	  { $$= new Item_date_add_interval($1,$3,$4,1); }
	;

/* expressions that begin with 'expr' that do NOT follow IN_SYM */
no_in_expr:
	no_in_expr BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_between($1,$3,$5); }
	| no_in_expr NOT BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_not(new Item_func_between($1,$4,$6)); }
	| no_in_expr OR_OR_CONCAT expr	{ $$= or_or_concat(YYTHD, $1,$3); }
	| no_in_expr OR expr		{ $$= new Item_cond_or($1,$3); }
        | no_in_expr XOR expr		{ $$= new Item_cond_xor($1,$3); }
	| no_in_expr AND expr		{ $$= new Item_cond_and($1,$3); }
	| no_in_expr SOUNDS_SYM LIKE expr { $$= Item_bool_func2::eq_creator(new Item_func_soundex($1), new Item_func_soundex($4));}
	| no_in_expr LIKE simple_expr opt_escape { $$= new Item_func_like($1,$3,$4); }
	| no_in_expr NOT LIKE simple_expr opt_escape { $$= new Item_func_not(new Item_func_like($1,$4,$5)); }
	| no_in_expr REGEXP expr { $$= new Item_func_regex($1,$3); }
	| no_in_expr NOT REGEXP expr { $$= new Item_func_not(new Item_func_regex($1,$4)); }
	| no_in_expr IS NULL_SYM	{ $$= new Item_func_isnull($1); }
	| no_in_expr IS NOT NULL_SYM { $$= new Item_func_isnotnull($1); }
	| no_in_expr EQUAL_SYM expr	{ $$= new Item_func_equal($1,$3); }
	| no_in_expr comp_op expr %prec EQ	{ $$= (*((*$2)(0)))($1,$3); }
	| no_in_expr comp_op all_or_any in_subselect %prec EQ
	{
	  Item_allany_subselect *it=
	    new Item_allany_subselect(YYTHD, $1, (*$2)($3), $4);
	  if ($3)
	    $$ = new Item_func_not(it);	/* ALL */
	  else
	    $$ = it;			/* ANY/SOME */
	}
	| no_in_expr SHIFT_LEFT expr  { $$= new Item_func_shift_left($1,$3); }
	| no_in_expr SHIFT_RIGHT expr { $$= new Item_func_shift_right($1,$3); }
	| no_in_expr '+' expr		{ $$= new Item_func_plus($1,$3); }
	| no_in_expr '-' expr		{ $$= new Item_func_minus($1,$3); }
	| no_in_expr '*' expr		{ $$= new Item_func_mul($1,$3); }
	| no_in_expr '/' expr		{ $$= new Item_func_div($1,$3); }
	| no_in_expr DIV_SYM expr	{ $$= new Item_func_int_div($1,$3); }
	| no_in_expr '|' expr		{ $$= new Item_func_bit_or($1,$3); }
        | no_in_expr '^' expr		{ $$= new Item_func_bit_xor($1,$3); }
	| no_in_expr '&' expr		{ $$= new Item_func_bit_and($1,$3); }
	| no_in_expr '%' expr		{ $$= new Item_func_mod($1,$3); }
	| no_in_expr MOD_SYM expr	{ $$= new Item_func_mod($1,$3); }
	| no_in_expr '+' interval_expr interval
	  { $$= new Item_date_add_interval($1,$3,$4,0); }
	| no_in_expr '-' interval_expr interval
	  { $$= new Item_date_add_interval($1,$3,$4,1); }
	| simple_expr;

/* expressions that begin with 'expr' that does NOT follow AND */
no_and_expr:
	  no_and_expr IN_SYM '(' expr_list ')'
	  { $$= new Item_func_in($1,*$4); }
	| no_and_expr NOT IN_SYM '(' expr_list ')'
	  { $$= new Item_func_not(new Item_func_in($1,*$5)); }
        | no_and_expr IN_SYM in_subselect
          { $$= new Item_in_subselect(YYTHD, $1, $3); }
	| no_and_expr NOT IN_SYM in_subselect
          {
            $$= new Item_func_not(new Item_in_subselect(YYTHD, $1, $4));
          }
	| no_and_expr BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_between($1,$3,$5); }
	| no_and_expr NOT BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_not(new Item_func_between($1,$4,$6)); }
	| no_and_expr OR_OR_CONCAT expr	{ $$= or_or_concat(YYTHD, $1,$3); }
	| no_and_expr OR expr		{ $$= new Item_cond_or($1,$3); }
        | no_and_expr XOR expr		{ $$= new Item_cond_xor($1,$3); }
	| no_and_expr SOUNDS_SYM LIKE expr { $$= Item_bool_func2::eq_creator(new Item_func_soundex($1), new Item_func_soundex($4));}
	| no_and_expr LIKE simple_expr opt_escape { $$= new Item_func_like($1,$3,$4); }
	| no_and_expr NOT LIKE simple_expr opt_escape	{ $$= new Item_func_not(new Item_func_like($1,$4,$5)); }
	| no_and_expr REGEXP expr { $$= new Item_func_regex($1,$3); }
	| no_and_expr NOT REGEXP expr { $$= new Item_func_not(new Item_func_regex($1,$4)); }
	| no_and_expr IS NULL_SYM	{ $$= new Item_func_isnull($1); }
	| no_and_expr IS NOT NULL_SYM { $$= new Item_func_isnotnull($1); }
	| no_and_expr EQUAL_SYM expr	{ $$= new Item_func_equal($1,$3); }
	| no_and_expr comp_op expr %prec EQ { $$= (*((*$2)(0)))($1,$3); }
	| no_and_expr comp_op all_or_any in_subselect %prec EQ
	{
	  Item_allany_subselect *it=
	    new Item_allany_subselect(YYTHD, $1, (*$2)($3), $4);
	  if ($3)
	    $$ = new Item_func_not(it);	/* ALL */
	  else
	    $$ = it;			/* ANY/SOME */
	}
	| no_and_expr SHIFT_LEFT expr  { $$= new Item_func_shift_left($1,$3); }
	| no_and_expr SHIFT_RIGHT expr { $$= new Item_func_shift_right($1,$3); }
	| no_and_expr '+' expr		{ $$= new Item_func_plus($1,$3); }
	| no_and_expr '-' expr		{ $$= new Item_func_minus($1,$3); }
	| no_and_expr '*' expr		{ $$= new Item_func_mul($1,$3); }
	| no_and_expr '/' expr		{ $$= new Item_func_div($1,$3); }
	| no_and_expr DIV_SYM expr	{ $$= new Item_func_int_div($1,$3); }
	| no_and_expr '|' expr		{ $$= new Item_func_bit_or($1,$3); }
        | no_and_expr '^' expr		{ $$= new Item_func_bit_xor($1,$3); }
	| no_and_expr '&' expr		{ $$= new Item_func_bit_and($1,$3); }
	| no_and_expr '%' expr		{ $$= new Item_func_mod($1,$3); }
	| no_and_expr MOD_SYM expr	{ $$= new Item_func_mod($1,$3); }
	| no_and_expr '+' interval_expr interval
	  { $$= new Item_date_add_interval($1,$3,$4,0); }
	| no_and_expr '-' interval_expr interval
	  { $$= new Item_date_add_interval($1,$3,$4,1); }
	| simple_expr;

interval_expr:
         INTERVAL_SYM expr { $$=$2; }
        ;

simple_expr:
	simple_ident
 	| simple_expr COLLATE_SYM ident_or_text %prec NEG
	  { 
	    $$= new Item_func_set_collation($1,new Item_string($3.str,$3.length,
					    YYTHD->charset()));
	  }
	| literal
	| param_marker
	| '@' ident_or_text SET_VAR expr
	  {
	    $$= new Item_func_set_user_var($2,$4);
	    Lex->uncacheable();
	  }
	| '@' ident_or_text
	  {
	    $$= new Item_func_get_user_var($2);
	    Lex->uncacheable();
	  }
	| '@' '@' opt_var_ident_type ident_or_text
	  {
	    if (!($$= get_system_var((enum_var_type) $3, $4)))
	      YYABORT;
	  }
	| sum_expr
	| '-' expr %prec NEG	{ $$= new Item_func_neg($2); }
	| '~' expr %prec NEG	{ $$= new Item_func_bit_neg($2); }
	| NOT expr %prec NEG	{ $$= new Item_func_not($2); }
	| '!' expr %prec NEG	{ $$= new Item_func_not($2); }
	| '(' expr ')'		{ $$= $2; }
	| '(' expr ',' expr_list ')'
	  {
	    $4->push_front($2);
	    $$= new Item_row(*$4);
	  }
	| ROW_SYM '(' expr ',' expr_list ')'
	  {
	    $5->push_front($3);
	    $$= new Item_row(*$5);
	  }
	| EXISTS exists_subselect { $$= $2; }
	| singlerow_subselect   { $$= $1; }
	| '{' ident expr '}'	{ $$= $3; }
        | MATCH ident_list_arg AGAINST '(' expr ')'
          { Select->add_ftfunc_to_list((Item_func_match *)
                   ($$=new Item_func_match_nl(*$2,$5))); }
        | MATCH ident_list_arg AGAINST '(' expr IN_SYM BOOLEAN_SYM MODE_SYM ')'
          { Select->add_ftfunc_to_list((Item_func_match *)
                   ($$=new Item_func_match_bool(*$2,$5))); }
	| ASCII_SYM '(' expr ')' { $$= new Item_func_ascii($3); }
	| BINARY expr %prec NEG 
	  {
	    $$= new Item_func_set_collation($2,new Item_string("BINARY",6,
					    &my_charset_latin1));
	  }
	| CAST_SYM '(' expr AS cast_type ')'  { $$= create_func_cast($3, $5); }
	| CASE_SYM opt_expr WHEN_SYM when_list opt_else END
	  { $$= new Item_func_case(* $4, $2, $5 ); }
	| CONVERT_SYM '(' expr ',' cast_type ')'  { $$= create_func_cast($3, $5); }
	| CONVERT_SYM '(' expr USING charset_name ')'
	  { $$= new Item_func_conv_charset($3,$5); }
	| CONVERT_SYM '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_conv_charset3($3,$7,$5); }
	| DEFAULT '(' simple_ident ')'
	  { $$= new Item_default_value($3); }
	| VALUES '(' simple_ident ')'
	  { $$= new Item_insert_value($3); }
	| FUNC_ARG0 '(' ')'
	  { $$= ((Item*(*)(void))($1.symbol->create_func))();}
	| FUNC_ARG1 '(' expr ')'
	  { $$= ((Item*(*)(Item*))($1.symbol->create_func))($3);}
	| FUNC_ARG2 '(' expr ',' expr ')'
	  { $$= ((Item*(*)(Item*,Item*))($1.symbol->create_func))($3,$5);}
	| FUNC_ARG3 '(' expr ',' expr ',' expr ')'
	  { $$= ((Item*(*)(Item*,Item*,Item*))($1.symbol->create_func))($3,$5,$7);}
	| ATAN	'(' expr ')'
	  { $$= new Item_func_atan($3); }
	| ATAN	'(' expr ',' expr ')'
	  { $$= new Item_func_atan($3,$5); }
	| CHAR_SYM '(' expr_list ')'
	  { $$= new Item_func_char(*$3); }
	| CHARSET '(' expr ')'
	  { $$= new Item_func_charset($3); }
	| COALESCE '(' expr_list ')'
	  { $$= new Item_func_coalesce(* $3); }
	| COLLATION_SYM '(' expr ')'
	  { $$= new Item_func_collation($3); }
	| CONCAT '(' expr_list ')'
	  { $$= new Item_func_concat(* $3); }
	| CONCAT_WS '(' expr ',' expr_list ')'
	  { $$= new Item_func_concat_ws($3, *$5); }
	| CURDATE optional_braces
	  { $$= new Item_func_curdate(); Lex->safe_to_cache_query=0; }
	| CURTIME optional_braces
	  { $$= new Item_func_curtime(); Lex->safe_to_cache_query=0; }
	| CURTIME '(' expr ')'
	  {
	    $$= new Item_func_curtime($3);
	    Lex->safe_to_cache_query=0;
	  }
	| DATE_ADD_INTERVAL '(' expr ',' interval_expr interval ')'
	  { $$= new Item_date_add_interval($3,$5,$6,0); }
	| DATE_SUB_INTERVAL '(' expr ',' interval_expr interval ')'
	  { $$= new Item_date_add_interval($3,$5,$6,1); }
	| DATABASE '(' ')'
	  {
	    $$= new Item_func_database();
            Lex->safe_to_cache_query=0;
	  }
	| ELT_FUNC '(' expr ',' expr_list ')'
	  { $$= new Item_func_elt($3, *$5); }
	| MAKE_SET_SYM '(' expr ',' expr_list ')'
	  { $$= new Item_func_make_set($3, *$5); }
	| ENCRYPT '(' expr ')'
	  {
	    $$= new Item_func_encrypt($3);
	    Lex->uncacheable();
	  }
	| ENCRYPT '(' expr ',' expr ')'   { $$= new Item_func_encrypt($3,$5); }
	| DECODE_SYM '(' expr ',' TEXT_STRING_literal ')'
	  { $$= new Item_func_decode($3,$5.str); }
	| ENCODE_SYM '(' expr ',' TEXT_STRING_literal ')'
	 { $$= new Item_func_encode($3,$5.str); }
	| DES_DECRYPT_SYM '(' expr ')'
        { $$= new Item_func_des_decrypt($3); }
	| DES_DECRYPT_SYM '(' expr ',' expr ')'
        { $$= new Item_func_des_decrypt($3,$5); }
	| DES_ENCRYPT_SYM '(' expr ')'
        { $$= new Item_func_des_encrypt($3); }
	| DES_ENCRYPT_SYM '(' expr ',' expr ')'
        { $$= new Item_func_des_encrypt($3,$5); }
	| EXPORT_SET '(' expr ',' expr ',' expr ')'
		{ $$= new Item_func_export_set($3, $5, $7); }
	| EXPORT_SET '(' expr ',' expr ',' expr ',' expr ')'
		{ $$= new Item_func_export_set($3, $5, $7, $9); }
	| EXPORT_SET '(' expr ',' expr ',' expr ',' expr ',' expr ')'
		{ $$= new Item_func_export_set($3, $5, $7, $9, $11); }
	| FALSE_SYM
	  { $$= new Item_int((char*) "FALSE",0,1); }
	| FORMAT_SYM '(' expr ',' NUM ')'
	  { $$= new Item_func_format($3,atoi($5.str)); }
	| FROM_UNIXTIME '(' expr ')'
	  { $$= new Item_func_from_unixtime($3); }
	| FROM_UNIXTIME '(' expr ',' expr ')'
	  {
	    $$= new Item_func_date_format (new Item_func_from_unixtime($3),$5,0);
	  }
	| FIELD_FUNC '(' expr ',' expr_list ')'
	  { $$= new Item_func_field($3, *$5); }
	| GEOMFROMTEXT '(' expr ')'
	  { $$= new Item_func_geometry_from_text($3); }
	| GEOMFROMTEXT '(' expr ',' expr ')'
	  { $$= new Item_func_geometry_from_text($3, $5); }
	| GEOMFROMWKB '(' expr ')'
	  { $$= new Item_func_geometry_from_wkb($3); }
	| GEOMFROMWKB '(' expr ',' expr ')'
	  { $$= new Item_func_geometry_from_wkb($3, $5); }
	| GEOMETRYCOLLECTION '(' expr_list ')'
	  { $$= new Item_func_spatial_collection(* $3,
                       Geometry::wkbGeometryCollection,
                       Geometry::wkbPoint); }
	| HOUR_SYM '(' expr ')'
	  { $$= new Item_func_hour($3); }
	| IF '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_if($3,$5,$7); }
	| INSERT '(' expr ',' expr ',' expr ',' expr ')'
	  { $$= new Item_func_insert($3,$5,$7,$9); }
	| interval_expr interval '+' expr
	  /* we cannot put interval before - */
	  { $$= new Item_date_add_interval($4,$1,$2,0); }
	| interval_expr
	  {
            if ($1->type() != Item::ROW_ITEM)
            {
              send_error(Lex->thd, ER_SYNTAX_ERROR);
              YYABORT;
            }
            $$= new Item_func_interval((Item_row *)$1);
          }
	| LAST_INSERT_ID '(' ')'
	  {
	    $$= get_system_var(OPT_SESSION, "last_insert_id", 14,
			      "last_insert_id()");
	    Lex->safe_to_cache_query= 0;
	  }
	| LAST_INSERT_ID '(' expr ')'
	  {
	    $$= new Item_func_set_last_insert_id($3);
	    Lex->safe_to_cache_query= 0;
	  }
	| LEFT '(' expr ',' expr ')'
	  { $$= new Item_func_left($3,$5); }
	| LINESTRING '(' expr_list ')'
	  { $$= new Item_func_spatial_collection(* $3,
               Geometry::wkbLineString, Geometry::wkbPoint); }
	| LOCATE '(' expr ',' expr ')'
	  { $$= new Item_func_locate($5,$3); }
	| LOCATE '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_locate($5,$3,$7); }
 	| GEOMCOLLFROMTEXT '(' expr ')'
	  { $$= new Item_func_geometry_from_text($3); }
	| GEOMCOLLFROMTEXT '(' expr ',' expr ')'
	  { $$= new Item_func_geometry_from_text($3, $5); }
	| GREATEST_SYM '(' expr ',' expr_list ')'
	  { $5->push_front($3); $$= new Item_func_max(*$5); }
	| LEAST_SYM '(' expr ',' expr_list ')'
	  { $5->push_front($3); $$= new Item_func_min(*$5); }
	| LOG_SYM '(' expr ')'
	  { $$= new Item_func_log($3); }
	| LOG_SYM '(' expr ',' expr ')'
	  { $$= new Item_func_log($3, $5); }
 	| LINEFROMTEXT '(' expr ')'
	  { $$= new Item_func_geometry_from_text($3); }
	| LINEFROMTEXT '(' expr ',' expr ')'
	  { $$= new Item_func_geometry_from_text($3, $5); }
	| MASTER_POS_WAIT '(' expr ',' expr ')'
	  { 
	    $$= new Item_master_pos_wait($3, $5);
	    Lex->safe_to_cache_query=0; 
		  }
	| MASTER_POS_WAIT '(' expr ',' expr ',' expr ')'
	  { 
	    $$= new Item_master_pos_wait($3, $5, $7);
	    Lex->safe_to_cache_query=0; 
	  }
	| MINUTE_SYM '(' expr ')'
	  { $$= new Item_func_minute($3); }
	| MOD_SYM '(' expr ',' expr ')'
	  { $$ = new Item_func_mod( $3, $5); }
	| MONTH_SYM '(' expr ')'
	  { $$= new Item_func_month($3); }
 	| MULTILINESTRING '(' expr_list ')'
 	  { $$= new Item_func_spatial_collection(* $3,
                    Geometry::wkbMultiLineString, Geometry::wkbLineString); }
 	| MLINEFROMTEXT '(' expr ')'
	  { $$= new Item_func_geometry_from_text($3); }
	| MLINEFROMTEXT '(' expr ',' expr ')'
	  { $$= new Item_func_geometry_from_text($3, $5); }
	| MPOINTFROMTEXT '(' expr ')'
	  { $$= new Item_func_geometry_from_text($3); }
	| MPOINTFROMTEXT '(' expr ',' expr ')'
	  { $$= new Item_func_geometry_from_text($3, $5); }
	| MPOLYFROMTEXT '(' expr ')'
	  { $$= new Item_func_geometry_from_text($3); }
	| MPOLYFROMTEXT '(' expr ',' expr ')'
	  { $$= new Item_func_geometry_from_text($3, $5); }
	| MULTIPOINT '(' expr_list ')'
	  { $$= new Item_func_spatial_collection(* $3,
                    Geometry::wkbMultiPoint, Geometry::wkbPoint); }
 	| MULTIPOLYGON '(' expr_list ')'
	  { $$= new Item_func_spatial_collection(* $3,
                       Geometry::wkbMultiPolygon, Geometry::wkbPolygon ); }
	| NOW_SYM optional_braces
	  { $$= new Item_func_now(); Lex->safe_to_cache_query=0;}
	| NOW_SYM '(' expr ')'
	  { $$= new Item_func_now($3); Lex->safe_to_cache_query=0;}
	| PASSWORD '(' expr ')'
	  { $$= new Item_func_password($3); }
        | PASSWORD '(' expr ',' expr ')'
          { $$= new Item_func_password($3,$5); }
	| POINT_SYM '(' expr ',' expr ')'
	  { $$= new Item_func_point($3,$5); }
 	| POINTFROMTEXT '(' expr ')'
	  { $$= new Item_func_geometry_from_text($3); }
	| POINTFROMTEXT '(' expr ',' expr ')'
	  { $$= new Item_func_geometry_from_text($3, $5); }
	| POLYFROMTEXT '(' expr ')'
	  { $$= new Item_func_geometry_from_text($3); }
	| POLYFROMTEXT '(' expr ',' expr ')'
	  { $$= new Item_func_geometry_from_text($3, $5); }
	| POLYGON '(' expr_list ')'
	  { $$= new Item_func_spatial_collection(* $3,
			Geometry::wkbPolygon, Geometry::wkbLineString); }
	| POSITION_SYM '(' no_in_expr IN_SYM expr ')'
	  { $$ = new Item_func_locate($5,$3); }
	| RAND '(' expr ')'
	  { $$= new Item_func_rand($3); Lex->uncacheable();}
	| RAND '(' ')'
	  { $$= new Item_func_rand(); Lex->uncacheable();}
	| REPLACE '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_replace($3,$5,$7); }
	| RIGHT '(' expr ',' expr ')'
	  { $$= new Item_func_right($3,$5); }
	| ROUND '(' expr ')'
	  { $$= new Item_func_round($3, new Item_int((char*)"0",0,1),0); }
	| ROUND '(' expr ',' expr ')' { $$= new Item_func_round($3,$5,0); }
	| SECOND_SYM '(' expr ')'
	  { $$= new Item_func_second($3); }
	| SUBSTRING '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_substr($3,$5,$7); }
	| SUBSTRING '(' expr ',' expr ')'
	  { $$= new Item_func_substr($3,$5); }
	| SUBSTRING '(' expr FROM expr FOR_SYM expr ')'
	  { $$= new Item_func_substr($3,$5,$7); }
	| SUBSTRING '(' expr FROM expr ')'
	  { $$= new Item_func_substr($3,$5); }
	| SUBSTRING_INDEX '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_substr_index($3,$5,$7); }
	| TRIM '(' expr ')'
	  { $$= new Item_func_trim($3,new Item_string(" ",1,default_charset_info)); }
	| TRIM '(' LEADING opt_pad FROM expr ')'
	  { $$= new Item_func_ltrim($6,$4); }
	| TRIM '(' TRAILING opt_pad FROM expr ')'
	  { $$= new Item_func_rtrim($6,$4); }
	| TRIM '(' BOTH opt_pad FROM expr ')'
	  { $$= new Item_func_trim($6,$4); }
	| TRIM '(' expr FROM expr ')'
	  { $$= new Item_func_trim($5,$3); }
	| TRUNCATE_SYM '(' expr ',' expr ')'
	  { $$= new Item_func_round($3,$5,1); }
	| TRUE_SYM
	  { $$= new Item_int((char*) "TRUE",1,1); }
	| UDA_CHAR_SUM '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_sum_udf_str($1, *$3);
	    else
	      $$ = new Item_sum_udf_str($1);
	  }
	| UDA_FLOAT_SUM '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_sum_udf_float($1, *$3);
	    else
	      $$ = new Item_sum_udf_float($1);
	  }
	| UDA_INT_SUM '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_sum_udf_int($1, *$3);
	    else
	      $$ = new Item_sum_udf_int($1);
	  }
	| UDF_CHAR_FUNC '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_func_udf_str($1, *$3);
	    else
	      $$ = new Item_func_udf_str($1);
	  }
	| UDF_FLOAT_FUNC '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_func_udf_float($1, *$3);
	    else
	      $$ = new Item_func_udf_float($1);
	  }
	| UDF_INT_FUNC '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_func_udf_int($1, *$3);
	    else
	      $$ = new Item_func_udf_int($1);
	  }
	| UNIQUE_USERS '(' text_literal ',' NUM ',' NUM ',' expr_list ')'
	  {
            $$= new Item_func_unique_users($3,atoi($5.str),atoi($7.str), * $9);
	  }
	| UNIX_TIMESTAMP '(' ')'
	  {
	    $$= new Item_func_unix_timestamp();
	    Lex->safe_to_cache_query=0;
	  }
	| UNIX_TIMESTAMP '(' expr ')'
	  { $$= new Item_func_unix_timestamp($3); }
	| USER '(' ')'
	  { $$= new Item_func_user(); Lex->safe_to_cache_query=0; }
	| WEEK_SYM '(' expr ')'
	  { 
            $$= new Item_func_week($3,new Item_int((char*) "0",
				   YYTHD->variables.default_week_format,1)); 
          }
	| WEEK_SYM '(' expr ',' expr ')'
	  { $$= new Item_func_week($3,$5); }
	| YEAR_SYM '(' expr ')'
	  { $$= new Item_func_year($3); }
	| YEARWEEK '(' expr ')'
	  { $$= new Item_func_yearweek($3,new Item_int((char*) "0",0,1)); }
	| YEARWEEK '(' expr ',' expr ')'
	  { $$= new Item_func_yearweek($3, $5); }
	| BENCHMARK_SYM '(' ULONG_NUM ',' expr ')'
	  {
	    $$=new Item_func_benchmark($3,$5);
	    Lex->uncacheable();
	  }
	| EXTRACT_SYM '(' interval FROM expr ')'
	{ $$=new Item_extract( $3, $5); };

udf_expr_list:
	/* empty */	{ $$= NULL; }
	| expr_list	{ $$= $1;};

sum_expr:
	AVG_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_avg($3); }
	| BIT_AND  '(' in_sum_expr ')'
	  { $$=new Item_sum_and($3); }
	| BIT_OR  '(' in_sum_expr ')'
	  { $$=new Item_sum_or($3); }
	| COUNT_SYM '(' opt_all '*' ')'
	  { $$=new Item_sum_count(new Item_int((int32) 0L,1)); }
	| COUNT_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_count($3); }
	| COUNT_SYM '(' DISTINCT expr_list ')'
	  { $$=new Item_sum_count_distinct(* $4); }
	| GROUP_UNIQUE_USERS '(' text_literal ',' NUM ',' NUM ',' in_sum_expr ')'
	  { $$= new Item_sum_unique_users($3,atoi($5.str),atoi($7.str),$9); }
	| MIN_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_min($3); }
	| MAX_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_max($3); }
	| STD_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_std($3); }
	| VARIANCE_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_variance($3); }
	| SUM_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_sum($3); }
	| GROUP_CONCAT_SYM '(' opt_distinct expr_list opt_gorder_clause opt_gconcat_separator ')'
	  { 
	    $$=new Item_func_group_concat($3,$4,Lex->gorder_list,$6); 
	    $4->empty();
	  };

opt_distinct:
    /* empty */ { $$ = 0; }
    |DISTINCT   { $$ = 1; };

opt_gconcat_separator:
    /* empty */        { $$ = new String(",",1,default_charset_info); }
    |SEPARATOR_SYM text_string  { $$ = $2; };
   

opt_gorder_clause:
    /* empty */ 
      {
        LEX *lex=Lex;
        lex->gorder_list = NULL;
      }
    | order_clause
      {
        LEX *lex=Lex;
        lex->gorder_list= (SQL_LIST*) sql_memdup((char*) &lex->current_select->order_list,sizeof(st_sql_list));
        lex->current_select->order_list.empty();
      };
	  

in_sum_expr:
	opt_all
	{
	  LEX *lex= Lex;
	  if (lex->current_select->inc_in_sum_expr())
	  {
	    send_error(lex->thd, ER_SYNTAX_ERROR);
	    YYABORT;
	  }
	}
	expr
	{
	  Select->select_lex()->in_sum_expr--;
	  $$= $3;
	};

cast_type:
	BINARY			{ $$=ITEM_CAST_BINARY; }
	| CHAR_SYM		{ $$=ITEM_CAST_CHAR; }
	| SIGNED_SYM		{ $$=ITEM_CAST_SIGNED_INT; }
	| SIGNED_SYM INT_SYM	{ $$=ITEM_CAST_SIGNED_INT; }
	| UNSIGNED		{ $$=ITEM_CAST_UNSIGNED_INT; }
	| UNSIGNED INT_SYM	{ $$=ITEM_CAST_UNSIGNED_INT; }
	| DATE_SYM		{ $$=ITEM_CAST_DATE; }
	| TIME_SYM		{ $$=ITEM_CAST_TIME; }
	| DATETIME		{ $$=ITEM_CAST_DATETIME; }
	;

expr_list:
	{ Select->expr_list.push_front(new List<Item>); }
	expr_list2
	{ $$= Select->expr_list.pop(); };

expr_list2:
	expr { Select->expr_list.head()->push_back($1); }
	| expr_list2 ',' expr { Select->expr_list.head()->push_back($3); };

ident_list_arg:
          ident_list          { $$= $1; }
        | '(' ident_list ')'  { $$= $2; };

ident_list:
        { Select->expr_list.push_front(new List<Item>); }
        ident_list2
        { $$= Select->expr_list.pop(); };

ident_list2:
        simple_ident { Select->expr_list.head()->push_back($1); }
        | ident_list2 ',' simple_ident { Select->expr_list.head()->push_back($3); };

opt_expr:
	/* empty */      { $$= NULL; }
	| expr         	 { $$= $1; };

opt_else:
	/* empty */    { $$= NULL; }
	| ELSE expr    { $$= $2; };

when_list:
        { Select->when_list.push_front(new List<Item>); }
	when_list2
	{ $$= Select->when_list.pop(); };

when_list2:
	expr THEN_SYM expr
	  {
	    SELECT_LEX_NODE *sel=Select;
	    sel->when_list.head()->push_back($1);
	    sel->when_list.head()->push_back($3);
	}
	| when_list2 WHEN_SYM expr THEN_SYM expr
	  {
	    SELECT_LEX_NODE *sel=Select;
	    sel->when_list.head()->push_back($3);
	    sel->when_list.head()->push_back($5);
	  };

opt_pad:
	/* empty */ { $$=new Item_string(" ",1,default_charset_info); }
	| expr	    { $$=$1; };

join_table_list:
	'(' join_table_list ')'	{ $$=$2; }
	| join_table		{ $$=$1; }
	| join_table_list ',' join_table_list { $$=$3; }
	| join_table_list normal_join join_table_list { $$=$3; }
	| join_table_list STRAIGHT_JOIN join_table_list
	  { $$=$3 ; $$->straight=1; }
	| join_table_list normal_join join_table_list ON expr
	  { add_join_on($3,$5); $$=$3; }
	| join_table_list normal_join join_table_list
	  USING 
	  {
	    SELECT_LEX *sel= Select->select_lex();
	    sel->db1=$1->db; sel->table1=$1->alias;
	    sel->db2=$3->db; sel->table2=$3->alias;
	  }
	  '(' using_list ')'
	  { add_join_on($3,$7); $$=$3; }

	| join_table_list LEFT opt_outer JOIN_SYM join_table_list ON expr
	  { add_join_on($5,$7); $5->outer_join|=JOIN_TYPE_LEFT; $$=$5; }
	| join_table_list LEFT opt_outer JOIN_SYM join_table_list
	  {
	    SELECT_LEX *sel= Select->select_lex();
	    sel->db1=$1->db; sel->table1=$1->alias;
	    sel->db2=$5->db; sel->table2=$5->alias;
	  }
	  USING '(' using_list ')'
	  { add_join_on($5,$9); $5->outer_join|=JOIN_TYPE_LEFT; $$=$5; }
	| join_table_list NATURAL LEFT opt_outer JOIN_SYM join_table_list
	  { add_join_natural($1,$6); $6->outer_join|=JOIN_TYPE_LEFT; $$=$6; }
	| join_table_list RIGHT opt_outer JOIN_SYM join_table_list ON expr
	  { add_join_on($1,$7); $1->outer_join|=JOIN_TYPE_RIGHT; $$=$1; }
	| join_table_list RIGHT opt_outer JOIN_SYM join_table_list
	  {
	    SELECT_LEX *sel= Select->select_lex();
	    sel->db1=$1->db; sel->table1=$1->alias;
	    sel->db2=$5->db; sel->table2=$5->alias;
	  }
	  USING '(' using_list ')'
	  { add_join_on($1,$9); $1->outer_join|=JOIN_TYPE_RIGHT; $$=$1; }
	| join_table_list NATURAL RIGHT opt_outer JOIN_SYM join_table_list
	  { add_join_natural($6,$1); $1->outer_join|=JOIN_TYPE_RIGHT; $$=$1; }
	| join_table_list NATURAL JOIN_SYM join_table_list
	  { add_join_natural($1,$4); $$=$4; };

normal_join:
	JOIN_SYM		{}
	| INNER_SYM JOIN_SYM	{}
	| CROSS JOIN_SYM	{}
	;

join_table:
	{
	  SELECT_LEX *sel= Select->select_lex();
	  sel->use_index_ptr=sel->ignore_index_ptr=0;
	  sel->table_join_options= 0;
	}
        table_ident opt_table_alias opt_key_definition
	{
	  LEX *lex= Lex;
	  SELECT_LEX_NODE *sel= lex->current_select;
	  if (!($$= sel->add_table_to_list(lex->thd, $2, $3,
					   sel->get_table_join_options(),
					   lex->lock_option,
					   sel->get_use_index(),
					   sel->get_ignore_index())))
	    YYABORT;
	}
	| '{' ident join_table LEFT OUTER JOIN_SYM join_table ON expr '}'
	  { add_join_on($7,$9); $7->outer_join|=JOIN_TYPE_LEFT; $$=$7; }
        | '(' SELECT_SYM select_derived ')' opt_table_alias
	{
	  LEX *lex=Lex;
	  SELECT_LEX_UNIT *unit= lex->current_select->master_unit();
	  lex->current_select= unit->outer_select();
	  if (!($$= lex->current_select->
                add_table_to_list(lex->thd, new Table_ident(unit), $5, 0,
				  TL_READ,(List<String> *)0,
	                          (List<String> *)0)))

	    YYABORT;
	};

select_derived:
        {
	  LEX *lex= Lex;
	  lex->derived_tables= 1;
	  if (((int)lex->sql_command >= (int)SQLCOM_HA_OPEN && 
	       lex->sql_command <= (int)SQLCOM_HA_READ) || lex->sql_command == (int)SQLCOM_KILL)
	  {	
	    send_error(lex->thd, ER_SYNTAX_ERROR);
	    YYABORT;
	  }
	  if (lex->current_select->linkage == GLOBAL_OPTIONS_TYPE ||
              mysql_new_select(lex, 1))
	    YYABORT;
	  mysql_init_select(lex);
	  lex->current_select->linkage= DERIVED_TABLE_TYPE;
	}
        select_options select_item_list opt_select_from union_opt
        ;

opt_outer:
	/* empty */	{}
	| OUTER		{};

opt_key_definition:
	/* empty */	{}
	| USE_SYM    key_usage_list
          {
	    SELECT_LEX *sel= Select->select_lex();
	    sel->use_index= *$2;
	    sel->use_index_ptr= &sel->use_index;
	  }
	| FORCE_SYM key_usage_list
          {
	    SELECT_LEX *sel= Select->select_lex();
	    sel->use_index= *$2;
	    sel->use_index_ptr= &sel->use_index;
	    sel->table_join_options|= TL_OPTION_FORCE_INDEX;
	  }
	| IGNORE_SYM key_usage_list
	  {
	    SELECT_LEX *sel= Select->select_lex();
	    sel->ignore_index= *$2;
	    sel->ignore_index_ptr= &sel->ignore_index;
	  };

key_usage_list:
	key_or_index { Select->select_lex()->interval_list.empty(); }
        '(' key_list_or_empty ')'
        { $$= &Select->select_lex()->interval_list; }
	;

key_list_or_empty:
	/* empty */ 		{}
	| key_usage_list2	{}
	;

key_usage_list2:
	key_usage_list2 ',' ident
        { Select->select_lex()->
	    interval_list.push_back(new String((const char*) $3.str, $3.length,
				    default_charset_info)); }
	| ident
        { Select->select_lex()->
	    interval_list.push_back(new String((const char*) $1.str, $1.length,
				    default_charset_info)); }
	| PRIMARY_SYM
        { Select->select_lex()->
	    interval_list.push_back(new String("PRIMARY", 7,
				    default_charset_info)); };

using_list:
	ident
	  {
	    SELECT_LEX *sel= Select->select_lex();
	    if (!($$= new Item_func_eq(new Item_field(sel->db1, sel->table1,
						      $1.str),
				       new Item_field(sel->db2, sel->table2,
						      $1.str))))
	      YYABORT;
	  }
	| using_list ',' ident
	  {
	    SELECT_LEX *sel= Select->select_lex();
	    if (!($$= new Item_cond_and(new Item_func_eq(new Item_field(sel->db1,sel->table1,$3.str), new Item_field(sel->db2,sel->table2,$3.str)), $1)))
	      YYABORT;
	  };

interval:
	 DAY_HOUR_SYM		{ $$=INTERVAL_DAY_HOUR; }
	| DAY_MINUTE_SYM	{ $$=INTERVAL_DAY_MINUTE; }
	| DAY_SECOND_SYM	{ $$=INTERVAL_DAY_SECOND; }
	| DAY_SYM		{ $$=INTERVAL_DAY; }
	| HOUR_MINUTE_SYM	{ $$=INTERVAL_HOUR_MINUTE; }
	| HOUR_SECOND_SYM	{ $$=INTERVAL_HOUR_SECOND; }
	| HOUR_SYM		{ $$=INTERVAL_HOUR; }
	| MINUTE_SECOND_SYM	{ $$=INTERVAL_MINUTE_SECOND; }
	| MINUTE_SYM		{ $$=INTERVAL_MINUTE; }
	| MONTH_SYM		{ $$=INTERVAL_MONTH; }
	| SECOND_SYM		{ $$=INTERVAL_SECOND; }
	| YEAR_MONTH_SYM	{ $$=INTERVAL_YEAR_MONTH; }
	| YEAR_SYM		{ $$=INTERVAL_YEAR; };

table_alias:
	/* empty */
	| AS
	| EQ;

opt_table_alias:
	/* empty */		{ $$=0; }
	| table_alias ident
	  { $$= (LEX_STRING*) sql_memdup(&$2,sizeof(LEX_STRING)); };

opt_all:
	/* empty */
	| ALL
	;

where_clause:
	/* empty */  { Select->select_lex()->where= 0; }
	| WHERE expr
	  {
	    Select->select_lex()->where= $2;
	    if ($2)
	      $2->top_level_item();
	  }
 	;

having_clause:
	/* empty */
	| HAVING
	  {
	    Select->select_lex()->parsing_place= SELECT_LEX_NODE::IN_HAVING;
          }
	  expr
	  {
	    SELECT_LEX *sel= Select->select_lex();
	    sel->having= $3;
	    sel->parsing_place= SELECT_LEX_NODE::NO_MATTER;
	    if ($3)
	      $3->top_level_item();
	  }
	;

opt_escape:
	ESCAPE_SYM TEXT_STRING_literal	{ $$= $2.str; }
	| /* empty */			{ $$= (char*) "\\"; };


/*
   group by statement in select
*/

group_clause:
	/* empty */
	| GROUP BY group_list olap_opt;

group_list:
	group_list ',' order_ident order_dir
	  { if (add_group_to_list(YYTHD, $3,(bool) $4)) YYABORT; }
	| order_ident order_dir
	  { if (add_group_to_list(YYTHD, $1,(bool) $2)) YYABORT; };

olap_opt:
	/* empty */ {}
	| WITH CUBE_SYM
          {
	    LEX *lex=Lex;
	    if (lex->current_select->linkage == GLOBAL_OPTIONS_TYPE)
	    {
	      net_printf(lex->thd, ER_WRONG_USAGE, "WITH CUBE",
		       "global union parameters");
	      YYABORT;
	    }
	    lex->current_select->select_lex()->olap= CUBE_TYPE;
	    net_printf(lex->thd, ER_NOT_SUPPORTED_YET, "CUBE");
	    YYABORT;	/* To be deleted in 4.1 */
	  }
	| WITH ROLLUP_SYM
          {
	    LEX *lex= Lex;
	    if (lex->current_select->linkage == GLOBAL_OPTIONS_TYPE)
	    {
	      net_printf(lex->thd, ER_WRONG_USAGE, "WITH ROLLUP",
		       "global union parameters");
	      YYABORT;
	    }
	    lex->current_select->select_lex()->olap= ROLLUP_TYPE;
	    net_printf(lex->thd, ER_NOT_SUPPORTED_YET, "ROLLUP");
	    YYABORT;	/* To be deleted in 4.1 */
	  }
	;

/*
   Order by statement in select
*/

opt_order_clause:
	/* empty */
	| order_clause;

order_clause:
	ORDER_SYM BY
        {
	  LEX *lex=Lex;
	  if (lex->current_select->linkage != GLOBAL_OPTIONS_TYPE &&
	      lex->current_select->select_lex()->olap !=
	      UNSPECIFIED_OLAP_TYPE)
	  {
	    net_printf(lex->thd, ER_WRONG_USAGE,
		       "CUBE/ROLLUP",
		       "ORDER BY");
	    YYABORT;
	  }
	} order_list;

order_list:
	order_list ',' order_ident order_dir
	  { if (add_order_to_list(YYTHD, $3,(bool) $4)) YYABORT; }
	| order_ident order_dir
	  { if (add_order_to_list(YYTHD, $1,(bool) $2)) YYABORT; };

order_dir:
	/* empty */ { $$ =  1; }
	| ASC  { $$ =1; }
	| DESC { $$ =0; };


opt_limit_clause_init:
	/* empty */
	{
	  SELECT_LEX_NODE *sel= Select;
          sel->offset_limit= 0L;
          sel->select_limit= Lex->thd->variables.select_limit;
	}
	| limit_clause {}
	;

opt_limit_clause:
	/* empty */	{}
	| limit_clause	{}
	;

limit_clause:
	LIMIT
	  {
	    LEX *lex= Lex;
	    if (lex->current_select->linkage != GLOBAL_OPTIONS_TYPE &&
	        lex->current_select->select_lex()->olap !=
		UNSPECIFIED_OLAP_TYPE)
	    {
	      net_printf(lex->thd, ER_WRONG_USAGE, "CUBE/ROLLUP",
		        "LIMIT");
	      YYABORT;
	    }
	  }
	  limit_options
	  {}
	;

limit_options:
	ULONG_NUM
	  {
            SELECT_LEX_NODE *sel= Select;
            sel->select_limit= $1;
            sel->offset_limit= 0L;
	  }
	| ULONG_NUM ',' ULONG_NUM
	  {
	    SELECT_LEX_NODE *sel= Select;
	    sel->select_limit= $3;
	    sel->offset_limit= $1;
	  }
	| ULONG_NUM OFFSET_SYM ULONG_NUM
	  {
	    SELECT_LEX_NODE *sel= Select;
	    sel->select_limit= $1;
	    sel->offset_limit= $3;
	  }
	;


delete_limit_clause:
	/* empty */
	{
	  LEX *lex=Lex;
	  lex->current_select->select_limit= HA_POS_ERROR;
	}
	| LIMIT ulonglong_num
	{ Select->select_limit= (ha_rows) $2; };

ULONG_NUM:
	NUM	    { $$= strtoul($1.str,NULL,10); }
	| LONG_NUM  { $$= (ulonglong) strtoll($1.str,NULL,10); }
	| ULONGLONG_NUM { $$= (ulong) strtoull($1.str,NULL,10); }
	| REAL_NUM  { $$= strtoul($1.str,NULL,10); }
	| FLOAT_NUM { $$= strtoul($1.str,NULL,10); };

ulonglong_num:
	NUM	    { $$= (ulonglong) strtoul($1.str,NULL,10); }
	| ULONGLONG_NUM { $$= strtoull($1.str,NULL,10); }
	| LONG_NUM  { $$= (ulonglong) strtoll($1.str,NULL,10); }
	| REAL_NUM  { $$= strtoull($1.str,NULL,10); }
	| FLOAT_NUM { $$= strtoull($1.str,NULL,10); };

procedure_clause:
	/* empty */
	| PROCEDURE ident			/* Procedure name */
	  {
	    LEX *lex=Lex;
	    if (&lex->select_lex != lex->current_select)
	    {
	      net_printf(lex->thd, ER_WRONG_USAGE,
			  "PROCEDURE",
			  "subquery");
	      YYABORT;
	    }
	    lex->proc_list.elements=0;
	    lex->proc_list.first=0;
	    lex->proc_list.next= (byte**) &lex->proc_list.first;
	    if (add_proc_to_list(lex->thd, new Item_field(NULL,NULL,$2.str)))
	      YYABORT;
	    Lex->uncacheable();
	  }
	  '(' procedure_list ')';


procedure_list:
	/* empty */ {}
	| procedure_list2 {};

procedure_list2:
	procedure_list2 ',' procedure_item
	| procedure_item;

procedure_item:
	  remember_name expr
	  {
	    LEX *lex= Lex;
	    if (add_proc_to_list(lex->thd, $2))
	      YYABORT;
	    if (!$2->name)
	      $2->set_name($1,(uint) ((char*) lex->tok_end - $1), YYTHD->charset());
	  }
          ;


select_var_list_init:
	   {
             LEX *lex=Lex;
	     if (!lex->describe && (!(lex->result= new select_dumpvar())))
	        YYABORT;
	   }
	   select_var_list
	   {}
           ;

select_var_list:
	   select_var_list ',' select_var_ident
	   | select_var_ident {}
           ;

select_var_ident:  '@' ident_or_text
           {
             LEX *lex=Lex;
	     if (lex->result && ((select_dumpvar *)lex->result)->var_list.push_back((LEX_STRING*) sql_memdup(&$2,sizeof(LEX_STRING))))
	       YYABORT;
	   }
           ;

into:
        INTO OUTFILE TEXT_STRING_sys
	{
	  LEX *lex=Lex;
	  if (!lex->describe)
	  {
	    lex->uncacheable();
	    if (!(lex->exchange= new sql_exchange($3.str,0)))
	      YYABORT;
	    if (!(lex->result= new select_export(lex->exchange)))
	      YYABORT;
	  }
	}
	opt_field_term opt_line_term
	| INTO DUMPFILE TEXT_STRING_sys
	{
	  LEX *lex=Lex;
	  if (!lex->describe)
	  {
	    lex->uncacheable();
	    if (!(lex->exchange= new sql_exchange($3.str,1)))
	      YYABORT;
	    if (!(lex->result= new select_dump(lex->exchange)))
	      YYABORT;
	  }
	}
        | INTO select_var_list_init
	{
	  Lex->uncacheable();
	}
        ;

/*
  DO statement
*/

do:	DO_SYM
	{
	  LEX *lex=Lex;
	  lex->sql_command = SQLCOM_DO;
	  if (!(lex->insert_list = new List_item))
	    YYABORT;
	}
	values
	{}
	;

/*
  Drop : delete tables or index
*/

drop:
	DROP opt_temporary TABLE_SYM if_exists table_list opt_restrict
	{
	  LEX *lex=Lex;
	  lex->sql_command = SQLCOM_DROP_TABLE;
	  lex->drop_temporary= $2;
	  lex->drop_if_exists= $4;
	}
	| DROP INDEX ident ON table_ident {}
	  {
	     LEX *lex=Lex;
	     lex->sql_command= SQLCOM_DROP_INDEX;
	     lex->drop_list.empty();
	     lex->drop_list.push_back(new Alter_drop(Alter_drop::KEY,
						     $3.str));
	     if (!lex->current_select->add_table_to_list(lex->thd, $5, NULL,
							TL_OPTION_UPDATING))
	      YYABORT;
	  }
	| DROP DATABASE if_exists ident
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_DROP_DB;
	    lex->drop_if_exists=$3;
	    lex->name=$4.str;
	 }
	| DROP UDF_SYM IDENT_sys
	  {
	    LEX *lex=Lex;
	    lex->sql_command = SQLCOM_DROP_FUNCTION;
	    lex->udf.name = $3;
	  };


table_list:
	table_name
	| table_list ',' table_name;

table_name:
	table_ident
	{
	  if (!Select->add_table_to_list(YYTHD, $1, NULL, TL_OPTION_UPDATING))
	    YYABORT;
	}
	;

if_exists:
	/* empty */ { $$= 0; }
	| IF EXISTS { $$= 1; }
	;

opt_temporary:
	/* empty */ { $$= 0; }
	| TEMPORARY { $$= 1; }
	;
/*
** Insert : add new data to table
*/

insert:
	INSERT
	{
	  LEX *lex= Lex;
	  lex->sql_command = SQLCOM_INSERT;
	  /* for subselects */
          lex->lock_option= (using_update_log) ? TL_READ_NO_INSERT : TL_READ;
	} insert_lock_option
	opt_ignore insert2
	{
	  Select->set_lock_for_tables($3);
	}
	insert_field_spec opt_insert_update
	{}
	;

replace:
	REPLACE
	{
	  LEX *lex=Lex;
	  lex->sql_command = SQLCOM_REPLACE;
	  lex->duplicates= DUP_REPLACE;
	}
	replace_lock_option insert2
	{
	  Select->set_lock_for_tables($3);
	}
	insert_field_spec
	{}
	{}
	;

insert_lock_option:
	/* empty */	{ $$= TL_WRITE_CONCURRENT_INSERT; }
	| LOW_PRIORITY	{ $$= TL_WRITE_LOW_PRIORITY; }
	| DELAYED_SYM	{ $$= TL_WRITE_DELAYED; }
	| HIGH_PRIORITY { $$= TL_WRITE; }
	;

replace_lock_option:
	opt_low_priority { $$= $1; }
	| DELAYED_SYM	 { $$= TL_WRITE_DELAYED; };

insert2:
	INTO insert_table {}
	| insert_table {};

insert_table:
	table_name
	{
	  LEX *lex=Lex;
	  lex->field_list.empty();
	  lex->many_values.empty();
	  lex->insert_list=0;
	};

insert_field_spec:
	opt_field_spec insert_values {}
	| SET
	  {
	    LEX *lex=Lex;
	    if (!(lex->insert_list = new List_item) ||
		lex->many_values.push_back(lex->insert_list))
	      YYABORT;
	   }
	   ident_eq_list;

opt_field_spec:
	/* empty */	  { }
	| '(' fields ')'  { }
	| '(' ')'	  { };

fields:
	fields ',' insert_ident { Lex->field_list.push_back($3); }
	| insert_ident		{ Lex->field_list.push_back($1); };

insert_values:
	VALUES	values_list  {}
	| VALUE_SYM values_list  {}
	| SELECT_SYM
	  {
	    LEX *lex=Lex;
	    lex->sql_command = (lex->sql_command == SQLCOM_INSERT ?
				SQLCOM_INSERT_SELECT : SQLCOM_REPLACE_SELECT);
	    lex->lock_option= (using_update_log) ? TL_READ_NO_INSERT : TL_READ;
	    mysql_init_select(lex);
	    lex->current_select->parsing_place= SELECT_LEX_NODE::SELECT_LIST;
	  }
	  select_options select_item_list
	  {
	    Select->parsing_place= SELECT_LEX_NODE::NO_MATTER;
	  }
	  opt_select_from select_lock_type
          union_clause {}
	;

values_list:
	values_list ','  no_braces
	| no_braces;

ident_eq_list:
	ident_eq_list ',' ident_eq_value
	|
	ident_eq_value;

ident_eq_value:
	simple_ident equal expr_or_default
	 {
	  LEX *lex=Lex;
	  if (lex->field_list.push_back($1) ||
	      lex->insert_list->push_back($3))
	    YYABORT;
	 };

equal:	EQ		{}
	| SET_VAR	{}
	;

opt_equal:
	/* empty */	{}
	| equal		{}
	;

no_braces:
	 '('
	 {
	    if (!(Lex->insert_list = new List_item))
	      YYABORT;
	 }
	 opt_values ')'
	 {
	  LEX *lex=Lex;
	  if (lex->many_values.push_back(lex->insert_list))
	    YYABORT;
	 };

opt_values:
	/* empty */ {}
	| values;

values:
	values ','  expr_or_default
	{
	  if (Lex->insert_list->push_back($3))
	    YYABORT;
	}
	| expr_or_default
	  {
	    if (Lex->insert_list->push_back($1))
	      YYABORT;
	  }
	;

expr_or_default:
	expr	  { $$= $1;}
	| DEFAULT {$$= new Item_default_value(); }
	;

opt_insert_update:
        /* empty */
        | ON DUPLICATE_SYM
          { /* for simplisity, let's forget about
               INSERT ... SELECT ... UPDATE
               for a moment */
	    if (Lex->sql_command != SQLCOM_INSERT)
            {
	      send_error(Lex->thd, ER_SYNTAX_ERROR);
              YYABORT;
            }
          }
          KEY_SYM UPDATE_SYM update_list
        ;

/* Update rows in a table */

update:
	UPDATE_SYM
	{
	  LEX *lex= Lex;
	  mysql_init_select(lex);
          lex->sql_command= SQLCOM_UPDATE;
        }
        opt_low_priority opt_ignore join_table_list
	SET update_list where_clause opt_order_clause delete_limit_clause
	{
	  LEX *lex= Lex;
	  Select->set_lock_for_tables($3);
          if (lex->select_lex.table_list.elements > 1)
            lex->sql_command= SQLCOM_UPDATE_MULTI;
	}
	;

update_list:
	update_list ',' simple_ident equal expr_or_default
	{
	  if (add_item_to_list(YYTHD, $3) || add_value_to_list(YYTHD, $5))
	    YYABORT;
	}
	| simple_ident equal expr_or_default
	  {
	    if (add_item_to_list(YYTHD, $1) || add_value_to_list(YYTHD, $3))
	      YYABORT;
	  };

opt_low_priority:
	/* empty */	{ $$= YYTHD->update_lock_default; }
	| LOW_PRIORITY	{ $$= TL_WRITE_LOW_PRIORITY; };

/* Delete rows from a table */

delete:
	DELETE_SYM
	{
	  LEX *lex= Lex;
	  lex->sql_command= SQLCOM_DELETE;
	  lex->select_lex.options= 0;
	  lex->lock_option= lex->thd->update_lock_default;
	  lex->select_lex.init_order();
	}
	opt_delete_options single_multi {}
	;

single_multi:
 	FROM table_ident
	{
	  if (!Select->add_table_to_list(YYTHD, $2, NULL, TL_OPTION_UPDATING,
					 Lex->lock_option))
	    YYABORT;
	}
	where_clause opt_order_clause
	delete_limit_clause {}
	| table_wild_list
	  { mysql_init_multi_delete(Lex); }
          FROM join_table_list where_clause
	| FROM table_wild_list
	  { mysql_init_multi_delete(Lex); }
	  USING join_table_list where_clause
	  {}
	;

table_wild_list:
	  table_wild_one {}
	  | table_wild_list ',' table_wild_one {};

table_wild_one:
	ident opt_wild opt_table_alias
	{
	  if (!Select->add_table_to_list(YYTHD, new Table_ident($1), $3,
					 TL_OPTION_UPDATING, Lex->lock_option))
	    YYABORT;
        }
	| ident '.' ident opt_wild opt_table_alias
	  {
	    if (!Select->add_table_to_list(YYTHD,
					   new Table_ident(YYTHD, $1, $3, 0),
					   $5, TL_OPTION_UPDATING,
					   Lex->lock_option))
	      YYABORT;
	  }
	;

opt_wild:
	/* empty */	{}
	| '.' '*'	{};


opt_delete_options:
	/* empty */	{}
	| opt_delete_option opt_delete_options {};

opt_delete_option:
	QUICK		{ Select->options|= OPTION_QUICK; }
	| LOW_PRIORITY	{ Lex->lock_option= TL_WRITE_LOW_PRIORITY; };

truncate:
	TRUNCATE_SYM opt_table_sym table_name
	{
	  LEX* lex= Lex;
	  lex->sql_command= SQLCOM_TRUNCATE;
	  lex->select_lex.options= 0;
	  lex->select_lex.init_order();
	}
	;

opt_table_sym:
	/* empty */
	| TABLE_SYM;

/* Show things */

show:	SHOW
	{
	  LEX *lex=Lex;
	  lex->wild=0;
	  bzero((char*) &lex->create_info,sizeof(lex->create_info));
	}
	show_param
	{}
	;

show_param:
	DATABASES wild
	  { Lex->sql_command= SQLCOM_SHOW_DATABASES; }
	| TABLES opt_db wild
	  {
	    LEX *lex= Lex;
	    lex->sql_command= SQLCOM_SHOW_TABLES;
	    lex->select_lex.db= $2;
	    lex->select_lex.options= 0;
	   }
	| TABLE_SYM STATUS_SYM opt_db wild
	  {
	    LEX *lex= Lex;
	    lex->sql_command= SQLCOM_SHOW_TABLES;
	    lex->select_lex.options|= SELECT_DESCRIBE;
	    lex->select_lex.db= $3;
	  }
	| OPEN_SYM TABLES opt_db wild
	  {
	    LEX *lex= Lex;
	    lex->sql_command= SQLCOM_SHOW_OPEN_TABLES;
	    lex->select_lex.db= $3;
	    lex->select_lex.options= 0;
	  }
	| opt_full COLUMNS from_or_in table_ident opt_db wild
	  {
	    Lex->sql_command= SQLCOM_SHOW_FIELDS;
	    if ($5)
	      $4->change_db($5);
	    if (!Select->add_table_to_list(YYTHD, $4, NULL, 0))
	      YYABORT;
	  }
        | NEW_SYM MASTER_SYM FOR_SYM SLAVE WITH MASTER_LOG_FILE_SYM EQ
	  TEXT_STRING_sys AND MASTER_LOG_POS_SYM EQ ulonglong_num
	  AND MASTER_SERVER_ID_SYM EQ
	ULONG_NUM
          {
	    Lex->sql_command = SQLCOM_SHOW_NEW_MASTER;
	    Lex->mi.log_file_name = $8.str;
	    Lex->mi.pos = $12;
	    Lex->mi.server_id = $16;
          }
        | BINARY LOGS_SYM
          {
	    Lex->sql_command = SQLCOM_SHOW_BINLOGS;
          }
        | SLAVE HOSTS_SYM
          {
	    Lex->sql_command = SQLCOM_SHOW_SLAVE_HOSTS;
          }
        | BINLOG_SYM EVENTS_SYM binlog_in binlog_from
          {
	    LEX *lex= Lex;
	    lex->sql_command= SQLCOM_SHOW_BINLOG_EVENTS;
          } opt_limit_clause_init
	| keys_or_index FROM table_ident opt_db
	  {
	    Lex->sql_command= SQLCOM_SHOW_KEYS;
	    if ($4)
	      $3->change_db($4);
	    if (!Select->add_table_to_list(YYTHD, $3, NULL, 0))
	      YYABORT;
	  }
	| COLUMN_SYM TYPES_SYM
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_SHOW_COLUMN_TYPES;
	  }
	| TABLE_SYM TYPES_SYM
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_SHOW_TABLE_TYPES;
	  }
	| PRIVILEGES
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_SHOW_PRIVILEGES;
	  }
        | COUNT_SYM '(' '*' ')' WARNINGS
          { (void) create_select_for_variable("warning_count"); }
        | COUNT_SYM '(' '*' ')' ERRORS
	  { (void) create_select_for_variable("error_count"); }
        | WARNINGS opt_limit_clause_init
          { Lex->sql_command = SQLCOM_SHOW_WARNS;}
        | ERRORS opt_limit_clause_init
          { Lex->sql_command = SQLCOM_SHOW_ERRORS;}
	| STATUS_SYM wild
	  { Lex->sql_command= SQLCOM_SHOW_STATUS; }
        | INNOBASE_SYM STATUS_SYM
          { Lex->sql_command = SQLCOM_SHOW_INNODB_STATUS;}
	| opt_full PROCESSLIST_SYM
	  { Lex->sql_command= SQLCOM_SHOW_PROCESSLIST;}
	| opt_var_type VARIABLES wild
	  {
	    THD *thd= YYTHD;
	    thd->lex.sql_command= SQLCOM_SHOW_VARIABLES;
	    thd->lex.option_type= (enum_var_type) $1;
	  }
	| charset wild
	  { Lex->sql_command= SQLCOM_SHOW_CHARSETS; }
	| COLLATION_SYM wild
	  { Lex->sql_command= SQLCOM_SHOW_COLLATIONS; }
	| LOGS_SYM
	  { Lex->sql_command= SQLCOM_SHOW_LOGS; }
	| GRANTS FOR_SYM user
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_SHOW_GRANTS;
	    lex->grant_user=$3;
	    lex->grant_user->password.str=NullS;
	  }
	| CREATE DATABASE opt_if_not_exists ident
	  {
	    Lex->sql_command=SQLCOM_SHOW_CREATE_DB;
	    Lex->create_info.options=$3;
	    Lex->name=$4.str;
	  }
        | CREATE TABLE_SYM table_ident
          {
	    Lex->sql_command = SQLCOM_SHOW_CREATE;
	    if (!Select->add_table_to_list(YYTHD, $3, NULL,0))
	      YYABORT;
	  }
        | MASTER_SYM STATUS_SYM
          {
	    Lex->sql_command = SQLCOM_SHOW_MASTER_STAT;
          }
        | SLAVE STATUS_SYM
          {
	    Lex->sql_command = SQLCOM_SHOW_SLAVE_STAT;
          };

opt_db:
	/* empty */  { $$= 0; }
	| from_or_in ident { $$= $2.str; };

wild:
	/* empty */
	| LIKE text_string { Lex->wild= $2; };

opt_full:
	/* empty */ { Lex->verbose=0; }
	| FULL	    { Lex->verbose=1; };

from_or_in:
	FROM
	| IN_SYM;

binlog_in:
	/* empty */ { Lex->mi.log_file_name = 0; }
        | IN_SYM TEXT_STRING_sys { Lex->mi.log_file_name = $2.str; };

binlog_from:
	/* empty */ { Lex->mi.pos = 4; /* skip magic number */ }
        | FROM ulonglong_num { Lex->mi.pos = $2; };


/* A Oracle compatible synonym for show */
describe:
	describe_command table_ident
	{
	  LEX *lex=Lex;
	  lex->wild=0;
	  lex->verbose=0;
	  lex->sql_command=SQLCOM_SHOW_FIELDS;
	  if (!Select->add_table_to_list(lex->thd, $2, NULL,0))
	    YYABORT;
	}
	opt_describe_column {}
	| describe_command { Lex->describe=1; } select
          {
	    LEX *lex=Lex;
	    lex->select_lex.options|= SELECT_DESCRIBE;
	  }
	;

describe_command:
	DESC
	| DESCRIBE;

opt_describe_column:
	/* empty */	{}
	| text_string	{ Lex->wild= $1; }
	| ident
	  { Lex->wild= new String((const char*) $1.str,$1.length,default_charset_info); };


/* flush things */

flush:
	FLUSH_SYM opt_no_write_to_binlog
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_FLUSH; lex->type=0;
          lex->no_write_to_binlog= $2; 
	}
	flush_options
	{}
	;

flush_options:
	flush_options ',' flush_option
	| flush_option;

flush_option:
	table_or_tables	{ Lex->type|= REFRESH_TABLES; } opt_table_list {}
	| TABLES WITH READ_SYM LOCK_SYM { Lex->type|= REFRESH_TABLES | REFRESH_READ_LOCK; }
	| QUERY_SYM CACHE_SYM { Lex->type|= REFRESH_QUERY_CACHE_FREE; }
	| HOSTS_SYM	{ Lex->type|= REFRESH_HOSTS; }
	| PRIVILEGES	{ Lex->type|= REFRESH_GRANT; }
	| LOGS_SYM	{ Lex->type|= REFRESH_LOG; }
	| STATUS_SYM	{ Lex->type|= REFRESH_STATUS; }
        | SLAVE         { Lex->type|= REFRESH_SLAVE; }
        | MASTER_SYM    { Lex->type|= REFRESH_MASTER; }
	| DES_KEY_FILE	{ Lex->type|= REFRESH_DES_KEY_FILE; }
 	| RESOURCES     { Lex->type|= REFRESH_USER_RESOURCES; };

opt_table_list:
	/* empty */  {;}
	| table_list {;};

reset:
	RESET_SYM
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_RESET; lex->type=0;
	} reset_options
	{}
	;

reset_options:
	reset_options ',' reset_option
	| reset_option;

reset_option:
        SLAVE                 { Lex->type|= REFRESH_SLAVE; }
        | MASTER_SYM          { Lex->type|= REFRESH_MASTER; }
	| QUERY_SYM CACHE_SYM { Lex->type|= REFRESH_QUERY_CACHE;};

purge:
	PURGE
	{
	  LEX *lex=Lex;
	  lex->type=0;
	} purge_options
	{}
	;

purge_options:
	LOGS_SYM purge_option
	| MASTER_SYM LOGS_SYM purge_option
	;

purge_option:
        TO_SYM TEXT_STRING_sys
        {
	   Lex->sql_command = SQLCOM_PURGE;
	   Lex->to_log = $2.str;
        }
	| BEFORE_SYM expr
	{
	  if ($2->check_cols(1) || $2->fix_fields(Lex->thd, 0, &$2))
	  {
	    net_printf(Lex->thd, ER_WRONG_ARGUMENTS, "PURGE LOGS BEFORE");
	    YYABORT;	  
	  }
	  Item *tmp= new Item_func_unix_timestamp($2);
	  Lex->sql_command = SQLCOM_PURGE_BEFORE;
	  Lex->purge_time= tmp->val_int();
	}
	;

/* kill threads */

kill:
	KILL_SYM expr
	{
	  LEX *lex=Lex;
	  if ($2->fix_fields(lex->thd, 0, &$2) || $2->check_cols(1))
	  {
	    send_error(lex->thd, ER_SET_CONSTANTS_ONLY);
	    YYABORT;
	  }
          lex->sql_command=SQLCOM_KILL;
	  lex->thread_id= (ulong) $2->val_int();
	};

/* change database */

use:	USE_SYM ident
	{
	  LEX *lex=Lex;
	  lex->sql_command=SQLCOM_CHANGE_DB;
	  lex->select_lex.db= $2.str;
	};

/* import, export of files */

load:	LOAD DATA_SYM load_data_lock opt_local INFILE TEXT_STRING_sys
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_LOAD;
	  lex->lock_option= $3;
	  lex->local_file=  $4;
	  if (!(lex->exchange= new sql_exchange($6.str,0)))
	    YYABORT;
	  lex->field_list.empty();
	}
	opt_duplicate INTO TABLE_SYM table_ident opt_field_term opt_line_term
	opt_ignore_lines opt_field_spec
	{
	  if (!Select->add_table_to_list(YYTHD, $11, NULL, TL_OPTION_UPDATING))
	    YYABORT;
	}
        |
	LOAD TABLE_SYM table_ident FROM MASTER_SYM
        {
	  Lex->sql_command = SQLCOM_LOAD_MASTER_TABLE;
	  if (!Select->add_table_to_list(YYTHD, $3, NULL, TL_OPTION_UPDATING))
	    YYABORT;

        }
        |
	LOAD DATA_SYM FROM MASTER_SYM
        {
	  Lex->sql_command = SQLCOM_LOAD_MASTER_DATA;
        };

opt_local:
	/* empty */	{ $$=0;}
	| LOCAL_SYM	{ $$=1;};

load_data_lock:
	/* empty */	{ $$= YYTHD->update_lock_default; }
	| CONCURRENT	{ $$= TL_WRITE_CONCURRENT_INSERT ; }
	| LOW_PRIORITY	{ $$= TL_WRITE_LOW_PRIORITY; };


opt_duplicate:
	/* empty */	{ Lex->duplicates=DUP_ERROR; }
	| REPLACE	{ Lex->duplicates=DUP_REPLACE; }
	| IGNORE_SYM	{ Lex->duplicates=DUP_IGNORE; };

opt_field_term:
	/* empty */
	| COLUMNS field_term_list;

field_term_list:
	field_term_list field_term
	| field_term;

field_term:
	TERMINATED BY text_string { Lex->exchange->field_term= $3;}
	| OPTIONALLY ENCLOSED BY text_string
	  {
	    LEX *lex=Lex;
	    lex->exchange->enclosed= $4;
	    lex->exchange->opt_enclosed=1;
	  }
	| ENCLOSED BY text_string { Lex->exchange->enclosed= $3;}
	| ESCAPED BY text_string  { Lex->exchange->escaped= $3;};

opt_line_term:
	/* empty */
	| LINES line_term_list;

line_term_list:
	line_term_list line_term
	| line_term;

line_term:
	TERMINATED BY text_string { Lex->exchange->line_term= $3;}
	| STARTING BY text_string { Lex->exchange->line_start= $3;};

opt_ignore_lines:
	/* empty */
	| IGNORE_SYM NUM LINES
	  { Lex->exchange->skip_lines=atol($2.str); };

/* Common definitions */

text_literal:
	TEXT_STRING_literal
	{
	  THD *thd= YYTHD;
	  $$ = new Item_string($1.str,$1.length,thd->variables.collation_connection);
	}
	| NCHAR_STRING
	{ $$=  new Item_string($1.str,$1.length,national_charset_info); }
	| UNDERSCORE_CHARSET TEXT_STRING
	  { $$ = new Item_string($2.str,$2.length,Lex->charset); }
	| text_literal TEXT_STRING_literal
	  { ((Item_string*) $1)->append($2.str,$2.length); }
	;

text_string:
	TEXT_STRING_literal
	{ $$=  new String($1.str,$1.length,YYTHD->variables.collation_connection); }
	| HEX_NUM
	  {
	    Item *tmp = new Item_varbinary($1.str,$1.length);
	    $$= tmp ? tmp->val_str((String*) 0) : (String*) 0;
	  }
	;

param_marker:
        '?'
        {
	  LEX *lex=Lex;
          if (YYTHD->prepare_command)
          {
            lex->param_list.push_back($$=new Item_param((uint)(lex->tok_start-(uchar *)YYTHD->query)));
            lex->param_count++;
          }
          else
          {
            yyerror("You have an error in your SQL syntax");
            YYABORT;
          }
        }
	;

literal:
	text_literal	{ $$ =	$1; }
	| NUM		{ $$ =	new Item_int($1.str, (longlong) strtol($1.str, NULL, 10),$1.length); }
	| LONG_NUM	{ $$ =	new Item_int($1.str, (longlong) strtoll($1.str,NULL,10), $1.length); }
	| ULONGLONG_NUM	{ $$ =	new Item_uint($1.str, $1.length); }
	| REAL_NUM	{ $$ =	new Item_real($1.str, $1.length); }
	| FLOAT_NUM	{ $$ =	new Item_float($1.str, $1.length); }
	| NULL_SYM	{ $$ =	new Item_null();
			  Lex->next_state=MY_LEX_OPERATOR_OR_IDENT;}
	| HEX_NUM	{ $$ =	new Item_varbinary($1.str,$1.length);}
	| UNDERSCORE_CHARSET HEX_NUM
	  { 
	    Item *tmp= new Item_varbinary($2.str,$2.length);
	    String *str= tmp ? tmp->val_str((String*) 0) : (String*) 0;
	    $$ = new Item_string(str ? str->ptr() : "", str ? str->length() : 0, 
				 Lex->charset);
	  }
	| DATE_SYM text_literal { $$ = $2; }
	| TIME_SYM text_literal { $$ = $2; }
	| TIMESTAMP text_literal { $$ = $2; };

/**********************************************************************
** Createing different items.
**********************************************************************/

insert_ident:
	simple_ident	 { $$=$1; }
	| table_wild	 { $$=$1; };

table_wild:
	ident '.' '*' 
	{
	  $$ = new Item_field(NullS,$1.str,"*");
	  Lex->current_select->select_lex()->with_wild++;
	}
	| ident '.' ident '.' '*'
	{
	  $$ = new Item_field((YYTHD->client_capabilities &
   			     CLIENT_NO_SCHEMA ? NullS : $1.str),
			     $3.str,"*");
	  Lex->current_select->select_lex()->with_wild++;
	}
	;

order_ident:
	expr { $$=$1; };

simple_ident:
	ident
	{
	  SELECT_LEX_NODE *sel=Select;
	  $$= (sel->parsing_place != SELECT_LEX_NODE::IN_HAVING ||
	       sel->get_in_sum_expr() > 0) ?
              (Item*) new Item_field(NullS,NullS,$1.str) :
	      (Item*) new Item_ref(NullS,NullS,$1.str);
	}
	| ident '.' ident
	{
	  THD *thd= YYTHD;
	  LEX *lex= &thd->lex;
	  SELECT_LEX_NODE *sel= lex->current_select;
	  if (sel->no_table_names_allowed)
	  {
	    my_printf_error(ER_TABLENAME_NOT_ALLOWED_HERE, 
			    ER(ER_TABLENAME_NOT_ALLOWED_HERE),
			    MYF(0), $1.str, thd->where);
	  }
	  $$= (sel->parsing_place != SELECT_LEX_NODE::IN_HAVING ||
	       sel->get_in_sum_expr() > 0) ?
	      (Item*) new Item_field(NullS,$1.str,$3.str) :
	      (Item*) new Item_ref(NullS,$1.str,$3.str);
	}
	| '.' ident '.' ident
	{
	  THD *thd= YYTHD;
	  LEX *lex= &thd->lex;
	  SELECT_LEX_NODE *sel= lex->current_select;
	  if (sel->no_table_names_allowed)
	  {
	    my_printf_error(ER_TABLENAME_NOT_ALLOWED_HERE, 
			    ER(ER_TABLENAME_NOT_ALLOWED_HERE),
			    MYF(0), $2.str, thd->where);
	  }
	  $$= (sel->parsing_place != SELECT_LEX_NODE::IN_HAVING ||
	       sel->get_in_sum_expr() > 0) ?
	      (Item*) new Item_field(NullS,$2.str,$4.str) :
              (Item*) new Item_ref(NullS,$2.str,$4.str);
	}
	| ident '.' ident '.' ident
	{
	  THD *thd= YYTHD;
	  LEX *lex= &thd->lex;
	  SELECT_LEX_NODE *sel= lex->current_select;
	  if (sel->no_table_names_allowed)
	  {
	    my_printf_error(ER_TABLENAME_NOT_ALLOWED_HERE, 
			    ER(ER_TABLENAME_NOT_ALLOWED_HERE),
			    MYF(0), $3.str, thd->where);
	  }
	  $$= (sel->parsing_place != SELECT_LEX_NODE::IN_HAVING ||
	       sel->get_in_sum_expr() > 0) ?
	      (Item*) new Item_field((YYTHD->client_capabilities &
				      CLIENT_NO_SCHEMA ? NullS : $1.str),
				     $3.str, $5.str) :
	      (Item*) new Item_ref((YYTHD->client_capabilities &
				    CLIENT_NO_SCHEMA ? NullS : $1.str),
                                   $3.str, $5.str);
	};


field_ident:
	ident			{ $$=$1;}
	| ident '.' ident	{ $$=$3;}	/* Skipp schema name in create*/
	| '.' ident		{ $$=$2;}	/* For Delphi */;

table_ident:
	ident			{ $$=new Table_ident($1); }
	| ident '.' ident	{ $$=new Table_ident(YYTHD, $1,$3,0);}
	| '.' ident		{ $$=new Table_ident($2);}
   /* For Delphi */;

IDENT_sys:
	IDENT
	{
	  THD *thd= YYTHD;
	  if (my_charset_same(thd->charset(),system_charset_info))
	  {
	    $$=$1;
	  }
	  else
	  {
	    String ident;
	    ident.copy($1.str,$1.length,thd->charset(),system_charset_info);
	    $$.str= thd->strmake(ident.ptr(),ident.length());
	    $$.length= ident.length();
	  }
	}
	;

TEXT_STRING_sys:
	TEXT_STRING
	{
	  THD *thd= YYTHD;
	  if (my_charset_same(thd->charset(),system_charset_info))
	  {
	    $$=$1;
	  }
	  else
	  {
	    String ident;
	    ident.copy($1.str,$1.length,thd->charset(),system_charset_info);
	    $$.str= thd->strmake(ident.ptr(),ident.length());
	    $$.length= ident.length();
	  }
	}
	;

TEXT_STRING_literal:
	TEXT_STRING
	{
	  THD *thd= YYTHD;
	  if (my_charset_same(thd->charset(),thd->variables.collation_connection))
	  {
	    $$=$1;
	  }
	  else
	  {
	    String ident;
	    ident.copy($1.str,$1.length,thd->charset(),thd->variables.collation_connection);
	    $$.str= thd->strmake(ident.ptr(),ident.length());
	    $$.length= ident.length();
	  }
	}
	;


ident:
	IDENT_sys	    { $$=$1; }
	| keyword
	{
	  LEX *lex= Lex;
	  $$.str= lex->thd->strmake($1.str,$1.length);
	  $$.length=$1.length;
	  if (lex->next_state != MY_LEX_END)
	    lex->next_state= MY_LEX_OPERATOR_OR_IDENT;
	}
	;

ident_or_text:
	ident 			{ $$=$1;}
	| TEXT_STRING_sys	{ $$=$1;}
	| LEX_HOSTNAME		{ $$=$1;};

user:
	ident_or_text
	{
	  THD *thd= YYTHD;
	  if (!($$=(LEX_USER*) thd->alloc(sizeof(st_lex_user))))
	    YYABORT;
	  $$->user = $1; $$->host.str=NullS;
	  }
	| ident_or_text '@' ident_or_text
	  {
	    THD *thd= YYTHD;
	    if (!($$=(LEX_USER*) thd->alloc(sizeof(st_lex_user))))
	      YYABORT;
	    $$->user = $1; $$->host=$3;
	  };

/* Keyword that we allow for identifiers */

keyword:
	ACTION			{}
	| AFTER_SYM		{}
	| AGAINST		{}
	| AGGREGATE_SYM		{}
	| ANY_SYM		{}
	| ASCII_SYM		{}
	| AUTO_INC		{}
	| AVG_ROW_LENGTH	{}
	| AVG_SYM		{}
	| BACKUP_SYM		{}
	| BEGIN_SYM		{}
	| BERKELEY_DB_SYM	{}
	| BINLOG_SYM		{}
	| BIT_SYM		{}
	| BOOL_SYM		{}
	| BOOLEAN_SYM		{}
	| BYTE_SYM		{}
	| CACHE_SYM		{}
	| CHANGED		{}
	| CHARSET		{}
	| CHECKSUM_SYM		{}
	| CIPHER_SYM		{}
	| CLIENT_SYM		{}
	| CLOSE_SYM		{}
	| COLLATION_SYM		{}
	| COMMENT_SYM		{}
	| COMMITTED_SYM		{}
	| COMMIT_SYM		{}
	| COMPRESSED_SYM	{}
	| CONCURRENT		{}
	| CUBE_SYM		{}
	| DATA_SYM		{}
	| DATETIME		{}
	| DATE_SYM		{}
	| DAY_SYM		{}
	| DELAY_KEY_WRITE_SYM	{}
	| DES_KEY_FILE		{}
	| DIRECTORY_SYM		{}
	| DO_SYM		{}
	| DUAL_SYM		{}
	| DUMPFILE		{}
	| DUPLICATE_SYM		{}
	| DYNAMIC_SYM		{}
	| END			{}
	| ENUM			{}
	| ESCAPE_SYM		{}
	| EVENTS_SYM		{}
	| EXECUTE_SYM		{}
	| EXTENDED_SYM		{}
	| FAST_SYM		{}
	| DISABLE_SYM		{}
	| ENABLE_SYM		{}
	| FULL			{}
	| FILE_SYM		{}
	| FIRST_SYM		{}
	| FIXED_SYM		{}
	| FLUSH_SYM		{}
	| GEOMETRY_SYM		{}
	| GEOMETRYCOLLECTION	{}
	| GRANTS		{}
	| GLOBAL_SYM		{}
	| HANDLER_SYM		{}
	| HEAP_SYM		{}
	| HELP_SYM		{}
	| HOSTS_SYM		{}
	| HOUR_SYM		{}
	| IDENTIFIED_SYM	{}
	| INDEXES		{}
	| ISOLATION		{}
	| ISAM_SYM		{}
	| ISSUER_SYM		{}
	| INNOBASE_SYM		{}
	| INSERT_METHOD		{}
	| RELAY_THREAD		{}
	| LAST_SYM		{}
	| LEVEL_SYM		{}
	| LINESTRING		{}
	| LOCAL_SYM		{}
	| LOCKS_SYM		{}
	| LOGS_SYM		{}
	| MAX_ROWS		{}
	| MASTER_SYM		{}
	| MASTER_HOST_SYM	{}
	| MASTER_PORT_SYM	{}
	| MASTER_LOG_FILE_SYM	{}
	| MASTER_LOG_POS_SYM	{}
	| MASTER_USER_SYM	{}
	| MASTER_PASSWORD_SYM	{}
	| MASTER_CONNECT_RETRY_SYM	{}
	| MAX_CONNECTIONS_PER_HOUR	 {}
	| MAX_QUERIES_PER_HOUR	{}
	| MAX_UPDATES_PER_HOUR	{}
	| MEDIUM_SYM		{}
	| MERGE_SYM		{}
	| MEMORY_SYM		{}
	| MINUTE_SYM		{}
	| MIN_ROWS		{}
	| MODIFY_SYM		{}
	| MODE_SYM		{}
	| MONTH_SYM		{}
	| MULTILINESTRING	{}
	| MULTIPOINT		{}
	| MULTIPOLYGON		{}
	| MYISAM_SYM		{}
	| NAMES_SYM		{}
	| NATIONAL_SYM		{}
	| NCHAR_SYM		{}
	| NEXT_SYM		{}
	| NEW_SYM		{}
	| NO_SYM		{}
	| NONE_SYM		{}
	| OFFSET_SYM		{}
	| OPEN_SYM		{}
	| PACK_KEYS_SYM		{}
	| PARTIAL		{}
	| PASSWORD		{}
	| POINT_SYM		{}
	| POLYGON		{}
	| PREV_SYM		{}
	| PROCESS		{}
	| PROCESSLIST_SYM	{}
	| QUERY_SYM		{}
	| QUICK			{}
	| RAID_0_SYM		{}
	| RAID_CHUNKS		{}
	| RAID_CHUNKSIZE	{}
	| RAID_STRIPED_SYM	{}
	| RAID_TYPE		{}
	| RELAY_LOG_FILE_SYM	{}
	| RELAY_LOG_POS_SYM	{}
	| RELOAD		{}
	| REPAIR		{}
	| REPEATABLE_SYM	{}
	| REPLICATION		{}
	| RESET_SYM		{}
	| RESOURCES		{}
	| RESTORE_SYM		{}
	| ROLLBACK_SYM		{}
	| ROLLUP_SYM		{}
	| ROWS_SYM		{}
	| ROW_FORMAT_SYM	{}
	| ROW_SYM		{}
	| SECOND_SYM		{}
	| SERIAL_SYM		{}
	| SERIALIZABLE_SYM	{}
	| SESSION_SYM		{}
	| SIGNED_SYM		{}
	| SIMPLE_SYM		{}
	| SHARE_SYM		{}
	| SHUTDOWN		{}
	| SLAVE			{}
	| SQL_CACHE_SYM		{}
	| SQL_BUFFER_RESULT	{}
	| SQL_NO_CACHE_SYM	{}
	| SQL_THREAD		{}
	| START_SYM		{}
	| STATUS_SYM		{}
	| STOP_SYM		{}
	| STRING_SYM		{}
	| SUBJECT_SYM		{}
	| SUPER_SYM		{}
	| TEMPORARY		{}
	| TEXT_SYM		{}
	| TRANSACTION_SYM	{}
	| TRUNCATE_SYM		{}
	| TIMESTAMP		{}
	| TIME_SYM		{}
	| TYPE_SYM		{}
	| UDF_SYM		{}
	| UNCOMMITTED_SYM	{}
	| UNICODE_SYM		{}
	| USE_FRM		{}
	| VARIABLES		{}
	| VALUE_SYM		{}
	| WORK_SYM		{}
	| YEAR_SYM		{}
	| SOUNDS_SYM            {}
	;

/* Option functions */

set:
	SET opt_option
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_SET_OPTION;
	  lex->option_type=OPT_DEFAULT;
	  lex->var_list.empty();
	}
	option_value_list
	{}
	;

opt_option:
	/* empty */ {}
	| OPTION {};

option_value_list:
	option_type option_value
	| option_value_list ',' option_type option_value;

option_type:
	/* empty */	{}
	| GLOBAL_SYM	{ Lex->option_type= OPT_GLOBAL; }
	| LOCAL_SYM	{ Lex->option_type= OPT_SESSION; }
	| SESSION_SYM	{ Lex->option_type= OPT_SESSION; }
	;

opt_var_type:
	/* empty */	{ $$=OPT_SESSION; }
	| GLOBAL_SYM	{ $$=OPT_GLOBAL; }
	| LOCAL_SYM	{ $$=OPT_SESSION; }
	| SESSION_SYM	{ $$=OPT_SESSION; }
	;

opt_var_ident_type:
	/* empty */		{ $$=OPT_DEFAULT; }
	| GLOBAL_SYM '.'	{ $$=OPT_GLOBAL; }
	| LOCAL_SYM '.'		{ $$=OPT_SESSION; }
	| SESSION_SYM '.'	{ $$=OPT_SESSION; }
	;

option_value:
	'@' ident_or_text equal expr
	{
	  Lex->var_list.push_back(new set_var_user(new Item_func_set_user_var($2,$4)));
	}
	| internal_variable_name equal set_expr_or_default
	  {
	    LEX *lex=Lex;
	    lex->var_list.push_back(new set_var(lex->option_type, $1, $3));
	  }
	| '@' '@' opt_var_ident_type internal_variable_name equal set_expr_or_default
	  {
	    LEX *lex=Lex;
	    lex->var_list.push_back(new set_var((enum_var_type) $3, $4, $6));
	  }
	| TRANSACTION_SYM ISOLATION LEVEL_SYM isolation_types
	  {
	    LEX *lex=Lex;
	    lex->var_list.push_back(new set_var(lex->option_type,
						find_sys_var("tx_isolation"),
						new Item_int((int32) $4)));
	  }
	| charset old_or_new_charset_name_or_default opt_collate
	{
	  THD *thd= YYTHD;
	  LEX *lex= Lex;
	  $2= $2 ? $2: global_system_variables.collation_client;
	  $3= $3 ? $3 : $2;
	  if (!my_charset_same($2,$3))
	  {
	    net_printf(thd,ER_COLLATION_CHARSET_MISMATCH,$3->name,$2->csname);
	    YYABORT;
	  }
	  lex->var_list.push_back(new set_var_collation_client($3,thd->db_charset,$3));
	}
	| NAMES_SYM charset_name_or_default opt_collate
	{
	  THD *thd= YYTHD;
	  LEX *lex= Lex;
	  $2= $2 ? $2 : global_system_variables.collation_client;
	  $3= $3 ? $3 : $2;
	  if (!my_charset_same($2,$3))
	  {
	    net_printf(thd,ER_COLLATION_CHARSET_MISMATCH,$3->name,$2->csname);
	    YYABORT;
	  }
	  lex->var_list.push_back(new set_var_collation_client($3,$3,$3));
	}
	| PASSWORD equal text_or_password
	  {
	    THD *thd=YYTHD;
	    LEX_USER *user;
	    if (!(user=(LEX_USER*) thd->alloc(sizeof(LEX_USER))))
	      YYABORT;
	    user->host.str=0;
	    user->user.str=thd->priv_user;
	    thd->lex.var_list.push_back(new set_var_password(user, $3));
	  }
	| PASSWORD FOR_SYM user equal text_or_password
	  {
	    Lex->var_list.push_back(new set_var_password($3,$5));
	  }
	;

internal_variable_name:
	ident
	{
	  sys_var *tmp=find_sys_var($1.str, $1.length);
	  if (!tmp)
	    YYABORT;
	  $$=tmp;
	}
        ;

isolation_types:
	READ_SYM UNCOMMITTED_SYM	{ $$= ISO_READ_UNCOMMITTED; }
	| READ_SYM COMMITTED_SYM	{ $$= ISO_READ_COMMITTED; }
	| REPEATABLE_SYM READ_SYM	{ $$= ISO_REPEATABLE_READ; }
	| SERIALIZABLE_SYM		{ $$= ISO_SERIALIZABLE; }
	;

text_or_password:
	TEXT_STRING { $$=$1.str;}
	| PASSWORD '(' TEXT_STRING ')'
	  {
	    if (!$3.length)
	      $$=$3.str;
	    else
	    {
	      char *buff=(char*) YYTHD->alloc(HASH_PASSWORD_LENGTH+1);
	      make_scrambled_password(buff,$3.str,use_old_passwords,
				      &YYTHD->rand);
	      $$=buff;
	    }
	  }
          ;


set_expr_or_default:
	expr      { $$=$1; }
	| DEFAULT { $$=0; }
	| ON	  { $$=new Item_string("ON",  2, system_charset_info); }
	| ALL	  { $$=new Item_string("ALL", 3, system_charset_info); }
	| BINARY  { $$=new Item_string("binary", 6, system_charset_info); }
	;


/* Lock function */

lock:
	LOCK_SYM table_or_tables
	{
	  Lex->sql_command=SQLCOM_LOCK_TABLES;
	}
	table_lock_list
	{}
	;

table_or_tables:
	TABLE_SYM
	| TABLES;

table_lock_list:
	table_lock
	| table_lock_list ',' table_lock;

table_lock:
	table_ident opt_table_alias lock_option
	{
	  if (!Select->add_table_to_list(YYTHD, $1, $2, 0, (thr_lock_type) $3))
	   YYABORT;
	}
        ;

lock_option:
	READ_SYM	{ $$=TL_READ_NO_INSERT; }
	| WRITE_SYM     { $$=YYTHD->update_lock_default; }
	| LOW_PRIORITY WRITE_SYM { $$=TL_WRITE_LOW_PRIORITY; }
	| READ_SYM LOCAL_SYM { $$= TL_READ; }
        ;

unlock:
	UNLOCK_SYM table_or_tables { Lex->sql_command=SQLCOM_UNLOCK_TABLES; }
        ;


/*
** Handler: direct access to ISAM functions
*/

handler:
	HANDLER_SYM table_ident OPEN_SYM opt_table_alias
	{
	  LEX *lex= Lex;
	  lex->sql_command = SQLCOM_HA_OPEN;
	  if (!lex->current_select->add_table_to_list(lex->thd, $2, $4, 0))
	    YYABORT;
	}
	| HANDLER_SYM table_ident CLOSE_SYM
	{
	  LEX *lex= Lex;
	  lex->sql_command = SQLCOM_HA_CLOSE;
	  if (!lex->current_select->add_table_to_list(lex->thd, $2, 0, 0))
	    YYABORT;
	}
	| HANDLER_SYM table_ident READ_SYM
	{
	  LEX *lex=Lex;
	  lex->sql_command = SQLCOM_HA_READ;
	  lex->ha_rkey_mode= HA_READ_KEY_EXACT;	/* Avoid purify warnings */
	  lex->current_select->select_limit= 1;
	  lex->current_select->offset_limit= 0L;
	  if (!lex->current_select->add_table_to_list(lex->thd, $2, 0, 0))
	    YYABORT;
        }
        handler_read_or_scan where_clause opt_limit_clause {}
        ;

handler_read_or_scan:
	handler_scan_function         { Lex->backup_dir= 0; }
        | ident handler_rkey_function { Lex->backup_dir= $1.str; }
        ;

handler_scan_function:
	FIRST_SYM  { Lex->ha_read_mode = RFIRST; }
	| NEXT_SYM { Lex->ha_read_mode = RNEXT;  }
        ;

handler_rkey_function:
	FIRST_SYM  { Lex->ha_read_mode = RFIRST; }
	| NEXT_SYM { Lex->ha_read_mode = RNEXT;  }
	| PREV_SYM { Lex->ha_read_mode = RPREV;  }
	| LAST_SYM { Lex->ha_read_mode = RLAST;  }
	| handler_rkey_mode
	{
	  LEX *lex=Lex;
	  lex->ha_read_mode = RKEY;
	  lex->ha_rkey_mode=$1;
	  if (!(lex->insert_list = new List_item))
	    YYABORT;
	} '(' values ')' { }
        ;

handler_rkey_mode:
	  EQ     { $$=HA_READ_KEY_EXACT;   }
	| GE     { $$=HA_READ_KEY_OR_NEXT; }
	| LE     { $$=HA_READ_KEY_OR_PREV; }
	| GT_SYM { $$=HA_READ_AFTER_KEY;   }
	| LT     { $$=HA_READ_BEFORE_KEY;  }
        ;

/* GRANT / REVOKE */

revoke:
	REVOKE
	{
	  LEX *lex=Lex;
	  lex->sql_command = SQLCOM_REVOKE;
	  lex->users_list.empty();
	  lex->columns.empty();
	  lex->grant= lex->grant_tot_col=0;
	  lex->select_lex.db=0;
	  lex->ssl_type= SSL_TYPE_NOT_SPECIFIED;
	  lex->ssl_cipher= lex->x509_subject= lex->x509_issuer= 0;
	  bzero((char*) &lex->mqh, sizeof(lex->mqh));
	}
	grant_privileges ON opt_table FROM user_list
	{}
	;

grant:
	GRANT
	{
	  LEX *lex=Lex;
	  lex->users_list.empty();
	  lex->columns.empty();
	  lex->sql_command = SQLCOM_GRANT;
	  lex->grant= lex->grant_tot_col= 0;
	  lex->select_lex.db= 0;
	  lex->ssl_type= SSL_TYPE_NOT_SPECIFIED;
	  lex->ssl_cipher= lex->x509_subject= lex->x509_issuer= 0;
	  bzero(&(lex->mqh),sizeof(lex->mqh));
	}
	grant_privileges ON opt_table TO_SYM user_list
	require_clause grant_options
	{}
	;

grant_privileges:
	grant_privilege_list {}
	| ALL PRIVILEGES	{ Lex->grant = GLOBAL_ACLS;}
	| ALL			{ Lex->grant = GLOBAL_ACLS;}
        ;

grant_privilege_list:
	grant_privilege
	| grant_privilege_list ',' grant_privilege;

grant_privilege:
	SELECT_SYM 	{ Lex->which_columns = SELECT_ACL;} opt_column_list {}
	| INSERT	{ Lex->which_columns = INSERT_ACL;} opt_column_list {}
	| UPDATE_SYM	{ Lex->which_columns = UPDATE_ACL; } opt_column_list {}
	| REFERENCES	{ Lex->which_columns = REFERENCES_ACL;} opt_column_list {}
	| DELETE_SYM	{ Lex->grant |= DELETE_ACL;}
	| USAGE		{}
	| INDEX		{ Lex->grant |= INDEX_ACL;}
	| ALTER		{ Lex->grant |= ALTER_ACL;}
	| CREATE	{ Lex->grant |= CREATE_ACL;}
	| DROP		{ Lex->grant |= DROP_ACL;}
	| EXECUTE_SYM	{ Lex->grant |= EXECUTE_ACL;}
	| RELOAD	{ Lex->grant |= RELOAD_ACL;}
	| SHUTDOWN	{ Lex->grant |= SHUTDOWN_ACL;}
	| PROCESS	{ Lex->grant |= PROCESS_ACL;}
	| FILE_SYM	{ Lex->grant |= FILE_ACL;}
	| GRANT OPTION  { Lex->grant |= GRANT_ACL;}
	| SHOW DATABASES { Lex->grant |= SHOW_DB_ACL;}
	| SUPER_SYM	{ Lex->grant |= SUPER_ACL;}
	| CREATE TEMPORARY TABLES { Lex->grant |= CREATE_TMP_ACL;}
	| LOCK_SYM TABLES   { Lex->grant |= LOCK_TABLES_ACL; }
	| REPLICATION SLAVE  { Lex->grant |= REPL_SLAVE_ACL;}
	| REPLICATION CLIENT_SYM { Lex->grant |= REPL_CLIENT_ACL;}
	;


opt_and:
	/* empty */	{}
	| AND		{}
	;

require_list:
	 require_list_element opt_and require_list
	 | require_list_element
	 ;

require_list_element:
	SUBJECT_SYM TEXT_STRING
	{
	  LEX *lex=Lex;
	  if (lex->x509_subject)
	  {
	    net_printf(lex->thd,ER_DUP_ARGUMENT, "SUBJECT");
	    YYABORT;
	  }
	  lex->x509_subject=$2.str;
	}
	| ISSUER_SYM TEXT_STRING
	{
	  LEX *lex=Lex;
	  if (lex->x509_issuer)
	  {
	    net_printf(lex->thd,ER_DUP_ARGUMENT, "ISSUER");
	    YYABORT;
	  }
	  lex->x509_issuer=$2.str;
	}
	| CIPHER_SYM TEXT_STRING
	{
	  LEX *lex=Lex;
	  if (lex->ssl_cipher)
	  {
	    net_printf(lex->thd,ER_DUP_ARGUMENT, "CIPHER");
	    YYABORT;
	  }
	  lex->ssl_cipher=$2.str;
	}
	;

opt_table:
	'*'
	  {
	    LEX *lex= Lex;
	    lex->current_select->select_lex()->db= lex->thd->db;
	    if (lex->grant == GLOBAL_ACLS)
	      lex->grant = DB_ACLS & ~GRANT_ACL;
	    else if (lex->columns.elements)
	    {
	      send_error(lex->thd,ER_ILLEGAL_GRANT_FOR_TABLE);
	      YYABORT;
	    }
	  }
	| ident '.' '*'
	  {
	    LEX *lex= Lex;
	    lex->current_select->select_lex()->db = $1.str;
	    if (lex->grant == GLOBAL_ACLS)
	      lex->grant = DB_ACLS & ~GRANT_ACL;
	    else if (lex->columns.elements)
	    {
	      send_error(lex->thd,ER_ILLEGAL_GRANT_FOR_TABLE);
	      YYABORT;
	    }
	  }
	| '*' '.' '*'
	  {
	    LEX *lex= Lex;
	    lex->current_select->select_lex()->db = NULL;
	    if (lex->grant == GLOBAL_ACLS)
	      lex->grant= GLOBAL_ACLS & ~GRANT_ACL;
	    else if (lex->columns.elements)
	    {
	      send_error(lex->thd,ER_ILLEGAL_GRANT_FOR_TABLE);
	      YYABORT;
	    }
	  }
	| table_ident
	  {
	    LEX *lex=Lex;
	    if (!lex->current_select->add_table_to_list(lex->thd, $1,NULL,0))
	      YYABORT;
	    if (lex->grant == GLOBAL_ACLS)
	      lex->grant =  TABLE_ACLS & ~GRANT_ACL;
	  }
          ;


user_list:
	grant_user  { if (Lex->users_list.push_back($1)) YYABORT;}
	| user_list ',' grant_user
	  {
	    if (Lex->users_list.push_back($3))
	      YYABORT;
	  }
	;


grant_user:
	user IDENTIFIED_SYM BY TEXT_STRING
	{
	   $$=$1; $1->password=$4;
	   if ($4.length)
	   {
	     char *buff=(char*) YYTHD->alloc(HASH_PASSWORD_LENGTH+1);
	     if (buff)
	     {
	       make_scrambled_password(buff,$4.str,use_old_passwords,
				       &YYTHD->rand);
	       $1->password.str=buff;
	       $1->password.length=HASH_PASSWORD_LENGTH;
	     }
	  }
	}
	| user IDENTIFIED_SYM BY PASSWORD TEXT_STRING
	  { $$=$1; $1->password=$5 ; }
	| user
	  { $$=$1; $1->password.str=NullS; }
        ;


opt_column_list:
	/* empty */
	{
	  LEX *lex=Lex;
	  lex->grant |= lex->which_columns;
	}
	| '(' column_list ')';

column_list:
	column_list ',' column_list_id
	| column_list_id;

column_list_id:
	ident
	{
	  String *new_str = new String((const char*) $1.str,$1.length,default_charset_info);
	  List_iterator <LEX_COLUMN> iter(Lex->columns);
	  class LEX_COLUMN *point;
	  LEX *lex=Lex;
	  while ((point=iter++))
	  {
	    if (!my_strcasecmp(system_charset_info,
                               point->column.ptr(), new_str->ptr()))
		break;
	  }
	  lex->grant_tot_col|= lex->which_columns;
	  if (point)
	    point->rights |= lex->which_columns;
	  else
	    lex->columns.push_back(new LEX_COLUMN (*new_str,lex->which_columns));
	}
        ;


require_clause: /* empty */
        | REQUIRE_SYM require_list
          {
            Lex->ssl_type=SSL_TYPE_SPECIFIED;
          }
        | REQUIRE_SYM SSL_SYM
          {
            Lex->ssl_type=SSL_TYPE_ANY;
          }
        | REQUIRE_SYM X509_SYM
          {
            Lex->ssl_type=SSL_TYPE_X509;
          }
	| REQUIRE_SYM NONE_SYM
	  {
	    Lex->ssl_type=SSL_TYPE_NONE;
	  }
          ;

grant_options:
	/* empty */ {}
	| WITH grant_option_list;

grant_option_list:
	grant_option_list grant_option {}
	| grant_option {}
        ;

grant_option:
	GRANT OPTION { Lex->grant |= GRANT_ACL;}
        | MAX_QUERIES_PER_HOUR ULONG_NUM
        {
	  Lex->mqh.questions=$2;
	  Lex->mqh.bits |= 1;
	}
        | MAX_UPDATES_PER_HOUR ULONG_NUM
        {
	  Lex->mqh.updates=$2;
	  Lex->mqh.bits |= 2;
	}
        | MAX_CONNECTIONS_PER_HOUR ULONG_NUM
        {
	  Lex->mqh.connections=$2;
	  Lex->mqh.bits |= 4;
	}
        ;

begin:
	BEGIN_SYM   { Lex->sql_command = SQLCOM_BEGIN;} opt_work {}
	;

opt_work:
	/* empty */ {}
	| WORK_SYM {;}
        ;

commit:
	COMMIT_SYM   { Lex->sql_command = SQLCOM_COMMIT;};

rollback:
	ROLLBACK_SYM { Lex->sql_command = SQLCOM_ROLLBACK;};


/*
   UNIONS : glue selects together
*/


union_clause:
	/* empty */ {}
	| union_list
	;

union_list:
	UNION_SYM    union_option
	{
	  LEX *lex=Lex;
	  if (lex->exchange)
	  {
	    /* Only the last SELECT can have  INTO...... */
	    net_printf(lex->thd, ER_WRONG_USAGE, "UNION", "INTO");
	    YYABORT;
	  }
	  if (lex->current_select->linkage == GLOBAL_OPTIONS_TYPE)
	  {
	    send_error(lex->thd, ER_SYNTAX_ERROR);
	    YYABORT;
	  }
	  if (mysql_new_select(lex, 0))
	    YYABORT;
          mysql_init_select(lex);
	  lex->current_select->linkage=UNION_TYPE;
	}
	select_init {}
	;

union_opt:
	union_list {}
	| optional_order_or_limit {}
	;

optional_order_or_limit:
      	/* Empty */ {}
	|
	  {
	    THD *thd= YYTHD;
	    LEX *lex= &thd->lex;
	    DBUG_ASSERT(lex->current_select->linkage != GLOBAL_OPTIONS_TYPE);
	    SELECT_LEX *sel= lex->current_select->select_lex();
	    SELECT_LEX_UNIT *unit= sel->master_unit();
	    unit->global_parameters= unit;
	    unit->no_table_names_allowed= 1;
	    lex->current_select= unit;
	    thd->where= "global ORDER clause";
	  }
	order_or_limit
          {
	    THD *thd= YYTHD;
	    thd->lex.current_select->no_table_names_allowed= 0;
	    thd->where= "";
          }
	;

order_or_limit:
	order_clause opt_limit_clause_init
	| limit_clause
	;

union_option:
	/* empty */ {}
	| ALL {Select->master_unit()->union_option= 1;};

singlerow_subselect:
	subselect_start singlerow_subselect_init
	subselect_end
	{
	  $$= $2;
	};

singlerow_subselect_init:
	select_init2
	{
	  $$= new Item_singlerow_subselect(YYTHD,
					   Lex->current_select->master_unit()->
                                             first_select());
	};

exists_subselect:
	subselect_start exists_subselect_init
	subselect_end
	{
	  $$= $2;
	};

exists_subselect_init:
	select_init2
	{
	  $$= new Item_exists_subselect(YYTHD,
					Lex->current_select->master_unit()->
					  first_select());
	};

in_subselect:
  subselect_start in_subselect_init
  subselect_end
  {
    $$= $2;
  };

in_subselect_init:
  select_init2
  {
    $$= Lex->current_select->master_unit()->first_select();
  };

subselect_start:
	'(' SELECT_SYM
	{
	  LEX *lex=Lex;
	  if (((int)lex->sql_command >= (int)SQLCOM_HA_OPEN &&
	       lex->sql_command <= (int)SQLCOM_HA_READ) || lex->sql_command == (int)SQLCOM_KILL)	  {	
	    send_error(lex->thd, ER_SYNTAX_ERROR);
	    YYABORT;
	  }
	  if (mysql_new_select(Lex, 1))
	    YYABORT;
	};

subselect_end:
	')'
	{
	  LEX *lex=Lex;
	  lex->current_select = lex->current_select->return_after_parsing();
	};
