/* Copyright (C) 2000,2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <myisampack.h>
#include "ha_heap.h"

/*****************************************************************************
** HEAP tables
*****************************************************************************/

const char **ha_heap::bas_ext() const
{ static const char *ext[1]= { NullS }; return ext; }


int ha_heap::open(const char *name, int mode, uint test_if_locked)
{
  if (!(file= heap_open(name, mode)) && my_errno == ENOENT)
  {
    HA_CREATE_INFO create_info;
    bzero(&create_info, sizeof(create_info));
    if (!create(name, table, &create_info))
    {
      file= heap_open(name, mode);
      implicit_emptied= 1;
    }
  }
  ref_length= sizeof(HEAP_PTR);
  if (file)
  {
    /* Initialize variables for the opened table */
    set_keys_for_scanning();
  }
  return (file ? 0 : 1);
}

int ha_heap::close(void)
{
  return heap_close(file);
}


/*
  Compute which keys to use for scanning

  SYNOPSIS
    set_keys_for_scanning()
    no parameter

  DESCRIPTION
    Set the bitmap btree_keys, which is used when the upper layers ask
    which keys to use for scanning. For each btree index the
    corresponding bit is set.

  RETURN
    void
*/

void ha_heap::set_keys_for_scanning(void)
{
  btree_keys.clear_all();
  for (uint i= 0 ; i < table->keys ; i++)
  {
    if (table->key_info[i].algorithm == HA_KEY_ALG_BTREE)
      btree_keys.set_bit(i);
  }
}

int ha_heap::write_row(byte * buf)
{
  statistic_increment(ha_write_count,&LOCK_status);
  if (table->timestamp_default_now)
    update_timestamp(buf+table->timestamp_default_now-1);
  if (table->next_number_field && buf == table->record[0])
    update_auto_increment();
  return heap_write(file,buf);
}

int ha_heap::update_row(const byte * old_data, byte * new_data)
{
  statistic_increment(ha_update_count,&LOCK_status);
  if (table->timestamp_on_update_now)
    update_timestamp(new_data+table->timestamp_on_update_now-1);
  return heap_update(file,old_data,new_data);
}

int ha_heap::delete_row(const byte * buf)
{
  statistic_increment(ha_delete_count,&LOCK_status);
  return heap_delete(file,buf);
}

int ha_heap::index_read(byte * buf, const byte * key, uint key_len,
			enum ha_rkey_function find_flag)
{
  statistic_increment(ha_read_key_count, &LOCK_status);
  int error = heap_rkey(file,buf,active_index, key, key_len, find_flag);
  table->status = error ? STATUS_NOT_FOUND : 0;
  return error;
}

int ha_heap::index_read_last(byte *buf, const byte *key, uint key_len)
{
  statistic_increment(ha_read_key_count, &LOCK_status);
  int error= heap_rkey(file, buf, active_index, key, key_len,
		       HA_READ_PREFIX_LAST);
  table->status= error ? STATUS_NOT_FOUND : 0;
  return error;
}

int ha_heap::index_read_idx(byte * buf, uint index, const byte * key,
			    uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(ha_read_key_count, &LOCK_status);
  int error = heap_rkey(file, buf, index, key, key_len, find_flag);
  table->status = error ? STATUS_NOT_FOUND : 0;
  return error;
}

int ha_heap::index_next(byte * buf)
{
  statistic_increment(ha_read_next_count,&LOCK_status);
  int error=heap_rnext(file,buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::index_prev(byte * buf)
{
  statistic_increment(ha_read_prev_count,&LOCK_status);
  int error=heap_rprev(file,buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::index_first(byte * buf)
{
  statistic_increment(ha_read_first_count,&LOCK_status);
  int error=heap_rfirst(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::index_last(byte * buf)
{
  statistic_increment(ha_read_last_count,&LOCK_status);
  int error=heap_rlast(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::rnd_init(bool scan)
{
  return scan ? heap_scan_init(file) : 0;
}

int ha_heap::rnd_next(byte *buf)
{
  statistic_increment(ha_read_rnd_next_count,&LOCK_status);
  int error=heap_scan(file, buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::rnd_pos(byte * buf, byte *pos)
{
  int error;
  HEAP_PTR position;
  statistic_increment(ha_read_rnd_count,&LOCK_status);
  memcpy_fixed((char*) &position,pos,sizeof(HEAP_PTR));
  error=heap_rrnd(file, buf, position);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

void ha_heap::position(const byte *record)
{
  *(HEAP_PTR*) ref= heap_position(file);	// Ref is aligned
}

void ha_heap::info(uint flag)
{
  HEAPINFO info;
  (void) heap_info(file,&info,flag);

  records = info.records;
  deleted = info.deleted;
  errkey  = info.errkey;
  mean_rec_length=info.reclength;
  data_file_length=info.data_length;
  index_file_length=info.index_length;
  max_data_file_length= info.max_records* info.reclength;
  delete_length= info.deleted * info.reclength;
  if (flag & HA_STATUS_AUTO)
    auto_increment_value= info.auto_increment;
}

int ha_heap::extra(enum ha_extra_function operation)
{
  return heap_extra(file,operation);
}

int ha_heap::delete_all_rows()
{
  heap_clear(file);
  return 0;
}

int ha_heap::external_lock(THD *thd, int lock_type)
{
  return 0;					// No external locking
}


/*
  Disable indexes.

  SYNOPSIS
    disable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      disable all non-unique keys
                HA_KEY_SWITCH_ALL          disable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE dis. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     dis. all keys and make persistent

  DESCRIPTION
    Disable indexes and clear keys to use for scanning.

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_NONUNIQ_SAVE  is not implemented with HEAP.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented with HEAP.

  RETURN
    0  ok
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_heap::disable_indexes(uint mode)
{
  int error;

  if (mode == HA_KEY_SWITCH_ALL)
  {
    if (!(error= heap_disable_indexes(file)))
      set_keys_for_scanning();
  }
  else
  {
    /* mode not implemented */
    error= HA_ERR_WRONG_COMMAND;
  }
  return error;
}


/*
  Enable indexes.

  SYNOPSIS
    enable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      enable all non-unique keys
                HA_KEY_SWITCH_ALL          enable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE en. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     en. all keys and make persistent

  DESCRIPTION
    Enable indexes and set keys to use for scanning.
    The indexes might have been disabled by disable_index() before.
    The function works only if both data and indexes are empty,
    since the heap storage engine cannot repair the indexes.
    To be sure, call handler::delete_all_rows() before.

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_NONUNIQ_SAVE  is not implemented with HEAP.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented with HEAP.

  RETURN
    0  ok
    HA_ERR_CRASHED  data or index is non-empty. Delete all rows and retry.
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_heap::enable_indexes(uint mode)
{
  int error;

  if (mode == HA_KEY_SWITCH_ALL)
  {
    if (!(error= heap_enable_indexes(file)))
      set_keys_for_scanning();
  }
  else
  {
    /* mode not implemented */
    error= HA_ERR_WRONG_COMMAND;
  }
  return error;
}


/*
  Test if indexes are disabled.

  SYNOPSIS
    indexes_are_disabled()
    no parameters

  RETURN
    0  indexes are not disabled
    1  all indexes are disabled
   [2  non-unique indexes are disabled - NOT YET IMPLEMENTED]
*/

int ha_heap::indexes_are_disabled(void)
{
  return heap_indexes_are_disabled(file);
}

THR_LOCK_DATA **ha_heap::store_lock(THD *thd,
				    THR_LOCK_DATA **to,
				    enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && file->lock.type == TL_UNLOCK)
    file->lock.type=lock_type;
  *to++= &file->lock;
  return to;
}

/*
  We have to ignore ENOENT entries as the HEAP table is created on open and
  not when doing a CREATE on the table.
*/

int ha_heap::delete_table(const char *name)
{
  int error=heap_delete_table(name);
  return error == ENOENT ? 0 : error;
}

int ha_heap::rename_table(const char * from, const char * to)
{
  return heap_rename(from,to);
}


ha_rows ha_heap::records_in_range(uint inx, key_range *min_key,
                                  key_range *max_key)
{
  KEY *key=table->key_info+inx;
  if (key->algorithm == HA_KEY_ALG_BTREE)
    return hp_rb_records_in_range(file, inx, min_key, max_key);

  if (min_key->length != max_key->length ||
      min_key->length != key->key_length ||
      min_key->flag != HA_READ_KEY_EXACT ||
      max_key->flag != HA_READ_AFTER_KEY)
    return HA_POS_ERROR;			// Can only use exact keys
  return 10;					// Good guess
}


int ha_heap::create(const char *name, TABLE *table_arg,
		    HA_CREATE_INFO *create_info)
{
  uint key, parts, mem_per_row= 0;
  uint auto_key= 0, auto_key_type= 0;
  ha_rows max_rows;
  HP_KEYDEF *keydef;
  HA_KEYSEG *seg;
  char buff[FN_REFLEN];
  int error;

  for (key= parts= 0; key < table_arg->keys; key++)
    parts+= table_arg->key_info[key].key_parts;

  if (!(keydef= (HP_KEYDEF*) my_malloc(table_arg->keys * sizeof(HP_KEYDEF) +
				       parts * sizeof(HA_KEYSEG),
				       MYF(MY_WME))))
    return my_errno;
  seg= my_reinterpret_cast(HA_KEYSEG*) (keydef + table_arg->keys);
  for (key= 0; key < table_arg->keys; key++)
  {
    KEY *pos= table_arg->key_info+key;
    KEY_PART_INFO *key_part=     pos->key_part;
    KEY_PART_INFO *key_part_end= key_part + pos->key_parts;

    mem_per_row+= (pos->key_length + (sizeof(char*) * 2));

    keydef[key].keysegs=   (uint) pos->key_parts;
    keydef[key].flag=      (pos->flags & (HA_NOSAME | HA_NULL_ARE_EQUAL));
    keydef[key].seg=       seg;
    keydef[key].algorithm= ((pos->algorithm == HA_KEY_ALG_UNDEF) ? 
			    HA_KEY_ALG_HASH : pos->algorithm);

    for (; key_part != key_part_end; key_part++, seg++)
    {
      uint flag=    key_part->key_type;
      Field *field= key_part->field;
      if (pos->algorithm == HA_KEY_ALG_BTREE)
	seg->type= field->key_type();
      else
      {
	if (!f_is_packed(flag) &&
	    f_packtype(flag) == (int) FIELD_TYPE_DECIMAL &&
	    !(flag & FIELDFLAG_BINARY))
	  seg->type= (int) HA_KEYTYPE_TEXT;
	else
	  seg->type= (int) HA_KEYTYPE_BINARY;
      }
      seg->start=   (uint) key_part->offset;
      seg->length=  (uint) key_part->length;
      seg->flag =   0;
      seg->charset= field->charset();
      if (field->null_ptr)
      {
	seg->null_bit= field->null_bit;
	seg->null_pos= (uint) (field->null_ptr - (uchar*) table_arg->record[0]);
      }
      else
      {
	seg->null_bit= 0;
	seg->null_pos= 0;
      }
      if (field->flags & AUTO_INCREMENT_FLAG)
      {
	auto_key= key + 1;
	auto_key_type= field->key_type();
      }
    }
  }
  mem_per_row+= MY_ALIGN(table_arg->reclength + 1, sizeof(char*));
  max_rows = (ha_rows) (current_thd->variables.max_heap_table_size /
			mem_per_row);
  HP_CREATE_INFO hp_create_info;
  hp_create_info.auto_key= auto_key;
  hp_create_info.auto_key_type= auto_key_type;
  hp_create_info.auto_increment= (create_info->auto_increment_value ?
				  create_info->auto_increment_value - 1 : 0);
  error= heap_create(fn_format(buff,name,"","",4+2),
		     table_arg->keys,keydef, table_arg->reclength,
		     (ulong) ((table_arg->max_rows < max_rows &&
			       table_arg->max_rows) ? 
			      table_arg->max_rows : max_rows),
		     (ulong) table_arg->min_rows, &hp_create_info);
  my_free((gptr) keydef, MYF(0));
  if (file)
    info(HA_STATUS_NO_LOCK | HA_STATUS_CONST | HA_STATUS_VARIABLE);
  return (error);
}


void ha_heap::update_create_info(HA_CREATE_INFO *create_info)
{
  table->file->info(HA_STATUS_AUTO);
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO))
    create_info->auto_increment_value= auto_increment_value;
}

longlong ha_heap::get_auto_increment()
{
  ha_heap::info(HA_STATUS_AUTO);
  return auto_increment_value;
}
