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


/* compare and test functions */

#include "assert.h"

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

extern Item_result item_cmp_type(Item_result a,Item_result b);
class Item_bool_func2;
class Arg_comparator;

typedef int (Arg_comparator::*arg_cmp_func)();

class Arg_comparator: public Sql_alloc
{
  Item **a, **b;
  arg_cmp_func func;
  Item_bool_func2 *owner;
  Arg_comparator *comparators;   // used only for compare_row()

public:
  Arg_comparator() {};
  Arg_comparator(Item **a1, Item **a2): a(a1), b(a2) {};

  int set_compare_func(Item_bool_func2 *owner, Item_result type);
  inline int set_compare_func(Item_bool_func2 *owner)
  {
    return set_compare_func(owner, item_cmp_type((*a)->result_type(),
						 (*b)->result_type()));
  }
  inline int set_cmp_func(Item_bool_func2 *owner,
			  Item **a1, Item **a2,
			  Item_result type)
  {
    a= a1;
    b= a2;
    return set_compare_func(owner, type);
  }
  inline int set_cmp_func(Item_bool_func2 *owner,
			  Item **a1, Item **a2)
  {
    return set_cmp_func(owner, a1, a2, item_cmp_type((*a1)->result_type(),
						     (*a2)->result_type()));
  }
  inline int compare() { return (this->*func)(); }

  int compare_string();		 // compare args[0] & args[1]
  int compare_real();            // compare args[0] & args[1]
  int compare_int();             // compare args[0] & args[1]
  int compare_row();             // compare args[0] & args[1]
  int compare_e_string();	 // compare args[0] & args[1]
  int compare_e_real();          // compare args[0] & args[1]
  int compare_e_int();           // compare args[0] & args[1]
  int compare_e_row();           // compare args[0] & args[1]

  static arg_cmp_func comparator_matrix [4][2];

  friend class Item_func;
};

class Item_bool_func :public Item_int_func
{
public:
  Item_bool_func() :Item_int_func() {}
  Item_bool_func(Item *a) :Item_int_func(a) {}
  Item_bool_func(Item *a,Item *b) :Item_int_func(a,b) {}
  void fix_length_and_dec() { decimals=0; max_length=1; }
};

class Item_cache;
class Item_in_optimizer: public Item_bool_func
{
protected:
  Item_cache *cache;
public:
  Item_in_optimizer(Item *a, Item_in_subselect *b):
    Item_bool_func(a, (Item *)b), cache(0) {}
  // used by row in transformer
  bool preallocate_row();
  bool fix_fields(THD *, struct st_table_list *, Item **);
  bool is_null();
  longlong val_int();
  
  Item_cache **get_cache() { return &cache; }
};

class Item_bool_func2 :public Item_int_func
{						/* Bool with 2 string args */
protected:
  Arg_comparator cmp;
  String tmp_value1,tmp_value2;
public:
  Item_bool_func2(Item *a,Item *b):
    Item_int_func(a,b), cmp(tmp_arg, tmp_arg+1) {}
  void fix_length_and_dec();
  void set_cmp_func()
  {
    cmp.set_cmp_func(this, tmp_arg, tmp_arg+1);
  }
  optimize_type select_optimize() const { return OPTIMIZE_OP; }
  virtual enum Functype rev_functype() const { return UNKNOWN_FUNC; }
  bool have_rev_func() const { return rev_functype() != UNKNOWN_FUNC; }
  void print(String *str) { Item_func::print_op(str); }
  bool is_null() { return test(args[0]->is_null() || args[1]->is_null()); }

  static Item_bool_func2* eq_creator(Item *a, Item *b);
  static Item_bool_func2* ne_creator(Item *a, Item *b);
  static Item_bool_func2* gt_creator(Item *a, Item *b);
  static Item_bool_func2* lt_creator(Item *a, Item *b);
  static Item_bool_func2* ge_creator(Item *a, Item *b);
  static Item_bool_func2* le_creator(Item *a, Item *b);

  friend class  Arg_comparator;
};

class Item_bool_rowready_func2 :public Item_bool_func2
{
public:
  Item_bool_rowready_func2(Item *a,Item *b) :Item_bool_func2(a,b)
  {
    allowed_arg_cols= a->cols();
  }
};

class Item_func_not :public Item_bool_func
{
public:
  Item_func_not(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  const char *func_name() const { return "not"; }
};

class Item_func_eq :public Item_bool_rowready_func2
{
public:
  Item_func_eq(Item *a,Item *b) :Item_bool_rowready_func2(a,b) { };
  longlong val_int();
  enum Functype functype() const { return EQ_FUNC; }
  enum Functype rev_functype() const { return EQ_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "="; }
};

class Item_func_equal :public Item_bool_rowready_func2
{
public:
  Item_func_equal(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {};
  longlong val_int();
  void fix_length_and_dec();
  enum Functype functype() const { return EQUAL_FUNC; }
  enum Functype rev_functype() const { return EQUAL_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "<=>"; }
};


class Item_func_ge :public Item_bool_rowready_func2
{
public:
  Item_func_ge(Item *a,Item *b) :Item_bool_rowready_func2(a,b) { };
  longlong val_int();
  enum Functype functype() const { return GE_FUNC; }
  enum Functype rev_functype() const { return LE_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return ">="; }
};


class Item_func_gt :public Item_bool_rowready_func2
{
public:
  Item_func_gt(Item *a,Item *b) :Item_bool_rowready_func2(a,b) { };
  longlong val_int();
  enum Functype functype() const { return GT_FUNC; }
  enum Functype rev_functype() const { return LT_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  const char *func_name() const { return ">"; }
};


class Item_func_le :public Item_bool_rowready_func2
{
public:
  Item_func_le(Item *a,Item *b) :Item_bool_rowready_func2(a,b) { };
  longlong val_int();
  enum Functype functype() const { return LE_FUNC; }
  enum Functype rev_functype() const { return GE_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "<="; }
};


class Item_func_lt :public Item_bool_rowready_func2
{
public:
  Item_func_lt(Item *a,Item *b) :Item_bool_rowready_func2(a,b) { }
  longlong val_int();
  enum Functype functype() const { return LT_FUNC; }
  enum Functype rev_functype() const { return GT_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  const char *func_name() const { return "<"; }
};


class Item_func_ne :public Item_bool_rowready_func2
{
public:
  Item_func_ne(Item *a,Item *b) :Item_bool_rowready_func2(a,b) { }
  longlong val_int();
  enum Functype functype() const { return NE_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "<>"; }
};


class Item_func_between :public Item_int_func
{
  int (*string_compare)(const String *x,const String *y);
public:
  Item_result cmp_type;
  String value0,value1,value2;
  Item_func_between(Item *a,Item *b,Item *c) :Item_int_func(a,b,c) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_KEY; }
  enum Functype functype() const   { return BETWEEN; }
  const char *func_name() const { return "between"; }
  void fix_length_and_dec();
};


class Item_func_strcmp :public Item_bool_func2
{
public:
  Item_func_strcmp(Item *a,Item *b) :Item_bool_func2(a,b) {}
  longlong val_int();
  void fix_length_and_dec() { max_length=2; }
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "strcmp"; }
};


class Item_func_interval :public Item_int_func
{
  Item *item;
  double *intervals;
public:
  Item_func_interval(Item *a,List<Item> &list)
    :Item_int_func(list),item(a),intervals(0) {}
  longlong val_int();
  bool fix_fields(THD *thd, struct st_table_list *tlist, Item **ref)
  {
    return (item->check_cols(1) || 
	    item->fix_fields(thd, tlist, &item) || 
	    Item_func::fix_fields(thd, tlist, ref));
  }
  void fix_length_and_dec();
  ~Item_func_interval() { delete item; }
  const char *func_name() const { return "interval"; }
  void update_used_tables();
  bool check_loop(uint id);
  void set_outer_resolving()
  {
    item->set_outer_resolving();
    Item_func::set_outer_resolving();
  }
};


class Item_func_ifnull :public Item_func
{
  enum Item_result cached_result_type;
public:
  Item_func_ifnull(Item *a,Item *b) :Item_func(a,b) { }
  double val();
  longlong val_int();
  String *val_str(String *str);
  enum Item_result result_type () const { return cached_result_type; }
  void fix_length_and_dec();
  const char *func_name() const { return "ifnull"; }
};


class Item_func_if :public Item_func
{
  enum Item_result cached_result_type;
public:
  Item_func_if(Item *a,Item *b,Item *c) :Item_func(a,b,c) { }
  double val();
  longlong val_int();
  String *val_str(String *str);
  enum Item_result result_type () const { return cached_result_type; }
  bool fix_fields(THD *thd,struct st_table_list *tlist, Item **ref)
  {
    args[0]->top_level_item();
    return Item_func::fix_fields(thd, tlist, ref);
  }
  void fix_length_and_dec();
  const char *func_name() const { return "if"; }
};


class Item_func_nullif :public Item_bool_func2
{
  enum Item_result cached_result_type;
public:
  Item_func_nullif(Item *a,Item *b) :Item_bool_func2(a,b) { }
  double val();
  longlong val_int();
  String *val_str(String *str);
  enum Item_result result_type () const { return cached_result_type; }
  void fix_length_and_dec();
  const char *func_name() const { return "nullif"; }
};


class Item_func_coalesce :public Item_func
{
  enum Item_result cached_result_type;
public:
  Item_func_coalesce(List<Item> &list) :Item_func(list) {}
  double val();
  longlong val_int();
  String *val_str(String *);
  void fix_length_and_dec();
  enum Item_result result_type () const { return cached_result_type; }
  const char *func_name() const { return "coalesce"; }
};

class Item_func_case :public Item_func
{
  Item * first_expr, *else_expr;
  enum Item_result cached_result_type;
  String tmp_value;
public:
  Item_func_case(List<Item> &list, Item *first_expr_, Item *else_expr_)
    :Item_func(list), first_expr(first_expr_), else_expr(else_expr_) {}
  double val();
  longlong val_int();
  String *val_str(String *);
  void fix_length_and_dec();
  void update_used_tables();
  enum Item_result result_type () const { return cached_result_type; }
  const char *func_name() const { return "case"; }
  void print(String *str);
  bool fix_fields(THD *thd, struct st_table_list *tlist, Item **ref);
  Item *find_item(String *str);
  bool check_loop(uint id);
  void set_outer_resolving();
};


/* Functions to handle the optimized IN */

class in_vector :public Sql_alloc
{
 protected:
  char *base;
  uint size;
  qsort_cmp compare;
  uint count;
public:
  uint used_count;
  in_vector() {}
  in_vector(uint elements,uint element_length,qsort_cmp cmp_func)
    :base((char*) sql_calloc(elements*element_length)),
     size(element_length), compare(cmp_func), count(elements),
     used_count(elements) {}
  virtual ~in_vector() {}
  virtual void set(uint pos,Item *item)=0;
  virtual byte *get_value(Item *item)=0;
  void sort()
  {
    qsort(base,used_count,size,compare);
  }
  int find(Item *item);
};

class in_string :public in_vector
{
  char buff[80];
  String tmp;
public:
  in_string(uint elements,qsort_cmp cmp_func);
  ~in_string();
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
};

class in_longlong :public in_vector
{
  longlong tmp;
public:
  in_longlong(uint elements);
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
};

class in_double :public in_vector
{
  double tmp;
public:
  in_double(uint elements);
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
};

/*
** Classes for easy comparing of non const items
*/

class cmp_item :public Sql_alloc
{
public:
  cmp_item() {}
  virtual ~cmp_item() {}
  virtual void store_value(Item *item)= 0;
  virtual int cmp(Item *item)= 0;
  // for optimized IN with row
  virtual int compare(cmp_item *item)= 0;
  static cmp_item* get_comparator(Item *);
  virtual cmp_item *make_same()= 0;
  virtual void store_value_by_template(cmp_item *tmpl, Item *item)
  {
    store_value(item);
  }
};

typedef int (*str_cmp_func_pointer)(const String *, const String *);
class cmp_item_string :public cmp_item 
{
protected:
  str_cmp_func_pointer str_cmp_func;
  String *value_res;
public:
  cmp_item_string (str_cmp_func_pointer cmp): str_cmp_func(cmp) {}
  friend class cmp_item_sort_string;
  friend class cmp_item_binary_string;
  friend class cmp_item_sort_string_in_static;
  friend class cmp_item_binary_string_in_static;
};

class cmp_item_sort_string :public cmp_item_string
{
protected:
  char value_buff[80];
  String value;
public:
  cmp_item_sort_string(str_cmp_func_pointer cmp):
    cmp_item_string(cmp),
    value(value_buff, sizeof(value_buff), default_charset_info) {}
  cmp_item_sort_string():
    cmp_item_string(&sortcmp),
    value(value_buff, sizeof(value_buff), default_charset_info) {}
  void store_value(Item *item)
  {
    value_res= item->val_str(&value);
  }
  int cmp(Item *arg)
  {
    char buff[80];
    String tmp(buff, sizeof(buff), default_charset_info), *res;
    if (!(res= arg->val_str(&tmp)))
      return 1;				/* Can't be right */
    return (*str_cmp_func)(value_res, res);
  }
  int compare(cmp_item *c)
  {
    cmp_item_string *cmp= (cmp_item_string *)c;
    return (*str_cmp_func)(value_res, cmp->value_res);
  } 
  cmp_item *make_same();
};

class cmp_item_binary_string :public cmp_item_sort_string {
public:
  cmp_item_binary_string(): cmp_item_sort_string(&stringcmp)  {}
  cmp_item *make_same();
};

class cmp_item_int :public cmp_item
{
  longlong value;
public:
  void store_value(Item *item)
  {
    value= item->val_int();
  }
  int cmp(Item *arg)
  {
    return value != arg->val_int();
  }
  int compare(cmp_item *c)
  {
    cmp_item_int *cmp= (cmp_item_int *)c;
    return (value < cmp->value) ? -1 : ((value == cmp->value) ? 0 : 1);
  }
  cmp_item *make_same();
};

class cmp_item_real :public cmp_item
{
  double value;
public:
  void store_value(Item *item)
  {
    value= item->val();
  }
  int cmp(Item *arg)
  {
    return value != arg->val();
  }
  int compare(cmp_item *c)
  {
    cmp_item_real *cmp= (cmp_item_real *)c;
    return (value < cmp->value)? -1 : ((value == cmp->value) ? 0 : 1);
  }
  cmp_item *make_same();
};

class cmp_item_row :public cmp_item
{
  cmp_item **comparators;
  uint n;
public:
  cmp_item_row(): comparators(0), n(0) {}
  ~cmp_item_row()
  {
    if(comparators)
      for(uint i= 0; i < n; i++)
	if (comparators[i])
	  delete comparators[i];
  }
  void store_value(Item *item);
  int cmp(Item *arg);
  int compare(cmp_item *arg);
  cmp_item *make_same();
  void store_value_by_template(cmp_item *tmpl, Item *);
};


class in_row :public in_vector
{
  cmp_item_row tmp;
public:
  in_row(uint elements, Item *);
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
};

/* 
   cmp_item for optimized IN with row (right part string, which never
   be changed)
*/

class cmp_item_sort_string_in_static :public cmp_item_string
{
 protected:
  String value;
public:
  cmp_item_sort_string_in_static(str_cmp_func_pointer cmp):
    cmp_item_string(cmp) {}
  cmp_item_sort_string_in_static(): cmp_item_string(&sortcmp) {}
  void store_value(Item *item)
  {
    value_res= item->val_str(&value);
  }
  int cmp(Item *item)
  {
    // Should never be called
    DBUG_ASSERT(0);
    return 1;
  }
  int compare(cmp_item *c)
  {
    cmp_item_string *cmp= (cmp_item_string *)c;
    return (*str_cmp_func)(value_res, cmp->value_res);
  }
  cmp_item * make_same()
  {
    return new cmp_item_sort_string_in_static();
  }
};

class cmp_item_binary_string_in_static :public cmp_item_sort_string_in_static {
public:
  cmp_item_binary_string_in_static():
    cmp_item_sort_string_in_static(&stringcmp) {}
  cmp_item * make_same()
  {
    return new cmp_item_binary_string_in_static();
  }
};

class Item_func_in :public Item_int_func
{
  Item *item;
  in_vector *array;
  cmp_item *in_item;
  bool have_null;
 public:
  Item_func_in(Item *a,List<Item> &list)
    :Item_int_func(list), item(a), array(0), in_item(0), have_null(0)
  {
    allowed_arg_cols= item->cols();
  }
  longlong val_int();
  bool fix_fields(THD *thd, struct st_table_list *tlist, Item **ref)
  {
    // We do not check item->cols(), because allowed_arg_cols assigned from it
    bool res=(item->fix_fields(thd, tlist, &item) ||
	      Item_func::fix_fields(thd, tlist, ref));
    with_sum_func= with_sum_func || item->with_sum_func;
    return res;
  }
  void fix_length_and_dec();
  ~Item_func_in() { delete item; delete array; delete in_item; }
  optimize_type select_optimize() const
    { return array ? OPTIMIZE_KEY : OPTIMIZE_NONE; }
  Item *key_item() const { return item; }
  void print(String *str);
  enum Functype functype() const { return IN_FUNC; }
  const char *func_name() const { return " IN "; }
  void update_used_tables();
  void split_sum_func(List<Item> &fields);
  bool check_loop(uint id)
  {
    DBUG_ENTER("Item_func_in::check_loop");
    if (Item_func::check_loop(id))
      DBUG_RETURN(1);
    DBUG_RETURN(item->check_loop(id));
  }
  bool nulls_in_row();
  void set_outer_resolving()
  {
    item->set_outer_resolving();
    Item_int_func::set_outer_resolving();
  }
};

/* Functions used by where clause */

class Item_func_isnull :public Item_bool_func
{
  longlong cached_value;
public:
  Item_func_isnull(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  enum Functype functype() const { return ISNULL_FUNC; }
  void fix_length_and_dec()
  {
    decimals=0; max_length=1; maybe_null=0;
    Item_func_isnull::update_used_tables();
  }
  const char *func_name() const { return "isnull"; }
  /* Optimize case of not_null_column IS NULL */
  void update_used_tables()
  {
    if (!args[0]->maybe_null)
      used_tables_cache=0;			/* is always false */
    else
    {
      args[0]->update_used_tables();
      used_tables_cache=args[0]->used_tables();
    }
    if (!used_tables_cache)
    {
      /* Remember if the value is always NULL or never NULL */
      args[0]->val();
      cached_value= args[0]->null_value ? (longlong) 1 : (longlong) 0;
    }
  }
  optimize_type select_optimize() const { return OPTIMIZE_NULL; }
};

class Item_func_isnotnull :public Item_bool_func
{
public:
  Item_func_isnotnull(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  enum Functype functype() const { return ISNOTNULL_FUNC; }
  void fix_length_and_dec()
  {
    decimals=0; max_length=1; maybe_null=0;
  }
  const char *func_name() const { return "isnotnull"; }
  optimize_type select_optimize() const { return OPTIMIZE_NULL; }
};

class Item_func_like :public Item_bool_func2
{
  char escape;

  // Turbo Boyer-Moore data
  bool        canDoTurboBM;	// pattern is '%abcd%' case
  const char* pattern;
  int         pattern_len;

  // TurboBM buffers, *this is owner
  int* bmGs; //   good suffix shift table, size is pattern_len + 1
  int* bmBc; // bad character shift table, size is alphabet_size

  void turboBM_compute_suffixes(int* suff);
  void turboBM_compute_good_suffix_shifts(int* suff);
  void turboBM_compute_bad_character_shifts();
  bool turboBM_matches(const char* text, int text_len) const;
  enum { alphabet_size = 256 };

public:
  Item_func_like(Item *a,Item *b, char* escape_arg)
    :Item_bool_func2(a,b), escape(*escape_arg), canDoTurboBM(false),
    pattern(0), pattern_len(0), bmGs(0), bmBc(0)
  {}
  longlong val_int();
  enum Functype functype() const { return LIKE_FUNC; }
  optimize_type select_optimize() const;
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "like"; }
  void fix_length_and_dec();
  bool fix_fields(THD *thd, struct st_table_list *tlist, Item **ref);
};

#ifdef USE_REGEX

#include <regex.h>

class Item_func_regex :public Item_bool_func
{
  regex_t preg;
  bool regex_compiled;
  bool regex_is_const;
  String prev_regexp;
public:
  Item_func_regex(Item *a,Item *b) :Item_bool_func(a,b),
    regex_compiled(0),regex_is_const(0) {}
  ~Item_func_regex();
  longlong val_int();
  bool fix_fields(THD *thd, struct st_table_list *tlist, Item **ref);
  const char *func_name() const { return "regex"; }
};

#else

class Item_func_regex :public Item_bool_func
{
public:
  Item_func_regex(Item *a,Item *b) :Item_bool_func(a,b) {}
  longlong val_int() { return 0;}
  const char *func_name() const { return "regex"; }
};

#endif /* USE_REGEX */


typedef class Item COND;

class Item_cond :public Item_bool_func
{
protected:
  List<Item> list;
  bool abort_on_null;
public:
  /* Item_cond() is only used to create top level items */
  Item_cond() : Item_bool_func(), abort_on_null(1) { const_item_cache=0; }
  Item_cond(Item *i1,Item *i2) :Item_bool_func(), abort_on_null(0)
  { list.push_back(i1); list.push_back(i2); }
  ~Item_cond() { list.delete_elements(); }
  bool add(Item *item) { return list.push_back(item); }
  bool fix_fields(THD *, struct st_table_list *, Item **ref);

  enum Type type() const { return COND_ITEM; }
  List<Item>* argument_list() { return &list; }
  table_map used_tables() const;
  void update_used_tables();
  void print(String *str);
  void split_sum_func(List<Item> &fields);
  friend int setup_conds(THD *thd,TABLE_LIST *tables,COND **conds);
  bool check_loop(uint id);
  void top_level_item() { abort_on_null=1; }
  void set_outer_resolving();
};


class Item_cond_and :public Item_cond
{
public:
  Item_cond_and() :Item_cond() {}
  Item_cond_and(Item *i1,Item *i2) :Item_cond(i1,i2) {}
  enum Functype functype() const { return COND_AND_FUNC; }
  longlong val_int();
  const char *func_name() const { return "and"; }
};

class Item_cond_or :public Item_cond
{
public:
  Item_cond_or() :Item_cond() {}
  Item_cond_or(Item *i1,Item *i2) :Item_cond(i1,i2) {}
  enum Functype functype() const { return COND_OR_FUNC; }
  longlong val_int();
  const char *func_name() const { return "or"; }
};


class Item_cond_xor :public Item_cond
{
public:
  Item_cond_xor() :Item_cond() {}
  Item_cond_xor(Item *i1,Item *i2) :Item_cond(i1,i2) {}
  enum Functype functype() const { return COND_XOR_FUNC; }
  longlong val_int();
  const char *func_name() const { return "xor"; }
};


/* Some usefull inline functions */

inline Item *and_conds(Item *a,Item *b)
{
  if (!b) return a;
  if (!a) return b;
  Item *cond=new Item_cond_and(a,b);
  if (cond)
    cond->update_used_tables();
  return cond;
}

Item *and_expressions(Item *a, Item *b, Item **org_item);

/**************************************************************
  Spatial relations
***************************************************************/

class Item_func_spatial_rel :public Item_bool_func2
{
  enum Functype spatial_rel;
public:
  Item_func_spatial_rel(Item *a,Item *b, enum Functype sp_rel) :
    Item_bool_func2(a,b) { spatial_rel = sp_rel; }
  longlong val_int();
  enum Functype functype() const 
  { 
    switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      return SP_WITHIN_FUNC;
    case SP_WITHIN_FUNC:
      return SP_CONTAINS_FUNC;
    default:
      return spatial_rel;
    }
  }
  enum Functype rev_functype() const { return spatial_rel; }
  const char *func_name() const 
  { 
    switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      return "contains";
    case SP_WITHIN_FUNC:
      return "within";
    case SP_EQUALS_FUNC:
      return "equals";
    case SP_DISJOINT_FUNC:
      return "disjoint";
    case SP_INTERSECTS_FUNC:
      return "intersects";
    case SP_TOUCHES_FUNC:
      return "touches";
    case SP_CROSSES_FUNC:
      return "crosses";
    case SP_OVERLAPS_FUNC:
      return "overlaps";
    default:
      return "sp_unknown"; 
    }
    }
};


class Item_func_isempty :public Item_bool_func
{
public:
  Item_func_isempty(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "isempty"; }
};

class Item_func_issimple :public Item_bool_func
{
public:
  Item_func_issimple(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "issimple"; }
};

class Item_func_isclosed :public Item_bool_func
{
public:
  Item_func_isclosed(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "isclosed"; }
};
