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

int mysql_union(THD *thd, LEX *lex, select_result *result,
		SELECT_LEX_UNIT *unit, bool tables_and_fields_initied)
{
  DBUG_ENTER("mysql_union");
  int res= 0;
  if (!(res= unit->prepare(thd, result, tables_and_fields_initied)))
    res= unit->exec();
  res|= unit->cleanup();
  DBUG_RETURN(res);
}


/***************************************************************************
** store records in temporary table for UNION
***************************************************************************/

select_union::select_union(TABLE *table_par)
  :table(table_par), not_describe(0)
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
  if (not_describe && list.elements != table->fields)
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
  if (thd->net.report_error || write_record(table,&info))
  {
    if (thd->net.last_errno == ER_RECORD_FILE_FULL)
    {
      thd->clear_error(); // do not report user about table overflow
      if (create_myisam_from_heap(thd, table, tmp_table_param,
				  info.last_errno, 0))
	return 1;
    }
    else
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

int st_select_lex_unit::prepare(THD *thd, select_result *result,
				bool tables_and_fields_initied)
{
  DBUG_ENTER("st_select_lex_unit::prepare");

  if (prepared)
    DBUG_RETURN(0);
  prepared= 1;
  res= 0;
  found_rows_for_union= 0;
  TMP_TABLE_PARAM tmp_table_param;
  this->result= result;
  t_and_f= tables_and_fields_initied;
  SELECT_LEX_NODE *lex_select_save= thd->lex.current_select;
  SELECT_LEX *sl;

  thd->lex.current_select= sl= first_select();
  /* Global option */
  if (((void*)(global_parameters)) == ((void*)this))
  {
    found_rows_for_union= first_select()->options & OPTION_FOUND_ROWS && 
      global_parameters->select_limit;
    if (found_rows_for_union)
      first_select()->options ^=  OPTION_FOUND_ROWS;
  }
  if (t_and_f)
  {
    // Item list and tables will be initialized by mysql_derived
    item_list= sl->item_list;
  }
  else
  {
    item_list.empty();
    TABLE_LIST *first_table= (TABLE_LIST*) first_select()->table_list.first;

    if (setup_tables(first_table) ||
	setup_wild(thd, first_table, sl->item_list, 0, sl->with_wild))
      goto err;
    List_iterator<Item> it(sl->item_list);	
    Item *item;
    while((item=it++))
      item->maybe_null=1;
    item_list= sl->item_list;
    sl->with_wild= 0;
    if (setup_ref_array(thd, &sl->ref_pointer_array, 
			(item_list.elements + sl->with_sum_func +
			 sl->order_list.elements + sl->group_list.elements)) ||
	setup_fields(thd, sl->ref_pointer_array, first_table, item_list,
		     0, 0, 1))
      goto err;
    t_and_f= 1;
  }

  bzero((char*) &tmp_table_param,sizeof(tmp_table_param));
  tmp_table_param.field_count=item_list.elements;
  if (!(table= create_tmp_table(thd, &tmp_table_param, item_list,
				(ORDER*) 0, !union_option,
				1, (first_select()->options | thd->options |
				    TMP_TABLE_ALL_COLUMNS),
				HA_POS_ERROR)))
    goto err;
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  bzero((char*) &result_table_list,sizeof(result_table_list));
  result_table_list.db= (char*) "";
  result_table_list.real_name=result_table_list.alias= (char*) "union";
  result_table_list.table=table;

  if (!(union_result=new select_union(table)))
    goto err;

  union_result->not_describe=1;
  union_result->tmp_table_param=&tmp_table_param;
  if (thd->lex.describe)
  {
    for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
    {
      JOIN *join= new JOIN(thd, sl->item_list, 
			   sl->options | thd->options | SELECT_NO_UNLOCK,
			   union_result);
      thd->lex.current_select= sl;
      offset_limit_cnt= sl->offset_limit;
      select_limit_cnt= sl->select_limit+sl->offset_limit;
      if (select_limit_cnt < sl->select_limit)
	select_limit_cnt= HA_POS_ERROR;		// no limit
      if (select_limit_cnt == HA_POS_ERROR)
	sl->options&= ~OPTION_FOUND_ROWS;
      
      res= join->prepare(&sl->ref_pointer_array,
			 (TABLE_LIST*) sl->table_list.first, sl->with_wild,
			 sl->where,
			 ((sl->braces) ? sl->order_list.elements : 0) +
			 sl->group_list.elements,
			 (sl->braces) ? 
			 (ORDER *)sl->order_list.first : (ORDER *) 0,
			 (ORDER*) sl->group_list.first,
			 sl->having,
			 (ORDER*) NULL,
			 sl, this, 0, t_and_f);
      t_and_f= 0;
      if (res | thd->is_fatal_error)
	goto err;
    }
  }
  item_list.empty();
  thd->lex.current_select= lex_select_save;
  {
    List_iterator<Item> it(first_select()->item_list);
    Field **field;

    for (field= table->field; *field; field++)
    {
      (void) it++;
      if (item_list.push_back(new Item_field(*field)))
	DBUG_RETURN(-1);
    }
  }

  DBUG_RETURN(res | thd->is_fatal_error);
err:
  thd->lex.current_select= lex_select_save;
  DBUG_RETURN(-1);
}

int st_select_lex_unit::exec()
{
  DBUG_ENTER("st_select_lex_unit::exec");
  SELECT_LEX_NODE *lex_select_save= thd->lex.current_select;
  
  if (executed && !(dependent || uncacheable))
    DBUG_RETURN(0);
  executed= 1;
  
  if ((dependent||uncacheable) || !item || !item->assigned())
  {
    if (optimized && item && item->assigned())
    {
      item->assigned(0); // We will reinit & rexecute unit
      item->reset();
      table->file->delete_all_rows();
    }
    for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
    {
      if (optimized)
	res= sl->join->reinit();
      else
      {
	JOIN *join= new JOIN(thd, sl->item_list, 
			     sl->options | thd->options | SELECT_NO_UNLOCK,
			     union_result);
	thd->lex.current_select= sl;
	offset_limit_cnt= sl->offset_limit;
	select_limit_cnt= sl->select_limit+sl->offset_limit;
	if (select_limit_cnt < sl->select_limit)
	  select_limit_cnt= HA_POS_ERROR;		// no limit
	if (select_limit_cnt == HA_POS_ERROR)
	  sl->options&= ~OPTION_FOUND_ROWS;
	
	res= join->prepare(&sl->ref_pointer_array,
			   (TABLE_LIST*) sl->table_list.first, sl->with_wild,
			   sl->where,
			   ((sl->braces) ? sl->order_list.elements : 0) +
			   sl->group_list.elements,
			   (sl->braces) ? 
			   (ORDER *)sl->order_list.first : (ORDER *) 0,
			   (ORDER*) sl->group_list.first,
			   sl->having,
			   (ORDER*) NULL,
			   sl, this, 0, t_and_f);
	t_and_f=0;
	if (res | thd->is_fatal_error)
	{
	  thd->lex.current_select= lex_select_save;
	  DBUG_RETURN(res);
	}
	res= sl->join->optimize();
      }
      if (!res)
      {
	sl->join->exec();
	res= sl->join->error;
	if (!res && union_result->flush())
	{
	  thd->lex.current_select= lex_select_save;
	  DBUG_RETURN(1);
	}
      }
      if (res)
      {
	thd->lex.current_select= lex_select_save;
	DBUG_RETURN(res);
      }
    }
  }
  optimized= 1;

  /* Send result to 'result' */

  // to correct ORDER BY reference resolving
  thd->lex.current_select = first_select();
  res =-1;
  {
#if 0
    List<Item_func_match> ftfunc_list;
    ftfunc_list.empty();
#else
    List<Item_func_match> empty_list;
    empty_list.empty();
    thd->lex.select_lex.ftfunc_list= &empty_list;
#endif

    if (!thd->is_fatal_error)			// Check if EOM
    {
      SELECT_LEX *sl=thd->lex.current_select->master_unit()->first_select();
      offset_limit_cnt= (sl->braces) ? global_parameters->offset_limit : 0;
      select_limit_cnt= (sl->braces) ? global_parameters->select_limit+
	global_parameters->offset_limit : HA_POS_ERROR;
      if (select_limit_cnt < global_parameters->select_limit)
	select_limit_cnt= HA_POS_ERROR;		// no limit
      if (select_limit_cnt == HA_POS_ERROR)
	thd->options&= ~OPTION_FOUND_ROWS;
      res= mysql_select(thd, &ref_pointer_array, &result_table_list,
			0, item_list, NULL,
			global_parameters->order_list.elements,
			(ORDER*)global_parameters->order_list.first,
			(ORDER*) NULL, NULL, (ORDER*) NULL,
			thd->options, result, this, first_select(), 1, 0);
      if (found_rows_for_union && !res)
	thd->limit_found_rows = (ulonglong)table->file->records;
    }
  }
  thd->lex.select_lex.ftfunc_list= &thd->lex.select_lex.ftfunc_list_alloc;
  thd->lex.current_select= lex_select_save;
  DBUG_RETURN(res);
}

int st_select_lex_unit::cleanup()
{
  DBUG_ENTER("st_select_lex_unit::cleanup");

  int error= 0;

  if (union_result)
  {
    delete union_result;
    if (table)
      free_tmp_table(thd, table);
    table= 0; // Safety
  }
  for (SELECT_LEX *sl= first_select(); sl; sl= sl->next_select())
  {
    JOIN *join;
    if ((join= sl->join))
    {
      error|= sl->join->cleanup(thd);
      delete join;
    }
  }
  DBUG_RETURN(error);
}
