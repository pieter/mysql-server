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


#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include "ha_myisammrg.h"
#ifndef MASTER
#include "../srclib/myisammrg/mymrgdef.h"
#else
#include "../myisammrg/mymrgdef.h"
#endif

/*****************************************************************************
** MyISAM MERGE tables
*****************************************************************************/

const char **ha_myisammrg::bas_ext() const
{ static const char *ext[]= { ".MRG", NullS }; return ext; }

int ha_myisammrg::open(const char *name, int mode, uint test_if_locked)
{
  char name_buff[FN_REFLEN];
  if (!(file=myrg_open(fn_format(name_buff,name,"","",2 | 4), mode,
		       test_if_locked)))
    return (my_errno ? my_errno : -1);

  if (!(test_if_locked == HA_OPEN_WAIT_IF_LOCKED ||
	test_if_locked == HA_OPEN_ABORT_IF_LOCKED))
    myrg_extra(file,HA_EXTRA_NO_WAIT_LOCK);
  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    myrg_extra(file,HA_EXTRA_WAIT_LOCK);
  if (table->reclength != mean_rec_length && mean_rec_length)
  {
    DBUG_PRINT("error",("reclength: %d  mean_rec_length: %d",
			table->reclength, mean_rec_length));
    myrg_close(file);
    file=0;
    return my_errno=HA_ERR_WRONG_TABLE_DEF;
  }
  return (0);
}

int ha_myisammrg::close(void)
{
  return myrg_close(file);
}

int ha_myisammrg::write_row(byte * buf)
{
  return (my_errno=HA_ERR_WRONG_COMMAND);
}

int ha_myisammrg::update_row(const byte * old_data, byte * new_data)
{
  statistic_increment(ha_update_count,&LOCK_status);
  if (table->time_stamp)
    update_timestamp(new_data+table->time_stamp-1);
  return myrg_update(file,old_data,new_data);
}

int ha_myisammrg::delete_row(const byte * buf)
{
  statistic_increment(ha_delete_count,&LOCK_status);
  return myrg_delete(file,buf);
}

int ha_myisammrg::index_read(byte * buf, const byte * key,
			  uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(ha_read_key_count,&LOCK_status);
  int error=myrg_rkey(file,buf,active_index, key, key_len, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_read_idx(byte * buf, uint index, const byte * key,
				 uint key_len, enum ha_rkey_function find_flag)
{
  statistic_increment(ha_read_key_count,&LOCK_status);
  int error=myrg_rkey(file,buf,index, key, key_len, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_next(byte * buf)
{
  statistic_increment(ha_read_next_count,&LOCK_status);
  int error=myrg_rnext(file,buf,active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_prev(byte * buf)
{
  statistic_increment(ha_read_prev_count,&LOCK_status);
  int error=myrg_rprev(file,buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_first(byte * buf)
{
  statistic_increment(ha_read_first_count,&LOCK_status);
  int error=myrg_rfirst(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_last(byte * buf)
{
  statistic_increment(ha_read_last_count,&LOCK_status);
  int error=myrg_rlast(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::rnd_init(bool scan)
{
  return myrg_extra(file,HA_EXTRA_RESET);
}

int ha_myisammrg::rnd_next(byte *buf)
{
  statistic_increment(ha_read_rnd_next_count,&LOCK_status);
  int error=myrg_rrnd(file, buf, HA_OFFSET_ERROR);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::rnd_pos(byte * buf, byte *pos)
{
  statistic_increment(ha_read_rnd_count,&LOCK_status);
  int error=myrg_rrnd(file, buf, ha_get_ptr(pos,ref_length));
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

void ha_myisammrg::position(const byte *record)
{
  ulonglong position= myrg_position(file);
  ha_store_ptr(ref, ref_length, (my_off_t) position);
}


void ha_myisammrg::info(uint flag)
{
  MYMERGE_INFO info;
  (void) myrg_status(file,&info,flag);
  records = (ha_rows) info.records;
  deleted = (ha_rows) info.deleted;
  data_file_length=info.data_file_length;
  errkey  = info.errkey;
  table->keys_in_use=(((key_map) 1) << table->keys)- (key_map) 1;
  table->db_options_in_use    = info.options;
  table->is_view=1;
  mean_rec_length=info.reclength;
  block_size=0;
  update_time=0;
#if SIZEOF_OFF_T > 4
  ref_length=6;					// Should be big enough
#else
  ref_length=4;					// Can't be > than my_off_t
#endif
}


int ha_myisammrg::extra(enum ha_extra_function operation)
{
  return myrg_extra(file,operation);
}

int ha_myisammrg::reset(void)
{
  return myrg_extra(file,HA_EXTRA_RESET);
}

int ha_myisammrg::external_lock(THD *thd, int lock_type)
{
  return myrg_lock_database(file,lock_type);
}

uint ha_myisammrg::lock_count(void) const
{
  return file->tables;
}


THR_LOCK_DATA **ha_myisammrg::store_lock(THD *thd,
					 THR_LOCK_DATA **to,
					 enum thr_lock_type lock_type)
{
  MYRG_TABLE *table;

  for (table=file->open_tables ; table != file->end_table ; table++)
  {
    *(to++)= &table->table->lock;
    if (lock_type != TL_IGNORE && table->table->lock.type == TL_UNLOCK)
      table->table->lock.type=lock_type;
  }
  return to;
}

void ha_myisammrg::update_create_info(HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_myisammrg::update_create_info");
  if (!(create_info->used_fields & HA_CREATE_USED_UNION))
  {
    MYRG_TABLE *table;
    THD *thd=current_thd;
    create_info->merge_list.next= &create_info->merge_list.first;
    create_info->merge_list.elements=0;

    for (table=file->open_tables ; table != file->end_table ; table++)
    {
      char *name=table->table->s->filename;
      char buff[FN_REFLEN];
      TABLE_LIST *ptr;
      if (!(ptr = (TABLE_LIST *) thd->calloc(sizeof(TABLE_LIST))))
	goto err;
      fn_format(buff,name,"","",3);
      if (!(ptr->real_name=thd->strdup(buff)))
	goto err;
      create_info->merge_list.elements++;
      (*create_info->merge_list.next) = (byte*) ptr;
      create_info->merge_list.next= (byte**) &ptr->next;
    }
    *create_info->merge_list.next=0;
  }
  DBUG_VOID_RETURN;

err:
  create_info->merge_list.elements=0;
  create_info->merge_list.first=0;
  DBUG_VOID_RETURN;
}

int ha_myisammrg::create(const char *name, register TABLE *form,
			 HA_CREATE_INFO *create_info)
{
  char buff[FN_REFLEN],**table_names,**pos;
  TABLE_LIST *tables= (TABLE_LIST*) create_info->merge_list.first;
  DBUG_ENTER("ha_myisammrg::create");

  if (!(table_names= (char**) sql_alloc((create_info->merge_list.elements+1)*
					sizeof(char*))))
    DBUG_RETURN(1);
  for (pos=table_names ; tables ; tables=tables->next)
    *pos++= tables->real_name;
  *pos=0;
  DBUG_RETURN(myrg_create(fn_format(buff,name,"","",2+4+16),
			  (const char **) table_names, (my_bool) 0));
}

void ha_myisammrg::append_create_info(String *packet)
{
  char buff[FN_REFLEN];
  packet->append(" UNION=(",8);
  MYRG_TABLE *table,*first;

  for (first=table=file->open_tables ; table != file->end_table ; table++)
  {
    char *name=table->table->s->filename;
    fn_format(buff,name,"","",3);
    if (table != first)
      packet->append(',');
    packet->append(buff,(uint) strlen(buff));
  }
  packet->append(')');
}
