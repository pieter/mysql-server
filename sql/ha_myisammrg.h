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

/* class for the the myisam merge handler */

#include <myisammrg.h>

class ha_myisammrg: public handler
{
  MYRG_INFO *file;

 public:
  ha_myisammrg(TABLE *table_arg);
  ~ha_myisammrg() {}
  const char *table_type() const { return "MRG_MyISAM"; }
  const char **bas_ext() const;
  const char *index_type(uint key_number);
  ulong table_flags() const
  {
    return (HA_REC_NOT_IN_SEQ | HA_AUTO_PART_KEY | HA_READ_RND_SAME |
	    HA_NULL_IN_KEY | HA_CAN_INDEX_BLOBS | HA_FILE_BASED |
            HA_ANY_INDEX_MAY_BE_UNIQUE | HA_CAN_BIT_FIELD);
  }
  ulong index_flags(uint inx, uint part, bool all_parts) const
  {
    return ((table->key_info[inx].algorithm == HA_KEY_ALG_FULLTEXT) ?
            0 : HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE |
            HA_READ_ORDER | HA_KEYREAD_ONLY);
  }
  uint max_supported_keys()          const { return MI_MAX_KEY; }
  uint max_supported_key_length()    const { return MI_MAX_KEY_LENGTH; }
  uint max_supported_key_part_length() const { return MI_MAX_KEY_LENGTH; }
  double scan_time()
    { return ulonglong2double(data_file_length) / IO_SIZE + file->tables; }

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  int write_row(byte * buf);
  int update_row(const byte * old_data, byte * new_data);
  int delete_row(const byte * buf);
  int index_read(byte * buf, const byte * key,
		 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(byte * buf, uint idx, const byte * key,
		     uint key_len, enum ha_rkey_function find_flag);
  int index_read_last(byte * buf, const byte * key, uint key_len);
  int index_next(byte * buf);
  int index_prev(byte * buf);
  int index_first(byte * buf);
  int index_last(byte * buf);
  int index_next_same(byte *buf, const byte *key, uint keylen);
  int rnd_init(bool scan);
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  void position(const byte *record);
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  int info(uint);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, ulong cache_size);
  int external_lock(THD *thd, int lock_type);
  uint lock_count(void) const;
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);
  void update_create_info(HA_CREATE_INFO *create_info);
  void append_create_info(String *packet);
  MYRG_INFO *myrg_info() { return file; }
  int check(THD* thd, HA_CHECK_OPT* check_opt);
};
