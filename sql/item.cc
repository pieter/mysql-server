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
#include "my_dir.h"
#include "sp_rcontext.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "sql_select.h"

static void mark_as_dependent(THD *thd,
			      SELECT_LEX *last, SELECT_LEX *current,
			      Item_ident *item);

const String my_null_string("NULL", 4, default_charset_info);

/*****************************************************************************
** Item functions
*****************************************************************************/

/* Init all special items */

void item_init(void)
{
  item_user_lock_init();
}


/*
TODO: make this functions class dependent
*/

bool Item::val_bool()
{
  switch(result_type()) {
  case INT_RESULT:
    return val_int();
  case DECIMAL_RESULT:
  {
    my_decimal decimal_value;
    my_decimal *val= val_decimal(&decimal_value);
    if (val)
      return !my_decimal_is_zero(val);
    return 0;
  }
  case REAL_RESULT:
  case STRING_RESULT:
    return val_real() != 0.0;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
    return 0;                                   // Wrong (but safe)
  }
}


String *Item::val_string_from_real(String *str)
{
  double nr= val_real();
  if (null_value)
    return 0;					/* purecov: inspected */
  str->set(nr,decimals, &my_charset_bin);
  return str;
}


String *Item::val_string_from_int(String *str)
{
  longlong nr= val_int();
  if (null_value)
    return 0;
  if (unsigned_flag)
    str->set((ulonglong) nr, &my_charset_bin);
  else
    str->set(nr, &my_charset_bin);
  return str;
}


String *Item::val_string_from_decimal(String *str)
{
  my_decimal dec_buf, *dec= val_decimal(&dec_buf);
  if (null_value)
    return 0;
  my_decimal_round(E_DEC_FATAL_ERROR, dec, decimals, FALSE, &dec_buf);
  my_decimal2string(E_DEC_FATAL_ERROR, &dec_buf, 0, 0, 0, str);
  return str;
}


my_decimal *Item::val_decimal_from_real(my_decimal *decimal_value)
{
  double nr= val_real();
  if (null_value)
    return 0;
  double2my_decimal(E_DEC_FATAL_ERROR, nr, decimal_value);
  return (decimal_value);
}


my_decimal *Item::val_decimal_from_int(my_decimal *decimal_value)
{
  longlong nr= val_int();
  if (null_value)
    return 0;
  int2my_decimal(E_DEC_FATAL_ERROR, nr, unsigned_flag, decimal_value);
  return decimal_value;
}


my_decimal *Item::val_decimal_from_string(my_decimal *decimal_value)
{
  String *res;
  char *end_ptr;
  if (!(res= val_str(&str_value)))
    return 0;                                   // NULL or EOM

  end_ptr= (char*) res->ptr()+ res->length();
  str2my_decimal(E_DEC_FATAL_ERROR, res->ptr(), res->length(), res->charset(),
                 decimal_value);
  return decimal_value;
}


double Item::val_real_from_decimal()
{
  /* Note that fix_fields may not be called for Item_avg_field items */
  double result;
  my_decimal value_buff, *dec_val= val_decimal(&value_buff);
  if (null_value)
    return 0.0;
  my_decimal2double(E_DEC_FATAL_ERROR, dec_val, &result);
  return result;
}


longlong Item::val_int_from_decimal()
{
  /* Note that fix_fields may not be called for Item_avg_field items */
  longlong result;
  my_decimal value, *dec_val= val_decimal(&value);
  if (null_value)
    return 0;
  my_decimal2int(E_DEC_FATAL_ERROR, dec_val, unsigned_flag, &result);
  return result;
}


Item::Item():
  name(0), orig_name(0), name_length(0), fixed(0),
  collation(&my_charset_bin, DERIVATION_COERCIBLE)
{
  marker= 0;
  maybe_null=null_value=with_sum_func=unsigned_flag=0;
  decimals= 0; max_length= 0;

  /* Put item in free list so that we can free all items at end */
  THD *thd= current_thd;
  next= thd->free_list;
  thd->free_list= this;
  /*
    Item constructor can be called during execution other then SQL_COM
    command => we should check thd->lex->current_select on zero (thd->lex
    can be uninitialised)
  */
  if (thd->lex->current_select)
  {
    enum_parsing_place place= 
      thd->lex->current_select->parsing_place;
    if (place == SELECT_LIST ||
	place == IN_HAVING)
      thd->lex->current_select->select_n_having_items++;
  }
}

/*
  Constructor used by Item_field, Item_*_ref & aggregate (sum) functions.
  Used for duplicating lists in processing queries with temporary
  tables
*/
Item::Item(THD *thd, Item *item):
  str_value(item->str_value),
  name(item->name),
  orig_name(item->orig_name),
  max_length(item->max_length),
  marker(item->marker),
  decimals(item->decimals),
  maybe_null(item->maybe_null),
  null_value(item->null_value),
  unsigned_flag(item->unsigned_flag),
  with_sum_func(item->with_sum_func),
  fixed(item->fixed),
  collation(item->collation)
{
  next= thd->free_list;				// Put in free list
  thd->free_list= this;
}


void Item::print_item_w_name(String *str)
{
  print(str);
  if (name)
  {
    THD *thd= current_thd;
    str->append(" AS ", 4);
    append_identifier(thd, str, name, strlen(name));
  }
}


void Item::cleanup()
{
  DBUG_ENTER("Item::cleanup");
  DBUG_PRINT("info", ("Item: 0x%lx, Type: %d, name %s, original name %s",
		      this, (int)type(), name ? name : "(null)",
                      orig_name ? orig_name : "null"));
  fixed=0;
  marker= 0;
  if (orig_name)
    name= orig_name;
  DBUG_VOID_RETURN;
}


/*
  cleanup() item if it is 'fixed'

  SYNOPSIS
    cleanup_processor()
    arg - a dummy parameter, is not used here
*/

bool Item::cleanup_processor(byte *arg)
{
  if (fixed)
    cleanup();
  return FALSE;
}


/*
  rename item (used for views, cleanup() return original name)

  SYNOPSIS
    Item::rename()
    new_name	new name of item;
*/

void Item::rename(char *new_name)
{
  /*
    we can compare pointers to names here, because if name was not changed,
    pointer will be same
  */
  if (!orig_name && new_name != name)
    orig_name= name;
  name= new_name;
}


Item_ident::Item_ident(const char *db_name_par,const char *table_name_par,
		       const char *field_name_par)
  :orig_db_name(db_name_par), orig_table_name(table_name_par), 
   orig_field_name(field_name_par),
   db_name(db_name_par), table_name(table_name_par),
   field_name(field_name_par),
   alias_name_used(FALSE), cached_field_index(NO_CACHED_FIELD_INDEX),
   cached_table(0), depended_from(0)
{
  name = (char*) field_name_par;
}


/* Constructor used by Item_field & Item_*_ref (see Item comment) */

Item_ident::Item_ident(THD *thd, Item_ident *item)
  :Item(thd, item),
   orig_db_name(item->orig_db_name),
   orig_table_name(item->orig_table_name), 
   orig_field_name(item->orig_field_name),
   db_name(item->db_name),
   table_name(item->table_name),
   field_name(item->field_name),
   alias_name_used(item->alias_name_used),
   cached_field_index(item->cached_field_index),
   cached_table(item->cached_table),
   depended_from(item->depended_from)
{}

void Item_ident::cleanup()
{
  DBUG_ENTER("Item_ident::cleanup");
#ifdef CANT_BE_USED_AS_MEMORY_IS_FREED
		       db_name ? db_name : "(null)",
                       orig_db_name ? orig_db_name : "(null)",
		       table_name ? table_name : "(null)",
                       orig_table_name ? orig_table_name : "(null)",
		       field_name ? field_name : "(null)",
                       orig_field_name ? orig_field_name : "(null)"));
#endif
  Item::cleanup();
  db_name= orig_db_name; 
  table_name= orig_table_name;
  field_name= orig_field_name;
  DBUG_VOID_RETURN;
}

bool Item_ident::remove_dependence_processor(byte * arg)
{
  DBUG_ENTER("Item_ident::remove_dependence_processor");
  if (depended_from == (st_select_lex *) arg)
    depended_from= 0;
  DBUG_RETURN(0);
}


/*
  Store the pointer to this item field into a list if not already there.

  SYNOPSIS
    Item_field::collect_item_field_processor()
    arg  pointer to a List<Item_field>

  DESCRIPTION
    The method is used by Item::walk to collect all unique Item_field objects
    from a tree of Items into a set of items represented as a list.

  IMPLEMENTATION
    Item_cond::walk() and Item_func::walk() stop the evaluation of the
    processor function for its arguments once the processor returns
    true.Therefore in order to force this method being called for all item
    arguments in a condition the method must return false.

  RETURN
    false to force the evaluation of collect_item_field_processor
          for the subsequent items.
*/

bool Item_field::collect_item_field_processor(byte *arg)
{
  DBUG_ENTER("Item_field::collect_item_field_processor");
  DBUG_PRINT("info", ("%s", field->field_name ? field->field_name : "noname"));
  List<Item_field> *item_list= (List<Item_field>*) arg;
  List_iterator<Item_field> item_list_it(*item_list);
  Item_field *curr_item;
  while ((curr_item= item_list_it++))
  {
    if (curr_item->eq(this, 1))
      DBUG_RETURN(false); /* Already in the set. */
  }
  item_list->push_back(this);
  DBUG_RETURN(false);
}


bool Item::check_cols(uint c)
{
  if (c != 1)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return 1;
  }
  return 0;
}


void Item::set_name(const char *str, uint length, CHARSET_INFO *cs)
{
  if (!length)
  {
    /* Empty string, used by AS or internal function like last_insert_id() */
    name= (char*) str;
    name_length= 0;
    return;
  }
  if (cs->ctype)
  {
    /*
      This will probably need a better implementation in the future:
      a function in CHARSET_INFO structure.
    */
    while (length && !my_isgraph(cs,*str))
    {						// Fix problem with yacc
      length--;
      str++;
    }
  }
  if (!my_charset_same(cs, system_charset_info))
  {
    uint32 res_length;
    name= sql_strmake_with_convert(str, name_length= length, cs,
				   MAX_ALIAS_NAME, system_charset_info,
				   &res_length);
  }
  else
    name= sql_strmake(str, (name_length= min(length,MAX_ALIAS_NAME)));
}


/*
  This function is called when:
  - Comparing items in the WHERE clause (when doing where optimization)
  - When trying to find an ORDER BY/GROUP BY item in the SELECT part
*/

bool Item::eq(const Item *item, bool binary_cmp) const
{
  return type() == item->type() && name && item->name &&
    !my_strcasecmp(system_charset_info,name,item->name);
}


Item *Item::safe_charset_converter(CHARSET_INFO *tocs)
{
  /*
    Don't allow automatic conversion to non-Unicode charsets,
    as it potentially loses data.
  */
  if (!(tocs->state & MY_CS_UNICODE))
    return NULL; // safe conversion is not possible
  return new Item_func_conv_charset(this, tocs);
}


Item *Item_string::safe_charset_converter(CHARSET_INFO *tocs)
{
  Item_string *conv;
  uint conv_errors;
  String tmp, cstr, *ostr= val_str(&tmp);
  cstr.copy(ostr->ptr(), ostr->length(), ostr->charset(), tocs, &conv_errors);
  if (conv_errors || !(conv= new Item_string(cstr.ptr(), cstr.length(),
                                             cstr.charset(),
                                             collation.derivation)))
  {
    /*
      Safe conversion is not possible (or EOM).
      We could not convert a string into the requested character set
      without data loss. The target charset does not cover all the
      characters from the string. Operation cannot be done correctly.
    */
    return NULL;
  }
  conv->str_value.copy();
  return conv;
}


bool Item_string::eq(const Item *item, bool binary_cmp) const
{
  if (type() == item->type())
  {
    if (binary_cmp)
      return !stringcmp(&str_value, &item->str_value);
    return !sortcmp(&str_value, &item->str_value, collation.collation);
  }
  return 0;
}


/*
  Get the value of the function as a TIME structure.
  As a extra convenience the time structure is reset on error!
 */

bool Item::get_date(TIME *ltime,uint fuzzydate)
{
  char buff[40];
  String tmp(buff,sizeof(buff), &my_charset_bin),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_datetime_with_warn(res->ptr(), res->length(),
                                ltime, fuzzydate) <= MYSQL_TIMESTAMP_ERROR)
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

/*
  Get time of first argument.
  As a extra convenience the time structure is reset on error!
 */

bool Item::get_time(TIME *ltime)
{
  char buff[40];
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_time_with_warn(res->ptr(), res->length(), ltime))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

CHARSET_INFO *Item::default_charset()
{
  return current_thd->variables.collation_connection;
}


int Item::save_in_field_no_warnings(Field *field, bool no_conversions)
{
  int res;
  THD *thd= field->table->in_use;
  enum_check_fields tmp= thd->count_cuted_fields;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  res= save_in_field(field, no_conversions);
  thd->count_cuted_fields= tmp;
  return res;
}


double Item_splocal::val_real()
{
  DBUG_ASSERT(fixed);
  Item *it= this_item();
  double ret= it->val_real();
  Item::null_value= it->null_value;
  return ret;
}


longlong Item_splocal::val_int()
{
  DBUG_ASSERT(fixed);
  Item *it= this_item();
  longlong ret= it->val_int();
  Item::null_value= it->null_value;
  return ret;
}


String *Item_splocal::val_str(String *sp)
{
  DBUG_ASSERT(fixed);
  Item *it= this_item();
  String *ret= it->val_str(sp);
  Item::null_value= it->null_value;
  return ret;
}


my_decimal *Item_splocal::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed);
  Item *it= this_item();
  my_decimal value, *val= it->val_decimal(&value);
  Item::null_value= it->null_value;
  return val;
}


bool Item_splocal::is_null()
{
  Item *it= this_item();
  bool ret= it->is_null();
  Item::null_value= it->null_value;
  return ret;
}


Item *
Item_splocal::this_item()
{
  THD *thd= current_thd;

  return thd->spcont->get_item(m_offset);
}

Item *
Item_splocal::this_const_item() const
{
  THD *thd= current_thd;

  return thd->spcont->get_item(m_offset);
}

Item::Type
Item_splocal::type() const
{
  THD *thd= current_thd;

  if (thd->spcont)
    return thd->spcont->get_item(m_offset)->type();
  return NULL_ITEM;		// Anything but SUBSELECT_ITEM
}


bool Item_splocal::fix_fields(THD *, struct st_table_list *, Item **)
{
  Item *it= this_item();
  DBUG_ASSERT(it->fixed);
  max_length= it->max_length;
  decimals= it->decimals;
  fixed= 1;
  return FALSE;
}


void Item_splocal::cleanup()
{
  fixed= 0;
}


void Item_splocal::print(String *str)
{
  str->reserve(m_name.length+8);
  str->append(m_name.str, m_name.length);
  str->append('@');
  str->qs_append(m_offset);
}



/*
  Move SUM items out from item tree and replace with reference

  SYNOPSIS
    split_sum_func2()
    thd			Thread handler
    ref_pointer_array	Pointer to array of reference fields
    fields		All fields in select
    ref			Pointer to item

  NOTES
   This is from split_sum_func2() for items that should be split

   All found SUM items are added FIRST in the fields list and
   we replace the item with a reference.

   thd->fatal_error() may be called if we are out of memory
*/


void Item::split_sum_func2(THD *thd, Item **ref_pointer_array,
                           List<Item> &fields, Item **ref)
{
  if (type() != SUM_FUNC_ITEM && with_sum_func)
  {
    /* Will split complicated items and ignore simple ones */
    split_sum_func(thd, ref_pointer_array, fields);
  }
  else if ((type() == SUM_FUNC_ITEM ||
            (used_tables() & ~PARAM_TABLE_BIT)) &&
           type() != REF_ITEM)
  {
    /*
      Replace item with a reference so that we can easily calculate
      it (in case of sum functions) or copy it (in case of fields)

      The test above is to ensure we don't do a reference for things
      that are constants (PARAM_TABLE_BIT is in effect a constant)
      or already referenced (for example an item in HAVING)
    */
    uint el= fields.elements;
    Item *new_item;    
    ref_pointer_array[el]= this;
    if (!(new_item= new Item_ref(ref_pointer_array + el, 0, name)))
      return;                                   // fatal_error is set
    fields.push_front(this);
    ref_pointer_array[el]= this;
    thd->change_item_tree(ref, new_item);
  }
}


/*
   Aggregate two collations together taking
   into account their coercibility (aka derivation):

   0 == DERIVATION_EXPLICIT  - an explicitly written COLLATE clause
   1 == DERIVATION_NONE      - a mix of two different collations
   2 == DERIVATION_IMPLICIT  - a column
   3 == DERIVATION_COERCIBLE - a string constant

   The most important rules are:

   1. If collations are the same:
      chose this collation, and the strongest derivation.

   2. If collations are different:
     - Character sets may differ, but only if conversion without
       data loss is possible. The caller provides flags whether
       character set conversion attempts should be done. If no
       flags are substituted, then the character sets must be the same.
       Currently processed flags are:
         MY_COLL_ALLOW_SUPERSET_CONV  - allow conversion to a superset
         MY_COLL_ALLOW_COERCIBLE_CONV - allow conversion of a coercible value
     - two EXPLICIT collations produce an error, e.g. this is wrong:
       CONCAT(expr1 collate latin1_swedish_ci, expr2 collate latin1_german_ci)
     - the side with smaller derivation value wins,
       i.e. a column is stronger than a string constant,
       an explicit COLLATE clause is stronger than a column.
     - if derivations are the same, we have DERIVATION_NONE,
       we'll wait for an explicit COLLATE clause which possibly can
       come from another argument later: for example, this is valid,
       but we don't know yet when collecting the first two arguments:
         CONCAT(latin1_swedish_ci_column,
                latin1_german1_ci_column,
                expr COLLATE latin1_german2_ci)
*/
bool DTCollation::aggregate(DTCollation &dt, uint flags)
{
  if (!my_charset_same(collation, dt.collation))
  {
    /* 
       We do allow to use binary strings (like BLOBS)
       together with character strings.
       Binaries have more precedence than a character
       string of the same derivation.
    */
    if (collation == &my_charset_bin)
    {
      if (derivation <= dt.derivation)
	; // Do nothing
      else
      {
	set(dt); 
      }
    }
    else if (dt.collation == &my_charset_bin)
    {
      if (dt.derivation <= derivation)
      {
        set(dt);
      }
      else
       ; // Do nothing
    }
    else if ((flags & MY_COLL_ALLOW_SUPERSET_CONV) &&
             derivation < dt.derivation &&
             collation->state & MY_CS_UNICODE)
    {
      // Do nothing
    }
    else if ((flags & MY_COLL_ALLOW_SUPERSET_CONV) &&
             dt.derivation < derivation &&
             dt.collation->state & MY_CS_UNICODE)
    {
      set(dt);
    }
    else if ((flags & MY_COLL_ALLOW_COERCIBLE_CONV) &&
             derivation < dt.derivation &&
             dt.derivation >= DERIVATION_SYSCONST)
    {
      // Do nothing;
    }
    else if ((flags & MY_COLL_ALLOW_COERCIBLE_CONV) &&
             dt.derivation < derivation &&
             derivation >= DERIVATION_SYSCONST)
    {
      set(dt);
    }
    else
    {
      // Cannot apply conversion
      set(0, DERIVATION_NONE);
      return 1;
    }
  }
  else if (derivation < dt.derivation)
  {
    // Do nothing
  }
  else if (dt.derivation < derivation)
  {
    set(dt);
  }
  else
  { 
    if (collation == dt.collation)
    {
      // Do nothing
    }
    else 
    {
      if (derivation == DERIVATION_EXPLICIT)
      {
	set(0, DERIVATION_NONE);
	return 1;
      }
      CHARSET_INFO *bin= get_charset_by_csname(collation->csname, 
					       MY_CS_BINSORT,MYF(0));
      set(bin, DERIVATION_NONE);
    }
  }
  return 0;
}

Item_field::Item_field(Field *f)
  :Item_ident(NullS, *f->table_name, f->field_name),
  item_equal(0), no_const_subst(0),
   have_privileges(0), any_privileges(0)
{
  set_field(f);
  /*
    field_name and talbe_name should not point to garbage
    if this item is to be reused
  */
  orig_table_name= orig_field_name= "";
}

Item_field::Item_field(THD *thd, Field *f)
  :Item_ident(f->table->s->db, *f->table_name, f->field_name),
   item_equal(0), no_const_subst(0),
   have_privileges(0), any_privileges(0)
{
  /*
    We always need to provide Item_field with a fully qualified field
    name to avoid ambiguity when executing prepared statements like
    SELECT * from d1.t1, d2.t1; (assuming d1.t1 and d2.t1 have columns
    with same names).
    This is because prepared statements never deal with wildcards in
    select list ('*') and always fix fields using fully specified path
    (i.e. db.table.column).
    No check for OOM: if db_name is NULL, we'll just get
    "Field not found" error.
    We need to copy db_name, table_name and field_name because they must
    be allocated in the statement memory, not in table memory (the table
    structure can go away and pop up again between subsequent executions
    of a prepared statement).
  */
  if (thd->current_arena->is_stmt_prepare())
  {
    if (db_name)
      orig_db_name= thd->strdup(db_name);
    orig_table_name= thd->strdup(table_name);
    orig_field_name= thd->strdup(field_name);
    /*
      We don't restore 'name' in cleanup because it's not changed
      during execution. Still we need it to point to persistent
      memory if this item is to be reused.
    */
    name= (char*) orig_field_name;
  }
  set_field(f);
}

// Constructor need to process subselect with temporary tables (see Item)
Item_field::Item_field(THD *thd, Item_field *item)
  :Item_ident(thd, item),
   field(item->field),
   result_field(item->result_field),
   item_equal(item->item_equal),
   no_const_subst(item->no_const_subst),
   have_privileges(item->have_privileges),
   any_privileges(item->any_privileges)
{
  collation.set(DERIVATION_IMPLICIT);
}

void Item_field::set_field(Field *field_par)
{
  field=result_field=field_par;			// for easy coding with fields
  maybe_null=field->maybe_null();
  decimals= field->decimals();
  max_length= field_par->representation_length();
  table_name= *field_par->table_name;
  field_name= field_par->field_name;
  db_name= field_par->table->s->db;
  alias_name_used= field_par->table->alias_name_used;
  unsigned_flag=test(field_par->flags & UNSIGNED_FLAG);
  collation.set(field_par->charset(), DERIVATION_IMPLICIT);
  fixed= 1;
}


/*
  Reset this item to point to a field from the new temporary table.
  This is used when we create a new temporary table for each execution
  of prepared statement.
*/

void Item_field::reset_field(Field *f)
{
  set_field(f);
  /* 'name' is pointing at field->field_name of old field */
  name= (char*) f->field_name;
}

const char *Item_ident::full_name() const
{
  char *tmp;
  if (!table_name || !field_name)
    return field_name ? field_name : name ? name : "tmp_field";
  if (db_name && db_name[0])
  {
    tmp=(char*) sql_alloc((uint) strlen(db_name)+(uint) strlen(table_name)+
			  (uint) strlen(field_name)+3);
    strxmov(tmp,db_name,".",table_name,".",field_name,NullS);
  }
  else
  {
    if (table_name[0])
    {
      tmp= (char*) sql_alloc((uint) strlen(table_name) +
			     (uint) strlen(field_name) + 2);
      strxmov(tmp, table_name, ".", field_name, NullS);
    }
    else
      tmp= (char*) field_name;
  }
  return tmp;
}

void Item_ident::print(String *str)
{
  THD *thd= current_thd;
  char d_name_buff[MAX_ALIAS_NAME], t_name_buff[MAX_ALIAS_NAME];
  const char *d_name= db_name, *t_name= table_name;
  if (lower_case_table_names== 1 ||
      (lower_case_table_names == 2 && !alias_name_used))
  {
    if (table_name && table_name[0])
    {
      strmov(t_name_buff, table_name);
      my_casedn_str(files_charset_info, t_name_buff);
      t_name= t_name_buff;
    }
    if (db_name && db_name[0])
    {
      strmov(d_name_buff, db_name);
      my_casedn_str(files_charset_info, d_name_buff);
      d_name= d_name_buff;
    }
  }

  if (!table_name || !field_name)
  {
    const char *nm= field_name ? field_name : name ? name : "tmp_field";
    append_identifier(thd, str, nm, strlen(nm));
    return;
  }
  if (db_name && db_name[0] && !alias_name_used)
  {
    append_identifier(thd, str, d_name, strlen(d_name));
    str->append('.');
    append_identifier(thd, str, t_name, strlen(t_name));
    str->append('.');
    append_identifier(thd, str, field_name, strlen(field_name));
  }
  else
  {
    if (table_name[0])
    {
      append_identifier(thd, str, t_name, strlen(t_name));
      str->append('.');
      append_identifier(thd, str, field_name, strlen(field_name));
    }
    else
      append_identifier(thd, str, field_name, strlen(field_name));
  }
}

/* ARGSUSED */
String *Item_field::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if ((null_value=field->is_null()))
    return 0;
  str->set_charset(str_value.charset());
  return field->val_str(str,&str_value);
}


double Item_field::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if ((null_value=field->is_null()))
    return 0.0;
  return field->val_real();
}


longlong Item_field::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if ((null_value=field->is_null()))
    return 0;
  return field->val_int();
}


my_decimal *Item_field::val_decimal(my_decimal *decimal_value)
{
  if ((null_value= field->is_null()))
    return 0;
  return field->val_decimal(decimal_value);
}


String *Item_field::str_result(String *str)
{
  if ((null_value=result_field->is_null()))
    return 0;
  str->set_charset(str_value.charset());
  return result_field->val_str(str,&str_value);
}

bool Item_field::get_date(TIME *ltime,uint fuzzydate)
{
  if ((null_value=field->is_null()) || field->get_date(ltime,fuzzydate))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

bool Item_field::get_date_result(TIME *ltime,uint fuzzydate)
{
  if ((null_value=result_field->is_null()) ||
      result_field->get_date(ltime,fuzzydate))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

bool Item_field::get_time(TIME *ltime)
{
  if ((null_value=field->is_null()) || field->get_time(ltime))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

double Item_field::val_result()
{
  if ((null_value=result_field->is_null()))
    return 0.0;
  return result_field->val_real();
}

longlong Item_field::val_int_result()
{
  if ((null_value=result_field->is_null()))
    return 0;
  return result_field->val_int();
}


my_decimal *Item_field::val_decimal_result(my_decimal *decimal_value)
{
  if ((null_value= result_field->is_null()))
    return 0;
  return result_field->val_decimal(decimal_value);
}


bool Item_field::val_bool_result()
{
  if ((null_value= result_field->is_null()))
    return FALSE;
  switch (result_field->result_type()) {
  case INT_RESULT:
    return result_field->val_int();
  case DECIMAL_RESULT:
  {
    my_decimal decimal_value;
    my_decimal *val= result_field->val_decimal(&decimal_value);
    if (val)
      return !my_decimal_is_zero(val);
    return 0;
  }
  case REAL_RESULT:
  case STRING_RESULT:
    return result_field->val_real() != 0.0;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
    return 0;                                   // Shut up compiler
  }
}


bool Item_field::eq(const Item *item, bool binary_cmp) const
{
  if (item->type() != FIELD_ITEM)
    return 0;
  
  Item_field *item_field= (Item_field*) item;
  if (item_field->field)
    return item_field->field == field;
  /*
    We may come here when we are trying to find a function in a GROUP BY
    clause from the select list.
    In this case the '100 % correct' way to do this would be to first
    run fix_fields() on the GROUP BY item and then retry this function, but
    I think it's better to relax the checking a bit as we will in
    most cases do the correct thing by just checking the field name.
    (In cases where we would choose wrong we would have to generate a
    ER_NON_UNIQ_ERROR).
  */
  return (!my_strcasecmp(system_charset_info, item_field->name,
			 field_name) &&
	  (!item_field->table_name ||
	   (!my_strcasecmp(table_alias_charset, item_field->table_name,
			   table_name) &&
	    (!item_field->db_name ||
	     (item_field->db_name && !strcmp(item_field->db_name,
					     db_name))))));
}


table_map Item_field::used_tables() const
{
  if (field->table->const_table)
    return 0;					// const item
  return (depended_from ? OUTER_REF_TABLE_BIT : field->table->map);
}


Item *Item_field::get_tmp_table_item(THD *thd)
{
  Item_field *new_item= new Item_field(thd, this);
  if (new_item)
    new_item->field= new_item->result_field;
  return new_item;
}


/*
  Create an item from a string we KNOW points to a valid longlong
  end \0 terminated number string.
  This is always 'signed'. Unsigned values are created with Item_uint()
*/

Item_int::Item_int(const char *str_arg, uint length)
{
  char *end_ptr= (char*) str_arg + length;
  int error;
  value= my_strtoll10(str_arg, &end_ptr, &error);
  max_length= (uint) (end_ptr - str_arg);
  name= (char*) str_arg;
  fixed= 1;
}


my_decimal *Item_int::val_decimal(my_decimal *decimal_value)
{
  int2my_decimal(E_DEC_FATAL_ERROR, value, unsigned_flag, decimal_value);
  return decimal_value;
}

String *Item_int::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  str->set(value, &my_charset_bin);
  return str;
}

void Item_int::print(String *str)
{
  // my_charset_bin is good enough for numbers
  str_value.set(value, &my_charset_bin);
  str->append(str_value);
}


Item_uint::Item_uint(const char *str_arg, uint length):
  Item_int(str_arg, length)
{
  unsigned_flag= 1;
}


String *Item_uint::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  str->set((ulonglong) value, &my_charset_bin);
  return str;
}


void Item_uint::print(String *str)
{
  // latin1 is good enough for numbers
  str_value.set((ulonglong) value, default_charset());
  str->append(str_value);
}


Item_decimal::Item_decimal(const char *str_arg, uint length,
                           CHARSET_INFO *charset)
{
  str2my_decimal(E_DEC_FATAL_ERROR, str_arg, length, charset, &decimal_value);
  name= (char*) str_arg;
  decimals= (uint8) decimal_value.frac;
  max_length= my_decimal_max_length(&decimal_value);
  fixed= 1;
  unsigned_flag= !decimal_value.sign();
}

Item_decimal::Item_decimal(longlong val, bool unsig)
{
  int2my_decimal(E_DEC_FATAL_ERROR, val, unsig, &decimal_value);
  decimals= (uint8) decimal_value.frac;
  max_length= my_decimal_max_length(&decimal_value);
  fixed= 1;
  unsigned_flag= !decimal_value.sign();
}


Item_decimal::Item_decimal(double val, int precision, int scale)
{
  double2my_decimal(E_DEC_FATAL_ERROR, val, &decimal_value);
  decimals= (uint8) decimal_value.frac;
  max_length= my_decimal_max_length(&decimal_value);
  fixed= 1;
  unsigned_flag= !decimal_value.sign();
}


Item_decimal::Item_decimal(const char *str, const my_decimal *val_arg,
                           uint decimal_par, uint length)
{
  my_decimal2decimal(val_arg, &decimal_value);
  name= (char*) str;
  decimals= (uint8) decimal_par;
  max_length= length;
  fixed= 1;
  unsigned_flag= !decimal_value.sign();
}


Item_decimal::Item_decimal(my_decimal *value_par)
{
  my_decimal2decimal(value_par, &decimal_value);
  decimals= (uint8) decimal_value.frac;
  max_length= my_decimal_max_length(value_par);
  fixed= 1;
  unsigned_flag= !decimal_value.sign();
}


Item_decimal::Item_decimal(const char *bin, int precision, int scale)
{
  binary2my_decimal(E_DEC_FATAL_ERROR, bin, &decimal_value, precision, scale);
  decimals= (uint8) decimal_value.frac;
  max_length= my_decimal_max_length(&decimal_value);
  fixed= 1;
  unsigned_flag= !decimal_value.sign();
}


longlong Item_decimal::val_int()
{
  longlong result;
  my_decimal2int(E_DEC_FATAL_ERROR, &decimal_value, unsigned_flag, &result);
  return result;
}

double Item_decimal::val_real()
{
  double result;
  my_decimal2double(E_DEC_FATAL_ERROR, &decimal_value, &result);
  return result;
}

String *Item_decimal::val_str(String *result)
{
  result->set_charset(&my_charset_bin);
  my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value, 0, 0, 0, result);
  return result;
}

void Item_decimal::print(String *str)
{
  my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value, 0, 0, 0, &str_value);
  str->append(str_value);
}


String *Item_float::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  str->set(value,decimals,&my_charset_bin);
  return str;
}


my_decimal *Item_float::val_decimal(my_decimal *decimal_value)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  double2my_decimal(E_DEC_FATAL_ERROR, value, decimal_value);
  return (decimal_value);
}


void Item_string::print(String *str)
{
  str->append('_');
  str->append(collation.collation->csname);
  str->append('\'');
  str_value.print(str);
  str->append('\'');
}


my_decimal *Item_string::val_decimal(my_decimal *decimal_value)
{
  /* following assert is redundant, because fixed=1 assigned in constructor */
  DBUG_ASSERT(fixed == 1);
  string2my_decimal(E_DEC_FATAL_ERROR, &str_value, decimal_value);
  return (decimal_value);
}


bool Item_null::eq(const Item *item, bool binary_cmp) const
{ return item->type() == type(); }


double Item_null::val_real()
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  null_value=1;
  return 0.0;
}
longlong Item_null::val_int()
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  null_value=1;
  return 0;
}
/* ARGSUSED */
String *Item_null::val_str(String *str)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  null_value=1;
  return 0;
}

my_decimal *Item_null::val_decimal(my_decimal *decimal_value)
{
  return 0;
}


Item *Item_null::safe_charset_converter(CHARSET_INFO *tocs)
{
  collation.set(tocs);
  return this;
}

/*********************** Item_param related ******************************/

/* 
  Default function of Item_param::set_param_func, so in case
  of malformed packet the server won't SIGSEGV
*/

static void
default_set_param_func(Item_param *param,
                       uchar **pos __attribute__((unused)),
                       ulong len __attribute__((unused)))
{
  param->set_null();
}


Item_param::Item_param(unsigned pos_in_query_arg) :
  state(NO_VALUE),
  item_result_type(STRING_RESULT),
  /* Don't pretend to be a literal unless value for this item is set. */
  item_type(PARAM_ITEM),
  param_type(MYSQL_TYPE_VARCHAR),
  pos_in_query(pos_in_query_arg),
  set_param_func(default_set_param_func)
{
  name= (char*) "?";
  /* 
    Since we can't say whenever this item can be NULL or cannot be NULL
    before mysql_stmt_execute(), so we assuming that it can be NULL until
    value is set.
  */
  maybe_null= 1;
}


void Item_param::set_null()
{
  DBUG_ENTER("Item_param::set_null");
  /* These are cleared after each execution by reset() method */
  null_value= 1;
  /* 
    Because of NULL and string values we need to set max_length for each new
    placeholder value: user can submit NULL for any placeholder type, and 
    string length can be different in each execution.
  */
  max_length= 0;
  decimals= 0;
  state= NULL_VALUE;
  DBUG_VOID_RETURN;
}

void Item_param::set_int(longlong i, uint32 max_length_arg)
{
  DBUG_ENTER("Item_param::set_int");
  value.integer= (longlong) i;
  state= INT_VALUE;
  max_length= max_length_arg;
  decimals= 0;
  maybe_null= 0;
  DBUG_VOID_RETURN;
}

void Item_param::set_double(double d)
{
  DBUG_ENTER("Item_param::set_double");
  value.real= d;
  state= REAL_VALUE;
  max_length= DBL_DIG + 8;
  decimals= NOT_FIXED_DEC;
  maybe_null= 0;
  DBUG_VOID_RETURN;
}


/*
  Set decimal parameter value from string.

  SYNOPSIS
    set_decimal()
      str    - character string
      length - string length

  NOTE
    as we use character strings to send decimal values in
    binary protocol, we use str2my_decimal to convert it to
    internal decimal value.
*/

void Item_param::set_decimal(const char *str, ulong length)
{
  char *end;
  DBUG_ENTER("Item_param::set_decimal");

  end= (char*) str+length;
  str2my_decimal(E_DEC_FATAL_ERROR, str, &decimal_value, &end);
  state= DECIMAL_VALUE;
  decimals= decimal_value.frac;
  max_length= decimal_value.intg + decimals + 2;
  maybe_null= 0;
  DBUG_VOID_RETURN;
}


/*
  Set parameter value from TIME value.

  SYNOPSIS
    set_time()
      tm             - datetime value to set (time_type is ignored)
      type           - type of datetime value
      max_length_arg - max length of datetime value as string

  NOTE
    If we value to be stored is not normalized, zero value will be stored
    instead and proper warning will be produced. This function relies on
    the fact that even wrong value sent over binary protocol fits into
    MAX_DATE_STRING_REP_LENGTH buffer.
*/
void Item_param::set_time(TIME *tm, timestamp_type type, uint32 max_length_arg)
{ 
  DBUG_ENTER("Item_param::set_time");

  value.time= *tm;
  value.time.time_type= type;

  if (value.time.year > 9999 || value.time.month > 12 ||
      value.time.day > 31 ||
      type != MYSQL_TIMESTAMP_TIME && value.time.hour > 23 ||
      value.time.minute > 59 || value.time.second > 59)
  {
    char buff[MAX_DATE_STRING_REP_LENGTH];
    uint length= my_TIME_to_str(&value.time, buff);
    make_truncated_value_warning(current_thd, buff, length, type, 0);
    set_zero_time(&value.time, MYSQL_TIMESTAMP_ERROR);
  }

  state= TIME_VALUE;
  maybe_null= 0;
  max_length= max_length_arg;
  decimals= 0;
  DBUG_VOID_RETURN;
}


bool Item_param::set_str(const char *str, ulong length)
{
  DBUG_ENTER("Item_param::set_str");
  /*
    Assign string with no conversion: data is converted only after it's
    been written to the binary log.
  */
  uint dummy_errors;
  if (str_value.copy(str, length, &my_charset_bin, &my_charset_bin,
                     &dummy_errors))
    DBUG_RETURN(TRUE);
  state= STRING_VALUE;
  max_length= length;
  maybe_null= 0;
  /* max_length and decimals are set after charset conversion */
  /* sic: str may be not null-terminated, don't add DBUG_PRINT here */
  DBUG_RETURN(FALSE);
}


bool Item_param::set_longdata(const char *str, ulong length)
{
  DBUG_ENTER("Item_param::set_longdata");

  /*
    If client character set is multibyte, end of long data packet
    may hit at the middle of a multibyte character.  Additionally,
    if binary log is open we must write long data value to the
    binary log in character set of client. This is why we can't
    convert long data to connection character set as it comes
    (here), and first have to concatenate all pieces together,
    write query to the binary log and only then perform conversion.
  */
  if (str_value.append(str, length, &my_charset_bin))
    DBUG_RETURN(TRUE);
  state= LONG_DATA_VALUE;
  maybe_null= 0;

  DBUG_RETURN(FALSE);
}


/*
  Set parameter value from user variable value.

  SYNOPSIS
   set_from_user_var
     thd   Current thread
     entry User variable structure (NULL means use NULL value)

  RETURN
    0 OK
    1 Out of memory
*/

bool Item_param::set_from_user_var(THD *thd, const user_var_entry *entry)
{
  DBUG_ENTER("Item_param::set_from_user_var");
  if (entry && entry->value)
  {
    item_result_type= entry->type;
    switch (entry->type) {
    case REAL_RESULT:
      set_double(*(double*)entry->value);
      item_type= Item::REAL_ITEM;
      item_result_type= REAL_RESULT;
      break;
    case INT_RESULT:
      set_int(*(longlong*)entry->value, 21);
      item_type= Item::INT_ITEM;
      item_result_type= INT_RESULT;
      break;
    case STRING_RESULT:
    {
      CHARSET_INFO *fromcs= entry->collation.collation;
      CHARSET_INFO *tocs= thd->variables.collation_connection;
      uint32 dummy_offset;

      value.cs_info.character_set_client= fromcs;
      /*
        Setup source and destination character sets so that they
        are different only if conversion is necessary: this will
        make later checks easier.
      */
      value.cs_info.final_character_set_of_str_value=
        String::needs_conversion(0, fromcs, tocs, &dummy_offset) ?
        tocs : fromcs;
      /*
        Exact value of max_length is not known unless data is converted to
        charset of connection, so we have to set it later.
      */
      item_type= Item::STRING_ITEM;
      item_result_type= STRING_RESULT;

      if (set_str((const char *)entry->value, entry->length))
        DBUG_RETURN(1);
      break;
    }
    case DECIMAL_RESULT:
    {
      const my_decimal *ent_value= (const my_decimal *)entry->value;
      my_decimal2decimal(ent_value, &decimal_value);
      state= DECIMAL_VALUE;
      decimals= ent_value->frac;
      max_length= ent_value->intg + decimals + 2;
      break;
    }
    default:
      DBUG_ASSERT(0);
      set_null();
    }
  }
  else
    set_null();

  DBUG_RETURN(0);
}

/*
    Resets parameter after execution.
  
  SYNOPSIS
     Item_param::reset()
 
  NOTES
    We clear null_value here instead of setting it in set_* methods, 
    because we want more easily handle case for long data.
*/

void Item_param::reset()
{
  /* Shrink string buffer if it's bigger than max possible CHAR column */
  if (str_value.alloced_length() > MAX_CHAR_WIDTH)
    str_value.free();
  else
    str_value.length(0);
  str_value_ptr.length(0);
  /*
    We must prevent all charset conversions until data has been written
    to the binary log.
  */
  str_value.set_charset(&my_charset_bin);
  state= NO_VALUE;
  maybe_null= 1;
  null_value= 0;
  /*
    Don't reset item_type to PARAM_ITEM: it's only needed to guard
    us from item optimizations at prepare stage, when item doesn't yet
    contain a literal of some kind.
    In all other cases when this object is accessed its value is
    set (this assumption is guarded by 'state' and
    DBUG_ASSERTS(state != NO_VALUE) in all Item_param::get_*
    methods).
  */
}


int Item_param::save_in_field(Field *field, bool no_conversions)
{
  field->set_notnull();

  switch (state) {
  case INT_VALUE:
    return field->store(value.integer);
  case REAL_VALUE:
    return field->store(value.real);
  case DECIMAL_VALUE:
    return field->store_decimal(&decimal_value);
  case TIME_VALUE:
    field->store_time(&value.time, value.time.time_type);
    return 0;
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    return field->store(str_value.ptr(), str_value.length(),
                        str_value.charset());
  case NULL_VALUE:
    return set_field_to_null_with_conversions(field, no_conversions);
  case NO_VALUE:
  default:
    DBUG_ASSERT(0);
  }
  return 1;
}


bool Item_param::get_time(TIME *res)
{
  if (state == TIME_VALUE)
  {
    *res= value.time;
    return 0;
  }
  /*
    If parameter value isn't supplied assertion will fire in val_str()
    which is called from Item::get_time().
  */
  return Item::get_time(res);
}


bool Item_param::get_date(TIME *res, uint fuzzydate)
{
  if (state == TIME_VALUE)
  {
    *res= value.time;
    return 0;
  }
  return Item::get_date(res, fuzzydate);
}


double Item_param::val_real()
{
  switch (state) {
  case REAL_VALUE:
    return value.real;
  case INT_VALUE:
    return (double) value.integer;
  case DECIMAL_VALUE:
  {
    double result;
    my_decimal2double(E_DEC_FATAL_ERROR, &decimal_value, &result);
    return result;
  }
  case STRING_VALUE:
  case LONG_DATA_VALUE:
  {
    int dummy_err;
    char *end_not_used;
    return my_strntod(str_value.charset(), (char*) str_value.ptr(),
                      str_value.length(), &end_not_used, &dummy_err);
  }
  case TIME_VALUE:
    /*
      This works for example when user says SELECT ?+0.0 and supplies
      time value for the placeholder.
    */
    return ulonglong2double(TIME_to_ulonglong(&value.time));
  case NULL_VALUE:
    return 0.0;
  default:
    DBUG_ASSERT(0);
  }
  return 0.0;
} 


longlong Item_param::val_int() 
{ 
  switch (state) {
  case REAL_VALUE:
    return (longlong) (value.real + (value.real > 0 ? 0.5 : -0.5));
  case INT_VALUE:
    return value.integer;
  case DECIMAL_VALUE:
  {
    longlong i;
    my_decimal2int(E_DEC_FATAL_ERROR, &decimal_value, unsigned_flag, &i);
    return i;
  }
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    {
      int dummy_err;
      return my_strntoll(str_value.charset(), str_value.ptr(),
                         str_value.length(), 10, (char**) 0, &dummy_err);
    }
  case TIME_VALUE:
    return (longlong) TIME_to_ulonglong(&value.time);
  case NULL_VALUE:
    return 0; 
  default:
    DBUG_ASSERT(0);
  }
  return 0;
}


my_decimal *Item_param::val_decimal(my_decimal *dec)
{
  switch (state) {
  case DECIMAL_VALUE:
    return &decimal_value;
  case REAL_VALUE:
    double2my_decimal(E_DEC_FATAL_ERROR, value.real, dec);
    return dec;
  case INT_VALUE:
    int2my_decimal(E_DEC_FATAL_ERROR, value.integer, unsigned_flag, dec);
    return dec;
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    string2my_decimal(E_DEC_FATAL_ERROR, &str_value, dec);
    return dec;
  case TIME_VALUE:
  {
    longlong i= (longlong) TIME_to_ulonglong(&value.time);
    int2my_decimal(E_DEC_FATAL_ERROR, i, 0, dec);
    return dec;
  }
  case NULL_VALUE:
    return 0; 
  default:
    DBUG_ASSERT(0);
  }
  return 0;
}


String *Item_param::val_str(String* str) 
{ 
  switch (state) {
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    return &str_value_ptr;
  case REAL_VALUE:
    str->set(value.real, NOT_FIXED_DEC, &my_charset_bin);
    return str;
  case INT_VALUE:
    str->set(value.integer, &my_charset_bin);
    return str;
  case DECIMAL_VALUE:
    if (my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value,
                          0, 0, 0, str) <= 1)
      return str;
    return NULL;
  case TIME_VALUE:
  {
    if (str->reserve(MAX_DATE_STRING_REP_LENGTH))
      break;
    str->length((uint) my_TIME_to_str(&value.time, (char*) str->ptr()));
    str->set_charset(&my_charset_bin);
    return str;
  }
  case NULL_VALUE:
    return NULL; 
  default:
    DBUG_ASSERT(0);
  }
  return str;
}

/*
  Return Param item values in string format, for generating the dynamic 
  query used in update/binary logs
  TODO: change interface and implementation to fill log data in place
  and avoid one more memcpy/alloc between str and log string.
*/

const String *Item_param::query_val_str(String* str) const
{
  switch (state) {
  case INT_VALUE:
    str->set(value.integer, &my_charset_bin);
    break;
  case REAL_VALUE:
    str->set(value.real, NOT_FIXED_DEC, &my_charset_bin);
    break;
  case DECIMAL_VALUE:
    if (my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value,
                          0, 0, 0, str) > 1)
      return &my_null_string;
    break;
  case TIME_VALUE:
    {
      char *buf, *ptr;
      str->length(0);
      /*
        TODO: in case of error we need to notify replication
        that binary log contains wrong statement 
      */
      if (str->reserve(MAX_DATE_STRING_REP_LENGTH+3))
        break; 

      /* Create date string inplace */
      buf= str->c_ptr_quick();
      ptr= buf;
      *ptr++= '\'';
      ptr+= (uint) my_TIME_to_str(&value.time, ptr);
      *ptr++= '\'';
      str->length((uint32) (ptr - buf));
      break;
    }
  case STRING_VALUE:
  case LONG_DATA_VALUE:
    {
      char *buf, *ptr;
      str->length(0);
      if (str->reserve(str_value.length()*2+3))
        break;

      buf= str->c_ptr_quick();
      ptr= buf;
      *ptr++= '\'';
      ptr+= escape_string_for_mysql(str_value.charset(), ptr,
                                    str_value.ptr(), str_value.length());
      *ptr++= '\'';
      str->length(ptr - buf);
      break;
    }
  case NULL_VALUE:
    return &my_null_string;
  default:
    DBUG_ASSERT(0);
  }
  return str;
}


/*
  Convert string from client character set to the character set of
  connection.
*/

bool Item_param::convert_str_value(THD *thd)
{
  bool rc= FALSE;
  if (state == STRING_VALUE || state == LONG_DATA_VALUE)
  {
    /*
      Check is so simple because all charsets were set up properly
      in setup_one_conversion_function, where typecode of
      placeholder was also taken into account: the variables are different
      here only if conversion is really necessary.
    */
    if (value.cs_info.final_character_set_of_str_value !=
        value.cs_info.character_set_client)
    {
      rc= thd->convert_string(&str_value,
                              value.cs_info.character_set_client,
                              value.cs_info.final_character_set_of_str_value);
    }
    else
      str_value.set_charset(value.cs_info.final_character_set_of_str_value);
    /* Here str_value is guaranteed to be in final_character_set_of_str_value */

    max_length= str_value.length();
    decimals= 0;
    /*
      str_value_ptr is returned from val_str(). It must be not alloced
      to prevent it's modification by val_str() invoker.
    */
    str_value_ptr.set(str_value.ptr(), str_value.length(),
                      str_value.charset());
  }
  return rc;
}


void Item_param::print(String *str)
{
  if (state == NO_VALUE)
  {
    str->append('?');
  }
  else
  {
    char buffer[STRING_BUFFER_USUAL_SIZE];
    String tmp(buffer, sizeof(buffer), &my_charset_bin);
    const String *res;
    res= query_val_str(&tmp);
    str->append(*res);
  }
}


/****************************************************************************
  Item_copy_string
****************************************************************************/

void Item_copy_string::copy()
{
  String *res=item->val_str(&str_value);
  if (res && res != &str_value)
    str_value.copy(*res);
  null_value=item->null_value;
}

/* ARGSUSED */
String *Item_copy_string::val_str(String *str)
{
  // Item_copy_string is used without fix_fields call
  if (null_value)
    return (String*) 0;
  return &str_value;
}


my_decimal *Item_copy_string::val_decimal(my_decimal *decimal_value)
{
  // Item_copy_string is used without fix_fields call
  if (null_value)
    return 0;
  string2my_decimal(E_DEC_FATAL_ERROR, &str_value, decimal_value);
  return (decimal_value);
}



int Item_copy_string::save_in_field(Field *field, bool no_conversions)
{
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store(str_value.ptr(),str_value.length(),
		      collation.collation);
}

/*
  Functions to convert item to field (for send_fields)
*/

/* ARGSUSED */
bool Item::fix_fields(THD *thd,
		      struct st_table_list *list,
		      Item ** ref)
{

  // We do not check fields which are fixed during construction
  DBUG_ASSERT(fixed == 0 || basic_const_item());
  fixed= 1;
  return FALSE;
}

double Item_ref_null_helper::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double tmp= (*ref)->val_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}


longlong Item_ref_null_helper::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong tmp= (*ref)->val_int_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}


my_decimal *Item_ref_null_helper::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  my_decimal *val= (*ref)->val_decimal_result(decimal_value);
  owner->was_null|= null_value= (*ref)->null_value;
  return val;
}


bool Item_ref_null_helper::val_bool()
{
  DBUG_ASSERT(fixed == 1);
  bool val= (*ref)->val_bool_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return val;
}


String* Item_ref_null_helper::val_str(String* s)
{
  DBUG_ASSERT(fixed == 1);
  String* tmp= (*ref)->str_result(s);
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}


bool Item_ref_null_helper::get_date(TIME *ltime, uint fuzzydate)
{  
  return (owner->was_null|= null_value= (*ref)->get_date(ltime, fuzzydate));
}


/*
  Mark item and SELECT_LEXs as dependent if item was resolved in outer SELECT

  SYNOPSIS
    mark_as_dependent()
    thd - thread handler
    last - select from which current item depend
    current  - current select
    resolved_item - item which was resolved in outer SELECT(for warning)
    mark_item - item which should be marked (can be differ in case of
                substitution)
*/

static void mark_as_dependent(THD *thd, SELECT_LEX *last, SELECT_LEX *current,
			      Item_ident *resolved_item,
                              Item_ident *mark_item)
{
  const char *db_name= (resolved_item->db_name ?
                        resolved_item->db_name : "");
  const char *table_name= (resolved_item->table_name ?
                           resolved_item->table_name : "");
  /* store pointer on SELECT_LEX from which item is dependent */
  if (mark_item)
    mark_item->depended_from= last;
  current->mark_as_dependent(last);
  if (thd->lex->describe & DESCRIBE_EXTENDED)
  {
    char warn_buff[MYSQL_ERRMSG_SIZE];
    sprintf(warn_buff, ER(ER_WARN_FIELD_RESOLVED),
            db_name, (db_name[0] ? "." : ""),
            table_name, (table_name [0] ? "." : ""),
            resolved_item->field_name,
	    current->select_number, last->select_number);
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		 ER_WARN_FIELD_RESOLVED, warn_buff);
  }
}




/*
  Search a GROUP BY clause for a field with a certain name.

  SYNOPSIS
    find_field_in_group_list()
    find_item  the item being searched for
    group_list GROUP BY clause

  DESCRIPTION
    Search the GROUP BY list for a column named as find_item. When searching
    preference is given to columns that are qualified with the same table (and
    database) name as the one being searched for.

  RETURN
    - the found item on success
    - NULL if find_item is not in group_list
*/

static Item** find_field_in_group_list(Item *find_item, ORDER *group_list)
{
  const char *db_name;
  const char *table_name;
  const char *field_name;
  ORDER      *found_group= NULL;
  int         found_match_degree= 0;
  Item_field *cur_field;
  int         cur_match_degree= 0;

  if (find_item->type() == Item::FIELD_ITEM ||
      find_item->type() == Item::REF_ITEM)
  {
    db_name=    ((Item_ident*) find_item)->db_name;
    table_name= ((Item_ident*) find_item)->table_name;
    field_name= ((Item_ident*) find_item)->field_name;
  }
  else
    return NULL;

  DBUG_ASSERT(field_name != 0);

  for (ORDER *cur_group= group_list ; cur_group ; cur_group= cur_group->next)
  {
    if ((*(cur_group->item))->type() == Item::FIELD_ITEM)
    {
      cur_field= (Item_field*) *cur_group->item;
      cur_match_degree= 0;
      
      DBUG_ASSERT(cur_field->field_name != 0);

      if (!my_strcasecmp(system_charset_info,
                         cur_field->field_name, field_name))
        ++cur_match_degree;
      else
        continue;

      if (cur_field->table_name && table_name)
      {
        /* If field_name is qualified by a table name. */
        if (strcmp(cur_field->table_name, table_name))
          /* Same field names, different tables. */
          return NULL;

        ++cur_match_degree;
        if (cur_field->db_name && db_name)
        {
          /* If field_name is also qualified by a database name. */
          if (strcmp(cur_field->db_name, db_name))
            /* Same field names, different databases. */
            return NULL;
          ++cur_match_degree;
        }
      }

      if (cur_match_degree > found_match_degree)
      {
        found_match_degree= cur_match_degree;
        found_group= cur_group;
      }
      else if (found_group && (cur_match_degree == found_match_degree) &&
               ! (*(found_group->item))->eq(cur_field, 0))
      {
        /*
          If the current resolve candidate matches equally well as the current
          best match, they must reference the same column, otherwise the field
          is ambiguous.
        */
        my_error(ER_NON_UNIQ_ERROR, MYF(0),
                 find_item->full_name(), current_thd->where);
        return NULL;
      }
    }
  }

  if (found_group)
    return found_group->item;
  else
    return NULL;
}


/*
  Resolve a column reference in a sub-select.

  SYNOPSIS
    resolve_ref_in_select_and_group()
    thd     current thread
    ref     column reference being resolved
    select  the sub-select that ref is resolved against

  DESCRIPTION
    Resolve a column reference (usually inside a HAVING clause) against the
    SELECT and GROUP BY clauses of the query described by 'select'. The name
    resolution algorithm searches both the SELECT and GROUP BY clauses, and in
    case of a name conflict prefers GROUP BY column names over SELECT names. If
    both clauses contain different fields with the same names, a warning is
    issued that name of 'ref' is ambiguous. We extend ANSI SQL in that when no
    GROUP BY column is found, then a HAVING name is resolved as a possibly
    derived SELECT column.

  NOTES
    The resolution procedure is:
    - Search for a column or derived column named col_ref_i [in table T_j]
      in the SELECT clause of Q.
    - Search for a column named col_ref_i [in table T_j]
      in the GROUP BY clause of Q.
    - If found different columns with the same name in GROUP BY and SELECT
      - issue a warning and return the GROUP BY column,
      - otherwise return the found SELECT column.


  RETURN
    NULL - there was an error, and the error was already reported
    not_found_item - the item was not resolved, no error was reported
    resolved item - if the item was resolved
*/

static Item**
resolve_ref_in_select_and_group(THD *thd, Item_ident *ref, SELECT_LEX *select)
{
  Item **group_by_ref= NULL;
  Item **select_ref= NULL;
  ORDER *group_list= (ORDER*) select->group_list.first;
  bool ambiguous_fields= FALSE;
  uint counter;
  bool not_used;

  /*
    Search for a column or derived column named as 'ref' in the SELECT
    clause of the current select.
  */
  if (!(select_ref= find_item_in_list(ref, *(select->get_item_list()),
                                      &counter, REPORT_EXCEPT_NOT_FOUND,
                                      &not_used)))
    return NULL; /* Some error occurred. */

  /* If this is a non-aggregated field inside HAVING, search in GROUP BY. */
  if (select->having_fix_field && !ref->with_sum_func && group_list)
  {
    group_by_ref= find_field_in_group_list(ref, group_list);
    
    /* Check if the fields found in SELECT and GROUP BY are the same field. */
    if (group_by_ref && (select_ref != not_found_item) &&
        !((*group_by_ref)->eq(*select_ref, 0)))
    {
      ambiguous_fields= TRUE;
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_NON_UNIQ_ERROR,
                          ER(ER_NON_UNIQ_ERROR), ref->full_name(),
                          current_thd->where);

    }
  }

  if (select_ref != not_found_item || group_by_ref)
  {
    if (select_ref != not_found_item && !ambiguous_fields)
    {
      DBUG_ASSERT(*select_ref != 0);
      if (!select->ref_pointer_array[counter])
      {
        my_error(ER_ILLEGAL_REFERENCE, MYF(0),
                 ref->name, "forward reference in item list");
        return NULL;
      }
      DBUG_ASSERT((*select_ref)->fixed);
      return (select->ref_pointer_array + counter);
    }
    if (group_by_ref)
      return group_by_ref;
    DBUG_ASSERT(FALSE);
    return NULL; /* So there is no compiler warning. */
  }

  return (Item**) not_found_item;
}


/*
  Resolve the name of a column reference.

  SYNOPSIS
    Item_field::fix_fields()
    thd       [in]      current thread
    tables    [in]      the tables in a FROM clause
    reference [in/out]  view column if this item was resolved to a view column

  DESCRIPTION
    The method resolves the column reference represented by 'this' as a column
    present in one of: FROM clause, SELECT clause, GROUP BY clause of a query
    Q, or in outer queries that contain Q.

  NOTES
    The name resolution algorithm used is (where [T_j] is an optional table
    name that qualifies the column name):

      resolve_column_reference([T_j].col_ref_i)
      {
        search for a column or derived column named col_ref_i
        [in table T_j] in the FROM clause of Q;

        if such a column is NOT found AND    // Lookup in outer queries.
           there are outer queries
        {
          for each outer query Q_k beginning from the inner-most one
          {
            if - Q_k is not a group query AND
               - Q_k is not inside an aggregate function
               OR
               - Q_(k-1) is not in a HAVING or SELECT clause of Q_k
            {
              search for a column or derived column named col_ref_i
              [in table T_j] in the FROM clause of Q_k;
            }

            if such a column is not found
              Search for a column or derived column named col_ref_i
              [in table T_j] in the SELECT and GROUP clauses of Q_k.
          }
        }
      }

    Notice that compared to Item_ref::fix_fields, here we first search the FROM
    clause, and then we search the SELECT and GROUP BY clauses.

  RETURN
    TRUE  if error
    FALSE on success
*/

bool Item_field::fix_fields(THD *thd, TABLE_LIST *tables, Item **reference)
{
  enum_parsing_place place= NO_MATTER;
  DBUG_ASSERT(fixed == 0);
  if (!field)					// If field is not checked
  {
    bool upward_lookup= FALSE;
    Field *from_field= (Field *)not_found_field;
    /*
      In case of view, find_field_in_tables() write pointer to view field
      expression to 'reference', i.e. it substitute that expression instead
      of this Item_field
    */
    if ((from_field= find_field_in_tables(thd, this, tables, reference,
                                   IGNORE_EXCEPT_NON_UNIQUE,
                                   !any_privileges)) ==
	not_found_field)
    {
      SELECT_LEX *last= 0;
      TABLE_LIST *table_list;
      Item **ref= (Item **) not_found_item;
      SELECT_LEX *current_sel= (SELECT_LEX *) thd->lex->current_select;
      /*
        If there is an outer select, and it is not a derived table (which do
        not support the use of outer fields for now), try to resolve this
        reference in the outer select(s).
      
        We treat each subselect as a separate namespace, so that different
        subselects may contain columns with the same names. The subselects are
        searched starting from the innermost.
      */
      if (current_sel->master_unit()->first_select()->linkage !=
          DERIVED_TABLE_TYPE)
      {
	SELECT_LEX_UNIT *prev_unit= current_sel->master_unit();
        SELECT_LEX *outer_sel= prev_unit->outer_select();
	for ( ; outer_sel ;
              outer_sel= (prev_unit= outer_sel->master_unit())->outer_select())
	{
          last= outer_sel;
	  Item_subselect *prev_subselect_item= prev_unit->item;
	  upward_lookup= TRUE;

          /* Search in the tables of the FROM clause of the outer select. */
	  table_list= outer_sel->get_table_list();
	  if (outer_sel->resolve_mode == SELECT_LEX::INSERT_MODE && table_list)
	  {
            /*
              It is a primary INSERT st_select_lex => do not resolve against the
              first table.
            */
	    table_list= table_list->next_local;
          }
          place= prev_subselect_item->parsing_place;
          /*
            Check table fields only if the subquery is used somewhere out of
            HAVING, or the outer SELECT does not use grouping (i.e. tables are
            accessible).

            In case of view, find_field_in_tables() write pointer to view
            field expression to 'reference', i.e. it substitute that
            expression instead of this Item_field
          */
          if ((place != IN_HAVING ||
               (outer_sel->with_sum_func == 0 &&
                outer_sel->group_list.elements == 0)) &&
              (from_field= find_field_in_tables(thd, this, table_list,
                                                reference,
                                                IGNORE_EXCEPT_NON_UNIQUE,
                                                TRUE)) !=
              not_found_field)
	  {
	    if (from_field)
            {
              if (from_field != view_ref_found)
              {
                prev_subselect_item->used_tables_cache|= from_field->table->map;
                prev_subselect_item->const_item_cache= 0;
              }
              else
              {
                Item::Type type= (*reference)->type();
                prev_subselect_item->used_tables_cache|=
                  (*reference)->used_tables();
                prev_subselect_item->const_item_cache&=
                  (*reference)->const_item();
                mark_as_dependent(thd, last, current_sel, this,
                                  ((type == REF_ITEM || type == FIELD_ITEM) ?
                                   (Item_ident*) (*reference) :
                                   0));
                /*
                  view reference found, we substituted it instead of this
                  Item (find_field_in_tables do it by assigning new value to
                  *reference), so can quit
                */
                return FALSE;
              }
            }
	    break;
	  }

          /* Search in the SELECT and GROUP lists of the outer select. */
	  if (outer_sel->resolve_mode == SELECT_LEX::SELECT_MODE)
          {
            if (!(ref= resolve_ref_in_select_and_group(thd, this, outer_sel)))
              return TRUE; /* Some error occurred (e.g. ambiguous names). */
            if (ref != not_found_item)
            {
              DBUG_ASSERT(*ref && (*ref)->fixed);
              prev_subselect_item->used_tables_cache|= (*ref)->used_tables();
	      prev_subselect_item->const_item_cache&= (*ref)->const_item();
              break;
            }
	  }

	  // Reference is not found => depend from outer (or just error)
	  prev_subselect_item->used_tables_cache|= OUTER_REF_TABLE_BIT;
	  prev_subselect_item->const_item_cache= 0;

	  if (outer_sel->master_unit()->first_select()->linkage ==
	      DERIVED_TABLE_TYPE)
	    break; // do not look over derived table
	}
      }

      DBUG_ASSERT(ref != 0);
      if (!from_field)
	return TRUE;
      if (ref == not_found_item && from_field == not_found_field)
      {
	if (upward_lookup)
	{
	  // We can't say exactly what absent table or field
	  my_error(ER_BAD_FIELD_ERROR, MYF(0), full_name(), thd->where);
	}
	else
	{
	  // Call to report error
	  find_field_in_tables(thd, this, tables, reference, REPORT_ALL_ERRORS,
                               TRUE);
	}
	return TRUE;
      }
      else if (ref != not_found_item)
      {
        Item *save;
        Item_ref *rf;

        /* Should have been checked in resolve_ref_in_select_and_group(). */
        DBUG_ASSERT(*ref && (*ref)->fixed);
        /*
          Here, a subset of actions performed by Item_ref::set_properties
          is not enough. So we pass ptr to NULL into Item_[direct]_ref
          constructor, so no initialization is performed, and call 
          fix_fields() below.
        */
        save= *ref;
        *ref= NULL;                             // Don't call set_properties()
        rf= (place == IN_HAVING ?
             new Item_ref(ref, (char*) table_name, (char*) field_name) :
             new Item_direct_ref(ref, (char*) table_name, (char*) field_name));
        *ref= save;
	if (!rf)
	  return TRUE;
        thd->change_item_tree(reference, rf);
	/*
	  rf is Item_ref => never substitute other items (in this case)
	  during fix_fields() => we can use rf after fix_fields()
	*/
        DBUG_ASSERT(!rf->fixed);                // Assured by Item_ref()
        if (rf->fix_fields(thd, tables, reference) || rf->check_cols(1))
	  return TRUE;

	mark_as_dependent(thd, last, current_sel, this, rf);
	return FALSE;
      }
      else
      {
	mark_as_dependent(thd, last, current_sel, this, this);
	if (last->having_fix_field)
	{
	  Item_ref *rf;
          rf= new Item_ref((cached_table->db[0] ? cached_table->db : 0),
                           (char*) cached_table->alias, (char*) field_name);
	  if (!rf)
	    return TRUE;
          thd->change_item_tree(reference, rf);
	  /*
	    rf is Item_ref => never substitute other items (in this case)
	    during fix_fields() => we can use rf after fix_fields()
	  */
          DBUG_ASSERT(!rf->fixed);                // Assured by Item_ref()
          return (rf->fix_fields(thd, tables, reference) || rf->check_cols(1));
	}
      }
    }
    else if (!from_field)
      return TRUE;

    /*
      if it is not expression from merged VIEW we will set this field.

      We can leave expression substituted from view for next PS/SP rexecution
      (i.e. do not register this substitution for reverting on cleupup()
      (register_item_tree_changing())), because this subtree will be
      fix_field'ed during setup_tables()->setup_ancestor() (i.e. before
      all other expressions of query, and references on tables which do
      not present in query will not make problems.

      Also we suppose that view can't be changed during PS/SP life.
    */
    if (from_field == view_ref_found)
      return FALSE;

    set_field(from_field);
  }
  else if (thd->set_query_id && field->query_id != thd->query_id)
  {
    /* We only come here in unions */
    TABLE *table=field->table;
    field->query_id=thd->query_id;
    table->used_fields++;
    table->used_keys.intersect(field->part_of_key);
  }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (any_privileges)
  {
    char *db, *tab;
    if (cached_table->view)
    {
      db= cached_table->view_db.str;
      tab= cached_table->view_name.str;
    }
    else
    {
      db= cached_table->db;
      tab= cached_table->table_name;
    }
    if (!(have_privileges= (get_column_grant(thd, &field->table->grant,
                                             db, tab, field_name) &
                            VIEW_ANY_ACL)))
    {
      my_error(ER_COLUMNACCESS_DENIED_ERROR, MYF(0),
               "ANY", thd->priv_user, thd->host_or_ip,
               field_name, tab);
      return TRUE;
    }
  }
#endif
  fixed= 1;
  return FALSE;
}


Item *Item_field::safe_charset_converter(CHARSET_INFO *tocs)
{
  no_const_subst= 1;
  return Item::safe_charset_converter(tocs);
}


void Item_field::cleanup()
{
  DBUG_ENTER("Item_field::cleanup");
  Item_ident::cleanup();
  /*
    Even if this object was created by direct link to field in setup_wild()
    it will be linked correctly next time by name of field and table alias.
    I.e. we can drop 'field'.
   */
  field= result_field= 0;
  DBUG_VOID_RETURN;
}

/*
  Find a field among specified multiple equalities 

  SYNOPSIS
    find_item_equal()
    cond_equal   reference to list of multiple equalities where
                 the field (this object) is to be looked for
  
  DESCRIPTION
    The function first searches the field among multiple equalities
    of the current level (in the cond_equal->current_level list).
    If it fails, it continues searching in upper levels accessed
    through a pointer cond_equal->upper_levels.
    The search terminates as soon as a multiple equality containing 
    the field is found. 

  RETURN VALUES
    First Item_equal containing the field, if success
    0, otherwise
*/
Item_equal *Item_field::find_item_equal(COND_EQUAL *cond_equal)
{
  Item_equal *item= 0;
  while (cond_equal)
  {
    List_iterator_fast<Item_equal> li(cond_equal->current_level);
    while ((item= li++))
    {
      if (item->contains(field))
        return item;
    }
    /* 
      The field is not found in any of the multiple equalities
      of the current level. Look for it in upper levels
    */
    cond_equal= cond_equal->upper_levels;
  }
  return 0;
}


/*
  Set a pointer to the multiple equality the field reference belongs to
  (if any)
   
  SYNOPSIS
    equal_fields_propagator()
    arg - reference to list of multiple equalities where
          the field (this object) is to be looked for
  
  DESCRIPTION
    The function looks for a multiple equality containing the field item
    among those referenced by arg.
    In the case such equality exists the function does the following.
    If the found multiple equality contains a constant, then the field
    reference is substituted for this constant, otherwise it sets a pointer
    to the multiple equality in the field item.

  NOTES
    This function is supposed to be called as a callback parameter in calls
    of the transform method.  

  RETURN VALUES
    pointer to the replacing constant item, if the field item was substituted 
    pointer to the field item, otherwise.
*/

Item *Item_field::equal_fields_propagator(byte *arg)
{
  if (no_const_subst)
    return this;
  item_equal= find_item_equal((COND_EQUAL *) arg);
  Item *item= 0;
  if (item_equal)
    item= item_equal->get_const();
  if (!item)
    item= this;
  return item;
}


/*
  Mark the item to not be part of substitution if it's not a binary item
  See comments in Arg_comparator::set_compare_func() for details
*/

Item *Item_field::set_no_const_sub(byte *arg)
{
  if (field->charset() != &my_charset_bin)
    no_const_subst=1;
  return this;
}


/*
  Set a pointer to the multiple equality the field reference belongs to
  (if any)
   
  SYNOPSIS
    replace_equal_field_processor()
    arg - a dummy parameter, is not used here
  
  DESCRIPTION
    The function replaces a pointer to a field in the Item_field object
    by a pointer to another field.
    The replacement field is taken from the very beginning of
    the item_equal list which the Item_field object refers to (belongs to)  
    If the Item_field object does not refer any Item_equal object,
    nothing is done.

  NOTES
    This function is supposed to be called as a callback parameter in calls
    of the walk method.  

  RETURN VALUES
    0 
*/

bool Item_field::replace_equal_field_processor(byte *arg)
{
  if (item_equal)
  {
    Item_field *subst= item_equal->get_first();
    if (!field->eq(subst->field))
    {
      field= subst->field;
      return 0;
    }
  }
  return 0;
}


void Item::init_make_field(Send_field *tmp_field,
			   enum enum_field_types field_type)
{
  char *empty_name= (char*) "";
  tmp_field->db_name=		empty_name;
  tmp_field->org_table_name=	empty_name;
  tmp_field->org_col_name=	empty_name;
  tmp_field->table_name=	empty_name;
  tmp_field->col_name=		name;
  tmp_field->charsetnr=         collation.collation->number;
  tmp_field->flags=             (maybe_null ? 0 : NOT_NULL_FLAG) | 
                                (my_binary_compare(collation.collation) ?
                                 BINARY_FLAG : 0);
  tmp_field->type=field_type;
  tmp_field->length=max_length;
  tmp_field->decimals=decimals;
  if (unsigned_flag)
    tmp_field->flags |= UNSIGNED_FLAG;
}

void Item::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field, field_type());
}


void Item_empty_string::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field, MYSQL_TYPE_VARCHAR);
}


enum_field_types Item::field_type() const
{
  switch (result_type()) {
  case STRING_RESULT:  return MYSQL_TYPE_VARCHAR;
  case INT_RESULT:     return FIELD_TYPE_LONGLONG;
  case DECIMAL_RESULT: return FIELD_TYPE_NEWDECIMAL;
  case REAL_RESULT:    return FIELD_TYPE_DOUBLE;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
    return MYSQL_TYPE_VARCHAR;
  }
}


/*
  Create a field to hold a string value from an item

  SYNOPSIS
    make_string_field()
    table		Table for which the field is created

  IMPLEMENTATION
    If max_length > CONVERT_IF_BIGGER_TO_BLOB create a blob
    If max_length > 0 create a varchar
    If max_length == 0 create a CHAR(0) 
*/


Field *Item::make_string_field(TABLE *table)
{
  if (max_length > CONVERT_IF_BIGGER_TO_BLOB)
    return new Field_blob(max_length, maybe_null, name, table,
                          collation.collation);
  if (max_length > 0)
    return new Field_varstring(max_length, maybe_null, name, table,
                               collation.collation);
  return new Field_string(max_length, maybe_null, name, table,
                          collation.collation);
}


/*
  Create a field based on field_type of argument

  For now, this is only used to create a field for
  IFNULL(x,something)

  RETURN
    0  error
    #  Created field
*/

Field *Item::tmp_table_field_from_field_type(TABLE *table)
{
  /*
    The field functions defines a field to be not null if null_ptr is not 0
  */
  uchar *null_ptr= maybe_null ? (uchar*) "" : 0;

  switch (field_type()) {
  case MYSQL_TYPE_DECIMAL:
    return new Field_decimal((char*) 0, max_length, null_ptr, 0, Field::NONE,
			     name, table, decimals, 0, unsigned_flag);
  case MYSQL_TYPE_NEWDECIMAL:
    return new Field_new_decimal((char*) 0, max_length - (decimals?1:0),
                                 null_ptr, 0,
                                 Field::NONE, name, table, decimals, 0,
                                 unsigned_flag);
  case MYSQL_TYPE_TINY:
    return new Field_tiny((char*) 0, max_length, null_ptr, 0, Field::NONE,
			  name, table, 0, unsigned_flag);
  case MYSQL_TYPE_SHORT:
    return new Field_short((char*) 0, max_length, null_ptr, 0, Field::NONE,
			   name, table, 0, unsigned_flag);
  case MYSQL_TYPE_LONG:
    return new Field_long((char*) 0, max_length, null_ptr, 0, Field::NONE,
			  name, table, 0, unsigned_flag);
#ifdef HAVE_LONG_LONG
  case MYSQL_TYPE_LONGLONG:
    return new Field_longlong((char*) 0, max_length, null_ptr, 0, Field::NONE,
			      name, table, 0, unsigned_flag);
#endif
  case MYSQL_TYPE_FLOAT:
    return new Field_float((char*) 0, max_length, null_ptr, 0, Field::NONE,
			   name, table, decimals, 0, unsigned_flag);
  case MYSQL_TYPE_DOUBLE:
    return new Field_double((char*) 0, max_length, null_ptr, 0, Field::NONE,
			    name, table, decimals, 0, unsigned_flag);
  case MYSQL_TYPE_NULL:
    return new Field_null((char*) 0, max_length, Field::NONE,
			  name, table, &my_charset_bin);
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_INT24:
    return new Field_medium((char*) 0, max_length, null_ptr, 0, Field::NONE,
			    name, table, 0, unsigned_flag);
  case MYSQL_TYPE_DATE:
    return new Field_date(maybe_null, name, table, &my_charset_bin);
  case MYSQL_TYPE_TIME:
    return new Field_time(maybe_null, name, table, &my_charset_bin);
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_DATETIME:
    return new Field_datetime(maybe_null, name, table, &my_charset_bin);
  case MYSQL_TYPE_YEAR:
    return new Field_year((char*) 0, max_length, null_ptr, 0, Field::NONE,
			  name, table);
  default:
    /* This case should never be chosen */
    DBUG_ASSERT(0);
    /* If something goes awfully wrong, it's better to get a string than die */
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
    return make_string_field(table);
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_GEOMETRY:
    return new Field_blob(max_length, maybe_null, name, table,
                          collation.collation);
    break;					// Blob handled outside of case
  }
}


/* ARGSUSED */
void Item_field::make_field(Send_field *tmp_field)
{
  field->make_field(tmp_field);
  DBUG_ASSERT(tmp_field->table_name != 0);
  if (name)
    tmp_field->col_name=name;			// Use user supplied name
}


/*
  Set a field:s value from a item
*/

void Item_field::save_org_in_field(Field *to)
{
  if (field->is_null())
  {
    null_value=1;
    set_field_to_null_with_conversions(to, 1);
  }
  else
  {
    to->set_notnull();
    field_conv(to,field);
    null_value=0;
  }
}

int Item_field::save_in_field(Field *to, bool no_conversions)
{
  if (result_field->is_null())
  {
    null_value=1;
    return set_field_to_null_with_conversions(to, no_conversions);
  }
  else
  {
    to->set_notnull();
    field_conv(to,result_field);
    null_value=0;
  }
  return 0;
}


/*
  Store null in field

  SYNOPSIS
    save_in_field()
    field		Field where we want to store NULL

  DESCRIPTION
    This is used on INSERT.
    Allow NULL to be inserted in timestamp and auto_increment values

  RETURN VALUES
    0	 ok
    1	 Field doesn't support NULL values and can't handle 'field = NULL'
*/   

int Item_null::save_in_field(Field *field, bool no_conversions)
{
  return set_field_to_null_with_conversions(field, no_conversions);
}


/*
  Store null in field

  SYNOPSIS
    save_safe_in_field()
    field		Field where we want to store NULL

  RETURN VALUES
    0	 OK
    1	 Field doesn't support NULL values
*/   

int Item_null::save_safe_in_field(Field *field)
{
  return set_field_to_null(field);
}


int Item::save_in_field(Field *field, bool no_conversions)
{
  int error;
  if (result_type() == STRING_RESULT ||
      result_type() == REAL_RESULT &&
      field->result_type() == STRING_RESULT)
  {
    String *result;
    CHARSET_INFO *cs= collation.collation;
    char buff[MAX_FIELD_WIDTH];		// Alloc buffer for small columns
    str_value.set_quick(buff, sizeof(buff), cs);
    result=val_str(&str_value);
    if (null_value)
    {
      str_value.set_quick(0, 0, cs);
      return set_field_to_null_with_conversions(field, no_conversions);
    }
    field->set_notnull();
    error=field->store(result->ptr(),result->length(),cs);
    str_value.set_quick(0, 0, cs);
  }
  else if (result_type() == REAL_RESULT)
  {
    double nr= val_real();
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    error=field->store(nr);
  }
  else if (result_type() == DECIMAL_RESULT)
  {
    my_decimal decimal_value;
    my_decimal *value= val_decimal(&decimal_value);
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    error=field->store_decimal(value);
  }
  else
  {
    longlong nr=val_int();
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store(nr);
  }
  return error;
}


int Item_string::save_in_field(Field *field, bool no_conversions)
{
  String *result;
  result=val_str(&str_value);
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store(result->ptr(),result->length(),collation.collation);
}

int Item_uint::save_in_field(Field *field, bool no_conversions)
{
  /*
    TODO: To be fixed when wen have a
    field->store(longlong, unsigned_flag) method 
  */
  return Item_int::save_in_field(field, no_conversions);
}


int Item_int::save_in_field(Field *field, bool no_conversions)
{
  longlong nr=val_int();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store(nr);
}


int Item_decimal::save_in_field(Field *field, bool no_conversions)
{
  field->set_notnull();
  return field->store_decimal(&decimal_value);
}


Item_num *Item_uint::neg()
{
  Item_decimal *item= new Item_decimal(value, 0);
  return item->neg();
}


static uint nr_of_decimals(const char *str, const char *end)
{
  const char *decimal_point;

  /* Find position for '.' */
  for (;;)
  {
    if (str == end)
      return 0;
    if (*str == 'e' || *str == 'E')
      return NOT_FIXED_DEC;    
    if (*str++ == '.')
      break;
  }
  decimal_point= str;
  for (; my_isdigit(system_charset_info, *str) ; str++)
    ;
  if (*str == 'e' || *str == 'E')
    return NOT_FIXED_DEC;
  return (uint) (str - decimal_point);
}


/*
  This function is only called during parsing. We will signal an error if
  value is not a true double value (overflow)
*/

Item_float::Item_float(const char *str_arg, uint length)
{
  int error;
  char *end_not_used;
  value= my_strntod(&my_charset_bin, (char*) str_arg, length, &end_not_used,
                    &error);
  if (error)
  {
    /*
      Note that we depend on that str_arg is null terminated, which is true
      when we are in the parser
    */
    DBUG_ASSERT(str_arg[length] == 0);
    my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "double", (char*) str_arg);
  }
  presentation= name=(char*) str_arg;
  decimals=(uint8) nr_of_decimals(str_arg, str_arg+length);
  max_length=length;
  fixed= 1;
}


int Item_float::save_in_field(Field *field, bool no_conversions)
{
  double nr= val_real();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return field->store(nr);
}


void Item_float::print(String *str)
{
  if (presentation)
  {
    str->append(presentation);
    return;
  }
  char buffer[20];
  String num(buffer, sizeof(buffer), &my_charset_bin);
  num.set(value, decimals, &my_charset_bin);
  str->append(num);
}


/*
  hex item
  In string context this is a binary string.
  In number context this is a longlong value.
*/

inline uint char_val(char X)
{
  return (uint) (X >= '0' && X <= '9' ? X-'0' :
		 X >= 'A' && X <= 'Z' ? X-'A'+10 :
		 X-'a'+10);
}


Item_hex_string::Item_hex_string(const char *str, uint str_length)
{
  name=(char*) str-2;				// Lex makes this start with 0x
  max_length=(str_length+1)/2;
  char *ptr=(char*) sql_alloc(max_length+1);
  if (!ptr)
    return;
  str_value.set(ptr,max_length,&my_charset_bin);
  char *end=ptr+max_length;
  if (max_length*2 != str_length)
    *ptr++=char_val(*str++);			// Not even, assume 0 prefix
  while (ptr != end)
  {
    *ptr++= (char) (char_val(str[0])*16+char_val(str[1]));
    str+=2;
  }
  *ptr=0;					// Keep purify happy
  collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
  fixed= 1;
}

longlong Item_hex_string::val_int()
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  char *end=(char*) str_value.ptr()+str_value.length(),
       *ptr=end-min(str_value.length(),sizeof(longlong));

  ulonglong value=0;
  for (; ptr != end ; ptr++)
    value=(value << 8)+ (ulonglong) (uchar) *ptr;
  return (longlong) value;
}


my_decimal *Item_hex_string::val_decimal(my_decimal *decimal_value)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  DBUG_ASSERT(fixed == 1);
  ulonglong value= (ulonglong)val_int();
  int2my_decimal(E_DEC_FATAL_ERROR, value, TRUE, decimal_value);
  return (decimal_value);
}


int Item_hex_string::save_in_field(Field *field, bool no_conversions)
{
  int error;
  field->set_notnull();
  if (field->result_type() == STRING_RESULT)
  {
    error=field->store(str_value.ptr(),str_value.length(),collation.collation);
  }
  else
  {
    longlong nr=val_int();
    error=field->store(nr);
  }
  return error;
}


/*
  bin item.
  In string context this is a binary string.
  In number context this is a longlong value.
*/
  
Item_bin_string::Item_bin_string(const char *str, uint str_length)
{
  const char *end= str + str_length - 1;
  uchar bits= 0;
  uint power= 1;

  name= (char*) str - 2;
  max_length= (str_length + 7) >> 3;
  char *ptr= (char*) sql_alloc(max_length + 1);
  if (!ptr)
    return;
  str_value.set(ptr, max_length, &my_charset_bin);
  ptr+= max_length - 1;
  ptr[1]= 0;                     // Set end null for string
  for (; end >= str; end--)
  {
    if (power == 256)
    {
      power= 1;
      *ptr--= bits;
      bits= 0;     
    }
    if (*end == '1')
      bits|= power; 
    power<<= 1;
  }
  *ptr= (char) bits;
  collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
  fixed= 1;
}


/*
  Pack data in buffer for sending
*/

bool Item_null::send(Protocol *protocol, String *packet)
{
  return protocol->store_null();
}

/*
  This is only called from items that is not of type item_field
*/

bool Item::send(Protocol *protocol, String *buffer)
{
  bool result;
  enum_field_types type;
  LINT_INIT(result);                     // Will be set if null_value == 0

  switch ((type=field_type())) {
  default:
  case MYSQL_TYPE_NULL:
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_GEOMETRY:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_BIT:
  case MYSQL_TYPE_NEWDECIMAL:
  {
    String *res;
    if ((res=val_str(buffer)))
      result= protocol->store(res->ptr(),res->length(),res->charset());
    break;
  }
  case MYSQL_TYPE_TINY:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_tiny(nr);
    break;
  }
  case MYSQL_TYPE_SHORT:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_short(nr);
    break;
  }
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_LONG:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_long(nr);
    break;
  }
  case MYSQL_TYPE_LONGLONG:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_longlong(nr, unsigned_flag);
    break;
  }
  case MYSQL_TYPE_FLOAT:
  {
    float nr;
    nr= (float) val_real();
    if (!null_value)
      result= protocol->store(nr, decimals, buffer);
    break;
  }
  case MYSQL_TYPE_DOUBLE:
  {
    double nr= val_real();
    if (!null_value)
      result= protocol->store(nr, decimals, buffer);
    break;
  }
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIMESTAMP:
  {
    TIME tm;
    get_date(&tm, TIME_FUZZY_DATE);
    if (!null_value)
    {
      if (type == MYSQL_TYPE_DATE)
	return protocol->store_date(&tm);
      else
	result= protocol->store(&tm);
    }
    break;
  }
  case MYSQL_TYPE_TIME:
  {
    TIME tm;
    get_time(&tm);
    if (!null_value)
      result= protocol->store_time(&tm);
    break;
  }
  }
  if (null_value)
    result= protocol->store_null();
  return result;
}


bool Item_field::send(Protocol *protocol, String *buffer)
{
  return protocol->store(result_field);
}


Item_ref::Item_ref(Item **item, const char *table_name_par,
                   const char *field_name_par)
  :Item_ident(NullS, table_name_par, field_name_par), result_field(0),
   ref(item)
{
  /*
    This constructor used to create some internals references over fixed items
  */
  DBUG_ASSERT(ref != 0);
  if (*ref)
    set_properties();
}


/*
  Resolve the name of a reference to a column reference.

  SYNOPSIS
    Item_ref::fix_fields()
    thd       [in]      current thread
    tables    [in]      the tables in a FROM clause
    reference [in/out]  view column if this item was resolved to a view column

  DESCRIPTION
    The method resolves the column reference represented by 'this' as a column
    present in one of: GROUP BY clause, SELECT clause, outer queries. It is
    used typically for columns in the HAVING clause which are not under
    aggregate functions.

  NOTES
    The name resolution algorithm used is (where [T_j] is an optional table
    name that qualifies the column name):

      resolve_extended([T_j].col_ref_i)
      {
        Search for a column or derived column named col_ref_i [in table T_j]
        in the SELECT and GROUP clauses of Q.

        if such a column is NOT found AND    // Lookup in outer queries.
           there are outer queries
        {
          for each outer query Q_k beginning from the inner-most one
         {
            Search for a column or derived column named col_ref_i
            [in table T_j] in the SELECT and GROUP clauses of Q_k.

            if such a column is not found AND
               - Q_k is not a group query AND
               - Q_k is not inside an aggregate function
               OR
               - Q_(k-1) is not in a HAVING or SELECT clause of Q_k
            {
              search for a column or derived column named col_ref_i
              [in table T_j] in the FROM clause of Q_k;
            }
          }
        }
      }

    This procedure treats GROUP BY and SELECT clauses as one namespace for
    column references in HAVING. Notice that compared to
    Item_field::fix_fields, here we first search the SELECT and GROUP BY
    clauses, and then we search the FROM clause.

  POSTCONDITION
    Item_ref::ref is 0 or points to a valid item

  RETURN
    TRUE  if error
    FALSE on success
*/

bool Item_ref::fix_fields(THD *thd, TABLE_LIST *tables, Item **reference)
{
  DBUG_ASSERT(fixed == 0);
  enum_parsing_place place= NO_MATTER;
  SELECT_LEX *current_sel= thd->lex->current_select;

  if (!ref || ref == not_found_item)
  {
    SELECT_LEX_UNIT *prev_unit= current_sel->master_unit();
    SELECT_LEX *outer_sel= prev_unit->outer_select();

    if (!(ref= resolve_ref_in_select_and_group(thd, this, current_sel)))
      return TRUE;             /* Some error occurred (e.g. ambiguous names). */

    if (ref == not_found_item) /* This reference was not resolved. */
    {
      TABLE_LIST *table_list;
      Field *from_field;
      SELECT_LEX *last;
      ref= 0;

      if (!outer_sel || (current_sel->master_unit()->first_select()->linkage ==
                        DERIVED_TABLE_TYPE))
      {
        /* The current reference cannot be resolved in this query. */
        my_error(ER_BAD_FIELD_ERROR,MYF(0),
                 this->full_name(), current_thd->where);
        return TRUE;
      }
      /*
        If there is an outer select, and it is not a derived table (which do
        not support the use of outer fields for now), try to resolve this
        reference in the outer select(s).

        We treat each subselect as a separate namespace, so that different
        subselects may contain columns with the same names. The subselects are
        searched starting from the innermost.
      */
      from_field= (Field*) not_found_field;
      last= 0;

      /* The following loop will always be excuted at least once */
      for ( ; outer_sel ;
            outer_sel= (prev_unit= outer_sel->master_unit())->outer_select())
      {
        last= outer_sel;
        Item_subselect *prev_subselect_item= prev_unit->item;

        /* Search in the SELECT and GROUP lists of the outer select. */
        if (outer_sel->resolve_mode == SELECT_LEX::SELECT_MODE)
        {
          if (!(ref= resolve_ref_in_select_and_group(thd, this, outer_sel)))
            return TRUE; /* Some error occurred (e.g. ambiguous names). */
          if (ref != not_found_item)
          {
            DBUG_ASSERT(*ref && (*ref)->fixed);
            prev_subselect_item->used_tables_cache|= (*ref)->used_tables();
            prev_subselect_item->const_item_cache&= (*ref)->const_item();
            break;
          }
          /*
            Set ref to 0 to ensure that we get an error in case we replaced
            this item with another item and still use this item in some
            other place of the parse tree.
          */
          ref= 0;
        }

        /* Search in the tables of the FROM clause of the outer select. */
        table_list= outer_sel->get_table_list();
        if (outer_sel->resolve_mode == SELECT_LEX::INSERT_MODE && table_list)
        {
          /*
            It is a primary INSERT st_select_lex => do not resolve against
            the first table.
          */
          table_list= table_list->next_local;
        }

        place= prev_subselect_item->parsing_place;
        /*
          Check table fields only if the subquery is used somewhere out of
          HAVING or the outer SELECT does not use grouping (i.e. tables are
          accessible).
          TODO: 
          Here we could first find the field anyway, and then test this
          condition, so that we can give a better error message -
          ER_WRONG_FIELD_WITH_GROUP, instead of the less informative
          ER_BAD_FIELD_ERROR which we produce now.
        */
        if ((place != IN_HAVING ||
             (!outer_sel->with_sum_func &&
              outer_sel->group_list.elements == 0)))
        {
          /*
            In case of view, find_field_in_tables() write pointer to view
            field expression to 'reference', i.e. it substitute that
            expression instead of this Item_ref
          */
          from_field= find_field_in_tables(thd, this, table_list,
                                           reference,
                                           IGNORE_EXCEPT_NON_UNIQUE,
                                           TRUE);
          if (! from_field)
            return TRUE;
          if (from_field == view_ref_found)
          {
            Item::Type type= (*reference)->type();
            prev_subselect_item->used_tables_cache|=
              (*reference)->used_tables();
            prev_subselect_item->const_item_cache&=
              (*reference)->const_item();
            DBUG_ASSERT((*reference)->type() == REF_ITEM);
            mark_as_dependent(thd, last, current_sel, this,
                              ((type == REF_ITEM || type == FIELD_ITEM) ?
                               (Item_ident*) (*reference) :
                               0));
            /*
              view reference found, we substituted it instead of this
              Item, so can quit
            */
            return FALSE;
          }
          if (from_field != not_found_field)
          {
            prev_subselect_item->used_tables_cache|= from_field->table->map;
            prev_subselect_item->const_item_cache= 0;
            break;
          }
        }
        DBUG_ASSERT(from_field == not_found_field);

        /* Reference is not found => depend on outer (or just error). */
        prev_subselect_item->used_tables_cache|= OUTER_REF_TABLE_BIT;
        prev_subselect_item->const_item_cache= 0;

        if (outer_sel->master_unit()->first_select()->linkage ==
            DERIVED_TABLE_TYPE)
          break; /* Do not consider derived tables. */
      }

      DBUG_ASSERT(from_field != 0 && from_field != view_ref_found);
      if (from_field != not_found_field)
      {
        Item_field* fld;
        if (!(fld= new Item_field(from_field)))
          return TRUE;
        thd->change_item_tree(reference, fld);
        mark_as_dependent(thd, last, thd->lex->current_select, this, fld);
        return FALSE;
      }
      if (ref == 0)
      {
        /* The item was not a table field and not a reference */
        my_error(ER_BAD_FIELD_ERROR, MYF(0),
                 this->full_name(), current_thd->where);
        return TRUE;
      }
      /* Should be checked in resolve_ref_in_select_and_group(). */
      DBUG_ASSERT(*ref && (*ref)->fixed);
      mark_as_dependent(thd, last, current_sel, this, this);
    }
  }

  DBUG_ASSERT(*ref);
  /*
    Check if this is an incorrect reference in a group function or forward
    reference. Do not issue an error if this is an unnamed reference inside an
    aggregate function.
  */
  if (((*ref)->with_sum_func && name &&
       (depended_from ||
	!(current_sel->linkage != GLOBAL_OPTIONS_TYPE &&
          current_sel->having_fix_field))) ||
      !(*ref)->fixed)
  {
    my_error(ER_ILLEGAL_REFERENCE, MYF(0),
             name, ((*ref)->with_sum_func?
                    "reference to group function":
                    "forward reference in item list"));
    return TRUE;
  }

  set_properties();

  if ((*ref)->check_cols(1))
    return TRUE;
  return FALSE;
}


void Item_ref::set_properties()
{
  max_length= (*ref)->max_length;
  maybe_null= (*ref)->maybe_null;
  decimals=   (*ref)->decimals;
  collation.set((*ref)->collation);
  /*
    We have to remember if we refer to a sum function, to ensure that
    split_sum_func() doesn't try to change the reference.
  */
  with_sum_func= (*ref)->with_sum_func;
  if ((*ref)->type() == FIELD_ITEM)
    alias_name_used= ((Item_ident *) (*ref))->alias_name_used;
  else
    alias_name_used= TRUE; // it is not field, so it is was resolved by alias
  fixed= 1;
}


void Item_ref::cleanup()
{
  DBUG_ENTER("Item_ref::cleanup");
  Item_ident::cleanup();
  result_field= 0;
  DBUG_VOID_RETURN;
}


void Item_ref::print(String *str)
{
  if (ref && *ref)
    (*ref)->print(str);
  else
    Item_ident::print(str);
}


bool Item_ref::send(Protocol *prot, String *tmp)
{
  if (result_field)
    return prot->store(result_field);
  return (*ref)->send(prot, tmp);
}


double Item_ref::val_result()
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0.0;
    return result_field->val_real();
  }
  return val_real();
}


longlong Item_ref::val_int_result()
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    return result_field->val_int();
  }
  return val_int();
}


String *Item_ref::str_result(String* str)
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    str->set_charset(str_value.charset());
    return result_field->val_str(str, &str_value);
  }
  return val_str(str);
}


my_decimal *Item_ref::val_decimal_result(my_decimal *decimal_value)
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    return result_field->val_decimal(decimal_value);
  }
  return val_decimal(decimal_value);
}


bool Item_ref::val_bool_result()
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    switch (result_field->result_type()) {
    case INT_RESULT:
      return result_field->val_int();
    case DECIMAL_RESULT:
    {
      my_decimal decimal_value;
      my_decimal *val= result_field->val_decimal(&decimal_value);
      if (val)
        return !my_decimal_is_zero(val);
      return 0;
    }
    case REAL_RESULT:
    case STRING_RESULT:
      return result_field->val_real() != 0.0;
    case ROW_RESULT:
    default:
      DBUG_ASSERT(0);
    }
  }
  return val_bool();
}


double Item_ref::val_real()
{
  DBUG_ASSERT(fixed);
  double tmp=(*ref)->val_result();
  null_value=(*ref)->null_value;
  return tmp;
}


longlong Item_ref::val_int()
{
  DBUG_ASSERT(fixed);
  longlong tmp=(*ref)->val_int_result();
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_ref::val_bool()
{
  DBUG_ASSERT(fixed);
  bool tmp= (*ref)->val_bool_result();
  null_value= (*ref)->null_value;
  return tmp;
}


String *Item_ref::val_str(String* tmp)
{
  DBUG_ASSERT(fixed);
  tmp=(*ref)->str_result(tmp);
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_ref::is_null()
{
  DBUG_ASSERT(fixed);
  (void) (*ref)->val_int_result();
  return (*ref)->null_value;
}


bool Item_ref::get_date(TIME *ltime,uint fuzzydate)
{
  return (null_value=(*ref)->get_date_result(ltime,fuzzydate));
}


my_decimal *Item_ref::val_decimal(my_decimal *decimal_value)
{
  my_decimal *val= (*ref)->val_decimal(decimal_value);
  null_value= (*ref)->null_value;
  return val;
}


void Item_ref_null_helper::print(String *str)
{
  str->append("<ref_null_helper>(", 18);
  if (ref && *ref)
    (*ref)->print(str);
  else
    str->append('?');
  str->append(')');
}


double Item_direct_ref::val_real()
{
  double tmp=(*ref)->val_real();
  null_value=(*ref)->null_value;
  return tmp;
}


longlong Item_direct_ref::val_int()
{
  longlong tmp=(*ref)->val_int();
  null_value=(*ref)->null_value;
  return tmp;
}


String *Item_direct_ref::val_str(String* tmp)
{
  tmp=(*ref)->val_str(tmp);
  null_value=(*ref)->null_value;
  return tmp;
}


my_decimal *Item_direct_ref::val_decimal(my_decimal *decimal_value)
{
  my_decimal *tmp= (*ref)->val_decimal(decimal_value);
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_direct_ref::val_bool()
{
  bool tmp= (*ref)->val_bool();
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_direct_ref::is_null()
{
  (void) (*ref)->val_int();
  return (*ref)->null_value;
}


bool Item_direct_ref::get_date(TIME *ltime,uint fuzzydate)
{
  return (null_value=(*ref)->get_date(ltime,fuzzydate));
}


void Item_null_helper::print(String *str)
{
  str->append("<null_helper>(", 14);
  store->print(str);
  str->append(')');
}


bool Item_default_value::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == DEFAULT_VALUE_ITEM && 
    ((Item_default_value *)item)->arg->eq(arg, binary_cmp);
}


bool Item_default_value::fix_fields(THD *thd,
				    struct st_table_list *table_list,
				    Item **items)
{
  Item_field *field_arg;
  Field *def_field;
  DBUG_ASSERT(fixed == 0);

  if (!arg)
  {
    fixed= 1;
    return FALSE;
  }
  if (!arg->fixed && arg->fix_fields(thd, table_list, &arg))
    return TRUE;
  
  if (arg->type() == REF_ITEM)
  {
    Item_ref *ref= (Item_ref *)arg;
    if (ref->ref[0]->type() != FIELD_ITEM)
    {
      return TRUE;
    }
    arg= ref->ref[0];
  }
  field_arg= (Item_field *)arg;
  if (field_arg->field->flags & NO_DEFAULT_VALUE_FLAG)
  {
    my_error(ER_NO_DEFAULT_FOR_FIELD, MYF(0), field_arg->field->field_name);
    return TRUE;
  }
  if (!(def_field= (Field*) sql_alloc(field_arg->field->size_of())))
    return TRUE;
  memcpy(def_field, field_arg->field, field_arg->field->size_of());
  def_field->move_field(def_field->table->s->default_values -
                        def_field->table->record[0]);
  set_field(def_field);
  return FALSE;
}

void Item_default_value::print(String *str)
{
  if (!arg)
  {
    str->append("default", 7);
    return;
  }
  str->append("default(", 8);
  arg->print(str);
  str->append(')');
}

bool Item_insert_value::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == INSERT_VALUE_ITEM &&
    ((Item_default_value *)item)->arg->eq(arg, binary_cmp);
}


bool Item_insert_value::fix_fields(THD *thd,
				   struct st_table_list *table_list,
				   Item **items)
{
  DBUG_ASSERT(fixed == 0);
  if (!arg->fixed && arg->fix_fields(thd, table_list, &arg))
    return TRUE;

  if (arg->type() == REF_ITEM)
  {
    Item_ref *ref= (Item_ref *)arg;
    if (ref->ref[0]->type() != FIELD_ITEM)
    {
      return TRUE;
    }
    arg= ref->ref[0];
  }
  Item_field *field_arg= (Item_field *)arg;
  if (field_arg->field->table->insert_values)
  {
    Field *def_field= (Field*) sql_alloc(field_arg->field->size_of());
    if (!def_field)
      return TRUE;
    memcpy(def_field, field_arg->field, field_arg->field->size_of());
    def_field->move_field(def_field->table->insert_values -
                          def_field->table->record[0]);
    set_field(def_field);
  }
  else
  {
    Field *tmp_field= field_arg->field;
    /* charset doesn't matter here, it's to avoid sigsegv only */
    set_field(new Field_null(0, 0, Field::NONE, tmp_field->field_name,
			     tmp_field->table, &my_charset_bin));
  }
  return FALSE;
}

void Item_insert_value::print(String *str)
{
  str->append("values(", 7);
  arg->print(str);
  str->append(')');
}


/*
  Bind item representing field of row being changed in trigger
  to appropriate Field object.

  SYNOPSIS
    setup_field()
      thd   - current thread context
      table - table of trigger (and where we looking for fields)
      event - type of trigger event

  NOTE
    This function does almost the same as fix_fields() for Item_field
    but is invoked during trigger definition parsing and takes TABLE
    object as its argument. If proper field was not found in table
    error will be reported at fix_fields() time.
*/
void Item_trigger_field::setup_field(THD *thd, TABLE *table,
                                     enum trg_event_type event)
{
  uint field_idx= (uint)-1;
  bool save_set_query_id= thd->set_query_id;

  /* TODO: Think more about consequences of this step. */
  thd->set_query_id= 0;

  if (find_field_in_real_table(thd, table, field_name,
                                     strlen(field_name), 0, 0,
                                     &field_idx))
  {
    field= (row_version == OLD_ROW && event == TRG_EVENT_UPDATE) ?
             table->triggers->old_field[field_idx] :
             table->field[field_idx];
  }

  thd->set_query_id= save_set_query_id;
}


bool Item_trigger_field::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == TRIGGER_FIELD_ITEM &&
         row_version == ((Item_trigger_field *)item)->row_version &&
         !my_strcasecmp(system_charset_info, field_name,
                        ((Item_trigger_field *)item)->field_name);
}


bool Item_trigger_field::fix_fields(THD *thd,
                                    TABLE_LIST *table_list,
                                    Item **items)
{
  /*
    Since trigger is object tightly associated with TABLE object most
    of its set up can be performed during trigger loading i.e. trigger
    parsing! So we have little to do in fix_fields. :)
    FIXME may be we still should bother about permissions here.
  */
  DBUG_ASSERT(fixed == 0);

  if (field)
  {
    // QQ: May be this should be moved to setup_field?
    set_field(field);
    fixed= 1;
    return 0;
  }

  my_error(ER_BAD_FIELD_ERROR, MYF(0), field_name,
           (row_version == NEW_ROW) ? "NEW" : "OLD");
  return 1;
}


void Item_trigger_field::print(String *str)
{
  str->append((row_version == NEW_ROW) ? "NEW" : "OLD", 3);
  str->append('.');
  str->append(field_name);
}


void Item_trigger_field::cleanup()
{
  /*
    Since special nature of Item_trigger_field we should not do most of
    things from Item_field::cleanup() or Item_ident::cleanup() here.
  */
  Item::cleanup();
}


/*
  If item is a const function, calculate it and return a const item
  The original item is freed if not returned
*/

Item_result item_cmp_type(Item_result a,Item_result b)
{
  if (a == STRING_RESULT && b == STRING_RESULT)
    return STRING_RESULT;
  if (a == INT_RESULT && b == INT_RESULT)
    return INT_RESULT;
  else if (a == ROW_RESULT || b == ROW_RESULT)
    return ROW_RESULT;
  if ((a == INT_RESULT || a == DECIMAL_RESULT) &&
      (b == INT_RESULT || b == DECIMAL_RESULT))
    return DECIMAL_RESULT;
  return REAL_RESULT;
}


void resolve_const_item(THD *thd, Item **ref, Item *comp_item)
{
  Item *item= *ref;
  Item *new_item;
  if (item->basic_const_item())
    return;                                     // Can't be better
  Item_result res_type=item_cmp_type(comp_item->result_type(),
				     item->result_type());
  char *name=item->name;			// Alloced by sql_alloc

  switch (res_type) {
  case STRING_RESULT:
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff),&my_charset_bin),*result;
    result=item->val_str(&tmp);
    if (item->null_value)
      new_item= new Item_null(name);
    else
    {
      uint length= result->length();
      char *tmp_str= sql_strmake(result->ptr(), length);
      new_item= new Item_string(name, tmp_str, length, result->charset());
    }
    break;
  }
  case INT_RESULT:
  {
    longlong result=item->val_int();
    uint length=item->max_length;
    bool null_value=item->null_value;
    new_item= (null_value ? (Item*) new Item_null(name) :
               (Item*) new Item_int(name, result, length));
    break;
  }
  case REAL_RESULT:
  {						// It must REAL_RESULT
    double result= item->val_real();
    uint length=item->max_length,decimals=item->decimals;
    bool null_value=item->null_value;
    new_item= (null_value ? (Item*) new Item_null(name) : (Item*)
               new Item_float(name, result, decimals, length));
    break;
  }
  case DECIMAL_RESULT:
  {
    my_decimal decimal_value;
    my_decimal *result= item->val_decimal(&decimal_value);
    uint length= item->max_length, decimals= item->decimals;
    bool null_value= item->null_value;
    new_item= (null_value ?
               (Item*) new Item_null(name) :
               (Item*) new Item_decimal(name, result, length, decimals));
    break;
  }
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
  if (new_item)
    thd->change_item_tree(ref, new_item);
}

/*
  Return true if the value stored in the field is equal to the const item
  We need to use this on the range optimizer because in some cases
  we can't store the value in the field without some precision/character loss.
*/

bool field_is_equal_to_item(Field *field,Item *item)
{

  Item_result res_type=item_cmp_type(field->result_type(),
				     item->result_type());
  if (res_type == STRING_RESULT)
  {
    char item_buff[MAX_FIELD_WIDTH];
    char field_buff[MAX_FIELD_WIDTH];
    String item_tmp(item_buff,sizeof(item_buff),&my_charset_bin),*item_result;
    String field_tmp(field_buff,sizeof(field_buff),&my_charset_bin);
    item_result=item->val_str(&item_tmp);
    if (item->null_value)
      return 1;					// This must be true
    field->val_str(&field_tmp);
    return !stringcmp(&field_tmp,item_result);
  }
  if (res_type == INT_RESULT)
    return 1;					// Both where of type int
  if (res_type == DECIMAL_RESULT)
  {
    my_decimal item_buf, *item_val,
               field_buf, *field_val;
    item_val= item->val_decimal(&item_buf);
    if (item->null_value)
      return 1;					// This must be true
    field_val= field->val_decimal(&field_buf);
    return !my_decimal_cmp(item_val, field_val);
  }
  double result= item->val_real();
  if (item->null_value)
    return 1;
  return result == field->val_real();
}

Item_cache* Item_cache::get_cache(Item_result type)
{
  switch (type) {
  case INT_RESULT:
    return new Item_cache_int();
  case REAL_RESULT:
    return new Item_cache_real();
  case DECIMAL_RESULT:
    return new Item_cache_decimal();
  case STRING_RESULT:
    return new Item_cache_str();
  case ROW_RESULT:
    return new Item_cache_row();
  default:
    // should never be in real life
    DBUG_ASSERT(0);
    return 0;
  }
}


void Item_cache::print(String *str)
{
  str->append("<cache>(", 8);
  if (example)
    example->print(str);
  else
    Item::print(str);
  str->append(')');
}


void Item_cache_int::store(Item *item)
{
  value= item->val_int_result();
  null_value= item->null_value;
  unsigned_flag= item->unsigned_flag;
}


String *Item_cache_int::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  str->set(value, default_charset());
  return str;
}


my_decimal *Item_cache_int::val_decimal(my_decimal *decimal_val)
{
  DBUG_ASSERT(fixed == 1);
  int2my_decimal(E_DEC_FATAL_ERROR, value, unsigned_flag, decimal_val);
  return decimal_val;
}


void Item_cache_real::store(Item *item)
{
  value= item->val_result();
  null_value= item->null_value;
}


longlong Item_cache_real::val_int()
{
  DBUG_ASSERT(fixed == 1);
  return (longlong) (value+(value > 0 ? 0.5 : -0.5));
}


String* Item_cache_real::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  str->set(value, decimals, default_charset());
  return str;
}


my_decimal *Item_cache_real::val_decimal(my_decimal *decimal_val)
{
  DBUG_ASSERT(fixed == 1);
  double2my_decimal(E_DEC_FATAL_ERROR, value, decimal_val);
  return decimal_val;
}


void Item_cache_decimal::store(Item *item)
{
  my_decimal *val= item->val_decimal_result(&decimal_value);
  if (!(null_value= item->null_value) && val != &decimal_value)
    my_decimal2decimal(val, &decimal_value);
}

double Item_cache_decimal::val_real()
{
  DBUG_ASSERT(fixed);
  double res;
  my_decimal2double(E_DEC_FATAL_ERROR, &decimal_value, &res);
  return res;
}

longlong Item_cache_decimal::val_int()
{
  DBUG_ASSERT(fixed);
  longlong res;
  my_decimal2int(E_DEC_FATAL_ERROR, &decimal_value, unsigned_flag, &res);
  return res;
}

String* Item_cache_decimal::val_str(String *str)
{
  DBUG_ASSERT(fixed);
  my_decimal_round(E_DEC_FATAL_ERROR, &decimal_value, decimals, FALSE,
                   &decimal_value);
  my_decimal2string(E_DEC_FATAL_ERROR, &decimal_value, 0, 0, 0, str);
  return str;
}

my_decimal *Item_cache_decimal::val_decimal(my_decimal *val)
{
  DBUG_ASSERT(fixed);
  return &decimal_value;
}


void Item_cache_str::store(Item *item)
{
  value_buff.set(buffer, sizeof(buffer), item->collation.collation);
  value= item->str_result(&value_buff);
  if ((null_value= item->null_value))
    value= 0;
  else if (value != &value_buff)
  {
    /*
      We copy string value to avoid changing value if 'item' is table field
      in queries like following (where t1.c is varchar):
      select a, 
             (select a,b,c from t1 where t1.a=t2.a) = ROW(a,2,'a'),
             (select c from t1 where a=t2.a)
        from t2;
    */
    value_buff.copy(*value);
    value= &value_buff;
  }
}

double Item_cache_str::val_real()
{
  DBUG_ASSERT(fixed == 1);
  int err_not_used;
  char *end_not_used;
  if (value)
    return my_strntod(value->charset(), (char*) value->ptr(),
		      value->length(), &end_not_used, &err_not_used);
  return (double) 0;
}


longlong Item_cache_str::val_int()
{
  DBUG_ASSERT(fixed == 1);
  int err;
  if (value)
    return my_strntoll(value->charset(), value->ptr(),
		       value->length(), 10, (char**) 0, &err);
  else
    return (longlong)0;
}

my_decimal *Item_cache_str::val_decimal(my_decimal *decimal_val)
{
  DBUG_ASSERT(fixed == 1);
  if (value)
    string2my_decimal(E_DEC_FATAL_ERROR, value, decimal_val);
  else
    decimal_val= 0;
  return decimal_val;
}


bool Item_cache_row::allocate(uint num)
{
  item_count= num;
  THD *thd= current_thd;
  return (!(values= 
	    (Item_cache **) thd->calloc(sizeof(Item_cache *)*item_count)));
}


bool Item_cache_row::setup(Item * item)
{
  example= item;
  if (!values && allocate(item->cols()))
    return 1;
  for (uint i= 0; i < item_count; i++)
  {
    Item *el= item->el(i);
    Item_cache *tmp;
    if (!(tmp= values[i]= Item_cache::get_cache(el->result_type())))
      return 1;
    tmp->setup(el);
  }
  return 0;
}


void Item_cache_row::store(Item * item)
{
  null_value= 0;
  item->bring_value();
  for (uint i= 0; i < item_count; i++)
  {
    values[i]->store(item->el(i));
    null_value|= values[i]->null_value;
  }
}


void Item_cache_row::illegal_method_call(const char *method)
{
  DBUG_ENTER("Item_cache_row::illegal_method_call");
  DBUG_PRINT("error", ("!!! %s method was called for row item", method));
  DBUG_ASSERT(0);
  my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
  DBUG_VOID_RETURN;
}


bool Item_cache_row::check_cols(uint c)
{
  if (c != item_count)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return 1;
  }
  return 0;
}


bool Item_cache_row::null_inside()
{
  for (uint i= 0; i < item_count; i++)
  {
    if (values[i]->cols() > 1)
    {
      if (values[i]->null_inside())
	return 1;
    }
    else
    {
      values[i]->val_int();
      if (values[i]->null_value)
	return 1;
    }
  }
  return 0;
}


void Item_cache_row::bring_value()
{
  for (uint i= 0; i < item_count; i++)
    values[i]->bring_value();
  return;
}


/*
  Returns field for temporary table dependind on item type

  SYNOPSIS
    get_holder_example_field()
    thd            - thread handler
    item           - pointer to item
    table          - empty table object

  NOTE
    It is possible to return field for Item_func 
    items only if field type of this item is 
    date or time or datetime type.
    also see function field_types_to_be_kept() from
    field.cc

  RETURN
    # - field
    0 - no field
*/

Field *get_holder_example_field(THD *thd, Item *item, TABLE *table)
{
  DBUG_ASSERT(table != 0);

  Item_func *tmp_item= 0;
  if (item->type() == Item::FIELD_ITEM)
    return (((Item_field*) item)->field);
  if (item->type() == Item::FUNC_ITEM)
    tmp_item= (Item_func *) item;
  else if (item->type() == Item::SUM_FUNC_ITEM)
  {
    Item_sum *item_sum= (Item_sum *) item;
    if (item_sum->keep_field_type())
    {
      if (item_sum->args[0]->type() == Item::FIELD_ITEM)
        return (((Item_field*) item_sum->args[0])->field);
      if (item_sum->args[0]->type() == Item::FUNC_ITEM)
        tmp_item= (Item_func *) item_sum->args[0];
    }
  }
  return (tmp_item && field_types_to_be_kept(tmp_item->field_type()) ?
          tmp_item->tmp_table_field(table) : 0);
}


Item_type_holder::Item_type_holder(THD *thd, Item *item, TABLE *table)
  :Item(thd, item), item_type(item->result_type()),
   orig_type(item_type)
{
  DBUG_ASSERT(item->fixed);

  /*
    It is safe assign pointer on field, because it will be used just after
    all JOIN::prepare calls and before any SELECT execution
  */
  field_example= get_holder_example_field(thd, item, table);
  max_length= real_length(item);
  maybe_null= item->maybe_null;
  collation.set(item->collation);
}


/*
  STRING_RESULT, REAL_RESULT, INT_RESULT, ROW_RESULT DECIMAL_RESULT

  ROW_RESULT should never appear in Item_type_holder::join_types,
  but it is included in following table just to make table full
  (there DBUG_ASSERT in function to catch ROW_RESULT)
*/
static Item_result type_convertor[5][5]=
{{STRING_RESULT, STRING_RESULT, STRING_RESULT, ROW_RESULT, STRING_RESULT},
 {STRING_RESULT, REAL_RESULT,   REAL_RESULT,   ROW_RESULT, REAL_RESULT},
 {STRING_RESULT, REAL_RESULT,   INT_RESULT,    ROW_RESULT, DECIMAL_RESULT},
 {ROW_RESULT,    ROW_RESULT,    ROW_RESULT,    ROW_RESULT, ROW_RESULT},
 {STRING_RESULT, REAL_RESULT,   DECIMAL_RESULT,    ROW_RESULT, DECIMAL_RESULT}};

/*
  Values of 'from' field can be stored in 'to' field.

  SYNOPSIS
    is_attr_compatible()
    from        Item which values should be saved
    to          Item where values should be saved

  RETURN
    1   can be saved
    0   can not be saved
*/

inline bool is_attr_compatible(Item *from, Item *to)
{
  return ((to->max_length >= from->max_length) &&
          ((to->result_type() != DECIMAL_RESULT &&
            to->result_type() != REAL_RESULT &&
            to->result_type() != INT_RESULT) ||
           (to->decimals >= from->decimals) &&
           ((to->max_length - to->decimals) >=
            (from->max_length - from->decimals))) &&
          (to->maybe_null || !from->maybe_null) &&
          (to->result_type() != STRING_RESULT ||
           from->result_type() != STRING_RESULT ||
          (from->collation.collation == to->collation.collation)));
}


bool Item_type_holder::join_types(THD *thd, Item *item, TABLE *table)
{
  uint32 new_length= real_length(item);
  bool use_new_field= 0, use_expression_type= 0;
  Item_result new_result_type= type_convertor[item_type][item->result_type()];
  Field *field= get_holder_example_field(thd, item, table);
  bool item_is_a_field= (field != NULL);
  /*
    Check if both items point to fields: in this case we
    can adjust column types of result table in the union smartly.
  */
  if (field_example && item_is_a_field)
  {
    /* Can 'field_example' field store data of the column? */
    if ((use_new_field=
         (!field->field_cast_compatible(field_example->field_cast_type()) ||
          !is_attr_compatible(item, this))))
    {
      /*
        The old field can't store value of the new field.
        Check if the new field can store value of the old one.
      */
      use_expression_type|=
        (!field_example->field_cast_compatible(field->field_cast_type()) ||
         !is_attr_compatible(this, item));
    }
  }
  else if (field_example || item_is_a_field)
  {
    /*
      Expression types can't be mixed with field types, we have to use
      expression types.
    */
    use_new_field= 1;                           // make next if test easier
    use_expression_type= 1;
  }

  /* Check whether size/type of the result item should be changed */
  if (use_new_field ||
      (new_result_type != item_type) || (new_length > max_length) ||
      (!maybe_null && item->maybe_null) ||
      ((new_result_type == REAL_RESULT || new_result_type == DECIMAL_RESULT) &&
       (decimals < item->decimals ||
        (max_length - decimals) < (new_length - item->decimals))) ||
      (item_type == STRING_RESULT && 
       collation.collation != item->collation.collation))
  {
    const char *old_cs,*old_derivation;
    if (use_expression_type || !item_is_a_field)
      field_example= 0;
    else
    {
      /*
        It is safe to assign a pointer to field here, because it will be used
        before any table is closed.
      */
      field_example= field;
    }

    old_cs= collation.collation->name;
    old_derivation= collation.derivation_name();
    if (item_type == STRING_RESULT && collation.aggregate(item->collation))
    {
      my_error(ER_CANT_AGGREGATE_2COLLATIONS, MYF(0),
               old_cs, old_derivation,
               item->collation.collation->name,
               item->collation.derivation_name(),
               "UNION");
      return 1;
    }

    if (new_result_type == DECIMAL_RESULT)
    {
      int intp1= new_length - item->decimals;
      int intp2= max_length - decimals;
      max_length= max(intp1, intp2);
      decimals= max(decimals, item->decimals);
      /* can't be overflow because it work only for decimals (no strings) */
      max_length+= decimals;
    }
    else
    {
      max_length= max(max_length, new_length);
      decimals= max(decimals, item->decimals);
    }
    maybe_null|= item->maybe_null;
    item_type= new_result_type;
  }
  DBUG_ASSERT(item_type != ROW_RESULT);
  return 0;
}


uint32 Item_type_holder::real_length(Item *item)
{
  if (item->type() == Item::FIELD_ITEM)
    return ((Item_field *)item)->max_disp_length();

  switch (item->result_type()) {
  case STRING_RESULT:
  case DECIMAL_RESULT:
    return item->max_length;
  case REAL_RESULT:
    return 53;
  case INT_RESULT:
    return 20;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0); // we should never go there
    return 0;
  }
}

double Item_type_holder::val_real()
{
  DBUG_ASSERT(0); // should never be called
  return 0.0;
}


longlong Item_type_holder::val_int()
{
  DBUG_ASSERT(0); // should never be called
  return 0;
}

my_decimal *Item_type_holder::val_decimal(my_decimal *)
{
  DBUG_ASSERT(0); // should never be called
  return 0;
}

String *Item_type_holder::val_str(String*)
{
  DBUG_ASSERT(0); // should never be called
  return 0;
}

void Item_result_field::cleanup()
{
  DBUG_ENTER("Item_result_field::cleanup()");
  Item::cleanup();
  result_field= 0;
  DBUG_VOID_RETURN;
}

/*****************************************************************************
** Instantiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<Item>;
template class List_iterator<Item>;
template class List_iterator_fast<Item>;
template class List_iterator_fast<Item_field>;
template class List<List_item>;
#endif
