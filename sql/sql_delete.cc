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

int mysql_delete(THD *thd, TABLE_LIST *table_list, COND *conds, ORDER *order,
                 ha_rows limit, thr_lock_type lock_type, ulong options)
{
  int		error;
  TABLE		*table;
  SQL_SELECT	*select=0;
  READ_RECORD	info;
  bool 		using_limit=limit != HA_POS_ERROR;
  bool	        using_transactions;
  ha_rows	deleted;
  DBUG_ENTER("mysql_delete");

  if (!table_list->db)
    table_list->db=thd->db;
  if ((thd->options & OPTION_SAFE_UPDATES) && !conds)
  {
    send_error(&thd->net,ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE);
    DBUG_RETURN(1);
  }

  if (!(table = open_ltable(thd,table_list, lock_type)))
    DBUG_RETURN(-1);
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  thd->proc_info="init";
  table->map=1;
  if (setup_conds(thd,table_list,&conds) || setup_ftfuncs(thd))
    DBUG_RETURN(-1);

  /* Test if the user wants to delete all rows */
  if (!using_limit && (!conds || conds->const_item()) &&
      !(specialflag & (SPECIAL_NO_NEW_FUNC | SPECIAL_SAFE_MODE)))
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

  table->used_keys=table->quick_keys=0;		// Can't use 'only index'
  select=make_select(table,0,0,conds,&error);
  if (error)
    DBUG_RETURN(-1);
  if ((select && select->check_quick(test(thd->options & SQL_SAFE_UPDATES),
				     limit)) || 
      !limit)
  {
    delete select;
    send_ok(&thd->net,0L);
    DBUG_RETURN(0);				// Nothing to delete
  }

  /* If running in safe sql mode, don't allow updates without keys */
  if (!table->quick_keys)
  {
    thd->lex.select_lex.options|=QUERY_NO_INDEX_USED;
    if ((thd->options & OPTION_SAFE_UPDATES) && limit == HA_POS_ERROR)
    {
      delete select;
      send_error(&thd->net,ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE);
      DBUG_RETURN(1);
    }
  }
  (void) table->file->extra(HA_EXTRA_NO_READCHECK);
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_QUICK);

  if (order)
  {
    uint         length;
    SORT_FIELD  *sortorder;
    TABLE_LIST   tables;
    List<Item>   fields;
    List<Item>   all_fields;
    ha_rows examined_rows;

    bzero((char*) &tables,sizeof(tables));
    tables.table = table;

    table->io_cache = (IO_CACHE *) my_malloc(sizeof(IO_CACHE),
                                             MYF(MY_FAE | MY_ZEROFILL));
    if (setup_order(thd, &tables, fields, all_fields, order) ||
        !(sortorder=make_unireg_sortorder(order, &length)) ||
        (table->found_records = filesort(table, sortorder, length,
                                        (SQL_SELECT *) 0, 0L, HA_POS_ERROR,
					 &examined_rows))
        == HA_POS_ERROR)
    {
      delete select;
      DBUG_RETURN(-1);		// This will force out message
    }
  }

  init_read_record(&info,thd,table,select,1,1);
  deleted=0L;
  init_ftfuncs(thd,1);
  thd->proc_info="updating";
  while (!(error=info.read_record(&info)) && !thd->killed)
  {
    if (!(select && select->skipp_record()))
    {
      if (!(error=table->file->delete_row(table->record[0])))
      {
	deleted++;
	if (!--limit && using_limit)
	{
	  error= -1;
	  break;
	}
      }
      else
      {
	table->file->print_error(error,MYF(0));
	error=0;
	break;
      }
    }
    else
      table->file->unlock_row();  // Row failed selection, release lock on it
  }
  thd->proc_info="end";
  end_read_record(&info);
  /* if (order) free_io_cache(table); */ /* QQ Should not be needed */
  (void) table->file->extra(HA_EXTRA_READCHECK);
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_NORMAL);

cleanup:
  using_transactions=table->file->has_transactions();
  if (deleted && (error <= 0 || !using_transactions))
  {
    mysql_update_log.write(thd,thd->query, thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query, using_transactions);
      if (mysql_bin_log.write(&qinfo) && using_transactions)
	error=1;
    }
    if (!using_transactions)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }
  if (using_transactions && ha_autocommit_or_rollback(thd,error >= 0))
    error=1;
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  if (deleted)
    query_cache.invalidate(table_list);
  delete select;
  if (error >= 0)				// Fatal error
    send_error(&thd->net,thd->killed ? ER_SERVER_SHUTDOWN: 0);
  else
  {
    send_ok(&thd->net,deleted);
    DBUG_PRINT("info",("%d records deleted",deleted));
  }
  DBUG_RETURN(0);
}


/***************************************************************************
** delete multiple tables from join 
***************************************************************************/

#define MEM_STRIP_BUF_SIZE sortbuff_size

int refposcmp2(void* arg, const void *a,const void *b)
{
  return memcmp(a,b, *(int*) arg);
}

multi_delete::multi_delete(THD *thd_arg, TABLE_LIST *dt,
			   thr_lock_type lock_option_arg,
			   uint num_of_tables_arg)
  : delete_tables (dt), thd(thd_arg), deleted(0),
    num_of_tables(num_of_tables_arg), error(0), lock_option(lock_option_arg),
    do_delete(false)
{
  uint counter=0;
  not_trans_safe=false;
  tempfiles = (Unique **) sql_calloc(sizeof(Unique *) * (num_of_tables-1));

  (void) dt->table->file->extra(HA_EXTRA_NO_READCHECK);
  /* Don't use key read with MULTI-TABLE-DELETE */
  (void) dt->table->file->extra(HA_EXTRA_NO_KEYREAD);
  dt->table->used_keys=0;
  for (dt=dt->next ; dt ; dt=dt->next,counter++)
  {
    TABLE *table=dt->table;
    (void) table->file->extra(HA_EXTRA_NO_READCHECK);
    (void) table->file->extra(HA_EXTRA_NO_KEYREAD);
    table->used_keys=0;
    tempfiles[counter] = new Unique (refposcmp2,
				     (void *) &table->file->ref_length,
				     table->file->ref_length,
				     MEM_STRIP_BUF_SIZE);
  }
}


int
multi_delete::prepare(List<Item> &values)
{
  DBUG_ENTER("multi_delete::prepare");
  do_delete = true;   
  thd->proc_info="deleting from main table";

  if (thd->options & OPTION_SAFE_UPDATES)
  {
    TABLE_LIST *table_ref;
    for (table_ref=delete_tables;  table_ref; table_ref=table_ref->next)
    {
      TABLE *table=table_ref->table;
      if ((thd->options & OPTION_SAFE_UPDATES) && !table->quick_keys)
      {
	my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,MYF(0));
	DBUG_RETURN(1);
      }
    }
  }
  DBUG_RETURN(0);
}


void
multi_delete::initialize_tables(JOIN *join)
{
  TABLE_LIST *walk;
  table_map tables_to_delete_from=0;
  for (walk= delete_tables ; walk ; walk=walk->next)
    tables_to_delete_from|= walk->table->map;

  walk= delete_tables;
  for (JOIN_TAB *tab=join->join_tab, *end=join->join_tab+join->tables;
       tab < end;
       tab++)
  {
    if (tab->table->map & tables_to_delete_from)
    {
      /* We are going to delete from this table */
      walk->table=tab->table;
      walk=walk->next;
      if (tab == join->join_tab)
	tab->table->no_keyread=1;
      if (!not_trans_safe && !tab->table->file->has_transactions())
	not_trans_safe=true;
    }
  }
  init_ftfuncs(thd,1);
}


multi_delete::~multi_delete()
{
  /* Add back EXTRA_READCHECK;  In 4.0.1 we shouldn't need this anymore */
  for (table_being_deleted=delete_tables ;
       table_being_deleted ;
       table_being_deleted=table_being_deleted->next)
  {
    TABLE *t=table_being_deleted->table;
    (void) t->file->extra(HA_EXTRA_READCHECK);
    t->no_keyread=0;
  }

  for (uint counter = 0; counter < num_of_tables-1; counter++)
  {
    if (tempfiles[counter])
      delete tempfiles[counter];
  }
}


bool multi_delete::send_data(List<Item> &values)
{
  int secure_counter= -1;
  for (table_being_deleted=delete_tables ;
       table_being_deleted ;
       table_being_deleted=table_being_deleted->next, secure_counter++)
  {
    TABLE *table=table_being_deleted->table;

    /* Check if we are using outer join and we didn't find the row */
    if (table->status & (STATUS_NULL_ROW | STATUS_DELETED))
      continue;

    table->file->position(table->record[0]);

    if (secure_counter < 0)
    {
      table->status|= STATUS_DELETED;
      if (!(error=table->file->delete_row(table->record[0])))
	deleted++;
      else
      {
	table->file->print_error(error,MYF(0));
	return 1;
      }
    }
    else
    {
      error=tempfiles[secure_counter]->unique_add((char*) table->file->ref);
      if (error)
      {
	error=-1;
	return 1;
      }
    }
  }
  return 0;
}

void multi_delete::send_error(uint errcode,const char *err)
{
  /* First send error what ever it is ... */
  ::send_error(&thd->net,errcode,err);

  /* reset used flags */
//  delete_tables->table->no_keyread=0;

  /* If nothing deleted return */
  if (!deleted)
    return;
  /* Below can happen when thread is killed early ... */
  if (!table_being_deleted)
    table_being_deleted=delete_tables;

  /*
    If rows from the first table only has been deleted and it is transactional,
    just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt deletes ...
  */
  if ((table_being_deleted->table->file->has_transactions() &&
       table_being_deleted == delete_tables) || !not_trans_safe)
    ha_rollback_stmt(thd);
  else if (do_delete)
    VOID(do_deletes(true));
}


/*
  Do delete from other tables.
  Returns values:
	0 ok
	1 error
*/

int multi_delete::do_deletes (bool from_send_error)
{
  int error = 0, counter = 0;

  if (from_send_error)
  {
    /* Found out table number for 'table_being_deleted' */
    for (TABLE_LIST *aux=delete_tables;
	 aux != table_being_deleted;
	 aux=aux->next)
      counter++;
  }
  else
    table_being_deleted = delete_tables;

  do_delete = false;
  for (table_being_deleted=table_being_deleted->next;
       table_being_deleted ;
       table_being_deleted=table_being_deleted->next, counter++)
  { 
    TABLE *table = table_being_deleted->table;
    if (tempfiles[counter]->get(table))
    {
      error=1;
      break;
    }

#if USE_REGENERATE_TABLE
    // nice little optimization ....
    // but Monty has to fix generate_table...
    // This will not work for transactional tables because for other types
    // records is not absolute
    if (num_of_positions == table->file->records) 
    {
      TABLE_LIST table_list;
      bzero((char*) &table_list,sizeof(table_list));
      table_list.name=table->table_name;
      table_list.real_name=table_being_deleted->real_name;
      table_list.table=table;
      table_list.grant=table->grant;
      table_list.db = table_being_deleted->db;
      error=generate_table(thd,&table_list,(TABLE *)0);
      if (error <= 0) {error = 1; break;}
      deleted += num_of_positions;
      continue;
    }
#endif /* USE_REGENERATE_TABLE */

    READ_RECORD	info;
    init_read_record(&info,thd,table,NULL,0,0);
    while (!(error=info.read_record(&info)) &&
	   (!thd->killed ||  from_send_error || not_trans_safe))
    {
      if ((error=table->file->delete_row(table->record[0])))
      {
	table->file->print_error(error,MYF(0));
	break;
      }
      deleted++;
    }
    end_read_record(&info);
    if (error == -1)				// End of file
      error = 0;
  }
  return error;
}


bool multi_delete::send_eof()
{
  thd->proc_info="deleting from reference tables";  /* out: 1 if error, 0 if success */

  /* Does deletes for the last n - 1 tables, returns 0 if ok */
  int error = do_deletes(false);   /* do_deletes returns 0 if success */

  /* reset used flags */
//  delete_tables->table->no_keyread=0; // Will stay in comment until Monty approves changes
  thd->proc_info="end";
  if (error)
  {
    ::send_error(&thd->net);
    return 1;
  }

  /* Write the SQL statement to the binlog if we deleted
   rows and we succeeded, or also in an error case when there
   was a non-transaction-safe table involved, since
   modifications in it cannot be rolled back. */

  if (deleted || not_trans_safe)
  {
    mysql_update_log.write(thd,thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query);
      if (mysql_bin_log.write(&qinfo) &&
	  !not_trans_safe)
	error=1;  // Log write failed: roll back the SQL statement
    }
    /* Commit or rollback the current SQL statement */ 
    VOID(ha_autocommit_or_rollback(thd,error > 0));
  }
  if (deleted)
    query_cache.invalidate(delete_tables);
  ::send_ok(&thd->net,deleted);
  return 0;
}


/***************************************************************************
* TRUNCATE TABLE
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

int mysql_truncate(THD *thd, TABLE_LIST *table_list, bool dont_send_ok)
{
  HA_CREATE_INFO create_info;
  char path[FN_REFLEN];
  TABLE **table_ptr;
  int error;
  DBUG_ENTER("mysql_truncate");

  /* If it is a temporary table, close and regenerate it */
  if (!dont_send_ok && (table_ptr=find_temporary_table(thd,table_list->db,
						       table_list->real_name)))
  {
    TABLE *table= *table_ptr;
    HA_CREATE_INFO create_info;
    table->file->info(HA_STATUS_AUTO | HA_STATUS_NO_LOCK);
    bzero((char*) &create_info,sizeof(create_info));
    create_info.auto_increment_value= table->file->auto_increment_value;
    db_type table_type=table->db_type;

    strmov(path,table->path);
    *table_ptr= table->next;			// Unlink table from list
    close_temporary(table,0);
    *fn_ext(path)=0;				// Remove the .frm extension
    ha_create_table(path, &create_info,1);
    // We don't need to call invalidate() because this table is not in cache
    if ((error= (int) !(open_temporary_table(thd, path, table_list->db,
					     table_list->real_name, 1))))
      (void) rm_temporary_table(table_type, path);
    DBUG_RETURN(error ? -1 : 0);
  }

  (void) sprintf(path,"%s/%s/%s%s",mysql_data_home,table_list->db,
		 table_list->real_name,reg_ext);
  fn_format(path,path,"","",4);

  if (!dont_send_ok)
  {
    db_type table_type;
    if ((table_type=get_table_type(path)) == DB_TYPE_UNKNOWN)
    {
      my_error(ER_NO_SUCH_TABLE, MYF(0), table_list->real_name);
      DBUG_RETURN(-1);
    }
    if (!ha_supports_generate(table_type))
    {
      /* Probably InnoDB table */
      DBUG_RETURN(mysql_delete(thd,table_list, (COND*) 0, (ORDER*) 0,
			       HA_POS_ERROR, TL_WRITE, 0));
    }
    if (lock_and_wait_for_table_name(thd, table_list))
      DBUG_RETURN(-1);
  }

  bzero((char*) &create_info,sizeof(create_info));
  *fn_ext(path)=0;				// Remove the .frm extension
  error= ha_create_table(path,&create_info,1) ? -1 : 0;
  query_cache.invalidate(table_list); 

  if (!dont_send_ok)
  {
    if (!error)
    {
      mysql_update_log.write(thd,thd->query,thd->query_length);
      if (mysql_bin_log.is_open())
      {
	Query_log_event qinfo(thd, thd->query);
	mysql_bin_log.write(&qinfo);
      }
      send_ok(&thd->net);		// This should return record count
    }
    unlock_table_name(thd, table_list);
  }
  else if (error)
    unlock_table_name(thd, table_list);
  DBUG_RETURN(error ? -1 : 0);
}
