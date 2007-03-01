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


#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

/* class for the the myisam handler */

#include <db.h>

#define BDB_HIDDEN_PRIMARY_KEY_LENGTH 5

typedef struct st_berkeley_share {
  ulonglong auto_ident;
  ha_rows rows, org_rows;
  ulong *rec_per_key;
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  DB *status_block, *file, **key_file;
  u_int32_t *key_type;
  uint table_name_length,use_count;
  uint status,version;
  uint ref_length;
  bool fixed_length_primary_key, fixed_length_row;
} BDB_SHARE;


class ha_berkeley: public handler
{
  THR_LOCK_DATA lock;
  DBT last_key,current_row;
  gptr alloc_ptr;
  byte *rec_buff;
  char *key_buff, *key_buff2, *primary_key_buff;
  DB *file, **key_file;
  DB_TXN *transaction;
  u_int32_t *key_type;
  DBC *cursor;
  BDB_SHARE *share;
  ulong int_table_flags;
  ulong alloced_rec_buff_length;
  ulong changed_rows;
  uint primary_key,last_dup_key, hidden_primary_key, version;
  bool key_read, using_ignore;
  bool fix_rec_buff_for_blob(ulong length);
  byte current_ident[BDB_HIDDEN_PRIMARY_KEY_LENGTH];

  ulong max_row_length(const byte *buf);
  int pack_row(DBT *row,const  byte *record, bool new_row);
  void unpack_row(char *record, DBT *row);
  void unpack_key(char *record, DBT *key, uint index);
  DBT *create_key(DBT *key, uint keynr, char *buff, const byte *record,
		  int key_length = MAX_KEY_LENGTH);
  DBT *pack_key(DBT *key, uint keynr, char *buff, const byte *key_ptr,
		uint key_length);
  int remove_key(DB_TXN *trans, uint keynr, const byte *record, DBT *prim_key);
  int remove_keys(DB_TXN *trans,const byte *record, DBT *new_record,
		  DBT *prim_key, key_map *keys);
  int restore_keys(DB_TXN *trans, key_map *changed_keys, uint primary_key,
		   const byte *old_row, DBT *old_key,
		   const byte *new_row, DBT *new_key);
  int key_cmp(uint keynr, const byte * old_row, const byte * new_row);
  int update_primary_key(DB_TXN *trans, bool primary_key_changed,
			 const byte * old_row, DBT *old_key,
			 const byte * new_row, DBT *prim_key,
			 bool local_using_ignore);
  int read_row(int error, char *buf, uint keynr, DBT *row, DBT *key, bool);
  DBT *get_pos(DBT *to, byte *pos);

 public:
  ha_berkeley(TABLE *table_arg);
  ~ha_berkeley() {}
  const char *table_type() const { return "BerkeleyDB"; }
  ulong index_flags(uint idx, uint part, bool all_parts) const;
  const char *index_type(uint key_number) { return "BTREE"; }
  const char **bas_ext() const;
  ulong table_flags(void) const { return int_table_flags; }
  uint max_supported_keys()        const { return MAX_KEY-1; }
  uint extra_rec_buf_length()	 { return BDB_HIDDEN_PRIMARY_KEY_LENGTH; }
  ha_rows estimate_rows_upper_bound();
  uint max_supported_key_length() const { return UINT_MAX32; }
  uint max_supported_key_part_length() const { return UINT_MAX32; }

  const key_map *keys_to_use_for_scanning() { return &key_map_full; }
  bool has_transactions()  { return 1;}

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  double scan_time();
  int write_row(byte * buf);
  int update_row(const byte * old_data, byte * new_data);
  int delete_row(const byte * buf);
  int index_init(uint index);
  int index_end();
  int index_read(byte * buf, const byte * key,
		 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(byte * buf, uint index, const byte * key,
		     uint key_len, enum ha_rkey_function find_flag);
  int index_read_last(byte * buf, const byte * key, uint key_len);
  int index_next(byte * buf);
  int index_next_same(byte * buf, const byte *key, uint keylen);
  int index_prev(byte * buf);
  int index_first(byte * buf);
  int index_last(byte * buf);
  int rnd_init(bool scan);
  int rnd_end();
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  void position(const byte *record);
  int info(uint);
  int extra(enum ha_extra_function operation);
  int reset(void);
  int external_lock(THD *thd, int lock_type);
  int start_stmt(THD *thd, thr_lock_type lock_type);
  void position(byte *record);
  int analyze(THD* thd,HA_CHECK_OPT* check_opt);
  int optimize(THD* thd, HA_CHECK_OPT* check_opt);
  int check(THD* thd, HA_CHECK_OPT* check_opt);

  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  int create(const char *name, register TABLE *form,
	     HA_CREATE_INFO *create_info);
  int delete_table(const char *name);
  int rename_table(const char* from, const char* to);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);

  void get_status();
  void get_auto_primary_key(byte *to);
  ulonglong get_auto_increment();
  void print_error(int error, myf errflag);
  uint8 table_cache_type() { return HA_CACHE_TBL_TRANSACT; }
  bool primary_key_is_clustered() { return true; }
  int cmp_ref(const byte *ref1, const byte *ref2);
};

extern bool berkeley_shared_data;
extern u_int32_t berkeley_init_flags,berkeley_env_flags, berkeley_lock_type,
                 berkeley_lock_types[];
extern ulong berkeley_cache_size, berkeley_max_lock, berkeley_log_buffer_size;
extern char *berkeley_home, *berkeley_tmpdir, *berkeley_logdir;
extern long berkeley_lock_scan_time;
extern TYPELIB berkeley_lock_typelib;

bool berkeley_init(void);
bool berkeley_end(void);
bool berkeley_flush_logs(void);
int berkeley_show_logs(Protocol *protocol);
