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
{
  arg_count=list.elements;
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

// Constructor used in processing select with temporary tebles
Item_sum::Item_sum(THD *thd, Item_sum &item):
  Item_result_field(thd, item), quick_group(item.quick_group)
{
  arg_count= item.arg_count;
  if (arg_count <= 2)
    args=tmp_args;
  else
    if (!(args=(Item**) sql_alloc(sizeof(Item*)*arg_count)))
      return;
  for (uint i= 0; i < arg_count; i++)
    args[i]= item.args[i];
}

void Item_sum::mark_as_sum_func()
{
  current_thd->lex.current_select->with_sum_func++;
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
    Field *result_field= sum_item->result_field;
    for (uint i=0 ; i < sum_item->arg_count ; i++)
    {
      Item *arg= sum_item->args[i];
      if (!arg->const_item())
      {
	if (arg->type() == Item::FIELD_ITEM)
	  ((Item_field*) arg)->field= result_field++;
	else
	  sum_item->args[i]= new Item_field(result_field++);
      }
    }
  }
  return sum_item;
}

String *
Item_sum_num::val_str(String *str)
{
  double nr=val();
  if (null_value)
    return 0;
  str->set(nr,decimals,default_charset());
  return str;
}


String *
Item_sum_int::val_str(String *str)
{
  longlong nr=val_int();
  if (null_value)
    return 0;
  str->set(nr,default_charset());
  return str;
}


bool
Item_sum_num::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  if (!thd->allow_sum_func)
  {
    my_error(ER_INVALID_GROUP_FUNC_USE,MYF(0));
    return 1;
  }
  thd->allow_sum_func=0;			// No included group funcs
  decimals=0;
  maybe_null=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (args[i]->fix_fields(thd, tables, args + i) || args[i]->check_cols(1))
      return 1;
    if (decimals < args[i]->decimals)
      decimals=args[i]->decimals;
    maybe_null |= args[i]->maybe_null;
  }
  result_field=0;
  max_length=float_length(decimals);
  null_value=1;
  fix_length_and_dec();
  thd->allow_sum_func=1;			// Allow group functions
  fixed= 1;
  return 0;
}


bool
Item_sum_hybrid::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  Item *item=args[0];
  if (!thd->allow_sum_func)
  {
    my_error(ER_INVALID_GROUP_FUNC_USE,MYF(0));
    return 1;
  }
  thd->allow_sum_func=0;			// No included group funcs
  if (item->fix_fields(thd, tables, args) || item->check_cols(1))
    return 1;
  hybrid_type=item->result_type();
  if (hybrid_type == INT_RESULT)
  {
    cmp_charset= &my_charset_bin;
    max_length=20;
  }
  else if (hybrid_type == REAL_RESULT)
  {
    cmp_charset= &my_charset_bin;
    max_length=float_length(decimals);
  }else
  {
    cmp_charset= item->charset();
    max_length=item->max_length;
  }
  decimals=item->decimals;
  maybe_null=item->maybe_null;
  unsigned_flag=item->unsigned_flag;
  set_charset(item->charset());
  result_field=0;
  null_value=1;
  fix_length_and_dec();
  thd->allow_sum_func=1;			// Allow group functions
  if (item->type() == Item::FIELD_ITEM)
    hybrid_field_type= ((Item_field*) item)->field->type();
  else
    hybrid_field_type= Item::field_type();
  fixed= 1;
  return 0;
}


/***********************************************************************
** reset and add of sum_func
***********************************************************************/

void Item_sum_sum::reset()
{
  null_value=0; sum=0.0; Item_sum_sum::add();
}

bool Item_sum_sum::add()
{
  sum+=args[0]->val();
  return 0;
}

double Item_sum_sum::val()
{
  return sum;
}


void Item_sum_count::reset()
{
  count=0; add();
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
  return (longlong) count;
}

/*
** Avgerage
*/

void Item_sum_avg::reset()
{
  sum=0.0; count=0; Item_sum_avg::add();
}

bool Item_sum_avg::add()
{
  double nr=args[0]->val();
  if (!args[0]->null_value)
  {
    sum+=nr;
    count++;
  }
  return 0;
}

double Item_sum_avg::val()
{
  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  null_value=0;
  return sum/ulonglong2double(count);
}


/*
** Standard deviation
*/

double Item_sum_std::val()
{
  double tmp= Item_sum_variance::val();
  return tmp <= 0.0 ? 0.0 : sqrt(tmp);
}

/*
** variance
*/

void Item_sum_variance::reset()
{
  sum=sum_sqr=0.0; 
  count=0; 
  (void) Item_sum_variance::add();
}

bool Item_sum_variance::add()
{
  double nr=args[0]->val();
  if (!args[0]->null_value)
  {
    sum+=nr;
    sum_sqr+=nr*nr;
    count++;
  }
  return 0;
}

double Item_sum_variance::val()
{
  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  null_value=0;
  /* Avoid problems when the precision isn't good enough */
  double tmp=ulonglong2double(count);
  double tmp2=(sum_sqr - sum*sum/tmp)/tmp;
  return tmp2 <= 0.0 ? 0.0 : tmp2;
}

void Item_sum_variance::reset_field()
{
  double nr=args[0]->val();
  char *res=result_field->ptr;

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

void Item_sum_variance::update_field(int offset)
{
  double nr,old_nr,old_sqr;
  longlong field_count;
  char *res=result_field->ptr;

  float8get(old_nr,res+offset);
  float8get(old_sqr,res+offset+sizeof(double));
  field_count=sint8korr(res+offset+sizeof(double)*2);

  nr=args[0]->val();
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

double Item_sum_hybrid::val()
{
  int err;
  if (null_value)
    return 0.0;
  switch (hybrid_type) {
  case STRING_RESULT:
    String *res;  res=val_str(&str_value);
    return (res ? my_strntod(res->charset(), (char*) res->ptr(),res->length(),
			     (char**) 0, &err) : 0.0);
  case INT_RESULT:
    if (unsigned_flag)
      return ulonglong2double(sum_int);
    return (double) sum_int;
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
  if (null_value)
    return 0;
  if (hybrid_type == INT_RESULT)
    return sum_int;
  return (longlong) Item_sum_hybrid::val();
}


String *
Item_sum_hybrid::val_str(String *str)
{
  if (null_value)
    return 0;
  switch (hybrid_type) {
  case STRING_RESULT:
    return &value;
  case REAL_RESULT:
    str->set(sum,decimals,default_charset());
    break;
  case INT_RESULT:
    if (unsigned_flag)
      str->set((ulonglong) sum_int,default_charset());
    else
      str->set((longlong) sum_int,default_charset());
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return str;					// Keep compiler happy
}

bool Item_sum_min::add()
{
  switch (hybrid_type) {
  case STRING_RESULT:
  {
    String *result=args[0]->val_str(&tmp_value);
    if (!args[0]->null_value &&
	(null_value || sortcmp(&value,result,cmp_charset) > 0))
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
  case REAL_RESULT:
  {
    double nr=args[0]->val();
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


bool Item_sum_max::add()
{
  switch (hybrid_type) {
  case STRING_RESULT:
  {
    String *result=args[0]->val_str(&tmp_value);
    if (!args[0]->null_value &&
	(null_value || sortcmp(&value,result,cmp_charset) < 0))
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
  case REAL_RESULT:
  {
    double nr=args[0]->val();
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
  return (longlong) bits;
}

void Item_sum_bit::reset()
{
  bits=reset_bits; add();
}

bool Item_sum_or::add()
{
  ulonglong value= (ulonglong) args[0]->val_int();
  if (!args[0]->null_value)
    bits|=value;
  return 0;
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
  double nr=args[0]->val();
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
  if (hybrid_type == STRING_RESULT)
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
  }
  else if (hybrid_type == INT_RESULT)
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
  }
  else						// REAL_RESULT
  {
    double nr=args[0]->val();

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
  }
}


void Item_sum_sum::reset_field()
{
  double nr=args[0]->val();			// Nulls also return 0
  float8store(result_field->ptr,nr);
  null_value=0;
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
  double nr=args[0]->val();
  char *res=result_field->ptr;

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

void Item_sum_bit::reset_field()
{
  char *res=result_field->ptr;
  ulonglong nr=(ulonglong) args[0]->val_int();
  int8store(res,nr);
}

/*
** calc next value and merge it with field_value
*/

void Item_sum_sum::update_field(int offset)
{
  double old_nr,nr;
  char *res=result_field->ptr;

  float8get(old_nr,res+offset);
  nr=args[0]->val();
  if (!args[0]->null_value)
    old_nr+=nr;
  float8store(res,old_nr);
}


void Item_sum_count::update_field(int offset)
{
  longlong nr;
  char *res=result_field->ptr;

  nr=sint8korr(res+offset);
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


void Item_sum_avg::update_field(int offset)
{
  double nr,old_nr;
  longlong field_count;
  char *res=result_field->ptr;

  float8get(old_nr,res+offset);
  field_count=sint8korr(res+offset+sizeof(double));

  nr=args[0]->val();
  if (!args[0]->null_value)
  {
    old_nr+=nr;
    field_count++;
  }
  float8store(res,old_nr);
  res+=sizeof(double);
  int8store(res,field_count);
}

void Item_sum_hybrid::update_field(int offset)
{
  if (hybrid_type == STRING_RESULT)
    min_max_update_str_field(offset);
  else if (hybrid_type == INT_RESULT)
    min_max_update_int_field(offset);
  else
    min_max_update_real_field(offset);
}


void
Item_sum_hybrid::min_max_update_str_field(int offset)
{
  String *res_str=args[0]->val_str(&value);

  if (args[0]->null_value)
    result_field->copy_from_tmp(offset);	// Use old value
  else
  {
    res_str->strip_sp();
    result_field->ptr+=offset;			// Get old max/min
    result_field->val_str(&tmp_value,&tmp_value);
    result_field->ptr-=offset;

    if (result_field->is_null() ||
	(cmp_sign * sortcmp(res_str,&tmp_value,cmp_charset)) < 0)
      result_field->store(res_str->ptr(),res_str->length(),res_str->charset());
    else
    {						// Use old value
      char *res=result_field->ptr;
      memcpy(res,res+offset,result_field->pack_length());
    }
    result_field->set_notnull();
  }
}


void
Item_sum_hybrid::min_max_update_real_field(int offset)
{
  double nr,old_nr;

  result_field->ptr+=offset;
  old_nr=result_field->val_real();
  nr=args[0]->val();
  if (!args[0]->null_value)
  {
    if (result_field->is_null(offset) ||
	(cmp_sign > 0 ? old_nr > nr : old_nr < nr))
      old_nr=nr;
    result_field->set_notnull();
  }
  else if (result_field->is_null(offset))
    result_field->set_null();
  result_field->ptr-=offset;
  result_field->store(old_nr);
}


void
Item_sum_hybrid::min_max_update_int_field(int offset)
{
  longlong nr,old_nr;

  result_field->ptr+=offset;
  old_nr=result_field->val_int();
  nr=args[0]->val_int();
  if (!args[0]->null_value)
  {
    if (result_field->is_null(offset))
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
  else if (result_field->is_null(offset))
    result_field->set_null();
  result_field->ptr-=offset;
  result_field->store(old_nr);
}


void Item_sum_or::update_field(int offset)
{
  ulonglong nr;
  char *res=result_field->ptr;

  nr=uint8korr(res+offset);
  nr|= (ulonglong) args[0]->val_int();
  int8store(res,nr);
}


void Item_sum_and::update_field(int offset)
{
  ulonglong nr;
  char *res=result_field->ptr;

  nr=uint8korr(res+offset);
  nr&= (ulonglong) args[0]->val_int();
  int8store(res,nr);
}


Item_avg_field::Item_avg_field(Item_sum_avg *item)
{
  name=item->name;
  decimals=item->decimals;
  max_length=item->max_length;
  field=item->result_field;
  maybe_null=1;
}

double Item_avg_field::val()
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

String *Item_avg_field::val_str(String *str)
{
  double nr=Item_avg_field::val();
  if (null_value)
    return 0;
  str->set(nr,decimals,default_charset());
  return str;
}

Item_std_field::Item_std_field(Item_sum_std *item)
  : Item_variance_field(item)
{
}

double Item_std_field::val()
{
  double tmp= Item_variance_field::val();
  return tmp <= 0.0 ? 0.0 : sqrt(tmp);
}

Item_variance_field::Item_variance_field(Item_sum_variance *item)
{
  name=item->name;
  decimals=item->decimals;
  max_length=item->max_length;
  field=item->result_field;
  maybe_null=1;
}

double Item_variance_field::val()
{
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
  double nr=val();
  if (null_value)
    return 0;
  str->set(nr,decimals,default_charset());
  return str;
}

/****************************************************************************
** COUNT(DISTINCT ...)
****************************************************************************/

#include "sql_select.h"

int simple_raw_key_cmp(void* arg, byte* key1, byte* key2)
{
  return memcmp(key1, key2, *(uint*) arg);
}

int simple_str_key_cmp(void* arg, byte* key1, byte* key2)
{
  Item_sum_count_distinct* item = (Item_sum_count_distinct*)arg;
  CHARSET_INFO *cs=item->key_charset;
  uint len=item->key_length;
  return my_strnncoll(cs, (const uchar*) key1, len, (const uchar*) key2, len);
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
  Field **field_end= field + item->table->fields;
  uint32 *lengths=item->field_lengths;
  for (; field < field_end; ++field)
  {
    Field* f = *field;
    int len = *lengths++;
    int res = f->key_cmp(key1, key2);
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
    restore_record(table,2) in setup()
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


Item_sum_count_distinct::~Item_sum_count_distinct()
{
  /*
    Free table and tree if they belong to this item (if item have not pointer
    to original item from which was made copy => it own its objects )
  */
  if (!original)
  {
    if (table)
      free_tmp_table(current_thd, table);
    delete tmp_table_param;
    if (use_tree)
      delete_tree(tree);
  }
}

bool Item_sum_count_distinct::fix_fields(THD *thd, TABLE_LIST *tables,
					 Item **ref)
{
  if (Item_sum_num::fix_fields(thd, tables, ref) ||
      !(tmp_table_param= new TMP_TABLE_PARAM))
    return 1;
  return 0;
}

bool Item_sum_count_distinct::setup(THD *thd)
{
  List<Item> list;
  SELECT_LEX *select_lex= current_lex->current_select->select_lex();
  if (select_lex->linkage == GLOBAL_OPTIONS_TYPE)
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
				HA_POS_ERROR)))
    return 1;
  table->file->extra(HA_EXTRA_NO_ROWS);		// Don't update rows
  table->no_rows=1;


  // no blobs, otherwise it would be MyISAM
  if (table->db_type == DB_TYPE_HEAP)
  {
    qsort_cmp2 compare_key;
    void* cmp_arg;

    // to make things easier for dump_leaf if we ever have to dump to MyISAM
    restore_record(table,2);

    if (table->fields == 1)
    {
      /*
	If we have only one field, which is the most common use of
	count(distinct), it is much faster to use a simpler key
	compare method that can take advantage of not having to worry
	about other fields
      */
      Field* field = table->field[0];
      switch(field->type())
      {
      case FIELD_TYPE_STRING:
      case FIELD_TYPE_VAR_STRING:
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
      field_end = (field = table->field) + table->fields;
      uint32 *lengths;
      if (!(field_lengths= 
	    (uint32*) thd->alloc(sizeof(uint32) * table->fields)))
	return 1;

      for (key_length = 0, lengths=field_lengths; field < field_end; ++field)
      {
	uint32 length= (*field)->pack_length();
	key_length += length;
	*lengths++ = length;
	if (!(*field)->binary())
	  all_binary = 0;			// Can't break loop here
      }
      rec_offset = table->reclength - key_length;
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

void Item_sum_count_distinct::reset()
{
  if (use_tree)
    reset_tree(tree);
  else if (table)
  {
    table->file->extra(HA_EXTRA_NO_CACHE);
    table->file->delete_all_rows();
    table->file->extra(HA_EXTRA_WRITE_CACHE);
  }
  (void) add();
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
  if (!table)					// Empty query
    return LL(0);
  if (use_tree)
    return tree->elements_in_tree;
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  return table->file->records;
}

/****************************************************************************
** Functions to handle dynamic loadable aggregates
** Original source by: Alexis Mikhailov <root@medinf.chuvashia.su>
** Adapted for UDAs by: Andreas F. Bobak <bobak@relog.ch>.
** Rewritten by: Monty.
****************************************************************************/

#ifdef HAVE_DLOPEN

void Item_udf_sum::reset()
{
  DBUG_ENTER("Item_udf_sum::reset");
  udf.reset(&null_value);
  DBUG_VOID_RETURN;
}

bool Item_udf_sum::add()
{
  DBUG_ENTER("Item_udf_sum::add");
  udf.add(&null_value);
  DBUG_RETURN(0);
}

double Item_sum_udf_float::val()
{
  DBUG_ENTER("Item_sum_udf_float::val");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));
  DBUG_RETURN(udf.val(&null_value));
}

String *Item_sum_udf_float::val_str(String *str)
{
  double nr=val();
  if (null_value)
    return 0;					/* purecov: inspected */
  else
    str->set(nr,decimals,default_charset());
  return str;
}


longlong Item_sum_udf_int::val_int()
{
  DBUG_ENTER("Item_sum_udf_int::val_int");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));
  DBUG_RETURN(udf.val_int(&null_value));
}

String *Item_sum_udf_int::val_str(String *str)
{
  longlong nr=val_int();
  if (null_value)
    return 0;
  else
    str->set(nr,default_charset());
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

String *Item_sum_udf_str::val_str(String *str)
{
  DBUG_ENTER("Item_sum_udf_str::str");
  String *res=udf.val_str(str,&str_value);
  null_value = !res;
  DBUG_RETURN(res);
}

#endif /* HAVE_DLOPEN */


/*****************************************************************************
 GROUP_CONCAT function
 Syntax:
 GROUP_CONCAT([DISTINCT] expr,... [ORDER BY col [ASC|DESC],...] 
   [SEPARATOR str_const])
 concat of values from "group by" operation
*****************************************************************************/

/*
  function of sort for syntax:
  GROUP_CONCAT(DISTINCT expr,...)
*/

static int group_concat_key_cmp_with_distinct(void* arg, byte* key1,
					      byte* key2)
{
  Item_func_group_concat* item= (Item_func_group_concat*)arg;
  for (int i= 0; i<item->arg_count_field; i++)
  {
    Item *field_item= item->expr[i];
    Field *field= field_item->tmp_table_field();
    if (field)
    {
      uint offset= field->offset();

      int res= field->key_cmp(key1 + offset, key2 + offset);
      /*
        if key1 and key2 is not equal than field->key_cmp return offset. This
	function must return value 1 for this case.
      */
      if (res)
        return 1;
    }
  }
  return 0;
}


/*
  function of sort for syntax:
  GROUP_CONCAT(expr,... ORDER BY col,... )
*/

static int group_concat_key_cmp_with_order(void* arg, byte* key1, byte* key2)
{
  Item_func_group_concat* item= (Item_func_group_concat*)arg;
  for (int i=0; i<item->arg_count_order; i++)
  {
    ORDER *order_item= item->order[i];
    Item *item= *order_item->item;
    Field *field= item->tmp_table_field();
    if (field)
    {
      uint offset= field->offset();

      bool dir= order_item->asc;
      int res= field->key_cmp(key1 + offset, key2 + offset);
      if (res)
        return dir ? res : -res;
    }
  }
  /*
    We can't return 0 because tree class remove this item as double value. 
  */   
  return 1;
}


/*
  function of sort for syntax:
  GROUP_CONCAT(DISTINCT expr,... ORDER BY col,... )
*/

static int group_concat_key_cmp_with_distinct_and_order(void* arg, byte* key1, byte* key2)
{
  Item_func_group_concat* item= (Item_func_group_concat*)arg;
  if (!group_concat_key_cmp_with_distinct(arg,key1,key2))
    return 0;
  return(group_concat_key_cmp_with_order(arg,key1,key2));
}


/*
  create result
  item is pointer to Item_func_group_concat
*/

static int dump_leaf_key(byte* key, uint32 count __attribute__((unused)),
                  Item_func_group_concat *group_concat_item)
{
  char buff[MAX_FIELD_WIDTH];
  String tmp((char *)&buff,sizeof(buff),default_charset_info);
  String tmp2((char *)&buff,sizeof(buff),default_charset_info);

  tmp.length(0);
  
  for (int i= 0; i < group_concat_item->arg_show_fields; i++)
  {
    Item *show_item= group_concat_item->expr[i];
    if (!show_item->const_item())
    {
      Field *f= show_item->tmp_table_field();
      uint offset= f->offset();
      char *sv= f->ptr;
      f->ptr= (char *)key + offset;
      String *res= f->val_str(&tmp,&tmp2);
      group_concat_item->result.append(*res);
      f->ptr= sv;
    }
    else 
    {
      String *res= show_item->val_str(&tmp);
      if (res)
        group_concat_item->result.append(*res);
    }
  }
  if (group_concat_item->tree_mode) // Last item of tree
  {
    group_concat_item->show_elements++;
    if (group_concat_item->show_elements < 
        group_concat_item->tree->elements_in_tree)
      group_concat_item->result.append(*group_concat_item->separator);
  }
  else
  {
    group_concat_item->result.append(*group_concat_item->separator); 
  }
  /*
    if length of result more than group_concat_max_len - stop !
  */  
  if (group_concat_item->result.length() > 
      group_concat_item->group_concat_max_len)
  {
    group_concat_item->count_cut_values++;
    group_concat_item->result.length(group_concat_item->group_concat_max_len);
    group_concat_item->warning_for_row= TRUE;
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

Item_func_group_concat::Item_func_group_concat(int is_distinct,
					       List<Item> *is_select,
					       SQL_LIST *is_order,
					       String *is_separator)
  :Item_sum(), tmp_table_param(0), warning_available(false),
   separator(is_separator), tree(&tree_base), table(0), distinct(is_distinct),
   tree_mode(0), count_cut_values(0)
{
  original= 0;
  quick_group= 0;
  mark_as_sum_func();
  SELECT_LEX *select_lex= current_lex->current_select->select_lex();
  order= 0;
    
  arg_show_fields= arg_count_field= is_select->elements;
  arg_count_order= is_order ? is_order->elements : 0;
  arg_count= arg_count_field;
  
  /*
    We need to allocate:
    args - arg_count+arg_count_order (for possible order items in temporare 
           tables)
    expr - arg_count_field
    order - arg_count_order
  */
  args= (Item**)sql_alloc(sizeof(Item*)*(arg_count+arg_count_order+arg_count_field)+
                          sizeof(ORDER*)*arg_count_order);
  if (!args)
  {
    my_error(ER_OUTOFMEMORY,MYF(0));
  } 
  expr= args;
  expr+= arg_count+arg_count_order;
  if (arg_count_order) 
  {
    order= (ORDER**)(expr + arg_count_field);
  }
  /*
    fill args items of show and sort
  */
  int i= 0;
  List_iterator_fast<Item> li(*is_select);
  Item *item_select;

  while ((item_select= li++))
  {
    args[i]= expr[i]= item_select;
    i++;
  }
      
  if (order)
  {
    uint j= 0;	
    for (ORDER *order_item= (ORDER*)is_order->first;
                order_item != NULL;
                order_item= order_item->next)
    {
      order[j++]= order_item;
    }
  }
}


Item_func_group_concat::~Item_func_group_concat()
{
  /*
    Free table and tree if they belong to this item (if item have not pointer
    to original item from which was made copy => it own its objects )
  */
  if (!original)
  {
    THD *thd= current_thd;
    if (warning_available)
    {
      char warn_buff[MYSQL_ERRMSG_SIZE];
      sprintf(warn_buff, ER(ER_CUT_VALUE_GROUP_CONCAT), count_cut_values);
      warning->set_msg(thd, warn_buff);
    }
    if (table)
      free_tmp_table(thd, table);
    if (tmp_table_param)
      delete tmp_table_param;
    if (tree_mode)
      delete_tree(tree); 
  }
}


void Item_func_group_concat::reset()
{
  result.length(0);
  result.copy();
  null_value= TRUE;
  warning_for_row= false;
  if (table)
  {
    table->file->extra(HA_EXTRA_NO_CACHE);
    table->file->delete_all_rows();
    table->file->extra(HA_EXTRA_WRITE_CACHE);
  }
  if (tree_mode)
    reset_tree(tree);
  add();
}


bool Item_func_group_concat::add()
{
  copy_fields(tmp_table_param);
  copy_funcs(tmp_table_param->items_to_copy);

  bool record_is_null= TRUE;
  for (int i= 0; i < arg_show_fields; i++)
  {
    Item *show_item= expr[i];
    if (!show_item->const_item())
    {
      Field *f= show_item->tmp_table_field();
      if (!f->is_null())
        record_is_null= FALSE;      
    }
  }
  if (record_is_null)
    return 0;
  null_value= FALSE;
  if (tree_mode)
  {
    if (!tree_insert(tree, table->record[0], 0,tree->custom_arg))
      return 1;
  }
  else
  {
    if (result.length() <= group_concat_max_len && !warning_for_row)
      dump_leaf_key(table->record[0],1,
                    (Item_func_group_concat*)this);
  }
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

  if (!thd->allow_sum_func)
  {
    my_error(ER_INVALID_GROUP_FUNC_USE,MYF(0));
    return 1;
  }
  
  thd->allow_sum_func= 0;
  maybe_null= 0;
  for (i= 0 ; i < arg_count ; i++)
  {
    if (args[i]->fix_fields(thd, tables, args + i) || args[i]->check_cols(1))
      return 1;
    maybe_null |= args[i]->maybe_null;
  }
  for (i= 0 ; i < arg_count_field ; i++)
  {
    if (expr[i]->fix_fields(thd, tables, expr + i) || expr[i]->check_cols(1))
      return 1;
    maybe_null |= expr[i]->maybe_null;
  }
  /*
    Fix fields for order clause in function:
    GROUP_CONCAT(expr,... ORDER BY col,... )
  */
  for (i= 0 ; i < arg_count_order ; i++)
  {
    ORDER *order_item= order[i];
    Item *item=*order_item->item;
    if (item->fix_fields(thd, tables, &item) || item->check_cols(1))
      return 1;
  }
  result_field= 0;
  null_value= 1;
  fix_length_and_dec();
  thd->allow_sum_func= 1;			
  if (!(tmp_table_param= new TMP_TABLE_PARAM))
    return 1;
  tables_list= tables;
  fixed= 1;
  return 0;
}


bool Item_func_group_concat::setup(THD *thd)
{
  List<Item> list;
  SELECT_LEX *select_lex= current_lex->current_select->select_lex();

  if (select_lex->linkage == GLOBAL_OPTIONS_TYPE)
    return 1;
  /*
    all not constant fields are push to list and create temp table
  */ 
  for (uint i= 0; i < arg_count; i++)
  {
    Item *item= args[i];
    if (list.push_back(item))
      return 1;
    if (item->const_item())
    {
      (void) item->val_int();
      if (item->null_value)
	always_null= 1;
    }
  }
        
  List<Item> all_fields(list);
  if (arg_count_order) 
  {
    bool hidden_group_fields;
    setup_group(thd, args, tables_list, list, all_fields, *order,
                &hidden_group_fields);
  }
  
  count_field_types(tmp_table_param,all_fields,0);
  /*
    We have to create a temporary table for that we get descriptions of fields 
    (types, sizes and so on).
  */
  if (!(table=create_tmp_table(thd, tmp_table_param, all_fields, 0,
        0, 0, 0,select_lex->options | thd->options)))
    return 1;
  table->file->extra(HA_EXTRA_NO_ROWS);
  table->no_rows= 1;
  qsort_cmp2 compare_key;
  
  tree_mode= distinct || arg_count_order;
  /*
    choise function of sort
  */  
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
      if (distinct)
        compare_key= (qsort_cmp2) group_concat_key_cmp_with_distinct;
      else 
       compare_key= NULL; 
    }
    /*
      Create a tree of sort. Tree is used for a sort and a remove dubl 
      values (according with syntax of the function). If function does't
      contain DISTINCT and ORDER BY clauses, we don't create this tree.
    */
    init_tree(tree, min(thd->variables.max_heap_table_size,
              thd->variables.sortbuff_size/16), 0,
              table->reclength, compare_key, 0, NULL, (void*) this);
    max_elements_in_tree= ((table->reclength) ? 
           thd->variables.max_heap_table_size/table->reclength : 1);
  };
  item_thd= thd;

  group_concat_max_len= thd->variables.group_concat_max_len;

  /*
    Copy table and tree_mode if they belong to this item (if item have not 
    pointer to original item from which was made copy => it own its objects)
  */
  if (original)
  {
    original->table= table;
    original->tree_mode= tree_mode;
  }
  return 0;
}

String* Item_func_group_concat::val_str(String* str)
{
  if (null_value)
    return 0;
  if (tree_mode)
  {
    show_elements= 0;
    tree_walk(tree, (tree_walk_action)&dump_leaf_key, (void*)this,
              left_root_right);
  }
  else
  {
    if (!warning_for_row)
      result.length(result.length()-separator->length());
  }
  if (count_cut_values && !warning_available)
  {
    warning_available= TRUE;
    warning= push_warning(item_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                           ER_CUT_VALUE_GROUP_CONCAT, NULL);
  }
  return &result;
}
