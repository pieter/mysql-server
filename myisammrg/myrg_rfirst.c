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

#include "mymrgdef.h"

	/*  Read first row through  a specfic key */

int myrg_rfirst(MYRG_INFO *info, byte *buf, int inx)
{
  MYRG_TABLE *table;
  MI_INFO *mi;
  int err;

  if (_myrg_init_queue(info,inx,HA_READ_KEY_OR_NEXT))
    return my_errno;

  for (table=info->open_tables ; table < info->end_table ; table++)
  {
    err=mi_rfirst(table->table,NULL,inx);
    info->last_used_table=table;

    if (err == HA_ERR_END_OF_FILE)
      continue;
    if (err)
      return err;

    /* adding to queue */
    queue_insert(&(info->by_key),(byte *)table);
  }

  if (!info->by_key.elements)
    return HA_ERR_END_OF_FILE;

  mi=(info->current_table=(MYRG_TABLE *)queue_top(&(info->by_key)))->table;
  return mi_rrnd(mi,buf,mi->lastpos);
}
