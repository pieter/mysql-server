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


/* Sum functions (COUNT, MIN...) */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"

Item_sum::Item_sum(List<Item> &list)
  :arg_count(list.elements)
{
  if ((args=(Item**) sql_alloc(sizeof(Item*)*arg_count)))
  {
    uint i=0;
    List_iterator_fast<Item> li(list);
    Item *item;

    while ((item=li++))
    {
      args[i++]= item;
    }
  }
  mark_as_sum_func();
  list.empty();					// Fields are used
}


/*
  Constructor used in processing select with temporary tebles
*/

Item_sum::Item_sum(THD *thd, Item_sum *item):
  Item_result_field(thd, item), arg_count(item->arg_count),
  quick_group(item->quick_group)
{
  if (arg_count <= 2)
    args=tmp_args;
  else
    if (!(args= (Item**) thd->alloc(sizeof(Item*)*arg_count)))
      return;
  memcpy(args, item->args, sizeof(Item*)*arg_count);
}


void Item_sum::mark_as_sum_func()
{
  current_thd->lex->current_select->with_sum_func= 1;
  with_sum_func= 1;
}


void Item_sum::make_field(Send_field *tmp_field)
{
  if (args[0]->type() == Item::FIELD_ITEM && keep_field_type())
  {
    ((Item_field*) args[0])->field->make_field(tmp_field);
    tmp_field->db_name=(char*)"";
    tmp_field->org_table_name=tmp_field->table_name=(char*)"";
    tmp_field->org_col_name=tmp_field->col_name=name;
    if (maybe_null)
      tmp_field->flags&= ~NOT_NULL_FLAG;
  }
  else
    init_make_field(tmp_field, field_type());
}


void Item_sum::print(String *str)
{
  str->append(func_name());
  str->append('(');
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str);
  }
  str->append(')');
}

void Item_sum::fix_num_length_and_dec()
{
  decimals=0;
  for (uint i=0 ; i < arg_count ; i++)
    set_if_bigger(decimals,args[i]->decimals);
  max_length=float_length(decimals);
}

Item *Item_sum::get_tmp_table_item(THD *thd)
{
  Item_sum* sum_item= (Item_sum *) copy_or_same(thd);
  if (sum_item && sum_item->result_field)	   // If not a const sum func
  {
    Field *result_field_tmp= sum_item->result_field;
    for (uint i=0 ; i < sum_item->arg_count ; i++)
    {
      Item *arg= sum_item->args[i];
      if (!arg->const_item())
      {
	if (arg->type() == Item::FIELD_ITEM)
	  ((Item_field*) arg)->field= result_field_tmp++;
	else
	  sum_item->args[i]= new Item_field(result_field_tmp++);
      }
    }
  }
  return sum_item;
}


my_decimal *Item_sum::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed);
  DBUG_ASSERT(decimal_value);
  int2my_decimal(E_DEC_FATAL_ERROR, val_int(), unsigned_flag, decimal_value);
  return decimal_value;
}


bool Item_sum::walk (Item_processor processor, byte *argument)
{
  if (arg_count)
  {
    Item **arg,**arg_end;
    for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
    {
      if ((*arg)->walk(processor, argument))
	return 1;
    }
  }
  return (this->*processor)(argument);
}


Field *Item_sum::create_tmp_field(bool group, TABLE *table,
                                  uint convert_blob_length)
{
  switch (result_type()) {
  case REAL_RESULT:
    return new Field_double(max_length,maybe_null,name,table,decimals);
  case INT_RESULT:
    return new Field_longlong(max_length,maybe_null,name,table,unsigned_flag);
  case STRING_RESULT:
    if (max_length > 255 && convert_blob_length)
      return new Field_varstring(convert_blob_length, maybe_null,
                                 name, table,
                                 collation.collation);
    return make_string_field(table);
  case DECIMAL_RESULT:
    return new Field_new_decimal(max_length - (decimals?1:0),
                                 maybe_null, name, table, decimals);
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    return 0;
  }
}


String *
Item_sum_num::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  double nr= val_real();
  if (null_value)
    return 0;
  str->set(nr,decimals, &my_charset_bin);
  return str;
}


my_decimal *Item_sum_num::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  double nr= val_real();
  if (null_value)
    return 0;
  double2my_decimal(E_DEC_FATAL_ERROR, nr, decimal_value);
  return (decimal_value);
}


String *
Item_sum_int::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  longlong nr= val_int();
  if (null_value)
    return 0;
  if (unsigned_flag)
    str->set((ulonglong) nr, &my_charset_bin);
  else
    str->set(nr, &my_charset_bin);
  return str;
}


bool
Item_sum_num::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  DBUG_ASSERT(fixed == 0);

  if (!thd->allow_sum_func)
  {
    my_message(ER_INVALID_GROUP_FUNC_USE, ER(ER_INVALID_GROUP_FUNC_USE),
               MYF(0));
    return TRUE;
  }
  thd->allow_sum_func=0;			// No included group funcs
  decimals=0;
  maybe_null=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (args[i]->fix_fields(thd, tables, args + i) || args[i]->check_cols(1))
      return TRUE;
    set_if_bigger(decimals, args[i]->decimals);
    maybe_null |= args[i]->maybe_null;
  }
  result_field=0;
  max_length=float_length(decimals);
  null_value=1;
  fix_length_and_dec();
  thd->allow_sum_func=1;			// Allow group functions
  fixed= 1;
  return FALSE;
}


Item_sum_hybrid::Item_sum_hybrid(THD *thd, Item_sum_hybrid *item)
  :Item_sum(thd, item), value(item->value), hybrid_type(item->hybrid_type),
  hybrid_field_type(item->hybrid_field_type), cmp_sign(item->cmp_sign),
  used_table_cache(item->used_table_cache), was_values(item->was_values)
{
  switch (hybrid_type)
  {
  case INT_RESULT:
    sum_int= item->sum_int;
    break;
  case DECIMAL_RESULT:
    my_decimal2decimal(&item->sum_dec, &sum_dec);
    break;
  case REAL_RESULT:
    sum= item->sum;
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
  collation.set(item->collation);
}

bool
Item_sum_hybrid::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  DBUG_ASSERT(fixed == 0);

  Item *item= args[0];
  if (!thd->allow_sum_func)
  {
    my_message(ER_INVALID_GROUP_FUNC_USE, ER(ER_INVALID_GROUP_FUNC_USE),
               MYF(0));
    return TRUE;
  }
  thd->allow_sum_func=0;			// No included group funcs

  // 'item' can be changed during fix_fields
  if (!item->fixed &&
      item->fix_fields(thd, tables, args) ||
      (item= args[0])->check_cols(1))
    return TRUE;
  decimals=item->decimals;

  switch (hybrid_type= item->result_type())
  {
  case INT_RESULT:
    max_length= 20;
    sum_int= 0;
    break;
  case DECIMAL_RESULT:
    max_length= item->max_length;
    my_decimal_set_zero(&sum_dec);
    break;
  case REAL_RESULT:
    max_length= float_length(decimals);
    sum= 0.0;
    break;
  case STRING_RESULT:
    max_length= item->max_length;
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  };
  /* MIN/MAX can return NULL for empty set indepedent of the used column */
  maybe_null= 1;
  unsigned_flag=item->unsigned_flag;
  collation.set(item->collation);
  result_field=0;
  null_value=1;
  fix_length_and_dec();
  thd->allow_sum_func=1;			// Allow group functions
  if (item->type() == Item::FIELD_ITEM)
    hybrid_field_type= ((Item_field*) item)->field->type();
  else
    hybrid_field_type= Item::field_type();
  fixed= 1;
  return FALSE;
}


/***********************************************************************
** reset and add of sum_func
***********************************************************************/

Item_sum_sum::Item_sum_sum(THD *thd, Item_sum_sum *item) 
  :Item_sum_num(thd, item), hybrid_type(item->hybrid_type),
   curr_dec_buff(item->curr_dec_buff)
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal2decimal(item->dec_buffs, dec_buffs);
    my_decimal2decimal(item->dec_buffs + 1, dec_buffs + 1);
  }
  else
    sum= item->sum;
}

Item *Item_sum_sum::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_sum(thd, this);
}


void Item_sum_sum::clear()
{
  DBUG_ENTER("Item_sum_sum::clear");
  null_value=1;
  if (hybrid_type == DECIMAL_RESULT)
  {
    curr_dec_buff= 0;
    my_decimal_set_zero(dec_buffs);
  }
  else
    sum= 0.0;
  DBUG_VOID_RETURN;
}


void Item_sum_sum::fix_length_and_dec()
{
  DBUG_ENTER("Item_sum_sum::fix_length_and_dec");
  maybe_null=null_value=1;
  decimals= args[0]->decimals;
  switch (args[0]->result_type())
  {
  case REAL_RESULT:
  case STRING_RESULT:
    hybrid_type= REAL_RESULT;
    sum= 0.0;
    break;
  case INT_RESULT:
  case DECIMAL_RESULT:
    /* SUM result can't be longer than length(arg) + length(MAX_ROWS) */
    max_length= min(args[0]->max_length + DECIMAL_LONGLONG_DIGITS,
                    DECIMAL_MAX_LENGTH);
    curr_dec_buff= 0;
    hybrid_type= DECIMAL_RESULT;
    my_decimal_set_zero(dec_buffs);
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
  DBUG_PRINT("info", ("Type: %s (%d, %d)",
                      (hybrid_type == REAL_RESULT ? "REAL_RESULT" :
                       hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT" :
                       hybrid_type == INT_RESULT ? "INT_RESULT" :
                       "--ILLEGAL!!!--"),
                      max_length,
                      (int)decimals));
  DBUG_VOID_RETURN;
}


bool Item_sum_sum::add()
{
  DBUG_ENTER("Item_sum_sum::add");
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs + (curr_dec_buff^1),
                     val, dec_buffs + curr_dec_buff);
      curr_dec_buff^= 1;
      null_value= 0;
    }
  }
  else
  {
    sum+= args[0]->val_real();
    if (!args[0]->null_value)
      null_value= 0;
  }
  DBUG_RETURN(0);
}


longlong Item_sum_sum::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == DECIMAL_RESULT)
  {
    longlong result;
    my_decimal2int(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff, unsigned_flag,
                   &result);
    return result;
  }
  return Item_sum_num::val_int();
}


double Item_sum_sum::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == DECIMAL_RESULT)
    my_decimal2double(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff, &sum);
  return sum;
}


String *Item_sum_sum::val_str(String*str)
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    if (null_value)
      return NULL;
    my_decimal_round(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff, decimals,
                     FALSE, dec_buffs + curr_dec_buff);
    my_decimal2string(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff,
                      0, 0, 0, str);
    return str;
  }
  return Item_sum_num::val_str(str);
}


my_decimal *Item_sum_sum::val_decimal(my_decimal *val)
{
  DBUG_ASSERT(hybrid_type == DECIMAL_RESULT);
  return(dec_buffs + curr_dec_buff);
}

/* Item_sum_sum_distinct */

Item_sum_sum_distinct::Item_sum_sum_distinct(Item *item)
  :Item_sum_sum(item), tree(0)
{
  /*
    quick_group is an optimizer hint, which means that GROUP BY can be
    handled with help of index on grouped columns.
    By setting quick_group to zero we force creation of temporary table
    to perform GROUP BY.
  */
  quick_group= 0;
}


Item_sum_sum_distinct::Item_sum_sum_distinct(THD *thd,
                                             Item_sum_sum_distinct *original)
  :Item_sum_sum(thd, original), tree(0), dec_bin_buff(original->dec_bin_buff)
{
  quick_group= 0;
}


void Item_sum_sum_distinct::fix_length_and_dec()
{
  Item_sum_sum::fix_length_and_dec();
  if (hybrid_type == DECIMAL_RESULT)
  {
    dec_bin_buff= (byte *)
      sql_alloc(my_decimal_get_binary_size(args[0]->max_length,
                                           args[0]->decimals));
  }
}


Item *
Item_sum_sum_distinct::copy_or_same(THD *thd)
{
  return new (thd->mem_root) Item_sum_sum_distinct(thd, this);
}

C_MODE_START

static int simple_raw_key_cmp(void* arg, const void* key1, const void* key2)
{
  return memcmp(key1, key2, *(uint *) arg);
}

C_MODE_END

bool Item_sum_sum_distinct::setup(THD *thd)
{
  DBUG_ENTER("Item_sum_sum_distinct::setup");
  SELECT_LEX *select_lex= thd->lex->current_select;
  /* what does it mean??? */
  if (select_lex->linkage == GLOBAL_OPTIONS_TYPE)
    DBUG_RETURN(1);

  DBUG_ASSERT(tree == 0);                 /* setup can not be called twice */

  /*
    Uniques handles all unique elements in a tree until they can't fit in.
    Then thee tree is dumped to the temporary file.
    See class Unique for details.
  */
  null_value= maybe_null= 1;
  /*
    TODO: if underlying item result fits in 4 bytes we can take advantage
    of it and have tree of long/ulong. It gives 10% performance boost
  */
  uint *key_length_ptr= (uint *)thd->alloc(sizeof(uint));
  *key_length_ptr= ((hybrid_type == DECIMAL_RESULT) ?
                    my_decimal_get_binary_size(args[0]->max_length,
                                               args[0]->decimals) :
                    sizeof(double));
  tree= new Unique(simple_raw_key_cmp, key_length_ptr, *key_length_ptr,
                   thd->variables.max_heap_table_size);
  DBUG_PRINT("info", ("tree 0x%lx, key length %d", (ulong)tree,
                      *key_length_ptr));
  DBUG_RETURN(tree == 0);
}

void Item_sum_sum_distinct::clear()
{
  DBUG_ENTER("Item_sum_sum_distinct::clear");
  DBUG_ASSERT(tree);                            /* we always have a tree */
  null_value= 1; 
  tree->reset();
  DBUG_VOID_RETURN;
}

void Item_sum_sum_distinct::cleanup()
{
  Item_sum_num::cleanup();
  delete tree;
  tree= 0;
}


bool Item_sum_sum_distinct::add()
{
  DBUG_ENTER("Item_sum_sum_distinct::add");
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      DBUG_ASSERT(tree);
      null_value= 0;
      my_decimal2binary(E_DEC_FATAL_ERROR, val, dec_bin_buff,
                        args[0]->max_length, args[0]->decimals);
      DBUG_RETURN(tree->unique_add(dec_bin_buff));
    }
  }
  else
  {
    /* args[0]->val() may reset args[0]->null_value */
    double val= args[0]->val_real();
    if (!args[0]->null_value)
    {
      DBUG_ASSERT(tree);
      null_value= 0;
      DBUG_PRINT("info", ("real: %lg, tree 0x%lx", val, (ulong)tree));
      if (val)
        DBUG_RETURN(tree->unique_add(&val));
    }
    else
      DBUG_PRINT("info", ("real: NULL"));
  }
  DBUG_RETURN(0);
}


void Item_sum_sum_distinct::add_real(double val)
{
  DBUG_ENTER("Item_sum_sum_distinct::add_real");
  sum+= val;
  DBUG_PRINT("info", ("sum %lg, val %lg", sum, val));
  DBUG_VOID_RETURN;
}


void Item_sum_sum_distinct::add_decimal(byte *val)
{
  binary2my_decimal(E_DEC_FATAL_ERROR, val, &tmp_dec,
                    args[0]->max_length, args[0]->decimals);
  my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs + (curr_dec_buff^1),
                 &tmp_dec, dec_buffs + curr_dec_buff);
  curr_dec_buff^= 1;
}

C_MODE_START

static int sum_sum_distinct_real(void *element, element_count num_of_dups,
                                 void *item_sum_sum_distinct)
{ 
  ((Item_sum_sum_distinct *)
   (item_sum_sum_distinct))->add_real(* (double *) element);
  return 0;
}

static int sum_sum_distinct_decimal(void *element, element_count num_of_dups,
                                    void *item_sum_sum_distinct)
{ 
  ((Item_sum_sum_distinct *)
   (item_sum_sum_distinct))->add_decimal((byte *)element);
  return 0;
}

C_MODE_END

double Item_sum_sum_distinct::val_real()
{
  DBUG_ENTER("Item_sum_sum_distinct::val");
  /*
    We don't have a tree only if 'setup()' hasn't been called;
    this is the case of sql_select.cc:return_zero_rows.
  */
  if (hybrid_type == DECIMAL_RESULT)
  {
    /* Item_sum_sum_distinct::val_decimal do not use argument */
    my_decimal *val= val_decimal(0);
    if (!null_value)
      my_decimal2double(E_DEC_FATAL_ERROR, val, &sum);
  }
  else
  {
    sum= 0.0;
    DBUG_PRINT("info", ("tree 0x%lx", (ulong)tree));
    if (tree) 
      tree->walk(sum_sum_distinct_real, (void *) this);
  }
  DBUG_RETURN(sum);
}


my_decimal *Item_sum_sum_distinct::val_decimal(my_decimal *fake)
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal_set_zero(dec_buffs);
    curr_dec_buff= 0;
    if (tree)
      tree->walk(sum_sum_distinct_decimal, (void *)this);
  }
  else
  {
    double real= val_real();
    double2my_decimal(E_DEC_FATAL_ERROR, real, dec_buffs + curr_dec_buff);
  }
  return(dec_buffs + curr_dec_buff);
}


longlong Item_sum_sum_distinct::val_int()
{
  longlong i;
  if (hybrid_type == DECIMAL_RESULT)
  {
    /* Item_sum_sum_distinct::val_decimal do not use argument */
    my_decimal *val= val_decimal(0);
    if (!null_value)
      my_decimal2int(E_DEC_FATAL_ERROR, val, unsigned_flag, &i);
  }
  else
    i= (longlong) val_real();
  return i;
}


String *Item_sum_sum_distinct::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == DECIMAL_RESULT)
  {
    /* Item_sum_sum_distinct::val_decimal do not use argument */
    my_decimal *val= val_decimal(0);
    if (null_value)
      return 0;
    my_decimal_round(E_DEC_FATAL_ERROR, val, decimals, FALSE, val);
    my_decimal2string(E_DEC_FATAL_ERROR, val, 0, 0, 0, str);
  }
  else
  {
    double nr= val_real();
    if (null_value)
      return 0;
    str->set(nr, decimals, &my_charset_bin);
  }
  return str;
}


/* end of Item_sum_sum_distinct */

Item *Item_sum_count::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_count(thd, this);
}


void Item_sum_count::clear()
{
  count= 0;
}


bool Item_sum_count::add()
{
  if (!args[0]->maybe_null)
    count++;
  else
  {
    (void) args[0]->val_int();
    if (!args[0]->null_value)
      count++;
  }
  return 0;
}

longlong Item_sum_count::val_int()
{
  DBUG_ASSERT(fixed == 1);
  return (longlong) count;
}


void Item_sum_count::cleanup()
{
  DBUG_ENTER("Item_sum_count::cleanup");
  Item_sum_int::cleanup();
  used_table_cache= ~(table_map) 0;
  DBUG_VOID_RETURN;
}


/*
  Avgerage
*/
void Item_sum_avg::fix_length_and_dec()
{
  Item_sum_sum::fix_length_and_dec();
  maybe_null=null_value=1;
  decimals= min(args[0]->decimals + 4, NOT_FIXED_DEC);
  if (hybrid_type == DECIMAL_RESULT)
  {
    f_scale= args[0]->decimals;
    max_length= DECIMAL_MAX_LENGTH + (f_scale ? 1 : 0);
    f_precision= DECIMAL_MAX_LENGTH;
    dec_bin_size= my_decimal_get_binary_size(f_precision, f_scale);
  }
}


Item *Item_sum_avg::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_avg(thd, this);
}


Field *Item_sum_avg::create_tmp_field(bool group, TABLE *table,
                                      uint convert_blob_len)
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    if (group)
      return new Field_string(dec_bin_size + sizeof(longlong),
                              0, name, table, &my_charset_bin);
    else
      return new Field_new_decimal(f_precision,
                                   maybe_null, name, table, f_scale);
  }
  else
  {
    if (group)
      return new Field_string(sizeof(double)+sizeof(longlong),
                              0, name,table,&my_charset_bin);
    else
      return new Field_double(max_length, maybe_null, name, table, decimals);
  }
}


void Item_sum_avg::clear()
{
  Item_sum_sum::clear();
  count=0;
}


bool Item_sum_avg::add()
{
  if (Item_sum_sum::add())
    return TRUE;
  if (!args[0]->null_value)
    count++;
  return FALSE;
}

double Item_sum_avg::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  return Item_sum_sum::val_real() / ulonglong2double(count);
}


my_decimal *Item_sum_avg::val_decimal(my_decimal *val)
{
  DBUG_ASSERT(fixed == 1);
  if (!count)
  {
    null_value=1;
    return NULL;
  }
  my_decimal sum, cnt;
  const my_decimal *sum_dec= Item_sum_sum::val_decimal(&sum);
  int2my_decimal(E_DEC_FATAL_ERROR, count, 0, &cnt);
  my_decimal_div(E_DEC_FATAL_ERROR, val, sum_dec, &cnt, 4);
  return val;
}


String *Item_sum_avg::val_str(String *str)
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *dec_val= val_decimal(&value);
    if (null_value)
      return NULL;
    my_decimal_round(E_DEC_FATAL_ERROR, dec_val, decimals, FALSE, &value);
    my_decimal2string(E_DEC_FATAL_ERROR, &value, 0, 0, 0, str);
    return str;
  }
  double nr= val_real();
  if (null_value)
    return NULL;
  str->set(nr, decimals, &my_charset_bin);
  return str;
}



/*
  Standard deviation
*/

double Item_sum_std::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double tmp= Item_sum_variance::val_real();
  return tmp <= 0.0 ? 0.0 : sqrt(tmp);
}

Item *Item_sum_std::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_std(thd, this);
}


/*
  Variance
*/


Item_sum_variance::Item_sum_variance(THD *thd, Item_sum_variance *item):
  Item_sum_num(thd, item), hybrid_type(item->hybrid_type),
    cur_dec(item->cur_dec), count(item->count)
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    memcpy(dec_sum, item->dec_sum, sizeof(item->dec_sum));
    memcpy(dec_sqr, item->dec_sqr, sizeof(item->dec_sqr));
    for (int i=0; i<2; i++)
    {
      dec_sum[i].fix_buffer_pointer();
      dec_sqr[i].fix_buffer_pointer();
    }
  }
  else
  {
    sum= item->sum;
    sum_sqr= item->sum_sqr;
  }
}


void Item_sum_variance::fix_length_and_dec()
{
  DBUG_ENTER("Item_sum_sum::fix_length_and_dec");
  maybe_null=null_value=1;
  decimals= args[0]->decimals + 4;
  switch (args[0]->result_type())
  {
  case REAL_RESULT:
  case STRING_RESULT:
    hybrid_type= REAL_RESULT;
    sum= 0.0;
    break;
  case INT_RESULT:
  case DECIMAL_RESULT:
    /* SUM result can't be longer than length(arg)*2 + digits_after_the_point_to_add*/
    max_length= args[0]->max_length*2 + 4;
    cur_dec= 0;
    hybrid_type= DECIMAL_RESULT;
    my_decimal_set_zero(dec_sum);
    my_decimal_set_zero(dec_sqr);
    f_scale0= args[0]->decimals;
    f_precision0= DECIMAL_MAX_LENGTH / 2;
    f_scale1= min(f_scale0 * 2, NOT_FIXED_DEC - 1);
    f_precision1= DECIMAL_MAX_LENGTH;
    dec_bin_size0= my_decimal_get_binary_size(f_precision0, f_scale0);
    dec_bin_size1= my_decimal_get_binary_size(f_precision1, f_scale1);
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
  DBUG_PRINT("info", ("Type: %s (%d, %d)",
                      (hybrid_type == REAL_RESULT ? "REAL_RESULT" :
                       hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT" :
                       hybrid_type == INT_RESULT ? "INT_RESULT" :
                       "--ILLEGAL!!!--"),
                      max_length,
                      (int)decimals));
  DBUG_VOID_RETURN;
}


Item *Item_sum_variance::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_variance(thd, this);
}


Field *Item_sum_variance::create_tmp_field(bool group, TABLE *table,
                                           uint convert_blob_len)
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    if (group)
      return new Field_string(dec_bin_size0+dec_bin_size1+sizeof(longlong),
                              0, name,table,&my_charset_bin);
    else
      return new Field_new_decimal(DECIMAL_MAX_LENGTH,
                                   maybe_null, name, table, f_scale1 + 4);
  }
  else
  {
    if (group)
      return new Field_string(sizeof(double)*2+sizeof(longlong),
                              0, name,table,&my_charset_bin);
    else
      return new Field_double(max_length, maybe_null,name,table,decimals);
  }
}


void Item_sum_variance::clear()
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal_set_zero(dec_sum);
    my_decimal_set_zero(dec_sqr);
    cur_dec= 0;
  }
  else
    sum=sum_sqr=0.0; 
  count=0; 
}

bool Item_sum_variance::add()
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal dec_buf, *dec= args[0]->val_decimal(&dec_buf);
    my_decimal sqr_buf;
    if (!args[0]->null_value)
    {
      count++;
      int next_dec= cur_dec ^ 1;
      my_decimal_mul(E_DEC_FATAL_ERROR, &sqr_buf, dec, dec);
      my_decimal_add(E_DEC_FATAL_ERROR, dec_sqr+next_dec,
                     dec_sqr+cur_dec, &sqr_buf);
      my_decimal_add(E_DEC_FATAL_ERROR, dec_sum+next_dec,
                     dec_sum+cur_dec, dec);
      cur_dec= next_dec;
    }
  }
  else
  {
    double nr= args[0]->val_real();
    if (!args[0]->null_value)
    {
      sum+=nr;
      sum_sqr+=nr*nr;
      count++;
    }
  }
  return 0;
}

double Item_sum_variance::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  null_value=0;
  if (hybrid_type == DECIMAL_RESULT)
  {
    double result;
    my_decimal dec_buf, *dec= Item_sum_variance::val_decimal(&dec_buf);
    my_decimal2double(E_DEC_FATAL_ERROR, dec, &result);
    return result;
  }
  /* Avoid problems when the precision isn't good enough */
  double tmp=ulonglong2double(count);
  double tmp2=(sum_sqr - sum*sum/tmp)/tmp;
  return tmp2 <= 0.0 ? 0.0 : tmp2;
}


my_decimal *Item_sum_variance::val_decimal(my_decimal *dec_buf)
{
  DBUG_ASSERT(fixed ==1 );
  if (hybrid_type == REAL_RESULT)
  {
    double result= Item_sum_variance::val_real();
    if (null_value)
      return 0;
    double2my_decimal(E_DEC_FATAL_ERROR, result, dec_buf);
    return dec_buf;
  }
  if (!count)
  {
    null_value= 1;
    return 0;
  }
  null_value= 0;
  my_decimal count_buf, sum_sqr_buf;
  int2my_decimal(E_DEC_FATAL_ERROR, count, 0, &count_buf);
  my_decimal_mul(E_DEC_FATAL_ERROR, &sum_sqr_buf,
                 dec_sum+cur_dec, dec_sum+cur_dec);
  my_decimal_div(E_DEC_FATAL_ERROR, dec_buf, &sum_sqr_buf, &count_buf, 2);
  my_decimal_sub(E_DEC_FATAL_ERROR, &sum_sqr_buf, dec_sqr+cur_dec, dec_buf);
  my_decimal_div(E_DEC_FATAL_ERROR, dec_buf, &sum_sqr_buf, &count_buf, 2);
  return dec_buf;
}

void Item_sum_variance::reset_field()
{
  char *res=result_field->ptr;
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_dec= args[0]->val_decimal(&value);
    if (args[0]->null_value)
    {
      my_decimal2binary(E_DEC_FATAL_ERROR, &decimal_zero,
                        res, f_precision0, f_scale0);
      my_decimal2binary(E_DEC_FATAL_ERROR, &decimal_zero,
                        res+dec_bin_size0, f_precision1, f_scale1);
      res+= dec_bin_size0 + dec_bin_size1;
      longlong tmp=0;
      int8store(res,tmp);
    }
    else
    {
      my_decimal2binary(E_DEC_FATAL_ERROR, arg_dec,
                        res, f_precision0, f_scale0);
      my_decimal_mul(E_DEC_FATAL_ERROR, dec_sum, arg_dec, arg_dec);
      my_decimal2binary(E_DEC_FATAL_ERROR, dec_sum,
                        res+dec_bin_size0, f_precision1, f_scale1);
      res+= dec_bin_size0 + dec_bin_size1;
      longlong tmp=1;
      int8store(res,tmp);
    }
    return;
  }
  double nr= args[0]->val_real();

  if (args[0]->null_value)
    bzero(res,sizeof(double)*2+sizeof(longlong));
  else
  {
    float8store(res,nr);
    nr*=nr;
    float8store(res+sizeof(double),nr);
    longlong tmp=1;
    int8store(res+sizeof(double)*2,tmp);
  }
}

void Item_sum_variance::update_field()
{
  longlong field_count;
  char *res=result_field->ptr;
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      binary2my_decimal(E_DEC_FATAL_ERROR, res,
                        dec_sum+1, f_precision0, f_scale0);
      binary2my_decimal(E_DEC_FATAL_ERROR, res+dec_bin_size0,
                        dec_sqr+1, f_precision1, f_scale1);
      field_count= sint8korr(res + (dec_bin_size0 + dec_bin_size1));
      my_decimal_add(E_DEC_FATAL_ERROR, dec_sum, arg_val, dec_sum+1);
      my_decimal_mul(E_DEC_FATAL_ERROR, dec_sum+1, arg_val, arg_val);
      my_decimal_add(E_DEC_FATAL_ERROR, dec_sqr, dec_sqr+1, dec_sum+1);
      field_count++;
      my_decimal2binary(E_DEC_FATAL_ERROR, dec_sum,
                        res, f_precision0, f_scale0);
      my_decimal2binary(E_DEC_FATAL_ERROR, dec_sqr,
                        res+dec_bin_size0, f_precision1, f_scale1);
      res+= dec_bin_size0 + dec_bin_size1;
      int8store(res, field_count);
    }
    return;
  }

  double nr,old_nr,old_sqr;
  float8get(old_nr, res);
  float8get(old_sqr, res+sizeof(double));
  field_count=sint8korr(res+sizeof(double)*2);

  nr= args[0]->val_real();
  if (!args[0]->null_value)
  {
    old_nr+=nr;
    old_sqr+=nr*nr;
    field_count++;
  }
  float8store(res,old_nr);
  float8store(res+sizeof(double),old_sqr);
  int8store(res+sizeof(double)*2,field_count);
}

/* min & max */

void Item_sum_hybrid::clear()
{
  switch (hybrid_type)
  {
  case INT_RESULT:
    sum_int= 0;
    break;
  case DECIMAL_RESULT:
    my_decimal_set_zero(&sum_dec);
    break;
  case REAL_RESULT:
    sum= 0.0;
    break;
  default:
    value.length(0);
  }
  null_value= 1;
}

double Item_sum_hybrid::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0.0;
  switch (hybrid_type) {
  case STRING_RESULT:
  {
    char *end_not_used;
    int err_not_used;
    String *res;  res=val_str(&str_value);
    return (res ? my_strntod(res->charset(), (char*) res->ptr(), res->length(),
			     &end_not_used, &err_not_used) : 0.0);
  }
  case INT_RESULT:
    if (unsigned_flag)
      return ulonglong2double(sum_int);
    return (double) sum_int;
  case DECIMAL_RESULT:
    my_decimal2double(E_DEC_FATAL_ERROR, &sum_dec, &sum);
    return sum;
  case REAL_RESULT:
    return sum;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    return 0;
  }
  return 0;					// Keep compiler happy
}

longlong Item_sum_hybrid::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  switch (hybrid_type)
  {
  case INT_RESULT:
    return sum_int;
  case DECIMAL_RESULT:
  {
    longlong result;
    my_decimal2int(E_DEC_FATAL_ERROR, &sum_dec, unsigned_flag, &result);
    return sum_int;
  }
  default:
    return (longlong) Item_sum_hybrid::val_real();
  }
}


my_decimal *Item_sum_hybrid::val_decimal(my_decimal *val)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  switch (hybrid_type) {
  case STRING_RESULT:
    string2my_decimal(E_DEC_FATAL_ERROR, &value, val);
    break;
  case REAL_RESULT:
    double2my_decimal(E_DEC_FATAL_ERROR, sum, val);
    break;
  case DECIMAL_RESULT:
    val= &sum_dec;
    break;
  case INT_RESULT:
    int2my_decimal(E_DEC_FATAL_ERROR, sum_int, unsigned_flag, val);
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return val;					// Keep compiler happy
}


String *
Item_sum_hybrid::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  switch (hybrid_type) {
  case STRING_RESULT:
    return &value;
  case REAL_RESULT:
    str->set(sum,decimals, &my_charset_bin);
    break;
  case DECIMAL_RESULT:
    my_decimal2string(E_DEC_FATAL_ERROR, &sum_dec, 0, 0, 0, str);
    return str;
  case INT_RESULT:
    if (unsigned_flag)
      str->set((ulonglong) sum_int, &my_charset_bin);
    else
      str->set((longlong) sum_int, &my_charset_bin);
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return str;					// Keep compiler happy
}


void Item_sum_hybrid::cleanup()
{
  DBUG_ENTER("Item_sum_hybrid::cleanup");
  Item_sum::cleanup();
  used_table_cache= ~(table_map) 0;

  /*
    by default it is TRUE to avoid TRUE reporting by
    Item_func_not_all/Item_func_nop_all if this item was never called.

    no_rows_in_result() set it to FALSE if was not results found.
    If some results found it will be left unchanged.
  */
  was_values= TRUE;
  DBUG_VOID_RETURN;
}

void Item_sum_hybrid::no_rows_in_result()
{
  Item_sum::no_rows_in_result();
  was_values= FALSE;
}


Item *Item_sum_min::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_min(thd, this);
}


bool Item_sum_min::add()
{
  switch (hybrid_type) {
  case STRING_RESULT:
  {
    String *result=args[0]->val_str(&tmp_value);
    if (!args[0]->null_value &&
	(null_value || sortcmp(&value,result,collation.collation) > 0))
    {
      value.copy(*result);
      null_value=0;
    }
  }
  break;
  case INT_RESULT:
  {
    longlong nr=args[0]->val_int();
    if (!args[0]->null_value && (null_value ||
				 (unsigned_flag && 
				  (ulonglong) nr < (ulonglong) sum_int) ||
				 (!unsigned_flag && nr < sum_int)))
    {
      sum_int=nr;
      null_value=0;
    }
  }
  break;
  case DECIMAL_RESULT:
  {
    my_decimal value, *val= args[0]->val_decimal(&value);
    if (!args[0]->null_value &&
        (null_value || (my_decimal_cmp(&sum_dec, val) > 0)))
    {
      my_decimal2decimal(val, &sum_dec);
      null_value= 0;
    }
  }
  break;
  case REAL_RESULT:
  {
    double nr= args[0]->val_real();
    if (!args[0]->null_value && (null_value || nr < sum))
    {
      sum=nr;
      null_value=0;
    }
  }
  break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return 0;
}


Item *Item_sum_max::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_max(thd, this);
}


bool Item_sum_max::add()
{
  switch (hybrid_type) {
  case STRING_RESULT:
  {
    String *result=args[0]->val_str(&tmp_value);
    if (!args[0]->null_value &&
	(null_value || sortcmp(&value,result,collation.collation) < 0))
    {
      value.copy(*result);
      null_value=0;
    }
  }
  break;
  case INT_RESULT:
  {
    longlong nr=args[0]->val_int();
    if (!args[0]->null_value && (null_value ||
				 (unsigned_flag && 
				  (ulonglong) nr > (ulonglong) sum_int) ||
				 (!unsigned_flag && nr > sum_int)))
    {
      sum_int=nr;
      null_value=0;
    }
  }
  break;
  case DECIMAL_RESULT:
  {
    my_decimal value, *val= args[0]->val_decimal(&value);
    if (!args[0]->null_value &&
        (null_value || (my_decimal_cmp(val, &sum_dec) > 0)))
    {
      my_decimal2decimal(val, &sum_dec);
      null_value= 0;
    }
  }
  break;
  case REAL_RESULT:
  {
    double nr= args[0]->val_real();
    if (!args[0]->null_value && (null_value || nr > sum))
    {
      sum=nr;
      null_value=0;
    }
  }
  break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return 0;
}


/* bit_or and bit_and */

longlong Item_sum_bit::val_int()
{
  DBUG_ASSERT(fixed == 1);
  return (longlong) bits;
}


void Item_sum_bit::clear()
{
  bits= reset_bits;
}

Item *Item_sum_or::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_or(thd, this);
}


bool Item_sum_or::add()
{
  ulonglong value= (ulonglong) args[0]->val_int();
  if (!args[0]->null_value)
    bits|=value;
  return 0;
}

Item *Item_sum_xor::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_xor(thd, this);
}


bool Item_sum_xor::add()
{
  ulonglong value= (ulonglong) args[0]->val_int();
  if (!args[0]->null_value)
    bits^=value;
  return 0;
}

Item *Item_sum_and::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_and(thd, this);
}


bool Item_sum_and::add()
{
  ulonglong value= (ulonglong) args[0]->val_int();
  if (!args[0]->null_value)
    bits&=value;
  return 0;
}

/************************************************************************
** reset result of a Item_sum with is saved in a tmp_table
*************************************************************************/

void Item_sum_num::reset_field()
{
  double nr= args[0]->val_real();
  char *res=result_field->ptr;

  if (maybe_null)
  {
    if (args[0]->null_value)
    {
      nr=0.0;
      result_field->set_null();
    }
    else
      result_field->set_notnull();
  }
  float8store(res,nr);
}


void Item_sum_hybrid::reset_field()
{
  switch(hybrid_type)
  {
  case STRING_RESULT:
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff),result_field->charset()),*res;

    res=args[0]->val_str(&tmp);
    if (args[0]->null_value)
    {
      result_field->set_null();
      result_field->reset();
    }
    else
    {
      result_field->set_notnull();
      result_field->store(res->ptr(),res->length(),tmp.charset());
    }
    break;
  }
  case INT_RESULT:
  {
    longlong nr=args[0]->val_int();

    if (maybe_null)
    {
      if (args[0]->null_value)
      {
	nr=0;
	result_field->set_null();
      }
      else
	result_field->set_notnull();
    }
    result_field->store(nr);
    break;
  }
  case REAL_RESULT:
  {
    double nr= args[0]->val_real();

    if (maybe_null)
    {
      if (args[0]->null_value)
      {
	nr=0.0;
	result_field->set_null();
      }
      else
	result_field->set_notnull();
    }
    result_field->store(nr);
    break;
  }
  case DECIMAL_RESULT:
  {
    my_decimal value, *arg_dec= args[0]->val_decimal(&value);

    if (maybe_null)
    {
      if (args[0]->null_value)
        result_field->set_null();
      else
        result_field->set_notnull();
    }
    if (!args[0]->null_value)
      result_field->store_decimal(arg_dec);
    break;
  }
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
}


void Item_sum_sum::reset_field()
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_val= args[0]->val_decimal(&value);
    if (args[0]->null_value)
      result_field->reset();
    else
      result_field->store_decimal(arg_val);
  }
  else
  {
    DBUG_ASSERT(hybrid_type == REAL_RESULT);
    double nr= args[0]->val_real();			// Nulls also return 0
    float8store(result_field->ptr, nr);
  }
  if (args[0]->null_value)
    result_field->set_null();
  else
    result_field->set_notnull();
}


void Item_sum_count::reset_field()
{
  char *res=result_field->ptr;
  longlong nr=0;

  if (!args[0]->maybe_null)
    nr=1;
  else
  {
    (void) args[0]->val_int();
    if (!args[0]->null_value)
      nr=1;
  }
  int8store(res,nr);
}


void Item_sum_avg::reset_field()
{
  char *res=result_field->ptr;
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_dec= args[0]->val_decimal(&value);
    if (args[0]->null_value)
    {
      my_decimal2binary(E_DEC_FATAL_ERROR, &decimal_zero,
                        res, f_precision, f_scale);
      res+= dec_bin_size;
      longlong tmp=0;
      int8store(res,tmp);
    }
    else
    {
      my_decimal2binary(E_DEC_FATAL_ERROR, arg_dec,
                        res, f_precision, f_scale);
      res+= dec_bin_size;
      longlong tmp=1;
      int8store(res,tmp);
    }
  }
  else
  {
    double nr= args[0]->val_real();

    if (args[0]->null_value)
      bzero(res,sizeof(double)+sizeof(longlong));
    else
    {
      float8store(res,nr);
      res+=sizeof(double);
      longlong tmp=1;
      int8store(res,tmp);
    }
  }
}

void Item_sum_bit::reset_field()
{
  reset();
  int8store(result_field->ptr, bits);
}

void Item_sum_bit::update_field()
{
  char *res=result_field->ptr;
  bits= uint8korr(res);
  add();
  int8store(res, bits);
}

/*
** calc next value and merge it with field_value
*/

void Item_sum_sum::update_field()
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      if (!result_field->is_null())
      {
        my_decimal field_value,
                   *field_val= result_field->val_decimal(&field_value);
        my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs, arg_val, field_val);
        result_field->store_decimal(dec_buffs);
      }
      else
      {
        result_field->store_decimal(arg_val);
        result_field->set_notnull();
      }
    }
  }
  else
  {
    double old_nr,nr;
    char *res=result_field->ptr;

    float8get(old_nr,res);
    nr= args[0]->val_real();
    if (!args[0]->null_value)
    {
      old_nr+=nr;
      result_field->set_notnull();
    }
    float8store(res,old_nr);
  }
}


void Item_sum_count::update_field()
{
  longlong nr;
  char *res=result_field->ptr;

  nr=sint8korr(res);
  if (!args[0]->maybe_null)
    nr++;
  else
  {
    (void) args[0]->val_int();
    if (!args[0]->null_value)
      nr++;
  }
  int8store(res,nr);
}


void Item_sum_avg::update_field()
{
  longlong field_count;
  char *res=result_field->ptr;
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *arg_val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      binary2my_decimal(E_DEC_FATAL_ERROR, res,
                        dec_buffs + 1, f_precision, f_scale);
      field_count= sint8korr(res + dec_bin_size);
      my_decimal_add(E_DEC_FATAL_ERROR, dec_buffs, arg_val, dec_buffs + 1);
      field_count++;
      my_decimal2binary(E_DEC_FATAL_ERROR, dec_buffs,
                        res, f_precision, f_scale);
      res+= dec_bin_size;
      int8store(res, field_count);
    }
  }
  else
  {
    double nr, old_nr;

    float8get(old_nr, res);
    field_count= sint8korr(res + sizeof(double));

    nr= args[0]->val_real();
    if (!args[0]->null_value)
    {
      old_nr+= nr;
      field_count++;
    }
    float8store(res,old_nr);
    res+= sizeof(double);
    int8store(res, field_count);
  }
}

void Item_sum_hybrid::update_field()
{
  switch (hybrid_type)
  {
  case STRING_RESULT:
    min_max_update_str_field();
    break;
  case INT_RESULT:
    min_max_update_int_field();
    break;
  case DECIMAL_RESULT:
    min_max_update_decimal_field();
    break;
  default:
    min_max_update_real_field();
  }
}


void
Item_sum_hybrid::min_max_update_str_field()
{
  String *res_str=args[0]->val_str(&value);

  if (!args[0]->null_value)
  {
    res_str->strip_sp();
    result_field->val_str(&tmp_value);

    if (result_field->is_null() ||
	(cmp_sign * sortcmp(res_str,&tmp_value,collation.collation)) < 0)
      result_field->store(res_str->ptr(),res_str->length(),res_str->charset());
    result_field->set_notnull();
  }
}


void
Item_sum_hybrid::min_max_update_real_field()
{
  double nr,old_nr;

  old_nr=result_field->val_real();
  nr= args[0]->val_real();
  if (!args[0]->null_value)
  {
    if (result_field->is_null(0) ||
	(cmp_sign > 0 ? old_nr > nr : old_nr < nr))
      old_nr=nr;
    result_field->set_notnull();
  }
  else if (result_field->is_null(0))
    result_field->set_null();
  result_field->store(old_nr);
}


void
Item_sum_hybrid::min_max_update_int_field()
{
  longlong nr,old_nr;

  old_nr=result_field->val_int();
  nr=args[0]->val_int();
  if (!args[0]->null_value)
  {
    if (result_field->is_null(0))
      old_nr=nr;
    else
    {
      bool res=(unsigned_flag ?
		(ulonglong) old_nr > (ulonglong) nr :
		old_nr > nr);
      /* (cmp_sign > 0 && res) || (!(cmp_sign > 0) && !res) */
      if ((cmp_sign > 0) ^ (!res))
	old_nr=nr;
    }
    result_field->set_notnull();
  }
  else if (result_field->is_null(0))
    result_field->set_null();
  result_field->store(old_nr);
}


void
Item_sum_hybrid::min_max_update_decimal_field()
{
  /* TODO: optimize: do not get result_field in case of args[0] is NULL */
  my_decimal old_val, nr_val;
  const my_decimal *old_nr= result_field->val_decimal(&old_val);
  const my_decimal *nr= args[0]->val_decimal(&nr_val);
  if (!args[0]->null_value)
  {
    if (result_field->is_null(0))
      old_nr=nr;
    else
    {
      bool res= my_decimal_cmp(old_nr, nr) > 0;
      /* (cmp_sign > 0 && res) || (!(cmp_sign > 0) && !res) */
      if ((cmp_sign > 0) ^ (!res))
        old_nr=nr;
    }
    result_field->set_notnull();
  }
  else if (result_field->is_null(0))
    result_field->set_null();
  result_field->store_decimal(old_nr);
}


Item_avg_field::Item_avg_field(Item_result res_type, Item_sum_avg *item)
{
  name=item->name;
  decimals=item->decimals;
  max_length=item->max_length;
  field=item->result_field;
  maybe_null=1;
  hybrid_type= res_type;
  if (hybrid_type == DECIMAL_RESULT)
  {
    f_scale= item->f_scale;
    f_precision= item->f_precision;
    dec_bin_size= item->dec_bin_size;
  }
}


double Item_avg_field::val_real()
{
  // fix_fields() never calls for this Item
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *dec_val= val_decimal(&value);
    if (null_value)
      return 0.0;
    double d;
    my_decimal2double(E_DEC_FATAL_ERROR, dec_val, &d);
    return d;
  }
  else
  {
    double nr;
    longlong count;
    float8get(nr,field->ptr);
    char *res=(field->ptr+sizeof(double));
    count=sint8korr(res);

    if (!count)
    {
      null_value=1;
      return 0.0;
    }
    null_value=0;
    return nr/(double) count;
  }
}

longlong Item_avg_field::val_int()
{
  return (longlong)val_real();
}


my_decimal *Item_avg_field::val_decimal(my_decimal * val)
{
  // fix_fields() never calls for this Item
  longlong count= sint8korr(field->ptr + dec_bin_size);
  if ((null_value= !count))
    return NULL;

  my_decimal dec_count, dec_field;
  binary2my_decimal(E_DEC_FATAL_ERROR,
                    field->ptr, &dec_field, f_precision, f_scale);
  int2my_decimal(E_DEC_FATAL_ERROR, count, 0, &dec_count);
  my_decimal_div(E_DEC_FATAL_ERROR, val, &dec_field, &dec_count, 4);
  return val;
}


String *Item_avg_field::val_str(String *str)
{
  // fix_fields() never calls for this Item
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal value, *dec_val= val_decimal(&value);
    if (null_value)
      return NULL;
    my_decimal_round(E_DEC_FATAL_ERROR, dec_val, decimals, FALSE, &value);
    my_decimal2string(E_DEC_FATAL_ERROR, &value, 0, 0, 0, str);
  }
  else
  {
    double nr= Item_avg_field::val_real();
    if (null_value)
      return 0;
    str->set(nr, decimals, &my_charset_bin);
  }
  return str;
}

Item_std_field::Item_std_field(Item_sum_std *item)
  : Item_variance_field(item)
{
}

double Item_std_field::val_real()
{
  // fix_fields() never calls for this Item
  double tmp= Item_variance_field::val_real();
  return tmp <= 0.0 ? 0.0 : sqrt(tmp);
}

Item_variance_field::Item_variance_field(Item_sum_variance *item)
{
  name=item->name;
  decimals=item->decimals;
  max_length=item->max_length;
  field=item->result_field;
  maybe_null=1;
  if ((hybrid_type= item->hybrid_type) == DECIMAL_RESULT)
  {
    f_scale0= item->f_scale0;
    f_precision0= item->f_precision0;
    dec_bin_size0= item->dec_bin_size0;
    f_scale1= item->f_scale1;
    f_precision1= item->f_precision1;
    dec_bin_size1= item->dec_bin_size1;
  }
}

double Item_variance_field::val_real()
{
  // fix_fields() never calls for this Item
  if (hybrid_type == DECIMAL_RESULT)
  {
    my_decimal dec_buf, *dec_val= val_decimal(&dec_buf);
    if (null_value)
      return 0.0;
    double d;
    my_decimal2double(E_DEC_FATAL_ERROR, dec_val, &d);
    return d;
  }
  double sum,sum_sqr;
  longlong count;
  float8get(sum,field->ptr);
  float8get(sum_sqr,(field->ptr+sizeof(double)));
  count=sint8korr(field->ptr+sizeof(double)*2);

  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  null_value=0;
  double tmp= (double) count;
  double tmp2=(sum_sqr - sum*sum/tmp)/tmp;
  return tmp2 <= 0.0 ? 0.0 : tmp2;
}

String *Item_variance_field::val_str(String *str)
{
  // fix_fields() never calls for this Item
  double nr= val_real();
  if (null_value)
    return 0;
  str->set(nr,decimals, &my_charset_bin);
  return str;
}


my_decimal *Item_variance_field::val_decimal(my_decimal *dec_buf)
{
  // fix_fields() never calls for this Item
  longlong count= sint8korr(field->ptr+dec_bin_size0+dec_bin_size1);
  if ((null_value= !count))
    return 0;

  my_decimal dec_count, dec_sum, dec_sqr, tmp;
  int2my_decimal(E_DEC_FATAL_ERROR, count, 0, &dec_count);
  binary2my_decimal(E_DEC_FATAL_ERROR, field->ptr,
                    &dec_sum, f_precision0, f_scale0);
  binary2my_decimal(E_DEC_FATAL_ERROR, field->ptr+dec_bin_size0,
                    &dec_sqr, f_precision1, f_scale1);
  my_decimal_mul(E_DEC_FATAL_ERROR, &tmp, &dec_sum, &dec_sum);
  my_decimal_div(E_DEC_FATAL_ERROR, dec_buf, &tmp, &dec_count, 2);
  my_decimal_sub(E_DEC_FATAL_ERROR, &dec_sum, &dec_sqr, dec_buf);
  my_decimal_div(E_DEC_FATAL_ERROR, dec_buf, &dec_sum, &dec_count, 2);
  return dec_buf;
}


/****************************************************************************
** COUNT(DISTINCT ...)
****************************************************************************/

#include "sql_select.h"

int simple_str_key_cmp(void* arg, byte* key1, byte* key2)
{
  Item_sum_count_distinct* item = (Item_sum_count_distinct*)arg;
  CHARSET_INFO *cs=item->key_charset;
  uint len=item->key_length;
  return cs->coll->strnncollsp(cs, 
			       (const uchar*) key1, len, 
			       (const uchar*) key2, len, 0);
}

/*
  Did not make this one static - at least gcc gets confused when
  I try to declare a static function as a friend. If you can figure
  out the syntax to make a static function a friend, make this one
  static
*/

int composite_key_cmp(void* arg, byte* key1, byte* key2)
{
  Item_sum_count_distinct* item = (Item_sum_count_distinct*)arg;
  Field **field    = item->table->field;
  Field **field_end= field + item->table->s->fields;
  uint32 *lengths=item->field_lengths;
  for (; field < field_end; ++field)
  {
    Field* f = *field;
    int len = *lengths++;
    int res = f->cmp((char *) key1, (char *) key2);
    if (res)
      return res;
    key1 += len;
    key2 += len;
  }
  return 0;
}

/*
  helper function for walking the tree when we dump it to MyISAM -
  tree_walk will call it for each leaf
*/

int dump_leaf(byte* key, uint32 count __attribute__((unused)),
		     Item_sum_count_distinct* item)
{
  byte* buf = item->table->record[0];
  int error;
  /*
    The first item->rec_offset bytes are taken care of with
    restore_record(table,default_values) in setup()
  */
  memcpy(buf + item->rec_offset, key, item->tree->size_of_element);
  if ((error = item->table->file->write_row(buf)))
  {
    if (error != HA_ERR_FOUND_DUPP_KEY &&
	error != HA_ERR_FOUND_DUPP_UNIQUE)
      return 1;
  }
  return 0;
}


void Item_sum_count_distinct::cleanup()
{
  DBUG_ENTER("Item_sum_count_distinct::cleanup");
  Item_sum_int::cleanup();
  /*
    Free table and tree if they belong to this item (if item have not pointer
    to original item from which was made copy => it own its objects )
  */
  if (!original)
  {
    if (table)
    {
      free_tmp_table(current_thd, table);
      table= 0;
    }
    delete tmp_table_param;
    tmp_table_param= 0;
    if (use_tree)
    {
      delete_tree(tree);
      use_tree= 0;
    }
  }
  DBUG_VOID_RETURN;
}


/* This is used by rollup to create a separate usable copy of the function */

void Item_sum_count_distinct::make_unique()
{
  table=0;
  original= 0;
  use_tree= 0; // to prevent delete_tree call on uninitialized tree
  tree= &tree_base;
}


bool Item_sum_count_distinct::setup(THD *thd)
{
  List<Item> list;
  SELECT_LEX *select_lex= thd->lex->current_select;
  if (select_lex->linkage == GLOBAL_OPTIONS_TYPE)
    return 1;
    
  if (!(tmp_table_param= new TMP_TABLE_PARAM))
    return 1;

  /* Create a table with an unique key over all parameters */
  for (uint i=0; i < arg_count ; i++)
  {
    Item *item=args[i];
    if (list.push_back(item))
      return 1;					// End of memory
    if (item->const_item())
    {
      (void) item->val_int();
      if (item->null_value)
	always_null=1;
    }
  }
  if (always_null)
    return 0;
  count_field_types(tmp_table_param,list,0);
  if (table)
  {
    free_tmp_table(thd, table);
    tmp_table_param->cleanup();
  }
  if (!(table= create_tmp_table(thd, tmp_table_param, list, (ORDER*) 0, 1,
				0,
				select_lex->options | thd->options,
				HA_POS_ERROR, (char*)"")))
    return 1;
  table->file->extra(HA_EXTRA_NO_ROWS);		// Don't update rows
  table->no_rows=1;


  // no blobs, otherwise it would be MyISAM
  if (table->s->db_type == DB_TYPE_HEAP)
  {
    qsort_cmp2 compare_key;
    void* cmp_arg;

    // to make things easier for dump_leaf if we ever have to dump to MyISAM
    restore_record(table,s->default_values);

    if (table->s->fields == 1)
    {
      /*
	If we have only one field, which is the most common use of
	count(distinct), it is much faster to use a simpler key
	compare method that can take advantage of not having to worry
	about other fields
      */
      Field* field = table->field[0];
      switch (field->type()) {
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_VAR_STRING:
	if (field->binary())
	{
	  compare_key = (qsort_cmp2)simple_raw_key_cmp;
	  cmp_arg = (void*) &key_length;
	}
	else
	{
	  /*
	    If we have a string, we must take care of charsets and case
	    sensitivity
	  */
	  compare_key = (qsort_cmp2)simple_str_key_cmp;
	  cmp_arg = (void*) this;
	}
	break;
      default:
	/*
	  Since at this point we cannot have blobs anything else can
	  be compared with memcmp
	*/
	compare_key = (qsort_cmp2)simple_raw_key_cmp;
	cmp_arg = (void*) &key_length;
	break;
      }
      key_charset = field->charset();
      key_length  = field->pack_length();
      rec_offset  = 1;
    }
    else // too bad, cannot cheat - there is more than one field
    {
      bool all_binary = 1;
      Field** field, **field_end;
      field_end = (field = table->field) + table->s->fields;
      uint32 *lengths;
      if (!(field_lengths= 
	    (uint32*) thd->alloc(sizeof(uint32) * table->s->fields)))
	return 1;

      for (key_length = 0, lengths=field_lengths; field < field_end; ++field)
      {
	uint32 length= (*field)->pack_length();
	key_length += length;
	*lengths++ = length;
	if (!(*field)->binary())
	  all_binary = 0;			// Can't break loop here
      }
      rec_offset= table->s->reclength - key_length;
      if (all_binary)
      {
	compare_key = (qsort_cmp2)simple_raw_key_cmp;
	cmp_arg = (void*) &key_length;
      }
      else
      {
	compare_key = (qsort_cmp2) composite_key_cmp ;
	cmp_arg = (void*) this;
      }
    }

    if (use_tree)
      delete_tree(tree);
    init_tree(tree, min(thd->variables.max_heap_table_size,
			thd->variables.sortbuff_size/16), 0,
	      key_length, compare_key, 0, NULL, cmp_arg);
    use_tree = 1;

    /*
      The only time key_length could be 0 is if someone does
      count(distinct) on a char(0) field - stupid thing to do,
      but this has to be handled - otherwise someone can crash
      the server with a DoS attack
    */
    max_elements_in_tree = ((key_length) ? 
			    thd->variables.max_heap_table_size/key_length : 1);

  }
  if (original)
  {
    original->table= table;
    original->use_tree= use_tree;
  }
  return 0;
}


int Item_sum_count_distinct::tree_to_myisam()
{
  if (create_myisam_from_heap(current_thd, table, tmp_table_param,
			      HA_ERR_RECORD_FILE_FULL, 1) ||
      tree_walk(tree, (tree_walk_action)&dump_leaf, (void*)this,
		left_root_right))
    return 1;
  delete_tree(tree);
  use_tree = 0;
  return 0;
}


Item *Item_sum_count_distinct::copy_or_same(THD* thd) 
{
  return new (thd->mem_root) Item_sum_count_distinct(thd, this);
}


void Item_sum_count_distinct::clear()
{
  if (use_tree)
    reset_tree(tree);
  else if (table)
  {
    table->file->extra(HA_EXTRA_NO_CACHE);
    table->file->delete_all_rows();
    table->file->extra(HA_EXTRA_WRITE_CACHE);
  }
}

bool Item_sum_count_distinct::add()
{
  int error;
  if (always_null)
    return 0;
  copy_fields(tmp_table_param);
  copy_funcs(tmp_table_param->items_to_copy);

  for (Field **field=table->field ; *field ; field++)
    if ((*field)->is_real_null(0))
      return 0;					// Don't count NULL

  if (use_tree)
  {
    /*
      If the tree got too big, convert to MyISAM, otherwise insert into the
      tree.
    */
    if (tree->elements_in_tree > max_elements_in_tree)
    {
      if (tree_to_myisam())
	return 1;
    }
    else if (!tree_insert(tree, table->record[0] + rec_offset, 0,
			  tree->custom_arg))
      return 1;
  }
  else if ((error=table->file->write_row(table->record[0])))
  {
    if (error != HA_ERR_FOUND_DUPP_KEY &&
	error != HA_ERR_FOUND_DUPP_UNIQUE)
    {
      if (create_myisam_from_heap(current_thd, table, tmp_table_param, error,
				  1))
	return 1;				// Not a table_is_full error
    }
  }
  return 0;
}


longlong Item_sum_count_distinct::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (!table)					// Empty query
    return LL(0);
  if (use_tree)
    return tree->elements_in_tree;
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  return table->file->records;
}


void Item_sum_count_distinct::print(String *str)
{
  str->append("count(distinct ", 15);
  args[0]->print(str);
  str->append(')');
}

/****************************************************************************
** Functions to handle dynamic loadable aggregates
** Original source by: Alexis Mikhailov <root@medinf.chuvashia.su>
** Adapted for UDAs by: Andreas F. Bobak <bobak@relog.ch>.
** Rewritten by: Monty.
****************************************************************************/

#ifdef HAVE_DLOPEN

void Item_udf_sum::clear()
{
  DBUG_ENTER("Item_udf_sum::clear");
  udf.clear();
  DBUG_VOID_RETURN;
}

bool Item_udf_sum::add()
{
  DBUG_ENTER("Item_udf_sum::add");
  udf.add(&null_value);
  DBUG_RETURN(0);
}

Item *Item_sum_udf_float::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_udf_float(thd, this);
}

double Item_sum_udf_float::val_real()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_sum_udf_float::val");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));
  DBUG_RETURN(udf.val(&null_value));
}

String *Item_sum_udf_float::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  double nr= val_real();
  if (null_value)
    return 0;					/* purecov: inspected */
  str->set(nr,decimals, &my_charset_bin);
  return str;
}


Item *Item_sum_udf_int::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_udf_int(thd, this);
}


String *Item_sum_udf_decimal::val_str(String *str)
{
  my_decimal dec_buf, *dec= udf.val_decimal(&null_value, &dec_buf);
  if (null_value)
    return 0;
  if (str->length() < DECIMAL_MAX_STR_LENGTH)
    str->length(DECIMAL_MAX_STR_LENGTH);
  my_decimal_round(E_DEC_FATAL_ERROR, dec, decimals, FALSE, &dec_buf);
  my_decimal2string(E_DEC_FATAL_ERROR, &dec_buf, 0, 0, '0', str);
  return str;
}


double Item_sum_udf_decimal::val_real()
{
  my_decimal dec_buf, *dec= udf.val_decimal(&null_value, &dec_buf);
  if (null_value)
    return 0.0;
  double result;
  my_decimal2double(E_DEC_FATAL_ERROR, dec, &result);
  return result;
}


longlong Item_sum_udf_decimal::val_int()
{
  my_decimal dec_buf, *dec= udf.val_decimal(&null_value, &dec_buf);
  if (null_value)
    return 0;
  longlong result;
  my_decimal2int(E_DEC_FATAL_ERROR, dec, unsigned_flag, &result);
  return result;
}


my_decimal *Item_sum_udf_decimal::val_decimal(my_decimal *dec_buf)
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_func_udf_decimal::val_decimal");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
                     args[0]->result_type(), arg_count));

  DBUG_RETURN(udf.val_decimal(&null_value, dec_buf));
}


Item *Item_sum_udf_decimal::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_udf_decimal(thd, this);
}


longlong Item_sum_udf_int::val_int()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_sum_udf_int::val_int");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));
  DBUG_RETURN(udf.val_int(&null_value));
}


String *Item_sum_udf_int::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  longlong nr=val_int();
  if (null_value)
    return 0;
  str->set(nr, &my_charset_bin);
  return str;
}

/* Default max_length is max argument length */

void Item_sum_udf_str::fix_length_and_dec()
{
  DBUG_ENTER("Item_sum_udf_str::fix_length_and_dec");
  max_length=0;
  for (uint i = 0; i < arg_count; i++)
    set_if_bigger(max_length,args[i]->max_length);
  DBUG_VOID_RETURN;
}


Item *Item_sum_udf_str::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_sum_udf_str(thd, this);
}


String *Item_sum_udf_str::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_sum_udf_str::str");
  String *res=udf.val_str(str,&str_value);
  null_value = !res;
  DBUG_RETURN(res);
}

#endif /* HAVE_DLOPEN */


/*****************************************************************************
 GROUP_CONCAT function

 SQL SYNTAX:
  GROUP_CONCAT([DISTINCT] expr,... [ORDER BY col [ASC|DESC],...] 
    [SEPARATOR str_const])

 concat of values from "group by" operation

 BUGS
   DISTINCT and ORDER BY only works if ORDER BY uses all fields and only fields
   in expression list
   Blobs doesn't work with DISTINCT or ORDER BY
*****************************************************************************/

/*
  function of sort for syntax:
  GROUP_CONCAT(DISTINCT expr,...)
*/

int group_concat_key_cmp_with_distinct(void* arg, byte* key1,
				       byte* key2)
{
  Item_func_group_concat* grp_item= (Item_func_group_concat*)arg;
  Item **field_item, **end;
  char *record= (char*) grp_item->table->record[0];

  for (field_item= grp_item->args, end= field_item + grp_item->arg_count_field;
       field_item < end;
       field_item++)
  {
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
    */
    Field *field= (*field_item)->get_tmp_table_field();
    if (field)
    {
      int res;
      uint offset= (uint) (field->ptr - record);
      if ((res= field->cmp((char *) key1 + offset, (char *) key2 + offset)))
	return res;
    }
  }
  return 0;
}


/*
  function of sort for syntax:
  GROUP_CONCAT(expr,... ORDER BY col,... )
*/

int group_concat_key_cmp_with_order(void* arg, byte* key1, byte* key2)
{
  Item_func_group_concat* grp_item= (Item_func_group_concat*) arg;
  ORDER **order_item, **end;
  char *record= (char*) grp_item->table->record[0];

  for (order_item= grp_item->order, end=order_item+ grp_item->arg_count_order;
       order_item < end;
       order_item++)
  {
    Item *item= *(*order_item)->item;
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
    */
    Field *field= item->get_tmp_table_field();
    if (field)
    {
      int res;
      uint offset= (uint) (field->ptr - record);
      if ((res= field->cmp((char *) key1 + offset, (char *) key2 + offset)))
        return (*order_item)->asc ? res : -res;
    }
  }
  /*
    We can't return 0 because in that case the tree class would remove this
    item as double value. This would cause problems for case-changes and
    if the the returned values are not the same we do the sort on.
  */
  return 1;
}


/*
  function of sort for syntax:
  GROUP_CONCAT(DISTINCT expr,... ORDER BY col,... )

  BUG:
    This doesn't work in the case when the order by contains data that
    is not part of the field list because tree-insert will not notice
    the duplicated values when inserting things sorted by ORDER BY
*/

int group_concat_key_cmp_with_distinct_and_order(void* arg,byte* key1,
						 byte* key2)
{
  if (!group_concat_key_cmp_with_distinct(arg,key1,key2))
    return 0;
  return(group_concat_key_cmp_with_order(arg,key1,key2));
}


/*
  Append data from current leaf to item->result
*/

int dump_leaf_key(byte* key, uint32 count __attribute__((unused)),
                  Item_func_group_concat *item)
{
  char buff[MAX_FIELD_WIDTH];
  String tmp((char *)&buff,sizeof(buff),default_charset_info), tmp2;
  char *record= (char*) item->table->record[0];

  if (item->result.length())
    item->result.append(*item->separator);

  tmp.length(0);
  
  for (uint i= 0; i < item->arg_count_field; i++)
  {
    Item *show_item= item->args[i];
    if (!show_item->const_item())
    {
      /*
	We have to use get_tmp_table_field() instead of
	real_item()->get_tmp_table_field() because we want the field in
	the temporary table, not the original field
      */
      Field *field= show_item->get_tmp_table_field();
      String *res;
      char *save_ptr= field->ptr;
      uint offset= (uint) (save_ptr - record);
      DBUG_ASSERT(offset < item->table->s->reclength);
      field->ptr= (char *) key + offset;
      res= field->val_str(&tmp,&tmp2);
      item->result.append(*res);
      field->ptr= save_ptr;
    }
    else 
    {
      String *res= show_item->val_str(&tmp);
      if (res)
        item->result.append(*res);
    }
  }

  /* stop if length of result more than group_concat_max_len */  
  if (item->result.length() > item->group_concat_max_len)
  {
    item->count_cut_values++;
    item->result.length(item->group_concat_max_len);
    item->warning_for_row= TRUE;
    return 1;
  }
  return 0;
}


/*
  Constructor of Item_func_group_concat
  is_distinct - distinct
  is_select - list of expression for show values
  is_order - list of sort columns 
  is_separator - string value of separator
*/

Item_func_group_concat::Item_func_group_concat(bool is_distinct,
					       List<Item> *is_select,
					       SQL_LIST *is_order,
					       String *is_separator)
  :Item_sum(), tmp_table_param(0), max_elements_in_tree(0), warning(0),
   key_length(0), tree_mode(0), distinct(is_distinct), warning_for_row(0),
   separator(is_separator), tree(&tree_base), table(0),
   order(0), tables_list(0),
   arg_count_order(0), arg_count_field(0),
   count_cut_values(0)
{
  Item *item_select;
  Item **arg_ptr;

  original= 0;
  quick_group= 0;
  mark_as_sum_func();
  order= 0;
  group_concat_max_len= current_thd->variables.group_concat_max_len;
    
  arg_count_field= is_select->elements;
  arg_count_order= is_order ? is_order->elements : 0;
  arg_count= arg_count_field + arg_count_order;
  
  /*
    We need to allocate:
    args - arg_count_field+arg_count_order
           (for possible order items in temporare tables)
    order - arg_count_order
  */
  if (!(args= (Item**) sql_alloc(sizeof(Item*) * arg_count +
				 sizeof(ORDER*)*arg_count_order)))
    return;

  order= (ORDER**)(args + arg_count);

  /* fill args items of show and sort */
  List_iterator_fast<Item> li(*is_select);

  for (arg_ptr=args ; (item_select= li++) ; arg_ptr++)
    *arg_ptr= item_select;

  if (arg_count_order) 
  {
    ORDER **order_ptr= order;
    for (ORDER *order_item= (ORDER*) is_order->first;
	 order_item != NULL;
	 order_item= order_item->next)
    {
      (*order_ptr++)= order_item;
      *arg_ptr= *order_item->item;
      order_item->item= arg_ptr++;
    }
  }
}
  

Item_func_group_concat::Item_func_group_concat(THD *thd,
					       Item_func_group_concat *item)
  :Item_sum(thd, item),item_thd(thd),
  tmp_table_param(item->tmp_table_param),
  max_elements_in_tree(item->max_elements_in_tree),
  warning(item->warning),
  key_length(item->key_length), 
  tree_mode(item->tree_mode),
  distinct(item->distinct),
  warning_for_row(item->warning_for_row),
  separator(item->separator),
  tree(item->tree),
  table(item->table),
  order(item->order),
  tables_list(item->tables_list),
  group_concat_max_len(item->group_concat_max_len),
  arg_count_order(item->arg_count_order),
  arg_count_field(item->arg_count_field),
  field_list_offset(item->field_list_offset),
  count_cut_values(item->count_cut_values),
  original(item)
{
  quick_group= item->quick_group;
}



void Item_func_group_concat::cleanup()
{
  DBUG_ENTER("Item_func_group_concat::cleanup");
  Item_sum::cleanup();

  /*
    Free table and tree if they belong to this item (if item have not pointer
    to original item from which was made copy => it own its objects )
  */
  if (!original)
  {
    THD *thd= current_thd;
    if (table)
    {
      free_tmp_table(thd, table);
      table= 0;
    }
    delete tmp_table_param;
    tmp_table_param= 0;
    if (tree_mode)
    {
      tree_mode= 0;
      delete_tree(tree); 
    }
    if (warning)
    {
      char warn_buff[MYSQL_ERRMSG_SIZE];
      sprintf(warn_buff, ER(ER_CUT_VALUE_GROUP_CONCAT), count_cut_values);
      warning->set_msg(thd, warn_buff);
      warning= 0;
    }
  }
  DBUG_VOID_RETURN;
}


Item_func_group_concat::~Item_func_group_concat()
{
}


Item *Item_func_group_concat::copy_or_same(THD* thd)
{
  return new (thd->mem_root) Item_func_group_concat(thd, this);
}


void Item_func_group_concat::clear()
{
  result.length(0);
  result.copy();
  null_value= TRUE;
  warning_for_row= FALSE;
  if (table)
  {
    table->file->extra(HA_EXTRA_NO_CACHE);
    table->file->delete_all_rows();
    table->file->extra(HA_EXTRA_WRITE_CACHE);
  }
  if (tree_mode)
    reset_tree(tree);
}


bool Item_func_group_concat::add()
{
  if (always_null)
    return 0;
  copy_fields(tmp_table_param);
  copy_funcs(tmp_table_param->items_to_copy);

  for (uint i= 0; i < arg_count_field; i++)
  {
    Item *show_item= args[i];
    if (!show_item->const_item())
    {
      /*
	Here we use real_item as we want the original field data that should
	be written to table->record[0]
      */
      Field *f= show_item->real_item()->get_tmp_table_field();
      if (f->is_null())
	return 0;				// Skip row if it contains null
    }
  }

  null_value= FALSE;

  TREE_ELEMENT *el= 0;                          // Only for safety
  if (tree_mode)
    el= tree_insert(tree, table->record[0], 0, tree->custom_arg);
  /*
    If the row is not a duplicate (el->count == 1)
    we can dump the row here in case of GROUP_CONCAT(DISTINCT...)
    instead of doing tree traverse later.
  */
  if (result.length() <= group_concat_max_len && 
      !warning_for_row &&
      (!tree_mode || (el->count == 1 && distinct && !arg_count_order)))
    dump_leaf_key(table->record[0], 1, this);

  return 0;
}


void Item_func_group_concat::reset_field()
{
  if (tree_mode)
    reset_tree(tree);
}


bool
Item_func_group_concat::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  uint i;			/* for loop variable */ 
  DBUG_ASSERT(fixed == 0);

  if (!thd->allow_sum_func)
  {
    my_message(ER_INVALID_GROUP_FUNC_USE, ER(ER_INVALID_GROUP_FUNC_USE),
               MYF(0));
    return TRUE;
  }
  
  thd->allow_sum_func= 0;
  maybe_null= 0;
  item_thd= thd;

  /*
    Fix fields for select list and ORDER clause
  */

  for (i=0 ; i < arg_count ; i++)  
  {
    if ((!args[i]->fixed &&
         args[i]->fix_fields(thd, tables, args + i)) ||
        args[i]->check_cols(1))
      return TRUE;
    if (i < arg_count_field)
      maybe_null|= args[i]->maybe_null;
  }

  result_field= 0;
  null_value= 1;
  max_length= group_concat_max_len;
  thd->allow_sum_func= 1;
  if (!(tmp_table_param= new TMP_TABLE_PARAM))
    return TRUE;
  /* We'll convert all blobs to varchar fields in the temporary table */
  tmp_table_param->convert_blob_length= group_concat_max_len;
  tables_list= tables;
  fixed= 1;
  return FALSE;
}


bool Item_func_group_concat::setup(THD *thd)
{
  List<Item> list;
  SELECT_LEX *select_lex= thd->lex->current_select;
  uint const_fields;
  qsort_cmp2 compare_key;
  DBUG_ENTER("Item_func_group_concat::setup");

  if (select_lex->linkage == GLOBAL_OPTIONS_TYPE)
    DBUG_RETURN(1);

  /*
    push all not constant fields to list and create temp table
  */
  const_fields= 0;
  always_null= 0;
  for (uint i= 0; i < arg_count_field; i++)
  {
    Item *item= args[i];
    if (list.push_back(item))
      DBUG_RETURN(1);
    if (item->const_item())
    {
      const_fields++;
      (void) item->val_int();
      if (item->null_value)
	always_null= 1;
    }
  }
  if (always_null)
    DBUG_RETURN(0);

  List<Item> all_fields(list);
  if (arg_count_order)
  {
    bool hidden_group_fields;
    setup_group(thd, args, tables_list, list, all_fields, *order,
                &hidden_group_fields);
  }

  count_field_types(tmp_table_param,all_fields,0);
  if (table)
  {
    /*
      We come here when we are getting the result from a temporary table,
      not the original tables used in the query
    */
    free_tmp_table(thd, table);
    tmp_table_param->cleanup();
  }
  /*
    We have to create a temporary table to get descriptions of fields 
    (types, sizes and so on).

    Note that in the table, we first have the ORDER BY fields, then the
    field list.

    We need to set set_sum_field in true for storing value of blob in buffer 
    of a record instead of a pointer of one. 
  */
  if (!(table=create_tmp_table(thd, tmp_table_param, all_fields, 
			       (ORDER*) 0, 0, TRUE,
                               select_lex->options | thd->options,
			       HA_POS_ERROR,(char *) "")))
    DBUG_RETURN(1);
  table->file->extra(HA_EXTRA_NO_ROWS);
  table->no_rows= 1;

  key_length= table->s->reclength;

  /* Offset to first result field in table */
  field_list_offset= table->s->fields - (list.elements - const_fields);

  if (tree_mode)
    delete_tree(tree);

  /* choose function of sort */  
  tree_mode= distinct || arg_count_order; 
  if (tree_mode)
  {
    if (arg_count_order)
    {
      if (distinct)
        compare_key= (qsort_cmp2) group_concat_key_cmp_with_distinct_and_order;
      else
        compare_key= (qsort_cmp2) group_concat_key_cmp_with_order;
    }
    else
    {
      compare_key= (qsort_cmp2) group_concat_key_cmp_with_distinct;
    }
    /*
      Create a tree of sort. Tree is used for a sort and a remove double 
      values (according with syntax of the function). If function doesn't
      contain DISTINCT and ORDER BY clauses, we don't create this tree.
    */
    init_tree(tree, min(thd->variables.max_heap_table_size,
			thd->variables.sortbuff_size/16), 0,
              key_length, compare_key, 0, NULL, (void*) this);
    max_elements_in_tree= (key_length ? 
			   thd->variables.max_heap_table_size/key_length : 1);
  };

  /*
    Copy table and tree_mode if they belong to this item (if item have not 
    pointer to original item from which was made copy => it own its objects)
  */
  if (original)
  {
    original->table= table;
    original->tree_mode= tree_mode;
  }
  DBUG_RETURN(0);
}


/* This is used by rollup to create a separate usable copy of the function */

void Item_func_group_concat::make_unique()
{
  table=0;
  original= 0;
  tree_mode= 0; // to prevent delete_tree call on uninitialized tree
  tree= &tree_base;
}


String* Item_func_group_concat::val_str(String* str)
{
  DBUG_ASSERT(fixed == 1);
  if (null_value)
    return 0;
  if (count_cut_values && !warning)
    warning= push_warning(item_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_CUT_VALUE_GROUP_CONCAT,
                          ER(ER_CUT_VALUE_GROUP_CONCAT));
  if (result.length())
    return &result;
  if (tree_mode)
  {
    tree_walk(tree, (tree_walk_action)&dump_leaf_key, (void*)this,
              left_root_right);
  }
  return &result;
}


void Item_func_group_concat::print(String *str)
{
  str->append("group_concat(", 13);
  if (distinct)
    str->append("distinct ", 9);
  for (uint i= 0; i < arg_count_field; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str);
  }
  if (arg_count_order)
  {
    str->append(" order by ", 10);
    for (uint i= 0 ; i < arg_count_order ; i++)
    {
      if (i)
	str->append(',');
      (*order[i]->item)->print(str);
    }
  }
  str->append(" separator \'", 12);
  str->append(*separator);
  str->append("\')", 2);
}
