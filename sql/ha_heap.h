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

/* class for the the heap handler */

#include <heap.h>

class ha_heap: public handler
{
  HP_INFO *file;
  key_map btree_keys;
  /* number of records changed since last statistics update */
  uint    records_changed;
  uint    key_stat_version;
public:
  ha_heap(TABLE *table);
  ~ha_heap() {}
  handler *clone(MEM_ROOT *mem_root);
  const char *table_type() const
  {
    return (table->in_use->variables.sql_mode & MODE_MYSQL323) ?
           "HEAP" : "MEMORY";
  }
  const char *index_type(uint inx)
  {
    return ((table->key_info[inx].algorithm == HA_KEY_ALG_BTREE) ? "BTREE" :
	    "HASH");
  }
  /* Rows also use a fixed-size format */
  enum row_type get_row_type() const { return ROW_TYPE_FIXED; }
  const char **bas_ext() const;
  ulong table_flags() const
  {
    return (HA_FAST_KEY_READ | HA_NO_BLOBS | HA_NULL_IN_KEY |
            HA_REC_NOT_IN_SEQ | HA_READ_RND_SAME |
            HA_CAN_INSERT_DELAYED);
  }
  ulong index_flags(uint inx, uint part, bool all_parts) const
  {
    return ((table->key_info[inx].algorithm == HA_KEY_ALG_BTREE) ?
            HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER | HA_READ_RANGE :
            HA_ONLY_WHOLE_INDEX | HA_KEY_SCAN_NOT_ROR);
  }
  const key_map *keys_to_use_for_scanning() { return &btree_keys; }
  uint max_supported_keys()          const { return MAX_KEY; }
  uint max_supported_key_part_length() const { return MAX_KEY_LENGTH; }
  double scan_time() { return (double) (records+deleted) / 20.0+10; }
  double read_time(uint index, uint ranges, ha_rows rows)
  { return (double) rows /  20.0+1; }

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  void set_keys_for_scanning(void);
  int write_row(byte * buf);
  int update_row(const byte * old_data, byte * new_data);
  int delete_row(const byte * buf);
  ulonglong get_auto_increment();
  int index_read(byte * buf, const byte * key,
		 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(byte * buf, uint idx, const byte * key,
		     uint key_len, enum ha_rkey_function find_flag);
  int index_read_last(byte * buf, const byte * key, uint key_len);
  int index_next(byte * buf);
  int index_prev(byte * buf);
  int index_first(byte * buf);
  int index_last(byte * buf);
  int rnd_init(bool scan);
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  void position(const byte *record);
  int info(uint);
  int extra(enum ha_extra_function operation);
  int external_lock(THD *thd, int lock_type);
  int delete_all_rows(void);
  int disable_indexes(uint mode);
  int enable_indexes(uint mode);
  int indexes_are_disabled(void);
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  int delete_table(const char *from);
  int rename_table(const char * from, const char * to);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);
  void update_create_info(HA_CREATE_INFO *create_info);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);
  int cmp_ref(const byte *ref1, const byte *ref2)
  {
    return memcmp(ref1, ref2, sizeof(HEAP_PTR));
  }
private:
  void update_key_stats();
};
