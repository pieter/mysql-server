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
  This file defines the NDB Cluster handler: the interface between MySQL and
  NDB Cluster
*/

/* The class defining a handle to an NDB Cluster table */

#ifdef __GNUC__
#pragma interface                       /* gcc class implementation */
#endif

#include <ndbapi_limits.h>

class Ndb;             // Forward declaration
class NdbOperation;    // Forward declaration
class NdbTransaction;  // Forward declaration
class NdbRecAttr;      // Forward declaration
class NdbScanOperation; 
class NdbScanFilter; 
class NdbIndexScanOperation; 
class NdbBlob;

// connectstring to cluster if given by mysqld
extern const char *ndbcluster_connectstring;

typedef enum ndb_index_type {
  UNDEFINED_INDEX = 0,
  PRIMARY_KEY_INDEX = 1,
  PRIMARY_KEY_ORDERED_INDEX = 2,
  UNIQUE_INDEX = 3,
  UNIQUE_ORDERED_INDEX = 4,
  ORDERED_INDEX = 5
} NDB_INDEX_TYPE;

typedef struct ndb_index_data {
  NDB_INDEX_TYPE type;
  void *index;
  void *unique_index;
} NDB_INDEX_DATA;

typedef struct st_ndbcluster_share {
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length,use_count;
} NDB_SHARE;

typedef enum ndb_item_type {
  NDB_VALUE = 0,   // Qualified more with Item::Type
  NDB_FIELD = 1,   // Qualified from table definition
  NDB_FUNCTION = 2,// Qualified from Item_func::Functype
  NDB_END_COND = 3 // End marker for condition group
} NDB_ITEM_TYPE;

typedef union ndb_item_qualification {
  Item::Type value_type; 
  enum_field_types field_type;       // Instead of Item::FIELD_ITEM
  Item_func::Functype function_type; // Instead of Item::FUNC_ITEM
} NDB_ITEM_QUALIFICATION;

class Ndb_item_string_value {
 public:
  String s;
  CHARSET_INFO *c;
};

typedef struct ndb_item_field_value {
  Field* field;
  int column_no;
} NDB_ITEM_FIELD_VALUE;

typedef union ndb_item_value {
  longlong int_value;
  double real_value;
  Ndb_item_string_value *string_value;
  NDB_ITEM_FIELD_VALUE *field_value;
} NDB_ITEM_VALUE;

class Ndb_item {
 public:
  Ndb_item(NDB_ITEM_TYPE item_type);
  Ndb_item(NDB_ITEM_TYPE item_type, 
	   NDB_ITEM_QUALIFICATION item_qualification,
	   const Item *item_value);
  Ndb_item(longlong int_value);
  Ndb_item(double real_value);
  Ndb_item(Field *field, int column_no);
  Ndb_item(Item_func::Functype func_type);
  ~Ndb_item();
  void print(String *str);
  uint32 pack_length() { return value.field_value->field->pack_length(); };
  // Getters and Setters
  longlong get_int_value() { return value.int_value; };
  double get_real_value() { return value.real_value; };
  String * get_string_value() { return &value.string_value->s; };
  CHARSET_INFO * get_string_charset() { return value.string_value->c; };
  Field * get_field() { return value.field_value->field; };
  int get_field_no() { return value.field_value->column_no; };

  const void * get_value()
  {      
    switch(qualification.value_type) {
    case(Item::INT_ITEM): {
      return (void *) &value.int_value;
    } 
    case(Item::REAL_ITEM): {
      return (void *) &value.real_value;
      break;
    }
    case(Item::STRING_ITEM): 
    case(Item::VARBIN_ITEM): {	
      return  value.string_value->s.ptr();
    }
    default:
      break;
    }

    return NULL;
  }
    
 public:
  NDB_ITEM_TYPE type;
  NDB_ITEM_QUALIFICATION qualification;


 private:
  NDB_ITEM_VALUE value;

};

class Ndb_cond {
 public:
  Ndb_cond() : ndb_item(NULL), next(NULL), prev(NULL) {};
  ~Ndb_cond() 
  { 
    if (ndb_item) delete ndb_item; 
    ndb_item= NULL; 
    if (next) delete next;
    next= prev= NULL; 
  };
  Ndb_item *ndb_item;
  Ndb_cond *next;
  Ndb_cond *prev;
};

class Ndb_cond_stack {
 public:
  Ndb_cond_stack() : ndb_cond(NULL), next(NULL) {};
  ~Ndb_cond_stack() 
  { 
    if (ndb_cond) delete ndb_cond; 
    ndb_cond= NULL; 
    next= NULL; 
  };
  Ndb_cond *ndb_cond;
  Ndb_cond_stack *next;
};

class Ndb_cond_traverse_context {
 public:
  Ndb_cond_traverse_context(TABLE *tab, void* ndb_tab, 
			    bool *supported, Ndb_cond_stack* stack)
    : table(tab), ndb_table(ndb_tab), 
    supported_ptr(supported), stack_ptr(stack), cond_ptr(NULL),
    expect_mask(0), expect_field_result_mask(0)
  {
    if (stack)
      cond_ptr= stack->ndb_cond;
  };
  void expect(Item::Type type)
  {
    expect_mask|= (1 << type);
  };
  void dont_expect(Item::Type type)
  {
    expect_mask&= ~(1 << type);
  };
  bool expecting(Item::Type type)
  {
    return (expect_mask & (1 << type));
  };
  void expect_nothing()
  {
    expect_mask= 0;
  };
  void expect_only(Item::Type type)
  {
    expect_mask= 0;
    expect(type);
  };

  void expect_field_result(Item_result result)
  {
    expect_field_result_mask|= (1 << result);
  };
  bool expecting_field_result(Item_result result)
  {
    return (expect_field_result_mask & (1 << result));
  };
  void expect_no_field_result()
  {
    expect_field_result_mask= 0;
  };
  void expect_only_field_result(Item_result result)
  {
    expect_field_result_mask= 0;
    expect_field_result(result);
  };

  TABLE* table;
  void* ndb_table;
  bool *supported_ptr;
  Ndb_cond_stack* stack_ptr;
  Ndb_cond* cond_ptr;
  uint expect_mask;
  uint expect_field_result_mask;
};

/*
  Place holder for ha_ndbcluster thread specific data
*/

class Thd_ndb {
 public:
  Thd_ndb();
  ~Thd_ndb();
  Ndb *ndb;
  ulong count;
  uint lock_count;
  int error;
};

class ha_ndbcluster: public handler
{
 public:
  ha_ndbcluster(TABLE *table);
  ~ha_ndbcluster();

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);

  int write_row(byte *buf);
  int update_row(const byte *old_data, byte *new_data);
  int delete_row(const byte *buf);
  int index_init(uint index);
  int index_end();
  int index_read(byte *buf, const byte *key, uint key_len, 
		 enum ha_rkey_function find_flag);
  int index_read_idx(byte *buf, uint index, const byte *key, uint key_len, 
		     enum ha_rkey_function find_flag);
  int index_next(byte *buf);
  int index_prev(byte *buf);
  int index_first(byte *buf);
  int index_last(byte *buf);
  int index_read_last(byte * buf, const byte * key, uint key_len);
  int rnd_init(bool scan);
  int rnd_end();
  int rnd_next(byte *buf);
  int rnd_pos(byte *buf, byte *pos);
  void position(const byte *record);
  int read_range_first(const key_range *start_key,
		       const key_range *end_key,
		       bool eq_range, bool sorted);
  int read_range_first_to_buf(const key_range *start_key,
			      const key_range *end_key,
			      bool eq_range, bool sorted,
			      byte* buf);
  int read_range_next();

  /**
   * Multi range stuff
   */
  int read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
			     KEY_MULTI_RANGE*ranges, uint range_count,
			     bool sorted, HANDLER_BUFFER *buffer);
  int read_multi_range_next(KEY_MULTI_RANGE **found_range_p);

  bool get_error_message(int error, String *buf);
  void info(uint);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, ulong cache_size);
  int external_lock(THD *thd, int lock_type);
  int start_stmt(THD *thd);
  const char * table_type() const;
  const char ** bas_ext() const;
  ulong table_flags(void) const;
  ulong index_flags(uint idx, uint part, bool all_parts) const;
  uint max_supported_record_length() const;
  uint max_supported_keys() const;
  uint max_supported_key_parts() const;
  uint max_supported_key_length() const;

  int rename_table(const char *from, const char *to);
  int delete_table(const char *name);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *info);
  THR_LOCK_DATA **store_lock(THD *thd,
			     THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);

  bool low_byte_first() const;
  bool has_transactions();
  const char* index_type(uint key_number);

  double scan_time();
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();

  static Thd_ndb* seize_thd_ndb();
  static void release_thd_ndb(Thd_ndb* thd_ndb);
 
  /*
    Condition pushdown
  */
  const COND *cond_push(const COND *cond);
  void cond_pop();

  uint8 table_cache_type();
    
 private:
  int alter_table_name(const char *to);
  int drop_table();
  int create_index(const char *name, KEY *key_info, bool unique);
  int create_ordered_index(const char *name, KEY *key_info);
  int create_unique_index(const char *name, KEY *key_info);
  int initialize_autoincrement(const void *table);
  enum ILBP {ILBP_CREATE = 0, ILBP_OPEN = 1}; // Index List Build Phase
  int build_index_list(TABLE *tab, enum ILBP phase);
  int get_metadata(const char* path);
  void release_metadata();
  NDB_INDEX_TYPE get_index_type(uint idx_no) const;
  NDB_INDEX_TYPE get_index_type_from_table(uint index_no) const;
  int check_index_fields_not_null(uint index_no);

  int pk_read(const byte *key, uint key_len, byte *buf);
  int complemented_pk_read(const byte *old_data, byte *new_data);
  int peek_row();
  int unique_index_read(const byte *key, uint key_len, 
			byte *buf);
  int ordered_index_scan(const key_range *start_key,
			 const key_range *end_key,
			 bool sorted, bool descending, byte* buf);
  int full_table_scan(byte * buf);
  int fetch_next(NdbScanOperation* op);
  int next_result(byte *buf); 
  int define_read_attrs(byte* buf, NdbOperation* op);
  int filtered_scan(const byte *key, uint key_len, 
		    byte *buf,
		    enum ha_rkey_function find_flag);
  int close_scan();
  void unpack_record(byte *buf);
  int get_ndb_lock_type(enum thr_lock_type type);

  void set_dbname(const char *pathname);
  void set_tabname(const char *pathname);
  void set_tabname(const char *pathname, char *tabname);

  bool set_hidden_key(NdbOperation*,
		      uint fieldnr, const byte* field_ptr);
  int set_ndb_key(NdbOperation*, Field *field,
		  uint fieldnr, const byte* field_ptr);
  int set_ndb_value(NdbOperation*, Field *field, uint fieldnr, bool *set_blob_value= 0);
  int get_ndb_value(NdbOperation*, Field *field, uint fieldnr, byte*);
  friend int g_get_ndb_blobs_value(NdbBlob *ndb_blob, void *arg);
  int get_ndb_blobs_value(NdbBlob *last_ndb_blob);
  int set_primary_key(NdbOperation *op, const byte *key);
  int set_primary_key(NdbOperation *op);
  int set_primary_key_from_old_data(NdbOperation *op, const byte *old_data);
  int set_bounds(NdbIndexScanOperation*, const key_range *keys[2], uint= 0);
  int key_cmp(uint keynr, const byte * old_row, const byte * new_row);
  int set_index_key(NdbOperation *, const KEY *key_info, const byte *key_ptr);
  void print_results();

  ulonglong get_auto_increment();
  int ndb_err(NdbTransaction*);
  bool uses_blob_value(bool all_fields);

  int write_ndb_file();

  int check_ndb_connection();

  void set_rec_per_key();
  void records_update();
  void no_uncommitted_rows_execute_failure();
  void no_uncommitted_rows_update(int);
  void no_uncommitted_rows_init(THD *);
  void no_uncommitted_rows_reset(THD *);

  /*
    Condition Pushdown to Handler (CPDH), private methods
  */
  void cond_clear();
  bool serialize_cond(const COND *cond, Ndb_cond_stack *ndb_cond);
  int build_scan_filter_predicate(Ndb_cond* &cond, 
				  NdbScanFilter* filter);
  int build_scan_filter_group(Ndb_cond* &cond, 
			      NdbScanFilter* filter);
  int build_scan_filter(Ndb_cond* &cond, NdbScanFilter* filter);
  int generate_scan_filter(Ndb_cond_stack* cond_stack, 
			   NdbScanOperation* op);

  friend int execute_commit(ha_ndbcluster*, NdbTransaction*);
  friend int execute_no_commit(ha_ndbcluster*, NdbTransaction*);
  friend int execute_no_commit_ie(ha_ndbcluster*, NdbTransaction*);

  NdbTransaction *m_active_trans;
  NdbScanOperation *m_active_cursor;
  void *m_table;
  void *m_table_info;
  char m_dbname[FN_HEADLEN];
  //char m_schemaname[FN_HEADLEN];
  char m_tabname[FN_HEADLEN];
  ulong m_table_flags;
  THR_LOCK_DATA m_lock;
  NDB_SHARE *m_share;
  NDB_INDEX_DATA  m_index[MAX_KEY];
  // NdbRecAttr has no reference to blob
  typedef union { const NdbRecAttr *rec; NdbBlob *blob; void *ptr; } NdbValue;
  NdbValue m_value[NDB_MAX_ATTRIBUTES_IN_TABLE];
  bool m_use_write;
  bool m_ignore_dup_key;
  bool m_primary_key_update;
  bool m_retrieve_all_fields;
  bool m_retrieve_primary_key;
  ha_rows m_rows_to_insert;
  ha_rows m_rows_inserted;
  ha_rows m_bulk_insert_rows;
  bool m_bulk_insert_not_flushed;
  ha_rows m_ops_pending;
  bool m_skip_auto_increment;
  bool m_blobs_pending;
  // memory for blobs in one tuple
  char *m_blobs_buffer;
  uint32 m_blobs_buffer_size;
  uint m_dupkey;
  // set from thread variables at external lock
  bool m_ha_not_exact_count;
  bool m_force_send;
  ha_rows m_autoincrement_prefetch;
  bool m_transaction_on;
  bool m_use_local_query_cache;
  Ndb_cond_stack *m_cond_stack;
  bool m_disable_multi_read;
  byte *m_multi_range_result_ptr;
  KEY_MULTI_RANGE *m_multi_ranges;
  KEY_MULTI_RANGE *m_multi_range_defined;
  const NdbOperation *m_current_multi_operation;
  NdbIndexScanOperation *m_multi_cursor;
  byte *m_multi_range_cursor_result_ptr;
  int setup_recattr(const NdbRecAttr*);
  Ndb *get_ndb();
};

bool ndbcluster_init(void);
bool ndbcluster_end(void);

int ndbcluster_commit(THD *thd, void* ndb_transaction);
int ndbcluster_rollback(THD *thd, void* ndb_transaction);

void ndbcluster_close_connection(THD *thd);

int ndbcluster_discover(THD* thd, const char* dbname, const char* name,
			const void** frmblob, uint* frmlen);
int ndbcluster_find_files(THD *thd,const char *db,const char *path,
			  const char *wild, bool dir, List<char> *files);
int ndbcluster_table_exists(THD* thd, const char *db, const char *name);
int ndbcluster_drop_database(const char* path);

void ndbcluster_print_error(int error, const NdbOperation *error_op);
