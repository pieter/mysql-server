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

/*
  TODO:
  Fix that MAYBE_KEY are stored in the tree so that we can detect use
  of full hash keys for queries like:

  select s.id, kws.keyword_id from sites as s,kws where s.id=kws.site_id and kws.keyword_id in (204,205);

*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include <nisam.h>
#include "sql_select.h"

#ifndef EXTRA_DEBUG
#define test_rb_tree(A,B) {}
#define test_use_count(A) {}
#endif


static int sel_cmp(Field *f,char *a,char *b,uint8 a_flag,uint8 b_flag);

static char is_null_string[2]= {1,0};

class SEL_ARG :public Sql_alloc
{
public:
  uint8 min_flag,max_flag,maybe_flag;
  uint8 part;					// Which key part
  uint8 maybe_null;
  uint16 elements;				// Elements in tree
  ulong use_count;				// use of this sub_tree
  Field *field;
  char *min_value,*max_value;			// Pointer to range

  SEL_ARG *left,*right,*next,*prev,*parent,*next_key_part;
  enum leaf_color { BLACK,RED } color;
  enum Type { IMPOSSIBLE, MAYBE, MAYBE_KEY, KEY_RANGE } type;

  SEL_ARG() {}
  SEL_ARG(SEL_ARG &);
  SEL_ARG(Field *,const char *,const char *);
  SEL_ARG(Field *field, uint8 part, char *min_value, char *max_value,
	  uint8 min_flag, uint8 max_flag, uint8 maybe_flag);
  SEL_ARG(enum Type type_arg)
    :elements(1),use_count(1),left(0),next_key_part(0),color(BLACK),
     type(type_arg)
  {}
  inline bool is_same(SEL_ARG *arg)
  {
    if (type != arg->type)
      return 0;
    if (type != KEY_RANGE)
      return 1;
    return cmp_min_to_min(arg) == 0 && cmp_max_to_max(arg) == 0;
  }
  inline void merge_flags(SEL_ARG *arg) { maybe_flag|=arg->maybe_flag; }
  inline void maybe_smaller() { maybe_flag=1; }
  inline int cmp_min_to_min(SEL_ARG* arg)
  {
    return sel_cmp(field,min_value, arg->min_value, min_flag, arg->min_flag);
  }
  inline int cmp_min_to_max(SEL_ARG* arg)
  {
    return sel_cmp(field,min_value, arg->max_value, min_flag, arg->max_flag);
  }
  inline int cmp_max_to_max(SEL_ARG* arg)
  {
    return sel_cmp(field,max_value, arg->max_value, max_flag, arg->max_flag);
  }
  inline int cmp_max_to_min(SEL_ARG* arg)
  {
    return sel_cmp(field,max_value, arg->min_value, max_flag, arg->min_flag);
  }
  SEL_ARG *clone_and(SEL_ARG* arg)
  {						// Get overlapping range
    char *new_min,*new_max;
    uint8 flag_min,flag_max;
    if (cmp_min_to_min(arg) >= 0)
    {
      new_min=min_value; flag_min=min_flag;
    }
    else
    {
      new_min=arg->min_value; flag_min=arg->min_flag; /* purecov: deadcode */
    }
    if (cmp_max_to_max(arg) <= 0)
    {
      new_max=max_value; flag_max=max_flag;
    }
    else
    {
      new_max=arg->max_value; flag_max=arg->max_flag;
    }
    return new SEL_ARG(field, part, new_min, new_max, flag_min, flag_max,
		       test(maybe_flag && arg->maybe_flag));
  }
  SEL_ARG *clone_first(SEL_ARG *arg)
  {						// min <= X < arg->min
    return new SEL_ARG(field,part, min_value, arg->min_value,
		       min_flag, arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX,
		       maybe_flag | arg->maybe_flag);
  }
  SEL_ARG *clone_last(SEL_ARG *arg)
  {						// min <= X <= key_max
    return new SEL_ARG(field, part, min_value, arg->max_value,
		       min_flag, arg->max_flag, maybe_flag | arg->maybe_flag);
  }
  SEL_ARG *clone(SEL_ARG *new_parent,SEL_ARG **next);

  bool copy_min(SEL_ARG* arg)
  {						// Get overlapping range
    if (cmp_min_to_min(arg) > 0)
    {
      min_value=arg->min_value; min_flag=arg->min_flag;
      if ((max_flag & (NO_MAX_RANGE | NO_MIN_RANGE)) ==
	  (NO_MAX_RANGE | NO_MIN_RANGE))
	return 1;				// Full range
    }
    maybe_flag|=arg->maybe_flag;
    return 0;
  }
  bool copy_max(SEL_ARG* arg)
  {						// Get overlapping range
    if (cmp_max_to_max(arg) <= 0)
    {
      max_value=arg->max_value; max_flag=arg->max_flag;
      if ((max_flag & (NO_MAX_RANGE | NO_MIN_RANGE)) ==
	  (NO_MAX_RANGE | NO_MIN_RANGE))
	return 1;				// Full range
    }
    maybe_flag|=arg->maybe_flag;
    return 0;
  }

  void copy_min_to_min(SEL_ARG *arg)
  {
    min_value=arg->min_value; min_flag=arg->min_flag;
  }
  void copy_min_to_max(SEL_ARG *arg)
  {
    max_value=arg->min_value;
    max_flag=arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX;
  }
  void copy_max_to_min(SEL_ARG *arg)
  {
    min_value=arg->max_value;
    min_flag=arg->max_flag & NEAR_MAX ? 0 : NEAR_MIN;
  }
  void store(uint length,char **min_key,uint min_key_flag,
	     char **max_key, uint max_key_flag)
  {
    if ((min_flag & GEOM_FLAG) ||
        (!(min_flag & NO_MIN_RANGE) &&
	!(min_key_flag & (NO_MIN_RANGE | NEAR_MIN))))
    {
      if (maybe_null && *min_value)
      {
	**min_key=1;
	bzero(*min_key+1,length);
      }
      else
	memcpy(*min_key,min_value,length+(int) maybe_null);
      (*min_key)+= length+(int) maybe_null;
    }
    if (!(max_flag & NO_MAX_RANGE) &&
	!(max_key_flag & (NO_MAX_RANGE | NEAR_MAX)))
    {
      if (maybe_null && *max_value)
      {
	**max_key=1;
	bzero(*max_key+1,length);
      }
      else
	memcpy(*max_key,max_value,length+(int) maybe_null);
      (*max_key)+= length+(int) maybe_null;
    }
  }

  void store_min_key(KEY_PART *key,char **range_key, uint *range_key_flag)
  {
    SEL_ARG *key_tree= first();
    key_tree->store(key[key_tree->part].part_length,
		    range_key,*range_key_flag,range_key,NO_MAX_RANGE);
    *range_key_flag|= key_tree->min_flag;
    if (key_tree->next_key_part &&
	key_tree->next_key_part->part == key_tree->part+1 &&
	!(*range_key_flag & (NO_MIN_RANGE | NEAR_MIN)) &&
	key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
      key_tree->next_key_part->store_min_key(key,range_key, range_key_flag);
  }

  void store_max_key(KEY_PART *key,char **range_key, uint *range_key_flag)
  {
    SEL_ARG *key_tree= last();
    key_tree->store(key[key_tree->part].part_length,
		    range_key, NO_MIN_RANGE, range_key,*range_key_flag);
    (*range_key_flag)|= key_tree->max_flag;
    if (key_tree->next_key_part &&
	key_tree->next_key_part->part == key_tree->part+1 &&
	!(*range_key_flag & (NO_MAX_RANGE | NEAR_MAX)) &&
	key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
      key_tree->next_key_part->store_max_key(key,range_key, range_key_flag);
  }

  SEL_ARG *insert(SEL_ARG *key);
  SEL_ARG *tree_delete(SEL_ARG *key);
  SEL_ARG *find_range(SEL_ARG *key);
  SEL_ARG *rb_insert(SEL_ARG *leaf);
  friend SEL_ARG *rb_delete_fixup(SEL_ARG *root,SEL_ARG *key, SEL_ARG *par);
#ifdef EXTRA_DEBUG
  friend int test_rb_tree(SEL_ARG *element,SEL_ARG *parent);
  void test_use_count(SEL_ARG *root);
#endif
  SEL_ARG *first();
  SEL_ARG *last();
  void make_root();
  inline bool simple_key()
  {
    return !next_key_part && elements == 1;
  }
  void increment_use_count(long count)
  {
    if (next_key_part)
    {
      next_key_part->use_count+=count;
      count*= (next_key_part->use_count-count);
      for (SEL_ARG *pos=next_key_part->first(); pos ; pos=pos->next)
	if (pos->next_key_part)
	  pos->increment_use_count(count);
    }
  }
  void free_tree()
  {
    for (SEL_ARG *pos=first(); pos ; pos=pos->next)
      if (pos->next_key_part)
      {
	pos->next_key_part->use_count--;
	pos->next_key_part->free_tree();
      }
  }

  inline SEL_ARG **parent_ptr()
  {
    return parent->left == this ? &parent->left : &parent->right;
  }
  SEL_ARG *clone_tree();
};

class SEL_IMERGE;

class SEL_TREE :public Sql_alloc
{
public:
  enum Type { IMPOSSIBLE, ALWAYS, MAYBE, KEY, KEY_SMALLER } type;
  SEL_TREE(enum Type type_arg) :type(type_arg) {}
  SEL_TREE() :type(KEY) { keys_map.clear_all(); bzero((char*) keys,sizeof(keys));}
  SEL_ARG *keys[MAX_KEY];
  key_map keys_map;        /* bitmask of non-NULL elements in keys         */
  List<SEL_IMERGE> merges; /* possible ways to read rows using index_merge */
};


typedef struct st_qsel_param {
  THD	*thd;
  TABLE *table;
  KEY_PART *key_parts,*key_parts_end,*key[MAX_KEY];
  MEM_ROOT *mem_root;
  table_map prev_tables,read_tables,current_table;
  uint baseflag, keys, max_key_part, range_count;
  uint real_keynr[MAX_KEY];
  char min_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH],
    max_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH];
  bool quick;				// Don't calulate possible keys
} PARAM;

static SEL_TREE * get_mm_parts(PARAM *param,Field *field,
			       Item_func::Functype type,Item *value,
			       Item_result cmp_type);
static SEL_ARG *get_mm_leaf(PARAM *param,Field *field,KEY_PART *key_part,
			    Item_func::Functype type,Item *value);
static SEL_TREE *get_mm_tree(PARAM *param,COND *cond);
static ha_rows check_quick_select(PARAM *param,uint index,SEL_ARG *key_tree);
static ha_rows check_quick_keys(PARAM *param,uint index,SEL_ARG *key_tree,
				char *min_key,uint min_key_flag,
				char *max_key, uint max_key_flag);

QUICK_RANGE_SELECT *get_quick_select(PARAM *param,uint index,
                                     SEL_ARG *key_tree, MEM_ROOT *alloc = NULL);
static int get_quick_select_params(SEL_TREE *tree, PARAM& param,
                                   key_map& needed_reg, TABLE *head,
                                   bool index_read_can_be_used,
                                   double* read_time, 
                                   ha_rows* records,
                                   SEL_ARG*** key_to_read);
#ifndef DBUG_OFF
static void print_quick_sel_imerge(QUICK_INDEX_MERGE_SELECT *quick,
                                   const key_map *needed_reg);
void print_quick_sel_range(QUICK_RANGE_SELECT *quick, 
                                  const key_map *needed_reg);

#endif
static SEL_TREE *tree_and(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2);
static SEL_TREE *tree_or(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2);
static SEL_ARG *sel_add(SEL_ARG *key1,SEL_ARG *key2);
static SEL_ARG *key_or(SEL_ARG *key1,SEL_ARG *key2);
static SEL_ARG *key_and(SEL_ARG *key1,SEL_ARG *key2,uint clone_flag);
static bool get_range(SEL_ARG **e1,SEL_ARG **e2,SEL_ARG *root1);
bool get_quick_keys(PARAM *param,QUICK_RANGE_SELECT *quick,KEY_PART *key,
			   SEL_ARG *key_tree,char *min_key,uint min_key_flag,
			   char *max_key,uint max_key_flag);
static bool eq_tree(SEL_ARG* a,SEL_ARG *b);

static SEL_ARG null_element(SEL_ARG::IMPOSSIBLE);
static bool null_part_in_key(KEY_PART *key_part, const char *key, uint length);
bool sel_trees_can_be_ored(SEL_TREE *tree1, SEL_TREE *tree2, PARAM* param);


/*
  SEL_IMERGE is a list of possible ways to do index merge, i.e. it is 
  a condition in the following form:
   (t_1||t_2||...||t_N) && (next) 

  where all t_i are SEL_TREEs, next is another SEL_IMERGE and no pair 
  (t_i,t_j) contains SEL_ARGS for the same index.

  SEL_TREE contained in SEL_IMERGE always has merges=NULL.

  This class relies on memory manager to do the cleanup.
*/

class SEL_IMERGE : public Sql_alloc
{
  enum { PREALLOCED_TREES= 10};
public:
  SEL_TREE *trees_prealloced[PREALLOCED_TREES];  
  SEL_TREE **trees;             /* trees used to do index_merge   */
  SEL_TREE **trees_next;        /* last of these trees            */
  SEL_TREE **trees_end;         /* end of allocated space         */

  SEL_ARG  ***best_keys;        /* best keys to read in SEL_TREEs */

  SEL_IMERGE() :
    trees(&trees_prealloced[0]),
    trees_next(trees),
    trees_end(trees + PREALLOCED_TREES)
  {}
  int or_sel_tree(PARAM *param, SEL_TREE *tree);
  int or_sel_tree_with_checks(PARAM *param, SEL_TREE *new_tree);
  int or_sel_imerge_with_checks(PARAM *param, SEL_IMERGE* imerge);
};


/* 
  Add SEL_TREE to this index_merge without any checks,

  NOTES 
    This function implements the following: 
      (x_1||...||x_N) || t = (x_1||...||x_N||t), where x_i, t are SEL_TREEs

  RETURN
     0 - OK
    -1 - Out of memory.
*/

int SEL_IMERGE::or_sel_tree(PARAM *param, SEL_TREE *tree)
{
  if (trees_next == trees_end)
  {
    const int realloc_ratio= 2;		/* Double size for next round */
    uint old_elements= (trees_end - trees);
    uint old_size= sizeof(SEL_TREE**) * old_elements;
    uint new_size= old_size * realloc_ratio;
    SEL_TREE **new_trees;
    if (!(new_trees= (SEL_TREE**)alloc_root(param->mem_root, new_size)))
      return -1;
    memcpy(new_trees, trees, old_size);
    trees=      new_trees;
    trees_next= trees + old_elements;
    trees_end=  trees + old_elements * realloc_ratio;
  }
  *(trees_next++)= tree;
  return 0;
}


/*
  Perform OR operation on this SEL_IMERGE and supplied SEL_TREE new_tree,
  combining new_tree with one of the trees in this SEL_IMERGE if they both
  have SEL_ARGs for the same key.
 
  SYNOPSIS
    or_sel_tree_with_checks()
      param    PARAM from SQL_SELECT::test_quick_select
      new_tree SEL_TREE with type KEY or KEY_SMALLER.

  NOTES 
    This does the following:
    (t_1||...||t_k)||new_tree = 
     either 
       = (t_1||...||t_k||new_tree)
     or
       = (t_1||....||(t_j|| new_tree)||...||t_k),
    
     where t_i, y are SEL_TREEs.
    new_tree is combined with the first t_j it has a SEL_ARG on common 
    key with. As a consequence of this, choice of keys to do index_merge 
    read may depend on the order of conditions in WHERE part of the query.

  RETURN 
    0  OK
    1  One of the trees was combined with new_tree to SEL_TREE::ALWAYS, 
       and (*this) should be discarded.
   -1  An error occurred.
*/

int SEL_IMERGE::or_sel_tree_with_checks(PARAM *param, SEL_TREE *new_tree)
{
  for (SEL_TREE** tree = trees;
       tree != trees_next;
       tree++)
  {
    if (sel_trees_can_be_ored(*tree, new_tree, param))
    {
      *tree = tree_or(param, *tree, new_tree);
      if (!*tree)
        return 1;
      if (((*tree)->type == SEL_TREE::MAYBE) ||
          ((*tree)->type == SEL_TREE::ALWAYS))
        return 1;
      /* SEL_TREE::IMPOSSIBLE is impossible here */
      return 0;
    }
  }

  /* new tree cannot be combined with any of existing trees */
  return or_sel_tree(param, new_tree);
}


/*
  Perform OR operation on this index_merge and supplied index_merge list.

  RETURN
    0 - OK
    1 - One of conditions in result is always TRUE and this SEL_IMERGE 
        should be discarded.
   -1 - An error occurred
*/

int SEL_IMERGE::or_sel_imerge_with_checks(PARAM *param, SEL_IMERGE* imerge)
{
  for (SEL_TREE** tree= imerge->trees;
       tree != imerge->trees_next;
       tree++)
  {
    if (or_sel_tree_with_checks(param, *tree))
      return 1;
  }
  return 0;
}


/* 
  Perform AND operation on two index_merge lists, storing result in *im1.

*/

inline void imerge_list_and_list(List<SEL_IMERGE> *im1, List<SEL_IMERGE> *im2)
{
  im1->concat(im2);
}


/*
  Perform OR operation on 2 index_merge lists, storing result in first list.

  NOTES 
    The following conversion is implemented:
     (a_1 &&...&& a_N)||(b_1 &&...&& b_K) = AND_i,j(a_i || b_j) =>
      => (a_1||b_1).
     
    i.e. all conjuncts except the first one are currently dropped. 
    This is done to avoid producing N*K ways to do index_merge.

    If (a_1||b_1) produce a condition that is always true, NULL is 
    returned and index_merge is discarded. (while it is actually 
    possible to try harder).

    As a consequence of this, choice of keys to do index_merge 
    read may depend on the order of conditions in WHERE part of 
    the query.

  RETURN
    0     OK, result is stored in *im1 
    other Error, both passed lists are unusable

*/

int imerge_list_or_list(PARAM *param, 
                        List<SEL_IMERGE> *im1,
                        List<SEL_IMERGE> *im2)
{
  SEL_IMERGE *imerge= im1->head();
  im1->empty();
  im1->push_back(imerge);
  
  return imerge->or_sel_imerge_with_checks(param, im2->head());
}


/*
  Perform OR operation on index_merge list and key tree.

  RETURN
    0     OK, result is stored in *im1 
    other Error
  
*/

int imerge_list_or_tree(PARAM *param, 
                        List<SEL_IMERGE> *im1,
                        SEL_TREE *tree)
{
  SEL_IMERGE *imerge;
  List_iterator<SEL_IMERGE> it(*im1);
  while((imerge= it++))
  {
    if (imerge->or_sel_tree_with_checks(param, tree))
      it.remove();
  }
  return im1->is_empty();
}

/***************************************************************************
** Basic functions for SQL_SELECT and QUICK_RANGE_SELECT
***************************************************************************/

	/* make a select from mysql info
	   Error is set as following:
	   0 = ok
	   1 = Got some error (out of memory?)
	   */

SQL_SELECT *make_select(TABLE *head, table_map const_tables,
			table_map read_tables, COND *conds, int *error)
{
  SQL_SELECT *select;
  DBUG_ENTER("make_select");

  *error=0;
  if (!conds)
    DBUG_RETURN(0);
  if (!(select= new SQL_SELECT))
  {
    *error= 1;			// out of memory
    DBUG_RETURN(0);		/* purecov: inspected */
  }
  select->read_tables=read_tables;
  select->const_tables=const_tables;
  select->head=head;
  select->cond=conds;

  if (head->sort.io_cache)
  {
    select->file= *head->sort.io_cache;
    select->records=(ha_rows) (select->file.end_of_file/
			       head->file->ref_length);
    my_free((gptr) (head->sort.io_cache),MYF(0));
    head->sort.io_cache=0;
  }
  DBUG_RETURN(select);
}


SQL_SELECT::SQL_SELECT() :quick(0),cond(0),free_cond(0)
{
  quick_keys.clear_all(); needed_reg.clear_all();
  my_b_clear(&file);
}


SQL_SELECT::~SQL_SELECT()
{
  delete quick;
  if (free_cond)
    delete cond;
  close_cached_file(&file);
}

#undef index					// Fix for Unixware 7

QUICK_SELECT_I::QUICK_SELECT_I()
  :max_used_key_length(0),
   used_key_parts(0)
{}

QUICK_RANGE_SELECT::QUICK_RANGE_SELECT(THD *thd, TABLE *table, uint key_nr, 
                                       bool no_alloc, MEM_ROOT *parent_alloc)
  :dont_free(0),error(0),it(ranges),range(0)
{
  index= key_nr;
  head=  table;

  if (!no_alloc && !parent_alloc)
  {
    // Allocates everything through the internal memroot
    init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
    my_pthread_setspecific_ptr(THR_MALLOC,&alloc);
  }
  else
    bzero((char*) &alloc,sizeof(alloc));
  file= head->file;
  record= head->record[0];
}

int QUICK_RANGE_SELECT::init()
{
  return (error= file->index_init(index));
}

QUICK_RANGE_SELECT::~QUICK_RANGE_SELECT()
{
  if (!dont_free)
  {
    file->index_end();
    free_root(&alloc,MYF(0));
  }
}


QUICK_INDEX_MERGE_SELECT::QUICK_INDEX_MERGE_SELECT(THD *thd_param, TABLE *table)
  :cur_quick_it(quick_selects), thd(thd_param), unique(NULL)
{
  index= MAX_KEY;
  head= table;
  reset_called= false;
  init_sql_alloc(&alloc,1024,0);
}

int QUICK_INDEX_MERGE_SELECT::init()
{
  cur_quick_it.rewind();
  cur_quick_select= cur_quick_it++;
  return cur_quick_select->init();
}

int QUICK_INDEX_MERGE_SELECT::reset()
{
  int result;
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::reset");
  if (reset_called)
    DBUG_RETURN(0);

  reset_called= true;
  result = cur_quick_select->reset() && prepare_unique();  
  DBUG_RETURN(result);
}

bool 
QUICK_INDEX_MERGE_SELECT::push_quick_back(QUICK_RANGE_SELECT *quick_sel_range)
{
  return quick_selects.push_back(quick_sel_range);
}

QUICK_INDEX_MERGE_SELECT::~QUICK_INDEX_MERGE_SELECT()
{
  quick_selects.delete_elements();
  free_root(&alloc,MYF(0));
}

QUICK_RANGE::QUICK_RANGE()
  :min_key(0),max_key(0),min_length(0),max_length(0),
   flag(NO_MIN_RANGE | NO_MAX_RANGE)
{}

SEL_ARG::SEL_ARG(SEL_ARG &arg) :Sql_alloc()
{
  type=arg.type;
  min_flag=arg.min_flag;
  max_flag=arg.max_flag;
  maybe_flag=arg.maybe_flag;
  maybe_null=arg.maybe_null;
  part=arg.part;
  field=arg.field;
  min_value=arg.min_value;
  max_value=arg.max_value;
  next_key_part=arg.next_key_part;
  use_count=1; elements=1;
}


inline void SEL_ARG::make_root()
{
  left=right= &null_element;
  color=BLACK;
  next=prev=0;
  use_count=0; elements=1;
}

SEL_ARG::SEL_ARG(Field *f,const char *min_value_arg,const char *max_value_arg)
  :min_flag(0), max_flag(0), maybe_flag(0), maybe_null(f->real_maybe_null()),
   elements(1), use_count(1), field(f), min_value((char*) min_value_arg),
   max_value((char*) max_value_arg), next(0),prev(0),
   next_key_part(0),color(BLACK),type(KEY_RANGE)
{
  left=right= &null_element;
}

SEL_ARG::SEL_ARG(Field *field_,uint8 part_,char *min_value_,char *max_value_,
		 uint8 min_flag_,uint8 max_flag_,uint8 maybe_flag_)
  :min_flag(min_flag_),max_flag(max_flag_),maybe_flag(maybe_flag_),
   part(part_),maybe_null(field_->real_maybe_null()), elements(1),use_count(1),
   field(field_), min_value(min_value_), max_value(max_value_),
   next(0),prev(0),next_key_part(0),color(BLACK),type(KEY_RANGE)
{
  left=right= &null_element;
}

SEL_ARG *SEL_ARG::clone(SEL_ARG *new_parent,SEL_ARG **next_arg)
{
  SEL_ARG *tmp;
  if (type != KEY_RANGE)
  {
    if (!(tmp= new SEL_ARG(type)))
      return 0;					// out of memory
    tmp->prev= *next_arg;			// Link into next/prev chain
    (*next_arg)->next=tmp;
    (*next_arg)= tmp;
  }
  else
  {
    if (!(tmp= new SEL_ARG(field,part, min_value,max_value,
			   min_flag, max_flag, maybe_flag)))
      return 0;					// OOM
    tmp->parent=new_parent;
    tmp->next_key_part=next_key_part;
    if (left != &null_element)
      tmp->left=left->clone(tmp,next_arg);

    tmp->prev= *next_arg;			// Link into next/prev chain
    (*next_arg)->next=tmp;
    (*next_arg)= tmp;

    if (right != &null_element)
      if (!(tmp->right= right->clone(tmp,next_arg)))
	return 0;				// OOM
  }
  increment_use_count(1);
  return tmp;
}

SEL_ARG *SEL_ARG::first()
{
  SEL_ARG *next_arg=this;
  if (!next_arg->left)
    return 0;					// MAYBE_KEY
  while (next_arg->left != &null_element)
    next_arg=next_arg->left;
  return next_arg;
}

SEL_ARG *SEL_ARG::last()
{
  SEL_ARG *next_arg=this;
  if (!next_arg->right)
    return 0;					// MAYBE_KEY
  while (next_arg->right != &null_element)
    next_arg=next_arg->right;
  return next_arg;
}


/*
  Check if a compare is ok, when one takes ranges in account
  Returns -2 or 2 if the ranges where 'joined' like  < 2 and >= 2
*/

static int sel_cmp(Field *field, char *a,char *b,uint8 a_flag,uint8 b_flag)
{
  int cmp;
  /* First check if there was a compare to a min or max element */
  if (a_flag & (NO_MIN_RANGE | NO_MAX_RANGE))
  {
    if ((a_flag & (NO_MIN_RANGE | NO_MAX_RANGE)) ==
	(b_flag & (NO_MIN_RANGE | NO_MAX_RANGE)))
      return 0;
    return (a_flag & NO_MIN_RANGE) ? -1 : 1;
  }
  if (b_flag & (NO_MIN_RANGE | NO_MAX_RANGE))
    return (b_flag & NO_MIN_RANGE) ? 1 : -1;

  if (field->real_maybe_null())			// If null is part of key
  {
    if (*a != *b)
    {
      return *a ? -1 : 1;
    }
    if (*a)
      goto end;					// NULL where equal
    a++; b++;					// Skip NULL marker
  }
  cmp=field->key_cmp((byte*) a,(byte*) b);
  if (cmp) return cmp < 0 ? -1 : 1;		// The values differed

  // Check if the compared equal arguments was defined with open/closed range
 end:
  if (a_flag & (NEAR_MIN | NEAR_MAX))
  {
    if ((a_flag & (NEAR_MIN | NEAR_MAX)) == (b_flag & (NEAR_MIN | NEAR_MAX)))
      return 0;
    if (!(b_flag & (NEAR_MIN | NEAR_MAX)))
      return (a_flag & NEAR_MIN) ? 2 : -2;
    return (a_flag & NEAR_MIN) ? 1 : -1;
  }
  if (b_flag & (NEAR_MIN | NEAR_MAX))
    return (b_flag & NEAR_MIN) ? -2 : 2;
  return 0;					// The elements where equal
}


SEL_ARG *SEL_ARG::clone_tree()
{
  SEL_ARG tmp_link,*next_arg,*root;
  next_arg= &tmp_link;
  root= clone((SEL_ARG *) 0, &next_arg);
  next_arg->next=0;				// Fix last link
  tmp_link.next->prev=0;			// Fix first link
  if (root)					// If not OOM
    root->use_count= 0;
  return root;
}

/*
  Test if a key can be used in different ranges

  SYNOPSIS
   SQL_SELECT::test_quick_select(thd,keys_to_use, prev_tables,
                                 limit, force_quick_range)

   Updates the following in the select parameter:
    needed_reg - Bits for keys with may be used if all prev regs are read
    quick      - Parameter to use when reading records.
   In the table struct the following information is updated:
    quick_keys - Which keys can be used
    quick_rows - How many rows the key matches

 RETURN VALUES
  -1 if impossible select
   0 if can't use quick_select
   1 if found usable range

 TODO
   check if the function really needs to modify keys_to_use, and change the
   code to pass it by reference if not
*/

int SQL_SELECT::test_quick_select(THD *thd, key_map keys_to_use,
				  table_map prev_tables,
				  ha_rows limit, bool force_quick_range)
{
  uint basflag;
  uint idx;
  double scan_time;
  QUICK_INDEX_MERGE_SELECT *quick_imerge= NULL;
  DBUG_ENTER("test_quick_select");
  DBUG_PRINT("enter",("keys_to_use: %lu  prev_tables: %lu  const_tables: %lu",
		      keys_to_use.to_ulonglong(), (ulong) prev_tables,
		      (ulong) const_tables));

  delete quick;
  quick=0;
  needed_reg.clear_all(); quick_keys.clear_all();
  if (!cond || (specialflag & SPECIAL_SAFE_MODE) && ! force_quick_range ||
      !limit)
    DBUG_RETURN(0); /* purecov: inspected */
  if (!((basflag= head->file->table_flags()) & HA_KEYPOS_TO_RNDPOS) &&
      keys_to_use.is_set_all() || keys_to_use.is_clear_all())
    DBUG_RETURN(0);				/* Not smart database */
  records=head->file->records;
  if (!records)
    records++;					/* purecov: inspected */
  scan_time=(double) records / TIME_FOR_COMPARE+1;
  read_time=(double) head->file->scan_time()+ scan_time + 1.0;
  if (head->force_index)
    scan_time= read_time= DBL_MAX;
  if (limit < records)
    read_time=(double) records+scan_time+1;	// Force to use index
  else if (read_time <= 2.0 && !force_quick_range)
    DBUG_RETURN(0);				/* No need for quick select */

  DBUG_PRINT("info",("Time to scan table: %g", read_time));

  keys_to_use.intersect(head->keys_in_use_for_query);
  if (!keys_to_use.is_clear_all())
  {
    MEM_ROOT *old_root,alloc;
    SEL_TREE *tree;
    KEY_PART *key_parts;
    PARAM param;

    /* set up parameter that is passed to all functions */
    param.thd= thd;
    param.baseflag=basflag;
    param.prev_tables=prev_tables | const_tables;
    param.read_tables=read_tables;
    param.current_table= head->map;
    param.table=head;
    param.keys=0;
    param.mem_root= &alloc;

    thd->no_errors=1;				// Don't warn about NULL
    init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
    if (!(param.key_parts = (KEY_PART*) alloc_root(&alloc,
						   sizeof(KEY_PART)*
						   head->key_parts)))
    {
      thd->no_errors=0;
      free_root(&alloc,MYF(0));			// Return memory & allocator
      DBUG_RETURN(0);				// Can't use range
    }
    key_parts= param.key_parts;
    old_root=my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC);
    my_pthread_setspecific_ptr(THR_MALLOC,&alloc);

    for (idx=0 ; idx < head->keys ; idx++)
    {
      if (!keys_to_use.is_set(idx))
	continue;
      KEY *key_info= &head->key_info[idx];
      if (key_info->flags & HA_FULLTEXT)
	continue;    // ToDo: ft-keys in non-ft ranges, if possible   SerG

      param.key[param.keys]=key_parts;
      for (uint part=0 ; part < key_info->key_parts ; part++,key_parts++)
      {
	key_parts->key=param.keys;
	key_parts->part=part;
	key_parts->part_length= key_info->key_part[part].length;
	key_parts->field=    key_info->key_part[part].field;
	key_parts->null_bit= key_info->key_part[part].null_bit;
	if (key_parts->field->type() == FIELD_TYPE_BLOB)
	  key_parts->part_length+=HA_KEY_BLOB_LENGTH;
        key_parts->image_type =
          (key_info->flags & HA_SPATIAL) ? Field::itMBR : Field::itRAW;
      }
      param.real_keynr[param.keys++]=idx;
    }
    param.key_parts_end=key_parts;

    if ((tree=get_mm_tree(&param,cond)))
    {
      if (tree->type == SEL_TREE::IMPOSSIBLE)
      {
	records=0L;				// Return -1 from this function
	read_time= (double) HA_POS_ERROR;
      }
      else if (tree->type == SEL_TREE::KEY ||
               tree->type == SEL_TREE::KEY_SMALLER)
      {
        /*
          It is possible to use a quick select (but maybe it would be slower
          than 'all' table scan).
        */
	SEL_ARG **best_key= 0;
	ha_rows found_records;
	double found_read_time= read_time;

        if (!get_quick_select_params(tree, param, needed_reg, head, true,
                                     &found_read_time, &found_records,
                                     &best_key))
        {
          /* 
            Ok, quick select is better than 'all' table scan and we have its 
            parameters, so construct it.
          */
          read_time= found_read_time;
          records= found_records;

          if ((quick= get_quick_select(&param,(uint) (best_key-tree->keys),
                                       *best_key)) && (!quick->init()))
          {
            quick->records= records;
            quick->read_time= read_time;
          }
        }

        /* 
           btw, tree type SEL_TREE::INDEX_MERGE was not introduced 
           intentionally
        */

        /* if no range select could be built, try using index_merge */
        if (!quick && !tree->merges.is_empty())
        {
          DBUG_PRINT("info",("No range reads possible,"
                             " trying to construct index_merge"));
          SEL_IMERGE *imerge;
          SEL_IMERGE *min_imerge= NULL;
          double  min_imerge_cost= DBL_MAX;
          ha_rows min_imerge_records;
          
          List_iterator_fast<SEL_IMERGE> it(tree->merges);
          while ((imerge= it++))
          {
            double  imerge_cost= 0;
            ha_rows imerge_total_records= 0;
            double  tree_read_time;
            ha_rows tree_records;
            imerge->best_keys=
              (SEL_ARG***)alloc_root(&alloc,
                                     (imerge->trees_next - imerge->trees)*
                                     sizeof(void*));
            for (SEL_TREE **ptree= imerge->trees;
                 ptree != imerge->trees_next;
                 ptree++)
            {
              tree_read_time= read_time;              
              if (get_quick_select_params(*ptree, param, needed_reg, head, 
                                          false,
                                          &tree_read_time, &tree_records,
                                          &(imerge->best_keys[ptree - 
                                          imerge->trees])))
                goto imerge_fail;

              imerge_cost += tree_read_time;
              imerge_total_records += tree_records;
            }
            imerge_total_records= min(imerge_total_records, 
                                      head->file->records);
            imerge_cost += imerge_total_records / TIME_FOR_COMPARE;
            if (imerge_cost < min_imerge_cost)
            {
              min_imerge= imerge;
              min_imerge_cost= imerge_cost;
              min_imerge_records= imerge_total_records;
            }
imerge_fail:;
          }
          
          if (!min_imerge)
            goto end_free;
          
          records= min_imerge_records;
          /* ok, got minimal imerge, *min_imerge, with cost min_imerge_cost */
          
          if (!head->used_keys.is_clear_all())
          {
            /* check if "ALL" +"using index" read would be faster */
            int key_for_use= find_shortest_key(head, &head->used_keys);
            ha_rows total_table_records= (0 == head->file->records)? 1 : 
                                          head->file->records;
            uint keys_per_block= (head->file->block_size/2/
                                  (head->key_info[key_for_use].key_length+
                                  head->file->ref_length) + 1);
            double all_index_scan_read_time= ((double)(total_table_records+
                                              keys_per_block-1)/
                                              (double) keys_per_block);

            DBUG_PRINT("info", 
                       ("'all' scan will be using key %d, read time %g",
                       key_for_use, all_index_scan_read_time));
            if (all_index_scan_read_time < min_imerge_cost)
            {
              DBUG_PRINT("info", 
                         ("index merge would be slower, "
                         "will do full 'index' scan"));
              goto end_free;
            }
          }
          else
          {
            /* check if "ALL" would be faster */
            if (read_time < min_imerge_cost)
            {
              DBUG_PRINT("info", 
                         ("index merge would be slower, "
                         "will do full table scan"));
              goto end_free;
            }
          }

          if (!(quick= quick_imerge= new QUICK_INDEX_MERGE_SELECT(thd, head)))
            goto end_free;

          quick->records= min_imerge_records;
          quick->read_time= min_imerge_cost;
          
          my_pthread_setspecific_ptr(THR_MALLOC, &quick_imerge->alloc);

          QUICK_RANGE_SELECT *new_quick;
          for (SEL_TREE **ptree = min_imerge->trees;
               ptree != min_imerge->trees_next;
               ptree++)
          {
            SEL_ARG **tree_best_key= 
              min_imerge->best_keys[ptree - min_imerge->trees];
            if ((new_quick= get_quick_select(&param,
                                             (uint)(tree_best_key-
                                             (*ptree)->keys),
                                             *tree_best_key,
                                             &quick_imerge->alloc)))
            {
              new_quick->records= min_imerge_records;
              new_quick->read_time= min_imerge_cost;
              /*
                QUICK_RANGE_SELECT::QUICK_RANGE_SELECT leaves THR_MALLOC
                pointing to its allocator, restore it back
              */
              quick_imerge->last_quick_select= new_quick;

              if (quick_imerge->push_quick_back(new_quick))
              {
                delete new_quick;
                delete quick;
                quick= quick_imerge= NULL;
                goto end_free;
              }
            }
            else
            {
              delete quick;
              quick= quick_imerge= NULL;
              goto end_free;
            }
          }

          free_root(&alloc,MYF(0));
          my_pthread_setspecific_ptr(THR_MALLOC,old_root);          
          if (quick->init())
          {
            delete quick;
            quick= quick_imerge= NULL;
            DBUG_PRINT("error", 
                       ("Failed to allocate index merge structures,"
                       "falling back to full scan."));
          }
          goto end;
        }
      }
    }
end_free:
    free_root(&alloc,MYF(0));			// Return memory & allocator
    my_pthread_setspecific_ptr(THR_MALLOC,old_root);
end:
    thd->no_errors=0;
  }

  DBUG_EXECUTE("info",
    {
      if (quick_imerge)
        print_quick_sel_imerge(quick_imerge, &needed_reg);
      else
        print_quick_sel_range((QUICK_RANGE_SELECT*)quick, &needed_reg);
    }
  );

  /*
    Assume that if the user is using 'limit' we will only need to scan
    limit rows if we are using a key
  */
  DBUG_RETURN(records ? test(quick) : -1);
}


/*
  Calculate quick select read time, # of records, and best key to use 
  without constructing QUICK_SELECT
*/

static int get_quick_select_params(SEL_TREE *tree, PARAM& param, 
                                   key_map& needed_reg, TABLE *head,
                                   bool index_read_can_be_used,
                                   double* read_time, ha_rows* records,
                                   SEL_ARG*** key_to_read)
{
  int idx;
  int result = 1;
  /*
    Note that there may be trees that have type SEL_TREE::KEY but contain 
    no key reads at all. For example, tree for expression "key1 is not null"
    where key1 is defined as "not null".
  */
  SEL_ARG **key,**end;

  for (idx= 0,key=tree->keys, end=key+param.keys ;
       key != end ;
       key++,idx++)
  {
    ha_rows found_records;
    double found_read_time;
    if (*key)
    {
      uint keynr= param.real_keynr[idx];
      if ((*key)->type == SEL_ARG::MAYBE_KEY ||
          (*key)->maybe_flag)
        needed_reg.set_bit(keynr);
      
      bool read_index_only= index_read_can_be_used? head->used_keys.is_set(keynr): false;
      found_records=check_quick_select(&param, idx, *key);
      if (found_records != HA_POS_ERROR && found_records > 2 &&
          read_index_only &&
          (head->file->index_flags(keynr) & HA_KEY_READ_ONLY))
      {
        /*
          We can resolve this by only reading through this key.
          Assume that we will read trough the whole key range
          and that all key blocks are half full (normally things are
          much better).
        */
        uint keys_per_block= (head->file->block_size/2/
			      (head->key_info[keynr].key_length+
			       head->file->ref_length) + 1);
	found_read_time=((double) (found_records+keys_per_block-1)/
			 (double) keys_per_block);
      }
      else
	found_read_time= (head->file->read_time(keynr,
						param.range_count,
						found_records)+
			  (double) found_records / TIME_FOR_COMPARE);
      if (*read_time > found_read_time && found_records != HA_POS_ERROR)
      {
        *read_time=   found_read_time;
        *records=     found_records;
        *key_to_read= key;
        result = 0;
      }
    }
  }
  return result;
}

	/* make a select tree of all keys in condition */

static SEL_TREE *get_mm_tree(PARAM *param,COND *cond)
{
  SEL_TREE *tree=0;
  DBUG_ENTER("get_mm_tree");

  if (cond->type() == Item::COND_ITEM)
  {
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      tree=0;
      Item *item;
      while ((item=li++))
      {
	SEL_TREE *new_tree=get_mm_tree(param,item);
	if (param->thd->is_fatal_error)
	  DBUG_RETURN(0);	// out of memory
	tree=tree_and(param,tree,new_tree);
	if (tree && tree->type == SEL_TREE::IMPOSSIBLE)
	  break;
      }
    }
    else
    {						// COND OR
      tree=get_mm_tree(param,li++);
      if (tree)
      {
	Item *item;
	while ((item=li++))
	{
	  SEL_TREE *new_tree=get_mm_tree(param,item);
	  if (!new_tree)
	    DBUG_RETURN(0);	// out of memory
	  tree=tree_or(param,tree,new_tree);
	  if (!tree || tree->type == SEL_TREE::ALWAYS)
	    break;
	}
      }
    }
    DBUG_RETURN(tree);
  }
  /* Here when simple cond */
  if (cond->const_item())
  {
    if (cond->val_int())
      DBUG_RETURN(new SEL_TREE(SEL_TREE::ALWAYS));
    DBUG_RETURN(new SEL_TREE(SEL_TREE::IMPOSSIBLE));
  }

  table_map ref_tables=cond->used_tables();
  if (cond->type() != Item::FUNC_ITEM)
  {						// Should be a field
    if ((ref_tables & param->current_table) ||
	(ref_tables & ~(param->prev_tables | param->read_tables)))
      DBUG_RETURN(0);
    DBUG_RETURN(new SEL_TREE(SEL_TREE::MAYBE));
  }

  Item_func *cond_func= (Item_func*) cond;
  if (cond_func->select_optimize() == Item_func::OPTIMIZE_NONE)
    DBUG_RETURN(0);				// Can't be calculated

  if (cond_func->functype() == Item_func::BETWEEN)
  {
    if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) (cond_func->arguments()[0]))->field;
      Item_result cmp_type=field->cmp_type();
      DBUG_RETURN(tree_and(param,
			   get_mm_parts(param, field,
					Item_func::GE_FUNC,
					cond_func->arguments()[1], cmp_type),
			   get_mm_parts(param, field,
					Item_func::LE_FUNC,
					cond_func->arguments()[2], cmp_type)));
    }
    DBUG_RETURN(0);
  }
  if (cond_func->functype() == Item_func::IN_FUNC)
  {						// COND OR
    Item_func_in *func=(Item_func_in*) cond_func;
    if (func->key_item()->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) (func->key_item()))->field;
      Item_result cmp_type=field->cmp_type();
      tree= get_mm_parts(param,field,Item_func::EQ_FUNC,
			 func->arguments()[1],cmp_type);
      if (!tree)
	DBUG_RETURN(tree);			// Not key field
      for (uint i=2 ; i < func->argument_count(); i++)
      {
	SEL_TREE *new_tree=get_mm_parts(param,field,Item_func::EQ_FUNC,
					func->arguments()[i],cmp_type);
	tree=tree_or(param,tree,new_tree);
      }
      DBUG_RETURN(tree);
    }
    DBUG_RETURN(0);				// Can't optimize this IN
  }

  if (ref_tables & ~(param->prev_tables | param->read_tables |
		     param->current_table))
    DBUG_RETURN(0);				// Can't be calculated yet
  if (!(ref_tables & param->current_table))
    DBUG_RETURN(new SEL_TREE(SEL_TREE::MAYBE)); // This may be false or true

  /* check field op const */
  /* btw, ft_func's arguments()[0] isn't FIELD_ITEM.  SerG*/
  if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
  {
    tree= get_mm_parts(param,
		       ((Item_field*) (cond_func->arguments()[0]))->field,
		       cond_func->functype(),
		       cond_func->arg_count > 1 ? cond_func->arguments()[1] :
		       0,
		       ((Item_field*) (cond_func->arguments()[0]))->field->
		       cmp_type());
  }
  /* check const op field */
  if (!tree &&
      cond_func->have_rev_func() &&
      cond_func->arguments()[1]->type() == Item::FIELD_ITEM)
  {
    DBUG_RETURN(get_mm_parts(param,
			     ((Item_field*)
			      (cond_func->arguments()[1]))->field,
			     ((Item_bool_func2*) cond_func)->rev_functype(),
			     cond_func->arguments()[0],
			     ((Item_field*)
			      (cond_func->arguments()[1]))->field->cmp_type()
			     ));
  }
  DBUG_RETURN(tree);
}


static SEL_TREE *
get_mm_parts(PARAM *param, Field *field, Item_func::Functype type, 
	     Item *value, Item_result cmp_type)
{
  bool ne_func= FALSE;
  DBUG_ENTER("get_mm_parts");
  if (field->table != param->table)
    DBUG_RETURN(0);

  if (type == Item_func::NE_FUNC)
  {
    ne_func= TRUE;
    type= Item_func::LT_FUNC;
  }

  KEY_PART *key_part = param->key_parts;
  KEY_PART *end = param->key_parts_end;
  SEL_TREE *tree=0;
  if (value &&
      value->used_tables() & ~(param->prev_tables | param->read_tables))
    DBUG_RETURN(0);
  for (; key_part != end ; key_part++)
  {
    if (field->eq(key_part->field))
    {
      SEL_ARG *sel_arg=0;
      if (!tree && !(tree=new SEL_TREE()))
	DBUG_RETURN(0);				// OOM
      if (!value || !(value->used_tables() & ~param->read_tables))
      {
	sel_arg=get_mm_leaf(param,key_part->field,key_part,type,value);
	if (!sel_arg)
	  continue;
	if (sel_arg->type == SEL_ARG::IMPOSSIBLE)
	{
	  tree->type=SEL_TREE::IMPOSSIBLE;
	  DBUG_RETURN(tree);
	}
      }
      else
      {
	// This key may be used later
	if (!(sel_arg= new SEL_ARG(SEL_ARG::MAYBE_KEY))) 
	  DBUG_RETURN(0);			// OOM
      }
      sel_arg->part=(uchar) key_part->part;
      tree->keys[key_part->key]=sel_add(tree->keys[key_part->key],sel_arg);
      tree->keys_map.set_bit(key_part->key);
    }
  }

  if (ne_func)
  {
    SEL_TREE *tree2= get_mm_parts(param, field, Item_func::GT_FUNC,
                                  value, cmp_type);
    if (tree2)
      tree= tree_or(param,tree,tree2);
  }
  DBUG_RETURN(tree);
}


static SEL_ARG *
get_mm_leaf(PARAM *param, Field *field, KEY_PART *key_part,
	    Item_func::Functype type,Item *value)
{
  uint maybe_null=(uint) field->real_maybe_null();
  uint field_length=field->pack_length()+maybe_null;
  SEL_ARG *tree;
  DBUG_ENTER("get_mm_leaf");

  if (type == Item_func::LIKE_FUNC)
  {
    bool like_error;
    char buff1[MAX_FIELD_WIDTH],*min_str,*max_str;
    String tmp(buff1,sizeof(buff1),value->collation.collation),*res;
    uint length,offset,min_length,max_length;

    if (!field->optimize_range(param->real_keynr[key_part->key]))
      DBUG_RETURN(0);				// Can't optimize this
    if (!(res= value->val_str(&tmp)))
      DBUG_RETURN(&null_element);

    /*
      TODO:
      Check if this was a function. This should have be optimized away
      in the sql_select.cc
    */
    if (res != &tmp)
    {
      tmp.copy(*res);				// Get own copy
      res= &tmp;
    }
    if (field->cmp_type() != STRING_RESULT)
      DBUG_RETURN(0);				// Can only optimize strings

    offset=maybe_null;
    length=key_part->part_length;
    if (field->type() == FIELD_TYPE_BLOB)
    {
      offset+=HA_KEY_BLOB_LENGTH;
      field_length=key_part->part_length-HA_KEY_BLOB_LENGTH;
    }
    else
    {
      if (length < field_length)
	length=field_length;			// Only if overlapping key
      else
	field_length=length;
    }
    length+=offset;
    if (!(min_str= (char*) alloc_root(param->mem_root, length*2)))
      DBUG_RETURN(0);
    max_str=min_str+length;
    if (maybe_null)
      max_str[0]= min_str[0]=0;

    like_error= my_like_range(field->charset(),
                                  res->ptr(),res->length(),
				  wild_prefix,wild_one,wild_many,
                                  field_length, 
				  min_str+offset, max_str+offset,
				  &min_length,&max_length);

    if (like_error)				// Can't optimize with LIKE
      DBUG_RETURN(0);
    if (offset != maybe_null)			// Blob
    {
      int2store(min_str+maybe_null,min_length);
      int2store(max_str+maybe_null,max_length);
    }
    DBUG_RETURN(new SEL_ARG(field,min_str,max_str));
  }

  if (!value)					// IS NULL or IS NOT NULL
  {
    if (field->table->outer_join)		// Can't use a key on this
      DBUG_RETURN(0);
    if (!maybe_null)				// Not null field
      DBUG_RETURN(type == Item_func::ISNULL_FUNC ? &null_element : 0);
    if (!(tree=new SEL_ARG(field,is_null_string,is_null_string)))
      DBUG_RETURN(0);		// out of memory
    if (type == Item_func::ISNOTNULL_FUNC)
    {
      tree->min_flag=NEAR_MIN;		    /* IS NOT NULL ->  X > NULL */
      tree->max_flag=NO_MAX_RANGE;
    }
    DBUG_RETURN(tree);
  }

  if (!field->optimize_range(param->real_keynr[key_part->key]) &&
      type != Item_func::EQ_FUNC &&
      type != Item_func::EQUAL_FUNC)
    DBUG_RETURN(0);				// Can't optimize this

  /*
    We can't always use indexes when comparing a string index to a number
    cmp_type() is checked to allow compare of dates to numbers
  */
  if (field->result_type() == STRING_RESULT &&
      value->result_type() != STRING_RESULT &&
      field->cmp_type() != value->result_type())
    DBUG_RETURN(0);

  if (value->save_in_field(field, 1) > 0)
  {
    /* This happens when we try to insert a NULL field in a not null column */
    DBUG_RETURN(&null_element);			// cmp with NULL is never true
  }
  // Get local copy of key
  char *str= (char*) alloc_root(param->mem_root,
				key_part->part_length+maybe_null);
  if (!str)
    DBUG_RETURN(0);
  if (maybe_null)
    *str= (char) field->is_real_null();		// Set to 1 if null
  field->get_key_image(str+maybe_null,key_part->part_length,
		       field->charset(),key_part->image_type);
  if (!(tree=new SEL_ARG(field,str,str)))
    DBUG_RETURN(0);		// out of memory

  switch (type) {
  case Item_func::LT_FUNC:
    if (field_is_equal_to_item(field,value))
      tree->max_flag=NEAR_MAX;
    /* fall through */
  case Item_func::LE_FUNC:
    if (!maybe_null)
      tree->min_flag=NO_MIN_RANGE;		/* From start */
    else
    {						// > NULL
      tree->min_value=is_null_string;
      tree->min_flag=NEAR_MIN;
    }
    break;
  case Item_func::GT_FUNC:
    if (field_is_equal_to_item(field,value))
      tree->min_flag=NEAR_MIN;
    /* fall through */
  case Item_func::GE_FUNC:
    tree->max_flag=NO_MAX_RANGE;
    break;
  case Item_func::SP_EQUALS_FUNC:
      tree->min_flag=GEOM_FLAG | HA_READ_MBR_EQUAL;// NEAR_MIN;//512;
      tree->max_flag=NO_MAX_RANGE;
      break;
  case Item_func::SP_DISJOINT_FUNC:
      tree->min_flag=GEOM_FLAG | HA_READ_MBR_DISJOINT;// NEAR_MIN;//512;
      tree->max_flag=NO_MAX_RANGE;
      break;
  case Item_func::SP_INTERSECTS_FUNC:
      tree->min_flag=GEOM_FLAG | HA_READ_MBR_INTERSECT;// NEAR_MIN;//512;
      tree->max_flag=NO_MAX_RANGE;
      break;
  case Item_func::SP_TOUCHES_FUNC:
      tree->min_flag=GEOM_FLAG | HA_READ_MBR_INTERSECT;// NEAR_MIN;//512;
      tree->max_flag=NO_MAX_RANGE;
      break;

  case Item_func::SP_CROSSES_FUNC:
      tree->min_flag=GEOM_FLAG | HA_READ_MBR_INTERSECT;// NEAR_MIN;//512;
      tree->max_flag=NO_MAX_RANGE;
      break;
  case Item_func::SP_WITHIN_FUNC:
      tree->min_flag=GEOM_FLAG | HA_READ_MBR_WITHIN;// NEAR_MIN;//512;
      tree->max_flag=NO_MAX_RANGE;
      break;

  case Item_func::SP_CONTAINS_FUNC:
      tree->min_flag=GEOM_FLAG | HA_READ_MBR_CONTAIN;// NEAR_MIN;//512;
      tree->max_flag=NO_MAX_RANGE;
      break;
  case Item_func::SP_OVERLAPS_FUNC:
      tree->min_flag=GEOM_FLAG | HA_READ_MBR_INTERSECT;// NEAR_MIN;//512;
      tree->max_flag=NO_MAX_RANGE;
      break;

  default:
    break;
  }
  DBUG_RETURN(tree);
}


/******************************************************************************
** Tree manipulation functions
** If tree is 0 it means that the condition can't be tested. It refers
** to a non existent table or to a field in current table with isn't a key.
** The different tree flags:
** IMPOSSIBLE:	 Condition is never true
** ALWAYS:	 Condition is always true
** MAYBE:	 Condition may exists when tables are read
** MAYBE_KEY:	 Condition refers to a key that may be used in join loop
** KEY_RANGE:	 Condition uses a key
******************************************************************************/

/*
  Add a new key test to a key when scanning through all keys
  This will never be called for same key parts.
*/

static SEL_ARG *
sel_add(SEL_ARG *key1,SEL_ARG *key2)
{
  SEL_ARG *root,**key_link;

  if (!key1)
    return key2;
  if (!key2)
    return key1;

  key_link= &root;
  while (key1 && key2)
  {
    if (key1->part < key2->part)
    {
      *key_link= key1;
      key_link= &key1->next_key_part;
      key1=key1->next_key_part;
    }
    else
    {
      *key_link= key2;
      key_link= &key2->next_key_part;
      key2=key2->next_key_part;
    }
  }
  *key_link=key1 ? key1 : key2;
  return root;
}

#define CLONE_KEY1_MAYBE 1
#define CLONE_KEY2_MAYBE 2
#define swap_clone_flag(A) ((A & 1) << 1) | ((A & 2) >> 1)


static SEL_TREE *
tree_and(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2)
{
  DBUG_ENTER("tree_and");
  if (!tree1)
    DBUG_RETURN(tree2);
  if (!tree2)
    DBUG_RETURN(tree1);
  if (tree1->type == SEL_TREE::IMPOSSIBLE || tree2->type == SEL_TREE::ALWAYS)
    DBUG_RETURN(tree1);
  if (tree2->type == SEL_TREE::IMPOSSIBLE || tree1->type == SEL_TREE::ALWAYS)
    DBUG_RETURN(tree2);
  if (tree1->type == SEL_TREE::MAYBE)
  {
    if (tree2->type == SEL_TREE::KEY)
      tree2->type=SEL_TREE::KEY_SMALLER;
    DBUG_RETURN(tree2);
  }
  if (tree2->type == SEL_TREE::MAYBE)
  {
    tree1->type=SEL_TREE::KEY_SMALLER;
    DBUG_RETURN(tree1);
  }

  key_map  result_keys;
  result_keys.clear_all();
  /* Join the trees key per key */
  SEL_ARG **key1,**key2,**end;
  for (key1= tree1->keys,key2= tree2->keys,end=key1+param->keys ;
       key1 != end ; key1++,key2++)
  {
    uint flag=0;
    if (*key1 || *key2)
    {
      if (*key1 && !(*key1)->simple_key())
	flag|=CLONE_KEY1_MAYBE;
      if (*key2 && !(*key2)->simple_key())
	flag|=CLONE_KEY2_MAYBE;
      *key1=key_and(*key1,*key2,flag);
      if ((*key1)->type == SEL_ARG::IMPOSSIBLE)
      {
	tree1->type= SEL_TREE::IMPOSSIBLE;
        DBUG_RETURN(tree1);
      }
      result_keys.set_bit(key1 - tree1->keys);
#ifdef EXTRA_DEBUG
      (*key1)->test_use_count(*key1);
#endif
    }
  }
  tree1->keys_map= result_keys;
  /* dispose index_merge if there is a "range" option */
  if (!result_keys.is_clear_all())
  {
    tree1->merges.empty();
    DBUG_RETURN(tree1);
  }

  /* ok, both trees are index_merge trees */
  imerge_list_and_list(&tree1->merges, &tree2->merges);
  DBUG_RETURN(tree1);
}


/*
  Check if two SEL_TREES can be combined into one without using index_merge
*/

bool sel_trees_can_be_ored(SEL_TREE *tree1, SEL_TREE *tree2, PARAM* param)
{
  key_map common_keys= tree1->keys_map;
  common_keys.intersect(tree2->keys_map);
  DBUG_ENTER("sel_trees_can_be_ored");

  if (common_keys.is_clear_all())
    DBUG_RETURN(false);
  
  /* trees have a common key, check if they refer to same key part */  
  SEL_ARG **key1,**key2;
  for (uint key_no=0; key_no < param->keys; key_no++)
  {
    if (common_keys.is_set(key_no))
    {
      key1= tree1->keys + key_no;
      key2= tree2->keys + key_no;
      if ((*key1)->part == (*key2)->part)
      {
        DBUG_RETURN(true);
      }
    }
  }
  DBUG_RETURN(false);
}

static SEL_TREE *
tree_or(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2)
{
  DBUG_ENTER("tree_or");
  if (!tree1 || !tree2)
    DBUG_RETURN(0);
  if (tree1->type == SEL_TREE::IMPOSSIBLE || tree2->type == SEL_TREE::ALWAYS)
    DBUG_RETURN(tree2);
  if (tree2->type == SEL_TREE::IMPOSSIBLE || tree1->type == SEL_TREE::ALWAYS)
    DBUG_RETURN(tree1);
  if (tree1->type == SEL_TREE::MAYBE)
    DBUG_RETURN(tree1);				// Can't use this
  if (tree2->type == SEL_TREE::MAYBE)
    DBUG_RETURN(tree2);

  SEL_TREE *result= 0;
  key_map  result_keys;
  result_keys.clear_all();
  if (sel_trees_can_be_ored(tree1, tree2, param))
  {
    /* Join the trees key per key */
    SEL_ARG **key1,**key2,**end;
    for (key1= tree1->keys,key2= tree2->keys,end= key1+param->keys ;
         key1 != end ; key1++,key2++)
    {
      *key1=key_or(*key1,*key2);
      if (*key1)
      {
        result=tree1;				// Added to tree1
        result_keys.set_bit(key1 - tree1->keys);
#ifdef EXTRA_DEBUG
        (*key1)->test_use_count(*key1);
#endif
      }
    }
    if (result)
      result->keys_map= result_keys;
  }
  else
  {
    /* ok, two trees have KEY type but cannot be used without index merge */
    if (tree1->merges.is_empty() && tree2->merges.is_empty())
    {
      SEL_IMERGE *merge;
      /* both trees are "range" trees, produce new index merge structure */
      if (!(result= new SEL_TREE()) || !(merge= new SEL_IMERGE()) ||
          (result->merges.push_back(merge)) ||
          (merge->or_sel_tree(param, tree1)) ||
          (merge->or_sel_tree(param, tree2)))
        result= NULL;
      else
        result->type= tree1->type;
    }
    else if (!tree1->merges.is_empty() && !tree2->merges.is_empty())
    {
      if (imerge_list_or_list(param, &tree1->merges, &tree2->merges))
        result= new SEL_TREE(SEL_TREE::ALWAYS);
      else
        result= tree1;
    }
    else
    {
      /* one tree is index merge tree and another is range tree */
      if (tree1->merges.is_empty())
        swap(SEL_TREE*, tree1, tree2);

      /* add tree2 to tree1->merges, checking if it collapses to ALWAYS */
      if (imerge_list_or_tree(param, &tree1->merges, tree2))
        result= new SEL_TREE(SEL_TREE::ALWAYS);
      else
        result= tree1;
    }
  }
  DBUG_RETURN(result);
}


/* And key trees where key1->part < key2 -> part */

static SEL_ARG *
and_all_keys(SEL_ARG *key1,SEL_ARG *key2,uint clone_flag)
{
  SEL_ARG *next;
  ulong use_count=key1->use_count;

  if (key1->elements != 1)
  {
    key2->use_count+=key1->elements-1;
    key2->increment_use_count((int) key1->elements-1);
  }
  if (key1->type == SEL_ARG::MAYBE_KEY)
  {
    key1->right= key1->left= &null_element;
    key1->next= key1->prev= 0;
  }
  for (next=key1->first(); next ; next=next->next)
  {
    if (next->next_key_part)
    {
      SEL_ARG *tmp=key_and(next->next_key_part,key2,clone_flag);
      if (tmp && tmp->type == SEL_ARG::IMPOSSIBLE)
      {
	key1=key1->tree_delete(next);
	continue;
      }
      next->next_key_part=tmp;
      if (use_count)
	next->increment_use_count(use_count);
    }
    else
      next->next_key_part=key2;
  }
  if (!key1)
    return &null_element;			// Impossible ranges
  key1->use_count++;
  return key1;
}


static SEL_ARG *
key_and(SEL_ARG *key1,SEL_ARG *key2,uint clone_flag)
{
  if (!key1)
    return key2;
  if (!key2)
    return key1;
  if (key1->part != key2->part)
  {
    if (key1->part > key2->part)
    {
      swap(SEL_ARG *,key1,key2);
      clone_flag=swap_clone_flag(clone_flag);
    }
    // key1->part < key2->part
    key1->use_count--;
    if (key1->use_count > 0)
      if (!(key1= key1->clone_tree()))
	return 0;				// OOM
    return and_all_keys(key1,key2,clone_flag);
  }

  if (((clone_flag & CLONE_KEY2_MAYBE) &&
       !(clone_flag & CLONE_KEY1_MAYBE) &&
       key2->type != SEL_ARG::MAYBE_KEY) ||
      key1->type == SEL_ARG::MAYBE_KEY)
  {						// Put simple key in key2
    swap(SEL_ARG *,key1,key2);
    clone_flag=swap_clone_flag(clone_flag);
  }

  // If one of the key is MAYBE_KEY then the found region may be smaller
  if (key2->type == SEL_ARG::MAYBE_KEY)
  {
    if (key1->use_count > 1)
    {
      key1->use_count--;
      if (!(key1=key1->clone_tree()))
	return 0;				// OOM
      key1->use_count++;
    }
    if (key1->type == SEL_ARG::MAYBE_KEY)
    {						// Both are maybe key
      key1->next_key_part=key_and(key1->next_key_part,key2->next_key_part,
				 clone_flag);
      if (key1->next_key_part &&
	  key1->next_key_part->type == SEL_ARG::IMPOSSIBLE)
	return key1;
    }
    else
    {
      key1->maybe_smaller();
      if (key2->next_key_part)
      {
	key1->use_count--;			// Incremented in and_all_keys
	return and_all_keys(key1,key2,clone_flag);
      }
      key2->use_count--;			// Key2 doesn't have a tree
    }
    return key1;
  }

  key1->use_count--;
  key2->use_count--;
  SEL_ARG *e1=key1->first(), *e2=key2->first(), *new_tree=0;

  while (e1 && e2)
  {
    int cmp=e1->cmp_min_to_min(e2);
    if (cmp < 0)
    {
      if (get_range(&e1,&e2,key1))
	continue;
    }
    else if (get_range(&e2,&e1,key2))
      continue;
    SEL_ARG *next=key_and(e1->next_key_part,e2->next_key_part,clone_flag);
    e1->increment_use_count(1);
    e2->increment_use_count(1);
    if (!next || next->type != SEL_ARG::IMPOSSIBLE)
    {
      SEL_ARG *new_arg= e1->clone_and(e2);
      if (!new_arg)
	return &null_element;			// End of memory
      new_arg->next_key_part=next;
      if (!new_tree)
      {
	new_tree=new_arg;
      }
      else
	new_tree=new_tree->insert(new_arg);
    }
    if (e1->cmp_max_to_max(e2) < 0)
      e1=e1->next;				// e1 can't overlapp next e2
    else
      e2=e2->next;
  }
  key1->free_tree();
  key2->free_tree();
  if (!new_tree)
    return &null_element;			// Impossible range
  return new_tree;
}


static bool
get_range(SEL_ARG **e1,SEL_ARG **e2,SEL_ARG *root1)
{
  (*e1)=root1->find_range(*e2);			// first e1->min < e2->min
  if ((*e1)->cmp_max_to_min(*e2) < 0)
  {
    if (!((*e1)=(*e1)->next))
      return 1;
    if ((*e1)->cmp_min_to_max(*e2) > 0)
    {
      (*e2)=(*e2)->next;
      return 1;
    }
  }
  return 0;
}


static SEL_ARG *
key_or(SEL_ARG *key1,SEL_ARG *key2)
{
  if (!key1)
  {
    if (key2)
    {
      key2->use_count--;
      key2->free_tree();
    }
    return 0;
  }
  else if (!key2)
  {
    key1->use_count--;
    key1->free_tree();
    return 0;
  }
  key1->use_count--;
  key2->use_count--;

  if (key1->part != key2->part)
  {
    key1->free_tree();
    key2->free_tree();
    return 0;					// Can't optimize this
  }

  // If one of the key is MAYBE_KEY then the found region may be bigger
  if (key1->type == SEL_ARG::MAYBE_KEY)
  {
    key2->free_tree();
    key1->use_count++;
    return key1;
  }
  if (key2->type == SEL_ARG::MAYBE_KEY)
  {
    key1->free_tree();
    key2->use_count++;
    return key2;
  }

  if (key1->use_count > 0)
  {
    if (key2->use_count == 0 || key1->elements > key2->elements)
    {
      swap(SEL_ARG *,key1,key2);
    }
    else if (!(key1=key1->clone_tree()))
      return 0;					// OOM
  }

  // Add tree at key2 to tree at key1
  bool key2_shared=key2->use_count != 0;
  key1->maybe_flag|=key2->maybe_flag;

  for (key2=key2->first(); key2; )
  {
    SEL_ARG *tmp=key1->find_range(key2);	// Find key1.min <= key2.min
    int cmp;

    if (!tmp)
    {
      tmp=key1->first();			// tmp.min > key2.min
      cmp= -1;
    }
    else if ((cmp=tmp->cmp_max_to_min(key2)) < 0)
    {						// Found tmp.max < key2.min
      SEL_ARG *next=tmp->next;
      if (cmp == -2 && eq_tree(tmp->next_key_part,key2->next_key_part))
      {
	// Join near ranges like tmp.max < 0 and key2.min >= 0
	SEL_ARG *key2_next=key2->next;
	if (key2_shared)
	{
	  if (!(key2=new SEL_ARG(*key2)))
	    return 0;		// out of memory
	  key2->increment_use_count(key1->use_count+1);
	  key2->next=key2_next;			// New copy of key2
	}
	key2->copy_min(tmp);
	if (!(key1=key1->tree_delete(tmp)))
	{					// Only one key in tree
	  key1=key2;
	  key1->make_root();
	  key2=key2_next;
	  break;
	}
      }
      if (!(tmp=next))				// tmp.min > key2.min
	break;					// Copy rest of key2
    }
    if (cmp < 0)
    {						// tmp.min > key2.min
      int tmp_cmp;
      if ((tmp_cmp=tmp->cmp_min_to_max(key2)) > 0) // if tmp.min > key2.max
      {
	if (tmp_cmp == 2 && eq_tree(tmp->next_key_part,key2->next_key_part))
	{					// ranges are connected
	  tmp->copy_min_to_min(key2);
	  key1->merge_flags(key2);
	  if (tmp->min_flag & NO_MIN_RANGE &&
	      tmp->max_flag & NO_MAX_RANGE)
	  {
	    if (key1->maybe_flag)
	      return new SEL_ARG(SEL_ARG::MAYBE_KEY);
	    return 0;
	  }
	  key2->increment_use_count(-1);	// Free not used tree
	  key2=key2->next;
	  continue;
	}
	else
	{
	  SEL_ARG *next=key2->next;		// Keys are not overlapping
	  if (key2_shared)
	  {
	    SEL_ARG *tmp= new SEL_ARG(*key2);	// Must make copy
	    if (!tmp)
	      return 0;				// OOM
	    key1=key1->insert(tmp);
	    key2->increment_use_count(key1->use_count+1);
	  }
	  else
	    key1=key1->insert(key2);		// Will destroy key2_root
	  key2=next;
	  continue;
	}
      }
    }

    // tmp.max >= key2.min && tmp.min <= key.max  (overlapping ranges)
    if (eq_tree(tmp->next_key_part,key2->next_key_part))
    {
      if (tmp->is_same(key2))
      {
	tmp->merge_flags(key2);			// Copy maybe flags
	key2->increment_use_count(-1);		// Free not used tree
      }
      else
      {
	SEL_ARG *last=tmp;
	while (last->next && last->next->cmp_min_to_max(key2) <= 0 &&
	       eq_tree(last->next->next_key_part,key2->next_key_part))
	{
	  SEL_ARG *save=last;
	  last=last->next;
	  key1=key1->tree_delete(save);
	}
	if (last->copy_min(key2) || last->copy_max(key2))
	{					// Full range
	  key1->free_tree();
	  for (; key2 ; key2=key2->next)
	    key2->increment_use_count(-1);	// Free not used tree
	  if (key1->maybe_flag)
	    return new SEL_ARG(SEL_ARG::MAYBE_KEY);
	  return 0;
	}
      }
      key2=key2->next;
      continue;
    }

    if (cmp >= 0 && tmp->cmp_min_to_min(key2) < 0)
    {						// tmp.min <= x < key2.min
      SEL_ARG *new_arg=tmp->clone_first(key2);
      if (!new_arg)
	return 0;				// OOM
      if ((new_arg->next_key_part= key1->next_key_part))
	new_arg->increment_use_count(key1->use_count+1);
      tmp->copy_min_to_min(key2);
      key1=key1->insert(new_arg);
    }

    // tmp.min >= key2.min && tmp.min <= key2.max
    SEL_ARG key(*key2);				// Get copy we can modify
    for (;;)
    {
      if (tmp->cmp_min_to_min(&key) > 0)
      {						// key.min <= x < tmp.min
	SEL_ARG *new_arg=key.clone_first(tmp);
	if (!new_arg)
	  return 0;				// OOM
	if ((new_arg->next_key_part=key.next_key_part))
	  new_arg->increment_use_count(key1->use_count+1);
	key1=key1->insert(new_arg);
      }
      if ((cmp=tmp->cmp_max_to_max(&key)) <= 0)
      {						// tmp.min. <= x <= tmp.max
	tmp->maybe_flag|= key.maybe_flag;
	key.increment_use_count(key1->use_count+1);
	tmp->next_key_part=key_or(tmp->next_key_part,key.next_key_part);
	if (!cmp)				// Key2 is ready
	  break;
	key.copy_max_to_min(tmp);
	if (!(tmp=tmp->next))
	{
	  SEL_ARG *tmp2= new SEL_ARG(key);
	  if (!tmp2)
	    return 0;				// OOM
	  key1=key1->insert(tmp2);
	  key2=key2->next;
	  goto end;
	}
	if (tmp->cmp_min_to_max(&key) > 0)
	{
	  SEL_ARG *tmp2= new SEL_ARG(key);
	  if (!tmp2)
	    return 0;				// OOM
	  key1=key1->insert(tmp2);
	  break;
	}
      }
      else
      {
	SEL_ARG *new_arg=tmp->clone_last(&key); // tmp.min <= x <= key.max
	if (!new_arg)
	  return 0;				// OOM
	tmp->copy_max_to_min(&key);
	tmp->increment_use_count(key1->use_count+1);
	new_arg->next_key_part=key_or(tmp->next_key_part,key.next_key_part);
	key1=key1->insert(new_arg);
	break;
      }
    }
    key2=key2->next;
  }

end:
  while (key2)
  {
    SEL_ARG *next=key2->next;
    if (key2_shared)
    {
      SEL_ARG *tmp=new SEL_ARG(*key2);		// Must make copy
      if (!tmp)
	return 0;
      key2->increment_use_count(key1->use_count+1);
      key1=key1->insert(tmp);
    }
    else
      key1=key1->insert(key2);			// Will destroy key2_root
    key2=next;
  }
  key1->use_count++;
  return key1;
}


/* Compare if two trees are equal */

static bool eq_tree(SEL_ARG* a,SEL_ARG *b)
{
  if (a == b)
    return 1;
  if (!a || !b || !a->is_same(b))
    return 0;
  if (a->left != &null_element && b->left != &null_element)
  {
    if (!eq_tree(a->left,b->left))
      return 0;
  }
  else if (a->left != &null_element || b->left != &null_element)
    return 0;
  if (a->right != &null_element && b->right != &null_element)
  {
    if (!eq_tree(a->right,b->right))
      return 0;
  }
  else if (a->right != &null_element || b->right != &null_element)
    return 0;
  if (a->next_key_part != b->next_key_part)
  {						// Sub range
    if (!a->next_key_part != !b->next_key_part ||
	!eq_tree(a->next_key_part, b->next_key_part))
      return 0;
  }
  return 1;
}


SEL_ARG *
SEL_ARG::insert(SEL_ARG *key)
{
  SEL_ARG *element,**par,*last_element;

  LINT_INIT(par); LINT_INIT(last_element);
  for (element= this; element != &null_element ; )
  {
    last_element=element;
    if (key->cmp_min_to_min(element) > 0)
    {
      par= &element->right; element= element->right;
    }
    else
    {
      par = &element->left; element= element->left;
    }
  }
  *par=key;
  key->parent=last_element;
	/* Link in list */
  if (par == &last_element->left)
  {
    key->next=last_element;
    if ((key->prev=last_element->prev))
      key->prev->next=key;
    last_element->prev=key;
  }
  else
  {
    if ((key->next=last_element->next))
      key->next->prev=key;
    key->prev=last_element;
    last_element->next=key;
  }
  key->left=key->right= &null_element;
  SEL_ARG *root=rb_insert(key);			// rebalance tree
  root->use_count=this->use_count;		// copy root info
  root->elements= this->elements+1;
  root->maybe_flag=this->maybe_flag;
  return root;
}


/*
** Find best key with min <= given key
** Because the call context this should never return 0 to get_range
*/

SEL_ARG *
SEL_ARG::find_range(SEL_ARG *key)
{
  SEL_ARG *element=this,*found=0;

  for (;;)
  {
    if (element == &null_element)
      return found;
    int cmp=element->cmp_min_to_min(key);
    if (cmp == 0)
      return element;
    if (cmp < 0)
    {
      found=element;
      element=element->right;
    }
    else
      element=element->left;
  }
}


/*
** Remove a element from the tree
** This also frees all sub trees that is used by the element
*/

SEL_ARG *
SEL_ARG::tree_delete(SEL_ARG *key)
{
  enum leaf_color remove_color;
  SEL_ARG *root,*nod,**par,*fix_par;
  root=this; this->parent= 0;

  /* Unlink from list */
  if (key->prev)
    key->prev->next=key->next;
  if (key->next)
    key->next->prev=key->prev;
  key->increment_use_count(-1);
  if (!key->parent)
    par= &root;
  else
    par=key->parent_ptr();

  if (key->left == &null_element)
  {
    *par=nod=key->right;
    fix_par=key->parent;
    if (nod != &null_element)
      nod->parent=fix_par;
    remove_color= key->color;
  }
  else if (key->right == &null_element)
  {
    *par= nod=key->left;
    nod->parent=fix_par=key->parent;
    remove_color= key->color;
  }
  else
  {
    SEL_ARG *tmp=key->next;			// next bigger key (exist!)
    nod= *tmp->parent_ptr()= tmp->right;	// unlink tmp from tree
    fix_par=tmp->parent;
    if (nod != &null_element)
      nod->parent=fix_par;
    remove_color= tmp->color;

    tmp->parent=key->parent;			// Move node in place of key
    (tmp->left=key->left)->parent=tmp;
    if ((tmp->right=key->right) != &null_element)
      tmp->right->parent=tmp;
    tmp->color=key->color;
    *par=tmp;
    if (fix_par == key)				// key->right == key->next
      fix_par=tmp;				// new parent of nod
  }

  if (root == &null_element)
    return 0;					// Maybe root later
  if (remove_color == BLACK)
    root=rb_delete_fixup(root,nod,fix_par);
  test_rb_tree(root,root->parent);

  root->use_count=this->use_count;		// Fix root counters
  root->elements=this->elements-1;
  root->maybe_flag=this->maybe_flag;
  return root;
}


	/* Functions to fix up the tree after insert and delete */

static void left_rotate(SEL_ARG **root,SEL_ARG *leaf)
{
  SEL_ARG *y=leaf->right;
  leaf->right=y->left;
  if (y->left != &null_element)
    y->left->parent=leaf;
  if (!(y->parent=leaf->parent))
    *root=y;
  else
    *leaf->parent_ptr()=y;
  y->left=leaf;
  leaf->parent=y;
}

static void right_rotate(SEL_ARG **root,SEL_ARG *leaf)
{
  SEL_ARG *y=leaf->left;
  leaf->left=y->right;
  if (y->right != &null_element)
    y->right->parent=leaf;
  if (!(y->parent=leaf->parent))
    *root=y;
  else
    *leaf->parent_ptr()=y;
  y->right=leaf;
  leaf->parent=y;
}


SEL_ARG *
SEL_ARG::rb_insert(SEL_ARG *leaf)
{
  SEL_ARG *y,*par,*par2,*root;
  root= this; root->parent= 0;

  leaf->color=RED;
  while (leaf != root && (par= leaf->parent)->color == RED)
  {					// This can't be root or 1 level under
    if (par == (par2= leaf->parent->parent)->left)
    {
      y= par2->right;
      if (y->color == RED)
      {
	par->color=BLACK;
	y->color=BLACK;
	leaf=par2;
	leaf->color=RED;		/* And the loop continues */
      }
      else
      {
	if (leaf == par->right)
	{
	  left_rotate(&root,leaf->parent);
	  par=leaf;			/* leaf is now parent to old leaf */
	}
	par->color=BLACK;
	par2->color=RED;
	right_rotate(&root,par2);
	break;
      }
    }
    else
    {
      y= par2->left;
      if (y->color == RED)
      {
	par->color=BLACK;
	y->color=BLACK;
	leaf=par2;
	leaf->color=RED;		/* And the loop continues */
      }
      else
      {
	if (leaf == par->left)
	{
	  right_rotate(&root,par);
	  par=leaf;
	}
	par->color=BLACK;
	par2->color=RED;
	left_rotate(&root,par2);
	break;
      }
    }
  }
  root->color=BLACK;
  test_rb_tree(root,root->parent);
  return root;
}


SEL_ARG *rb_delete_fixup(SEL_ARG *root,SEL_ARG *key,SEL_ARG *par)
{
  SEL_ARG *x,*w;
  root->parent=0;

  x= key;
  while (x != root && x->color == SEL_ARG::BLACK)
  {
    if (x == par->left)
    {
      w=par->right;
      if (w->color == SEL_ARG::RED)
      {
	w->color=SEL_ARG::BLACK;
	par->color=SEL_ARG::RED;
	left_rotate(&root,par);
	w=par->right;
      }
      if (w->left->color == SEL_ARG::BLACK && w->right->color == SEL_ARG::BLACK)
      {
	w->color=SEL_ARG::RED;
	x=par;
      }
      else
      {
	if (w->right->color == SEL_ARG::BLACK)
	{
	  w->left->color=SEL_ARG::BLACK;
	  w->color=SEL_ARG::RED;
	  right_rotate(&root,w);
	  w=par->right;
	}
	w->color=par->color;
	par->color=SEL_ARG::BLACK;
	w->right->color=SEL_ARG::BLACK;
	left_rotate(&root,par);
	x=root;
	break;
      }
    }
    else
    {
      w=par->left;
      if (w->color == SEL_ARG::RED)
      {
	w->color=SEL_ARG::BLACK;
	par->color=SEL_ARG::RED;
	right_rotate(&root,par);
	w=par->left;
      }
      if (w->right->color == SEL_ARG::BLACK && w->left->color == SEL_ARG::BLACK)
      {
	w->color=SEL_ARG::RED;
	x=par;
      }
      else
      {
	if (w->left->color == SEL_ARG::BLACK)
	{
	  w->right->color=SEL_ARG::BLACK;
	  w->color=SEL_ARG::RED;
	  left_rotate(&root,w);
	  w=par->left;
	}
	w->color=par->color;
	par->color=SEL_ARG::BLACK;
	w->left->color=SEL_ARG::BLACK;
	right_rotate(&root,par);
	x=root;
	break;
      }
    }
    par=x->parent;
  }
  x->color=SEL_ARG::BLACK;
  return root;
}


	/* Test that the proporties for a red-black tree holds */

#ifdef EXTRA_DEBUG
int test_rb_tree(SEL_ARG *element,SEL_ARG *parent)
{
  int count_l,count_r;

  if (element == &null_element)
    return 0;					// Found end of tree
  if (element->parent != parent)
  {
    sql_print_error("Wrong tree: Parent doesn't point at parent");
    return -1;
  }
  if (element->color == SEL_ARG::RED &&
      (element->left->color == SEL_ARG::RED ||
       element->right->color == SEL_ARG::RED))
  {
    sql_print_error("Wrong tree: Found two red in a row");
    return -1;
  }
  if (element->left == element->right && element->left != &null_element)
  {						// Dummy test
    sql_print_error("Wrong tree: Found right == left");
    return -1;
  }
  count_l=test_rb_tree(element->left,element);
  count_r=test_rb_tree(element->right,element);
  if (count_l >= 0 && count_r >= 0)
  {
    if (count_l == count_r)
      return count_l+(element->color == SEL_ARG::BLACK);
    sql_print_error("Wrong tree: Incorrect black-count: %d - %d",
	    count_l,count_r);
  }
  return -1;					// Error, no more warnings
}

static ulong count_key_part_usage(SEL_ARG *root, SEL_ARG *key)
{
  ulong count= 0;
  for (root=root->first(); root ; root=root->next)
  {
    if (root->next_key_part)
    {
      if (root->next_key_part == key)
	count++;
      if (root->next_key_part->part < key->part)
	count+=count_key_part_usage(root->next_key_part,key);
    }
  }
  return count;
}


void SEL_ARG::test_use_count(SEL_ARG *root)
{
  if (this == root && use_count != 1)
  {
    sql_print_error("Note: Use_count: Wrong count %lu for root",use_count);
    return;
  }
  if (this->type != SEL_ARG::KEY_RANGE)
    return;
  uint e_count=0;
  for (SEL_ARG *pos=first(); pos ; pos=pos->next)
  {
    e_count++;
    if (pos->next_key_part)
    {
      ulong count=count_key_part_usage(root,pos->next_key_part);
      if (count > pos->next_key_part->use_count)
      {
	sql_print_error("Note: Use_count: Wrong count for key at %lx, %lu should be %lu",
			pos,pos->next_key_part->use_count,count);
	return;
      }
      pos->next_key_part->test_use_count(root);
    }
  }
  if (e_count != elements)
    sql_print_error("Warning: Wrong use count: %u for tree at %lx", e_count,
		    (gptr) this);
}

#endif



/*****************************************************************************
** Check how many records we will find by using the found tree
*****************************************************************************/

static ha_rows
check_quick_select(PARAM *param,uint idx,SEL_ARG *tree)
{
  ha_rows records;
  DBUG_ENTER("check_quick_select");

  if (!tree)
    DBUG_RETURN(HA_POS_ERROR);			// Can't use it
  param->max_key_part=0;
  param->range_count=0;
  if (tree->type == SEL_ARG::IMPOSSIBLE)
    DBUG_RETURN(0L);				// Impossible select. return
  if (tree->type != SEL_ARG::KEY_RANGE || tree->part != 0)
    DBUG_RETURN(HA_POS_ERROR);				// Don't use tree
  records=check_quick_keys(param,idx,tree,param->min_key,0,param->max_key,0);
  if (records != HA_POS_ERROR)
  {
    uint key=param->real_keynr[idx];
    param->table->quick_keys.set_bit(key);
    param->table->quick_rows[key]=records;
    param->table->quick_key_parts[key]=param->max_key_part+1;
  }
  DBUG_RETURN(records);
}


static ha_rows
check_quick_keys(PARAM *param,uint idx,SEL_ARG *key_tree,
		 char *min_key,uint min_key_flag, char *max_key,
		 uint max_key_flag)
{
  ha_rows records=0,tmp;

  param->max_key_part=max(param->max_key_part,key_tree->part);
  if (key_tree->left != &null_element)
  {
    records=check_quick_keys(param,idx,key_tree->left,min_key,min_key_flag,
			     max_key,max_key_flag);
    if (records == HA_POS_ERROR)			// Impossible
      return records;
  }

  uint tmp_min_flag,tmp_max_flag,keynr;
  char *tmp_min_key=min_key,*tmp_max_key=max_key;

  key_tree->store(param->key[idx][key_tree->part].part_length,
		  &tmp_min_key,min_key_flag,&tmp_max_key,max_key_flag);
  uint min_key_length= (uint) (tmp_min_key- param->min_key);
  uint max_key_length= (uint) (tmp_max_key- param->max_key);

  if (key_tree->next_key_part &&
      key_tree->next_key_part->part == key_tree->part+1 &&
      key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
  {						// const key as prefix
    if (min_key_length == max_key_length &&
	!memcmp(min_key,max_key, (uint) (tmp_max_key - max_key)) &&
	!key_tree->min_flag && !key_tree->max_flag)
    {
      tmp=check_quick_keys(param,idx,key_tree->next_key_part,
			   tmp_min_key, min_key_flag | key_tree->min_flag,
			   tmp_max_key, max_key_flag | key_tree->max_flag);
      goto end;					// Ugly, but efficient
    }
    tmp_min_flag=key_tree->min_flag;
    tmp_max_flag=key_tree->max_flag;
    if (!tmp_min_flag)
      key_tree->next_key_part->store_min_key(param->key[idx], &tmp_min_key,
					     &tmp_min_flag);
    if (!tmp_max_flag)
      key_tree->next_key_part->store_max_key(param->key[idx], &tmp_max_key,
					     &tmp_max_flag);
    min_key_length= (uint) (tmp_min_key- param->min_key);
    max_key_length= (uint) (tmp_max_key- param->max_key);
  }
  else
  {
    tmp_min_flag=min_key_flag | key_tree->min_flag;
    tmp_max_flag=max_key_flag | key_tree->max_flag;
  }

  keynr=param->real_keynr[idx];
  param->range_count++;
  if (!tmp_min_flag && ! tmp_max_flag &&
      (uint) key_tree->part+1 == param->table->key_info[keynr].key_parts &&
      (param->table->key_info[keynr].flags & HA_NOSAME) &&
      min_key_length == max_key_length &&
      !memcmp(param->min_key,param->max_key,min_key_length))
    tmp=1;					// Max one record
  else
  {
    if (tmp_min_flag & GEOM_FLAG)
    {
      tmp= param->table->file->
	records_in_range((int) keynr, (byte*)(param->min_key),
			 min_key_length,
			 (ha_rkey_function)(tmp_min_flag ^ GEOM_FLAG),
			 (byte *)NullS, 0, HA_READ_KEY_EXACT);
    }
    else
    {
      tmp=param->table->file->
	records_in_range((int) keynr,
			 (byte*) (!min_key_length ? NullS :
				  param->min_key),
			 min_key_length,
                         tmp_min_flag & NEAR_MIN ?
			  HA_READ_AFTER_KEY : HA_READ_KEY_EXACT,
			 (byte*) (!max_key_length ? NullS :
				  param->max_key),
			 max_key_length,
			 (tmp_max_flag & NEAR_MAX ?
			  HA_READ_BEFORE_KEY : HA_READ_AFTER_KEY));
    }
  }
 end:
  if (tmp == HA_POS_ERROR)			// Impossible range
    return tmp;
  records+=tmp;
  if (key_tree->right != &null_element)
  {
    tmp=check_quick_keys(param,idx,key_tree->right,min_key,min_key_flag,
			 max_key,max_key_flag);
    if (tmp == HA_POS_ERROR)
      return tmp;
    records+=tmp;
  }
  return records;
}


/****************************************************************************
** change a tree to a structure to be used by quick_select
** This uses it's own malloc tree
** The caller should call QUICK_SELCT::init for returned quick select
****************************************************************************/
QUICK_RANGE_SELECT *
get_quick_select(PARAM *param,uint idx,SEL_ARG *key_tree,
                 MEM_ROOT *parent_alloc)
{
  QUICK_RANGE_SELECT *quick;
  DBUG_ENTER("get_quick_select");
  if ((quick=new QUICK_RANGE_SELECT(param->thd, param->table,
                                    param->real_keynr[idx],test(parent_alloc),
                                    parent_alloc)))
  {
    if (quick->error ||
	get_quick_keys(param,quick,param->key[idx],key_tree,param->min_key,0,
		       param->max_key,0))
    {
      delete quick;
      quick=0;
    }
    else
    {
      quick->key_parts=(KEY_PART*)
        memdup_root(parent_alloc? parent_alloc : &quick->alloc,
                    (char*) param->key[idx],
                    sizeof(KEY_PART)*
                    param->table->key_info[param->real_keynr[idx]].key_parts);
    }
  }
  DBUG_RETURN(quick);
}


/*
** Fix this to get all possible sub_ranges
*/
bool
get_quick_keys(PARAM *param,QUICK_RANGE_SELECT *quick,KEY_PART *key,
	       SEL_ARG *key_tree,char *min_key,uint min_key_flag,
	       char *max_key, uint max_key_flag)
{
  QUICK_RANGE *range;
  uint flag;

  if (key_tree->left != &null_element)
  {
    if (get_quick_keys(param,quick,key,key_tree->left,
		       min_key,min_key_flag, max_key, max_key_flag))
      return 1;
  }
  char *tmp_min_key=min_key,*tmp_max_key=max_key;
  key_tree->store(key[key_tree->part].part_length,
		  &tmp_min_key,min_key_flag,&tmp_max_key,max_key_flag);

  if (key_tree->next_key_part &&
      key_tree->next_key_part->part == key_tree->part+1 &&
      key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
  {						  // const key as prefix
    if (!((tmp_min_key - min_key) != (tmp_max_key - max_key) ||
	  memcmp(min_key,max_key, (uint) (tmp_max_key - max_key)) ||
	  key_tree->min_flag || key_tree->max_flag))
    {
      if (get_quick_keys(param,quick,key,key_tree->next_key_part,
			 tmp_min_key, min_key_flag | key_tree->min_flag,
			 tmp_max_key, max_key_flag | key_tree->max_flag))
	return 1;
      goto end;					// Ugly, but efficient
    }
    {
      uint tmp_min_flag=key_tree->min_flag,tmp_max_flag=key_tree->max_flag;
      if (!tmp_min_flag)
	key_tree->next_key_part->store_min_key(key, &tmp_min_key,
					       &tmp_min_flag);
      if (!tmp_max_flag)
	key_tree->next_key_part->store_max_key(key, &tmp_max_key,
					       &tmp_max_flag);
      flag=tmp_min_flag | tmp_max_flag;
    }
  }
  else
  {
    flag = (key_tree->min_flag & GEOM_FLAG) ?
      key_tree->min_flag : key_tree->min_flag | key_tree->max_flag;
  }

  /*
    Ensure that some part of min_key and max_key are used.  If not,
    regard this as no lower/upper range
  */
  if ((flag & GEOM_FLAG) == 0)
  {
    if (tmp_min_key != param->min_key)
      flag&= ~NO_MIN_RANGE;
    else
      flag|= NO_MIN_RANGE;
    if (tmp_max_key != param->max_key)
      flag&= ~NO_MAX_RANGE;
    else
      flag|= NO_MAX_RANGE;
  }
  if (flag == 0)
  {
    uint length= (uint) (tmp_min_key - param->min_key);
    if (length == (uint) (tmp_max_key - param->max_key) &&
	!memcmp(param->min_key,param->max_key,length))
    {
      KEY *table_key=quick->head->key_info+quick->index;
      flag=EQ_RANGE;
      if (table_key->flags & HA_NOSAME && key->part == table_key->key_parts-1)
      {
	if (!(table_key->flags & HA_NULL_PART_KEY) ||
	    !null_part_in_key(key,
			      param->min_key,
			      (uint) (tmp_min_key - param->min_key)))
	  flag|= UNIQUE_RANGE;
	else
	  flag|= NULL_RANGE;
      }
    }
  }

  /* Get range for retrieving rows in QUICK_SELECT::get_next */
  if (!(range= new QUICK_RANGE(param->min_key,
			       (uint) (tmp_min_key - param->min_key),
			       param->max_key,
			       (uint) (tmp_max_key - param->max_key),
			       flag)))
    return 1;			// out of memory

  set_if_bigger(quick->max_used_key_length,range->min_length);
  set_if_bigger(quick->max_used_key_length,range->max_length);
  set_if_bigger(quick->used_key_parts, (uint) key_tree->part+1);
  quick->ranges.push_back(range);

 end:
  if (key_tree->right != &null_element)
    return get_quick_keys(param,quick,key,key_tree->right,
			  min_key,min_key_flag,
			  max_key,max_key_flag);
  return 0;
}

/*
  Return 1 if there is only one range and this uses the whole primary key
*/

bool QUICK_RANGE_SELECT::unique_key_range()
{
  if (ranges.elements == 1)
  {
    QUICK_RANGE *tmp;
    if (((tmp=ranges.head())->flag & (EQ_RANGE | NULL_RANGE)) == EQ_RANGE)
    {
      KEY *key=head->key_info+index;
      return ((key->flags & HA_NOSAME) &&
	      key->key_length == tmp->min_length);
    }
  }
  return 0;
}


/* Returns true if any part of the key is NULL */

static bool null_part_in_key(KEY_PART *key_part, const char *key, uint length)
{
  for (const char *end=key+length ; 
       key < end;
       key+= key_part++->part_length)
  {
    if (key_part->null_bit)
    {
      if (*key++)
	return 1;
    }
  }
  return 0;
}

/****************************************************************************
** Create a QUICK RANGE based on a key
****************************************************************************/

QUICK_RANGE_SELECT *get_quick_select_for_ref(THD *thd, TABLE *table, 
                                             TABLE_REF *ref)
{
  table->file->index_end();			// Remove old cursor
  QUICK_RANGE_SELECT *quick=new QUICK_RANGE_SELECT(thd, table, ref->key, 1);  
  KEY *key_info = &table->key_info[ref->key];
  KEY_PART *key_part;
  uint part;

  if (!quick)
    return 0;			/* no ranges found */
  if (quick->init())
  {
    delete quick;
    return 0;
  }

  if (cp_buffer_from_ref(ref))
  {
    if (thd->is_fatal_error)
      return 0;					// out of memory
    return quick;				// empty range
  }

  QUICK_RANGE *range= new QUICK_RANGE();
  if (!range)
    goto err;			// out of memory

  range->min_key=range->max_key=(char*) ref->key_buff;
  range->min_length=range->max_length=ref->key_length;
  range->flag= ((ref->key_length == key_info->key_length &&
		 (key_info->flags & HA_NOSAME)) ? EQ_RANGE : 0);

  if (!(quick->key_parts=key_part=(KEY_PART *)
	alloc_root(&quick->alloc,sizeof(KEY_PART)*ref->key_parts)))
    goto err;

  for (part=0 ; part < ref->key_parts ;part++,key_part++)
  {
    key_part->part=part;
    key_part->field=        key_info->key_part[part].field;
    key_part->part_length=  key_info->key_part[part].length;
    if (key_part->field->type() == FIELD_TYPE_BLOB)
      key_part->part_length+=HA_KEY_BLOB_LENGTH;
    key_part->null_bit=     key_info->key_part[part].null_bit;
  }
  if (!quick->ranges.push_back(range))
    return quick;

err:
  delete quick;
  return 0;
}


#define MEM_STRIP_BUF_SIZE current_thd->variables.sortbuff_size
/*
  Fetch all row ids into unique.
*/
int QUICK_INDEX_MERGE_SELECT::prepare_unique()
{
  int result;
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::prepare_unique");
  
  /* we're going to just read rowids */
  head->file->extra(HA_EXTRA_KEYREAD);

  unique= new Unique(refposcmp2, (void *) &head->file->ref_length,
                     head->file->ref_length,
                     MEM_STRIP_BUF_SIZE);  
  if (!unique)
    DBUG_RETURN(1);
  do
  { 
    while ((result= cur_quick_select->get_next()) == HA_ERR_END_OF_FILE)
    {      
      cur_quick_select= cur_quick_it++;
      if (!cur_quick_select)
        break;
      cur_quick_select->init();
      if (cur_quick_select->reset())
        DBUG_RETURN(1);
    }

    if (result)
    {      
      /* 
        table read error (including HA_ERR_END_OF_FILE on last quick select
        in index_merge)
      */
      if (result != HA_ERR_END_OF_FILE)
      {
        DBUG_RETURN(result);
      }
      else
        break;
    }
    
    if (thd->killed)
      DBUG_RETURN(1);

    cur_quick_select->file->position(cur_quick_select->record);
    if (unique->unique_add((char*)cur_quick_select->file->ref))
      DBUG_RETURN(1);

  }while(true);  

  /* ok, all row ids are in Unique */
  result= unique->get(head);

  /* index_merge currently doesn't support "using index" at all */
  head->file->extra(HA_EXTRA_NO_KEYREAD);
  DBUG_RETURN(result);
}

int QUICK_INDEX_MERGE_SELECT::get_next()
{
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::get_next");
  
  DBUG_PRINT("QUICK_INDEX_MERGE_SELECT", 
             ("ERROR: index merge error: get_next should not be called "));
  DBUG_ASSERT(0);
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}

	/* get next possible record using quick-struct */

int QUICK_RANGE_SELECT::get_next()
{
  DBUG_ENTER("QUICK_RANGE_SELECT::get_next");

  for (;;)
  {
    int result;
    if (range)
    {						// Already read through key
/*       result=((range->flag & EQ_RANGE) ?
	       file->index_next_same(record, (byte*) range->min_key,
				     range->min_length) :
	       file->index_next(record));
*/
       result=((range->flag & (EQ_RANGE | GEOM_FLAG) ) ?
	       file->index_next_same(record, (byte*) range->min_key,
				     range->min_length) :
	       file->index_next(record));

      if (!result)
      {
	if ((range->flag & GEOM_FLAG) || !cmp_next(*it.ref()))
	  DBUG_RETURN(0);
      }
      else if (result != HA_ERR_END_OF_FILE)
	DBUG_RETURN(result);
    }

    if (!(range=it++))
      DBUG_RETURN(HA_ERR_END_OF_FILE);		// All ranges used

    if (range->flag & GEOM_FLAG)
    {
      if ((result = file->index_read(record,
				     (byte*) (range->min_key),
				     range->min_length,
				     (ha_rkey_function)(range->flag ^
							GEOM_FLAG))))
      {
        if (result != HA_ERR_KEY_NOT_FOUND)
	  DBUG_RETURN(result);
        range=0;				// Not found, to next range
        continue;
      }
      DBUG_RETURN(0);
    }

    if (range->flag & NO_MIN_RANGE)		// Read first record
    {
      int local_error;
      if ((local_error=file->index_first(record)))
	DBUG_RETURN(local_error);		// Empty table
      if (cmp_next(range) == 0)
	DBUG_RETURN(0);
      range=0;			// No matching records; go to next range
      continue;
    }
    if ((result = file->index_read(record,
				   (byte*) (range->min_key +
					    test(range->flag & GEOM_FLAG)),
				   range->min_length,
				   (range->flag & NEAR_MIN) ?
				   HA_READ_AFTER_KEY:
				   (range->flag & EQ_RANGE) ?
				   HA_READ_KEY_EXACT :
				   HA_READ_KEY_OR_NEXT)))

    {
      if (result != HA_ERR_KEY_NOT_FOUND)
	DBUG_RETURN(result);
      range=0;					// Not found, to next range
      continue;
    }
    if (cmp_next(range) == 0)
    {
      if (range->flag == (UNIQUE_RANGE | EQ_RANGE))
	range=0;				// Stop searching
      DBUG_RETURN(0);				// Found key is in range
    }
    range=0;					// To next range
  }
}


/*
  Compare if found key is over max-value
  Returns 0 if key <= range->max_key
*/

int QUICK_RANGE_SELECT::cmp_next(QUICK_RANGE *range_arg)
{
  if (range_arg->flag & NO_MAX_RANGE)
    return 0;					/* key can't be to large */

  KEY_PART *key_part=key_parts;
  for (char *key=range_arg->max_key, *end=key+range_arg->max_length;
       key < end;
       key+= key_part++->part_length)
  {
    int cmp;
    if (key_part->null_bit)
    {
      if (*key++)
      {
	if (!key_part->field->is_null())
	  return 1;
	continue;
      }
      else if (key_part->field->is_null())
	return 0;
    }
    if ((cmp=key_part->field->key_cmp((byte*) key, key_part->part_length)) < 0)
      return 0;
    if (cmp > 0)
      return 1;
  }
  return (range_arg->flag & NEAR_MAX) ? 1 : 0;		// Exact match
}


/*
  This is a hack: we inherit from QUICK_SELECT so that we can use the
  get_next() interface, but we have to hold a pointer to the original
  QUICK_SELECT because its data are used all over the place.  What
  should be done is to factor out the data that is needed into a base
  class (QUICK_SELECT), and then have two subclasses (_ASC and _DESC)
  which handle the ranges and implement the get_next() function.  But
  for now, this seems to work right at least.
 */

QUICK_SELECT_DESC::QUICK_SELECT_DESC(QUICK_RANGE_SELECT *q, 
                                     uint used_key_parts)
 : QUICK_RANGE_SELECT(*q), rev_it(rev_ranges)
{
  bool not_read_after_key = file->table_flags() & HA_NOT_READ_AFTER_KEY;
  QUICK_RANGE *r;

  it.rewind();
  for (r = it++; r; r = it++)
  {
    rev_ranges.push_front(r);
    if (not_read_after_key && range_reads_after_key(r))
    {
      it.rewind();				// Reset range
      error = HA_ERR_UNSUPPORTED;
      dont_free=1;				// Don't free memory from 'q'
      return;
    }
  }
  /* Remove EQ_RANGE flag for keys that are not using the full key */
  for (r = rev_it++; r; r = rev_it++)
  {
    if ((r->flag & EQ_RANGE) &&
	head->key_info[index].key_length != r->max_length)
      r->flag&= ~EQ_RANGE;
  }
  rev_it.rewind();
  q->dont_free=1;				// Don't free shared mem
  delete q;
}


int QUICK_SELECT_DESC::get_next()
{
  DBUG_ENTER("QUICK_SELECT_DESC::get_next");

  /* The max key is handled as follows:
   *   - if there is NO_MAX_RANGE, start at the end and move backwards
   *   - if it is an EQ_RANGE, which means that max key covers the entire
   *     key, go directly to the key and read through it (sorting backwards is
   *     same as sorting forwards)
   *   - if it is NEAR_MAX, go to the key or next, step back once, and
   *     move backwards
   *   - otherwise (not NEAR_MAX == include the key), go after the key,
   *     step back once, and move backwards
   */

  for (;;)
  {
    int result;
    if (range)
    {						// Already read through key
      result = ((range->flag & EQ_RANGE)
		? file->index_next_same(record, (byte*) range->min_key,
					range->min_length) :
		file->index_prev(record));
      if (!result)
      {
	if (cmp_prev(*rev_it.ref()) == 0)
	  DBUG_RETURN(0);
      }
      else if (result != HA_ERR_END_OF_FILE)
	DBUG_RETURN(result);
    }

    if (!(range=rev_it++))
      DBUG_RETURN(HA_ERR_END_OF_FILE);		// All ranges used

    if (range->flag & NO_MAX_RANGE)		// Read last record
    {
      int local_error;
      if ((local_error=file->index_last(record)))
	DBUG_RETURN(local_error);		// Empty table
      if (cmp_prev(range) == 0)
	DBUG_RETURN(0);
      range=0;			// No matching records; go to next range
      continue;
    }

    if (range->flag & EQ_RANGE)
    {
      result = file->index_read(record, (byte*) range->max_key,
				range->max_length, HA_READ_KEY_EXACT);
    }
    else
    {
      DBUG_ASSERT(range->flag & NEAR_MAX || range_reads_after_key(range));
#ifndef NOT_IMPLEMENTED_YET
      result=file->index_read(record, (byte*) range->max_key,
			      range->max_length,
			      ((range->flag & NEAR_MAX) ?
			       HA_READ_BEFORE_KEY : HA_READ_PREFIX_LAST_OR_PREV));
#else
      /* Heikki changed Sept 11, 2002: since InnoDB does not store the cursor
	 position if READ_KEY_EXACT is used to a primary key with all
	 key columns specified, we must use below HA_READ_KEY_OR_NEXT,
	 so that InnoDB stores the cursor position and is able to move
	 the cursor one step backward after the search. */

      /* Note: even if max_key is only a prefix, HA_READ_AFTER_KEY will
       * do the right thing - go past all keys which match the prefix */

      result=file->index_read(record, (byte*) range->max_key,
			      range->max_length,
			      ((range->flag & NEAR_MAX) ?
			       HA_READ_KEY_OR_NEXT : HA_READ_AFTER_KEY));
      result = file->index_prev(record);
#endif
    }
    if (result)
    {
      if (result != HA_ERR_KEY_NOT_FOUND)
	DBUG_RETURN(result);
      range=0;					// Not found, to next range
      continue;
    }
    if (cmp_prev(range) == 0)
    {
      if (range->flag == (UNIQUE_RANGE | EQ_RANGE))
	range = 0;				// Stop searching
      DBUG_RETURN(0);				// Found key is in range
    }
    range = 0;					// To next range
  }
}


/*
  Returns 0 if found key is inside range (found key >= range->min_key).
*/

int QUICK_SELECT_DESC::cmp_prev(QUICK_RANGE *range_arg)
{
  if (range_arg->flag & NO_MIN_RANGE)
    return 0;					/* key can't be to small */

  KEY_PART *key_part = key_parts;
  for (char *key = range_arg->min_key, *end = key + range_arg->min_length;
       key < end;
       key += key_part++->part_length)
  {
    int cmp;
    if (key_part->null_bit)
    {
      // this key part allows null values; NULL is lower than everything else
      if (*key++)
      {
	// the range is expecting a null value
	if (!key_part->field->is_null())
	  return 0;	// not null -- still inside the range
	continue;	// null -- exact match, go to next key part
      }
      else if (key_part->field->is_null())
	return 1;	// null -- outside the range
    }
    if ((cmp = key_part->field->key_cmp((byte*) key,
					key_part->part_length)) > 0)
      return 0;
    if (cmp < 0)
      return 1;
  }
  return (range_arg->flag & NEAR_MIN) ? 1 : 0;		// Exact match
}


/*
 * True if this range will require using HA_READ_AFTER_KEY
   See comment in get_next() about this
 */

bool QUICK_SELECT_DESC::range_reads_after_key(QUICK_RANGE *range_arg)
{
  return ((range_arg->flag & (NO_MAX_RANGE | NEAR_MAX)) ||
	  !(range_arg->flag & EQ_RANGE) ||
	  head->key_info[index].key_length != range_arg->max_length) ? 1 : 0;
}


/* True if we are reading over a key that may have a NULL value */

#ifdef NOT_USED
bool QUICK_SELECT_DESC::test_if_null_range(QUICK_RANGE *range_arg,
					   uint used_key_parts)
{
  uint offset,end;
  KEY_PART *key_part = key_parts,
           *key_part_end= key_part+used_key_parts;

  for (offset= 0,  end = min(range_arg->min_length, range_arg->max_length) ;
       offset < end && key_part != key_part_end ;
       offset += key_part++->part_length)
  {
    uint null_length=test(key_part->null_bit);
    if (!memcmp((char*) range_arg->min_key+offset,
		(char*) range_arg->max_key+offset,
		key_part->part_length + null_length))
    {
      offset+=null_length;
      continue;
    }
    if (null_length && range_arg->min_key[offset])
      return 1;				// min_key is null and max_key isn't
    // Range doesn't cover NULL. This is ok if there is no more null parts
    break;
  }
  /*
    If the next min_range is > NULL, then we can use this, even if
    it's a NULL key
    Example:  SELECT * FROM t1 WHERE a = 2 AND b >0 ORDER BY a DESC,b DESC;

  */
  if (key_part != key_part_end && key_part->null_bit)
  {
    if (offset >= range_arg->min_length || range_arg->min_key[offset])
      return 1;					// Could be null
    key_part++;
  }
  /*
    If any of the key parts used in the ORDER BY could be NULL, we can't
    use the key to sort the data.
  */
  for (; key_part != key_part_end ; key_part++)
    if (key_part->null_bit)
      return 1;					// Covers null part
  return 0;
}
#endif


/*****************************************************************************
** Print a quick range for debugging
** TODO:
** This should be changed to use a String to store each row instead
** of locking the DEBUG stream !
*****************************************************************************/

#ifndef DBUG_OFF

static void
print_key(KEY_PART *key_part,const char *key,uint used_length)
{
  char buff[1024];
  String tmp(buff,sizeof(buff),&my_charset_bin);

  for (uint length=0;
       length < used_length ;
       length+=key_part->part_length, key+=key_part->part_length, key_part++)
  {
    Field *field=key_part->field;
    if (length != 0)
      fputc('/',DBUG_FILE);
    if (field->real_maybe_null())
    {
      length++;				// null byte is not in part_length
      if (*key++)
      {
	fwrite("NULL",sizeof(char),4,DBUG_FILE);
	continue;
      }
    }
    field->set_key_image((char*) key,key_part->part_length -
			 ((field->type() == FIELD_TYPE_BLOB) ?
			  HA_KEY_BLOB_LENGTH : 0),
			 field->charset());
    field->val_str(&tmp,&tmp);
    fwrite(tmp.ptr(),sizeof(char),tmp.length(),DBUG_FILE);
  }
}

static void print_quick_sel_imerge(QUICK_INDEX_MERGE_SELECT *quick, 
                            const key_map *needed_reg)
{
  DBUG_ENTER("print_param");
  if (! _db_on_ || !quick)
    DBUG_VOID_RETURN;

  List_iterator_fast<QUICK_RANGE_SELECT> it(quick->quick_selects);
  QUICK_RANGE_SELECT* quick_range_sel;
  while ((quick_range_sel= it++))
  {
    print_quick_sel_range(quick_range_sel, needed_reg);
  }
  DBUG_VOID_RETURN;
}

void print_quick_sel_range(QUICK_RANGE_SELECT *quick,const key_map *needed_reg)
{
  QUICK_RANGE *range;
  char buf[MAX_KEY/8+1];
  DBUG_ENTER("print_param");
  if (! _db_on_ || !quick)
    DBUG_VOID_RETURN;

  List_iterator<QUICK_RANGE> li(quick->ranges);
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE,"Used quick_range on key: %d (other_keys: 0x%s):\n",
	  quick->index, needed_reg->print(buf));
  while ((range=li++))
  {
    if (!(range->flag & NO_MIN_RANGE))
    {
      print_key(quick->key_parts,range->min_key,range->min_length);
      if (range->flag & NEAR_MIN)
	fputs(" < ",DBUG_FILE);
      else
	fputs(" <= ",DBUG_FILE);
    }
    fputs("X",DBUG_FILE);

    if (!(range->flag & NO_MAX_RANGE))
    {
      if (range->flag & NEAR_MAX)
	fputs(" < ",DBUG_FILE);
      else
	fputs(" <= ",DBUG_FILE);
      print_key(quick->key_parts,range->max_key,range->max_length);
    }
    fputs("\n",DBUG_FILE);
  }
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}

#endif

/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<QUICK_RANGE>;
template class List_iterator<QUICK_RANGE>;
#endif
