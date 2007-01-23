/* Copyright (C) 2000-2004 MySQL AB

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

/* drop and alter of tables */

#include "mysql_priv.h"
#ifdef HAVE_BERKELEY_DB
#include "ha_berkeley.h"
#endif
#include <hash.h>
#include <myisam.h>
#include <my_dir.h>
#include "sp_head.h"
#include "sql_trigger.h"

#ifdef __WIN__
#include <io.h>
#endif

const char *primary_key_name="PRIMARY";

static bool check_if_keyname_exists(const char *name,KEY *start, KEY *end);
static char *make_unique_key_name(const char *field_name,KEY *start,KEY *end);
static int copy_data_between_tables(TABLE *from,TABLE *to,
                                    List<create_field> &create, bool ignore,
				    uint order_num, ORDER *order,
				    ha_rows *copied,ha_rows *deleted,
                                    enum enum_enable_or_disable keys_onoff);

static bool prepare_blob_field(THD *thd, create_field *sql_field);
static bool check_engine(THD *thd, const char *table_name,
                         enum db_type *new_engine);                             


/*
 Build the path to a file for a table (or the base path that can
 then have various extensions stuck on to it).

  SYNOPSIS
   build_table_path()
   buff                 Buffer to build the path into
   bufflen              sizeof(buff)
   db                   Name of database
   table                Name of table
   ext                  Filename extension

  RETURN
    0                   Error
    #                   Size of path
 */

static uint build_table_path(char *buff, size_t bufflen, const char *db,
                             const char *table, const char *ext)
{
  strxnmov(buff, bufflen-1, mysql_data_home, "/", db, "/", table, ext,
           NullS);
  return unpack_filename(buff,buff);
}



/*
 delete (drop) tables.

  SYNOPSIS
   mysql_rm_table()
   thd			Thread handle
   tables		List of tables to delete
   if_exists		If 1, don't give error if one table doesn't exists

  NOTES
    Will delete all tables that can be deleted and give a compact error
    messages for tables that could not be deleted.
    If a table is in use, we will wait for all users to free the table
    before dropping it

    Wait if global_read_lock (FLUSH TABLES WITH READ LOCK) is set.

  RETURN
    FALSE OK.  In this case ok packet is sent to user
    TRUE  Error

*/

bool mysql_rm_table(THD *thd,TABLE_LIST *tables, my_bool if_exists,
                    my_bool drop_temporary)
{
  bool error= FALSE, need_start_waiters= FALSE;
  DBUG_ENTER("mysql_rm_table");

  /* mark for close and remove all cached entries */

  if (!drop_temporary)
  {
    if ((error= wait_if_global_read_lock(thd, 0, 1)))
    {
      my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE, MYF(0), tables->table_name);
      DBUG_RETURN(TRUE);
    }
    else
      need_start_waiters= TRUE;
  }

  /*
    Acquire LOCK_open after wait_if_global_read_lock(). If we would hold
    LOCK_open during wait_if_global_read_lock(), other threads could not
    close their tables. This would make a pretty deadlock.
  */
  thd->mysys_var->current_mutex= &LOCK_open;
  thd->mysys_var->current_cond= &COND_refresh;
  VOID(pthread_mutex_lock(&LOCK_open));

  error= mysql_rm_table_part2(thd, tables, if_exists, drop_temporary, 0, 0);

  pthread_mutex_unlock(&LOCK_open);

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond= 0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  if (need_start_waiters)
    start_waiting_global_read_lock(thd);

  if (error)
    DBUG_RETURN(TRUE);
  send_ok(thd);
  DBUG_RETURN(FALSE);
}


/*
 delete (drop) tables.

  SYNOPSIS
    mysql_rm_table_part2_with_lock()
    thd			Thread handle
    tables		List of tables to delete
    if_exists		If 1, don't give error if one table doesn't exists
    dont_log_query	Don't write query to log files. This will also not
                        generate warnings if the handler files doesn't exists

 NOTES
   Works like documented in mysql_rm_table(), but don't check
   global_read_lock and don't send_ok packet to server.

 RETURN
  0	ok
  1	error
*/

int mysql_rm_table_part2_with_lock(THD *thd,
				   TABLE_LIST *tables, bool if_exists,
				   bool drop_temporary, bool dont_log_query)
{
  int error;
  thd->mysys_var->current_mutex= &LOCK_open;
  thd->mysys_var->current_cond= &COND_refresh;
  VOID(pthread_mutex_lock(&LOCK_open));

  error= mysql_rm_table_part2(thd, tables, if_exists, drop_temporary, 1,
			      dont_log_query);

  pthread_mutex_unlock(&LOCK_open);

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond= 0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);
  return error;
}


/*
  Execute the drop of a normal or temporary table

  SYNOPSIS
    mysql_rm_table_part2()
    thd			Thread handler
    tables		Tables to drop
    if_exists		If set, don't give an error if table doesn't exists.
			In this case we give an warning of level 'NOTE'
    drop_temporary	Only drop temporary tables
    drop_view		Allow to delete VIEW .frm
    dont_log_query	Don't write query to log files. This will also not
			generate warnings if the handler files doesn't exists  

  TODO:
    When logging to the binary log, we should log
    tmp_tables and transactional tables as separate statements if we
    are in a transaction;  This is needed to get these tables into the
    cached binary log that is only written on COMMIT.

   The current code only writes DROP statements that only uses temporary
   tables to the cache binary log.  This should be ok on most cases, but
   not all.

 RETURN
   0	ok
   1	Error
   -1	Thread was killed
*/

int mysql_rm_table_part2(THD *thd, TABLE_LIST *tables, bool if_exists,
			 bool drop_temporary, bool drop_view,
			 bool dont_log_query)
{
  TABLE_LIST *table;
  char	path[FN_REFLEN], *alias;
  String wrong_tables;
  int error;
  bool some_tables_deleted=0, tmp_table_deleted=0, foreign_key_error=0;

  DBUG_ENTER("mysql_rm_table_part2");

  if (!drop_temporary && lock_table_names(thd, tables))
    DBUG_RETURN(1);

  /* Don't give warnings for not found errors, as we already generate notes */
  thd->no_warnings_for_error= 1;

  for (table= tables; table; table= table->next_local)
  {
    char *db=table->db;
    db_type table_type= DB_TYPE_UNKNOWN;

    mysql_ha_flush(thd, table, MYSQL_HA_CLOSE_FINAL, TRUE);
    if (!close_temporary_table(thd, db, table->table_name))
    {
      tmp_table_deleted=1;
      continue;					// removed temporary table
    }

    error=0;
    if (!drop_temporary)
    {
      abort_locked_tables(thd, db, table->table_name);
      remove_table_from_cache(thd, db, table->table_name,
	                      RTFC_WAIT_OTHER_THREAD_FLAG |
			      RTFC_CHECK_KILLED_FLAG);
      drop_locked_tables(thd, db, table->table_name);
      if (thd->killed)
      {
        thd->no_warnings_for_error= 0;
	DBUG_RETURN(-1);
      }
      alias= (lower_case_table_names == 2) ? table->alias : table->table_name;
      /* remove form file and isam files */
      build_table_path(path, sizeof(path), db, alias, reg_ext);
    }
    if (drop_temporary ||
       (access(path,F_OK) &&
         ha_create_table_from_engine(thd,db,alias)) ||
        (!drop_view &&
	 mysql_frm_type(thd, path, &table_type) != FRMTYPE_TABLE))
    {
      // Table was not found on disk and table can't be created from engine
      if (if_exists)
	push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			    ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR),
			    table->table_name);
      else
        error= 1;
    }
    else
    {
      char *end;
      if (table_type == DB_TYPE_UNKNOWN)
	mysql_frm_type(thd, path, &table_type);
      *(end=fn_ext(path))=0;			// Remove extension for delete
      error= ha_delete_table(thd, table_type, path, table->table_name,
                             !dont_log_query);
      if ((error == ENOENT || error == HA_ERR_NO_SUCH_TABLE) && 
	  (if_exists || table_type == DB_TYPE_UNKNOWN))
	error= 0;
      if (error == HA_ERR_ROW_IS_REFERENCED)
      {
	/* the table is referenced by a foreign key constraint */
	foreign_key_error=1;
      }
      if (!error || error == ENOENT || error == HA_ERR_NO_SUCH_TABLE)
      {
        int new_error;
	/* Delete the table definition file */
	strmov(end,reg_ext);
	if (!(new_error=my_delete(path,MYF(MY_WME))))
        {
	  some_tables_deleted=1;
          new_error= Table_triggers_list::drop_all_triggers(thd, db,
                                                            table->table_name);
        }
        error|= new_error;
      }
    }
    if (error)
    {
      if (wrong_tables.length())
	wrong_tables.append(',');
      wrong_tables.append(String(table->table_name,system_charset_info));
    }
  }
  thd->tmp_table_used= tmp_table_deleted;
  error= 0;
  if (wrong_tables.length())
  {
    if (!foreign_key_error)
      my_printf_error(ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR), MYF(0),
                      wrong_tables.c_ptr());
    else
      my_message(ER_ROW_IS_REFERENCED, ER(ER_ROW_IS_REFERENCED), MYF(0));
    error= 1;
  }

  if (some_tables_deleted || tmp_table_deleted || !error)
  {
    query_cache_invalidate3(thd, tables, 0);
    if (!dont_log_query && mysql_bin_log.is_open())
    {
      if (!error)
        thd->clear_error();
      Query_log_event qinfo(thd, thd->query, thd->query_length, FALSE, FALSE);
      mysql_bin_log.write(&qinfo);
    }
  }

  if (!drop_temporary)
    unlock_table_names(thd, tables, (TABLE_LIST*) 0);
  thd->no_warnings_for_error= 0;
  DBUG_RETURN(error);
}


int quick_rm_table(enum db_type base,const char *db,
		   const char *table_name)
{
  char path[FN_REFLEN];
  int error=0;
  build_table_path(path, sizeof(path), db, table_name, reg_ext);
  if (my_delete(path,MYF(0)))
    error=1; /* purecov: inspected */
  *fn_ext(path)= 0;                             // Remove reg_ext
  return ha_delete_table(current_thd, base, path, table_name, 0) || error;
}

/*
  Sort keys in the following order:
  - PRIMARY KEY
  - UNIQUE keyws where all column are NOT NULL
  - Other UNIQUE keys
  - Normal keys
  - Fulltext keys

  This will make checking for duplicated keys faster and ensure that
  PRIMARY keys are prioritized.
*/

static int sort_keys(KEY *a, KEY *b)
{
  if (a->flags & HA_NOSAME)
  {
    if (!(b->flags & HA_NOSAME))
      return -1;
    if ((a->flags ^ b->flags) & (HA_NULL_PART_KEY | HA_END_SPACE_KEY))
    {
      /* Sort NOT NULL keys before other keys */
      return (a->flags & (HA_NULL_PART_KEY | HA_END_SPACE_KEY)) ? 1 : -1;
    }
    if (a->name == primary_key_name)
      return -1;
    if (b->name == primary_key_name)
      return 1;
  }
  else if (b->flags & HA_NOSAME)
    return 1;					// Prefer b

  if ((a->flags ^ b->flags) & HA_FULLTEXT)
  {
    return (a->flags & HA_FULLTEXT) ? 1 : -1;
  }
  /*
    Prefer original key order.	usable_key_parts contains here
    the original key position.
  */
  return ((a->usable_key_parts < b->usable_key_parts) ? -1 :
	  (a->usable_key_parts > b->usable_key_parts) ? 1 :
	  0);
}

/*
  Check TYPELIB (set or enum) for duplicates

  SYNOPSIS
    check_duplicates_in_interval()
    set_or_name   "SET" or "ENUM" string for warning message
    name	  name of the checked column
    typelib	  list of values for the column

  DESCRIPTION
    This function prints an warning for each value in list
    which has some duplicates on its right

  RETURN VALUES
    void
*/

void check_duplicates_in_interval(const char *set_or_name,
                                  const char *name, TYPELIB *typelib,
                                  CHARSET_INFO *cs)
{
  TYPELIB tmp= *typelib;
  const char **cur_value= typelib->type_names;
  unsigned int *cur_length= typelib->type_lengths;
  
  for ( ; tmp.count > 1; cur_value++, cur_length++)
  {
    tmp.type_names++;
    tmp.type_lengths++;
    tmp.count--;
    if (find_type2(&tmp, (const char*)*cur_value, *cur_length, cs))
    {
      push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_DUPLICATED_VALUE_IN_TYPE,
			  ER(ER_DUPLICATED_VALUE_IN_TYPE),
			  name,*cur_value,set_or_name);
    }
  }
}


/*
  Check TYPELIB (set or enum) max and total lengths

  SYNOPSIS
    calculate_interval_lengths()
    cs            charset+collation pair of the interval
    typelib       list of values for the column
    max_length    length of the longest item
    tot_length    sum of the item lengths

  DESCRIPTION
    After this function call:
    - ENUM uses max_length
    - SET uses tot_length.

  RETURN VALUES
    void
*/
void calculate_interval_lengths(CHARSET_INFO *cs, TYPELIB *interval,
                                uint32 *max_length, uint32 *tot_length)
{
  const char **pos;
  uint *len;
  *max_length= *tot_length= 0;
  for (pos= interval->type_names, len= interval->type_lengths;
       *pos ; pos++, len++)
  {
    uint length= cs->cset->numchars(cs, *pos, *pos + *len);
    *tot_length+= length;
    set_if_bigger(*max_length, (uint32)length);
  }
}


/*
  Prepare a create_table instance for packing

  SYNOPSIS
    prepare_create_field()
    sql_field     field to prepare for packing
    blob_columns  count for BLOBs
    timestamps    count for timestamps
    table_flags   table flags

  DESCRIPTION
    This function prepares a create_field instance.
    Fields such as pack_flag are valid after this call.

  RETURN VALUES
   0	ok
   1	Error
*/

int prepare_create_field(create_field *sql_field, 
			 uint *blob_columns, 
			 int *timestamps, int *timestamps_with_niladic,
			 uint table_flags)
{
  DBUG_ENTER("prepare_field");

  /*
    This code came from mysql_prepare_table.
    Indent preserved to make patching easier
  */
  DBUG_ASSERT(sql_field->charset);

  switch (sql_field->sql_type) {
  case FIELD_TYPE_BLOB:
  case FIELD_TYPE_MEDIUM_BLOB:
  case FIELD_TYPE_TINY_BLOB:
  case FIELD_TYPE_LONG_BLOB:
    sql_field->pack_flag=FIELDFLAG_BLOB |
      pack_length_to_packflag(sql_field->pack_length -
                              portable_sizeof_char_ptr);
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->length=8;			// Unireg field length
    sql_field->unireg_check=Field::BLOB_FIELD;
    (*blob_columns)++;
    break;
  case FIELD_TYPE_GEOMETRY:
#ifdef HAVE_SPATIAL
    if (!(table_flags & HA_CAN_GEOMETRY))
    {
      my_printf_error(ER_CHECK_NOT_IMPLEMENTED, ER(ER_CHECK_NOT_IMPLEMENTED),
                      MYF(0), "GEOMETRY");
      DBUG_RETURN(1);
    }
    sql_field->pack_flag=FIELDFLAG_GEOM |
      pack_length_to_packflag(sql_field->pack_length -
                              portable_sizeof_char_ptr);
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->length=8;			// Unireg field length
    sql_field->unireg_check=Field::BLOB_FIELD;
    (*blob_columns)++;
    break;
#else
    my_printf_error(ER_FEATURE_DISABLED,ER(ER_FEATURE_DISABLED), MYF(0),
                    sym_group_geom.name, sym_group_geom.needed_define);
    DBUG_RETURN(1);
#endif /*HAVE_SPATIAL*/
  case MYSQL_TYPE_VARCHAR:
#ifndef QQ_ALL_HANDLERS_SUPPORT_VARCHAR
    if (table_flags & HA_NO_VARCHAR)
    {
      /* convert VARCHAR to CHAR because handler is not yet up to date */
      sql_field->sql_type=    MYSQL_TYPE_VAR_STRING;
      sql_field->pack_length= calc_pack_length(sql_field->sql_type,
                                               (uint) sql_field->length);
      if ((sql_field->length / sql_field->charset->mbmaxlen) >
          MAX_FIELD_CHARLENGTH)
      {
        my_printf_error(ER_TOO_BIG_FIELDLENGTH, ER(ER_TOO_BIG_FIELDLENGTH),
                        MYF(0), sql_field->field_name, MAX_FIELD_CHARLENGTH);
        DBUG_RETURN(1);
      }
    }
#endif
    /* fall through */
  case FIELD_TYPE_STRING:
    sql_field->pack_flag=0;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    break;
  case FIELD_TYPE_ENUM:
    sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
      FIELDFLAG_INTERVAL;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->unireg_check=Field::INTERVAL_FIELD;
    check_duplicates_in_interval("ENUM",sql_field->field_name,
                                 sql_field->interval,
                                 sql_field->charset);
    break;
  case FIELD_TYPE_SET:
    sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
      FIELDFLAG_BITFIELD;
    if (sql_field->charset->state & MY_CS_BINSORT)
      sql_field->pack_flag|=FIELDFLAG_BINARY;
    sql_field->unireg_check=Field::BIT_FIELD;
    check_duplicates_in_interval("SET",sql_field->field_name,
                                 sql_field->interval,
                                 sql_field->charset);
    break;
  case FIELD_TYPE_DATE:			// Rest of string types
  case FIELD_TYPE_NEWDATE:
  case FIELD_TYPE_TIME:
  case FIELD_TYPE_DATETIME:
  case FIELD_TYPE_NULL:
    sql_field->pack_flag=f_settype((uint) sql_field->sql_type);
    break;
  case FIELD_TYPE_BIT:
    /* 
      We have sql_field->pack_flag already set here, see mysql_prepare_table().
    */
    break;
  case FIELD_TYPE_NEWDECIMAL:
    sql_field->pack_flag=(FIELDFLAG_NUMBER |
                          (sql_field->flags & UNSIGNED_FLAG ? 0 :
                           FIELDFLAG_DECIMAL) |
                          (sql_field->flags & ZEROFILL_FLAG ?
                           FIELDFLAG_ZEROFILL : 0) |
                          (sql_field->decimals << FIELDFLAG_DEC_SHIFT));
    break;
  case FIELD_TYPE_TIMESTAMP:
    /* We should replace old TIMESTAMP fields with their newer analogs */
    if (sql_field->unireg_check == Field::TIMESTAMP_OLD_FIELD)
    {
      if (!*timestamps)
      {
        sql_field->unireg_check= Field::TIMESTAMP_DNUN_FIELD;
        (*timestamps_with_niladic)++;
      }
      else
        sql_field->unireg_check= Field::NONE;
    }
    else if (sql_field->unireg_check != Field::NONE)
      (*timestamps_with_niladic)++;

    (*timestamps)++;
    /* fall-through */
  default:
    sql_field->pack_flag=(FIELDFLAG_NUMBER |
                          (sql_field->flags & UNSIGNED_FLAG ? 0 :
                           FIELDFLAG_DECIMAL) |
                          (sql_field->flags & ZEROFILL_FLAG ?
                           FIELDFLAG_ZEROFILL : 0) |
                          f_settype((uint) sql_field->sql_type) |
                          (sql_field->decimals << FIELDFLAG_DEC_SHIFT));
    break;
  }
  if (!(sql_field->flags & NOT_NULL_FLAG))
    sql_field->pack_flag|= FIELDFLAG_MAYBE_NULL;
  if (sql_field->flags & NO_DEFAULT_VALUE_FLAG)
    sql_field->pack_flag|= FIELDFLAG_NO_DEFAULT;
  DBUG_RETURN(0);
}

/*
  Preparation for table creation

  SYNOPSIS
    mysql_prepare_table()
    thd			Thread object
    create_info		Create information (like MAX_ROWS)
    alter_info          List of columns and indexes to create

  DESCRIPTION
    Prepares the table and key structures for table creation.

  NOTES
    sets create_info->varchar if the table has a varchar

  RETURN VALUES
    0	ok
    -1	error
*/

static int mysql_prepare_table(THD *thd, HA_CREATE_INFO *create_info,
                               Alter_info *alter_info,
                               bool tmp_table,
                               uint *db_options,
                               handler *file, KEY **key_info_buffer,
                               uint *key_count, int select_field_count)
{
  const char	*key_name;
  create_field	*sql_field,*dup_field;
  uint		field,null_fields,blob_columns,max_key_length;
  ulong		record_offset= 0;
  KEY		*key_info;
  KEY_PART_INFO *key_part_info;
  int		timestamps= 0, timestamps_with_niladic= 0;
  int		field_no,dup_no;
  int		select_field_pos,auto_increment=0;
  List_iterator<create_field> it(alter_info->create_list);
  List_iterator<create_field> it2(alter_info->create_list);
  uint total_uneven_bit_length= 0;
  DBUG_ENTER("mysql_prepare_table");

  select_field_pos= alter_info->create_list.elements - select_field_count;
  null_fields=blob_columns=0;
  create_info->varchar= 0;
  max_key_length= file->max_key_length();

  for (field_no=0; (sql_field=it++) ; field_no++)
  {
    CHARSET_INFO *save_cs;

    /*
      Initialize length from its original value (number of characters),
      which was set in the parser. This is necessary if we're
      executing a prepared statement for the second time.
    */
    sql_field->length= sql_field->char_length;
    if (!sql_field->charset)
      sql_field->charset= create_info->default_table_charset;
    /*
      table_charset is set in ALTER TABLE if we want change character set
      for all varchar/char columns.
      But the table charset must not affect the BLOB fields, so don't
      allow to change my_charset_bin to somethig else.
    */
    if (create_info->table_charset && sql_field->charset != &my_charset_bin)
      sql_field->charset= create_info->table_charset;

    save_cs= sql_field->charset;
    if ((sql_field->flags & BINCMP_FLAG) &&
	!(sql_field->charset= get_charset_by_csname(sql_field->charset->csname,
						    MY_CS_BINSORT,MYF(0))))
    {
      char tmp[64];
      strmake(strmake(tmp, save_cs->csname, sizeof(tmp)-4),
              STRING_WITH_LEN("_bin"));
      my_error(ER_UNKNOWN_COLLATION, MYF(0), tmp);
      DBUG_RETURN(-1);
    }

    /*
      Convert the default value from client character
      set into the column character set if necessary.
    */
    if (sql_field->def && 
        save_cs != sql_field->def->collation.collation &&
        (sql_field->sql_type == FIELD_TYPE_VAR_STRING ||
         sql_field->sql_type == FIELD_TYPE_STRING ||
         sql_field->sql_type == FIELD_TYPE_SET ||
         sql_field->sql_type == FIELD_TYPE_ENUM))
    {
      Query_arena backup_arena;
      bool need_to_change_arena= !thd->stmt_arena->is_conventional();
      if (need_to_change_arena)
      {
        /* Asser that we don't do that at every PS execute */
        DBUG_ASSERT(thd->stmt_arena->is_first_stmt_execute() ||
                    thd->stmt_arena->is_first_sp_execute());
        thd->set_n_backup_active_arena(thd->stmt_arena, &backup_arena);
      }

      sql_field->def= sql_field->def->safe_charset_converter(save_cs);

      if (need_to_change_arena)
        thd->restore_active_arena(thd->stmt_arena, &backup_arena);

      if (sql_field->def == NULL)
      {
        /* Could not convert */
        my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
        DBUG_RETURN(-1);
      }
    }

    if (sql_field->sql_type == FIELD_TYPE_SET ||
        sql_field->sql_type == FIELD_TYPE_ENUM)
    {
      uint32 dummy;
      CHARSET_INFO *cs= sql_field->charset;
      TYPELIB *interval= sql_field->interval;

      /*
        Create typelib from interval_list, and if necessary
        convert strings from client character set to the
        column character set.
      */
      if (!interval)
      {
        /*
          Create the typelib in prepared statement memory if we're
          executing one.
        */
        MEM_ROOT *stmt_root= thd->stmt_arena->mem_root;

        interval= sql_field->interval= typelib(stmt_root,
                                               sql_field->interval_list);
        List_iterator<String> it(sql_field->interval_list);
        String conv, *tmp;
        char comma_buf[2];
        int comma_length= cs->cset->wc_mb(cs, ',', (uchar*) comma_buf,
                                          (uchar*) comma_buf + 
                                          sizeof(comma_buf));
        DBUG_ASSERT(comma_length > 0);
        for (uint i= 0; (tmp= it++); i++)
        {
          uint lengthsp;
          if (String::needs_conversion(tmp->length(), tmp->charset(),
                                       cs, &dummy))
          {
            uint cnv_errs;
            conv.copy(tmp->ptr(), tmp->length(), tmp->charset(), cs, &cnv_errs);
            interval->type_names[i]= strmake_root(stmt_root, conv.ptr(),
                                                  conv.length());
            interval->type_lengths[i]= conv.length();
          }

          // Strip trailing spaces.
          lengthsp= cs->cset->lengthsp(cs, interval->type_names[i],
                                       interval->type_lengths[i]);
          interval->type_lengths[i]= lengthsp;
          ((uchar *)interval->type_names[i])[lengthsp]= '\0';
          if (sql_field->sql_type == FIELD_TYPE_SET)
          {
            if (cs->coll->instr(cs, interval->type_names[i], 
                                interval->type_lengths[i], 
                                comma_buf, comma_length, NULL, 0))
            {
              my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "set", tmp->ptr());
              DBUG_RETURN(-1);
            }
          }
        }
        sql_field->interval_list.empty(); // Don't need interval_list anymore
      }

      if (sql_field->sql_type == FIELD_TYPE_SET)
      {
        uint32 field_length;
        if (sql_field->def != NULL)
        {
          char *not_used;
          uint not_used2;
          bool not_found= 0;
          String str, *def= sql_field->def->val_str(&str);
          if (def == NULL) /* SQL "NULL" maps to NULL */
          {
            if ((sql_field->flags & NOT_NULL_FLAG) != 0)
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(-1);
            }

            /* else, NULL is an allowed value */
            (void) find_set(interval, NULL, 0,
                            cs, &not_used, &not_used2, &not_found);
          }
          else /* not NULL */
          {
            (void) find_set(interval, def->ptr(), def->length(),
                            cs, &not_used, &not_used2, &not_found);
          }

          if (not_found)
          {
            my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
            DBUG_RETURN(-1);
          }
        }
        calculate_interval_lengths(cs, interval, &dummy, &field_length);
        sql_field->length= field_length + (interval->count - 1);
      }
      else  /* FIELD_TYPE_ENUM */
      {
        uint32 field_length;
        DBUG_ASSERT(sql_field->sql_type == FIELD_TYPE_ENUM);
        if (sql_field->def != NULL)
        {
          String str, *def= sql_field->def->val_str(&str);
          if (def == NULL) /* SQL "NULL" maps to NULL */
          {
            if ((sql_field->flags & NOT_NULL_FLAG) != 0)
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(-1);
            }

            /* else, the defaults yield the correct length for NULLs. */
          } 
          else /* not NULL */
          {
            def->length(cs->cset->lengthsp(cs, def->ptr(), def->length()));
            if (find_type2(interval, def->ptr(), def->length(), cs) == 0) /* not found */
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(-1);
            }
          }
        }
        calculate_interval_lengths(cs, interval, &field_length, &dummy);
        sql_field->length= field_length;
      }
      set_if_smaller(sql_field->length, MAX_FIELD_WIDTH-1);
    }

    if (sql_field->sql_type == FIELD_TYPE_BIT)
    { 
      sql_field->pack_flag= FIELDFLAG_NUMBER;
      if (file->table_flags() & HA_CAN_BIT_FIELD)
        total_uneven_bit_length+= sql_field->length & 7;
      else
        sql_field->pack_flag|= FIELDFLAG_TREAT_BIT_AS_CHAR;
    }

    sql_field->create_length_to_internal_length();
    if (prepare_blob_field(thd, sql_field))
      DBUG_RETURN(-1);

    if (!(sql_field->flags & NOT_NULL_FLAG))
      null_fields++;

    if (check_column_name(sql_field->field_name))
    {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), sql_field->field_name);
      DBUG_RETURN(-1);
    }

    /* Check if we have used the same field name before */
    for (dup_no=0; (dup_field=it2++) != sql_field; dup_no++)
    {
      if (my_strcasecmp(system_charset_info,
			sql_field->field_name,
			dup_field->field_name) == 0)
      {
	/*
	  If this was a CREATE ... SELECT statement, accept a field
	  redefinition if we are changing a field in the SELECT part
	*/
	if (field_no < select_field_pos || dup_no >= select_field_pos)
	{
	  my_error(ER_DUP_FIELDNAME, MYF(0), sql_field->field_name);
	  DBUG_RETURN(-1);
	}
	else
	{
	  /* Field redefined */
	  sql_field->def=		dup_field->def;
	  sql_field->sql_type=		dup_field->sql_type;
	  sql_field->charset=		(dup_field->charset ?
					 dup_field->charset :
					 create_info->default_table_charset);
	  sql_field->length=		dup_field->char_length;
          sql_field->pack_length=	dup_field->pack_length;
          sql_field->key_length=	dup_field->key_length;
	  sql_field->create_length_to_internal_length();
	  sql_field->decimals=		dup_field->decimals;
	  sql_field->unireg_check=	dup_field->unireg_check;
          /* 
            We're making one field from two, the result field will have
            dup_field->flags as flags. If we've incremented null_fields
            because of sql_field->flags, decrement it back.
          */
          if (!(sql_field->flags & NOT_NULL_FLAG))
            null_fields--;
	  sql_field->flags=		dup_field->flags;
          sql_field->interval=          dup_field->interval;
	  it2.remove();			// Remove first (create) definition
	  select_field_pos--;
	  break;
	}
      }
    }
    /* Don't pack rows in old tables if the user has requested this */
    if ((sql_field->flags & BLOB_FLAG) ||
	sql_field->sql_type == MYSQL_TYPE_VARCHAR &&
	create_info->row_type != ROW_TYPE_FIXED)
      (*db_options)|= HA_OPTION_PACK_RECORD;
    it2.rewind();
  }

  /* record_offset will be increased with 'length-of-null-bits' later */
  record_offset= 0;
  null_fields+= total_uneven_bit_length;

  it.rewind();
  while ((sql_field=it++))
  {
    DBUG_ASSERT(sql_field->charset != 0);

    if (prepare_create_field(sql_field, &blob_columns, 
			     &timestamps, &timestamps_with_niladic,
			     file->table_flags()))
      DBUG_RETURN(-1);
    if (sql_field->sql_type == MYSQL_TYPE_VARCHAR)
      create_info->varchar= 1;
    sql_field->offset= record_offset;
    if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
      auto_increment++;
    record_offset+= sql_field->pack_length;
  }
  if (timestamps_with_niladic > 1)
  {
    my_message(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS,
               ER(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS), MYF(0));
    DBUG_RETURN(-1);
  }
  if (auto_increment > 1)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    DBUG_RETURN(-1);
  }
  if (auto_increment &&
      (file->table_flags() & HA_NO_AUTO_INCREMENT))
  {
    my_message(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT,
               ER(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT), MYF(0));
    DBUG_RETURN(-1);
  }

  if (blob_columns && (file->table_flags() & HA_NO_BLOBS))
  {
    my_message(ER_TABLE_CANT_HANDLE_BLOB, ER(ER_TABLE_CANT_HANDLE_BLOB),
               MYF(0));
    DBUG_RETURN(-1);
  }

  /* Create keys */

  List_iterator<Key> key_iterator(alter_info->key_list);
  List_iterator<Key> key_iterator2(alter_info->key_list);
  uint key_parts=0, fk_key_count=0;
  bool primary_key=0,unique_key=0;
  Key *key, *key2;
  uint tmp, key_number;
  /* special marker for keys to be ignored */
  static char ignore_key[1];

  /* Calculate number of key segements */
  *key_count= 0;

  while ((key=key_iterator++))
  {
    if (key->type == Key::FOREIGN_KEY)
    {
      fk_key_count++;
      foreign_key *fk_key= (foreign_key*) key;
      if (fk_key->ref_columns.elements &&
	  fk_key->ref_columns.elements != fk_key->columns.elements)
      {
        my_error(ER_WRONG_FK_DEF, MYF(0),
                 (fk_key->name ?  fk_key->name : "foreign key without name"),
                 ER(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF));
	DBUG_RETURN(-1);
      }
      continue;
    }
    (*key_count)++;
    tmp=file->max_key_parts();
    if (key->columns.elements > tmp)
    {
      my_error(ER_TOO_MANY_KEY_PARTS,MYF(0),tmp);
      DBUG_RETURN(-1);
    }
    if (key->name && strlen(key->name) > NAME_LEN)
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), key->name);
      DBUG_RETURN(-1);
    }
    key_iterator2.rewind ();
    if (key->type != Key::FOREIGN_KEY)
    {
      while ((key2 = key_iterator2++) != key)
      {
	/*
          foreign_key_prefix(key, key2) returns 0 if key or key2, or both, is
          'generated', and a generated key is a prefix of the other key.
          Then we do not need the generated shorter key.
        */
        if ((key2->type != Key::FOREIGN_KEY &&
             key2->name != ignore_key &&
             !foreign_key_prefix(key, key2)))
        {
          /* TODO: issue warning message */
          /* mark that the generated key should be ignored */
          if (!key2->generated ||
              (key->generated && key->columns.elements <
               key2->columns.elements))
            key->name= ignore_key;
          else
          {
            key2->name= ignore_key;
            key_parts-= key2->columns.elements;
            (*key_count)--;
          }
          break;
        }
      }
    }
    if (key->name != ignore_key)
      key_parts+=key->columns.elements;
    else
      (*key_count)--;
    if (key->name && !tmp_table &&
	!my_strcasecmp(system_charset_info,key->name,primary_key_name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name);
      DBUG_RETURN(-1);
    }
  }
  tmp=file->max_keys();
  if (*key_count > tmp)
  {
    my_error(ER_TOO_MANY_KEYS,MYF(0),tmp);
    DBUG_RETURN(-1);
  }

  (*key_info_buffer) = key_info= (KEY*) sql_calloc(sizeof(KEY)* *key_count);
  key_part_info=(KEY_PART_INFO*) sql_calloc(sizeof(KEY_PART_INFO)*key_parts);
  if (!*key_info_buffer || ! key_part_info)
    DBUG_RETURN(-1);				// Out of memory

  key_iterator.rewind();
  key_number=0;
  for (; (key=key_iterator++) ; key_number++)
  {
    uint key_length=0;
    key_part_spec *column;

    if (key->name == ignore_key)
    {
      /* ignore redundant keys */
      do
	key=key_iterator++;
      while (key && key->name == ignore_key);
      if (!key)
	break;
    }

    switch(key->type){
    case Key::MULTIPLE:
	key_info->flags= 0;
	break;
    case Key::FULLTEXT:
	key_info->flags= HA_FULLTEXT;
	break;
    case Key::SPATIAL:
#ifdef HAVE_SPATIAL
	key_info->flags= HA_SPATIAL;
	break;
#else
	my_error(ER_FEATURE_DISABLED, MYF(0),
                 sym_group_geom.name, sym_group_geom.needed_define);
	DBUG_RETURN(-1);
#endif
    case Key::FOREIGN_KEY:
      key_number--;				// Skip this key
      continue;
    default:
      key_info->flags = HA_NOSAME;
      break;
    }
    if (key->generated)
      key_info->flags|= HA_GENERATED_KEY;

    key_info->key_parts=(uint8) key->columns.elements;
    key_info->key_part=key_part_info;
    key_info->usable_key_parts= key_number;
    key_info->algorithm=key->algorithm;

    if (key->type == Key::FULLTEXT)
    {
      if (!(file->table_flags() & HA_CAN_FULLTEXT))
      {
	my_message(ER_TABLE_CANT_HANDLE_FT, ER(ER_TABLE_CANT_HANDLE_FT),
                   MYF(0));
	DBUG_RETURN(-1);
      }
    }
    /*
       Make SPATIAL to be RTREE by default
       SPATIAL only on BLOB or at least BINARY, this
       actually should be replaced by special GEOM type
       in near future when new frm file is ready
       checking for proper key parts number:
    */

    /* TODO: Add proper checks if handler supports key_type and algorithm */
    if (key_info->flags & HA_SPATIAL)
    {
      if (!(file->table_flags() & HA_CAN_RTREEKEYS))
      {
        my_message(ER_TABLE_CANT_HANDLE_SPKEYS, ER(ER_TABLE_CANT_HANDLE_SPKEYS),
                   MYF(0));
        DBUG_RETURN(-1);
      }
      if (key_info->key_parts != 1)
      {
	my_error(ER_WRONG_ARGUMENTS, MYF(0), "SPATIAL INDEX");
	DBUG_RETURN(-1);
      }
    }
    else if (key_info->algorithm == HA_KEY_ALG_RTREE)
    {
#ifdef HAVE_RTREE_KEYS
      if ((key_info->key_parts & 1) == 1)
      {
	my_error(ER_WRONG_ARGUMENTS, MYF(0), "RTREE INDEX");
	DBUG_RETURN(-1);
      }
      /* TODO: To be deleted */
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "RTREE INDEX");
      DBUG_RETURN(-1);
#else
      my_error(ER_FEATURE_DISABLED, MYF(0),
               sym_group_rtree.name, sym_group_rtree.needed_define);
      DBUG_RETURN(-1);
#endif
    }

    List_iterator<key_part_spec> cols(key->columns), cols2(key->columns);
    CHARSET_INFO *ft_key_charset=0;  // for FULLTEXT
    for (uint column_nr=0 ; (column=cols++) ; column_nr++)
    {
      uint length;
      key_part_spec *dup_column;

      it.rewind();
      field=0;
      while ((sql_field=it++) &&
	     my_strcasecmp(system_charset_info,
			   column->field_name,
			   sql_field->field_name))
	field++;
      if (!sql_field)
      {
	my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name);
	DBUG_RETURN(-1);
      }
      while ((dup_column= cols2++) != column)
      {
        if (!my_strcasecmp(system_charset_info,
	     	           column->field_name, dup_column->field_name))
	{
	  my_printf_error(ER_DUP_FIELDNAME,
			  ER(ER_DUP_FIELDNAME),MYF(0),
			  column->field_name);
	  DBUG_RETURN(-1);
	}
      }
      cols2.rewind();
      if (key->type == Key::FULLTEXT)
      {
	if ((sql_field->sql_type != MYSQL_TYPE_STRING &&
	     sql_field->sql_type != MYSQL_TYPE_VARCHAR &&
	     !f_is_blob(sql_field->pack_flag)) ||
	    sql_field->charset == &my_charset_bin ||
	    sql_field->charset->mbminlen > 1 || // ucs2 doesn't work yet
	    (ft_key_charset && sql_field->charset != ft_key_charset))
	{
	    my_error(ER_BAD_FT_COLUMN, MYF(0), column->field_name);
	    DBUG_RETURN(-1);
	}
	ft_key_charset=sql_field->charset;
	/*
	  for fulltext keys keyseg length is 1 for blobs (it's ignored in ft
	  code anyway, and 0 (set to column width later) for char's. it has
	  to be correct col width for char's, as char data are not prefixed
	  with length (unlike blobs, where ft code takes data length from a
	  data prefix, ignoring column->length).
	*/
	column->length=test(f_is_blob(sql_field->pack_flag));
      }
      else
      {
	column->length*= sql_field->charset->mbmaxlen;

	if (f_is_blob(sql_field->pack_flag) ||
            (f_is_geom(sql_field->pack_flag) && key->type != Key::SPATIAL))
	{
	  if (!(file->table_flags() & HA_CAN_INDEX_BLOBS))
	  {
	    my_error(ER_BLOB_USED_AS_KEY, MYF(0), column->field_name);
	    DBUG_RETURN(-1);
	  }
          if (f_is_geom(sql_field->pack_flag) && sql_field->geom_type ==
              Field::GEOM_POINT)
            column->length= 21;
	  if (!column->length)
	  {
	    my_error(ER_BLOB_KEY_WITHOUT_LENGTH, MYF(0), column->field_name);
	    DBUG_RETURN(-1);
	  }
	}
#ifdef HAVE_SPATIAL
	if (key->type == Key::SPATIAL)
	{
	  if (!column->length)
	  {
	    /*
              4 is: (Xmin,Xmax,Ymin,Ymax), this is for 2D case
              Lately we'll extend this code to support more dimensions
	    */
	    column->length= 4*sizeof(double);
	  }
	}
#endif
	if (!(sql_field->flags & NOT_NULL_FLAG))
	{
	  if (key->type == Key::PRIMARY)
	  {
	    /* Implicitly set primary key fields to NOT NULL for ISO conf. */
	    sql_field->flags|= NOT_NULL_FLAG;
	    sql_field->pack_flag&= ~FIELDFLAG_MAYBE_NULL;
            null_fields--;
	  }
	  else
	     key_info->flags|= HA_NULL_PART_KEY;
	  if (!(file->table_flags() & HA_NULL_IN_KEY))
	  {
	    my_error(ER_NULL_COLUMN_IN_INDEX, MYF(0), column->field_name);
	    DBUG_RETURN(-1);
	  }
	  if (key->type == Key::SPATIAL)
	  {
	    my_message(ER_SPATIAL_CANT_HAVE_NULL,
                       ER(ER_SPATIAL_CANT_HAVE_NULL), MYF(0));
	    DBUG_RETURN(-1);
	  }
	}
	if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
	{
	  if (column_nr == 0 || (file->table_flags() & HA_AUTO_PART_KEY))
	    auto_increment--;			// Field is used
	}
      }

      key_part_info->fieldnr= field;
      key_part_info->offset=  (uint16) sql_field->offset;
      key_part_info->key_type=sql_field->pack_flag;
      length= sql_field->key_length;

      if (column->length)
      {
	if (f_is_blob(sql_field->pack_flag))
	{
	  if ((length=column->length) > max_key_length ||
	      length > file->max_key_part_length())
	  {
	    length=min(max_key_length, file->max_key_part_length());
	    if (key->type == Key::MULTIPLE)
	    {
	      /* not a critical problem */
	      char warn_buff[MYSQL_ERRMSG_SIZE];
	      my_snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
			  length);
	      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			   ER_TOO_LONG_KEY, warn_buff);
	    }
	    else
	    {
	      my_error(ER_TOO_LONG_KEY,MYF(0),length);
	      DBUG_RETURN(-1);
	    }
	  }
	}
	else if (!f_is_geom(sql_field->pack_flag) &&
		  (column->length > length ||
		   ((f_is_packed(sql_field->pack_flag) ||
		     ((file->table_flags() & HA_NO_PREFIX_CHAR_KEYS) &&
		      (key_info->flags & HA_NOSAME))) &&
		    column->length != length)))
	{
	  my_message(ER_WRONG_SUB_KEY, ER(ER_WRONG_SUB_KEY), MYF(0));
	  DBUG_RETURN(-1);
	}
	else if (!(file->table_flags() & HA_NO_PREFIX_CHAR_KEYS))
	  length=column->length;
      }
      else if (length == 0)
      {
	my_error(ER_WRONG_KEY_COLUMN, MYF(0), column->field_name);
	  DBUG_RETURN(-1);
      }
      if (length > file->max_key_part_length() && key->type != Key::FULLTEXT)
      {
        length= file->max_key_part_length();
        /* Align key length to multibyte char boundary */
        length-= length % sql_field->charset->mbmaxlen;
	if (key->type == Key::MULTIPLE)
	{
	  /* not a critical problem */
	  char warn_buff[MYSQL_ERRMSG_SIZE];
	  my_snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
		      length);
	  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
		       ER_TOO_LONG_KEY, warn_buff);
	}
	else
	{
	  my_error(ER_TOO_LONG_KEY,MYF(0),length);
	  DBUG_RETURN(-1);
	}
      }
      key_part_info->length=(uint16) length;
      /* Use packed keys for long strings on the first column */
      if (!((*db_options) & HA_OPTION_NO_PACK_KEYS) &&
	  (length >= KEY_DEFAULT_PACK_LENGTH &&
	   (sql_field->sql_type == MYSQL_TYPE_STRING ||
	    sql_field->sql_type == MYSQL_TYPE_VARCHAR ||
	    sql_field->pack_flag & FIELDFLAG_BLOB)))
      {
	if (column_nr == 0 && (sql_field->pack_flag & FIELDFLAG_BLOB) ||
            sql_field->sql_type == MYSQL_TYPE_VARCHAR)
	  key_info->flags|= HA_BINARY_PACK_KEY | HA_VAR_LENGTH_KEY;
	else
	  key_info->flags|= HA_PACK_KEY;
      }
      key_length+=length;
      key_part_info++;

      /* Create the key name based on the first column (if not given) */
      if (column_nr == 0)
      {
	if (key->type == Key::PRIMARY)
	{
	  if (primary_key)
	  {
	    my_message(ER_MULTIPLE_PRI_KEY, ER(ER_MULTIPLE_PRI_KEY),
                       MYF(0));
	    DBUG_RETURN(-1);
	  }
	  key_name=primary_key_name;
	  primary_key=1;
	}
	else if (!(key_name = key->name))
	  key_name=make_unique_key_name(sql_field->field_name,
					*key_info_buffer, key_info);
	if (check_if_keyname_exists(key_name, *key_info_buffer, key_info))
	{
	  my_error(ER_DUP_KEYNAME, MYF(0), key_name);
	  DBUG_RETURN(-1);
	}
	key_info->name=(char*) key_name;
      }
    }
    if (!key_info->name || check_column_name(key_info->name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key_info->name);
      DBUG_RETURN(-1);
    }
    if (!(key_info->flags & HA_NULL_PART_KEY))
      unique_key=1;
    key_info->key_length=(uint16) key_length;
    if (key_length > max_key_length && key->type != Key::FULLTEXT)
    {
      my_error(ER_TOO_LONG_KEY,MYF(0),max_key_length);
      DBUG_RETURN(-1);
    }
    key_info++;
  }
  if (!unique_key && !primary_key &&
      (file->table_flags() & HA_REQUIRE_PRIMARY_KEY))
  {
    my_message(ER_REQUIRES_PRIMARY_KEY, ER(ER_REQUIRES_PRIMARY_KEY), MYF(0));
    DBUG_RETURN(-1);
  }
  if (auto_increment > 0)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    DBUG_RETURN(-1);
  }
  /* Sort keys in optimized order */
  qsort((gptr) *key_info_buffer, *key_count, sizeof(KEY),
	(qsort_cmp) sort_keys);
  create_info->null_bits= null_fields;

  DBUG_RETURN(0);
}


/*
  Extend long VARCHAR fields to blob & prepare field if it's a blob

  SYNOPSIS
    prepare_blob_field()
    sql_field		Field to check

  RETURN
    0	ok
    1	Error (sql_field can't be converted to blob)
        In this case the error is given
*/

static bool prepare_blob_field(THD *thd, create_field *sql_field)
{
  DBUG_ENTER("prepare_blob_field");

  if (sql_field->length > MAX_FIELD_VARCHARLENGTH &&
      !(sql_field->flags & BLOB_FLAG))
  {
    /* Convert long VARCHAR columns to TEXT or BLOB */
    char warn_buff[MYSQL_ERRMSG_SIZE];

    if (sql_field->def || (thd->variables.sql_mode & (MODE_STRICT_TRANS_TABLES |
                                                      MODE_STRICT_ALL_TABLES)))
    {
      my_error(ER_TOO_BIG_FIELDLENGTH, MYF(0), sql_field->field_name,
               MAX_FIELD_VARCHARLENGTH / sql_field->charset->mbmaxlen);
      DBUG_RETURN(1);
    }
    sql_field->sql_type= FIELD_TYPE_BLOB;
    sql_field->flags|= BLOB_FLAG;
    sprintf(warn_buff, ER(ER_AUTO_CONVERT), sql_field->field_name,
            (sql_field->charset == &my_charset_bin) ? "VARBINARY" : "VARCHAR",
            (sql_field->charset == &my_charset_bin) ? "BLOB" : "TEXT");
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE, ER_AUTO_CONVERT,
                 warn_buff);
  }
    
  if ((sql_field->flags & BLOB_FLAG) && sql_field->length)
  {
    if (sql_field->sql_type == FIELD_TYPE_BLOB)
    {
      /* The user has given a length to the blob column */
      sql_field->sql_type= get_blob_type_from_length(sql_field->length);
      sql_field->pack_length= calc_pack_length(sql_field->sql_type, 0);
    }
    sql_field->length= 0;
  }
  DBUG_RETURN(0);
}


/*
  Preparation of create_field for SP function return values.
  Based on code used in the inner loop of mysql_prepare_table() above

  SYNOPSIS
    sp_prepare_create_field()
    thd			Thread object
    sql_field		Field to prepare

  DESCRIPTION
    Prepares the field structures for field creation.

*/

void sp_prepare_create_field(THD *thd, create_field *sql_field)
{
  if (sql_field->sql_type == FIELD_TYPE_SET ||
      sql_field->sql_type == FIELD_TYPE_ENUM)
  {
    uint32 field_length, dummy;
    if (sql_field->sql_type == FIELD_TYPE_SET)
    {
      calculate_interval_lengths(sql_field->charset,
                                 sql_field->interval, &dummy, 
                                 &field_length);
      sql_field->length= field_length + 
                         (sql_field->interval->count - 1);
    }
    else /* FIELD_TYPE_ENUM */
    {
      calculate_interval_lengths(sql_field->charset,
                                 sql_field->interval,
                                 &field_length, &dummy);
      sql_field->length= field_length;
    }
    set_if_smaller(sql_field->length, MAX_FIELD_WIDTH-1);
  }

  if (sql_field->sql_type == FIELD_TYPE_BIT)
  {
    sql_field->pack_flag= FIELDFLAG_NUMBER |
                          FIELDFLAG_TREAT_BIT_AS_CHAR;
  }
  sql_field->create_length_to_internal_length();
  DBUG_ASSERT(sql_field->def == 0);
  /* Can't go wrong as sql_field->def is not defined */
  (void) prepare_blob_field(thd, sql_field);
}


/*
  Create a table

  SYNOPSIS
    mysql_create_table()
    thd                  Thread object
    db                   Database
    table_name           Table name
    create_info [in/out] Create information (like MAX_ROWS)
    alter_info  [in/out] List of columns and indexes to create
    internal_tmp_table   Set to 1 if this is an internal temporary table
                         (From ALTER TABLE)

  DESCRIPTION
    If one creates a temporary table, this is automatically opened

    no_log is needed for the case of CREATE ... SELECT,
    as the logging will be done later in sql_insert.cc
    select_field_count is also used for CREATE ... SELECT,
    and must be zero for standard create of table.

    Note that structures passed as 'create_info' and 'alter_info' parameters
    may be modified by this function. It is responsibility of the caller to
    make a copy of create_info in order to provide correct execution in
    prepared statements/stored routines.

  RETURN VALUES
    FALSE OK
    TRUE  error
*/

bool mysql_create_table(THD *thd,const char *db, const char *table_name,
                        HA_CREATE_INFO *create_info,
                        Alter_info *alter_info,
                        bool internal_tmp_table,
                        uint select_field_count)
{
  char		path[FN_REFLEN];
  const char	*alias;
  uint		db_options, key_count;
  KEY		*key_info_buffer;
  handler	*file;
  bool		error= TRUE;
  DBUG_ENTER("mysql_create_table");

  /* Check for duplicate fields and check type of table to create */
  if (!alter_info->create_list.elements)
  {
    my_message(ER_TABLE_MUST_HAVE_COLUMNS, ER(ER_TABLE_MUST_HAVE_COLUMNS),
               MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (check_engine(thd, table_name, &create_info->db_type))
    DBUG_RETURN(TRUE);
  db_options= create_info->table_options;
  if (create_info->row_type == ROW_TYPE_DYNAMIC)
    db_options|=HA_OPTION_PACK_RECORD;
  alias= table_case_name(create_info, table_name);
  file= get_new_handler((TABLE*) 0, thd->mem_root, create_info->db_type);

#ifdef NOT_USED
  /*
    if there is a technical reason for a handler not to have support
    for temp. tables this code can be re-enabled.
    Otherwise, if a handler author has a wish to prohibit usage of
    temporary tables for his handler he should implement a check in
    ::create() method
  */
  if ((create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      (file->table_flags() & HA_NO_TEMP_TABLES))
  {
    my_error(ER_ILLEGAL_HA, MYF(0), table_name);
    DBUG_RETURN(TRUE);
  }
#endif

  /*
    If the table character set was not given explicitely,
    let's fetch the database default character set and
    apply it to the table.
  */
  if (!create_info->default_table_charset)
  {
    HA_CREATE_INFO db_info;

    load_db_opt_by_name(thd, db, &db_info);

    create_info->default_table_charset= db_info.default_table_charset;
  }

  if (mysql_prepare_table(thd, create_info, alter_info, internal_tmp_table,
                          &db_options, file,
                          &key_info_buffer, &key_count,
                          select_field_count))
    DBUG_RETURN(TRUE);

      /* Check if table exists */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    my_snprintf(path, sizeof(path), "%s%s%lx_%lx_%x%s",
		mysql_tmpdir, tmp_file_prefix, current_pid, thd->thread_id,
		thd->tmp_table++, reg_ext);
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, path);
    create_info->table_options|=HA_CREATE_DELAY_KEY_WRITE;
  }
  else  
  {
	#ifdef FN_DEVCHAR
	  /* check if the table name contains FN_DEVCHAR when defined */
	  const char *start= alias;
	  while (*start != '\0')
	  {
		  if (*start == FN_DEVCHAR)
		  {
			  my_error(ER_WRONG_TABLE_NAME, MYF(0), alias);
			  DBUG_RETURN(TRUE);
		  }
		  start++;
	  }	  
	#endif
    build_table_path(path, sizeof(path), db, alias, reg_ext);
  }

  /* Check if table already exists */
  if ((create_info->options & HA_LEX_CREATE_TMP_TABLE)
      && find_temporary_table(thd,db,table_name))
  {
    if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
    {
      create_info->table_existed= 1;		// Mark that table existed
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                          alias);
      DBUG_RETURN(FALSE);
    }
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), alias);
    DBUG_RETURN(TRUE);
  }
  VOID(pthread_mutex_lock(&LOCK_open));
  if (!internal_tmp_table && !(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    if (!access(path,F_OK))
    {
      if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
        goto warn;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto end;
    }
  }

  /*
    Check that table with given name does not already
    exist in any storage engine. In such a case it should
    be discovered and the error ER_TABLE_EXISTS_ERROR be returned
    unless user specified CREATE TABLE IF EXISTS
    The LOCK_open mutex has been locked to make sure no
    one else is attempting to discover the table. Since
    it's not on disk as a frm file, no one could be using it!
  */
  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    bool create_if_not_exists =
      create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS;
    if (ha_table_exists_in_engine(thd, db, table_name))
    {
      DBUG_PRINT("info", ("Table with same name already existed in handler"));

      if (create_if_not_exists)
        goto warn;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto end;
    }
  }

  thd->proc_info="creating table";
  create_info->table_existed= 0;		// Mark that table is created

  if (thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE)
    create_info->data_file_name= create_info->index_file_name= 0;
  create_info->table_options=db_options;

  if (rea_create_table(thd, path, db, table_name,
                       create_info, alter_info->create_list,
                       key_count, key_info_buffer))
    goto end;
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    /* Open table and put in temporary table list */
    if (!(open_temporary_table(thd, path, db, table_name, 1)))
    {
      (void) rm_temporary_table(create_info->db_type, path);
      goto end;
    }
    thd->tmp_table_used= 1;
  }
  if (!internal_tmp_table && mysql_bin_log.is_open())
  {
    thd->clear_error();
    Query_log_event qinfo(thd, thd->query, thd->query_length, FALSE, FALSE);
    mysql_bin_log.write(&qinfo);
  }
  error= FALSE;

end:
  VOID(pthread_mutex_unlock(&LOCK_open));
  thd->proc_info="After create";
  DBUG_RETURN(error);

warn:
  error= FALSE;
  push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                      ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                      alias);
  create_info->table_existed= 1;		// Mark that table existed
  goto end;
}

/*
** Give the key name after the first field with an optional '_#' after
**/

static bool
check_if_keyname_exists(const char *name, KEY *start, KEY *end)
{
  for (KEY *key=start ; key != end ; key++)
    if (!my_strcasecmp(system_charset_info,name,key->name))
      return 1;
  return 0;
}


static char *
make_unique_key_name(const char *field_name,KEY *start,KEY *end)
{
  char buff[MAX_FIELD_NAME],*buff_end;

  if (!check_if_keyname_exists(field_name,start,end) &&
      my_strcasecmp(system_charset_info,field_name,primary_key_name))
    return (char*) field_name;			// Use fieldname
  buff_end=strmake(buff,field_name, sizeof(buff)-4);

  /*
    Only 3 chars + '\0' left, so need to limit to 2 digit
    This is ok as we can't have more than 100 keys anyway
  */
  for (uint i=2 ; i< 100; i++)
  {
    *buff_end= '_';
    int10_to_str(i, buff_end+1, 10);
    if (!check_if_keyname_exists(buff,start,end))
      return sql_strdup(buff);
  }
  return (char*) "not_specified";		// Should never happen
}


/****************************************************************************
** Alter a table definition
****************************************************************************/

bool
mysql_rename_table(enum db_type base,
		   const char *old_db,
		   const char *old_name,
		   const char *new_db,
		   const char *new_name)
{
  THD *thd= current_thd;
  char from[FN_REFLEN], to[FN_REFLEN], lc_from[FN_REFLEN], lc_to[FN_REFLEN];
  char *from_base= from, *to_base= to;
  char tmp_name[NAME_LEN+1];
  handler *file= (base == DB_TYPE_UNKNOWN ? 0 :
                  get_new_handler((TABLE*) 0, thd->mem_root, base));
  int error=0;
  DBUG_ENTER("mysql_rename_table");

  build_table_path(from, sizeof(from), old_db, old_name, "");
  build_table_path(to, sizeof(to), new_db, new_name, "");

  /*
    If lower_case_table_names == 2 (case-preserving but case-insensitive
    file system) and the storage is not HA_FILE_BASED, we need to provide
    a lowercase file name, but we leave the .frm in mixed case.
   */
  if (lower_case_table_names == 2 && file &&
      !(file->table_flags() & HA_FILE_BASED))
  {
    strmov(tmp_name, old_name);
    my_casedn_str(files_charset_info, tmp_name);
    build_table_path(lc_from, sizeof(lc_from), old_db, tmp_name, "");
    from_base= lc_from;

    strmov(tmp_name, new_name);
    my_casedn_str(files_charset_info, tmp_name);
    build_table_path(lc_to, sizeof(lc_to), new_db, tmp_name, "");
    to_base= lc_to;
  }

  if (!file || !(error=file->rename_table(from_base, to_base)))
  {
    if (rename_file_ext(from,to,reg_ext))
    {
      error=my_errno;
      /* Restore old file name */
      if (file)
        file->rename_table(to_base, from_base);
    }
  }
  delete file;
  if (error == HA_ERR_WRONG_COMMAND)
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "ALTER TABLE");
  else if (error)
    my_error(ER_ERROR_ON_RENAME, MYF(0), from, to, error);
  DBUG_RETURN(error != 0);
}


/*
  Force all other threads to stop using the table

  SYNOPSIS
    wait_while_table_is_used()
    thd			Thread handler
    table		Table to remove from cache
    function		HA_EXTRA_PREPARE_FOR_DELETE if table is to be deleted
			HA_EXTRA_FORCE_REOPEN if table is not be used
  NOTES
   When returning, the table will be unusable for other threads until
   the table is closed.

  PREREQUISITES
    Lock on LOCK_open
    Win32 clients must also have a WRITE LOCK on the table !
*/

static void wait_while_table_is_used(THD *thd,TABLE *table,
				     enum ha_extra_function function)
{
  DBUG_PRINT("enter",("table: %s", table->s->table_name));
  DBUG_ENTER("wait_while_table_is_used");
  safe_mutex_assert_owner(&LOCK_open);

  VOID(table->file->extra(function));
  /* Mark all tables that are in use as 'old' */
  mysql_lock_abort(thd, table);			// end threads waiting on lock

  /* Wait until all there are no other threads that has this table open */
  remove_table_from_cache(thd, table->s->db,
                          table->s->table_name, RTFC_WAIT_OTHER_THREAD_FLAG);
  DBUG_VOID_RETURN;
}

/*
  Close a cached table

  SYNOPSIS
    close_cached_table()
    thd			Thread handler
    table		Table to remove from cache

  NOTES
    Function ends by signaling threads waiting for the table to try to
    reopen the table.

  PREREQUISITES
    Lock on LOCK_open
    Win32 clients must also have a WRITE LOCK on the table !
*/

void close_cached_table(THD *thd, TABLE *table)
{
  DBUG_ENTER("close_cached_table");

  wait_while_table_is_used(thd, table, HA_EXTRA_PREPARE_FOR_DELETE);
  /* Close lock if this is not got with LOCK TABLES */
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;			// Start locked threads
  }
  /* Close all copies of 'table'.  This also frees all LOCK TABLES lock */
  thd->open_tables=unlink_open_table(thd,thd->open_tables,table);

  /* When lock on LOCK_open is freed other threads can continue */
  broadcast_refresh();
  DBUG_VOID_RETURN;
}

static int send_check_errmsg(THD *thd, TABLE_LIST* table,
			     const char* operator_name, const char* errmsg)

{
  Protocol *protocol= thd->protocol;
  protocol->prepare_for_resend();
  protocol->store(table->alias, system_charset_info);
  protocol->store((char*) operator_name, system_charset_info);
  protocol->store(STRING_WITH_LEN("error"), system_charset_info);
  protocol->store(errmsg, system_charset_info);
  thd->clear_error();
  if (protocol->write())
    return -1;
  return 1;
}


static int prepare_for_restore(THD* thd, TABLE_LIST* table,
			       HA_CHECK_OPT *check_opt)
{
  DBUG_ENTER("prepare_for_restore");

  if (table->table) // do not overwrite existing tables on restore
  {
    DBUG_RETURN(send_check_errmsg(thd, table, "restore",
				  "table exists, will not overwrite on restore"
				  ));
  }
  else
  {
    char* backup_dir= thd->lex->backup_dir;
    char src_path[FN_REFLEN], dst_path[FN_REFLEN];
    char* table_name= table->table_name;
    char* db= table->db;

    if (fn_format_relative_to_data_home(src_path, table_name, backup_dir,
					reg_ext))
      DBUG_RETURN(-1); // protect buffer overflow

    my_snprintf(dst_path, sizeof(dst_path), "%s%s/%s",
		mysql_real_data_home, db, table_name);

    if (lock_and_wait_for_table_name(thd,table))
      DBUG_RETURN(-1);

    if (my_copy(src_path,
		fn_format(dst_path, dst_path,"", reg_ext, 4),
		MYF(MY_WME)))
    {
      pthread_mutex_lock(&LOCK_open);
      unlock_table_name(thd, table);
      pthread_mutex_unlock(&LOCK_open);
      DBUG_RETURN(send_check_errmsg(thd, table, "restore",
				    "Failed copying .frm file"));
    }
    if (mysql_truncate(thd, table, 1))
    {
      pthread_mutex_lock(&LOCK_open);
      unlock_table_name(thd, table);
      pthread_mutex_unlock(&LOCK_open);
      DBUG_RETURN(send_check_errmsg(thd, table, "restore",
				    "Failed generating table from .frm file"));
    }
  }

  /*
    Now we should be able to open the partially restored table
    to finish the restore in the handler later on
  */
  pthread_mutex_lock(&LOCK_open);
  if (reopen_name_locked_table(thd, table))
  {
    unlock_table_name(thd, table);
    pthread_mutex_unlock(&LOCK_open);
    DBUG_RETURN(send_check_errmsg(thd, table, "restore",
                                  "Failed to open partially restored table"));
  }
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(0);
}


static int prepare_for_repair(THD* thd, TABLE_LIST *table_list,
			      HA_CHECK_OPT *check_opt)
{
  int error= 0;
  TABLE tmp_table, *table;
  DBUG_ENTER("prepare_for_repair");

  if (!(check_opt->sql_flags & TT_USEFRM))
    DBUG_RETURN(0);

  if (!(table= table_list->table))		/* if open_ltable failed */
  {
    char name[FN_REFLEN];
    build_table_path(name, sizeof(name), table_list->db,
                     table_list->table_name, "");
    if (openfrm(thd, name, "", 0, 0, 0, &tmp_table))
      DBUG_RETURN(0);				// Can't open frm file
    table= &tmp_table;
  }

  /*
    User gave us USE_FRM which means that the header in the index file is
    trashed.
    In this case we will try to fix the table the following way:
    - Rename the data file to a temporary name
    - Truncate the table
    - Replace the new data file with the old one
    - Run a normal repair using the new index file and the old data file
  */

  char from[FN_REFLEN],tmp[FN_REFLEN+32];
  const char **ext= table->file->bas_ext();
  MY_STAT stat_info;

  /*
    Check if this is a table type that stores index and data separately,
    like ISAM or MyISAM
  */
  if (!ext[0] || !ext[1])
    goto end;					// No data file

  strxmov(from, table->s->path, ext[1], NullS);	// Name of data file
  if (!my_stat(from, &stat_info, MYF(0)))
    goto end;				// Can't use USE_FRM flag

  my_snprintf(tmp, sizeof(tmp), "%s-%lx_%lx",
	      from, current_pid, thd->thread_id);

  /* If we could open the table, close it */
  if (table_list->table)
  {
    pthread_mutex_lock(&LOCK_open);
    close_cached_table(thd, table);
    pthread_mutex_unlock(&LOCK_open);
  }
  if (lock_and_wait_for_table_name(thd,table_list))
  {
    error= -1;
    goto end;
  }
  if (my_rename(from, tmp, MYF(MY_WME)))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
			     "Failed renaming data file");
    goto end;
  }
  if (mysql_truncate(thd, table_list, 1))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
			     "Failed generating table from .frm file");
    goto end;
  }
  if (my_rename(tmp, from, MYF(MY_WME)))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
			     "Failed restoring .MYD file");
    goto end;
  }

  /*
    Now we should be able to open the partially repaired table
    to finish the repair in the handler later on.
  */
  pthread_mutex_lock(&LOCK_open);
  if (reopen_name_locked_table(thd, table_list))
  {
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
                             "Failed to open partially repaired table");
    goto end;
  }
  pthread_mutex_unlock(&LOCK_open);

end:
  if (table == &tmp_table)
    closefrm(table);				// Free allocated memory
  DBUG_RETURN(error);
}



/*
  RETURN VALUES
    FALSE Message sent to net (admin operation went ok)
    TRUE  Message should be sent by caller 
          (admin operation or network communication failed)
*/
static bool mysql_admin_table(THD* thd, TABLE_LIST* tables,
                              HA_CHECK_OPT* check_opt,
                              const char *operator_name,
                              thr_lock_type lock_type,
                              bool open_for_modify,
                              bool no_warnings_for_error,
                              uint extra_open_options,
                              int (*prepare_func)(THD *, TABLE_LIST *,
                                                  HA_CHECK_OPT *),
                              int (handler::*operator_func)(THD *,
                                                            HA_CHECK_OPT *),
                              int (view_operator_func)(THD *, TABLE_LIST*))
{
  TABLE_LIST *table, *save_next_global, *save_next_local;
  SELECT_LEX *select= &thd->lex->select_lex;
  List<Item> field_list;
  Item *item;
  Protocol *protocol= thd->protocol;
  LEX *lex= thd->lex;
  int result_code;
  DBUG_ENTER("mysql_admin_table");

  field_list.push_back(item = new Item_empty_string("Table", NAME_LEN*2));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Op", 10));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Msg_type", 10));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Msg_text", 255));
  item->maybe_null = 1;
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  mysql_ha_flush(thd, tables, MYSQL_HA_CLOSE_FINAL, FALSE);
  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    char* db = table->db;
    bool fatal_error=0;

    strxmov(table_name, db, ".", table->table_name, NullS);
    thd->open_options|= extra_open_options;
    table->lock_type= lock_type;
    /* open only one table from local list of command */
    save_next_global= table->next_global;
    table->next_global= 0;
    save_next_local= table->next_local;
    table->next_local= 0;
    select->table_list.first= (byte*)table;
    /*
      Time zone tables and SP tables can be add to lex->query_tables list,
      so it have to be prepared.
      TODO: Investigate if we can put extra tables into argument instead of
      using lex->query_tables
    */
    lex->query_tables= table;
    lex->query_tables_last= &table->next_global;
    lex->query_tables_own_last= 0;
    thd->no_warnings_for_error= no_warnings_for_error;
    if (view_operator_func == NULL)
      table->required_type=FRMTYPE_TABLE;
    open_and_lock_tables(thd, table);
    thd->no_warnings_for_error= 0;
    table->next_global= save_next_global;
    table->next_local= save_next_local;
    thd->open_options&= ~extra_open_options;

    if (prepare_func)
    {
      switch ((*prepare_func)(thd, table, check_opt)) {
      case  1:           // error, message written to net
        close_thread_tables(thd);
        continue;
      case -1:           // error, message could be written to net
        goto err;
      default:           // should be 0 otherwise
        ;
      }
    }

    /*
      CHECK TABLE command is only command where VIEW allowed here and this
      command use only temporary teble method for VIEWs resolving => there
      can't be VIEW tree substitition of join view => if opening table
      succeed then table->table will have real TABLE pointer as value (in
      case of join view substitution table->table can be 0, but here it is
      impossible)
    */
    if (!table->table)
    {
      char buf[ERRMSGSIZE+ERRMSGSIZE+2];
      const char *err_msg;
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      if (!(err_msg=thd->net.last_error))
	err_msg=ER(ER_CHECK_NO_SUCH_TABLE);
      /* if it was a view will check md5 sum */
      if (table->view &&
          view_checksum(thd, table) == HA_ADMIN_WRONG_CHECKSUM)
      {
        strxmov(buf, err_msg, "; ", ER(ER_VIEW_CHECKSUM), NullS);
        err_msg= (const char *)buf;
      }
      protocol->store(err_msg, system_charset_info);
      lex->cleanup_after_one_table_open();
      thd->clear_error();
      /*
        View opening can be interrupted in the middle of process so some
        tables can be left opening
      */
      close_thread_tables(thd);
      lex->reset_query_tables_list(FALSE);
      if (protocol->write())
	goto err;
      continue;
    }

    if (table->view)
    {
      result_code= (*view_operator_func)(thd, table);
      goto send_result;
    }

    if ((table->table->db_stat & HA_READ_ONLY) && open_for_modify)
    {
      char buff[FN_REFLEN + MYSQL_ERRMSG_SIZE];
      uint length;
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      length= my_snprintf(buff, sizeof(buff), ER(ER_OPEN_AS_READONLY),
                          table_name);
      protocol->store(buff, length, system_charset_info);
      close_thread_tables(thd);
      table->table=0;				// For query cache
      if (protocol->write())
	goto err;
      continue;
    }

    /* Close all instances of the table to allow repair to rename files */
    if (lock_type == TL_WRITE && table->table->s->version)
    {
      pthread_mutex_lock(&LOCK_open);
      const char *old_message=thd->enter_cond(&COND_refresh, &LOCK_open,
					      "Waiting to get writelock");
      mysql_lock_abort(thd,table->table);
      remove_table_from_cache(thd, table->table->s->db,
                              table->table->s->table_name,
                              RTFC_WAIT_OTHER_THREAD_FLAG |
                              RTFC_CHECK_KILLED_FLAG);
      thd->exit_cond(old_message);
      if (thd->killed)
	goto err;
      /* Flush entries in the query cache involving this table. */
      query_cache_invalidate3(thd, table->table, 0);
      open_for_modify= 0;
    }

    if (table->table->s->crashed && operator_func == &handler::ha_check)
    {
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store(STRING_WITH_LEN("warning"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Table is marked as crashed"),
                      system_charset_info);
      if (protocol->write())
        goto err;
    }

    if (operator_func == &handler::ha_repair)
    {
      if ((table->table->file->check_old_types() == HA_ADMIN_NEEDS_ALTER) ||
          (table->table->file->ha_check_for_upgrade(check_opt) ==
           HA_ADMIN_NEEDS_ALTER))
      {
        my_bool save_no_send_ok= thd->net.no_send_ok;
        close_thread_tables(thd);
        tmp_disable_binlog(thd); // binlogging is done by caller if wanted
        thd->net.no_send_ok= TRUE;
        result_code= mysql_recreate_table(thd, table);
        thd->net.no_send_ok= save_no_send_ok;
        reenable_binlog(thd);
        goto send_result;
      }

    }

    result_code = (table->table->file->*operator_func)(thd, check_opt);

send_result:

    lex->cleanup_after_one_table_open();
    thd->clear_error();  // these errors shouldn't get client
    protocol->prepare_for_resend();
    protocol->store(table_name, system_charset_info);
    protocol->store(operator_name, system_charset_info);

send_result_message:

    DBUG_PRINT("info", ("result_code: %d", result_code));
    switch (result_code) {
    case HA_ADMIN_NOT_IMPLEMENTED:
      {
	char buf[ERRMSGSIZE+20];
	uint length=my_snprintf(buf, ERRMSGSIZE,
				ER(ER_CHECK_NOT_IMPLEMENTED), operator_name);
	protocol->store(STRING_WITH_LEN("note"), system_charset_info);
	protocol->store(buf, length, system_charset_info);
      }
      break;

    case HA_ADMIN_NOT_BASE_TABLE:
      {
        char buf[ERRMSGSIZE+20];
        uint length= my_snprintf(buf, ERRMSGSIZE,
                                 ER(ER_BAD_TABLE_ERROR), table_name);
        protocol->store(STRING_WITH_LEN("note"), system_charset_info);
        protocol->store(buf, length, system_charset_info);
      }
      break;

    case HA_ADMIN_OK:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("OK"), system_charset_info);
      break;

    case HA_ADMIN_FAILED:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Operation failed"),
                      system_charset_info);
      break;

    case HA_ADMIN_REJECT:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Operation need committed state"),
                      system_charset_info);
      open_for_modify= FALSE;
      break;

    case HA_ADMIN_ALREADY_DONE:
      protocol->store(STRING_WITH_LEN("status"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Table is already up to date"),
                      system_charset_info);
      break;

    case HA_ADMIN_CORRUPT:
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Corrupt"), system_charset_info);
      fatal_error=1;
      break;

    case HA_ADMIN_INVALID:
      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      protocol->store(STRING_WITH_LEN("Invalid argument"),
                      system_charset_info);
      break;

    case HA_ADMIN_TRY_ALTER:
    {
      my_bool save_no_send_ok= thd->net.no_send_ok;
      /*
        This is currently used only by InnoDB. ha_innobase::optimize() answers
        "try with alter", so here we close the table, do an ALTER TABLE,
        reopen the table and do ha_innobase::analyze() on it.
      */
      close_thread_tables(thd);
      TABLE_LIST *save_next_local= table->next_local,
                 *save_next_global= table->next_global;
      table->next_local= table->next_global= 0;
      tmp_disable_binlog(thd); // binlogging is done by caller if wanted
      thd->net.no_send_ok= TRUE;
      result_code= mysql_recreate_table(thd, table);
      thd->net.no_send_ok= save_no_send_ok;
      reenable_binlog(thd);
      close_thread_tables(thd);
      if (!result_code) // recreation went ok
      {
        if ((table->table= open_ltable(thd, table, lock_type)) &&
            ((result_code= table->table->file->analyze(thd, check_opt)) > 0))
          result_code= 0; // analyze went ok
      }
      if (result_code) // either mysql_recreate_table or analyze failed
      {
        const char *err_msg;
        if ((err_msg= thd->net.last_error))
        {
          if (!thd->vio_ok())
          {
            sql_print_error(err_msg);
          }
          else
          {
            /* Hijack the row already in-progress. */
            protocol->store(STRING_WITH_LEN("error"), system_charset_info);
            protocol->store(err_msg, system_charset_info);
            (void)protocol->write();
            /* Start off another row for HA_ADMIN_FAILED */
            protocol->prepare_for_resend();
            protocol->store(table_name, system_charset_info);
            protocol->store(operator_name, system_charset_info);
          }
        }
      }
      result_code= result_code ? HA_ADMIN_FAILED : HA_ADMIN_OK;
      table->next_local= save_next_local;
      table->next_global= save_next_global;
      goto send_result_message;
    }
    case HA_ADMIN_WRONG_CHECKSUM:
    {
      protocol->store(STRING_WITH_LEN("note"), system_charset_info);
      protocol->store(ER(ER_VIEW_CHECKSUM), strlen(ER(ER_VIEW_CHECKSUM)),
                      system_charset_info);
      break;
    }

    case HA_ADMIN_NEEDS_UPGRADE:
    case HA_ADMIN_NEEDS_ALTER:
    {
      char buf[ERRMSGSIZE];
      uint length;

      protocol->store(STRING_WITH_LEN("error"), system_charset_info);
      length=my_snprintf(buf, ERRMSGSIZE, ER(ER_TABLE_NEEDS_UPGRADE), table->table_name);
      protocol->store(buf, length, system_charset_info);
      fatal_error=1;
      break;
    }

    default:				// Probably HA_ADMIN_INTERNAL_ERROR
      {
        char buf[ERRMSGSIZE+20];
        uint length=my_snprintf(buf, ERRMSGSIZE,
                                "Unknown - internal error %d during operation",
                                result_code);
        protocol->store(STRING_WITH_LEN("error"), system_charset_info);
        protocol->store(buf, length, system_charset_info);
        fatal_error=1;
        break;
      }
    }
    if (table->table)
    {
      if (fatal_error)
        table->table->s->version=0;               // Force close of table
      else if (open_for_modify)
      {
        if (table->table->s->tmp_table)
          table->table->file->info(HA_STATUS_CONST);
        else
        {
          pthread_mutex_lock(&LOCK_open);
          remove_table_from_cache(thd, table->table->s->db,
                                  table->table->s->table_name, RTFC_NO_FLAG);
          pthread_mutex_unlock(&LOCK_open);
        }
        /* May be something modified consequently we have to invalidate cache */
        query_cache_invalidate3(thd, table->table, 0);
      }
    }
    close_thread_tables(thd);
    lex->reset_query_tables_list(FALSE);
    table->table=0;				// For query cache
    if (protocol->write())
      goto err;
  }

  send_eof(thd);
  DBUG_RETURN(FALSE);
 err:
  close_thread_tables(thd);			// Shouldn't be needed
  if (table)
    table->table=0;
  DBUG_RETURN(TRUE);
}


bool mysql_backup_table(THD* thd, TABLE_LIST* table_list)
{
  DBUG_ENTER("mysql_backup_table");
  DBUG_RETURN(mysql_admin_table(thd, table_list, 0,
				"backup", TL_READ, 0, 0, 0, 0,
				&handler::backup, 0));
}


bool mysql_restore_table(THD* thd, TABLE_LIST* table_list)
{
  DBUG_ENTER("mysql_restore_table");
  DBUG_RETURN(mysql_admin_table(thd, table_list, 0,
				"restore", TL_WRITE, 1, 1, 0,
				&prepare_for_restore,
				&handler::restore, 0));
}


bool mysql_repair_table(THD* thd, TABLE_LIST* tables, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("mysql_repair_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"repair", TL_WRITE, 1,
                                test(check_opt->sql_flags & TT_USEFRM),
                                HA_OPEN_FOR_REPAIR,
				&prepare_for_repair,
				&handler::ha_repair, 0));
}


bool mysql_optimize_table(THD* thd, TABLE_LIST* tables, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("mysql_optimize_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"optimize", TL_WRITE, 1,0,0,0,
				&handler::optimize, 0));
}


/*
  Assigned specified indexes for a table into key cache

  SYNOPSIS
    mysql_assign_to_keycache()
    thd		Thread object
    tables	Table list (one table only)

  RETURN VALUES
   FALSE ok
   TRUE  error
*/

bool mysql_assign_to_keycache(THD* thd, TABLE_LIST* tables,
			     LEX_STRING *key_cache_name)
{
  HA_CHECK_OPT check_opt;
  KEY_CACHE *key_cache;
  DBUG_ENTER("mysql_assign_to_keycache");

  check_opt.init();
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (!(key_cache= get_key_cache(key_cache_name)))
  {
    pthread_mutex_unlock(&LOCK_global_system_variables);
    my_error(ER_UNKNOWN_KEY_CACHE, MYF(0), key_cache_name->str);
    DBUG_RETURN(TRUE);
  }
  pthread_mutex_unlock(&LOCK_global_system_variables);
  check_opt.key_cache= key_cache;
  DBUG_RETURN(mysql_admin_table(thd, tables, &check_opt,
				"assign_to_keycache", TL_READ_NO_INSERT, 0, 0,
				0, 0, &handler::assign_to_keycache, 0));
}


/*
  Reassign all tables assigned to a key cache to another key cache

  SYNOPSIS
    reassign_keycache_tables()
    thd		Thread object
    src_cache	Reference to the key cache to clean up
    dest_cache	New key cache

  NOTES
    This is called when one sets a key cache size to zero, in which
    case we have to move the tables associated to this key cache to
    the "default" one.

    One has to ensure that one never calls this function while
    some other thread is changing the key cache. This is assured by
    the caller setting src_cache->in_init before calling this function.

    We don't delete the old key cache as there may still be pointers pointing
    to it for a while after this function returns.

 RETURN VALUES
    0	  ok
*/

int reassign_keycache_tables(THD *thd, KEY_CACHE *src_cache,
			     KEY_CACHE *dst_cache)
{
  DBUG_ENTER("reassign_keycache_tables");

  DBUG_ASSERT(src_cache != dst_cache);
  DBUG_ASSERT(src_cache->in_init);
  src_cache->param_buff_size= 0;		// Free key cache
  ha_resize_key_cache(src_cache);
  ha_change_key_cache(src_cache, dst_cache);
  DBUG_RETURN(0);
}


/*
  Preload specified indexes for a table into key cache

  SYNOPSIS
    mysql_preload_keys()
    thd		Thread object
    tables	Table list (one table only)

  RETURN VALUES
    FALSE ok
    TRUE  error
*/

bool mysql_preload_keys(THD* thd, TABLE_LIST* tables)
{
  DBUG_ENTER("mysql_preload_keys");
  DBUG_RETURN(mysql_admin_table(thd, tables, 0,
				"preload_keys", TL_READ, 0, 0, 0, 0,
				&handler::preload_keys, 0));
}


/*
  Create a table identical to the specified table

  SYNOPSIS
    mysql_create_like_table()
    thd		Thread object
    table	Table list (one table only)
    create_info Create info
    table_ident Src table_ident

  RETURN VALUES
    FALSE OK
    TRUE  error
*/

bool mysql_create_like_table(THD* thd, TABLE_LIST* table,
                             HA_CREATE_INFO *create_info,
                             Table_ident *table_ident)
{
  TABLE **tmp_table;
  char src_path[FN_REFLEN], dst_path[FN_REFLEN];
  char *db= table->db;
  char *table_name= table->table_name;
  char *src_db;
  char *src_table= table_ident->table.str;
  int  err;
  bool res= TRUE;
  db_type not_used;

  TABLE_LIST src_tables_list;
  DBUG_ENTER("mysql_create_like_table");
  DBUG_ASSERT(table_ident->db.str); /* Must be set in the parser */
  src_db= table_ident->db.str;

  /*
    Validate the source table
  */
  if (table_ident->table.length > NAME_LEN ||
      (table_ident->table.length &&
       check_table_name(src_table,table_ident->table.length)))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), src_table);
    DBUG_RETURN(TRUE);
  }
  if (!src_db || check_db_name(src_db))
  {
    my_error(ER_WRONG_DB_NAME, MYF(0), src_db ? src_db : "NULL");
    DBUG_RETURN(-1);
  }

  bzero((gptr)&src_tables_list, sizeof(src_tables_list));
  src_tables_list.db= src_db;
  src_tables_list.table_name= src_table;

  if (lock_and_wait_for_table_name(thd, &src_tables_list))
    goto err;

  if ((tmp_table= find_temporary_table(thd, src_db, src_table)))
    strxmov(src_path, (*tmp_table)->s->path, reg_ext, NullS);
  else
  {
    strxmov(src_path, mysql_data_home, "/", src_db, "/", src_table,
	    reg_ext, NullS);
    /* Resolve symlinks (for windows) */
    fn_format(src_path, src_path, "", "", MYF(MY_UNPACK_FILENAME));
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, src_path);
    if (access(src_path, F_OK))
    {
      my_error(ER_BAD_TABLE_ERROR, MYF(0), src_table);
      goto err;
    }
  }

  /* 
     create like should be not allowed for Views, Triggers, ... 
  */
  if (mysql_frm_type(thd, src_path, &not_used) != FRMTYPE_TABLE)
  {
    my_error(ER_WRONG_OBJECT, MYF(0), src_db, src_table, "BASE TABLE");
    goto err;
  }

  /*
    Validate the destination table

    skip the destination table name checking as this is already
    validated.
  */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (find_temporary_table(thd, db, table_name))
      goto table_exists;
    my_snprintf(dst_path, sizeof(dst_path), "%s%s%lx_%lx_%x%s",
		mysql_tmpdir, tmp_file_prefix, current_pid,
		thd->thread_id, thd->tmp_table++, reg_ext);
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, dst_path);
    create_info->table_options|= HA_CREATE_DELAY_KEY_WRITE;
  }
  else
  {
    strxmov(dst_path, mysql_data_home, "/", db, "/", table_name,
	    reg_ext, NullS);
    fn_format(dst_path, dst_path, "", "", MYF(MY_UNPACK_FILENAME));
    if (!access(dst_path, F_OK))
      goto table_exists;
  }

  /*
    Create a new table by copying from source table
  */
  if (my_copy(src_path, dst_path, MYF(MY_DONT_OVERWRITE_FILE)))
  {
    if (my_errno == ENOENT)
      my_error(ER_BAD_DB_ERROR,MYF(0),db);
    else
      my_error(ER_CANT_CREATE_FILE,MYF(0),dst_path,my_errno);
    goto err;
  }

  /*
    As mysql_truncate don't work on a new table at this stage of
    creation, instead create the table directly (for both normal
    and temporary tables).
  */
  *fn_ext(dst_path)= 0;
  err= ha_create_table(dst_path, create_info, 1);

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (err || !open_temporary_table(thd, dst_path, db, table_name, 1))
    {
      (void) rm_temporary_table(create_info->db_type,
				dst_path); /* purecov: inspected */
      goto err;     /* purecov: inspected */
    }
  }
  else if (err)
  {
    (void) quick_rm_table(create_info->db_type, db,
			  table_name); /* purecov: inspected */
    goto err;	    /* purecov: inspected */
  }

  // Must be written before unlock
  if (mysql_bin_log.is_open())
  {
    thd->clear_error();
    Query_log_event qinfo(thd, thd->query, thd->query_length, FALSE, FALSE);
    mysql_bin_log.write(&qinfo);
  }
  res= FALSE;
  goto err;

table_exists:
  if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
  {
    char warn_buff[MYSQL_ERRMSG_SIZE];
    my_snprintf(warn_buff, sizeof(warn_buff),
		ER(ER_TABLE_EXISTS_ERROR), table_name);
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		 ER_TABLE_EXISTS_ERROR,warn_buff);
    res= FALSE;
  }
  else
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), table_name);

err:
  pthread_mutex_lock(&LOCK_open);
  unlock_table_name(thd, &src_tables_list);
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(res);
}


bool mysql_analyze_table(THD* thd, TABLE_LIST* tables, HA_CHECK_OPT* check_opt)
{
#ifdef OS2
  thr_lock_type lock_type = TL_WRITE;
#else
  thr_lock_type lock_type = TL_READ_NO_INSERT;
#endif

  DBUG_ENTER("mysql_analyze_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"analyze", lock_type, 1, 0, 0, 0,
				&handler::analyze, 0));
}


bool mysql_check_table(THD* thd, TABLE_LIST* tables,HA_CHECK_OPT* check_opt)
{
#ifdef OS2
  thr_lock_type lock_type = TL_WRITE;
#else
  thr_lock_type lock_type = TL_READ_NO_INSERT;
#endif

  DBUG_ENTER("mysql_check_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"check", lock_type,
				0, HA_OPEN_FOR_REPAIR, 0, 0,
				&handler::ha_check, &view_checksum));
}


/* table_list should contain just one table */
static int
mysql_discard_or_import_tablespace(THD *thd,
                                   TABLE_LIST *table_list,
                                   enum tablespace_op_type tablespace_op)
{
  TABLE *table;
  my_bool discard;
  int error;
  DBUG_ENTER("mysql_discard_or_import_tablespace");

  /*
    Note that DISCARD/IMPORT TABLESPACE always is the only operation in an
    ALTER TABLE
  */

  thd->proc_info="discard_or_import_tablespace";

  discard= test(tablespace_op == DISCARD_TABLESPACE);

 /*
   We set this flag so that ha_innobase::open and ::external_lock() do
   not complain when we lock the table
 */
  thd->tablespace_op= TRUE;
  if (!(table=open_ltable(thd,table_list,TL_WRITE)))
  {
    thd->tablespace_op=FALSE;
    DBUG_RETURN(-1);
  }

  error=table->file->discard_or_import_tablespace(discard);

  thd->proc_info="end";

  if (error)
    goto err;

  /*
    The 0 in the call below means 'not in a transaction', which means
    immediate invalidation; that is probably what we wish here
  */
  query_cache_invalidate3(thd, table_list, 0);

  /* The ALTER TABLE is always in its own transaction */
  error = ha_commit_stmt(thd);
  if (ha_commit(thd))
    error=1;
  if (error)
    goto err;
  if (mysql_bin_log.is_open())
  {
    Query_log_event qinfo(thd, thd->query, thd->query_length, FALSE, FALSE);
    mysql_bin_log.write(&qinfo);
  }
err:
  close_thread_tables(thd);
  thd->tablespace_op=FALSE;
  
  if (error == 0)
  {
    send_ok(thd);
    DBUG_RETURN(0);
  }

  table->file->print_error(error, MYF(0));
    
  DBUG_RETURN(-1);
}


/*
  Manages enabling/disabling of indexes for ALTER TABLE

  SYNOPSIS
    alter_table_manage_keys()
      table                  Target table
      indexes_were_disabled  Whether the indexes of the from table
                             were disabled
      keys_onoff             ENABLE | DISABLE | LEAVE_AS_IS

  RETURN VALUES
    FALSE  OK
    TRUE   Error
*/

static
bool alter_table_manage_keys(TABLE *table, int indexes_were_disabled,
                             enum enum_enable_or_disable keys_onoff)
{
  int error= 0;
  DBUG_ENTER("alter_table_manage_keys");
  DBUG_PRINT("enter", ("table=%p were_disabled=%d on_off=%d",
             table, indexes_were_disabled, keys_onoff));

  switch (keys_onoff) {
  case ENABLE:
    error= table->file->enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
    break;
  case LEAVE_AS_IS:
    if (!indexes_were_disabled)
      break;
    /* fall-through: disabled indexes */
  case DISABLE:
    error= table->file->disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
  }

  if (error == HA_ERR_WRONG_COMMAND)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                        ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA), table->s->table_name);
    error= 0;
  } else if (error)
    table->file->print_error(error, MYF(0));

  DBUG_RETURN(error);
}


/*
  Alter table


  NOTE
    The structures passed as 'create_info' and 'alter_info' parameters may
    be modified by this function. It is responsibility of the caller to make
    a copy of create_info in order to provide correct execution in prepared
    statements/stored routines.
*/

bool mysql_alter_table(THD *thd,char *new_db, char *new_name,
                       HA_CREATE_INFO *create_info,
                       TABLE_LIST *table_list,
                       Alter_info *alter_info,
                       uint order_num, ORDER *order, bool ignore)
{
  TABLE *table,*new_table=0;
  int error;
  char tmp_name[80],old_name[32],new_name_buff[FN_REFLEN];
  char new_alias_buff[FN_REFLEN], *table_name, *db, *new_alias, *alias;
  char index_file[FN_REFLEN], data_file[FN_REFLEN];
  ha_rows copied,deleted;
  ulonglong next_insert_id;
  uint db_create_options, used_fields;
  enum db_type old_db_type, new_db_type, table_type;
  bool need_copy_table;
  bool no_table_reopen= FALSE, varchar= FALSE;
  frm_type_enum frm_type;
  DBUG_ENTER("mysql_alter_table");

  thd->proc_info="init";
  table_name=table_list->table_name;
  alias= (lower_case_table_names == 2) ? table_list->alias : table_name;

  db=table_list->db;
  if (!new_db || !my_strcasecmp(table_alias_charset, new_db, db))
    new_db= db;
  used_fields=create_info->used_fields;
  
  mysql_ha_flush(thd, table_list, MYSQL_HA_CLOSE_FINAL, FALSE);

  /* DISCARD/IMPORT TABLESPACE is always alone in an ALTER TABLE */
  if (alter_info->tablespace_op != NO_TABLESPACE_OP)
    /* Conditionally writes to binlog. */
    DBUG_RETURN(mysql_discard_or_import_tablespace(thd,table_list,
						   alter_info->tablespace_op));
  sprintf(new_name_buff,"%s/%s/%s%s",mysql_data_home, db, table_name, reg_ext);
  unpack_filename(new_name_buff, new_name_buff);
  if (lower_case_table_names != 2)
    my_casedn_str(files_charset_info, new_name_buff);
  frm_type= mysql_frm_type(thd, new_name_buff, &table_type);
  /* Rename a view */
  if (frm_type == FRMTYPE_VIEW && !(alter_info->flags & ~ALTER_RENAME))
  {
    /*
      Avoid problems with a rename on a table that we have locked or
      if the user is trying to to do this in a transcation context
    */

    if (thd->locked_tables || thd->active_transaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                 ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      DBUG_RETURN(1);
    }

    if (wait_if_global_read_lock(thd,0,1))
      DBUG_RETURN(1);
    VOID(pthread_mutex_lock(&LOCK_open));
    if (lock_table_names(thd, table_list))
      goto view_err;
    
    error=0;
    if (!do_rename(thd, table_list, new_db, new_name, new_name, 1))
    {
      if (mysql_bin_log.is_open())
      {
        thd->clear_error();
        Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
        mysql_bin_log.write(&qinfo);
      }
      send_ok(thd);
    }

    unlock_table_names(thd, table_list, (TABLE_LIST*) 0);

view_err:
    pthread_mutex_unlock(&LOCK_open);
    start_waiting_global_read_lock(thd);
    DBUG_RETURN(error);
  }
  if (!(table=open_ltable(thd,table_list,TL_WRITE_ALLOW_READ)))
    DBUG_RETURN(TRUE);

  /* Check that we are not trying to rename to an existing table */
  if (new_name)
  {
    strmov(new_name_buff,new_name);
    strmov(new_alias= new_alias_buff, new_name);
    if (lower_case_table_names)
    {
      if (lower_case_table_names != 2)
      {
	my_casedn_str(files_charset_info, new_name_buff);
	new_alias= new_name;			// Create lower case table name
      }
      my_casedn_str(files_charset_info, new_name);
    }
    if (new_db == db &&
	!my_strcasecmp(table_alias_charset, new_name_buff, table_name))
    {
      /*
	Source and destination table names are equal: make later check
	easier.
      */
      new_alias= new_name= table_name;
    }
    else
    {
      if (table->s->tmp_table)
      {
	if (find_temporary_table(thd,new_db,new_name_buff))
	{
	  my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_name_buff);
	  DBUG_RETURN(TRUE);
	}
      }
      else
      {
	char dir_buff[FN_REFLEN];
	strxnmov(dir_buff, FN_REFLEN, mysql_real_data_home, new_db, NullS);
	if (!access(fn_format(new_name_buff,new_name_buff,dir_buff,reg_ext,0),
		    F_OK))
	{
	  /* Table will be closed in do_command() */
	  my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
	  DBUG_RETURN(TRUE);
	}
      }
    }
  }
  else
  {
    new_alias= (lower_case_table_names == 2) ? alias : table_name;
    new_name= table_name;
  }

  old_db_type= table->s->db_type;
  if (create_info->db_type == DB_TYPE_DEFAULT)
    create_info->db_type= old_db_type;
  if (check_engine(thd, new_name, &create_info->db_type))
    DBUG_RETURN(TRUE);
  new_db_type= create_info->db_type;
  if (create_info->row_type == ROW_TYPE_NOT_USED)
    create_info->row_type= table->s->row_type;

  DBUG_PRINT("info", ("old type: %d  new type: %d", old_db_type, new_db_type));
  if (ha_check_storage_engine_flag(old_db_type, HTON_ALTER_NOT_SUPPORTED) ||
      ha_check_storage_engine_flag(new_db_type, HTON_ALTER_NOT_SUPPORTED))
  {
    DBUG_PRINT("info", ("doesn't support alter"));
    my_error(ER_ILLEGAL_HA, MYF(0), table_name);
    DBUG_RETURN(TRUE);
  }
  
  thd->proc_info="setup";
  if (!(alter_info->flags & ~(ALTER_RENAME | ALTER_KEYS_ONOFF)) &&
      !table->s->tmp_table) // no need to touch frm
  {
    VOID(pthread_mutex_lock(&LOCK_open));

    switch (alter_info->keys_onoff) {
    case LEAVE_AS_IS:
      error= 0;
      break;
    case ENABLE:
      wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
      error= table->file->enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
      /* COND_refresh will be signaled in close_thread_tables() */
      break;
    case DISABLE:
      wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
      error=table->file->disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
      /* COND_refresh will be signaled in close_thread_tables() */
      break;
    }
    if (error == HA_ERR_WRONG_COMMAND)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
			  table->alias);
      error= 0;
    }

    if (!error && (new_name != table_name || new_db != db))
    {
      thd->proc_info="rename";
      /* Then do a 'simple' rename of the table */
      if (!access(new_name_buff,F_OK))
      {
	my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_name);
	error= -1;
      }
      else
      {
	*fn_ext(new_name)=0;
	close_cached_table(thd, table);
	if (mysql_rename_table(old_db_type,db,table_name,new_db,new_alias))
	  error= -1;
        else if (Table_triggers_list::change_table_name(thd, db, table_name,
                                                        new_db, new_alias))
        {
          VOID(mysql_rename_table(old_db_type, new_db, new_alias, db,
                                  table_name));
          error= -1;
        }
      }
    }

    if (error == HA_ERR_WRONG_COMMAND)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
			  table->alias);
      error= 0;
    }

    if (!error)
    {
      if (mysql_bin_log.is_open())
      {
	thd->clear_error();
	Query_log_event qinfo(thd, thd->query, thd->query_length, FALSE, FALSE);
	mysql_bin_log.write(&qinfo);
      }
      send_ok(thd);
    }
    else if (error > 0)
    {
      table->file->print_error(error, MYF(0));
      error= -1;
    }
    VOID(pthread_mutex_unlock(&LOCK_open));
    table_list->table= NULL;                    // For query cache
    query_cache_invalidate3(thd, table_list, 0);
    DBUG_RETURN(error);
  }

  /* Full alter table */

  /* Let new create options override the old ones */
  if (!(used_fields & HA_CREATE_USED_MIN_ROWS))
    create_info->min_rows= table->s->min_rows;
  if (!(used_fields & HA_CREATE_USED_MAX_ROWS))
    create_info->max_rows= table->s->max_rows;
  if (!(used_fields & HA_CREATE_USED_AVG_ROW_LENGTH))
    create_info->avg_row_length= table->s->avg_row_length;
  if (!(used_fields & HA_CREATE_USED_DEFAULT_CHARSET))
    create_info->default_table_charset= table->s->table_charset;

  restore_record(table, s->default_values);     // Empty record for DEFAULT
  List_iterator<Alter_drop> drop_it(alter_info->drop_list);
  List_iterator<create_field> def_it(alter_info->create_list);
  List_iterator<Alter_column> alter_it(alter_info->alter_list);
  Alter_info new_info;                   // Add new columns and indexes here
  create_field *def;

  /*
    First collect all fields from table which isn't in drop_list
  */

  Field **f_ptr,*field;
  for (f_ptr=table->field ; (field= *f_ptr) ; f_ptr++)
  {
    if (field->type() == MYSQL_TYPE_STRING)
      varchar= TRUE;
    /* Check if field should be dropped */
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++))
    {
      if (drop->type == Alter_drop::COLUMN &&
	  !my_strcasecmp(system_charset_info,field->field_name, drop->name))
      {
	/* Reset auto_increment value if it was dropped */
	if (MTYP_TYPENR(field->unireg_check) == Field::NEXT_NUMBER &&
	    !(used_fields & HA_CREATE_USED_AUTO))
	{
	  create_info->auto_increment_value=0;
	  create_info->used_fields|=HA_CREATE_USED_AUTO;
	}
	break;
      }
    }
    if (drop)
    {
      drop_it.remove();
      continue;
    }
    /* Check if field is changed */
    def_it.rewind();
    while ((def=def_it++))
    {
      if (def->change &&
	  !my_strcasecmp(system_charset_info,field->field_name, def->change))
	break;
    }
    if (def)
    {						// Field is changed
      def->field=field;
      if (!def->after)
      {
	new_info.create_list.push_back(def);
	def_it.remove();
      }
    }
    else
    {						// Use old field value
      new_info.create_list.push_back(def= new create_field(field, field));
      alter_it.rewind();			// Change default if ALTER
      Alter_column *alter;
      while ((alter=alter_it++))
      {
	if (!my_strcasecmp(system_charset_info,field->field_name, alter->name))
	  break;
      }
      if (alter)
      {
	if (def->sql_type == FIELD_TYPE_BLOB)
	{
	  my_error(ER_BLOB_CANT_HAVE_DEFAULT, MYF(0), def->change);
	  DBUG_RETURN(TRUE);
	}
	if ((def->def=alter->def))              // Use new default
          def->flags&= ~NO_DEFAULT_VALUE_FLAG;
        else
          def->flags|= NO_DEFAULT_VALUE_FLAG;
	alter_it.remove();
      }
    }
  }
  def_it.rewind();
  List_iterator<create_field> find_it(new_info.create_list);
  while ((def=def_it++))			// Add new columns
  {
    if (def->change && ! def->field)
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), def->change, table_name);
      DBUG_RETURN(TRUE);
    }
    if (!def->after)
      new_info.create_list.push_back(def);
    else if (def->after == first_keyword)
      new_info.create_list.push_front(def);
    else
    {
      create_field *find;
      find_it.rewind();
      while ((find=find_it++))			// Add new columns
      {
	if (!my_strcasecmp(system_charset_info,def->after, find->field_name))
	  break;
      }
      if (!find)
      {
	my_error(ER_BAD_FIELD_ERROR, MYF(0), def->after, table_name);
	DBUG_RETURN(TRUE);
      }
      find_it.after(def);			// Put element after this
    }
  }
  if (alter_info->alter_list.elements)
  {
    my_error(ER_BAD_FIELD_ERROR, MYF(0),
             alter_info->alter_list.head()->name, table_name);
    DBUG_RETURN(TRUE);
  }
  if (!new_info.create_list.elements)
  {
    my_message(ER_CANT_REMOVE_ALL_FIELDS, ER(ER_CANT_REMOVE_ALL_FIELDS),
               MYF(0));
    DBUG_RETURN(TRUE);
  }

  /*
    Collect all keys which isn't in drop list. Add only those
    for which some fields exists.
  */

  List_iterator<Key> key_it(alter_info->key_list);
  List_iterator<create_field> field_it(new_info.create_list);
  List<key_part_spec> key_parts;

  KEY *key_info=table->key_info;
  for (uint i=0 ; i < table->s->keys ; i++,key_info++)
  {
    char *key_name= key_info->name;
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++))
    {
      if (drop->type == Alter_drop::KEY &&
	  !my_strcasecmp(system_charset_info,key_name, drop->name))
	break;
    }
    if (drop)
    {
      drop_it.remove();
      continue;
    }

    KEY_PART_INFO *key_part= key_info->key_part;
    key_parts.empty();
    for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      if (!key_part->field)
	continue;				// Wrong field (from UNIREG)
      const char *key_part_name=key_part->field->field_name;
      create_field *cfield;
      field_it.rewind();
      while ((cfield=field_it++))
      {
	if (cfield->change)
	{
	  if (!my_strcasecmp(system_charset_info, key_part_name,
			     cfield->change))
	    break;
	}
	else if (!my_strcasecmp(system_charset_info,
				key_part_name, cfield->field_name))
	  break;
      }
      if (!cfield)
	continue;				// Field is removed
      uint key_part_length=key_part->length;
      if (cfield->field)			// Not new field
      {
        /*
          If the field can't have only a part used in a key according to its
          new type, or should not be used partially according to its
          previous type, or the field length is less than the key part
          length, unset the key part length.

          We also unset the key part length if it is the same as the
          old field's length, so the whole new field will be used.

          BLOBs may have cfield->length == 0, which is why we test it before
          checking whether cfield->length < key_part_length (in chars).
         */
        if (!Field::type_can_have_key_part(cfield->field->type()) ||
            !Field::type_can_have_key_part(cfield->sql_type) ||
            (cfield->field->field_length == key_part_length &&
             !f_is_blob(key_part->key_type)) ||
	    (cfield->length && (cfield->length < key_part_length /
                                key_part->field->charset()->mbmaxlen)))
	  key_part_length= 0;			// Use whole field
      }
      key_part_length /= key_part->field->charset()->mbmaxlen;
      key_parts.push_back(new key_part_spec(cfield->field_name,
					    key_part_length));
    }
    if (key_parts.elements)
    {
      Key *key;
      enum Key::Keytype key_type;

      if (key_info->flags & HA_SPATIAL)
        key_type= Key::SPATIAL;
      else if (key_info->flags & HA_NOSAME)
      {
        if (! my_strcasecmp(system_charset_info, key_name, primary_key_name))
          key_type= Key::PRIMARY;
        else
          key_type= Key::UNIQUE;
      }
      else if (key_info->flags & HA_FULLTEXT)
        key_type= Key::FULLTEXT;
      else
        key_type= Key::MULTIPLE;

      key= new Key(key_type, key_name,
                   key_info->algorithm,
                   test(key_info->flags & HA_GENERATED_KEY),
                   key_parts);
      new_info.key_list.push_back(key);
    }
  }
  {
    Key *key;
    while ((key=key_it++))			// Add new keys
    {
      if (key->type != Key::FOREIGN_KEY)
        new_info.key_list.push_back(key);
      if (key->name &&
	  !my_strcasecmp(system_charset_info,key->name,primary_key_name))
      {
	my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name);
	DBUG_RETURN(TRUE);
      }
    }
  }

  if (alter_info->drop_list.elements)
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0),
             alter_info->drop_list.head()->name);
    goto err;
  }
  if (alter_info->alter_list.elements)
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0),
             alter_info->alter_list.head()->name);
    goto err;
  }

  db_create_options= table->s->db_create_options & ~(HA_OPTION_PACK_RECORD);
  my_snprintf(tmp_name, sizeof(tmp_name), "%s-%lx_%lx", tmp_file_prefix,
	      current_pid, thd->thread_id);
  /* Safety fix for innodb */
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, tmp_name);
  if (new_db_type != old_db_type && !table->file->can_switch_engines()) {
    my_error(ER_ROW_IS_REFERENCED, MYF(0));
    goto err;
  }
  create_info->db_type=new_db_type;
  if (!create_info->comment.str)
  {
    create_info->comment.str= table->s->comment.str;
    create_info->comment.length= table->s->comment.length;
  }

  table->file->update_create_info(create_info);
  if ((create_info->table_options &
       (HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS)) ||
      (used_fields & HA_CREATE_USED_PACK_KEYS))
    db_create_options&= ~(HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS);
  if (create_info->table_options &
      (HA_OPTION_CHECKSUM | HA_OPTION_NO_CHECKSUM))
    db_create_options&= ~(HA_OPTION_CHECKSUM | HA_OPTION_NO_CHECKSUM);
  if (create_info->table_options &
      (HA_OPTION_DELAY_KEY_WRITE | HA_OPTION_NO_DELAY_KEY_WRITE))
    db_create_options&= ~(HA_OPTION_DELAY_KEY_WRITE |
			  HA_OPTION_NO_DELAY_KEY_WRITE);
  create_info->table_options|= db_create_options;

  if (table->s->tmp_table)
    create_info->options|=HA_LEX_CREATE_TMP_TABLE;

  /*
    better have a negative test here, instead of positive, like
    alter_info->flags & ALTER_ADD_COLUMN|ALTER_ADD_INDEX|...
    so that ALTER TABLE won't break when somebody will add new flag

    MySQL uses frm version to determine the type of the data fields and
    their layout. See Field_string::type() for details.
    Thus, if the table is too old we may have to rebuild the data to
    update the layout.

    There was a bug prior to mysql-4.0.25. Number of null fields was
    calculated incorrectly. As a result frm and data files gets out of
    sync after fast alter table. There is no way to determine by which
    mysql version (in 4.0 and 4.1 branches) table was created, thus we
    disable fast alter table for all tables created by mysql versions
    prior to 5.0 branch.
    See BUG#6236.
  */
  need_copy_table= (alter_info->flags &
                    ~(ALTER_CHANGE_COLUMN_DEFAULT|ALTER_OPTIONS) ||
                    (create_info->used_fields &
                     ~(HA_CREATE_USED_COMMENT|HA_CREATE_USED_PASSWORD)) ||
                    table->s->tmp_table ||
                    !table->s->mysql_version ||
                    (table->s->frm_version < FRM_VER_TRUE_VARCHAR && varchar));
  create_info->frm_only= !need_copy_table;

  /*
    Handling of symlinked tables:
    If no rename:
      Create new data file and index file on the same disk as the
      old data and index files.
      Copy data.
      Rename new data file over old data file and new index file over
      old index file.
      Symlinks are not changed.

   If rename:
      Create new data file and index file on the same disk as the
      old data and index files.  Create also symlinks to point at
      the new tables.
      Copy data.
      At end, rename temporary tables and symlinks to temporary table
      to final table name.
      Remove old table and old symlinks

    If rename is made to another database:
      Create new tables in new database.
      Copy data.
      Remove old table and symlinks.
  */

  if (!strcmp(db, new_db))		// Ignore symlink if db changed
  {
    if (create_info->index_file_name)
    {
      /* Fix index_file_name to have 'tmp_name' as basename */
      strmov(index_file, tmp_name);
      create_info->index_file_name=fn_same(index_file,
					   create_info->index_file_name,
					   1);
    }
    if (create_info->data_file_name)
    {
      /* Fix data_file_name to have 'tmp_name' as basename */
      strmov(data_file, tmp_name);
      create_info->data_file_name=fn_same(data_file,
					  create_info->data_file_name,
					  1);
    }
  }
  else
    create_info->data_file_name=create_info->index_file_name=0;

  /* We don't log the statement, it will be logged later. */
  {
    tmp_disable_binlog(thd);
    error= mysql_create_table(thd, new_db, tmp_name,
                              create_info, &new_info, 1, 0);
    reenable_binlog(thd);
    if (error)
      DBUG_RETURN(error);
  }
  if (need_copy_table)
  {
    if (table->s->tmp_table)
    {
      TABLE_LIST tbl;
      bzero((void*) &tbl, sizeof(tbl));
      tbl.db= new_db;
      tbl.table_name= tbl.alias= tmp_name;
      new_table= open_table(thd, &tbl, thd->mem_root, (bool*) 0,
                            MYSQL_LOCK_IGNORE_FLUSH);
    }
    else
    {
      char path[FN_REFLEN];
      my_snprintf(path, sizeof(path), "%s/%s/%s", mysql_data_home,
                  new_db, tmp_name);
      fn_format(path,path,"","",4);
      new_table=open_temporary_table(thd, path, new_db, tmp_name,0);
    }
    if (!new_table)
    {
      VOID(quick_rm_table(new_db_type,new_db,tmp_name));
      goto err;
    }
  }

  /* We don't want update TIMESTAMP fields during ALTER TABLE. */
  thd->count_cuted_fields= CHECK_FIELD_WARN;	// calc cuted fields
  thd->cuted_fields=0L;
  thd->proc_info="copy to tmp table";
  next_insert_id=thd->next_insert_id;		// Remember for logging
  copied=deleted=0;
  if (new_table && !new_table->s->is_view)
  {
    new_table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    new_table->next_number_field=new_table->found_next_number_field;
    error= copy_data_between_tables(table, new_table, new_info.create_list,
                                    ignore, order_num, order,
                                    &copied, &deleted, alter_info->keys_onoff);
  }
  else if (!new_table)
  {
    VOID(pthread_mutex_lock(&LOCK_open));
    wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
    table->file->external_lock(thd, F_WRLCK);
    alter_table_manage_keys(table, table->file->indexes_are_disabled(),
                            alter_info->keys_onoff);
    table->file->external_lock(thd, F_UNLCK);
    VOID(pthread_mutex_unlock(&LOCK_open));
  }

  thd->last_insert_id=next_insert_id;		// Needed for correct log
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;

  if (table->s->tmp_table)
  {
    /* We changed a temporary table */
    if (error)
    {
      /*
	The following function call will free the new_table pointer,
	in close_temporary_table(), so we can safely directly jump to err
      */
      close_temporary_table(thd,new_db,tmp_name);
      goto err;
    }
    /* Close lock if this is a transactional table */
    if (thd->lock)
    {
      mysql_unlock_tables(thd, thd->lock);
      thd->lock=0;
    }
    /* Remove link to old table and rename the new one */
    close_temporary_table(thd, table->s->db, table_name);
    /* Should pass the 'new_name' as we store table name in the cache */
    if (rename_temporary_table(thd, new_table, new_db, new_name))
    {						// Fatal error
      close_temporary_table(thd,new_db,tmp_name);
      my_free((gptr) new_table,MYF(0));
      goto err;
    }
    /* 
     Writing to the binlog does not need to be synchronized for temporary tables, 
     which are thread-specific. 
    */
    if (mysql_bin_log.is_open())
    {
      thd->clear_error();
      Query_log_event qinfo(thd, thd->query, thd->query_length, FALSE, FALSE);
      mysql_bin_log.write(&qinfo);
    }
    goto end_temporary;
  }

  if (new_table)
  {
    intern_close_table(new_table);              /* close temporary table */
    my_free((gptr) new_table,MYF(0));
  }
  VOID(pthread_mutex_lock(&LOCK_open));
  if (error)
  {
    VOID(quick_rm_table(new_db_type,new_db,tmp_name));
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }

  /*
    Data is copied.  Now we rename the old table to a temp name,
    rename the new one to the old name, remove all entries from the old table
    from the cache, free all locks, close the old table and remove it.
  */

  thd->proc_info="rename result table";
  my_snprintf(old_name, sizeof(old_name), "%s2-%lx-%lx", tmp_file_prefix,
	      current_pid, thd->thread_id);
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, old_name);
  if (new_name != table_name || new_db != db)
  {
    if (!access(new_name_buff,F_OK))
    {
      error=1;
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_name_buff);
      VOID(quick_rm_table(new_db_type,new_db,tmp_name));
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
  }

#if (!defined( __WIN__) && !defined( __EMX__) && !defined( OS2))
  if (table->file->has_transactions())
#endif
  {
    /*
      Win32 and InnoDB can't drop a table that is in use, so we must
      close the original table at before doing the rename
    */
    close_cached_table(thd, table);
    table=0;					// Marker that table is closed
    no_table_reopen= TRUE;
  }
#if (!defined( __WIN__) && !defined( __EMX__) && !defined( OS2))
  else
    table->file->extra(HA_EXTRA_FORCE_REOPEN);	// Don't use this file anymore
#endif


  error=0;
  if (!need_copy_table)
    new_db_type=old_db_type=DB_TYPE_UNKNOWN; // this type cannot happen in regular ALTER
  if (mysql_rename_table(old_db_type,db,table_name,db,old_name))
  {
    error=1;
    VOID(quick_rm_table(new_db_type,new_db,tmp_name));
  }
  else if (mysql_rename_table(new_db_type,new_db,tmp_name,new_db,
			      new_alias) ||
           (new_name != table_name || new_db != db) && // we also do rename
           Table_triggers_list::change_table_name(thd, db, table_name,
                                                  new_db, new_alias))
       
  {						// Try to get everything back
    error=1;
    VOID(quick_rm_table(new_db_type,new_db,new_alias));
    VOID(quick_rm_table(new_db_type,new_db,tmp_name));
    VOID(mysql_rename_table(old_db_type,db,old_name,db,alias));
  }
  if (error)
  {
    /*
      This shouldn't happen.  We solve this the safe way by
      closing the locked table.
    */
    if (table)
      close_cached_table(thd,table);
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }
  if (thd->lock || new_name != table_name || no_table_reopen)  // True if WIN32
  {
    /*
      Not table locking or alter table with rename
      free locks and remove old table
    */
    if (table)
      close_cached_table(thd,table);
    VOID(quick_rm_table(old_db_type,db,old_name));
  }
  else
  {
    /*
      Using LOCK TABLES without rename.
      This code is never executed on WIN32!
      Remove old renamed table, reopen table and get new locks
    */
    if (table)
    {
      VOID(table->file->extra(HA_EXTRA_FORCE_REOPEN)); // Use new file
      /* Mark in-use copies old */
      remove_table_from_cache(thd,db,table_name,RTFC_NO_FLAG);
      /* end threads waiting on lock */
      mysql_lock_abort(thd,table);
    }
    VOID(quick_rm_table(old_db_type,db,old_name));
    if (close_data_tables(thd,db,table_name) ||
	reopen_tables(thd,1,0))
    {						// This shouldn't happen
      if (table)
	close_cached_table(thd,table);		// Remove lock for table
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
  }
  /* The ALTER TABLE is always in its own transaction */
  error = ha_commit_stmt(thd);
  if (ha_commit(thd))
    error=1;
  if (error)
  {
    VOID(pthread_mutex_unlock(&LOCK_open));
    broadcast_refresh();
    goto err;
  }
  thd->proc_info="end";
  if (mysql_bin_log.is_open())
  {
    thd->clear_error();
    Query_log_event qinfo(thd, thd->query, thd->query_length, FALSE, FALSE);
    mysql_bin_log.write(&qinfo);
  }
  broadcast_refresh();
  VOID(pthread_mutex_unlock(&LOCK_open));
#ifdef HAVE_BERKELEY_DB
  if (old_db_type == DB_TYPE_BERKELEY_DB)
  {
    /*
      For the alter table to be properly flushed to the logs, we
      have to open the new table.  If not, we get a problem on server
      shutdown.
    */
    char path[FN_REFLEN];
    build_table_path(path, sizeof(path), new_db, table_name, "");
    table=open_temporary_table(thd, path, new_db, tmp_name,0);
    if (table)
    {
      intern_close_table(table);
      my_free((char*) table, MYF(0));
    }
    else
      sql_print_warning("Could not open BDB table %s.%s after rename\n",
                        new_db,table_name);
    (void) berkeley_flush_logs();
  }
#endif
  table_list->table=0;				// For query cache
  query_cache_invalidate3(thd, table_list, 0);

end_temporary:
  my_snprintf(tmp_name, sizeof(tmp_name), ER(ER_INSERT_INFO),
	      (ulong) (copied + deleted), (ulong) deleted,
	      (ulong) thd->cuted_fields);
  send_ok(thd, copied + deleted, 0L, tmp_name);
  thd->some_tables_deleted=0;
  DBUG_RETURN(FALSE);

err:
  DBUG_RETURN(TRUE);
}


static int
copy_data_between_tables(TABLE *from,TABLE *to,
			 List<create_field> &create,
                         bool ignore,
			 uint order_num, ORDER *order,
			 ha_rows *copied,
			 ha_rows *deleted,
                         enum enum_enable_or_disable keys_onoff)
{
  int error;
  Copy_field *copy,*copy_end;
  ulong found_count,delete_count;
  THD *thd= current_thd;
  uint length= 0;
  SORT_FIELD *sortorder;
  READ_RECORD info;
  TABLE_LIST   tables;
  List<Item>   fields;
  List<Item>   all_fields;
  ha_rows examined_rows;
  bool auto_increment_field_copied= 0;
  ulong save_sql_mode;
  DBUG_ENTER("copy_data_between_tables");

  /*
    Turn off recovery logging since rollback of an alter table is to
    delete the new table so there is no need to log the changes to it.
    
    This needs to be done before external_lock
  */
  error= ha_enable_transaction(thd, FALSE);
  if (error)
    DBUG_RETURN(-1);
  
  if (!(copy= new Copy_field[to->s->fields]))
    DBUG_RETURN(-1);				/* purecov: inspected */

  if (to->file->external_lock(thd, F_WRLCK))
    DBUG_RETURN(-1);

  /* We need external lock before we can disable/enable keys */
  alter_table_manage_keys(to, from->file->indexes_are_disabled(), keys_onoff);

  /* We can abort alter table for any table type */
  thd->no_trans_update= 0;
  thd->abort_on_warning= !ignore && test(thd->variables.sql_mode &
                                         (MODE_STRICT_TRANS_TABLES |
                                          MODE_STRICT_ALL_TABLES));

  from->file->info(HA_STATUS_VARIABLE);
  to->file->start_bulk_insert(from->file->records);

  save_sql_mode= thd->variables.sql_mode;

  List_iterator<create_field> it(create);
  create_field *def;
  copy_end=copy;
  for (Field **ptr=to->field ; *ptr ; ptr++)
  {
    def=it++;
    if (def->field)
    {
      if (*ptr == to->next_number_field)
      {
        auto_increment_field_copied= TRUE;
        /*
          If we are going to copy contents of one auto_increment column to
          another auto_increment column it is sensible to preserve zeroes.
          This condition also covers case when we are don't actually alter
          auto_increment column.
        */
        if (def->field == from->found_next_number_field)
          thd->variables.sql_mode|= MODE_NO_AUTO_VALUE_ON_ZERO;
      }
      (copy_end++)->set(*ptr,def->field,0);
    }

  }

  found_count=delete_count=0;

  if (order)
  {
    from->sort.io_cache=(IO_CACHE*) my_malloc(sizeof(IO_CACHE),
					      MYF(MY_FAE | MY_ZEROFILL));
    bzero((char*) &tables,sizeof(tables));
    tables.table= from;
    tables.alias= tables.table_name= (char*) from->s->table_name;
    tables.db=    (char*) from->s->db;
    error=1;

    if (thd->lex->select_lex.setup_ref_array(thd, order_num) ||
	setup_order(thd, thd->lex->select_lex.ref_pointer_array,
		    &tables, fields, all_fields, order) ||
	!(sortorder=make_unireg_sortorder(order, &length, NULL)) ||
	(from->sort.found_records = filesort(thd, from, sortorder, length,
					     (SQL_SELECT *) 0, HA_POS_ERROR,
					     &examined_rows)) ==
	HA_POS_ERROR)
      goto err;
  };

  /*
    Handler must be told explicitly to retrieve all columns, because
    this function does not set field->query_id in the columns to the
    current query id
  */
  from->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
  init_read_record(&info, thd, from, (SQL_SELECT *) 0, 1,1);
  if (ignore)
    to->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  thd->row_count= 0;
  restore_record(to, s->default_values);        // Create empty record
  while (!(error=info.read_record(&info)))
  {
    if (thd->killed)
    {
      thd->send_kill_message();
      error= 1;
      break;
    }
    thd->row_count++;
    if (to->next_number_field)
    {
      if (auto_increment_field_copied)
        to->auto_increment_field_not_null= TRUE;
      else
        to->next_number_field->reset();
    }
    
    for (Copy_field *copy_ptr=copy ; copy_ptr != copy_end ; copy_ptr++)
    {
      copy_ptr->do_copy(copy_ptr);
    }
    if ((error=to->file->write_row((byte*) to->record[0])))
    {
      if (!ignore ||
	  (error != HA_ERR_FOUND_DUPP_KEY &&
	   error != HA_ERR_FOUND_DUPP_UNIQUE))
      {
	to->file->print_error(error,MYF(0));
	break;
      }
      to->file->restore_auto_increment();
      delete_count++;
    }
    else
      found_count++;
  }
  end_read_record(&info);
  free_io_cache(from);
  delete [] copy;				// This is never 0

  if (to->file->end_bulk_insert() && error <= 0)
  {
    to->file->print_error(my_errno,MYF(0));
    error=1;
  }
  to->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  ha_enable_transaction(thd,TRUE);

  /*
    Ensure that the new table is saved properly to disk so that we
    can do a rename
  */
  if (ha_commit_stmt(thd))
    error=1;
  if (ha_commit(thd))
    error=1;

 err:
  thd->variables.sql_mode= save_sql_mode;
  thd->abort_on_warning= 0;
  free_io_cache(from);
  *copied= found_count;
  *deleted=delete_count;
  if (to->file->external_lock(thd,F_UNLCK))
    error=1;
  DBUG_RETURN(error > 0 ? -1 : 0);
}


/*
  Recreates tables by calling mysql_alter_table().

  SYNOPSIS
    mysql_recreate_table()
    thd			Thread handler
    tables		Tables to recreate

 RETURN
    Like mysql_alter_table().
*/
bool mysql_recreate_table(THD *thd, TABLE_LIST *table_list)
{
  HA_CREATE_INFO create_info;
  Alter_info alter_info;

  DBUG_ENTER("mysql_recreate_table");

  bzero((char*) &create_info, sizeof(create_info));
  create_info.db_type=DB_TYPE_DEFAULT;
  create_info.row_type=ROW_TYPE_NOT_USED;
  create_info.default_table_charset=default_charset_info;
  /* Force alter table to recreate table */
  alter_info.flags= ALTER_CHANGE_COLUMN;
  DBUG_RETURN(mysql_alter_table(thd, NullS, NullS, &create_info,
                                table_list, &alter_info,
                                0, (ORDER *) 0, 0));
}


bool mysql_checksum_table(THD *thd, TABLE_LIST *tables, HA_CHECK_OPT *check_opt)
{
  TABLE_LIST *table;
  List<Item> field_list;
  Item *item;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysql_checksum_table");

  field_list.push_back(item = new Item_empty_string("Table", NAME_LEN*2));
  item->maybe_null= 1;
  field_list.push_back(item=new Item_int("Checksum",(longlong) 1,21));
  item->maybe_null= 1;
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  for (table= tables; table; table= table->next_local)
  {
    char table_name[NAME_LEN*2+2];
    TABLE *t;

    strxmov(table_name, table->db ,".", table->table_name, NullS);

    t= table->table= open_ltable(thd, table, TL_READ);
    thd->clear_error();			// these errors shouldn't get client

    protocol->prepare_for_resend();
    protocol->store(table_name, system_charset_info);

    if (!t)
    {
      /* Table didn't exist */
      protocol->store_null();
      thd->clear_error();
    }
    else
    {
      if (t->file->table_flags() & HA_HAS_CHECKSUM &&
	  !(check_opt->flags & T_EXTEND))
	protocol->store((ulonglong)t->file->checksum());
      else if (!(t->file->table_flags() & HA_HAS_CHECKSUM) &&
	       (check_opt->flags & T_QUICK))
	protocol->store_null();
      else
      {
	/* calculating table's checksum */
	ha_checksum crc= 0;
        uchar null_mask=256 -  (1 << t->s->last_null_bit_pos);

	/* InnoDB must be told explicitly to retrieve all columns, because
	this function does not set field->query_id in the columns to the
	current query id */
	t->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);

	if (t->file->ha_rnd_init(1))
	  protocol->store_null();
	else
	{
	  for (;;)
	  {
	    ha_checksum row_crc= 0;
            int error= t->file->rnd_next(t->record[0]);
            if (unlikely(error))
            {
              if (error == HA_ERR_RECORD_DELETED)
                continue;
              break;
            }
	    if (t->s->null_bytes)
            {
              /* fix undefined null bits */
              t->record[0][t->s->null_bytes-1] |= null_mask;
              if (!(t->s->db_create_options & HA_OPTION_PACK_RECORD))
                t->record[0][0] |= 1;

	      row_crc= my_checksum(row_crc, t->record[0], t->s->null_bytes);
            }

	    for (uint i= 0; i < t->s->fields; i++ )
	    {
	      Field *f= t->field[i];
	      if ((f->type() == FIELD_TYPE_BLOB) ||
                  (f->type() == MYSQL_TYPE_VARCHAR))
	      {
		String tmp;
		f->val_str(&tmp);
		row_crc= my_checksum(row_crc, (byte*) tmp.ptr(), tmp.length());
	      }
	      else
		row_crc= my_checksum(row_crc, (byte*) f->ptr,
				     f->pack_length());
	    }

	    crc+= row_crc;
	  }
	  protocol->store((ulonglong)crc);
          t->file->ha_rnd_end();
	}
      }
      thd->clear_error();
      close_thread_tables(thd);
      table->table=0;				// For query cache
    }
    if (protocol->write())
      goto err;
  }

  send_eof(thd);
  DBUG_RETURN(FALSE);

 err:
  close_thread_tables(thd);			// Shouldn't be needed
  if (table)
    table->table=0;
  DBUG_RETURN(TRUE);
}

static bool check_engine(THD *thd, const char *table_name,
                         enum db_type *new_engine)
{
  enum db_type req_engine= *new_engine;
  bool no_substitution=
        test(thd->variables.sql_mode & MODE_NO_ENGINE_SUBSTITUTION);
  if ((*new_engine=
       ha_checktype(thd, req_engine, no_substitution, 1)) == DB_TYPE_UNKNOWN)
    return TRUE;

  if (req_engine != *new_engine)
  {
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                       ER_WARN_USING_OTHER_HANDLER,
                       ER(ER_WARN_USING_OTHER_HANDLER),
                       ha_get_storage_engine(*new_engine),
                       table_name);
  }
  return FALSE;
}
