/* Copyright (C) 2000 MySQL AB

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
  UNION  of select's
  UNION's  were introduced by Monty and Sinisa <sinisa@mysql.com>
*/


#include "mysql_priv.h"
#include "sql_select.h"

int mysql_union(THD *thd, LEX *lex, select_result *result)
{
  DBUG_ENTER("mysql_union");
  SELECT_LEX_UNIT *unit= &lex->unit;
  int res= 0;
  if (!(res= unit->prepare(thd, result)))
    res= unit->exec();
  res|= unit->cleanup();
  DBUG_RETURN(res);
}


/***************************************************************************
** store records in temporary table for UNION
***************************************************************************/

select_union::select_union(TABLE *table_par)
    :table(table_par)
{
  bzero((char*) &info,sizeof(info));
  /*
    We can always use DUP_IGNORE because the temporary table will only
    contain a unique key if we are using not using UNION ALL
  */
  info.handle_duplicates= DUP_IGNORE;
}

select_union::~select_union()
{
}


int select_union::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  unit= u;
  if (save_time_stamp && list.elements != table->fields)
  {
    my_message(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT,
	       ER(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT),MYF(0));
    return -1;
  }
  return 0;
}

bool select_union::send_data(List<Item> &values)
{
  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
  }
  fill_record(table->field,values);
  if ((write_record(table,&info)))
  {
    if (create_myisam_from_heap(table, tmp_table_param, info.last_errno, 0))
      return 1;
  }
  return 0;
}

bool select_union::send_eof()
{
  return 0;
}

bool select_union::flush()
{
  int error;
  if ((error=table->file->extra(HA_EXTRA_NO_CACHE)))
  {
    table->file->print_error(error,MYF(0));
    ::send_error(thd);
    return 1;
  }
  return 0;
}

typedef JOIN * JOIN_P;
int st_select_lex_unit::prepare(THD *thd, select_result *result)
{
  DBUG_ENTER("st_select_lex_unit::prepare");

  if (prepared)
    DBUG_RETURN(0);
  prepared= 1;
  union_result=0;
  res= 0;
  found_rows_for_union= false;
  TMP_TABLE_PARAM tmp_table_param;
  this->thd= thd;
  this->result= result;
  SELECT_LEX *lex_select_save= thd->lex.select, *sl;

  /* Global option */
  if (((void*)(global_parameters)) == ((void*)this))
  {
    found_rows_for_union= first_select()->options & OPTION_FOUND_ROWS && 
      global_parameters->select_limit;
    if (found_rows_for_union)
      first_select()->options ^=  OPTION_FOUND_ROWS;
  }
  item_list.empty();
  {
    Item *item;
    List_iterator<Item> it(first_select()->item_list);
    TABLE_LIST *first_table= (TABLE_LIST*) first_select()->table_list.first;

    /* Create a list of items that will be in the result set */
    while ((item= it++))
      if (item_list.push_back(item))
	goto err;
    if (setup_fields(thd,first_table,item_list,0,0,1))
      goto err;
  }

  bzero((char*) &tmp_table_param,sizeof(tmp_table_param));
  tmp_table_param.field_count=item_list.elements;
  if (!(table= create_tmp_table(thd, &tmp_table_param, item_list,
				(ORDER*) 0, !union_option,
				1, 0,
				(first_select()->options | thd->options |
				 TMP_TABLE_ALL_COLUMNS),
				this)))
    goto err;
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  bzero((char*) &result_table_list,sizeof(result_table_list));
  result_table_list.db= (char*) "";
  result_table_list.real_name=result_table_list.alias= (char*) "union";
  result_table_list.table=table;

  if (!(union_result=new select_union(table)))
    goto err;

  union_result->save_time_stamp=1;
  union_result->tmp_table_param=&tmp_table_param;

  // prepare selects
  joins.empty();
  for (sl= first_select(); sl; sl= sl->next_select())
  {
    JOIN *join= new JOIN(thd, sl->item_list, 
			 sl->options | thd->options | SELECT_NO_UNLOCK,
			 union_result);
    joins.push_back(new JOIN_P(join));
    thd->lex.select=sl;
    offset_limit_cnt= sl->offset_limit;
    select_limit_cnt= sl->select_limit+sl->offset_limit;
    if (select_limit_cnt < sl->select_limit)
      select_limit_cnt= HA_POS_ERROR;		// no limit
    if (select_limit_cnt == HA_POS_ERROR)
      sl->options&= ~OPTION_FOUND_ROWS;

    res= join->prepare((TABLE_LIST*) sl->table_list.first,
		       sl->where,
		       (sl->braces) ? 
		       (ORDER *)sl->order_list.first : (ORDER *) 0,
		       (ORDER*) sl->group_list.first,
		       sl->having,
		       (ORDER*) NULL,
		       sl, this, 0);
    if (res | thd->fatal_error)
      goto err;
  }
  thd->lex.select= lex_select_save;
  DBUG_RETURN(res | thd->fatal_error);
err:
  thd->lex.select= lex_select_save;
  DBUG_RETURN(-1);
}

int st_select_lex_unit::exec()
{
  DBUG_ENTER("st_select_lex_unit::exec");
  SELECT_LEX *lex_select_save= thd->lex.select;

  if(depended || !item || !item->assigned())
  {
    if (optimized && item && item->assigned())
      item->assigned(0); // We will reinit & rexecute unit
    for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
    {
      thd->lex.select=sl;
      offset_limit_cnt= sl->offset_limit;
      select_limit_cnt= sl->select_limit+sl->offset_limit;
      if (select_limit_cnt < sl->select_limit)
	select_limit_cnt= HA_POS_ERROR;		// no limit
      if (select_limit_cnt == HA_POS_ERROR)
	sl->options&= ~OPTION_FOUND_ROWS;

      if (!optimized)
	res= sl->join->optimize();
      else
	res= sl->join->reinit();

      if (!res)
      {
	sl->join->exec();
	res= sl->join->error;
      }
      if (res)
      {
	thd->lex.select= lex_select_save;
	DBUG_RETURN(res);
      }
    }
    optimized= 1;
  }

  if (union_result->flush())
  {
    thd->lex.select= lex_select_save;
    DBUG_RETURN(1);
  }

  /* Send result to 'result' */
  thd->lex.select = first_select();
  res =-1;
  {
    /* Create a list of fields in the temporary table */
    List_iterator<Item> it(item_list);
    Field **field;
#if 0
    List<Item_func_match> ftfunc_list;
    ftfunc_list.empty();
#else
    List<Item_func_match> empty_list;
    empty_list.empty();
    thd->lex.select_lex.ftfunc_list= &empty_list;
#endif

    for (field=table->field ; *field ; field++)
    {
      (void) it++;
      (void) it.replace(new Item_field(*field));
    }
    if (!thd->fatal_error)			// Check if EOM
    {
      offset_limit_cnt= global_parameters->offset_limit;
      select_limit_cnt= global_parameters->select_limit+
	global_parameters->offset_limit;
      if (select_limit_cnt < global_parameters->select_limit)
	select_limit_cnt= HA_POS_ERROR;		// no limit
      if (select_limit_cnt == HA_POS_ERROR)
	thd->options&= ~OPTION_FOUND_ROWS;
      res= mysql_select(thd,&result_table_list,
			item_list, NULL,
			(ORDER*)global_parameters->order_list.first,
			(ORDER*) NULL, NULL, (ORDER*) NULL,
			thd->options, result, this, first_select(), 1);
      if (found_rows_for_union && !res)
	thd->limit_found_rows = (ulonglong)table->file->records;
    }
  }
  thd->lex.select_lex.ftfunc_list= &thd->lex.select_lex.ftfunc_list_alloc;
  thd->lex.select= lex_select_save;
  DBUG_RETURN(res);
}

int st_select_lex_unit::cleanup()
{
  DBUG_ENTER("st_select_lex_unit::cleanup");
  if (union_result)
  {
    delete union_result;
    free_tmp_table(thd,table);
    table= 0; // Safety
  }
  List_iterator<JOIN*> j(joins);
  JOIN** join;
  while ((join= j++))
  {
    (*join)->cleanup(thd);
    delete *join;
    delete join;
  }
  joins.empty();
  DBUG_RETURN(0);
}
