/* Copyright (C) 2000-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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
class sp_head;
class sp_name;
class sp_instr;
class sp_pcontext;
class st_alter_tablespace;
class partition_info;
class Event_parse_data;

#ifdef MYSQL_SERVER
/*
  The following hack is needed because mysql_yacc.cc does not define
  YYSTYPE before including this file
*/

#include "set_var.h"

#ifdef MYSQL_YACC
#define LEX_YYSTYPE void *
#else
#include "lex_symbol.h"
#if MYSQL_LEX
#include "sql_yacc.h"
#define LEX_YYSTYPE YYSTYPE *
#else
#define LEX_YYSTYPE void *
#endif
#endif
#endif

/*
  When a command is added here, be sure it's also added in mysqld.cc
  in "struct show_var_st status_vars[]= {" ...

  If the command returns a result set or is not allowed in stored
  functions or triggers, please also make sure that
  sp_get_flags_for_command (sp_head.cc) returns proper flags for the
  added SQLCOM_.
*/

enum enum_sql_command {
  SQLCOM_SELECT, SQLCOM_CREATE_TABLE, SQLCOM_CREATE_INDEX, SQLCOM_ALTER_TABLE,
  SQLCOM_UPDATE, SQLCOM_INSERT, SQLCOM_INSERT_SELECT,
  SQLCOM_DELETE, SQLCOM_TRUNCATE, SQLCOM_DROP_TABLE, SQLCOM_DROP_INDEX,

  SQLCOM_SHOW_DATABASES, SQLCOM_SHOW_TABLES, SQLCOM_SHOW_FIELDS,
  SQLCOM_SHOW_KEYS, SQLCOM_SHOW_VARIABLES, SQLCOM_SHOW_STATUS,
  SQLCOM_SHOW_ENGINE_LOGS, SQLCOM_SHOW_ENGINE_STATUS, SQLCOM_SHOW_ENGINE_MUTEX,
  SQLCOM_SHOW_PROCESSLIST, SQLCOM_SHOW_MASTER_STAT, SQLCOM_SHOW_SLAVE_STAT,
  SQLCOM_SHOW_GRANTS, SQLCOM_SHOW_CREATE, SQLCOM_SHOW_CHARSETS,
  SQLCOM_SHOW_COLLATIONS, SQLCOM_SHOW_CREATE_DB, SQLCOM_SHOW_TABLE_STATUS,
  SQLCOM_SHOW_TRIGGERS,

  SQLCOM_LOAD,SQLCOM_SET_OPTION,SQLCOM_LOCK_TABLES,SQLCOM_UNLOCK_TABLES,
  SQLCOM_GRANT,
  SQLCOM_CHANGE_DB, SQLCOM_CREATE_DB, SQLCOM_DROP_DB, SQLCOM_ALTER_DB,
  SQLCOM_RENAME_DB,
  SQLCOM_REPAIR, SQLCOM_REPLACE, SQLCOM_REPLACE_SELECT,
  SQLCOM_CREATE_FUNCTION, SQLCOM_DROP_FUNCTION,
  SQLCOM_REVOKE,SQLCOM_OPTIMIZE, SQLCOM_CHECK,
  SQLCOM_ASSIGN_TO_KEYCACHE, SQLCOM_PRELOAD_KEYS,
  SQLCOM_FLUSH, SQLCOM_KILL, SQLCOM_ANALYZE,
  SQLCOM_ROLLBACK, SQLCOM_ROLLBACK_TO_SAVEPOINT,
  SQLCOM_COMMIT, SQLCOM_SAVEPOINT, SQLCOM_RELEASE_SAVEPOINT,
  SQLCOM_SLAVE_START, SQLCOM_SLAVE_STOP,
  SQLCOM_BEGIN, SQLCOM_LOAD_MASTER_TABLE, SQLCOM_CHANGE_MASTER,
  SQLCOM_RENAME_TABLE, SQLCOM_BACKUP_TABLE, SQLCOM_RESTORE_TABLE,
  SQLCOM_RESET, SQLCOM_PURGE, SQLCOM_PURGE_BEFORE, SQLCOM_SHOW_BINLOGS,
  SQLCOM_SHOW_OPEN_TABLES, SQLCOM_LOAD_MASTER_DATA,
  SQLCOM_HA_OPEN, SQLCOM_HA_CLOSE, SQLCOM_HA_READ,
  SQLCOM_SHOW_SLAVE_HOSTS, SQLCOM_DELETE_MULTI, SQLCOM_UPDATE_MULTI,
  SQLCOM_SHOW_BINLOG_EVENTS, SQLCOM_SHOW_NEW_MASTER, SQLCOM_DO,
  SQLCOM_SHOW_WARNS, SQLCOM_EMPTY_QUERY, SQLCOM_SHOW_ERRORS,
  SQLCOM_SHOW_COLUMN_TYPES, SQLCOM_SHOW_STORAGE_ENGINES, SQLCOM_SHOW_PRIVILEGES,
  SQLCOM_HELP, SQLCOM_CREATE_USER, SQLCOM_DROP_USER, SQLCOM_RENAME_USER,
  SQLCOM_REVOKE_ALL, SQLCOM_CHECKSUM,
  SQLCOM_CREATE_PROCEDURE, SQLCOM_CREATE_SPFUNCTION, SQLCOM_CALL,
  SQLCOM_DROP_PROCEDURE, SQLCOM_ALTER_PROCEDURE,SQLCOM_ALTER_FUNCTION,
  SQLCOM_SHOW_CREATE_PROC, SQLCOM_SHOW_CREATE_FUNC,
  SQLCOM_SHOW_STATUS_PROC, SQLCOM_SHOW_STATUS_FUNC,
  SQLCOM_PREPARE, SQLCOM_EXECUTE, SQLCOM_DEALLOCATE_PREPARE,
  SQLCOM_CREATE_VIEW, SQLCOM_DROP_VIEW,
  SQLCOM_CREATE_TRIGGER, SQLCOM_DROP_TRIGGER,
  SQLCOM_XA_START, SQLCOM_XA_END, SQLCOM_XA_PREPARE,
  SQLCOM_XA_COMMIT, SQLCOM_XA_ROLLBACK, SQLCOM_XA_RECOVER,
  SQLCOM_SHOW_PROC_CODE, SQLCOM_SHOW_FUNC_CODE,
  SQLCOM_ALTER_TABLESPACE,
  SQLCOM_INSTALL_PLUGIN, SQLCOM_UNINSTALL_PLUGIN,
  SQLCOM_SHOW_AUTHORS, SQLCOM_BINLOG_BASE64_EVENT,
  SQLCOM_SHOW_PLUGINS,
  SQLCOM_SHOW_CONTRIBUTORS,
  SQLCOM_CREATE_SERVER, SQLCOM_DROP_SERVER, SQLCOM_ALTER_SERVER,
  SQLCOM_CREATE_EVENT, SQLCOM_ALTER_EVENT, SQLCOM_DROP_EVENT,
  SQLCOM_SHOW_CREATE_EVENT, SQLCOM_SHOW_EVENTS, 

  /* This should be the last !!! */

  SQLCOM_END
};

// describe/explain types
#define DESCRIBE_NORMAL		1
#define DESCRIBE_EXTENDED	2
/*
  This is not within #ifdef because we want "EXPLAIN PARTITIONS ..." to produce
  additional "partitions" column even if partitioning is not compiled in.
*/
#define DESCRIBE_PARTITIONS	4

#ifdef MYSQL_SERVER

enum enum_sp_suid_behaviour
{
  SP_IS_DEFAULT_SUID= 0,
  SP_IS_NOT_SUID,
  SP_IS_SUID
};

enum enum_sp_data_access
{
  SP_DEFAULT_ACCESS= 0,
  SP_CONTAINS_SQL,
  SP_NO_SQL,
  SP_READS_SQL_DATA,
  SP_MODIFIES_SQL_DATA
};

const LEX_STRING sp_data_access_name[]=
{
  { C_STRING_WITH_LEN("") },
  { C_STRING_WITH_LEN("CONTAINS SQL") },
  { C_STRING_WITH_LEN("NO SQL") },
  { C_STRING_WITH_LEN("READS SQL DATA") },
  { C_STRING_WITH_LEN("MODIFIES SQL DATA") }
};

#define DERIVED_SUBQUERY	1
#define DERIVED_VIEW		2

enum enum_view_create_mode
{
  VIEW_CREATE_NEW,		// check that there are not such VIEW/table
  VIEW_ALTER,			// check that VIEW .frm with such name exists
  VIEW_CREATE_OR_REPLACE	// check only that there are not such table
};

enum enum_drop_mode
{
  DROP_DEFAULT, // mode is not specified
  DROP_CASCADE, // CASCADE option
  DROP_RESTRICT // RESTRICT option
};

typedef List<Item> List_item;

/* SERVERS CACHE CHANGES */
typedef struct st_lex_server_options
{
  long port;
  uint server_name_length;
  char *server_name, *host, *db, *username, *password, *scheme, *socket, *owner;
} LEX_SERVER_OPTIONS;

typedef struct st_lex_master_info
{
  char *host, *user, *password, *log_file_name;
  uint port, connect_retry;
  ulonglong pos;
  ulong server_id;
  /*
    Enum is used for making it possible to detect if the user
    changed variable or if it should be left at old value
   */
  enum {SSL_UNCHANGED, SSL_DISABLE, SSL_ENABLE}
    ssl, ssl_verify_server_cert;
  char *ssl_key, *ssl_cert, *ssl_ca, *ssl_capath, *ssl_cipher;
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

enum tablespace_op_type
{
  NO_TABLESPACE_OP, DISCARD_TABLESPACE, IMPORT_TABLESPACE
};

/* 
  String names used to print a statement with index hints.
  Keep in sync with index_hint_type.
*/
extern const char * index_hint_type_name[];
typedef byte index_clause_map;

/*
  Bits in index_clause_map : one for each possible FOR clause in
  USE/FORCE/IGNORE INDEX index hint specification
*/
#define INDEX_HINT_MASK_JOIN  (1)
#define INDEX_HINT_MASK_GROUP (1 << 1)
#define INDEX_HINT_MASK_ORDER (1 << 2)

#define INDEX_HINT_MASK_ALL (INDEX_HINT_MASK_JOIN | INDEX_HINT_MASK_GROUP | \
                             INDEX_HINT_MASK_ORDER)

/* Single element of an USE/FORCE/IGNORE INDEX list specified as a SQL hint  */
class index_hint : public Sql_alloc
{
public:
  /* The type of the hint : USE/FORCE/IGNORE */
  enum index_hint_type type;
  /* Where the hit applies to. A bitmask of INDEX_HINT_MASK_<place> values */
  index_clause_map clause;
  /* 
    The index name. Empty (str=NULL) name represents an empty list 
    USE INDEX () clause 
  */ 
  LEX_STRING key_name;

  index_hint (enum index_hint_type type_arg, index_clause_map clause_arg,
              char *str, uint length) :
    type(type_arg), clause(clause_arg)
  {
    key_name.str= str;
    key_name.length= length;
  }
}; 

/* 
  The state of the lex parsing for selects 
   
   master and slaves are pointers to select_lex.
   master is pointer to upper level node.
   slave is pointer to lower level node
   select_lex is a SELECT without union
   unit is container of either
     - One SELECT
     - UNION of selects
   select_lex and unit are both inherited form select_lex_node
   neighbors are two select_lex or units on the same level

   All select describing structures linked with following pointers:
   - list of neighbors (next/prev) (prev of first element point to slave
     pointer of upper structure)
     - For select this is a list of UNION's (or one element list)
     - For units this is a list of sub queries for the upper level select

   - pointer to master (master), which is
     If this is a unit
       - pointer to outer select_lex
     If this is a select_lex
       - pointer to outer unit structure for select

   - pointer to slave (slave), which is either:
     If this is a unit:
       - first SELECT that belong to this unit
     If this is a select_lex
       - first unit that belong to this SELECT (subquries or derived tables)

   - list of all select_lex (link_next/link_prev)
     This is to be used for things like derived tables creation, where we
     go through this list and create the derived tables.

   If unit contain several selects (UNION now, INTERSECT etc later)
   then it have special select_lex called fake_select_lex. It used for
   storing global parameters (like ORDER BY, LIMIT) and executing union.
   Subqueries used in global ORDER BY clause will be attached to this
   fake_select_lex, which will allow them correctly resolve fields of
   'upper' UNION and outer selects.

   For example for following query:

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

   select1: (select * from table1 ...)
   select2: (select * from table2 ...)
   select3: (select * from table3)
   select1.1.1: (select * from table1_1_1)
   ...

     main unit
     fake0
     select1 select2 select3
     |^^     |^
    s|||     ||master
    l|||     |+---------------------------------+
    a|||     +---------------------------------+|
    v|||master                         slave   ||
    e||+-------------------------+             ||
     V|            neighbor      |             V|
     unit1.1<+==================>unit1.2       unit2.1
     fake1.1
     select1.1.1 select 1.1.2    select1.2.1   select2.1.1
                                               |^
                                               ||
                                               V|
                                               unit2.1.1.1
                                               select2.1.1.1.1


   relation in main unit will be following:
   (bigger picture for:
      main unit
      fake0
      select1 select2 select3
   in the above picture)

         main unit
         |^^^^|fake_select_lex
         |||||+--------------------------------------------+
         ||||+--------------------------------------------+|
         |||+------------------------------+              ||
         ||+--------------+                |              ||
    slave||master         |                |              ||
         V|      neighbor |       neighbor |        master|V
         select1<========>select2<========>select3        fake0

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
struct st_lex;
class st_select_lex;
class st_select_lex_unit;
class st_select_lex_node {
protected:
  st_select_lex_node *next, **prev,   /* neighbor list */
    *master, *slave,                  /* vertical links */
    *link_next, **link_prev;          /* list of whole SELECT_LEX */
public:

  ulonglong options;

  /*
    In sql_cache we store SQL_CACHE flag as specified by user to be
    able to restore SELECT statement from internal structures.
  */
  enum e_sql_cache { SQL_CACHE_UNSPECIFIED, SQL_NO_CACHE, SQL_CACHE };
  e_sql_cache sql_cache;

  /*
    result of this query can't be cached, bit field, can be :
      UNCACHEABLE_DEPENDENT
      UNCACHEABLE_RAND
      UNCACHEABLE_SIDEEFFECT
      UNCACHEABLE_EXPLAIN
      UNCACHEABLE_PREPARE
  */
  uint8 uncacheable;
  enum sub_select_type linkage;
  bool no_table_names_allowed; /* used for global order by */
  bool no_error; /* suppress error message (convert it to warnings) */

  static void *operator new(size_t size)
  {
    return (void*) sql_alloc((uint) size);
  }
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint) size); }
  static void operator delete(void *ptr,size_t size) { TRASH(ptr, size); }
  static void operator delete(void *ptr, MEM_ROOT *mem_root) {}
  st_select_lex_node(): linkage(UNSPECIFIED_TYPE) {}
  virtual ~st_select_lex_node() {}
  inline st_select_lex_node* get_master() { return master; }
  virtual void init_query();
  virtual void init_select();
  void include_down(st_select_lex_node *upper);
  void include_neighbour(st_select_lex_node *before);
  void include_standalone(st_select_lex_node *sel, st_select_lex_node **ref);
  void include_global(st_select_lex_node **plink);
  void exclude();

  virtual st_select_lex_unit* master_unit()= 0;
  virtual st_select_lex* outer_select()= 0;
  virtual st_select_lex* return_after_parsing()= 0;

  virtual bool set_braces(bool value);
  virtual bool inc_in_sum_expr();
  virtual uint get_in_sum_expr();
  virtual TABLE_LIST* get_table_list();
  virtual List<Item>* get_item_list();
  virtual ulong get_table_join_options();
  virtual TABLE_LIST *add_table_to_list(THD *thd, Table_ident *table,
					LEX_STRING *alias,
					ulong table_options,
					thr_lock_type flags= TL_UNLOCK,
					List<index_hint> *hints= 0,
                                        LEX_STRING *option= 0);
  virtual void set_lock_for_tables(thr_lock_type lock_type) {}

  friend class st_select_lex_unit;
  friend bool mysql_new_select(struct st_lex *lex, bool move_down);
  friend bool mysql_make_view(THD *thd, File_parser *parser,
                              TABLE_LIST *table, uint flags);
private:
  void fast_exclude();
};
typedef class st_select_lex_node SELECT_LEX_NODE;

/* 
   SELECT_LEX_UNIT - unit of selects (UNION, INTERSECT, ...) group 
   SELECT_LEXs
*/
class THD;
class select_result;
class JOIN;
class select_union;
class Procedure;
class st_select_lex_unit: public st_select_lex_node {
protected:
  TABLE_LIST result_table_list;
  select_union *union_result;
  TABLE *table; /* temporary table using for appending UNION results */

  select_result *result;
  ulonglong found_rows_for_union;
  bool saved_error;

public:
  bool  prepared, // prepare phase already performed for UNION (unit)
    optimized, // optimize phase already performed for UNION (unit)
    executed, // already executed
    cleaned;

  // list of fields which points to temporary table for union
  List<Item> item_list;
  /*
    list of types of items inside union (used for union & derived tables)
    
    Item_type_holders from which this list consist may have pointers to Field,
    pointers is valid only after preparing SELECTS of this unit and before
    any SELECT of this unit execution
  */
  List<Item> types;
  /*
    Pointer to 'last' select or pointer to unit where stored
    global parameters for union
  */
  st_select_lex *global_parameters;
  //node on wich we should return current_select pointer after parsing subquery
  st_select_lex *return_to;
  /* LIMIT clause runtime counters */
  ha_rows select_limit_cnt, offset_limit_cnt;
  /* not NULL if unit used in subselect, point to subselect item */
  Item_subselect *item;
  /* thread handler */
  THD *thd;
  /*
    SELECT_LEX for hidden SELECT in onion which process global
    ORDER BY and LIMIT
  */
  st_select_lex *fake_select_lex;

  st_select_lex *union_distinct; /* pointer to the last UNION DISTINCT */
  bool describe; /* union exec() called for EXPLAIN */
  Procedure *last_procedure;	 /* Pointer to procedure, if such exists */

  void init_query();
  st_select_lex_unit* master_unit();
  st_select_lex* outer_select();
  st_select_lex* first_select()
  {
    return my_reinterpret_cast(st_select_lex*)(slave);
  }
  st_select_lex_unit* next_unit()
  {
    return my_reinterpret_cast(st_select_lex_unit*)(next);
  }
  st_select_lex* return_after_parsing() { return return_to; }
  void exclude_level();
  void exclude_tree();

  /* UNION methods */
  bool prepare(THD *thd, select_result *result, ulong additional_options);
  bool exec();
  bool cleanup();
  inline void unclean() { cleaned= 0; }
  void reinit_exec_mechanism();

  void print(String *str);

  bool add_fake_select_lex(THD *thd);
  void init_prepare_fake_select_lex(THD *thd);
  inline bool is_prepared() { return prepared; }
  bool change_result(select_subselect *result, select_subselect *old_result);
  void set_limit(st_select_lex *values);
  void set_thd(THD *thd_arg) { thd= thd_arg; }
  inline bool is_union (); 

  friend void lex_start(THD *thd);
  friend int subselect_union_engine::exec();

  List<Item> *get_unit_column_types();
};

typedef class st_select_lex_unit SELECT_LEX_UNIT;

/*
  SELECT_LEX - store information of parsed SELECT statment
*/
class st_select_lex: public st_select_lex_node
{
public:
  Name_resolution_context context;
  char *db;
  Item *where, *having;                         /* WHERE & HAVING clauses */
  Item *prep_where; /* saved WHERE clause for prepared statement processing */
  Item *prep_having;/* saved HAVING clause for prepared statement processing */
  /* Saved values of the WHERE and HAVING clauses*/
  Item::cond_result cond_value, having_value;
  /* point on lex in which it was created, used in view subquery detection */
  st_lex *parent_lex;
  enum olap_type olap;
  /* FROM clause - points to the beginning of the TABLE_LIST::next_local list. */
  SQL_LIST	      table_list;
  SQL_LIST	      group_list; /* GROUP BY clause. */
  List<Item>          item_list;  /* list of fields & expressions */
  List<String>        interval_list;
  bool	              is_item_list_lookup;
  /* 
    Usualy it is pointer to ftfunc_list_alloc, but in union used to create fake
    select_lex for calling mysql_select under results of union
  */
  List<Item_func_match> *ftfunc_list;
  List<Item_func_match> ftfunc_list_alloc;
  JOIN *join; /* after JOIN::prepare it is pointer to corresponding JOIN */
  List<TABLE_LIST> top_join_list; /* join list of the top level          */
  List<TABLE_LIST> *join_list;    /* list for the currently parsed join  */
  TABLE_LIST *embedding;          /* table embedding to the above list   */
  /*
    Beginning of the list of leaves in a FROM clause, where the leaves
    inlcude all base tables including view tables. The tables are connected
    by TABLE_LIST::next_leaf, so leaf_tables points to the left-most leaf.
  */
  TABLE_LIST *leaf_tables;
  const char *type;               /* type of select for EXPLAIN          */

  SQL_LIST order_list;                /* ORDER clause */
  List<List_item>     expr_list;
  SQL_LIST *gorder_list;
  Item *select_limit, *offset_limit;  /* LIMIT clause parameters */
  // Arrays of pointers to top elements of all_fields list
  Item **ref_pointer_array;

  /*
    number of items in select_list and HAVING clause used to get number
    bigger then can be number of entries that will be added to all item
    list during split_sum_func
  */
  uint select_n_having_items;
  uint cond_count;    /* number of arguments of and/or/xor in where/having/on */
  uint between_count; /* number of between predicates in where/having/on      */   
  /*
    Number of fields used in select list or where clause of current select
    and all inner subselects.
  */
  uint select_n_where_fields;
  enum_parsing_place parsing_place; /* where we are parsing expression */
  bool with_sum_func;   /* sum function indicator */
  /* 
    PS or SP cond natural joins was alredy processed with permanent
    arena and all additional items which we need alredy stored in it
  */
  bool conds_processed_with_permanent_arena;

  ulong table_join_options;
  uint in_sum_expr;
  uint select_number; /* number of select (used for EXPLAIN) */
  int nest_level;     /* nesting level of select */
  Item_sum *inner_sum_func_list; /* list of sum func in nested selects */ 
  uint with_wild; /* item list contain '*' */
  bool  braces;   	/* SELECT ... UNION (SELECT ... ) <- this braces */
  /* TRUE when having fix field called in processing of this SELECT */
  bool having_fix_field;
  /* List of references to fields referenced from inner selects */
  List<Item_outer_ref> inner_refs_list;
  /* Number of Item_sum-derived objects in this SELECT */
  uint n_sum_items;
  /* Number of Item_sum-derived objects in children and descendant SELECTs */
  uint n_child_sum_items;

  /* explicit LIMIT clause was used */
  bool explicit_limit;
  /*
    there are subquery in HAVING clause => we can't close tables before
    query processing end even if we use temporary table
  */
  bool subquery_in_having;
  /* TRUE <=> this SELECT is correlated w.r.t. some ancestor select */
  bool is_correlated;
  /*
    This variable is required to ensure proper work of subqueries and
    stored procedures. Generally, one should use the states of
    Query_arena to determine if it's a statement prepare or first
    execution of a stored procedure. However, in case when there was an
    error during the first execution of a stored procedure, the SP body
    is not expelled from the SP cache. Therefore, a deeply nested
    subquery might be left unoptimized. So we need this per-subquery
    variable to inidicate the optimization/execution state of every
    subquery. Prepared statements work OK in that regard, as in
    case of an error during prepare the PS is not created.
  */
  bool first_execution;
  bool first_cond_optimization;
  /* do not wrap view fields with Item_ref */
  bool no_wrap_view_item;
  /* exclude this select from check of unique_table() */
  bool exclude_from_table_unique_test;
  /* List of fields that aren't under an aggregate function */
  List<Item_field> non_agg_fields;
  /* index in the select list of the expression currently being fixed */
  int cur_pos_in_select_list;

  List<udf_func>     udf_list;                  /* udf function calls stack */
  /* 
    This is a copy of the original JOIN USING list that comes from
    the parser. The parser :
      1. Sets the natural_join of the second TABLE_LIST in the join
         and the st_select_lex::prev_join_using.
      2. Makes a parent TABLE_LIST and sets its is_natural_join/
       join_using_fields members.
      3. Uses the wrapper TABLE_LIST as a table in the upper level.
    We cannot assign directly to join_using_fields in the parser because
    at stage (1.) the parent TABLE_LIST is not constructed yet and
    the assignment will override the JOIN USING fields of the lower level
    joins on the right.
  */
  List<String> *prev_join_using;
  void init_query();
  void init_select();
  st_select_lex_unit* master_unit();
  st_select_lex_unit* first_inner_unit() 
  { 
    return (st_select_lex_unit*) slave; 
  }
  st_select_lex* outer_select();
  st_select_lex* next_select() { return (st_select_lex*) next; }
  st_select_lex* next_select_in_list() 
  {
    return (st_select_lex*) link_next;
  }
  st_select_lex_node** next_select_in_list_addr()
  {
    return &link_next;
  }
  st_select_lex* return_after_parsing()
  {
    return master_unit()->return_after_parsing();
  }

  void mark_as_dependent(st_select_lex *last);

  bool set_braces(bool value);
  bool inc_in_sum_expr();
  uint get_in_sum_expr();

  bool add_item_to_list(THD *thd, Item *item);
  bool add_group_to_list(THD *thd, Item *item, bool asc);
  bool add_ftfunc_to_list(Item_func_match *func);
  bool add_order_to_list(THD *thd, Item *item, bool asc);
  TABLE_LIST* add_table_to_list(THD *thd, Table_ident *table,
				LEX_STRING *alias,
				ulong table_options,
				thr_lock_type flags= TL_UNLOCK,
				List<index_hint> *hints= 0,
                                LEX_STRING *option= 0);
  TABLE_LIST* get_table_list();
  bool init_nested_join(THD *thd);
  TABLE_LIST *end_nested_join(THD *thd);
  TABLE_LIST *nest_last_join(THD *thd);
  void add_joined_table(TABLE_LIST *table);
  TABLE_LIST *convert_right_join();
  List<Item>* get_item_list();
  ulong get_table_join_options();
  void set_lock_for_tables(thr_lock_type lock_type);
  inline void init_order()
  {
    order_list.elements= 0;
    order_list.first= 0;
    order_list.next= (byte**) &order_list.first;
  }
  /*
    This method created for reiniting LEX in mysql_admin_table() and can be
    used only if you are going remove all SELECT_LEX & units except belonger
    to LEX (LEX::unit & LEX::select, for other purposes there are
    SELECT_LEX_UNIT::exclude_level & SELECT_LEX_UNIT::exclude_tree
  */
  void cut_subtree() { slave= 0; }
  bool test_limit();

  friend void lex_start(THD *thd);
  st_select_lex() : n_sum_items(0), n_child_sum_items(0) {}
  void make_empty_select()
  {
    init_query();
    init_select();
  }
  bool setup_ref_array(THD *thd, uint order_group_num);
  void print(THD *thd, String *str);
  static void print_order(String *str, ORDER *order);
  void print_limit(THD *thd, String *str);
  void fix_prepare_information(THD *thd, Item **conds, Item **having_conds);
  /*
    Destroy the used execution plan (JOIN) of this subtree (this
    SELECT_LEX and all nested SELECT_LEXes and SELECT_LEX_UNITs).
  */
  bool cleanup();
  /*
    Recursively cleanup the join of this select lex and of all nested
    select lexes.
  */
  void cleanup_all_joins(bool full);

  void set_index_hint_type(enum index_hint_type type, index_clause_map clause);

  /* 
   Add a index hint to the tagged list of hints. The type and clause of the
   hint will be the current ones (set by set_index_hint()) 
  */
  bool add_index_hint (THD *thd, char *str, uint length);

  /* make a list to hold index hints */
  void alloc_index_hints (THD *thd);
  /* read and clear the index hints */
  List<index_hint>* pop_index_hints(void) 
  {
    List<index_hint> *hints= index_hints;
    index_hints= NULL;
    return hints;
  }

  void clear_index_hints(void) { index_hints= NULL; }

private:  
  /* current index hint kind. used in filling up index_hints */
  enum index_hint_type current_index_hint_type;
  index_clause_map current_index_hint_clause;
  /* a list of USE/FORCE/IGNORE INDEX */
  List<index_hint> *index_hints;
};
typedef class st_select_lex SELECT_LEX;

inline bool st_select_lex_unit::is_union ()
{ 
  return first_select()->next_select() && 
    first_select()->next_select()->linkage == UNION_TYPE;
}

#define ALTER_ADD_COLUMN	(1L << 0)
#define ALTER_DROP_COLUMN	(1L << 1)
#define ALTER_CHANGE_COLUMN	(1L << 2)
#define ALTER_ADD_INDEX		(1L << 3)
#define ALTER_DROP_INDEX	(1L << 4)
#define ALTER_RENAME		(1L << 5)
#define ALTER_ORDER		(1L << 6)
#define ALTER_OPTIONS		(1L << 7)
#define ALTER_CHANGE_COLUMN_DEFAULT (1L << 8)
#define ALTER_KEYS_ONOFF        (1L << 9)
#define ALTER_CONVERT           (1L << 10)
#define ALTER_FORCE		(1L << 11)
#define ALTER_RECREATE          (1L << 12)
#define ALTER_ADD_PARTITION     (1L << 13)
#define ALTER_DROP_PARTITION    (1L << 14)
#define ALTER_COALESCE_PARTITION (1L << 15)
#define ALTER_REORGANIZE_PARTITION (1L << 16) 
#define ALTER_PARTITION          (1L << 17)
#define ALTER_OPTIMIZE_PARTITION (1L << 18)
#define ALTER_TABLE_REORG        (1L << 19)
#define ALTER_REBUILD_PARTITION  (1L << 20)
#define ALTER_ALL_PARTITION      (1L << 21)
#define ALTER_ANALYZE_PARTITION  (1L << 22)
#define ALTER_CHECK_PARTITION    (1L << 23)
#define ALTER_REPAIR_PARTITION   (1L << 24)
#define ALTER_REMOVE_PARTITIONING (1L << 25)
#define ALTER_FOREIGN_KEY         (1L << 26)

typedef struct st_alter_info
{
  List<Alter_drop>            drop_list;
  List<Alter_column>          alter_list;
  uint                        flags;
  enum enum_enable_or_disable keys_onoff;
  enum tablespace_op_type     tablespace_op;
  List<char>                  partition_names;
  uint                        no_parts;

  st_alter_info(){clear();}
  void clear()
  {
    keys_onoff= LEAVE_AS_IS;
    tablespace_op= NO_TABLESPACE_OP;
    no_parts= 0;
    partition_names.empty();
  }
  void reset(){drop_list.empty();alter_list.empty();clear();}
} ALTER_INFO;

struct st_sp_chistics
{
  LEX_STRING comment;
  enum enum_sp_suid_behaviour suid;
  bool detistic;
  enum enum_sp_data_access daccess;
};


struct st_trg_chistics
{
  enum trg_action_time_type action_time;
  enum trg_event_type event;
};

extern sys_var *trg_new_row_fake_var;

enum xa_option_words {XA_NONE, XA_JOIN, XA_RESUME, XA_ONE_PHASE,
                      XA_SUSPEND, XA_FOR_MIGRATE};


/*
  Class representing list of all tables used by statement.
  It also contains information about stored functions used by statement
  since during its execution we may have to add all tables used by its
  stored functions/triggers to this list in order to pre-open and lock
  them.

  Also used by st_lex::reset_n_backup/restore_backup_query_tables_list()
  methods to save and restore this information.
*/

class Query_tables_list
{
public:
  /* Global list of all tables used by this statement */
  TABLE_LIST *query_tables;
  /* Pointer to next_global member of last element in the previous list. */
  TABLE_LIST **query_tables_last;
  /*
    If non-0 then indicates that query requires prelocking and points to
    next_global member of last own element in query table list (i.e. last
    table which was not added to it as part of preparation to prelocking).
    0 - indicates that this query does not need prelocking.
  */
  TABLE_LIST **query_tables_own_last;
  /*
    Set of stored routines called by statement.
    (Note that we use lazy-initialization for this hash).
  */
  enum { START_SROUTINES_HASH_SIZE= 16 };
  HASH sroutines;
  /*
    List linking elements of 'sroutines' set. Allows you to add new elements
    to this set as you iterate through the list of existing elements.
    'sroutines_list_own_last' is pointer to ::next member of last element of
    this list which represents routine which is explicitly used by query.
    'sroutines_list_own_elements' number of explicitly used routines.
    We use these two members for restoring of 'sroutines_list' to the state
    in which it was right after query parsing.
  */
  SQL_LIST sroutines_list;
  byte     **sroutines_list_own_last;
  uint     sroutines_list_own_elements;

  /*
    Tells if the parsing stage detected that some items require row-based
    binlogging to give a reliable binlog/replication, or if we will use
    stored functions or triggers which themselves need require row-based
    binlogging.
  */
  bool binlog_row_based_if_mixed;

  /*
    These constructor and destructor serve for creation/destruction
    of Query_tables_list instances which are used as backup storage.
  */
  Query_tables_list() {}
  ~Query_tables_list() {}

  /* Initializes (or resets) Query_tables_list object for "real" use. */
  void reset_query_tables_list(bool init);
  void destroy_query_tables_list();
  void set_query_tables_list(Query_tables_list *state)
  {
    *this= *state;
  }

  /*
    Direct addition to the list of query tables.
    If you are using this function, you must ensure that the table
    object, in particular table->db member, is initialized.
  */
  void add_to_query_tables(TABLE_LIST *table)
  {
    *(table->prev_global= query_tables_last)= table;
    query_tables_last= &table->next_global;
  }
  bool requires_prelocking()
  {
    return test(query_tables_own_last);
  }
  void mark_as_requiring_prelocking(TABLE_LIST **tables_own_last)
  {
    query_tables_own_last= tables_own_last;
  }
  /* Return pointer to first not-own table in query-tables or 0 */
  TABLE_LIST* first_not_own_table()
  {
    return ( query_tables_own_last ? *query_tables_own_last : 0);
  }
  void chop_off_not_own_tables()
  {
    if (query_tables_own_last)
    {
      *query_tables_own_last= 0;
      query_tables_last= query_tables_own_last;
      query_tables_own_last= 0;
    }
  }
};


/*
  st_parsing_options contains the flags for constructions that are
  allowed in the current statement.
*/

struct st_parsing_options
{
  bool allows_variable;
  bool allows_select_into;
  bool allows_select_procedure;
  bool allows_derived;

  st_parsing_options() { reset(); }
  void reset();
};


/**
  This class represents the character input stream consumed during
  lexical analysis.
*/
class Lex_input_stream
{
public:
  Lex_input_stream(THD *thd, const char* buff, unsigned int length);
  ~Lex_input_stream();

  /** Current thread. */
  THD *m_thd;

  /** Current line number. */
  uint yylineno;

  /** Length of the last token parsed. */
  uint yytoklen;

  /** Interface with bison, value of the last token parsed. */
  LEX_YYSTYPE yylval;

  /** Pointer to the current position in the input stream. */
  const char* ptr;

  /** Starting position of the last token parsed. */
  const char* tok_start;

  /** Ending position of the last token parsed. */
  const char* tok_end;

  /** End of the query text in the input stream. */
  const char* end_of_query;

  /** Starting position of the previous token parsed. */
  const char* tok_start_prev;

  /** Begining of the query text in the input stream. */
  const char* buf;

  /** Current state of the lexical analyser. */
  enum my_lex_states next_state;

  /** Position of ';' in the stream, to delimit multiple queries. */
  const char* found_semicolon;

  /** SQL_MODE = IGNORE_SPACE. */
  bool ignore_space;
  /*
    TRUE if we're parsing a prepared statement: in this mode
    we should allow placeholders and disallow multi-statements.
  */
  bool stmt_prepare_mode;
};


/* The state of the lex parsing. This is saved in the THD struct */

typedef struct st_lex : public Query_tables_list
{
  SELECT_LEX_UNIT unit;                         /* most upper unit */
  SELECT_LEX select_lex;                        /* first SELECT_LEX */
  /* current SELECT_LEX in parsing */
  SELECT_LEX *current_select;
  /* list of all SELECT_LEX */
  SELECT_LEX *all_selects_list;

  char *length,*dec,*change;
  LEX_STRING name;
  Table_ident *like_name;
  char *help_arg;
  char *backup_dir;				/* For RESTORE/BACKUP */
  char* to_log;                                 /* For PURGE MASTER LOGS TO */
  char* x509_subject,*x509_issuer,*ssl_cipher;
  String *wild;
  sql_exchange *exchange;
  select_result *result;
  Item *default_value, *on_update_value;
  LEX_STRING comment, ident;
  LEX_USER *grant_user;
  XID *xid;
  gptr yacc_yyss,yacc_yyvs;
  THD *thd;

  /* maintain a list of used plugins for this LEX */
  DYNAMIC_ARRAY plugins;
  plugin_ref plugins_static_buffer[INITIAL_LEX_PLUGIN_LIST_SIZE];
  
  CHARSET_INFO *charset, *underscore_charset;
  /* store original leaf_tables for INSERT SELECT and PS/SP */
  TABLE_LIST *leaf_tables_insert;
  /* Position (first character index) of SELECT of CREATE VIEW statement */
  uint create_view_select_start;
  /* Partition info structure filled in by PARTITION BY parse part */
  partition_info *part_info;

  /*
    The definer of the object being created (view, trigger, stored routine).
    I.e. the value of DEFINER clause.
  */
  LEX_USER *definer;

  List<key_part_spec> col_list;
  List<key_part_spec> ref_list;
  List<String>	      interval_list;
  List<LEX_USER>      users_list;
  List<LEX_COLUMN>    columns;
  List<Key>	      key_list;
  List<create_field>  create_list;
  List<Item>	      *insert_list,field_list,value_list,update_list;
  List<List_item>     many_values;
  List<set_var_base>  var_list;
  List<Item_param>    param_list;
  List<LEX_STRING>    view_list; // view list (list of field names in view)
  /*
    A stack of name resolution contexts for the query. This stack is used
    at parse time to set local name resolution contexts for various parts
    of a query. For example, in a JOIN ... ON (some_condition) clause the
    Items in 'some_condition' must be resolved only against the operands
    of the the join, and not against the whole clause. Similarly, Items in
    subqueries should be resolved against the subqueries (and outer queries).
    The stack is used in the following way: when the parser detects that
    all Items in some clause need a local context, it creates a new context
    and pushes it on the stack. All newly created Items always store the
    top-most context in the stack. Once the parser leaves the clause that
    required a local context, the parser pops the top-most context.
  */
  List<Name_resolution_context> context_stack;
  List<LEX_STRING>     db_list;

  SQL_LIST	      proc_list, auxiliary_table_list, save_list;
  create_field	      *last_field;
  Item_sum *in_sum_func;
  udf_func udf;
  HA_CHECK_OPT   check_opt;			// check/repair options
  HA_CREATE_INFO create_info;
  KEY_CREATE_INFO key_create_info;
  LEX_MASTER_INFO mi;				// used by CHANGE MASTER
  LEX_SERVER_OPTIONS server_options;
  USER_RESOURCES mqh;
  ulong type;
  /*
    This variable is used in post-parse stage to declare that sum-functions,
    or functions which have sense only if GROUP BY is present, are allowed.
    For example in a query
    SELECT ... FROM ...WHERE MIN(i) == 1 GROUP BY ... HAVING MIN(i) > 2
    MIN(i) in the WHERE clause is not allowed in the opposite to MIN(i)
    in the HAVING clause. Due to possible nesting of select construct
    the variable can contain 0 or 1 for each nest level.
  */
  nesting_map allow_sum_func;
  enum_sql_command sql_command;
  /*
    Usually `expr` rule of yacc is quite reused but some commands better
    not support subqueries which comes standard with this rule, like
    KILL, HA_READ, CREATE/ALTER EVENT etc. Set this to `false` to get
    syntax error back.
  */
  bool expr_allows_subselect;

  thr_lock_type lock_option;
  enum SSL_type ssl_type;			/* defined in violite.h */
  enum enum_duplicates duplicates;
  enum enum_tx_isolation tx_isolation;
  enum enum_ha_read_modes ha_read_mode;
  union {
    enum ha_rkey_function ha_rkey_mode;
    enum xa_option_words xa_opt;
  };
  enum enum_var_type option_type;
  enum enum_view_create_mode create_view_mode;
  enum enum_drop_mode drop_mode;
  uint uint_geom_type;
  uint grant, grant_tot_col, which_columns;
  uint fk_delete_opt, fk_update_opt, fk_match_option;
  uint slave_thd_opt, start_transaction_opt;
  int nest_level;
  /*
    In LEX representing update which were transformed to multi-update
    stores total number of tables. For LEX representing multi-delete
    holds number of tables from which we will delete records.
  */
  uint table_count;
  uint8 describe;
  /*
    A flag that indicates what kinds of derived tables are present in the
    query (0 if no derived tables, otherwise a combination of flags
    DERIVED_SUBQUERY and DERIVED_VIEW).
  */
  uint8 derived_tables;
  uint8 create_view_algorithm;
  uint8 create_view_check;
  bool drop_if_exists, drop_temporary, local_file, one_shot_set;
  bool in_comment, verbose, no_write_to_binlog;
  bool tx_chain, tx_release;
  /*
    Special JOIN::prepare mode: changing of query is prohibited.
    When creating a view, we need to just check its syntax omitting
    any optimizations: afterwards definition of the view will be
    reconstructed by means of ::print() methods and written to
    to an .frm file. We need this definition to stay untouched.
  */
  bool view_prepare_mode;
  bool safe_to_cache_query;
  bool subqueries, ignore;
  st_parsing_options parsing_options;
  ALTER_INFO alter_info;
  /* Prepared statements SQL syntax:*/
  LEX_STRING prepared_stmt_name; /* Statement name (in all queries) */
  /*
    Prepared statement query text or name of variable that holds the
    prepared statement (in PREPARE ... queries)
  */
  LEX_STRING prepared_stmt_code;
  /* If true, prepared_stmt_code is a name of variable that holds the query */
  bool prepared_stmt_code_is_varref;
  /* Names of user variables holding parameters (in EXECUTE) */
  List<LEX_STRING> prepared_stmt_params;
  sp_head *sphead;
  sp_name *spname;
  bool sp_lex_in_use;	/* Keep track on lex usage in SPs for error handling */
  bool all_privileges;
  sp_pcontext *spcont;

  st_sp_chistics sp_chistics;

  Event_parse_data *event_parse_data;

  bool only_view;       /* used for SHOW CREATE TABLE/VIEW */
  /*
    field_list was created for view and should be removed before PS/SP
    rexecuton
  */
  bool empty_field_list_on_rset;
  /*
    view created to be run from definer (standard behaviour)
  */
  uint8 create_view_suid;
  /* Characterstics of trigger being created */
  st_trg_chistics trg_chistics;
  /*
    List of all items (Item_trigger_field objects) representing fields in
    old/new version of row in trigger. We use this list for checking whenever
    all such fields are valid at trigger creation time and for binding these
    fields to TABLE object at table open (altough for latter pointer to table
    being opened is probably enough).
  */
  SQL_LIST trg_table_fields;

  /*
    stmt_definition_begin is intended to point to the next word after
    DEFINER-clause in the following statements:
      - CREATE TRIGGER (points to "TRIGGER");
      - CREATE PROCEDURE (points to "PROCEDURE");
      - CREATE FUNCTION (points to "FUNCTION" or "AGGREGATE");

    This pointer is required to add possibly omitted DEFINER-clause to the
    DDL-statement before dumping it to the binlog. 
  */
  const char *stmt_definition_begin;

  /*
    Pointers to part of LOAD DATA statement that should be rewritten
    during replication ("LOCAL 'filename' REPLACE INTO" part).
  */
  const char *fname_start;
  const char *fname_end;

  /*
    Reference to a struct that contains information in various commands
    to add/create/drop/change table spaces.
  */
  st_alter_tablespace *alter_tablespace_info;
  
  bool escape_used;

  st_lex();

  virtual ~st_lex()
  {
    destroy_query_tables_list();
    plugin_unlock_list(NULL, (plugin_ref *)plugins.buffer, plugins.elements);
    delete_dynamic(&plugins);
  }

  inline void uncacheable(uint8 cause)
  {
    safe_to_cache_query= 0;

    /*
      There are no sense to mark select_lex and union fields of LEX,
      but we should merk all subselects as uncacheable from current till
      most upper
    */
    SELECT_LEX *sl;
    SELECT_LEX_UNIT *un;
    for (sl= current_select, un= sl->master_unit();
	 un != &unit;
	 sl= sl->outer_select(), un= sl->master_unit())
    {
      sl->uncacheable|= cause;
      un->uncacheable|= cause;
    }
  }
  TABLE_LIST *unlink_first_table(bool *link_to_local);
  void link_first_table_back(TABLE_LIST *first, bool link_to_local);
  void first_lists_tables_same();

  bool can_be_merged();
  bool can_use_merged();
  bool can_not_use_merged();
  bool only_view_structure();
  bool need_correct_ident();
  uint8 get_effective_with_check(st_table_list *view);
  /*
    Is this update command where 'WHITH CHECK OPTION' clause is important

    SYNOPSIS
      st_lex::which_check_option_applicable()

    RETURN
      TRUE   have to take 'WHITH CHECK OPTION' clause into account
      FALSE  'WHITH CHECK OPTION' clause do not need
  */
  inline bool which_check_option_applicable()
  {
    switch (sql_command) {
    case SQLCOM_UPDATE:
    case SQLCOM_UPDATE_MULTI:
    case SQLCOM_INSERT:
    case SQLCOM_INSERT_SELECT:
    case SQLCOM_REPLACE:
    case SQLCOM_REPLACE_SELECT:
    case SQLCOM_LOAD:
      return TRUE;
    default:
      return FALSE;
    }
  }

  void cleanup_after_one_table_open();

  bool push_context(Name_resolution_context *context)
  {
    return context_stack.push_front(context);
  }

  void pop_context()
  {
    context_stack.pop();
  }

  Name_resolution_context *current_context()
  {
    return context_stack.head();
  }
  /*
    Restore the LEX and THD in case of a parse error.
  */
  static void cleanup_lex_after_parse_error(THD *thd);

  void reset_n_backup_query_tables_list(Query_tables_list *backup);
  void restore_backup_query_tables_list(Query_tables_list *backup);

  bool table_or_sp_used();
} LEX;

struct st_lex_local: public st_lex
{
  static void *operator new(size_t size)
  {
    return (void*) sql_alloc((uint) size);
  }
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  {
    return (void*) alloc_root(mem_root, (uint) size);
  }
  static void operator delete(void *ptr,size_t size)
  { TRASH(ptr, size); }
  static void operator delete(void *ptr, MEM_ROOT *mem_root)
  { /* Never called */ }
};

extern void lex_init(void);
extern void lex_free(void);
extern void lex_start(THD *thd);
extern void lex_end(LEX *lex);
extern int MYSQLlex(void *arg, void *yythd);
extern const char *skip_rear_comments(const char *ubegin, const char *uend);

extern bool is_lex_native_function(const LEX_STRING *name);

#endif /* MYSQL_SERVER */
