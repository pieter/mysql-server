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
  Delete of records and truncate of tables.

  Multi-table deletes were introduced by Monty and Sinisa
*/



#include "mysql_priv.h"
#include "ha_innodb.h"
#include "sql_select.h"
#include "sp_head.h"
#include "sql_trigger.h"

bool mysql_delete(THD *thd, TABLE_LIST *table_list, COND *conds,
                  SQL_LIST *order, ha_rows limit, ulong options)
{
  bool          will_batch;
  int		error, loc_error;
  TABLE		*table;
  SQL_SELECT	*select=0;
  READ_RECORD	info;
  bool          using_limit=limit != HA_POS_ERROR;
  bool		transactional_table, safe_update, const_cond;
  ha_rows	deleted;
  SELECT_LEX   *select_lex= &thd->lex->select_lex;
  DBUG_ENTER("mysql_delete");

  if (open_and_lock_tables(thd, table_list))
    DBUG_RETURN(TRUE);
  if (!(table= table_list->table))
  {
    my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
	     table_list->view_db.str, table_list->view_name.str);
    DBUG_RETURN(TRUE);
  }
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  thd->proc_info="init";
  table->map=1;

  if (mysql_prepare_delete(thd, table_list, &conds))
    DBUG_RETURN(TRUE);

  const_cond= (!conds || conds->const_item());
  safe_update=test(thd->options & OPTION_SAFE_UPDATES);
  if (safe_update && const_cond)
  {
    my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
               ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
    DBUG_RETURN(TRUE);
  }

  select_lex->no_error= thd->lex->ignore;

  /*
    Test if the user wants to delete all rows and deletion doesn't have
    any side-effects (because of triggers), so we can use optimized
    handler::delete_all_rows() method.
  */
  if (!using_limit && const_cond && (!conds || conds->val_int()) &&
      !(specialflag & (SPECIAL_NO_NEW_FUNC | SPECIAL_SAFE_MODE)) &&
      !(table->triggers && table->triggers->has_delete_triggers()))
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
    uint         length;
    SORT_FIELD  *sortorder;
    TABLE_LIST   tables;
    List<Item>   fields;
    List<Item>   all_fields;
    ha_rows examined_rows;

    bzero((char*) &tables,sizeof(tables));
    tables.table = table;
    tables.alias = table_list->alias;

    table->sort.io_cache = (IO_CACHE *) my_malloc(sizeof(IO_CACHE),
                                             MYF(MY_FAE | MY_ZEROFILL));
      if (select_lex->setup_ref_array(thd, order->elements) ||
	  setup_order(thd, select_lex->ref_pointer_array, &tables,
		      fields, all_fields, (ORDER*) order->first) ||
	  !(sortorder=make_unireg_sortorder((ORDER*) order->first, &length)) ||
	  (table->sort.found_records = filesort(thd, table, sortorder, length,
					   select, HA_POS_ERROR,
					   &examined_rows))
	  == HA_POS_ERROR)
    {
      delete select;
      free_underlaid_joins(thd, select_lex);
      DBUG_RETURN(TRUE);
    }
    /*
      Filesort has already found and selected the rows we want to delete,
      so we don't need the where clause
    */
    delete select;
    select= 0;
  }

  /* If quick select is used, initialize it before retrieving rows. */
  if (select && select->quick && select->quick->reset())
  {
    delete select;
    free_underlaid_joins(thd, select_lex);
    DBUG_RETURN(TRUE);
  }
  init_read_record(&info,thd,table,select,1,1);
  deleted=0L;
  init_ftfuncs(thd, select_lex, 1);
  thd->proc_info="updating";
  will_batch= !table->file->start_bulk_delete();
  while (!(error=info.read_record(&info)) && !thd->killed &&
	 !thd->net.report_error)
  {
    // thd->net.report_error is tested to disallow delete row on error
    if (!(select && select->skip_record())&& !thd->net.report_error )
    {

      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, FALSE))
      {
        error= 1;
        break;
      }

      if (!(error= table->file->delete_row(table->record[0])))
      {
	deleted++;
        if (table->triggers &&
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
  if (thd->killed && !error)
    error= 1;					// Aborted
  if (will_batch && (loc_error= table->file->end_bulk_delete()))
  {
    if (error != 1)
      table->file->print_error(loc_error,MYF(0));
    error=1;
  }
  thd->proc_info= "end";
  end_read_record(&info);
  free_io_cache(table);				// Will not do any harm
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_NORMAL);

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
  /*
    We write to the binary log even if we deleted no row, because maybe the
    user is using this command to ensure that a table is clean on master *and
    on slave*. Think of the case of a user having played separately with the
    master's table and slave's table and wanting to take a fresh identical
    start now.
    error < 0 means "really no error". error <= 0 means "maybe some error".
  */
  if ((deleted || (error < 0)) && (error <= 0 || !transactional_table))
  {
    if (mysql_bin_log.is_open())
    {
      if (error <= 0)
        thd->clear_error();
      Query_log_event qinfo(thd, thd->query, thd->query_length,
			    transactional_table, FALSE);
      if (mysql_bin_log.write(&qinfo) && transactional_table)
	error=1;
    }
    if (!transactional_table)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }
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
  free_underlaid_joins(thd, select_lex);
  if (error < 0)
  {
    thd->row_count_func= deleted;
    send_ok(thd,deleted);
    DBUG_PRINT("info",("%d records deleted",deleted));
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
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  DBUG_ENTER("mysql_prepare_delete");

  if (setup_tables(thd, table_list, conds, &select_lex->leaf_tables, FALSE) ||
      setup_conds(thd, table_list, select_lex->leaf_tables, conds) ||
      setup_ftfuncs(select_lex))
    DBUG_RETURN(TRUE);
  if (!table_list->updatable || check_key_in_view(thd, table_list))
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "DELETE");
    DBUG_RETURN(TRUE);
  }
  if (unique_table(table_list, table_list->next_global))
  {
    my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->table_name);
    DBUG_RETURN(TRUE);
  }
  select_lex->fix_prepare_information(thd, conds);
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
  TABLE_LIST *aux_tables= (TABLE_LIST *)lex->auxilliary_table_list.first;
  TABLE_LIST *target_tbl;
  DBUG_ENTER("mysql_multi_delete_prepare");

  /*
    setup_tables() need for VIEWs. JOIN::prepare() will not do it second
    time.

    lex->query_tables also point on local list of DELETE SELECT_LEX
  */
  if (setup_tables(thd, lex->query_tables, &lex->select_lex.where,
                   &lex->select_lex.leaf_tables, FALSE))
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
                  target_tbl->correspondent_table->ancestor &&
                  target_tbl->correspondent_table->ancestor->next_local);
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
    if (unique_table(target_tbl->correspondent_table, lex->query_tables))
    {
      my_error(ER_UPDATE_TABLE_USED, MYF(0),
               target_tbl->correspondent_table->table_name);
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
}


multi_delete::multi_delete(THD *thd_arg, TABLE_LIST *dt,
			   uint num_of_tables_arg)
  : delete_tables(dt), thd(thd_arg), deleted(0), found(0),
    num_of_tables(num_of_tables_arg), error(0),
    do_delete(0), transactional_tables(0), normal_tables(0)
{
  tempfiles = (Unique **) sql_calloc(sizeof(Unique *) * (num_of_tables-1));
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
    }
  }
  walk= delete_tables;
  tempfiles_ptr= tempfiles;
  for (walk= walk->next_local ;walk ;walk= walk->next_local)
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
    TABLE *t=table_being_deleted->table;
    free_io_cache(t);				// Alloced by unique
    t->no_keyread=0;
  }

  for (uint counter= 0; counter < num_of_tables-1; counter++)
  {
    if (tempfiles[counter])
      delete tempfiles[counter];
  }
}


bool multi_delete::send_data(List<Item> &values)
{
  int secure_counter= -1;
  DBUG_ENTER("multi_delete::send_data");

  for (table_being_deleted= delete_tables;
       table_being_deleted;
       table_being_deleted= table_being_deleted->next_local, secure_counter++)
  {
    TABLE *table=table_being_deleted->table;

    /* Check if we are using outer join and we didn't find the row */
    if (table->status & (STATUS_NULL_ROW | STATUS_DELETED))
      continue;

    table->file->position(table->record[0]);
    found++;

    if (secure_counter < 0)
    {
      /* If this is the table we are scanning */
      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, FALSE))
	DBUG_RETURN(1);
      table->status|= STATUS_DELETED;
      if (!(error=table->file->delete_row(table->record[0])))
      {
	deleted++;
        if (table->triggers &&
            table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                              TRG_ACTION_AFTER, FALSE))
	  DBUG_RETURN(1);
      }
      else if (!table_being_deleted->next_local ||
	       table_being_deleted->table->file->has_transactions())
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
	error=-1;
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

  /* If nothing deleted return */
  if (!deleted)
    DBUG_VOID_RETURN;

  /* Something already deleted so we have to invalidate cache */
  query_cache_invalidate3(thd, delete_tables, 1);

  /* Below can happen when thread is killed early ... */
  if (!table_being_deleted)
    table_being_deleted=delete_tables;

  /*
    If rows from the first table only has been deleted and it is
    transactional, just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt deletes ...
  */
  if ((table_being_deleted->table->file->has_transactions() &&
       table_being_deleted == delete_tables) || !normal_tables)
    ha_rollback_stmt(thd);
  else if (do_delete)
  {
    VOID(do_deletes(1));
  }
  DBUG_VOID_RETURN;
}


/*
  Do delete from other tables.
  Returns values:
	0 ok
	1 error
*/

int multi_delete::do_deletes(bool from_send_error)
{
  int local_error= 0, counter= 0, error;
  bool will_batch;
  DBUG_ENTER("do_deletes");

  if (from_send_error)
  {
    /* Found out table number for 'table_being_deleted*/
    for (TABLE_LIST *aux= delete_tables;
	 aux != table_being_deleted;
	 aux= aux->next_local)
      counter++;
  }
  else
    table_being_deleted = delete_tables;

  do_delete= 0;
  if (!found)
    DBUG_RETURN(0);
  for (table_being_deleted= table_being_deleted->next_local;
       table_being_deleted;
       table_being_deleted= table_being_deleted->next_local, counter++)
  { 
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
    will_batch= !table->file->start_bulk_delete();
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
    if (will_batch && (error= table->file->end_bulk_delete()))
    {
      if (!local_error)
      {
        local_error= error;
        table->file->print_error(local_error,MYF(0));
      }
    }
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
  thd->proc_info="deleting from reference tables";

  /* Does deletes for the last n - 1 tables, returns 0 if ok */
  int local_error= do_deletes(0);		// returns 0 if success

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

  /*
    Write the SQL statement to the binlog if we deleted
    rows and we succeeded, or also in an error case when there
    was a non-transaction-safe table involved, since
    modifications in it cannot be rolled back.
    Note that if we deleted nothing we don't write to the binlog (TODO:
    fix this).
  */
  if (deleted && ((error <= 0 && !local_error) || normal_tables))
  {
    if (mysql_bin_log.is_open())
    {
      if (error <= 0 && !local_error)
        thd->clear_error();
      Query_log_event qinfo(thd, thd->query, thd->query_length,
			    transactional_tables, FALSE);
      if (mysql_bin_log.write(&qinfo) && !normal_tables)
	local_error=1;  // Log write failed: roll back the SQL statement
    }
    if (!transactional_tables)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }
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
    strmov(path, table->s->path);
    *table_ptr= table->next;			// Unlink table from list
    close_temporary(table,0);
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
  fn_format(path,path,"","",4);

  if (!dont_send_ok)
  {
    db_type table_type;
    if ((table_type=get_table_type(path)) == DB_TYPE_UNKNOWN)
    {
      my_error(ER_NO_SUCH_TABLE, MYF(0),
               table_list->db, table_list->table_name);
      DBUG_RETURN(TRUE);
    }
    if (!ha_supports_generate(table_type) || thd->lex->sphead)
    {
      /* Probably InnoDB table */
      ulong save_options= thd->options;
      table_list->lock_type= TL_WRITE;
      thd->options&= ~(ulong) (OPTION_BEGIN | OPTION_NOT_AUTOCOMMIT);
      ha_enable_transaction(thd, FALSE);
      mysql_init_select(thd->lex);
      error= mysql_delete(thd, table_list, (COND*) 0, (SQL_LIST*) 0,
			  HA_POS_ERROR, 0);
      ha_enable_transaction(thd, TRUE);
      thd->options= save_options;
      DBUG_RETURN(error);
    }
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
}
