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


/* This file defines all compare functions */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include "assert.h"

/*
  Test functions
  These returns 0LL if false and 1LL if true and null if some arg is null
  'AND' and 'OR' never return null
*/

longlong Item_func_not::val_int()
{
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return !null_value && value == 0 ? 1 : 0;
}

/*
  Convert a constant expression or string to an integer.
  This is done when comparing DATE's of different formats and
  also when comparing bigint to strings (in which case the string
  is converted once to a bigint).

  RESULT VALUES
  0	Can't convert item
  1	Item was replaced with an integer version of the item
*/

static bool convert_constant_item(Field *field, Item **item)
{
  if ((*item)->const_item() && (*item)->type() != Item::INT_ITEM)
  {
    if (!(*item)->save_in_field(field) && !((*item)->null_value))
    {
      Item *tmp=new Item_int_with_ref(field->val_int(), *item);
      if (tmp)
	*item=tmp;
      return 1;					// Item was replaced
    }
  }
  return 0;
}


void Item_bool_func2::fix_length_and_dec()
{
  max_length=1;					// Function returns 0 or 1

  /*
    As some compare functions are generated after sql_yacc,
    we have to check for out of memory conditons here
  */
  if (!args[0] || !args[1])
    return;
  // Make a special case of compare with fields to get nicer DATE comparisons
  if (args[0]->type() == FIELD_ITEM)
  {
    Field *field=((Item_field*) args[0])->field;
    if (field->store_for_compare())
    {
      if (convert_constant_item(field,&args[1]))
      {
	cmp_func= new Compare_func_int(this);  // Works for all types.
	return;
      }
    }
  }
  if (args[1]->type() == FIELD_ITEM)
  {
    Field *field=((Item_field*) args[1])->field;
    if (field->store_for_compare())
    {
      if (convert_constant_item(field,&args[0]))
      {
	cmp_func= new Compare_func_int(this);  // Works for all types.
	return;
      }
    }
  }
  set_cmp_func(args[0], args[1]);
}

Compare_func* Compare_func::get_compare_func(Item_bool_func2 *owner,
					     Item *a, Item* b)
{
  switch (item_cmp_type(a->result_type(), b->result_type()))
  {
  case STRING_RESULT:
    return new Compare_func_string(owner);
  case REAL_RESULT:
    return new Compare_func_real(owner);
  case INT_RESULT:
    return new Compare_func_int(owner);
  case ROW_RESULT:
    return new Compare_func_row(owner, a, b);
  }
  return 0;
}

Compare_func_row::Compare_func_row(Item_bool_func2 *owner, Item *a, Item* b):
  Compare_func(owner)
{
  uint n= a->cols();
  if (n != b->cols())
  {
    my_error(ER_CARDINALITY_COL, MYF(0), n);
    cmp_func= 0;
    return;
  }
  cmp_func= (Compare_func **) sql_alloc(sizeof(Compare_func*)*n);
  for(uint i=0; i < n; i++)
    cmp_func[i]= Compare_func::get_compare_func(owner, a->el(i), b->el(i));
}

int Compare_func_string::compare(Item *a, Item *b)
{
  String *res1,*res2;
  if ((res1= a->val_str(&owner->tmp_value1)))
  {
    if ((res2= b->val_str(&owner->tmp_value2)))
    {
      owner->null_value= 0;
      return owner->binary() ? stringcmp(res1,res2) : sortcmp(res1,res2);
    }
  }
  owner->null_value= 1;
  return -1;
}

int Compare_func_real::compare(Item *a, Item *b)
{
  double val1= a->val();
  if (!a->null_value)
  {
    double val2= b->val();
    if (!b->null_value)
    {
      owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2) return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}


int Compare_func_int::compare(Item *a, Item *b)
{
  longlong val1= a->val_int();
  if (!a->null_value)
  {
    longlong val2= b->val_int();
    if (!b->null_value)
    {
      owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2)   return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}

int Compare_func_row::compare(Item *a, Item *b)
{
  int res= 0;
  uint n= a->cols();
  for(uint i= 0; i<n; i++)
  {
    if ((res= cmp_func[i]->compare(a->el(i), b->el(i))))
      return res;
    if (owner->null_value)
      return -1;
  }
  return res;
}

longlong Item_func_eq::val_int()
{
  int value= cmp_func->compare(args[0], args[1]);
  return value == 0 ? 1 : 0;
}

/* Same as Item_func_eq, but NULL = NULL */

void Item_func_equal::fix_length_and_dec()
{
  Item_bool_func2::fix_length_and_dec();
  cmp_result_type=item_cmp_type(args[0]->result_type(),args[1]->result_type());
  maybe_null=null_value=0;
}

longlong Item_func_equal::val_int()
{
  switch (cmp_result_type) {
  case STRING_RESULT:
  {
    String *res1,*res2;
    res1=args[0]->val_str(&tmp_value1);
    res2=args[1]->val_str(&tmp_value2);
    if (!res1 || !res2)
      return test(res1 == res2);
    return (binary() ? test(stringcmp(res1,res2) == 0) :
	    test(sortcmp(res1,res2) == 0));
  }
  case REAL_RESULT:
  {
    double val1=args[0]->val();
    double val2=args[1]->val();
    if (args[0]->null_value || args[1]->null_value)
      return test(args[0]->null_value && args[1]->null_value);
    return test(val1 == val2);
  }
  case INT_RESULT:
  {
    longlong val1=args[0]->val_int();
    longlong val2=args[1]->val_int();
    if (args[0]->null_value || args[1]->null_value)
      return test(args[0]->null_value && args[1]->null_value);
    return test(val1 == val2);
  }
  case ROW_RESULT:
  {
    my_error(ER_WRONG_USAGE, MYF(0), "row", "<=>");
    return 0;
  }
  }
  return 0;					// Impossible
}


longlong Item_func_ne::val_int()
{
  int value= cmp_func->compare(args[0], args[1]);
  return value != 0 && !null_value ? 1 : 0;
}


longlong Item_func_ge::val_int()
{
  int value= cmp_func->compare(args[0], args[1]);
  return value >= 0 ? 1 : 0;
}


longlong Item_func_gt::val_int()
{
  int value= cmp_func->compare(args[0], args[1]);
  return value > 0 ? 1 : 0;
}

longlong Item_func_le::val_int()
{
  int value= cmp_func->compare(args[0], args[1]);
  return value <= 0 && !null_value ? 1 : 0;
}


longlong Item_func_lt::val_int()
{
  int value= cmp_func->compare(args[0], args[1]);
  return value < 0 && !null_value ? 1 : 0;
}


longlong Item_func_strcmp::val_int()
{
  String *a=args[0]->val_str(&tmp_value1);
  String *b=args[1]->val_str(&tmp_value2);
  if (!a || !b)
  {
    null_value=1;
    return 0;
  }
  int value= binary() ? stringcmp(a,b) : sortcmp(a,b);
  null_value=0;
  return !value ? 0 : (value < 0 ? (longlong) -1 : (longlong) 1);
}


void Item_func_interval::fix_length_and_dec()
{
  bool nums=1;
  uint i;
  for (i=0 ; i < arg_count ; i++)
  {
    if (!args[i])
      return;					// End of memory
    if (args[i]->type() != Item::INT_ITEM &&
	args[i]->type() != Item::REAL_ITEM)
    {
      nums=0;
      break;
    }
  }
  if (nums && arg_count >= 8)
  {
    if ((intervals=(double*) sql_alloc(sizeof(double)*arg_count)))
    {
      for (i=0 ; i < arg_count ; i++)
	intervals[i]=args[i]->val();
    }
  }
  maybe_null=0; max_length=2;
  used_tables_cache|=item->used_tables();
}

/*
  return -1 if null value,
	  0 if lower than lowest
	  1 - arg_count if between args[n] and args[n+1]
	  arg_count+1 if higher than biggest argument
*/

longlong Item_func_interval::val_int()
{
  double value=item->val();
  if (item->null_value)
    return -1;				// -1 if null /* purecov: inspected */
  if (intervals)
  {					// Use binary search to find interval
    uint start,end;
    start=0; end=arg_count-1;
    while (start != end)
    {
      uint mid=(start+end+1)/2;
      if (intervals[mid] <= value)
	start=mid;
      else
	end=mid-1;
    }
    return (value < intervals[start]) ? 0 : start+1;
  }
  if (args[0]->val() > value)
    return 0;
  for (uint i=1 ; i < arg_count ; i++)
  {
    if (args[i]->val() > value)
      return i;
  }
  return (longlong) arg_count;
}


void Item_func_interval::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}

bool Item_func_interval::check_loop(uint id)
{
  DBUG_ENTER("Item_func_interval::check_loop");
  if (Item_func::check_loop(id))
    DBUG_RETURN(1);
  DBUG_RETURN(item->check_loop(id));
}

void Item_func_between::fix_length_and_dec()
{
   max_length=1;

  /*
    As some compare functions are generated after sql_yacc,
    we have to check for out of memory conditons here
  */
  if (!args[0] || !args[1] || !args[2])
    return;
  cmp_type=args[0]->result_type();
  if (args[0]->binary())
    string_compare=stringcmp;
  else
    string_compare=sortcmp;

  // Make a special case of compare with fields to get nicer DATE comparisons
  if (args[0]->type() == FIELD_ITEM)
  {
    Field *field=((Item_field*) args[0])->field;
    if (field->store_for_compare())
    {
      if (convert_constant_item(field,&args[1]))
	cmp_type=INT_RESULT;			// Works for all types.
      if (convert_constant_item(field,&args[2]))
	cmp_type=INT_RESULT;			// Works for all types.
    }
  }
}


longlong Item_func_between::val_int()
{						// ANSI BETWEEN
  if (cmp_type == STRING_RESULT)
  {
    String *value,*a,*b;
    value=args[0]->val_str(&value0);
    if ((null_value=args[0]->null_value))
      return 0;
    a=args[1]->val_str(&value1);
    b=args[2]->val_str(&value2);
    if (!args[1]->null_value && !args[2]->null_value)
      return (string_compare(value,a) >= 0 && string_compare(value,b) <= 0) ?
	1 : 0;
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= string_compare(value,b) <= 0; // not null if false range.
    }
    else
    {
      null_value= string_compare(value,a) >= 0; // not null if false range.
    }
  }
  else if (cmp_type == INT_RESULT)
  {
    longlong value=args[0]->val_int(),a,b;
    if ((null_value=args[0]->null_value))
      return 0;					/* purecov: inspected */
    a=args[1]->val_int();
    b=args[2]->val_int();
    if (!args[1]->null_value && !args[2]->null_value)
      return (value >= a && value <= b) ? 1 : 0;
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= value <= b;			// not null if false range.
    }
    else
    {
      null_value= value >= a;
    }
  }
  else
  {
    double value=args[0]->val(),a,b;
    if ((null_value=args[0]->null_value))
      return 0;					/* purecov: inspected */
    a=args[1]->val();
    b=args[2]->val();
    if (!args[1]->null_value && !args[2]->null_value)
      return (value >= a && value <= b) ? 1 : 0;
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= value <= b;			// not null if false range.
    }
    else
    {
      null_value= value >= a;
    }
  }
  return 0;
}

void
Item_func_ifnull::fix_length_and_dec()
{
  maybe_null=args[1]->maybe_null;
  max_length=max(args[0]->max_length,args[1]->max_length);
  decimals=max(args[0]->decimals,args[1]->decimals);
  cached_result_type=args[0]->result_type();
}

double
Item_func_ifnull::val()
{
  double value=args[0]->val();
  if (!args[0]->null_value)
  {
    null_value=0;
    return value;
  }
  value=args[1]->val();
  if ((null_value=args[1]->null_value))
    return 0.0;
  return value;
}

longlong
Item_func_ifnull::val_int()
{
  longlong value=args[0]->val_int();
  if (!args[0]->null_value)
  {
    null_value=0;
    return value;
  }
  value=args[1]->val_int();
  if ((null_value=args[1]->null_value))
    return 0;
  return value;
}

String *
Item_func_ifnull::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if (!args[0]->null_value)
  {
    null_value=0;
    return res;
  }
  res=args[1]->val_str(str);
  if ((null_value=args[1]->null_value))
    return 0;
  return res;
}


void
Item_func_if::fix_length_and_dec()
{
  maybe_null=args[1]->maybe_null || args[2]->maybe_null;
  max_length=max(args[1]->max_length,args[2]->max_length);
  decimals=max(args[1]->decimals,args[2]->decimals);
  enum Item_result arg1_type=args[1]->result_type();
  enum Item_result arg2_type=args[2]->result_type();
  bool null1=args[1]->null_value;
  bool null2=args[2]->null_value;

  if (null1)
  {
    cached_result_type= arg2_type;
    set_charset(args[2]->charset());
  }
  else if (null2)
  {
    cached_result_type= arg1_type;
    set_charset(args[1]->charset());
  }
  else if (arg1_type == STRING_RESULT || arg2_type == STRING_RESULT)
  {
    cached_result_type = STRING_RESULT;
    set_charset( (args[1]->binary() || args[2]->binary()) ? 
		my_charset_bin : args[1]->charset());
  }
  else
  {
    set_charset(my_charset_bin);	// Number
    if (arg1_type == REAL_RESULT || arg2_type == REAL_RESULT)
      cached_result_type = REAL_RESULT;
    else
      cached_result_type=arg1_type;		// Should be INT_RESULT
  }
}


double
Item_func_if::val()
{
  Item *arg= args[0]->val_int() ? args[1] : args[2];
  double value=arg->val();
  null_value=arg->null_value;
  return value;
}

longlong
Item_func_if::val_int()
{
  Item *arg= args[0]->val_int() ? args[1] : args[2];
  longlong value=arg->val_int();
  null_value=arg->null_value;
  return value;
}

String *
Item_func_if::val_str(String *str)
{
  Item *arg= args[0]->val_int() ? args[1] : args[2];
  String *res=arg->val_str(str);
  null_value=arg->null_value;
  return res;
}


void
Item_func_nullif::fix_length_and_dec()
{
  Item_bool_func2::fix_length_and_dec();
  maybe_null=1;
  if (args[0])					// Only false if EOM
  {
    max_length=args[0]->max_length;
    decimals=args[0]->decimals;
    cached_result_type=args[0]->result_type();
  }
}

/*
  nullif () returns NULL if arguments are different, else it returns the
  first argument.
  Note that we have to evaluate the first argument twice as the compare
  may have been done with a different type than return value
*/

double
Item_func_nullif::val()
{
  double value;
  if (!cmp_func->compare(args[0], args[1]) || null_value)
  {
    null_value=1;
    return 0.0;
  }
  value=args[0]->val();
  null_value=args[0]->null_value;
  return value;
}

longlong
Item_func_nullif::val_int()
{
  longlong value;
  if (!cmp_func->compare(args[0], args[1]) || null_value)
  {
    null_value=1;
    return 0;
  }
  value=args[0]->val_int();
  null_value=args[0]->null_value;
  return value;
}

String *
Item_func_nullif::val_str(String *str)
{
  String *res;
  if (!cmp_func->compare(args[0], args[1]) || null_value)
  {
    null_value=1;
    return 0;
  }
  res=args[0]->val_str(str);
  null_value=args[0]->null_value;
  return res;
}

/*
  CASE expression 
  Return the matching ITEM or NULL if all compares (including else) failed
*/

Item *Item_func_case::find_item(String *str)
{
  String *first_expr_str,*tmp;
  longlong first_expr_int;
  double   first_expr_real;
  bool int_used, real_used,str_used;
  int_used=real_used=str_used=0;

  /* These will be initialized later */
  LINT_INIT(first_expr_str);
  LINT_INIT(first_expr_int);
  LINT_INIT(first_expr_real);

  // Compare every WHEN argument with it and return the first match
  for (uint i=0 ; i < arg_count ; i+=2)
  {
    if (!first_expr)
    {
      // No expression between CASE and first WHEN
      if (args[i]->val_int())
	return args[i+1];
      continue;
    }
    switch (args[i]->result_type()) {
    case STRING_RESULT:
      if (!str_used)
      {
	str_used=1;
	// We can't use 'str' here as this may be overwritten
	if (!(first_expr_str= first_expr->val_str(&str_value)))
	  return else_expr;			// Impossible
      }
      if ((tmp=args[i]->val_str(str)))		// If not null
      {
	if (first_expr->binary() || args[i]->binary())
	{
	  if (stringcmp(tmp,first_expr_str)==0)
	    return args[i+1];
	}
	else if (sortcmp(tmp,first_expr_str)==0)
	  return args[i+1];
      }
      break;
    case INT_RESULT:
      if (!int_used)
      {
	int_used=1;
	first_expr_int= first_expr->val_int();
	if (first_expr->null_value)
	  return else_expr;
      }
      if (args[i]->val_int()==first_expr_int && !args[i]->null_value) 
        return args[i+1];
      break;
    case REAL_RESULT: 
      if (!real_used)
      {
	real_used=1;
	first_expr_real= first_expr->val();
	if (first_expr->null_value)
	  return else_expr;
      }
      if (args[i]->val()==first_expr_real && !args[i]->null_value) 
        return args[i+1];
      break;
    case ROW_RESULT:
      // This case should never be choosen
      DBUG_ASSERT(0);
      break;
    }
  }
  // No, WHEN clauses all missed, return ELSE expression
  return else_expr;
}



String *Item_func_case::val_str(String *str)
{
  String *res;
  Item *item=find_item(str);

  if (!item)
  {
    null_value=1;
    return 0;
  }
  if (!(res=item->val_str(str)))
    null_value=1;
  return res;
}


longlong Item_func_case::val_int()
{
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff,sizeof(buff),default_charset_info);
  Item *item=find_item(&dummy_str);
  longlong res;

  if (!item)
  {
    null_value=1;
    return 0;
  }
  res=item->val_int();
  null_value=item->null_value;
  return res;
}

double Item_func_case::val()
{
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff,sizeof(buff),default_charset_info);
  Item *item=find_item(&dummy_str);
  double res;

  if (!item)
  {
    null_value=1;
    return 0;
  }
  res=item->val();
  null_value=item->null_value;
  return res;
}


bool
Item_func_case::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  if (first_expr && (first_expr->check_cols(1) ||
		     first_expr->fix_fields(thd, tables, &first_expr)) ||
      else_expr && (else_expr->check_cols(1) ||
		    else_expr->fix_fields(thd, tables, &else_expr)))
    return 1;
  if (Item_func::fix_fields(thd, tables, ref))
    return 1;
  if (first_expr)
  {
    used_tables_cache|=(first_expr)->used_tables();
    const_item_cache&= (first_expr)->const_item();
  }
  if (else_expr)
  {
    used_tables_cache|=(else_expr)->used_tables();
    const_item_cache&= (else_expr)->const_item();
  }
  if (!else_expr || else_expr->maybe_null)
    maybe_null=1;				// The result may be NULL
  return 0;
}

bool Item_func_case::check_loop(uint id)
{
  DBUG_ENTER("Item_func_case::check_loop");
  if (Item_func::check_loop(id))
    DBUG_RETURN(1);

  DBUG_RETURN((first_expr && first_expr->check_loop(id)) ||
	      (else_expr && else_expr->check_loop(id)));
}

void Item_func_case::update_used_tables()
{
  Item_func::update_used_tables();
  if (first_expr)
  {
    used_tables_cache|=(first_expr)->used_tables();
    const_item_cache&= (first_expr)->const_item();
  }
  if (else_expr)
  {
    used_tables_cache|=(else_expr)->used_tables();
    const_item_cache&= (else_expr)->const_item();
  }
}


void Item_func_case::fix_length_and_dec()
{
  max_length=0;
  decimals=0;
  cached_result_type = args[1]->result_type();
  for (uint i=0 ; i < arg_count ; i+=2)
  {
    set_if_bigger(max_length,args[i+1]->max_length);
    set_if_bigger(decimals,args[i+1]->decimals);
  }
  if (else_expr != NULL) 
  {
    set_if_bigger(max_length,else_expr->max_length);
    set_if_bigger(decimals,else_expr->decimals);
  }
}

/* TODO:  Fix this so that it prints the whole CASE expression */

void Item_func_case::print(String *str)
{
  str->append("case ");				// Not yet complete
}

/*
  Coalesce - return first not NULL argument.
*/

String *Item_func_coalesce::val_str(String *str)
{
  null_value=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    String *res;
    if ((res=args[i]->val_str(str)))
      return res;
  }
  null_value=1;
  return 0;
}

longlong Item_func_coalesce::val_int()
{
  null_value=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    longlong res=args[i]->val_int();
    if (!args[i]->null_value)
      return res;
  }
  null_value=1;
  return 0;
}

double Item_func_coalesce::val()
{
  null_value=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    double res=args[i]->val();
    if (!args[i]->null_value)
      return res;
  }
  null_value=1;
  return 0;
}


void Item_func_coalesce::fix_length_and_dec()
{
  max_length=0;
  decimals=0;
  cached_result_type = args[0]->result_type();
  for (uint i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length,args[i]->max_length);
    set_if_bigger(decimals,args[i]->decimals);
  }
}

/****************************************************************************
 Classes and function for the IN operator
****************************************************************************/

static int cmp_longlong(longlong *a,longlong *b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

static int cmp_double(double *a,double *b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

int in_vector::find(Item *item)
{
  byte *result=get_value(item);
  if (!result || !used_count)
    return 0;				// Null value

  uint start,end;
  start=0; end=used_count-1;
  while (start != end)
  {
    uint mid=(start+end+1)/2;
    int res;
    if ((res=(*compare)(base+mid*size,result)) == 0)
      return 1;
    if (res < 0)
      start=mid;
    else
      end=mid-1;
  }
  return (int) ((*compare)(base+start*size,result) == 0);
}


in_string::in_string(uint elements,qsort_cmp cmp_func)
  :in_vector(elements,sizeof(String),cmp_func),tmp(buff,sizeof(buff),default_charset_info)
{}

in_string::~in_string()
{
  for (uint i=0 ; i < count ; i++)
    ((String*) base)[i].free();
}

void in_string::set(uint pos,Item *item)
{
  String *str=((String*) base)+pos;
  String *res=item->val_str(str);
  if (res && res != str)
    *str= *res;
  // BAR TODO: I'm not sure this is absolutely correct
  if (!str->charset())
      str->set_charset(default_charset_info);
}

byte *in_string::get_value(Item *item)
{
  return (byte*) item->val_str(&tmp);
}


in_longlong::in_longlong(uint elements)
  :in_vector(elements,sizeof(longlong),(qsort_cmp) cmp_longlong)
{}

void in_longlong::set(uint pos,Item *item)
{
  ((longlong*) base)[pos]=item->val_int();
}

byte *in_longlong::get_value(Item *item)
{
  tmp=item->val_int();
  if (item->null_value)
    return 0;					/* purecov: inspected */
  return (byte*) &tmp;
}


in_double::in_double(uint elements)
  :in_vector(elements,sizeof(double),(qsort_cmp) cmp_double)
{}

void in_double::set(uint pos,Item *item)
{
  ((double*) base)[pos]=item->val();
}

byte *in_double::get_value(Item *item)
{
  tmp=item->val();
  if (item->null_value)
    return 0;					/* purecov: inspected */
  return (byte*) &tmp;
}


void Item_func_in::fix_length_and_dec()
{
  if (const_item())
  {
    switch (item->result_type()) {
    case STRING_RESULT:
      if (item->binary())
	array=new in_string(arg_count,(qsort_cmp) stringcmp); /* purecov: inspected */
      else
	array=new in_string(arg_count,(qsort_cmp) sortcmp);
      break;
    case INT_RESULT:
      array= new in_longlong(arg_count);
      break;
    case REAL_RESULT:
      array= new in_double(arg_count);
      break;
    case ROW_RESULT:
      // This case should never be choosen
      DBUG_ASSERT(0);
      break;
    }
    uint j=0;
    for (uint i=0 ; i < arg_count ; i++)
    {
      array->set(j,args[i]);
      if (!args[i]->null_value)			// Skip NULL values
	j++;
    }
    if ((array->used_count=j))
      array->sort();
  }
  else
  {
    switch (item->result_type()) {
    case STRING_RESULT:
      if (item->binary())
	in_item= new cmp_item_binary_string;
      else
	in_item= new cmp_item_sort_string;
      break;
    case INT_RESULT:
      in_item=	  new cmp_item_int;
      break;
    case REAL_RESULT:
      in_item=	  new cmp_item_real;
      break;
    case ROW_RESULT:
      // This case should never be choosen
      DBUG_ASSERT(0);
      break;
    }
  }
  maybe_null= item->maybe_null;
  max_length=2;
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


void Item_func_in::print(String *str)
{
  str->append('(');
  item->print(str);
  Item_func::print(str);
  str->append(')');
}


longlong Item_func_in::val_int()
{
  if (array)
  {
    int tmp=array->find(item);
    null_value=item->null_value;
    return tmp;
  }
  in_item->store_value(item);
  if ((null_value=item->null_value))
    return 0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (!in_item->cmp(args[i]) && !args[i]->null_value)
      return 1;					// Would maybe be nice with i ?
  }
  return 0;
}


void Item_func_in::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


longlong Item_func_bit_or::val_int()
{
  ulonglong arg1= (ulonglong) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  ulonglong arg2= (ulonglong) args[1]->val_int();
  if (args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (longlong) (arg1 | arg2);
}


longlong Item_func_bit_and::val_int()
{
  ulonglong arg1= (ulonglong) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  ulonglong arg2= (ulonglong) args[1]->val_int();
  if (args[1]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) (arg1 & arg2);
}


bool
Item_cond::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  List_iterator<Item> li(list);
  Item *item;
  char buff[sizeof(char*)];			// Max local vars in function
  used_tables_cache=0;
  const_item_cache=0;

  if (thd && check_stack_overrun(thd,buff))
    return 0;					// Fatal error flag is set!
  while ((item=li++))
  {
    while (item->type() == Item::COND_ITEM &&
	   ((Item_cond*) item)->functype() == functype())
    {						// Identical function
      li.replace(((Item_cond*) item)->list);
      ((Item_cond*) item)->list.empty();
#ifdef DELETE_ITEMS
      delete (Item_cond*) item;
#endif
      item= *li.ref();				// new current item
    }
    if (item->check_cols(1) || item->fix_fields(thd, tables, li.ref()))
      return 1; /* purecov: inspected */
    used_tables_cache|=item->used_tables();
    with_sum_func= with_sum_func || item->with_sum_func;
    const_item_cache&=item->const_item();
    if (item->maybe_null)
      maybe_null=1;
  }
  if (thd)
    thd->cond_count+=list.elements;
  fix_length_and_dec();
  return 0;
}

bool Item_cond::check_loop(uint id)
{
  DBUG_ENTER("Item_cond::check_loop");
  if (Item_func::check_loop(id))
    DBUG_RETURN(1);
  List_iterator<Item> li(list);
  Item *item;
  while ((item= li++))
  {
    if (item->check_loop(id))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

void Item_cond::split_sum_func(List<Item> &fields)
{
  List_iterator<Item> li(list);
  Item *item;
  used_tables_cache=0;
  const_item_cache=0;
  while ((item=li++))
  {
    if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
      item->split_sum_func(fields);
    else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
    {
      fields.push_front(item);
      li.replace(new Item_ref((Item**) fields.head_ref(),0,item->name));
    }
    item->update_used_tables();
    used_tables_cache|=item->used_tables();
    const_item_cache&=item->const_item();
  }
}


table_map
Item_cond::used_tables() const
{						// This caches used_tables
  return used_tables_cache;
}

void Item_cond::update_used_tables()
{
  used_tables_cache=0;
  const_item_cache=1;
  List_iterator_fast<Item> li(list);
  Item *item;
  while ((item=li++))
  {
    item->update_used_tables();
    used_tables_cache|=item->used_tables();
    const_item_cache&= item->const_item();
  }
}


void Item_cond::print(String *str)
{
  str->append('(');
  List_iterator_fast<Item> li(list);
  Item *item;
  if ((item=li++))
    item->print(str);
  while ((item=li++))
  {
    str->append(' ');
    str->append(func_name());
    str->append(' ');
    item->print(str);
  }
  str->append(')');
}


longlong Item_cond_and::val_int()
{
  List_iterator_fast<Item> li(list);
  Item *item;
  while ((item=li++))
  {
    if (item->val_int() == 0)
    {
      /*
	TODO: In case of NULL, ANSI would require us to continue evaluation
	until we get a FALSE value or run out of values; This would
	require a lot of unnecessary evaluation, which we skip for now
      */
      null_value=item->null_value;
      return 0;
    }
  }
  null_value=0;
  return 1;
}

longlong Item_cond_or::val_int()
{
  List_iterator_fast<Item> li(list);
  Item *item;
  null_value=0;
  while ((item=li++))
  {
    if (item->val_int() != 0)
    {
      null_value=0;
      return 1;
    }
    if (item->null_value)
      null_value=1;
  }
  return 0;
}

longlong Item_func_isnull::val_int()
{
  /*
    Handle optimization if the argument can't be null
    This has to be here because of the test in update_used_tables().
  */
  if (!used_tables_cache)
    return cached_value;
  return args[0]->is_null() ? 1: 0;
}

longlong Item_func_isnotnull::val_int()
{
  return args[0]->is_null() ? 0 : 1;
}


void Item_func_like::fix_length_and_dec()
{
  decimals=0; max_length=1;
  //  cmp_type=STRING_RESULT;			// For quick select
}

longlong Item_func_like::val_int()
{
  String* res = args[0]->val_str(&tmp_value1);
  if (args[0]->null_value)
  {
    null_value=1;
    return 0;
  }
  String* res2 = args[1]->val_str(&tmp_value2);
  if (args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if ((res->charset()->state & MY_CS_BINSORT) ||
      (res2->charset()->state & MY_CS_BINSORT))
    set_charset(my_charset_bin);
  if (canDoTurboBM)
    return turboBM_matches(res->ptr(), res->length()) ? 1 : 0;
  if (binary())
    return wild_compare(*res,*res2,escape) ? 0 : 1;
  else
    return wild_case_compare(*res,*res2,escape) ? 0 : 1;
}


/* We can optimize a where if first character isn't a wildcard */

Item_func::optimize_type Item_func_like::select_optimize() const
{
  if (args[1]->type() == STRING_ITEM)
  {
    if (((Item_string *) args[1])->str_value[0] != wild_many)
    {
      if ((args[0]->result_type() != STRING_RESULT) ||
	  ((Item_string *) args[1])->str_value[0] != wild_one)
	return OPTIMIZE_OP;
    }
  }
  return OPTIMIZE_NONE;
}

bool Item_func_like::fix_fields(THD *thd, TABLE_LIST *tlist, Item ** ref)
{
  if (Item_bool_func2::fix_fields(thd, tlist, ref))
    return 1;

  /*
    TODO--we could do it for non-const, but we'd have to
    recompute the tables for each row--probably not worth it.
  */
  if (args[1]->const_item() && !(specialflag & SPECIAL_NO_NEW_FUNC))
  {
    String* res2 = args[1]->val_str(&tmp_value2);
    if (!res2)
      return 0;					// Null argument

    const size_t len   = res2->length();
    const char*  first = res2->ptr();
    const char*  last  = first + len - 1;
    /*
      len must be > 2 ('%pattern%')
      heuristic: only do TurboBM for pattern_len > 2
    */

    if (len > MIN_TURBOBM_PATTERN_LEN + 2 &&
	*first == wild_many &&
	*last  == wild_many)
    {
      const char* tmp = first + 1;
      for (; *tmp != wild_many && *tmp != wild_one && *tmp != escape; tmp++) ;
      canDoTurboBM = tmp == last;
    }

    if (canDoTurboBM)
    {
      pattern     = first + 1;
      pattern_len = len - 2;
      DBUG_PRINT("info", ("Initializing pattern: '%s'", first));
      int *suff = (int*) thd->alloc(sizeof(int)*((pattern_len + 1)*2+
						 alphabet_size));
      bmGs      = suff + pattern_len + 1;
      bmBc      = bmGs + pattern_len + 1;
      turboBM_compute_good_suffix_shifts(suff);
      turboBM_compute_bad_character_shifts();
      DBUG_PRINT("info",("done"));
    }
  }
  return 0;
}

#ifdef USE_REGEX

bool
Item_func_regex::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  if (args[0]->check_cols(1) ||
      args[1]->check_cols(1) ||
      args[0]->fix_fields(thd, tables, args) ||
      args[1]->fix_fields(thd,tables, args + 1))
    return 1;					/* purecov: inspected */
  with_sum_func=args[0]->with_sum_func || args[1]->with_sum_func;
  max_length=1; decimals=0;
  if (args[0]->binary() || args[1]->binary())
    set_charset(my_charset_bin);

  used_tables_cache=args[0]->used_tables() | args[1]->used_tables();
  const_item_cache=args[0]->const_item() && args[1]->const_item();
  if (!regex_compiled && args[1]->const_item())
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff),default_charset_info);
    String *res=args[1]->val_str(&tmp);
    if (args[1]->null_value)
    {						// Will always return NULL
      maybe_null=1;
      return 0;
    }
    int error;
    if ((error=regcomp(&preg,res->c_ptr(),
		       binary() ? REG_EXTENDED | REG_NOSUB :
		       REG_EXTENDED | REG_NOSUB | REG_ICASE,
		       res->charset())))
    {
      (void) regerror(error,&preg,buff,sizeof(buff));
      my_printf_error(ER_REGEXP_ERROR,ER(ER_REGEXP_ERROR),MYF(0),buff);
      return 1;
    }
    regex_compiled=regex_is_const=1;
    maybe_null=args[0]->maybe_null;
  }
  else
    maybe_null=1;
  return 0;
}

longlong Item_func_regex::val_int()
{
  char buff[MAX_FIELD_WIDTH];
  String *res, tmp(buff,sizeof(buff),default_charset_info);

  res=args[0]->val_str(&tmp);
  if (args[0]->null_value)
  {
    null_value=1;
    return 0;
  }
  if (!regex_is_const)
  {
    char buff2[MAX_FIELD_WIDTH];
    String *res2, tmp2(buff2,sizeof(buff2),default_charset_info);

    res2= args[1]->val_str(&tmp2);
    if (args[1]->null_value)
    {
      null_value=1;
      return 0;
    }
    if (!regex_compiled || stringcmp(res2,&prev_regexp))
    {
      prev_regexp.copy(*res2);
      if (regex_compiled)
      {
	regfree(&preg);
	regex_compiled=0;
      }
      if (regcomp(&preg,res2->c_ptr(),
		  binary() ? REG_EXTENDED | REG_NOSUB :
		  REG_EXTENDED | REG_NOSUB | REG_ICASE,
		  res->charset()))

      {
	null_value=1;
	return 0;
      }
      regex_compiled=1;
    }
  }
  null_value=0;
  return regexec(&preg,res->c_ptr(),0,(regmatch_t*) 0,0) ? 0 : 1;
}


Item_func_regex::~Item_func_regex()
{
  if (regex_compiled)
  {
    regfree(&preg);
    regex_compiled=0;
  }
}

#endif /* USE_REGEX */


#ifdef LIKE_CMP_TOUPPER
#define likeconv(cs,A) (uchar) (cs)->toupper(A)
#else
#define likeconv(cs,A) (uchar) (cs)->sort_order[(uchar) (A)]
#endif


/**********************************************************************
  turboBM_compute_suffixes()
  Precomputation dependent only on pattern_len.
**********************************************************************/

void Item_func_like::turboBM_compute_suffixes(int* suff)
{
  const int   plm1 = pattern_len - 1;
  int            f = 0;
  int            g = plm1;
  int *const splm1 = suff + plm1;
  CHARSET_INFO	*cs=system_charset_info;	// QQ Needs to be fixed

  *splm1 = pattern_len;

  if (binary())
  {
    int i;
    for (i = pattern_len - 2; i >= 0; i--)
    {
      int tmp = *(splm1 + i - f);
      if (g < i && tmp < i - g)
	suff[i] = tmp;
      else
      {
	if (i < g)
	  g = i; // g = min(i, g)
	f = i;
	while (g >= 0 && pattern[g] == pattern[g + plm1 - f])
	  g--;
	suff[i] = f - g;
      }
    }
  }
  else
  {
    int i;
    for (i = pattern_len - 2; 0 <= i; --i)
    {
      int tmp = *(splm1 + i - f);
      if (g < i && tmp < i - g)
	suff[i] = tmp;
      else
      {
	if (i < g)
	  g = i; // g = min(i, g)
	f = i;
	while (g >= 0 && likeconv(cs, pattern[g]) ==
	       likeconv(cs, pattern[g + plm1 - f]))
	  g--;
	suff[i] = f - g;
      }
    }
  }
}


/**********************************************************************
   turboBM_compute_good_suffix_shifts()
   Precomputation dependent only on pattern_len.
**********************************************************************/

void Item_func_like::turboBM_compute_good_suffix_shifts(int* suff)
{
  turboBM_compute_suffixes(suff);

  int* end = bmGs + pattern_len;
  int* k;
  for (k = bmGs; k < end; k++)
    *k = pattern_len;

  int tmp;
  int i;
  int j          = 0;
  const int plm1 = pattern_len - 1;
  for (i = plm1; i > -1; i--)
  {
    if (suff[i] == i + 1)
    {
      for (tmp = plm1 - i; j < tmp; j++)
      {
	int* tmp2 = bmGs + j;
	if (*tmp2 == pattern_len)
	  *tmp2 = tmp;
      }
    }
  }

  int* tmp2;
  for (tmp = plm1 - i; j < tmp; j++)
  {
    tmp2 = bmGs + j;
    if (*tmp2 == pattern_len)
      *tmp2 = tmp;
  }

  tmp2 = bmGs + plm1;
  for (i = 0; i <= pattern_len - 2; i++)
    *(tmp2 - suff[i]) = plm1 - i;
}


/**********************************************************************
   turboBM_compute_bad_character_shifts()
   Precomputation dependent on pattern_len.
**********************************************************************/

void Item_func_like::turboBM_compute_bad_character_shifts()
{
  int *i;
  int *end = bmBc + alphabet_size;
  int j;
  const int plm1 = pattern_len - 1;
  CHARSET_INFO	*cs=system_charset_info;	// QQ Needs to be fixed

  for (i = bmBc; i < end; i++)
    *i = pattern_len;

  if (binary())
  {
    for (j = 0; j < plm1; j++)
      bmBc[pattern[j]] = plm1 - j;
  }
  else
  {
    for (j = 0; j < plm1; j++)
      bmBc[likeconv(cs,pattern[j])] = plm1 - j;
  }
}


/**********************************************************************
  turboBM_matches()
  Search for pattern in text, returns true/false for match/no match
**********************************************************************/

bool Item_func_like::turboBM_matches(const char* text, int text_len) const
{
  register int bcShift;
  register int turboShift;
  int shift = pattern_len;
  int j     = 0;
  int u     = 0;
  CHARSET_INFO	*cs=system_charset_info;	// QQ Needs to be fixed

  const int plm1  = pattern_len - 1;
  const int tlmpl =    text_len - pattern_len;

  /* Searching */
  if (binary())
  {
    while (j <= tlmpl)
    {
      register int i = plm1;
      while (i >= 0 && pattern[i] == text[i + j])
      {
	i--;
	if (i == plm1 - shift)
	  i -= u;
      }
      if (i < 0)
	return true;

      register const int v = plm1 - i;
      turboShift = u - v;
      bcShift    = bmBc[text[i + j]] - plm1 + i;
      shift      = max(turboShift, bcShift);
      shift      = max(shift, bmGs[i]);
      if (shift == bmGs[i])
	u = min(pattern_len - shift, v);
      else
      {
	if (turboShift < bcShift)
	  shift = max(shift, u + 1);
	u = 0;
      }
      j += shift;
    }
    return false;
  }
  else
  {
    while (j <= tlmpl)
    {
      register int i = plm1;
      while (i >= 0 && likeconv(cs,pattern[i]) == likeconv(cs,text[i + j]))
      {
	i--;
	if (i == plm1 - shift)
	  i -= u;
      }
      if (i < 0)
	return true;

      register const int v = plm1 - i;
      turboShift = u - v;
      bcShift    = bmBc[likeconv(cs, text[i + j])] - plm1 + i;
      shift      = max(turboShift, bcShift);
      shift      = max(shift, bmGs[i]);
      if (shift == bmGs[i])
	u = min(pattern_len - shift, v);
      else
      {
	if (turboShift < bcShift)
	  shift = max(shift, u + 1);
	u = 0;
      }
      j += shift;
    }
    return false;
  }
}


/*
  Make a logical XOR of the arguments.

  SYNOPSIS
    val_int()

  DESCRIPTION
  If either operator is NULL, return NULL.

  NOTE
    As we don't do any index optimization on XOR this is not going to be
    very fast to use.

  TODO (low priority)
    Change this to be optimized as:
      A XOR B   ->  (A) == 1 AND (B) <> 1) OR (A <> 1 AND (B) == 1)
    To be able to do this, we would however first have to extend the MySQL
    range optimizer to handle OR better.
*/

longlong Item_cond_xor::val_int()
{
  List_iterator<Item> li(list);
  Item *item;
  int result=0;	
  null_value=0;
  while ((item=li++))
  {
    result^= (item->val_int() != 0);
    if (item->null_value)
    {
      null_value=1;
      return 0;
    }
  }
  return (longlong) result;
}

/****************************************************************
 Classes and functions for spatial relations
*****************************************************************/

longlong Item_func_spatial_rel::val_int()
{
  String *res1=args[0]->val_str(&tmp_value1);
  String *res2=args[1]->val_str(&tmp_value2);
  Geometry g1, g2;
  MBR mbr1,mbr2;

  if ((null_value=(args[0]->null_value ||
                   args[1]->null_value ||
                   g1.create_from_wkb(res1->ptr(),res1->length()) || 
                   g2.create_from_wkb(res2->ptr(),res2->length()) ||
                   g1.get_mbr(&mbr1) || 
                   g2.get_mbr(&mbr2))))
   return 0;

  switch (spatial_rel)
  {
    case SP_CONTAINS_FUNC:
      return mbr1.contains(&mbr2);
    case SP_WITHIN_FUNC:
      return mbr1.within(&mbr2);
    case SP_EQUALS_FUNC:
      return mbr1.equals(&mbr2);
    case SP_DISJOINT_FUNC:
      return mbr1.disjoint(&mbr2);
    case SP_INTERSECTS_FUNC:
      return mbr1.intersects(&mbr2);
    case SP_TOUCHES_FUNC:
      return mbr1.touches(&mbr2);
    case SP_OVERLAPS_FUNC:
      return mbr1.overlaps(&mbr2);
    case SP_CROSSES_FUNC:
      return 0;
    default:
      break;
  }

  null_value=1;
  return 0;
}

longlong Item_func_isempty::val_int()
{
  String tmp; 
  null_value=0;
  return args[0]->null_value ? 1 : 0;
}

longlong Item_func_issimple::val_int()
{
  String tmp;
  String *wkb=args[0]->val_str(&tmp);

  if ((null_value= (!wkb || args[0]->null_value )))
    return 0;
  /* TODO: Ramil or Holyfoot, add real IsSimple calculation */
  return 0;
}

longlong Item_func_isclosed::val_int()
{
  String tmp;
  String *wkb=args[0]->val_str(&tmp);
  Geometry geom;
  int isclosed;

  null_value= (!wkb || 
               args[0]->null_value ||
               geom.create_from_wkb(wkb->ptr(),wkb->length()) ||
               !GEOM_METHOD_PRESENT(geom,is_closed) ||
               geom.is_closed(&isclosed));

  return (longlong) isclosed;
}
