/* Copyright (C) 2000 MySQL AB

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

/*
  Delete of records and truncate of tables.

  Multi-table deletes were introduced by Monty and Sinisa
*/

#include "mysql_priv.h"
#include "ha_innodb.h"
#include "sql_select.h"
#include "sp_head.h"
#include "sql_trigger.h"

bool mysql_delete(THD *thd, TABLE_LIST *table_list, COND *conds,
                  SQL_LIST *order, ha_rows limit, ulonglong options,
                  bool reset_auto_increment)
{
  int		error;
  TABLE		*table;
  SQL_SELECT	*select=0;
  READ_RECORD	info;
  bool          using_limit=limit != HA_POS_ERROR;
  bool		transactional_table, safe_update, const_cond;
  bool          triggers_applicable;
  ha_rows	deleted;
  uint usable_index= MAX_KEY;
  SELECT_LEX   *select_lex= &thd->lex->select_lex;
  THD::killed_state killed_status= THD::NOT_KILLED;
  DBUG_ENTER("mysql_delete");

  if (open_and_lock_tables(thd, table_list))
    DBUG_RETURN(TRUE);
  if (!(table= table_list->table))
  {
    my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
	     table_list->view_db.str, table_list->view_name.str);
    DBUG_RETURN(TRUE);
  }
  error= table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  if (error)
  {
    table->file->print_error(error, MYF(0));
    DBUG_RETURN(error);
  }
  thd->proc_info="init";
  table->map=1;

  if (mysql_prepare_delete(thd, table_list, &conds))
    DBUG_RETURN(TRUE);

  /* check ORDER BY even if it can be ignored */
  if (order && order->elements)
  {
    TABLE_LIST   tables;
    List<Item>   fields;
    List<Item>   all_fields;

    bzero((char*) &tables,sizeof(tables));
    tables.table = table;
    tables.alias = table_list->alias;

      if (select_lex->setup_ref_array(thd, order->elements) ||
	  setup_order(thd, select_lex->ref_pointer_array, &tables,
                    fields, all_fields, (ORDER*) order->first))
    {
      delete select;
      free_underlaid_joins(thd, &thd->lex->select_lex);
      DBUG_RETURN(TRUE);
    }
  }

  const_cond= (!conds || conds->const_item());
  safe_update=test(thd->options & OPTION_SAFE_UPDATES);
  if (safe_update && const_cond)
  {
    my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
               ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
    DBUG_RETURN(TRUE);
  }

  select_lex->no_error= thd->lex->ignore;

  /* NOTE: TRUNCATE must not invoke triggers. */

  triggers_applicable= table->triggers &&
                       thd->lex->sql_command != SQLCOM_TRUNCATE;

  /*
    Test if the user wants to delete all rows and deletion doesn't have
    any side-effects (because of triggers), so we can use optimized
    handler::delete_all_rows() method.
    We implement fast TRUNCATE for InnoDB even if triggers are present. 
    TRUNCATE ignores triggers.
  */
  if (!using_limit && const_cond && (!conds || conds->val_int()) &&
      !(specialflag & (SPECIAL_NO_NEW_FUNC | SPECIAL_SAFE_MODE)) &&
      !(triggers_applicable && table->triggers->has_delete_triggers())
     )
  {
    deleted= table->file->records;
    if (!(error=table->file->delete_all_rows()))
    {
      error= -1;				// ok
      goto cleanup;
    }
    if (error != HA_ERR_WRONG_COMMAND)
    {
      table->file->print_error(error,MYF(0));
      error=0;
      goto cleanup;
    }
    /* Handler didn't support fast delete; Delete rows one by one */
  }

  if (conds)
  {
    Item::cond_result result;
    conds= remove_eq_conds(thd, conds, &result);
    if (result == Item::COND_FALSE)             // Impossible where
      limit= 0;
  }

  table->used_keys.clear_all();
  table->quick_keys.clear_all();		// Can't use 'only index'
  select=make_select(table, 0, 0, conds, 0, &error);
  if (error)
    DBUG_RETURN(TRUE);
  if ((select && select->check_quick(thd, safe_update, limit)) || !limit)
  {
    delete select;
    free_underlaid_joins(thd, select_lex);
    thd->row_count_func= 0;
    send_ok(thd,0L);

    /*
      We don't need to call reset_auto_increment in this case, because
      mysql_truncate always gives a NULL conds argument, hence we never
      get here.
    */

    DBUG_RETURN(0);				// Nothing to delete
  }

  /* If running in safe sql mode, don't allow updates without keys */
  if (table->quick_keys.is_clear_all())
  {
    thd->server_status|=SERVER_QUERY_NO_INDEX_USED;
    if (safe_update && !using_limit)
    {
      delete select;
      free_underlaid_joins(thd, select_lex);
      my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
                 ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
      DBUG_RETURN(TRUE);
    }
  }
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_QUICK);

  if (order && order->elements)
  {
    uint         length= 0;
    SORT_FIELD  *sortorder;
    ha_rows examined_rows;
    
    if ((!select || table->quick_keys.is_clear_all()) && limit != HA_POS_ERROR)
      usable_index= get_index_for_order(table, (ORDER*)(order->first), limit);

    if (usable_index == MAX_KEY)
    {
      table->sort.io_cache= (IO_CACHE *) my_malloc(sizeof(IO_CACHE),
                                                   MYF(MY_FAE | MY_ZEROFILL));
    
      if (!(sortorder= make_unireg_sortorder((ORDER*) order->first,
                                             &length, NULL)) ||
	  (table->sort.found_records = filesort(thd, table, sortorder, length,
                                                select, HA_POS_ERROR,
                                                &examined_rows))
	  == HA_POS_ERROR)
      {
        delete select;
        free_underlaid_joins(thd, &thd->lex->select_lex);
        DBUG_RETURN(TRUE);
      }
      /*
        Filesort has already found and selected the rows we want to delete,
        so we don't need the where clause
      */
      delete select;
      free_underlaid_joins(thd, select_lex);
      select= 0;
    }
  }

  /* If quick select is used, initialize it before retrieving rows. */
  if (select && select->quick && select->quick->reset())
  {
    delete select;
    free_underlaid_joins(thd, select_lex);
    DBUG_RETURN(TRUE);
  }
  if (usable_index==MAX_KEY)
    init_read_record(&info,thd,table,select,1,1);
  else
    init_read_record_idx(&info, thd, table, 1, usable_index);

  deleted=0L;
  init_ftfuncs(thd, select_lex, 1);
  thd->proc_info="updating";

  if (triggers_applicable)
  {
    table->triggers->mark_fields_used(thd, TRG_EVENT_DELETE);
    if (table->triggers->has_triggers(TRG_EVENT_DELETE,
                                      TRG_ACTION_AFTER))
    {
      /*
	The table has AFTER DELETE triggers that might access to subject table
	and therefore might need delete to be done immediately. So we turn-off
	the batching.
      */
      (void) table->file->extra(HA_EXTRA_DELETE_CANNOT_BATCH);
    }
  }

  while (!(error=info.read_record(&info)) && !thd->killed &&
	 !thd->net.report_error)
  {
    // thd->net.report_error is tested to disallow delete row on error
    if (!(select && select->skip_record())&& !thd->net.report_error )
    {

      if (triggers_applicable &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, FALSE))
      {
        error= 1;
        break;
      }

      if (!(error=table->file->delete_row(table->record[0])))
      {
	deleted++;
        if (triggers_applicable &&
            table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                              TRG_ACTION_AFTER, FALSE))
        {
          error= 1;
          break;
        }
	if (!--limit && using_limit)
	{
	  error= -1;
	  break;
	}
      }
      else
      {
	table->file->print_error(error,MYF(0));
	/*
	  In < 4.0.14 we set the error number to 0 here, but that
	  was not sensible, because then MySQL would not roll back the
	  failed DELETE, and also wrote it to the binlog. For MyISAM
	  tables a DELETE probably never should fail (?), but for
	  InnoDB it can fail in a FOREIGN KEY error or an
	  out-of-tablespace error.
	*/
 	error= 1;
	break;
      }
    }
    else
      table->file->unlock_row();  // Row failed selection, release lock on it
  }
  killed_status= thd->killed;
  error= (killed_status == THD::NOT_KILLED)?  error : 1;
  thd->proc_info="end";
  end_read_record(&info);
  free_io_cache(table);				// Will not do any harm
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_NORMAL);

  if (reset_auto_increment && (error < 0))
  {
    /*
      We're really doing a truncate and need to reset the table's
      auto-increment counter.
    */
    int error2= table->file->reset_auto_increment(0);

    if (error2 && (error2 != HA_ERR_WRONG_COMMAND))
    {
      table->file->print_error(error2, MYF(0));
      error= 1;
    }
  }

cleanup:
  /*
    Invalidate the table in the query cache if something changed. This must
    be before binlog writing and ha_autocommit_...
  */
  if (deleted)
  {
    query_cache_invalidate3(thd, table_list, 1);
  }

  delete select;
  transactional_table= table->file->has_transactions();
  if (!transactional_table && deleted > 0)
    thd->transaction.stmt.modified_non_trans_table= TRUE;
  
  /* See similar binlogging code in sql_update.cc, for comments */
  if ((error < 0) || thd->transaction.stmt.modified_non_trans_table)
  {
    if (mysql_bin_log.is_open())
    {
      if (error < 0)
        thd->clear_error();
      Query_log_event qinfo(thd, thd->query, thd->query_length,
			    transactional_table, FALSE, killed_status);
      if (mysql_bin_log.write(&qinfo) && transactional_table)
	error=1;
    }
    if (thd->transaction.stmt.modified_non_trans_table)
      thd->transaction.all.modified_non_trans_table= TRUE;
  }
  DBUG_ASSERT(transactional_table || !deleted || thd->transaction.stmt.modified_non_trans_table);
  free_underlaid_joins(thd, select_lex);
  if (transactional_table)
  {
    if (ha_autocommit_or_rollback(thd,error >= 0))
      error=1;
  }

  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  if (error < 0 || (thd->lex->ignore && !thd->is_fatal_error))
  {
    thd->row_count_func= deleted;
    send_ok(thd,deleted);
    DBUG_PRINT("info",("%ld records deleted",(long) deleted));
  }
  DBUG_RETURN(error >= 0 || thd->net.report_error);
}


/*
  Prepare items in DELETE statement

  SYNOPSIS
    mysql_prepare_delete()
    thd			- thread handler
    table_list		- global/local table list
    conds		- conditions

  RETURN VALUE
    FALSE OK
    TRUE  error
*/
bool mysql_prepare_delete(THD *thd, TABLE_LIST *table_list, Item **conds)
{
  Item *fake_conds= 0;
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  DBUG_ENTER("mysql_prepare_delete");
  List<Item> all_fields;

  thd->lex->allow_sum_func= 0;
  if (setup_tables_and_check_access(thd, &thd->lex->select_lex.context,
                                    &thd->lex->select_lex.top_join_list,
                                    table_list, conds, 
                                    &select_lex->leaf_tables, FALSE, 
                                    DELETE_ACL, SELECT_ACL) ||
      setup_conds(thd, table_list, select_lex->leaf_tables, conds) ||
      setup_ftfuncs(select_lex))
    DBUG_RETURN(TRUE);
  if (!table_list->updatable || check_key_in_view(thd, table_list))
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "DELETE");
    DBUG_RETURN(TRUE);
  }
  {
    TABLE_LIST *duplicate;
    if ((duplicate= unique_table(thd, table_list, table_list->next_global, 0)))
    {
      update_non_unique_table_error(table_list, "DELETE", duplicate);
      DBUG_RETURN(TRUE);
    }
  }

  if (select_lex->inner_refs_list.elements &&
    fix_inner_refs(thd, all_fields, select_lex, select_lex->ref_pointer_array))
    DBUG_RETURN(-1);

  select_lex->fix_prepare_information(thd, conds, &fake_conds);
  DBUG_RETURN(FALSE);
}


/***************************************************************************
  Delete multiple tables from join 
***************************************************************************/

#define MEM_STRIP_BUF_SIZE current_thd->variables.sortbuff_size

extern "C" int refpos_order_cmp(void* arg, const void *a,const void *b)
{
  handler *file= (handler*)arg;
  return file->cmp_ref((const byte*)a, (const byte*)b);
}

/*
  make delete specific preparation and checks after opening tables

  SYNOPSIS
    mysql_multi_delete_prepare()
    thd         thread handler

  RETURN
    FALSE OK
    TRUE  Error
*/

bool mysql_multi_delete_prepare(THD *thd)
{
  LEX *lex= thd->lex;
  TABLE_LIST *aux_tables= (TABLE_LIST *)lex->auxiliary_table_list.first;
  TABLE_LIST *target_tbl;
  DBUG_ENTER("mysql_multi_delete_prepare");

  /*
    setup_tables() need for VIEWs. JOIN::prepare() will not do it second
    time.

    lex->query_tables also point on local list of DELETE SELECT_LEX
  */
  if (setup_tables_and_check_access(thd, &thd->lex->select_lex.context,
                                    &thd->lex->select_lex.top_join_list,
                                    lex->query_tables, &lex->select_lex.where,
                                    &lex->select_lex.leaf_tables, FALSE, 
                                    DELETE_ACL, SELECT_ACL))
    DBUG_RETURN(TRUE);


  /*
    Multi-delete can't be constructed over-union => we always have
    single SELECT on top and have to check underlying SELECTs of it
  */
  lex->select_lex.exclude_from_table_unique_test= TRUE;
  /* Fix tables-to-be-deleted-from list to point at opened tables */
  for (target_tbl= (TABLE_LIST*) aux_tables;
       target_tbl;
       target_tbl= target_tbl->next_local)
  {
    if (!(target_tbl->table= target_tbl->correspondent_table->table))
    {
      DBUG_ASSERT(target_tbl->correspondent_table->view &&
                  target_tbl->correspondent_table->merge_underlying_list &&
                  target_tbl->correspondent_table->merge_underlying_list->
                  next_local);
      my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
               target_tbl->correspondent_table->view_db.str,
               target_tbl->correspondent_table->view_name.str);
      DBUG_RETURN(TRUE);
    }

    if (!target_tbl->correspondent_table->updatable ||
        check_key_in_view(thd, target_tbl->correspondent_table))
    {
      my_error(ER_NON_UPDATABLE_TABLE, MYF(0),
               target_tbl->table_name, "DELETE");
      DBUG_RETURN(TRUE);
    }
    /*
      Check that table from which we delete is not used somewhere
      inside subqueries/view.
    */
    {
      TABLE_LIST *duplicate;
      if ((duplicate= unique_table(thd, target_tbl->correspondent_table,
                                   lex->query_tables, 0)))
      {
        update_non_unique_table_error(target_tbl->correspondent_table,
                                      "DELETE", duplicate);
        DBUG_RETURN(TRUE);
      }
    }
  }
  DBUG_RETURN(FALSE);
}


multi_delete::multi_delete(TABLE_LIST *dt, uint num_of_tables_arg)
  : delete_tables(dt), deleted(0), found(0),
    num_of_tables(num_of_tables_arg), error(0),
    do_delete(0), transactional_tables(0), normal_tables(0), error_handled(0)
{
  tempfiles= (Unique **) sql_calloc(sizeof(Unique *) * num_of_tables);
}


int
multi_delete::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  DBUG_ENTER("multi_delete::prepare");
  unit= u;
  do_delete= 1;
  thd->proc_info="deleting from main table";
  DBUG_RETURN(0);
}


bool
multi_delete::initialize_tables(JOIN *join)
{
  TABLE_LIST *walk;
  Unique **tempfiles_ptr;
  DBUG_ENTER("initialize_tables");

  if ((thd->options & OPTION_SAFE_UPDATES) && error_if_full_join(join))
    DBUG_RETURN(1);

  table_map tables_to_delete_from=0;
  for (walk= delete_tables; walk; walk= walk->next_local)
    tables_to_delete_from|= walk->table->map;

  walk= delete_tables;
  delete_while_scanning= 1;
  for (JOIN_TAB *tab=join->join_tab, *end=join->join_tab+join->tables;
       tab < end;
       tab++)
  {
    if (tab->table->map & tables_to_delete_from)
    {
      /* We are going to delete from this table */
      TABLE *tbl=walk->table=tab->table;
      walk= walk->next_local;
      /* Don't use KEYREAD optimization on this table */
      tbl->no_keyread=1;
      /* Don't use record cache */
      tbl->no_cache= 1;
      tbl->used_keys.clear_all();
      if (tbl->file->has_transactions())
	transactional_tables= 1;
      else
	normal_tables= 1;
      if (tbl->triggers)
      {
        tbl->triggers->mark_fields_used(thd, TRG_EVENT_DELETE);
	if (tbl->triggers->has_triggers(TRG_EVENT_DELETE,
                                      TRG_ACTION_AFTER))
	{
	  /*
	    The table has AFTER DELETE triggers that might access to subject 
	    table and therefore might need delete to be done immediately. 
	    So we turn-off the batching.
	  */
	  (void) tbl->file->extra(HA_EXTRA_DELETE_CANNOT_BATCH);
	}
      }
    }
    else if ((tab->type != JT_SYSTEM && tab->type != JT_CONST) &&
             walk == delete_tables)
    {
      /*
        We are not deleting from the table we are scanning. In this
        case send_data() shouldn't delete any rows a we may touch
        the rows in the deleted table many times
      */
      delete_while_scanning= 0;
    }
  }
  walk= delete_tables;
  tempfiles_ptr= tempfiles;
  if (delete_while_scanning)
  {
    table_being_deleted= delete_tables;
    walk= walk->next_local;
  }
  for (;walk ;walk= walk->next_local)
  {
    TABLE *table=walk->table;
    *tempfiles_ptr++= new Unique (refpos_order_cmp,
				  (void *) table->file,
				  table->file->ref_length,
				  MEM_STRIP_BUF_SIZE);
  }
  init_ftfuncs(thd, thd->lex->current_select, 1);
  DBUG_RETURN(thd->is_fatal_error != 0);
}


multi_delete::~multi_delete()
{
  for (table_being_deleted= delete_tables;
       table_being_deleted;
       table_being_deleted= table_being_deleted->next_local)
  {
    TABLE *table= table_being_deleted->table;
    free_io_cache(table);                       // Alloced by unique
    table->no_keyread=0;
  }

  for (uint counter= 0; counter < num_of_tables; counter++)
  {
    if (tempfiles[counter])
      delete tempfiles[counter];
  }
}


bool multi_delete::send_data(List<Item> &values)
{
  int secure_counter= delete_while_scanning ? -1 : 0;
  TABLE_LIST *del_table;
  DBUG_ENTER("multi_delete::send_data");

  for (del_table= delete_tables;
       del_table;
       del_table= del_table->next_local, secure_counter++)
  {
    TABLE *table= del_table->table;

    /* Check if we are using outer join and we didn't find the row */
    if (table->status & (STATUS_NULL_ROW | STATUS_DELETED))
      continue;

    table->file->position(table->record[0]);
    found++;

    if (secure_counter < 0)
    {
      /* We are scanning the current table */
      DBUG_ASSERT(del_table == table_being_deleted);
      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, FALSE))
        DBUG_RETURN(1);
      table->status|= STATUS_DELETED;
      if (!(error=table->file->delete_row(table->record[0])))
      {
        deleted++;
        if (!table->file->has_transactions())
          thd->transaction.stmt.modified_non_trans_table= TRUE;
        if (table->triggers &&
            table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                              TRG_ACTION_AFTER, FALSE))
          DBUG_RETURN(1);
      }
      else
      {
        table->file->print_error(error,MYF(0));
        DBUG_RETURN(1);
      }
    }
    else
    {
      error=tempfiles[secure_counter]->unique_add((char*) table->file->ref);
      if (error)
      {
	error= 1;                               // Fatal error
	DBUG_RETURN(1);
      }
    }
  }
  DBUG_RETURN(0);
}


void multi_delete::send_error(uint errcode,const char *err)
{
  DBUG_ENTER("multi_delete::send_error");

  /* First send error what ever it is ... */
  my_message(errcode, err, MYF(0));

  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      !thd->transaction.stmt.modified_non_trans_table && !deleted)
    DBUG_VOID_RETURN;

  /* Something already deleted so we have to invalidate cache */
  if (deleted)
    query_cache_invalidate3(thd, delete_tables, 1);

  /*
    If rows from the first table only has been deleted and it is
    transactional, just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt deletes ...
  */
  if ((table_being_deleted == delete_tables &&
       table_being_deleted->table->file->has_transactions()) ||
      !normal_tables)
    ha_rollback_stmt(thd);
  else if (do_delete)
  {
    /*
      We have to execute the recorded do_deletes() and write info into the
      error log
    */
    error= 1;
    send_eof();
    DBUG_ASSERT(error_handled);
    DBUG_VOID_RETURN;
  }
  
  if (thd->transaction.stmt.modified_non_trans_table)
  {
    /* 
       there is only side effects; to binlog with the error
    */
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query, thd->query_length,
                            transactional_tables, FALSE);
      mysql_bin_log.write(&qinfo);
    }
    thd->transaction.all.modified_non_trans_table= true;
  }
  DBUG_VOID_RETURN;
}



/*
  Do delete from other tables.
  Returns values:
	0 ok
	1 error
*/

int multi_delete::do_deletes()
{
  int local_error= 0, counter= 0;
  DBUG_ENTER("do_deletes");
  DBUG_ASSERT(do_delete);

  do_delete= 0;                                 // Mark called
  if (!found)
    DBUG_RETURN(0);

  table_being_deleted= (delete_while_scanning ? delete_tables->next_local :
                        delete_tables);
 
  for (; table_being_deleted;
       table_being_deleted= table_being_deleted->next_local, counter++)
  { 
    ha_rows last_deleted= deleted;
    TABLE *table = table_being_deleted->table;
    if (tempfiles[counter]->get(table))
    {
      local_error=1;
      break;
    }

    READ_RECORD	info;
    init_read_record(&info,thd,table,NULL,0,1);
    /*
      Ignore any rows not found in reference tables as they may already have
      been deleted by foreign key handling
    */
    info.ignore_not_found_rows= 1;
    while (!(local_error=info.read_record(&info)) && !thd->killed)
    {
      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, FALSE))
      {
        local_error= 1;
        break;
      }
      if ((local_error=table->file->delete_row(table->record[0])))
      {
	table->file->print_error(local_error,MYF(0));
	break;
      }
      deleted++;
      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_AFTER, FALSE))
      {
        local_error= 1;
        break;
      }
    }
    if (last_deleted != deleted && !table->file->has_transactions())
      thd->transaction.stmt.modified_non_trans_table= TRUE;
    end_read_record(&info);
    if (thd->killed && !local_error)
      local_error= 1;
    if (local_error == -1)				// End of file
      local_error = 0;
  }
  DBUG_RETURN(local_error);
}


/*
  Send ok to the client

  return:  0 sucess
	   1 error
*/

bool multi_delete::send_eof()
{
  THD::killed_state killed_status= THD::NOT_KILLED;
  thd->proc_info="deleting from reference tables";

  /* Does deletes for the last n - 1 tables, returns 0 if ok */
  int local_error= do_deletes();		// returns 0 if success

  /* compute a total error to know if something failed */
  local_error= local_error || error;
  killed_status= (local_error == 0)? THD::NOT_KILLED : thd->killed;
  /* reset used flags */
  thd->proc_info="end";

  /*
    We must invalidate the query cache before binlog writing and
    ha_autocommit_...
  */
  if (deleted)
  {
    query_cache_invalidate3(thd, delete_tables, 1);
  }
  if ((local_error == 0) || thd->transaction.stmt.modified_non_trans_table)
  {
    if (mysql_bin_log.is_open())
    {
      if (local_error == 0)
        thd->clear_error();
      Query_log_event qinfo(thd, thd->query, thd->query_length,
			    transactional_tables, FALSE, killed_status);
      if (mysql_bin_log.write(&qinfo) && !normal_tables)
	local_error=1;  // Log write failed: roll back the SQL statement
    }
    if (thd->transaction.stmt.modified_non_trans_table)
      thd->transaction.all.modified_non_trans_table= TRUE;
  }
  if (local_error != 0)
    error_handled= TRUE; // to force early leave from ::send_error()

  /* Commit or rollback the current SQL statement */
  if (transactional_tables)
    if (ha_autocommit_or_rollback(thd,local_error > 0))
      local_error=1;

  if (!local_error)
  {
    thd->row_count_func= deleted;
    ::send_ok(thd, deleted);
  }
  return 0;
}


/***************************************************************************
  TRUNCATE TABLE
****************************************************************************/

/*
  Optimize delete of all rows by doing a full generate of the table
  This will work even if the .ISM and .ISD tables are destroyed

  dont_send_ok should be set if:
  - We should always wants to generate the table (even if the table type
    normally can't safely do this.
  - We don't want an ok to be sent to the end user.
  - We don't want to log the truncate command
  - If we want to have a name lock on the table on exit without errors.
*/

bool mysql_truncate(THD *thd, TABLE_LIST *table_list, bool dont_send_ok)
{
  HA_CREATE_INFO create_info;
  char path[FN_REFLEN];
  TABLE **table_ptr;
  bool error;
  DBUG_ENTER("mysql_truncate");

  bzero((char*) &create_info,sizeof(create_info));
  /* If it is a temporary table, close and regenerate it */
  if (!dont_send_ok && (table_ptr=find_temporary_table(thd,table_list->db,
						       table_list->table_name)))
  {
    TABLE *table= *table_ptr;
    table->file->info(HA_STATUS_AUTO | HA_STATUS_NO_LOCK);
    db_type table_type= table->s->db_type;
    if (!ha_check_storage_engine_flag(table_type, HTON_CAN_RECREATE))
      goto trunc_by_del;
    strmov(path, table->s->path);
    *table_ptr= table->next;			// Unlink table from list
    close_temporary(table,0);
    if (thd->slave_thread)
      --slave_open_temp_tables;
    *fn_ext(path)=0;				// Remove the .frm extension
    ha_create_table(path, &create_info,1);
    // We don't need to call invalidate() because this table is not in cache
    if ((error= (int) !(open_temporary_table(thd, path, table_list->db,
					     table_list->table_name, 1))))
      (void) rm_temporary_table(table_type, path);
    /*
      If we return here we will not have logged the truncation to the bin log
      and we will not send_ok() to the client.
    */
    goto end;
  }

  (void) sprintf(path,"%s/%s/%s%s",mysql_data_home,table_list->db,
		 table_list->table_name,reg_ext);
  fn_format(path, path, "", "", MY_UNPACK_FILENAME);

  if (!dont_send_ok)
  {
    db_type table_type;
    mysql_frm_type(thd, path, &table_type);
    if (table_type == DB_TYPE_UNKNOWN)
    {
      my_error(ER_NO_SUCH_TABLE, MYF(0),
               table_list->db, table_list->table_name);
      DBUG_RETURN(TRUE);
    }
    if (!ha_check_storage_engine_flag(table_type, HTON_CAN_RECREATE))
      goto trunc_by_del;
    if (lock_and_wait_for_table_name(thd, table_list))
      DBUG_RETURN(TRUE);
  }

  *fn_ext(path)=0;				// Remove the .frm extension
  error= ha_create_table(path,&create_info,1);
  query_cache_invalidate3(thd, table_list, 0);

end:
  if (!dont_send_ok)
  {
    if (!error)
    {
      if (mysql_bin_log.is_open())
      {
        thd->clear_error();
	Query_log_event qinfo(thd, thd->query, thd->query_length,
			      0, FALSE);
	mysql_bin_log.write(&qinfo);
      }
      send_ok(thd);		// This should return record count
    }
    VOID(pthread_mutex_lock(&LOCK_open));
    unlock_table_name(thd, table_list);
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  else if (error)
  {
    VOID(pthread_mutex_lock(&LOCK_open));
    unlock_table_name(thd, table_list);
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  DBUG_RETURN(error);

 trunc_by_del:
  /* Probably InnoDB table */
  ulonglong save_options= thd->options;
  table_list->lock_type= TL_WRITE;
  thd->options&= ~(ulong) (OPTION_BEGIN | OPTION_NOT_AUTOCOMMIT);
  ha_enable_transaction(thd, FALSE);
  mysql_init_select(thd->lex);
  error= mysql_delete(thd, table_list, (COND*) 0, (SQL_LIST*) 0,
                      HA_POS_ERROR, LL(0), TRUE);
  ha_enable_transaction(thd, TRUE);
  thd->options= save_options;
  DBUG_RETURN(error);
}
