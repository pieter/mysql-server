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


/* Update of records 
   Multi-table updates were introduced by Monty and Sinisa <sinisa@mysql.com>
*/

#include "mysql_priv.h"
#include "sql_acl.h"
#include "sql_select.h"

/* Return 0 if row hasn't changed */

static bool compare_record(TABLE *table, ulong query_id)
{
  if (!table->blob_fields)
    return cmp_record(table,1);
  /* Compare null bits */
  if (memcmp(table->null_flags,
	     table->null_flags+table->rec_buff_length,
	     table->null_bytes))
    return 1;					// Diff in NULL value
  /* Compare updated fields */
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {
    if ((*ptr)->query_id == query_id &&
	(*ptr)->cmp_binary_offset(table->rec_buff_length))
      return 1;
  }
  return 0;
}


int mysql_update(THD *thd,
                 TABLE_LIST *table_list,
                 List<Item> &fields,
		 List<Item> &values,
                 COND *conds,
                 uint order_num, ORDER *order,
		 ha_rows limit,
		 enum enum_duplicates handle_duplicates)
{
  bool 		using_limit=limit != HA_POS_ERROR;
  bool		safe_update= thd->options & OPTION_SAFE_UPDATES;
  bool		used_key_is_modified, transactional_table, log_delayed;
  int		error=0;
  uint		used_index, want_privilege;
  ulong		query_id=thd->query_id, timestamp_query_id;
  key_map	old_used_keys;
  TABLE		*table;
  SQL_SELECT	*select;
  READ_RECORD	info;
  TABLE_LIST    *update_table_list= (TABLE_LIST*) 
    thd->lex.select_lex.table_list.first;
  DBUG_ENTER("mysql_update");
  LINT_INIT(used_index);
  LINT_INIT(timestamp_query_id);

  if ((open_and_lock_tables(thd, table_list)))
    DBUG_RETURN(-1);
  fix_tables_pointers(thd->lex.all_selects_list);
  table= table_list->table;

  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  thd->proc_info="init";

  /* Calculate "table->used_keys" based on the WHERE */
  table->used_keys=table->keys_in_use;
  table->quick_keys=0;
  want_privilege=table->grant.want_privilege;
  table->grant.want_privilege=(SELECT_ACL & ~table->grant.privilege);
  if (setup_tables(update_table_list) || 
      setup_conds(thd,update_table_list,&conds)
      || setup_ftfuncs(&thd->lex.select_lex))
    DBUG_RETURN(-1);				/* purecov: inspected */
  if (find_real_table_in_list(table_list->next, 
			      table_list->db, table_list->real_name))
  {
    my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->real_name);
    DBUG_RETURN(-1);
  }

  old_used_keys=table->used_keys;		// Keys used in WHERE

  /*
    Change the query_id for the timestamp column so that we can
    check if this is modified directly
  */
  if (table->timestamp_field)
  {
    timestamp_query_id=table->timestamp_field->query_id;
    table->timestamp_field->query_id=thd->query_id-1;
    table->time_stamp= table->timestamp_field->offset() +1;
  }

  /* Check the fields we are going to modify */
  table->grant.want_privilege=want_privilege;
  if (setup_fields(thd, 0, update_table_list, fields, 1, 0, 0))
    DBUG_RETURN(-1);				/* purecov: inspected */
  if (table->timestamp_field)
  {
    // Don't set timestamp column if this is modified
    if (table->timestamp_field->query_id == thd->query_id)
      table->time_stamp=0;
    else
      table->timestamp_field->query_id=timestamp_query_id;
  }

  /* Check values */
  table->grant.want_privilege=(SELECT_ACL & ~table->grant.privilege);
  if (setup_fields(thd, 0, update_table_list, values, 0, 0, 0))
  {
    free_underlaid_joins(thd, &thd->lex.select_lex);
    DBUG_RETURN(-1);				/* purecov: inspected */
  }

  // Don't count on usage of 'only index' when calculating which key to use
  table->used_keys=0;
  select=make_select(table,0,0,conds,&error);
  if (error ||
      (select && select->check_quick(safe_update, limit)) || !limit)
  {
    delete select;
    free_underlaid_joins(thd, &thd->lex.select_lex);
    if (error)
    {
      DBUG_RETURN(-1);				// Error in where
    }
    send_ok(thd);				// No matching records
    DBUG_RETURN(0);
  }
  /* If running in safe sql mode, don't allow updates without keys */
  if (!table->quick_keys)
  {
    thd->lex.select_lex.options|=QUERY_NO_INDEX_USED;
    if (safe_update && !using_limit)
    {
      delete select;
      free_underlaid_joins(thd, &thd->lex.select_lex);
      send_error(thd,ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE);
      DBUG_RETURN(1);
    }
  }
  init_ftfuncs(thd, &thd->lex.select_lex, 1);
  /* Check if we are modifying a key that we are used to search with */
  if (select && select->quick)
    used_key_is_modified= (!select->quick->unique_key_range() &&
			   check_if_key_used(table,
					     (used_index=select->quick->index),
					     fields));
  else if ((used_index=table->file->key_used_on_scan) < MAX_KEY)
    used_key_is_modified=check_if_key_used(table, used_index, fields);
  else
    used_key_is_modified=0;
  if (used_key_is_modified || order)
  {
    /*
      We can't update table directly;  We must first search after all
      matching rows before updating the table!
    */
    table->file->extra(HA_EXTRA_DONT_USE_CURSOR_TO_UPDATE);
    IO_CACHE tempfile;
    if (open_cached_file(&tempfile, mysql_tmpdir,TEMP_PREFIX,
			  DISK_BUFFER_SIZE, MYF(MY_WME)))
    {
      delete select; /* purecov: inspected */
      free_underlaid_joins(thd, &thd->lex.select_lex);
      DBUG_RETURN(-1);
    }
    if (old_used_keys & ((key_map) 1 << used_index))
    {
      table->key_read=1;
      table->file->extra(HA_EXTRA_KEYREAD);
    }

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
      if (setup_ref_array(thd, &thd->lex.select_lex.ref_pointer_array,
			order_num)||
	  setup_order(thd, thd->lex.select_lex.ref_pointer_array,
		      &tables, fields, all_fields, order) ||
          !(sortorder=make_unireg_sortorder(order, &length)) ||
          (table->found_records = filesort(thd, table, sortorder, length,
                                           (SQL_SELECT *) 0,
					   HA_POS_ERROR, &examined_rows))
          == HA_POS_ERROR)
      {
	delete select;
	free_underlaid_joins(thd, &thd->lex.select_lex);
	DBUG_RETURN(-1);
      }
    }

    init_read_record(&info,thd,table,select,0,1);
    thd->proc_info="Searching rows for update";

    while (!(error=info.read_record(&info)) && !thd->killed)
    {
      if (!(select && select->skipp_record()))
      {
	table->file->position(table->record[0]);
	if (my_b_write(&tempfile,table->file->ref,
		       table->file->ref_length))
	{
	  error=1; /* purecov: inspected */
	  break; /* purecov: inspected */
	}
      }
      else
      {
	if (!(test_flags & 512))		/* For debugging */
	{
	  DBUG_DUMP("record",(char*) table->record[0],table->reclength);
	}
      }
    }
    end_read_record(&info);

    if (table->key_read)
    {
      table->key_read=0;
      table->file->extra(HA_EXTRA_NO_KEYREAD);
    }
    /* Change select to use tempfile */
    if (select)
    {
      delete select->quick;
      if (select->free_cond)
	delete select->cond;
      select->quick=0;
      select->cond=0;
    }
    else
    {
      select= new SQL_SELECT;
      select->head=table;
    }
    if (reinit_io_cache(&tempfile,READ_CACHE,0L,0,0))
      error=1; /* purecov: inspected */
    select->file=tempfile;			// Read row ptrs from this file
    if (error >= 0)
    {
      delete select;
      free_underlaid_joins(thd, &thd->lex.select_lex);
      DBUG_RETURN(-1);
    }
  }

  if (handle_duplicates == DUP_IGNORE)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  init_read_record(&info,thd,table,select,0,1);

  ha_rows updated=0L,found=0L;
  thd->count_cuted_fields=1;			/* calc cuted fields */
  thd->cuted_fields=0L;
  thd->proc_info="Updating";
  query_id=thd->query_id;

  while (!(error=info.read_record(&info)) && !thd->killed)
  {
    if (!(select && select->skipp_record()))
    {
      store_record(table,1);
      if (fill_record(fields,values) || thd->net.report_error)
	break; /* purecov: inspected */
      found++;
      if (compare_record(table, query_id))
      {
	if (!(error=table->file->update_row((byte*) table->record[1],
					    (byte*) table->record[0])))
	{
	  updated++;
	  if (!--limit && using_limit)
	  {
	    error= -1;
	    break;
	  }
	}
	else if (handle_duplicates != DUP_IGNORE ||
		 error != HA_ERR_FOUND_DUPP_KEY)
	{
	  table->file->print_error(error,MYF(0));
	  error= 1;
	  break;
	}
      }
    }
    else
      table->file->unlock_row();
  }
  end_read_record(&info);
  thd->proc_info="end";
  VOID(table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY));
  transactional_table= table->file->has_transactions();
  log_delayed= (transactional_table || table->tmp_table);
  if (updated && (error <= 0 || !transactional_table))
  {
    mysql_update_log.write(thd,thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query, thd->query_length,
			    log_delayed);
      if (mysql_bin_log.write(&qinfo) && transactional_table)
	error=1;				// Rollback update
    }
    if (!log_delayed)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }
  if (transactional_table)
  {
    if (ha_autocommit_or_rollback(thd, error >= 0))
      error=1;
  }

  /*
    Store table for future invalidation  or invalidate it in
    the query cache if something changed
  */
  if (updated)
  {
    query_cache_invalidate3(thd, table_list, 1);
  }
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }

  delete select;
  free_underlaid_joins(thd, &thd->lex.select_lex);
  if (error >= 0)
    send_error(thd,thd->killed ? ER_SERVER_SHUTDOWN : 0); /* purecov: inspected */
  else
  {
    char buff[80];
    sprintf(buff,ER(ER_UPDATE_INFO), (long) found, (long) updated,
	    (long) thd->cuted_fields);
    send_ok(thd,
	    (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated,
	    thd->insert_id_used ? thd->insert_id() : 0L,buff);
    DBUG_PRINT("info",("%d records updated",updated));
  }
  thd->count_cuted_fields=0;			/* calc cuted fields */
  free_io_cache(table);

  DBUG_RETURN(0);
}


/***************************************************************************
  Update multiple tables from join 
***************************************************************************/

/*
  Setup multi-update handling and call SELECT to do the join
*/

int mysql_multi_update(THD *thd,
		       TABLE_LIST *table_list,
		       List<Item> *fields,
		       List<Item> *values,
		       COND *conds,
		       ulong options,
		       enum enum_duplicates handle_duplicates,
		       SELECT_LEX_UNIT *unit, SELECT_LEX *select_lex)
{
  int res;
  multi_update *result;
  TABLE_LIST *tl;
  DBUG_ENTER("mysql_multi_update");

  table_list->grant.want_privilege=(SELECT_ACL & ~table_list->grant.privilege);
  if ((res=open_and_lock_tables(thd,table_list)))
    DBUG_RETURN(res);

  thd->select_limit=HA_POS_ERROR;
  if (setup_fields(thd, 0, table_list, *fields, 1, 0, 0))
    DBUG_RETURN(-1);

  /*
    Count tables and setup timestamp handling
  */
  for (tl= (TABLE_LIST*) table_list ; tl ; tl=tl->next)
  {
    TABLE *table= tl->table;
    if (table->timestamp_field)
    {
      table->time_stamp=0;
      // Only set timestamp column if this is not modified
      if (table->timestamp_field->query_id != thd->query_id)
	table->time_stamp= table->timestamp_field->offset() +1;
    }
  }

  if (!(result=new multi_update(thd, table_list, fields, values,
				handle_duplicates)))
    DBUG_RETURN(-1);

  List<Item> total_list;
  res= mysql_select(thd, &select_lex->ref_pointer_array,
		    table_list, select_lex->with_wild, total_list,
		    conds, 0, (ORDER *) NULL, (ORDER *)NULL, (Item *) NULL,
		    (ORDER *)NULL,
		    options | SELECT_NO_JOIN_CACHE,
		    result, unit, select_lex, 0, 0);
  delete result;
  DBUG_RETURN(res);
}


multi_update::multi_update(THD *thd_arg, TABLE_LIST *table_list,
			   List<Item> *field_list, List<Item> *value_list,
			   enum enum_duplicates handle_duplicates_arg)
  :all_tables(table_list), update_tables(0), thd(thd_arg), tmp_tables(0),
   updated(0), found(0), fields(field_list), values(value_list),
   table_count(0), copy_field(0), handle_duplicates(handle_duplicates_arg),
   do_update(1), trans_safe(0)
{}


/*
  Connect fields with tables and create list of tables that are updated
*/

int multi_update::prepare(List<Item> &not_used_values, SELECT_LEX_UNIT *unit)
{
  TABLE_LIST *table_ref;
  SQL_LIST update;
  table_map tables_to_update= 0;
  Item_field *item;
  List_iterator_fast<Item> field_it(*fields);
  List_iterator_fast<Item> value_it(*values);
  uint i, max_fields;
  DBUG_ENTER("multi_update::prepare");

  thd->count_cuted_fields=1;
  thd->cuted_fields=0L;
  thd->proc_info="updating main table";

  while ((item= (Item_field *) field_it++))
    tables_to_update|= item->used_tables();

  if (!tables_to_update)
  {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
	     "You didn't specify any tables to UPDATE");
    DBUG_RETURN(1);
  }

  /*
    We have to check values after setup_tables to get used_keys right in
    reference tables
  */

  if (setup_fields(thd, 0, all_tables, *values, 1, 0, 0))
    DBUG_RETURN(1);

  /*
    Save tables beeing updated in update_tables
    update_table->shared is position for table
    Don't use key read on tables that are updated
  */

  update.empty();
  for (table_ref= all_tables;  table_ref; table_ref=table_ref->next)
  {
    TABLE *table=table_ref->table;
    if (tables_to_update & table->map)
    {
      TABLE_LIST *tl= (TABLE_LIST*) thd->memdup((char*) table_ref,
						sizeof(*tl));
      if (!tl)
	DBUG_RETURN(1);
      update.link_in_list((byte*) tl, (byte**) &tl->next);
      tl->shared= table_count++;
      table->no_keyread=1;
      table->used_keys=0;
      table->pos_in_table_list= tl;
    }
  }
  table_count=  update.elements;
  update_tables= (TABLE_LIST*) update.first;

  tmp_tables = (TABLE **) thd->calloc(sizeof(TABLE *) * table_count);
  tmp_table_param = (TMP_TABLE_PARAM*) thd->calloc(sizeof(TMP_TABLE_PARAM) *
						   table_count);
  fields_for_table= (List_item **) thd->alloc(sizeof(List_item *) *
					      table_count);
  values_for_table= (List_item **) thd->alloc(sizeof(List_item *) *
					      table_count);
  if (thd->is_fatal_error)
    DBUG_RETURN(1);
  for (i=0 ; i < table_count ; i++)
  {
    fields_for_table[i]= new List_item;
    values_for_table[i]= new List_item;
  }
  if (thd->is_fatal_error)
    DBUG_RETURN(1);

  /* Split fields into fields_for_table[] and values_by_table[] */

  field_it.rewind();
  while ((item= (Item_field *) field_it++))
  {
    Item *value= value_it++;
    uint offset= item->field->table->pos_in_table_list->shared;
    fields_for_table[offset]->push_back(item);
    values_for_table[offset]->push_back(value);
  }
  if (thd->is_fatal_error)
    DBUG_RETURN(1);

  /* Allocate copy fields */
  max_fields=0;
  for (i=0 ; i < table_count ; i++)
    set_if_bigger(max_fields, fields_for_table[i]->elements);
  copy_field= new Copy_field[max_fields];
  DBUG_RETURN(thd->is_fatal_error != 0);
}


/*
  Store first used table in main_table as this should be updated first
  This is because we know that no row in this table will be read twice.

  Create temporary tables to store changed values for all other tables
  that are updated.
*/

bool
multi_update::initialize_tables(JOIN *join)
{
  TABLE_LIST *table_ref;
  DBUG_ENTER("initialize_tables");

  if ((thd->options & OPTION_SAFE_UPDATES) && error_if_full_join(join))
    DBUG_RETURN(1);
  main_table=join->join_tab->table;
  trans_safe= transactional_tables= main_table->file->has_transactions();
  log_delayed= trans_safe || main_table->tmp_table != NO_TMP_TABLE;

  /* Create a temporary table for all tables after except main table */
  for (table_ref= update_tables; table_ref; table_ref=table_ref->next)
  {
    TABLE *table=table_ref->table;
    if (table != main_table)
    {
      uint cnt= table_ref->shared;
      ORDER     group;
      List<Item> temp_fields= *fields_for_table[cnt];
      TMP_TABLE_PARAM *tmp_param= tmp_table_param+cnt;

      /*
	Create a temporary table to store all fields that are changed for this
	table. The first field in the temporary table is a pointer to the
	original row so that we can find and update it
      */

      /* ok to be on stack as this is not referenced outside of this func */
      Field_string offset(table->file->ref_length, 0, "offset",
			  table, &my_charset_bin);
      if (temp_fields.push_front(new Item_field(((Field *) &offset))))
	DBUG_RETURN(1);

      /* Make an unique key over the first field to avoid duplicated updates */
      bzero((char*) &group, sizeof(group));
      group.asc= 1;
      group.item= (Item**) temp_fields.head_ref();

      tmp_param->quick_group=1;
      tmp_param->field_count=temp_fields.elements;
      tmp_param->group_parts=1;
      tmp_param->group_length= table->file->ref_length;
      if (!(tmp_tables[cnt]=create_tmp_table(thd,
					     tmp_param,
					     temp_fields,
					     (ORDER*) &group, 0, 0,
					     TMP_TABLE_ALL_COLUMNS,
					     HA_POS_ERROR)))
	DBUG_RETURN(1);
      tmp_tables[cnt]->file->extra(HA_EXTRA_WRITE_CACHE);
    }
  }
  DBUG_RETURN(0);
}


multi_update::~multi_update()
{
  TABLE_LIST *table;
  for (table= update_tables ; table; table= table->next)
    table->table->no_keyread=0;

  if (tmp_tables)
  {
    for (uint cnt = 0; cnt < table_count; cnt++)
    {
      if (tmp_tables[cnt])
      {
	free_tmp_table(thd, tmp_tables[cnt]);
	tmp_table_param[cnt].cleanup();
      }
    }
  }
  if (copy_field)
    delete [] copy_field;
  thd->count_cuted_fields=0;			// Restore this setting
  if (!trans_safe)
    thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
}


bool multi_update::send_data(List<Item> &not_used_values)
{
  TABLE_LIST *cur_table;
  DBUG_ENTER("multi_update::send_data");

  for (cur_table= update_tables; cur_table ; cur_table= cur_table->next)
  {
    TABLE *table= cur_table->table;
    /* Check if we are using outer join and we didn't find the row */
    if (table->status & (STATUS_NULL_ROW | STATUS_UPDATED))
      continue;

    uint offset= cur_table->shared;
    table->file->position(table->record[0]);
    if (table == main_table)
    {
      table->status|= STATUS_UPDATED;
      store_record(table,1);
      if (fill_record(*fields_for_table[offset], *values_for_table[offset]))
	DBUG_RETURN(1);
      found++;
      if (compare_record(table, thd->query_id))
      {
	int error;
	if (!updated++)
	{
	  /*
	    Inform the main table that we are going to update the table even
	    while we may be scanning it.  This will flush the read cache
	    if it's used.
	  */
	  main_table->file->extra(HA_EXTRA_PREPARE_FOR_UPDATE);
	}
	if ((error=table->file->update_row(table->record[1],
					   table->record[0])))
	{
	  table->file->print_error(error,MYF(0));
	  updated--;
	  DBUG_RETURN(1);
	}
      }
    }
    else
    {
      int error;
      TABLE *tmp_table= tmp_tables[offset];
      fill_record(tmp_table->field+1, *values_for_table[offset]);
      found++;
      /* Store pointer to row */
      memcpy((char*) tmp_table->field[0]->ptr,
	     (char*) table->file->ref, table->file->ref_length);
      /* Write row, ignoring duplicated updates to a row */
      if ((error= tmp_table->file->write_row(tmp_table->record[0])) &&
	  (error != HA_ERR_FOUND_DUPP_KEY &&
	   error != HA_ERR_FOUND_DUPP_UNIQUE))
      {
	if (create_myisam_from_heap(thd, table, tmp_table_param + offset,
				    error, 1))
	{
	  do_update=0;
	  DBUG_RETURN(1);			// Not a table_is_full error
	}
      }
    }
  }
  DBUG_RETURN(0);
}


void multi_update::send_error(uint errcode,const char *err)
{
  /* First send error what ever it is ... */
  ::send_error(thd,errcode,err);

  /* If nothing updated return */
  if (!updated)
    return;

  /* Something already updated so we have to invalidate cache */
  query_cache_invalidate3(thd, update_tables, 1);

  /*
    If all tables that has been updated are trans safe then just do rollback.
    If not attempt to do remaining updates.
  */

  if (trans_safe)
    ha_rollback_stmt(thd);
  else if (do_update && table_count > 1)
  {
    /* Add warning here */
    VOID(do_updates(0));
  }
}


int multi_update::do_updates(bool from_send_error)
{
  TABLE_LIST *cur_table;
  int local_error;
  ha_rows org_updated;
  TABLE *table;
  DBUG_ENTER("do_updates");

  do_update= 0;					// Don't retry this function
  for (cur_table= update_tables; cur_table ; cur_table= cur_table->next)
  {
    table = cur_table->table;
    if (table == main_table)
      continue;					// Already updated

    org_updated= updated;
    byte *ref_pos;
    TABLE *tmp_table= tmp_tables[cur_table->shared];
    tmp_table->file->extra(HA_EXTRA_CACHE);	// Change to read cache
    table->file->extra(HA_EXTRA_NO_CACHE);

    /*
      Setup copy functions to copy fields from temporary table
    */
    List_iterator_fast<Item> field_it(*fields_for_table[cur_table->shared]);
    Field **field= tmp_table->field+1;		// Skip row pointer
    Copy_field *copy_field_ptr= copy_field, *copy_field_end;
    for ( ; *field ; field++)
    {
      Item_field *item= (Item_field* ) field_it++;
      (copy_field_ptr++)->set(item->field, *field, 0);
    }
    copy_field_end=copy_field_ptr;

    if ((local_error = tmp_table->file->rnd_init(1)))
      goto err;

    ref_pos= (byte*) tmp_table->field[0]->ptr;
    for (;;)
    {
      if (thd->killed && trans_safe)
	goto err;
      if ((local_error=tmp_table->file->rnd_next(tmp_table->record[0])))
      {
	if (local_error == HA_ERR_END_OF_FILE)
	  break;
	if (local_error == HA_ERR_RECORD_DELETED)
	  continue;				// May happen on dup key
	goto err;
      }
      if ((local_error= table->file->rnd_pos(table->record[0], ref_pos)))
	goto err;
      table->status|= STATUS_UPDATED;
      store_record(table,1);

      /* Copy data from temporary table to current table */
      for (copy_field_ptr=copy_field;
	   copy_field_ptr != copy_field_end;
	   copy_field_ptr++)
	(*copy_field_ptr->do_copy)(copy_field_ptr);

      if (compare_record(table, thd->query_id))
      {
	if ((local_error=table->file->update_row(table->record[1],
						 table->record[0])))
	{
	  if (local_error != HA_ERR_FOUND_DUPP_KEY ||
	      handle_duplicates != DUP_IGNORE)
	    goto err;
	}
	updated++;
	if (table->tmp_table != NO_TMP_TABLE)
	  log_delayed= 1;
      }
    }

    if (updated != org_updated)
    {
      if (table->tmp_table != NO_TMP_TABLE)
	log_delayed= 1;				// Tmp tables forces delay log
      if (table->file->has_transactions())
	log_delayed= transactional_tables= 1;
      else
	trans_safe= 0;				// Can't do safe rollback
    }
  }
  DBUG_RETURN(0);

err:
  if (!from_send_error)
    table->file->print_error(local_error,MYF(0));

  if (updated != org_updated)
  {
    if (table->tmp_table != NO_TMP_TABLE)
      log_delayed= 1;
    if (table->file->has_transactions())
      log_delayed= transactional_tables= 1;
    else
      trans_safe= 0;
  }
  DBUG_RETURN(1);
}


/* out: 1 if error, 0 if success */

bool multi_update::send_eof()
{
  char buff[80];
  thd->proc_info="updating reference tables";

  /* Does updates for the last n - 1 tables, returns 0 if ok */
  int local_error = (table_count) ? do_updates(0) : 0;
  thd->proc_info= "end";

  /*
    Write the SQL statement to the binlog if we updated
    rows and we succeeded or if we updated some non
    transacational tables
  */

  if (updated && (local_error <= 0 || !trans_safe))
  {
    mysql_update_log.write(thd,thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query, thd->query_length,
			    log_delayed);
      if (mysql_bin_log.write(&qinfo) && trans_safe)
	local_error= 1;				// Rollback update
    }
    if (!log_delayed)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }

  if (transactional_tables)
  {
    if (ha_autocommit_or_rollback(thd, local_error != 0))
      local_error=1;
  }

  if (local_error > 0) // if the above log write did not fail ...
  {
    /* Safety: If we haven't got an error before (should not happen) */
    my_message(ER_UNKNOWN_ERROR, "An error occured in multi-table update",
	       MYF(0));
    ::send_error(thd);
    return 1;
  }


  sprintf(buff,ER(ER_UPDATE_INFO), (long) found, (long) updated,
	  (long) thd->cuted_fields);
  if (updated)
  {
    query_cache_invalidate3(thd, update_tables, 1);
  }
  ::send_ok(thd,
	    (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated,
	    thd->insert_id_used ? thd->insert_id() : 0L,buff);
  return 0;
}
