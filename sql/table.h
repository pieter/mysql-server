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


/* Structs that defines the TABLE */

class Item;				/* Needed by ORDER */
class GRANT_TABLE;
class st_select_lex_unit;
class st_select_lex;
class partition_info;
class COND_EQUAL;

/* Order clause list element */

typedef struct st_order {
  struct st_order *next;
  Item	 **item;			/* Point at item in select fields */
  Item	 *item_ptr;			/* Storage for initial item */
  Item   **item_copy;			/* For SPs; the original item ptr */
  int    counter;                       /* position in SELECT list, correct
                                           only if counter_used is true*/
  bool	 asc;				/* true if ascending */
  bool	 free_me;			/* true if item isn't shared  */
  bool	 in_field_list;			/* true if in select field list */
  bool   counter_used;                  /* parameter was counter of columns */
  Field  *field;			/* If tmp-table group */
  char	 *buff;				/* If tmp-table group */
  table_map used, depend_map;
} ORDER;

typedef struct st_grant_info
{
  GRANT_TABLE *grant_table;
  uint version;
  ulong privilege;
  ulong want_privilege;
} GRANT_INFO;

enum tmp_table_type {NO_TMP_TABLE=0, TMP_TABLE=1, TRANSACTIONAL_TMP_TABLE=2};

enum frm_type_enum
{
  FRMTYPE_ERROR= 0,
  FRMTYPE_TABLE,
  FRMTYPE_VIEW
};

typedef struct st_filesort_info
{
  IO_CACHE *io_cache;           /* If sorted through filebyte                */
  byte     *addon_buf;          /* Pointer to a buffer if sorted with fields */
  uint      addon_length;       /* Length of the buffer                      */
  struct st_sort_addon_field *addon_field;     /* Pointer to the fields info */
  void    (*unpack)(struct st_sort_addon_field *, byte *); /* To unpack back */
  byte     *record_pointers;    /* If sorted in memory                       */
  ha_rows   found_records;      /* How many records in sort                  */
} FILESORT_INFO;


/*
  Values in this enum are used to indicate how a tables TIMESTAMP field
  should be treated. It can be set to the current timestamp on insert or
  update or both.
  WARNING: The values are used for bit operations. If you change the
  enum, you must keep the bitwise relation of the values. For example:
  (int) TIMESTAMP_AUTO_SET_ON_BOTH must be equal to
  (int) TIMESTAMP_AUTO_SET_ON_INSERT | (int) TIMESTAMP_AUTO_SET_ON_UPDATE.
  We use an enum here so that the debugger can display the value names.
*/
enum timestamp_auto_set_type
{
  TIMESTAMP_NO_AUTO_SET= 0, TIMESTAMP_AUTO_SET_ON_INSERT= 1,
  TIMESTAMP_AUTO_SET_ON_UPDATE= 2, TIMESTAMP_AUTO_SET_ON_BOTH= 3
};
#define clear_timestamp_auto_bits(_target_, _bits_) \
  (_target_)= (enum timestamp_auto_set_type)((int)(_target_) & ~(int)(_bits_))

class Field_timestamp;
class Field_blob;
class Table_triggers_list;

/* This structure is shared between different table objects */

typedef struct st_table_share
{
#ifdef HAVE_PARTITION_DB
  partition_info *part_info;            /* Partition related information */
#endif
  /* hash of field names (contains pointers to elements of field array) */
  HASH	name_hash;			/* hash of field names */
  MEM_ROOT mem_root;
  TYPELIB keynames;			/* Pointers to keynames */
  TYPELIB fieldnames;			/* Pointer to fieldnames */
  TYPELIB *intervals;			/* pointer to interval info */
#ifdef NOT_YET
  pthread_mutex_t mutex;                /* For locking the share  */
  pthread_cond_t cond;			/* To signal that share is ready */
  struct st_table *open_tables;		/* link to open tables */
  struct st_table *used_next,		/* Link to used tables */
		 **used_prev;
  /* The following is copied to each TABLE on OPEN */
  Field **field;
  KEY  *key_info;			/* data of keys in database */
#endif
  uint	*blob_field;			/* Index to blobs in Field arrray*/
  byte	*default_values;		/* row with default values */
  char	*comment;			/* Comment about table */
  CHARSET_INFO *table_charset;		/* Default charset of string fields */

  /* A pair "database_name\0table_name\0", widely used as simply a db name */
  char	*table_cache_key;
  const char *db;                       /* Pointer to db */
  const char *table_name;               /* Table name (for open) */
  const char *path;                     /* Path to .frm file (from datadir) */
  key_map keys_in_use;                  /* Keys in use for table */
  key_map keys_for_keyread;
  ulong   avg_row_length;		/* create information */
  ulong   raid_chunksize;
  ulong   version, flush_version, mysql_version;
  ulong   timestamp_offset;		/* Set to offset+1 of record */
  ulong   reclength;			/* Recordlength */

  ha_rows min_rows, max_rows;		/* create information */
  enum db_type db_type;			/* table_type for handler */
  enum row_type row_type;		/* How rows are stored */
  enum tmp_table_type tmp_table;

  uint blob_ptr_size;			/* 4 or 8 */
  uint null_bytes;
  uint key_length;			/* Length of table_cache_key */
  uint fields;				/* Number of fields */
  uint rec_buff_length;                 /* Size of table->record[] buffer */
  uint keys, key_parts;
  uint max_key_length, max_unique_length, total_key_length;
  uint uniques;                         /* Number of UNIQUE index */
  uint null_fields;			/* number of null fields */
  uint blob_fields;			/* number of blob fields */
  uint varchar_fields;                  /* number of varchar fields */
  uint db_create_options;		/* Create options from database */
  uint db_options_in_use;		/* Options in use */
  uint db_record_offset;		/* if HA_REC_IN_SEQ */
  uint raid_type, raid_chunks;
  uint open_count;			/* Number of tables in open list */
  /* Index of auto-updated TIMESTAMP field in field array */
  uint primary_key;
  uint timestamp_field_offset;
  uint next_number_index;
  uint next_number_key_offset;
  uchar	  frm_version;
  my_bool system;			/* Set if system record */
  my_bool crypted;                      /* If .frm file is crypted */
  my_bool db_low_byte_first;		/* Portable row format */
  my_bool crashed;
  my_bool is_view;
  my_bool name_lock, replace_with_name_lock;
  /*
    TRUE if this is a system table like 'mysql.proc', which we want to be
    able to open and lock even when we already have some tables open and
    locked. To avoid deadlocks we have to put certain restrictions on
    locking of this table for writing. FALSE - otherwise.
  */
  my_bool system_table;
} TABLE_SHARE;


/* Information for one open table */

struct st_table {
  TABLE_SHARE	*s;
  handler	*file;
#ifdef NOT_YET
  struct st_table *used_next, **used_prev;	/* Link to used tables */
  struct st_table *open_next, **open_prev;	/* Link to open tables */
#endif
  struct st_table *next, *prev;

  THD	*in_use;                        /* Which thread uses this */
  Field **field;			/* Pointer to fields */

  byte *record[2];			/* Pointer to records */
  byte *insert_values;                  /* used by INSERT ... UPDATE */
  key_map quick_keys, used_keys, keys_in_use_for_query;
  KEY  *key_info;			/* data of keys in database */

  Field *next_number_field,		/* Set if next_number is activated */
	*found_next_number_field,	/* Set on open */
        *rowid_field;
  Field_timestamp *timestamp_field;

  /* Table's triggers, 0 if there are no of them */
  Table_triggers_list *triggers;
  struct st_table_list *pos_in_table_list;/* Element referring to this table */
  ORDER		*group;
  const char	*alias;            	  /* alias or table name */
  uchar		*null_flags;
  MY_BITMAP     *read_set;
  MY_BITMAP     *write_set;
  query_id_t	query_id;

  ha_rows	quick_rows[MAX_KEY];
  key_part_map  const_key_parts[MAX_KEY];
  uint		quick_key_parts[MAX_KEY];

  /*
    If this table has TIMESTAMP field with auto-set property (pointed by
    timestamp_field member) then this variable indicates during which
    operations (insert only/on update/in both cases) we should set this
    field to current timestamp. If there are no such field in this table
    or we should not automatically set its value during execution of current
    statement then the variable contains TIMESTAMP_NO_AUTO_SET (i.e. 0).

    Value of this variable is set for each statement in open_table() and
    if needed cleared later in statement processing code (see mysql_update()
    as example).
  */
  timestamp_auto_set_type timestamp_field_type;
  table_map	map;                    /* ID bit of table (1,2,4,8,16...) */
  
  uint		tablenr,used_fields;
  uint          temp_pool_slot;		/* Used by intern temp tables */
  uint		status;                 /* What's in record[0] */
  uint		db_stat;		/* mode of file as in handler.h */
  /* number of select if it is derived table */
  uint          derived_select_number;
  int		current_lock;           /* Type of lock on table */
  my_bool copy_blobs;			/* copy_blobs when storing */
  
  /* 
    0 or JOIN_TYPE_{LEFT|RIGHT}. Currently this is only compared to 0.
    If maybe_null !=0, this table is inner w.r.t. some outer join operation,
    and null_row may be true.
  */
  uint maybe_null;
  /*
    If true, the current table row is considered to have all columns set to 
    NULL, including columns declared as "not null" (see maybe_null).
  */
  my_bool null_row;
  my_bool force_index;
  my_bool distinct,const_table,no_rows;
  my_bool key_read, no_keyread;
  my_bool locked_by_flush;
  my_bool locked_by_name;
  my_bool fulltext_searched;
  my_bool no_cache;
  /* To signal that we should reset query_id for tables and cols */
  my_bool clear_query_id;
  my_bool auto_increment_field_not_null;
  my_bool insert_or_update;             /* Can be used by the handler */
  my_bool alias_name_used;		/* true if table_name is alias */
  my_bool get_fields_in_item_tree;      /* Signal to fix_field */

  REGINFO reginfo;			/* field connections */
  MEM_ROOT mem_root;
  GRANT_INFO grant;
  FILESORT_INFO sort;
  TABLE_SHARE share_not_to_be_used;     /* To be deleted when true shares */
};


typedef struct st_foreign_key_info
{
  LEX_STRING *forein_id;
  LEX_STRING *referenced_db;
  LEX_STRING *referenced_table;
  LEX_STRING *constraint_method;
  List<LEX_STRING> foreign_fields;
  List<LEX_STRING> referenced_fields;
} FOREIGN_KEY_INFO;


enum enum_schema_tables
{
  SCH_CHARSETS= 0,
  SCH_COLLATIONS,
  SCH_COLLATION_CHARACTER_SET_APPLICABILITY,
  SCH_COLUMNS,
  SCH_COLUMN_PRIVILEGES,
  SCH_KEY_COLUMN_USAGE,
  SCH_OPEN_TABLES,
  SCH_PROCEDURES,
  SCH_SCHEMATA,
  SCH_SCHEMA_PRIVILEGES,
  SCH_STATISTICS,
  SCH_STATUS,
  SCH_TABLES,
  SCH_TABLE_CONSTRAINTS,
  SCH_TABLE_NAMES,
  SCH_TABLE_PRIVILEGES,
  SCH_TRIGGERS,
  SCH_VARIABLES,
  SCH_VIEWS,
  SCH_USER_PRIVILEGES
};


typedef struct st_field_info
{
  const char* field_name;
  uint field_length;
  enum enum_field_types field_type;
  int value;
  bool maybe_null;
  const char* old_name;
} ST_FIELD_INFO;


struct st_table_list;
typedef class Item COND;

typedef struct st_schema_table
{
  const char* table_name;
  ST_FIELD_INFO *fields_info;
  /* Create information_schema table */
  TABLE *(*create_table)  (THD *thd, struct st_table_list *table_list);
  /* Fill table with data */
  int (*fill_table) (THD *thd, struct st_table_list *tables, COND *cond);
  /* Handle fileds for old SHOW */
  int (*old_format) (THD *thd, struct st_schema_table *schema_table);
  int (*process_table) (THD *thd, struct st_table_list *tables,
                        TABLE *table, bool res, const char *base_name,
                        const char *file_name);
  int idx_field1, idx_field2; 
  bool hidden;
} ST_SCHEMA_TABLE;


#define JOIN_TYPE_LEFT	1
#define JOIN_TYPE_RIGHT	2

#define VIEW_ALGORITHM_UNDEFINED        0
#define VIEW_ALGORITHM_TMPTABLE         1
#define VIEW_ALGORITHM_MERGE            2

/* view WITH CHECK OPTION parameter options */
#define VIEW_CHECK_NONE       0
#define VIEW_CHECK_LOCAL      1
#define VIEW_CHECK_CASCADED   2

/* result of view WITH CHECK OPTION parameter check */
#define VIEW_CHECK_OK         0
#define VIEW_CHECK_ERROR      1
#define VIEW_CHECK_SKIP       2

struct st_lex;
struct st_table_list;
class select_union;
class TMP_TABLE_PARAM;

Item *create_view_field(THD *thd, st_table_list *view, Item **field_ref,
                        const char *name);

struct Field_translator
{
  Item *item;
  const char *name;
};


typedef struct st_table_list
{
  /* link in a local table list (used by SQL_LIST) */
  struct st_table_list *next_local;
  /* link in a global list of all queries tables */
  struct st_table_list *next_global, **prev_global;
  char		*db, *alias, *table_name, *schema_table_name;
  char          *option;                /* Used by cache index  */
  Item		*on_expr;		/* Used with outer join */
  /*
    The scturcture of ON expression presented in the member above
    can be changed during certain optimizations. This member
    contains a snapshot of AND-OR structure of the ON expression
    made after permanent transformations of the parse tree, and is
    used to restore ON clause before every reexecution of a prepared
    statement or stored procedure.
  */
  Item          *prep_on_expr;
  COND_EQUAL    *cond_equal;            /* Used with outer join */
  struct st_table_list *natural_join;	/* natural join on this table*/
  /* ... join ... USE INDEX ... IGNORE INDEX */
  List<String>	*use_index, *ignore_index;
  TABLE         *table;                 /* opened table */
  /*
    select_result for derived table to pass it from table creation to table
    filling procedure
  */
  select_union  *derived_result;
  /*
    Reference from aux_tables to local list entry of main select of
    multi-delete statement:
    delete t1 from t2,t1 where t1.a<'B' and t2.b=t1.b;
    here it will be reference of first occurrence of t1 to second (as you
    can see this lists can't be merged)
  */
  st_table_list	*correspondent_table;
  st_select_lex_unit *derived;		/* SELECT_LEX_UNIT of derived table */
  ST_SCHEMA_TABLE *schema_table;        /* Information_schema table */
  st_select_lex	*schema_select_lex;
  bool schema_table_reformed;
  TMP_TABLE_PARAM *schema_table_param;
  /* link to select_lex where this table was used */
  st_select_lex	*select_lex;
  st_lex	*view;			/* link on VIEW lex for merging */
  Field_translator *field_translation;	/* array of VIEW fields */
  /* pointer to element after last one in translation table above */
  Field_translator *field_translation_end;
  /* list of ancestor(s) of this table (underlying table(s)/view(s) */
  st_table_list	*ancestor;
  /* most upper view this table belongs to */
  st_table_list	*belong_to_view;
  /* list of join table tree leaves */
  st_table_list	*next_leaf;
  Item          *where;                 /* VIEW WHERE clause condition */
  Item          *check_option;          /* WITH CHECK OPTION condition */
  LEX_STRING	query;			/* text of (CRETE/SELECT) statement */
  LEX_STRING	md5;			/* md5 of query text */
  LEX_STRING	source;			/* source of CREATE VIEW */
  LEX_STRING	view_db;		/* saved view database */
  LEX_STRING	view_name;		/* saved view name */
  LEX_STRING	timestamp;		/* GMT time stamp of last operation */
  ulonglong	file_version;		/* version of file's field set */
  ulonglong     updatable_view;         /* VIEW can be updated */
  ulonglong	revision;		/* revision control number */
  ulonglong	algorithm;		/* 0 any, 1 tmp tables , 2 merging */
  ulonglong     with_check;             /* WITH CHECK OPTION */
  /*
    effective value of WITH CHECK OPTION (differ for temporary table
    algorithm)
  */
  uint8         effective_with_check;
  uint8         effective_algorithm;    /* which algorithm was really used */
  GRANT_INFO	grant;
  /* data need by some engines in query cache*/
  ulonglong     engine_data;
  /* call back function for asking handler about caching in query cache */
  qc_engine_callback callback_func;
  thr_lock_type lock_type;
  uint		outer_join;		/* Which join type */
  uint		shared;			/* Used in multi-upd */
  uint32        db_length, table_name_length;
  bool          updatable;		/* VIEW/TABLE can be updated now */
  bool		straight;		/* optimize with prev table */
  bool          updating;               /* for replicate-do/ignore table */
  bool		force_index;		/* prefer index over table scan */
  bool          ignore_leaves;          /* preload only non-leaf nodes */
  table_map     dep_tables;             /* tables the table depends on      */
  table_map     on_expr_dep_tables;     /* tables on expression depends on  */
  struct st_nested_join *nested_join;   /* if the element is a nested join  */
  st_table_list *embedding;             /* nested join containing the table */
  List<struct st_table_list> *join_list;/* join list the table belongs to   */
  bool		cacheable_table;	/* stop PS caching */
  /* used in multi-upd/views privilege check */
  bool		table_in_first_from_clause;
  bool		skip_temporary;		/* this table shouldn't be temporary */
  /* TRUE if this merged view contain auto_increment field */
  bool          contain_auto_increment;
  bool          multitable_view;        /* TRUE iff this is multitable view */
  /* view where processed */
  bool          where_processed;
  /* FRMTYPE_ERROR if any type is acceptable */
  enum frm_type_enum required_type;
  char		timestamp_buffer[20];	/* buffer for timestamp (19+1) */
  /*
    This TABLE_LIST object is just placeholder for prelocking, it will be
    used for implicit LOCK TABLES only and won't be used in real statement.
  */
  bool          prelocking_placeholder;

  void calc_md5(char *buffer);
  void set_ancestor();
  int view_check_option(THD *thd, bool ignore_failure);
  bool setup_ancestor(THD *thd);
  void cleanup_items();
  bool placeholder() {return derived || view; }
  void print(THD *thd, String *str);
  bool check_single_table(st_table_list **table, table_map map,
                          st_table_list *view);
  bool set_insert_values(MEM_ROOT *mem_root);
  void hide_view_error(THD *thd);
  st_table_list *find_underlying_table(TABLE *table);
  inline bool prepare_check_option(THD *thd)
  {
    bool res= FALSE;
    if (effective_with_check)
      res= prep_check_option(thd, effective_with_check);
    return res;
  }
  inline bool prepare_where(THD *thd, Item **conds,
                            bool no_where_clause)
  {
    if (effective_algorithm == VIEW_ALGORITHM_MERGE)
      return prep_where(thd, conds, no_where_clause);
    return FALSE;
  }
private:
  bool prep_check_option(THD *thd, uint8 check_opt_type);
  bool prep_where(THD *thd, Item **conds, bool no_where_clause);
} TABLE_LIST;

class Item;

class Field_iterator: public Sql_alloc
{
public:
  virtual ~Field_iterator() {}
  virtual void set(TABLE_LIST *)= 0;
  virtual void next()= 0;
  virtual bool end_of_fields()= 0;              /* Return 1 at end of list */
  virtual const char *name()= 0;
  virtual Item *create_item(THD *)= 0;
  virtual Field *field()= 0;
};


class Field_iterator_table: public Field_iterator
{
  Field **ptr;
public:
  Field_iterator_table() :ptr(0) {}
  void set(TABLE_LIST *table) { ptr= table->table->field; }
  void set_table(TABLE *table) { ptr= table->field; }
  void next() { ptr++; }
  bool end_of_fields() { return *ptr == 0; }
  const char *name();
  Item *create_item(THD *thd);
  Field *field() { return *ptr; }
};


class Field_iterator_view: public Field_iterator
{
  Field_translator *ptr, *array_end;
  TABLE_LIST *view;
public:
  Field_iterator_view() :ptr(0), array_end(0) {}
  void set(TABLE_LIST *table);
  void next() { ptr++; }
  bool end_of_fields() { return ptr == array_end; }
  const char *name();
  Item *create_item(THD *thd);
  Item **item_ptr() {return &ptr->item; }
  Field *field() { return 0; }

  inline Item *item() { return ptr->item; }
};


typedef struct st_nested_join
{
  List<TABLE_LIST>  join_list;       /* list of elements in the nested join */
  table_map         used_tables;     /* bitmap of tables in the nested join */
  table_map         not_null_tables; /* tables that rejects nulls           */
  struct st_join_table *first_nested;/* the first nested table in the plan  */
  uint              counter;         /* to count tables in the nested join  */
} NESTED_JOIN;


typedef struct st_changed_table_list
{
  struct	st_changed_table_list *next;
  char		*key;
  uint32        key_length;
} CHANGED_TABLE_LIST;


typedef struct st_open_table_list{
  struct st_open_table_list *next;
  char	*db,*table;
  uint32 in_use,locked;
} OPEN_TABLE_LIST;


