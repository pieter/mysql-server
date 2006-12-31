/* Copyright (C) 2000-2005 MySQL AB

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

/* Compability file ; This file only contains dummy functions */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface
#endif

#include <queues.h>

class Item_func_unique_users :public Item_real_func
{
public:
  Item_func_unique_users(Item *name_arg,int start,int end,List<Item> &list)
    :Item_real_func(list) {}
  double val_real() { DBUG_ASSERT(fixed == 1); return 0.0; }
  void fix_length_and_dec() { decimals=0; max_length=6; }
  void print(String *str) { str->append(STRING_WITH_LEN("0.0")); }
  const char *func_name() const { return "unique_users"; }
};


class Item_sum_unique_users :public Item_sum_num
{
public:
  Item_sum_unique_users(Item *name_arg,int start,int end,Item *item_arg)
    :Item_sum_num(item_arg) {}
  Item_sum_unique_users(THD *thd, Item_sum_unique_users *item)
    :Item_sum_num(thd, item) {}
  double val_real() { DBUG_ASSERT(fixed == 1); return 0.0; }
  enum Sumfunctype sum_func () const {return UNIQUE_USERS_FUNC;}
  void clear() {}
  bool add() { return 0; }
  void reset_field() {}
  void update_field() {}
  bool fix_fields(THD *thd, Item **ref)
  {
    DBUG_ASSERT(fixed == 0);
    fixed= 1;
    return FALSE;
  }
  Item *copy_or_same(THD* thd)
  {
    return new Item_sum_unique_users(thd, this);
  }
  void print(String *str) { str->append(STRING_WITH_LEN("0.0")); }
  Field *create_tmp_field(bool group, TABLE *table, uint convert_blob_length);
  const char *func_name() const { return "sum_unique_users"; }
};
