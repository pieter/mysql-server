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

/*
  The following hack is needed because mysql_yacc.cc does not define
  YYSTYPE before including this file
*/

#include "set_var.h"

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
  SQLCOM_SHOW_INNODB_STATUS,
  SQLCOM_SHOW_PROCESSLIST, SQLCOM_SHOW_MASTER_STAT, SQLCOM_SHOW_SLAVE_STAT,
  SQLCOM_SHOW_GRANTS, SQLCOM_SHOW_CREATE, SQLCOM_SHOW_CHARSETS,
  SQLCOM_SHOW_CREATE_DB,

  SQLCOM_LOAD,SQLCOM_SET_OPTION,SQLCOM_LOCK_TABLES,SQLCOM_UNLOCK_TABLES,
  SQLCOM_GRANT, 
  SQLCOM_CHANGE_DB, SQLCOM_CREATE_DB, SQLCOM_DROP_DB, SQLCOM_ALTER_DB,
  SQLCOM_REPAIR, SQLCOM_REPLACE, SQLCOM_REPLACE_SELECT, 
  SQLCOM_CREATE_FUNCTION, SQLCOM_DROP_FUNCTION,
  SQLCOM_REVOKE,SQLCOM_OPTIMIZE, SQLCOM_CHECK,
  SQLCOM_FLUSH, SQLCOM_KILL,  SQLCOM_ANALYZE,
  SQLCOM_ROLLBACK, SQLCOM_COMMIT, SQLCOM_SLAVE_START, SQLCOM_SLAVE_STOP,
  SQLCOM_BEGIN, SQLCOM_LOAD_MASTER_TABLE, SQLCOM_CHANGE_MASTER,
  SQLCOM_RENAME_TABLE, SQLCOM_BACKUP_TABLE, SQLCOM_RESTORE_TABLE,
  SQLCOM_RESET, SQLCOM_PURGE, SQLCOM_SHOW_BINLOGS,
  SQLCOM_SHOW_OPEN_TABLES, SQLCOM_LOAD_MASTER_DATA,
  SQLCOM_HA_OPEN, SQLCOM_HA_CLOSE, SQLCOM_HA_READ,
  SQLCOM_SHOW_SLAVE_HOSTS, SQLCOM_DELETE_MULTI, SQLCOM_MULTI_UPDATE,
  SQLCOM_SHOW_BINLOG_EVENTS, SQLCOM_SHOW_NEW_MASTER, SQLCOM_DO,
  SQLCOM_SHOW_WARNS, SQLCOM_EMPTY_QUERY, SQLCOM_SHOW_ERRORS,
  SQLCOM_SHOW_COLUMN_TYPES, SQLCOM_SHOW_TABLE_TYPES, SQLCOM_SHOW_PRIVILEGES,
  SQLCOM_END, SQLCOM_HELP,
};

enum lex_states
{
  STATE_START, STATE_CHAR, STATE_IDENT, STATE_IDENT_SEP, STATE_IDENT_START,
  STATE_FOUND_IDENT, STATE_SIGNED_NUMBER, STATE_REAL, STATE_HEX_NUMBER,
  STATE_CMP_OP, STATE_LONG_CMP_OP, STATE_STRING, STATE_COMMENT, STATE_END,
  STATE_OPERATOR_OR_IDENT, STATE_NUMBER_IDENT, STATE_INT_OR_REAL,
  STATE_REAL_OR_POINT, STATE_BOOL, STATE_EOL, STATE_ESCAPE, STATE_LONG_COMMENT,
  STATE_END_LONG_COMMENT, STATE_COLON, STATE_SET_VAR, STATE_USER_END,
  STATE_HOSTNAME, STATE_SKIP, STATE_USER_VARIABLE_DELIMITER, STATE_SYSTEM_VAR,
  STATE_IDENT_OR_KEYWORD
};


typedef List<Item> List_item;

typedef struct st_lex_master_info
{
  char *host, *user, *password, *log_file_name;
  uint port, connect_retry;
  ulonglong pos;
  ulong server_id;
  char *relay_log_name;
  ulong relay_log_pos;
} LEX_MASTER_INFO;


enum sub_select_type
{
  UNSPECIFIED_TYPE,UNION_TYPE, INTERSECT_TYPE,
  EXCEPT_TYPE, GLOBAL_OPTIONS_TYPE, DERIVED_TABLE_TYPE, OLAP_TYPE
};

enum olap_type 
{
  UNSPECIFIED_OLAP_TYPE, CUBE_TYPE, ROLLUP_TYPE
};

/* 
  The state of the lex parsing for selects 
   
   All select describing structures linked with following pointers:
   - list of neighbors (next/prev) (prev of first element point to slave 
     pointer of upper structure)
     - one level units for unit (union) structure
     - member of one union(unit) for ordinary select_lex
   - pointer to master
     - outer select_lex for unit (union)
     - unit structure for ordinary select_lex
   - pointer to slave
     - first list element of select_lex belonged to this unit for unit
     - first unit in list of units that belong to this select_lex (as
       subselects or derived tables) for ordinary select_lex
   - list of all select_lex (for group operation like correcting list of opened
     tables)
   for example for following query:

   select *
     from table1
     where table1.field IN (select * from table1_1_1 union
                            select * from table1_1_2)
     union
   select *
     from table2
     where table2.field=(select (select f1 from table2_1_1_1_1
                                   where table2_1_1_1_1.f2=table2_1_1.f3)
                           from table2_1_1
                           where table2_1_1.f1=table2.f2)
     union
   select * from table3;

   we will have following structure:


     main unit
     select1 select2 select3
     |^^     |^
    s|||     ||master
    l|||     |+---------------------------------+
    a|||     +---------------------------------+|
    v|||master                         slave   ||
    e||+-------------------------+             ||
     V|            neighbor      |             V|
     unit 1.1<==================>unit1.2       unit2.1
     select1.1.1 select 1.1.2    select1.2.1   select2.1.1 select2.1.2
                                               |^
                                               ||
                                               V|
                                               unit2.1.1.1
                                               select2.1.1.1.1


   relation in main unit will be following:
                          
         main unit
         |^^^
         ||||
         |||+------------------------------+
         ||+--------------+                |
    slave||master         |                |
         V|      neighbor |       neighbor |
         select1<========>select2<========>select3

    list of all select_lex will be following (as it will be constructed by
    parser):

    select1->select2->select3->select2.1.1->select 2.1.2->select2.1.1.1.1-+
                                                                          |
    +---------------------------------------------------------------------+
    |
    +->select1.1.1->select1.1.2

*/

/* 
    Base class for st_select_lex (SELECT_LEX) & 
    st_select_lex_unit (SELECT_LEX_UNIT)
*/
class st_select_lex_node {
protected:
  st_select_lex_node *next, **prev,   /* neighbor list */
    *master, *slave,                  /* vertical links */
    *link_next, **link_prev;          /* list of whole SELECT_LEX */
public:
  ulong options;
  enum sub_select_type linkage;
  SQL_LIST order_list;                /* ORDER clause */
  ha_rows select_limit, offset_limit; /* LIMIT clause parameters */
  void init_query();
  void init_select();
  void include_down(st_select_lex_node *upper);
  void include_neighbour(st_select_lex_node *before);
  void include_global(st_select_lex_node **plink);
  void exclude();
private:
  void fast_exclude();
};

/* 
   SELECT_LEX_UNIT - unit of selects (UNION, INTERSECT, ...) group 
   SELECT_LEXs
*/
struct st_lex;
class st_select_lex;
class THD;
class select_result;
class JOIN;
class select_union;
class st_select_lex_unit: public st_select_lex_node {
protected:
  List<Item> item_list; 
  List<JOIN*> joins; /* list of *JOINs, to delete it in cleanup() */
  TABLE_LIST result_table_list;
  select_union *union_result;
  TABLE *table; /* temporary table using for appending UNION results */
  THD *thd;
  select_result *result;
  int res;
  bool describe, found_rows_for_union,
    prepared, // prepare phase already performed for UNION (unit)
    optimized, // optimize phase already performed for UNION (unit)
    executed; // already executed
public:
  /*
    Pointer to 'last' select or pointer to unit where stored
    global parameters for union
  */
  st_select_lex_node *global_parameters;
  /* LIMIT clause runtime counters */
  ha_rows select_limit_cnt, offset_limit_cnt;
  bool depended; /* depended from outer select subselect */
  /* not NULL if union used in subselect, point to subselect item */
  Item_subselect *item;
  uint union_option;

  void init_query();
  bool create_total_list(THD *thd, st_lex *lex, TABLE_LIST **result);
  st_select_lex* outer_select() { return (st_select_lex*) master; }
  st_select_lex* first_select() { return (st_select_lex*) slave; }
  st_select_lex_unit* next_unit() { return (st_select_lex_unit*) next; }

  /* UNION methods */
  int prepare(THD *thd, select_result *result);
  int exec();
  int cleanup();
  
  friend void mysql_init_query(THD *thd);
private:
  bool create_total_list_n_last_return(THD *thd, st_lex *lex,
				       TABLE_LIST ***result);
};
typedef class st_select_lex_unit SELECT_LEX_UNIT;

/*
  SELECT_LEX - store information of parsed SELECT_LEX statment
*/
class st_select_lex: public st_select_lex_node
{
public:
  char *db, *db1, *table1, *db2, *table2;      	/* For outer join using .. */
  Item *where, *having;                         /* WHERE & HAVING clauses */
  enum olap_type olap;
  List<List_item>     expr_list;
  List<List_item>     when_list;                /* WHEN clause */
  SQL_LIST	      table_list, group_list;   /* FROM & GROUP BY clauses */
  List<Item>          item_list; /* list of fields & expressions */
  List<String>        interval_list, use_index, *use_index_ptr,
		      ignore_index, *ignore_index_ptr;
  /* 
    Usualy it is pointer to ftfunc_list_alloc, but in union used to create fake
    select_lex for calling mysql_select under results of union
  */
  List<Item_func_match> *ftfunc_list;
  List<Item_func_match> ftfunc_list_alloc;
  JOIN *join; /* after JOIN::prepare it is pointer to corresponding JOIN */
  const char *type; /* type of select for EXPLAIN */
  uint in_sum_expr;
  uint select_number; /* number of select (used for EXPLAIN) */
  bool	create_refs;
  bool  braces;   	/* SELECT ... UNION (SELECT ... ) <- this braces */
  bool depended;	/* depended from outer select subselect */
  /* TRUE when having fix field called in processing of this SELECT */
  bool having_fix_field;

  void init_query();
  void init_select();
  st_select_lex_unit* master_unit() { return (st_select_lex_unit*) master; }
  st_select_lex_unit* first_inner_unit() 
  { 
    return (st_select_lex_unit*) slave; 
  }
  st_select_lex* outer_select() 
  { 
    return (st_select_lex*) master_unit()->outer_select(); 
  }
  st_select_lex* next_select() { return (st_select_lex*) next; }
  st_select_lex*  next_select_in_list() 
  {
    return (st_select_lex*) link_next;
  }
  st_select_lex_node** next_select_in_list_addr()
  {
    return &link_next;
  }

  friend void mysql_init_query(THD *thd);
};
typedef class st_select_lex SELECT_LEX;


/* The state of the lex parsing. This is saved in the THD struct */

typedef struct st_lex
{
  uint	 yylineno,yytoklen;			/* Simulate lex */
  LEX_YYSTYPE yylval;
  SELECT_LEX_UNIT unit;                         /* most upper unit */
  SELECT_LEX select_lex,                        /* first SELECT_LEX */
    /* current SELECT_LEX in parsing */
    *select;
  uchar *ptr,*tok_start,*tok_end,*end_of_query;
  char *length,*dec,*change,*name;
  char *backup_dir;				/* For RESTORE/BACKUP */
  char* to_log;                                 /* For PURGE MASTER LOGS TO */
  char* x509_subject,*x509_issuer,*ssl_cipher;
  enum SSL_type ssl_type;			/* defined in violite.h */
  String *wild;
  sql_exchange *exchange;
  select_result *result;

  List<key_part_spec> col_list;
  List<key_part_spec> ref_list;
  List<Alter_drop>    drop_list;
  List<Alter_column>  alter_list;
  List<String>	      interval_list;
  List<LEX_USER>      users_list;
  List<LEX_COLUMN>    columns;
  List<Key>	      key_list;
  List<create_field>  create_list;
  List<Item>	      *insert_list,field_list,value_list;
  List<List_item>     many_values;
  List<set_var_base>  var_list;
  List<Item>          param_list;
  SQL_LIST	      proc_list, auxilliary_table_list;
  TYPELIB	      *interval;
  create_field	      *last_field;
  Item *default_value, *comment;
  CONVERT *convert_set;
  CONVERT *thd_convert_set;			// Set with SET CHAR SET
  LEX_USER *grant_user;
  gptr yacc_yyss,yacc_yyvs;
  THD *thd;
  udf_func udf;
  HA_CHECK_OPT   check_opt;			// check/repair options
  HA_CREATE_INFO create_info;
  LEX_MASTER_INFO mi;				// used by CHANGE MASTER
  USER_RESOURCES mqh;
  ulong thread_id,type;
  enum_sql_command sql_command;
  thr_lock_type lock_option;
  enum lex_states next_state;
  enum enum_duplicates duplicates;
  enum enum_tx_isolation tx_isolation;
  enum enum_ha_read_modes ha_read_mode;
  enum ha_rkey_function ha_rkey_mode;
  enum enum_enable_or_disable alter_keys_onoff;
  enum enum_var_type option_type;
  uint grant, grant_tot_col, which_columns;
  uint fk_delete_opt, fk_update_opt, fk_match_option;
  uint param_count;
  bool drop_primary, drop_if_exists, local_file, olap;
  bool in_comment, ignore_space, verbose, simple_alter;
  bool derived_tables, describe;
  uint slave_thd_opt;
  CHARSET_INFO *charset;
  char *help_arg;
} LEX;


void lex_init(void);
void lex_free(void);
LEX *lex_start(THD *thd, uchar *buf,uint length);
void lex_end(LEX *lex);

extern pthread_key(LEX*,THR_LEX);

extern LEX_STRING tmp_table_alias;

#define current_lex (&current_thd->lex)
