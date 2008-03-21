/* Copyright (C) 2004-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

/**
   @file backup_progress.cc

   This file contains the methods to create and update rows in the 
   online backup progress tables.

   @todo Decide on what "size" means. Does it mean number of rows or number
         of byte?
  */

#include "backup_progress.h"
#include "be_thread.h"

/* BACKUP_HISTORY_LOG name */
LEX_STRING BACKUP_HISTORY_LOG_NAME= {C_STRING_WITH_LEN("online_backup")};

/* BACKUP_PROGRESS_LOG name */
LEX_STRING BACKUP_PROGRESS_LOG_NAME= {C_STRING_WITH_LEN("online_backup_progress")};

/**
   Check online backup progress tables.

   This method attempts to open the online backup progress tables. It returns
   an error if either table is not present or cannot be opened.

   @param[in] THD * The current thread.

   @returns Information whether backup progress tables can be used.

   @retval FALSE  success
   @retval TRUE  failed to open one of the tables
  */
my_bool check_ob_progress_tables(THD *thd)
{
  TABLE_LIST tables;
  my_bool ret= FALSE;

  DBUG_ENTER("check_ob_progress_tables");

  /* Check mysql.online_backup */
  tables.init_one_table("mysql", "online_backup", TL_READ);
  if (simple_open_n_lock_tables(thd, &tables))
  {
    ret= TRUE;
    sql_print_error(ER(ER_BACKUP_PROGRESS_TABLES));
    DBUG_RETURN(ret);
  }
  close_thread_tables(thd);

  /* Check mysql.online_backup_progress */
  tables.init_one_table("mysql", "online_backup_progress", TL_READ);
  if (simple_open_n_lock_tables(thd, &tables))
  {
    ret= TRUE;
    sql_print_error(ER(ER_BACKUP_PROGRESS_TABLES));
    DBUG_RETURN(ret);
  }
  close_thread_tables(thd);
  DBUG_RETURN(ret);
}

/**
   Get text string for state.

   @param enum_backup_state  state        The current state of the operation

   @returns char * a text string for state.
  */
void get_state_string(enum_backup_state state, String *str)
{
  DBUG_ENTER("get_state_string()");

  str->length(0);
  switch (state) {
  case BUP_COMPLETE:
    str->append("complete");
    break;
  case BUP_STARTING:
    str->append("starting");
    break;
  case BUP_VALIDITY_POINT:
    str->append("validity point");
    break;
  case BUP_RUNNING:
    str->append("running");
    break;
  case BUP_ERRORS:
    str->append("error");
    break;
  case BUP_CANCEL:
    str->append("cancel");
    break;
  default:
    str->append("unknown");
    break;
  }
  DBUG_VOID_RETURN;
}

/**
  Write the backup log entry for the backup history log to a table.

  This method creates a new row in the backup history log with the
  information provided.

  @param[IN]   thd          The current thread
  @param[OUT]  backup_id    The new row id for the backup history
  @param[IN]   process_id   The process id of the operation 
  @param[IN]   state        The current state of the operation
  @param[IN]   operation    The current operation (backup or restore)
  @param[IN]   error_num    The error number
  @param[IN]   user_comment The user's comment specified in the
                            command (not implemented yet)
  @param[IN]   backup_file  The name of the target file
  @param[IN]   command      The actual command entered

  @retval TRUE if error.

  @todo Add internal error handler to handle errors that occur on
        open. See  thd->push_internal_handler(&error_handler).
*/
bool backup_history_log_write(THD *thd, 
                              ulonglong *backup_id,
                              int process_id,
                              enum_backup_state state,
                              enum_backup_operation operation,
                              int error_num,
                              const char *user_comment,
                              const char *backup_file,
                              const char *command)
{
  TABLE_LIST table_list;
  TABLE *table= NULL;
  bool result= TRUE;
  bool need_close= FALSE;
  bool need_rnd_end= FALSE;
  Open_tables_state open_tables_backup;
  bool save_time_zone_used;
  char *host= current_thd->security_ctx->host; // host name
  char *user= current_thd->security_ctx->user; // user name

  save_time_zone_used= thd->time_zone_used;
  bzero(& table_list, sizeof(TABLE_LIST));
  table_list.alias= table_list.table_name= BACKUP_HISTORY_LOG_NAME.str;
  table_list.table_name_length= BACKUP_HISTORY_LOG_NAME.length;

  table_list.lock_type= TL_WRITE_CONCURRENT_INSERT;

  table_list.db= MYSQL_SCHEMA_NAME.str;
  table_list.db_length= MYSQL_SCHEMA_NAME.length;

  if (!(table= open_performance_schema_table(thd, & table_list,
                                             & open_tables_backup)))
    goto err;

  need_close= TRUE;

  if (table->file->extra(HA_EXTRA_MARK_AS_LOG_TABLE) ||
      table->file->ha_rnd_init(0))
    goto err;

  need_rnd_end= TRUE;

  /* Honor next number columns if present */
  table->next_number_field= table->found_next_number_field;

  /*
    Get defaults for new record.
  */
  restore_record(table, s->default_values); 

  /* check that all columns exist */
  if (table->s->fields < ET_OBH_FIELD_COUNT)
    goto err;

  /*
    Fill in the data.
  */
  table->field[ET_OBH_FIELD_PROCESS_ID]->store(process_id, TRUE);
  table->field[ET_OBH_FIELD_PROCESS_ID]->set_notnull();
  table->field[ET_OBH_FIELD_BACKUP_STATE]->store(state, TRUE);
  table->field[ET_OBH_FIELD_BACKUP_STATE]->set_notnull();
  table->field[ET_OBH_FIELD_OPER]->store(operation, TRUE);
  table->field[ET_OBH_FIELD_OPER]->set_notnull();
  table->field[ET_OBH_FIELD_ERROR_NUM]->store(error_num, TRUE);
  table->field[ET_OBH_FIELD_ERROR_NUM]->set_notnull();

  if (host)
  {
    if(table->field[ET_OBH_FIELD_HOST_OR_SERVER]->store(host, 
       strlen(host), system_charset_info))
      goto err;
    table->field[ET_OBH_FIELD_HOST_OR_SERVER]->set_notnull();
  }

  if (user)
  {
    if (table->field[ET_OBH_FIELD_USERNAME]->store(user,
        strlen(user), system_charset_info))
      goto err;
    table->field[ET_OBH_FIELD_USERNAME]->set_notnull();
  }

  if (user_comment)
  {
    if (table->field[ET_OBH_FIELD_COMMENT]->store(user_comment,
        strlen(user_comment), system_charset_info))
      goto err;
    table->field[ET_OBH_FIELD_COMMENT]->set_notnull();
  }

  if (backup_file)
  {
    if (table->field[ET_OBH_FIELD_BACKUP_FILE]->store(backup_file, 
        strlen(backup_file), system_charset_info))
      goto err;
    table->field[ET_OBH_FIELD_BACKUP_FILE]->set_notnull();
  }

  if (command)
  {
    if (table->field[ET_OBH_FIELD_COMMAND]->store(command,
        strlen(command), system_charset_info))
      goto err;
    table->field[ET_OBH_FIELD_COMMAND]->set_notnull();
  }

  /* log table entries are not replicated */
  if (table->file->ha_write_row(table->record[0]))
    goto err;

  /*
    Get last insert id for row.
  */
  *backup_id= table->file->insert_id_for_cur_row;

  result= FALSE;

err:
  if (result && !thd->killed)
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_BACKUP_LOG_WRITE_ERROR,
                        ER(ER_BACKUP_LOG_WRITE_ERROR),
                        "mysql.online_backup");

  if (need_rnd_end)
  {
    table->file->ha_rnd_end();
    table->file->ha_release_auto_increment();
  }
  if (need_close)
    close_performance_schema_table(thd, & open_tables_backup);

  thd->time_zone_used= save_time_zone_used;
  return result;
}

/**
  Update a backup history log entry for the given backup_id to a table.

  This method updates a row in the backup history log using one
  of four data types as determined by the field (see fld).

  @param[IN]   thd          The current thread
  @param[IN]   backup_id    The row id for the backup history to be updated
  @param[IN]   fld          The enum for the field to be updated 
  @param[IN]   val_long     The value for long fields
  @param[IN]   val_time     The value for time fields
  @param[IN]   val_str      The value for char * fields
  @param[IN]   val_state    The value for state fields

  @retval TRUE if error.

  @todo Add internal error handler to handle errors that occur on
        open. See  thd->push_internal_handler(&error_handler).
*/
bool backup_history_log_update(THD *thd, 
                               ulonglong backup_id,
                               enum_backup_history_table_field fld,
                               ulonglong val_long,
                               time_t val_time,
                               const char *val_str,
                               int val_state)
{
  TABLE_LIST table_list;
  TABLE *table= NULL;
  bool result= TRUE;
  bool need_close= FALSE;
  Open_tables_state open_tables_backup;
  bool save_time_zone_used;
  int ret= 0;

  save_time_zone_used= thd->time_zone_used;

  bzero(& table_list, sizeof(TABLE_LIST));
  table_list.alias= table_list.table_name= BACKUP_HISTORY_LOG_NAME.str;
  table_list.table_name_length= BACKUP_HISTORY_LOG_NAME.length;

  table_list.lock_type= TL_WRITE_CONCURRENT_INSERT;

  table_list.db= MYSQL_SCHEMA_NAME.str;
  table_list.db_length= MYSQL_SCHEMA_NAME.length;

  if (!(table= open_performance_schema_table(thd, & table_list,
                                             & open_tables_backup)))
    goto err;

  if (find_backup_history_row(table, backup_id))
    goto err;

  need_close= TRUE;

  store_record(table, record[1]);

  /*
    Fill in the data.
  */
  switch (fld) {
    case ET_OBH_FIELD_BINLOG_POS:
    case ET_OBH_FIELD_ERROR_NUM:
    case ET_OBH_FIELD_NUM_OBJ:
    case ET_OBH_FIELD_TOTAL_BYTES:
    {
      table->field[fld]->store(val_long, TRUE);
      table->field[fld]->set_notnull();
      break;
    }
    case ET_OBH_FIELD_BINLOG_FILE:
    {
      if (val_str)
      {
        if(table->field[fld]->store(val_str, strlen(val_str), 
                                    system_charset_info))
          goto err;
        table->field[fld]->set_notnull();
      }
      break;
    }    
    case ET_OBH_FIELD_ENGINES:
    {
      String str;    // engines string
      str.length(0);
      table->field[fld]->val_str(&str);
      if (str.length() > 0)
        str.append(", ");
      str.append(val_str);
      if (str.length() > 0)
      {
        if(table->field[fld]->store(str.c_ptr(), 
           str.length(), system_charset_info))
          goto err;
        table->field[fld]->set_notnull();
      }
      break;
    }    
    case ET_OBH_FIELD_START_TIME:
    case ET_OBH_FIELD_STOP_TIME:
    case ET_OBH_FIELD_VP:
    {
      if (val_time)
      {
        MYSQL_TIME time;
        my_tz_OFFSET0->gmt_sec_to_TIME(&time, (my_time_t)val_time);

        table->field[fld]->set_notnull();
        table->field[fld]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
      }
      break;
    }
    case ET_OBH_FIELD_BACKUP_STATE:
    {
      table->field[fld]->store(val_state, TRUE);
      table->field[fld]->set_notnull();
      break;
    }
    default:
      goto err;
  }

  /*
    Update the row.
  */
  if ((ret= table->file->ha_update_row(table->record[1], table->record[0])))
    goto err;

  result= FALSE;

err:
  if (result && !thd->killed)
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_BACKUP_LOG_WRITE_ERROR,
                        ER(ER_BACKUP_LOG_WRITE_ERROR),
                        "mysql.online_backup");

  if (need_close)
    close_performance_schema_table(thd, & open_tables_backup);

  thd->time_zone_used= save_time_zone_used;
  return result;
}

/**
  Write the backup log entry for the backup progress log to a table.

  This method creates a new row in the backup progress log with the
  information provided.

  @param[IN]   thd         The current thread
  @param[OUT]  backup_id   The id of the backup/restore operation for
                           the progress information
  @param[IN]   object      The name of the object processed
  @param[IN]   start       Start datetime
  @param[IN]   stop        Stop datetime
  @param[IN]   size        Size value
  @param[IN]   progress    Progress (percent)
  @param[IN]   error_num   Error number (should be 0 if success)
  @param[IN]   notes       Misc data from engine

  @retval TRUE if error.

  @todo Add internal error handler to handle errors that occur on
        open. See  thd->push_internal_handler(&error_handler).
*/
bool backup_progress_log_write(THD *thd,
                               ulonglong backup_id,
                               const char *object,
                               time_t start,
                               time_t stop,
                               longlong size,
                               longlong progress,
                               int error_num,
                               const char *notes)
{
  TABLE_LIST table_list;
  TABLE *table;
  bool result= TRUE;
  bool need_close= FALSE;
  bool need_rnd_end= FALSE;
  Open_tables_state open_tables_backup;
  bool save_time_zone_used;

  save_time_zone_used= thd->time_zone_used;

  bzero(& table_list, sizeof(TABLE_LIST));
  table_list.alias= table_list.table_name= BACKUP_PROGRESS_LOG_NAME.str;
  table_list.table_name_length= BACKUP_PROGRESS_LOG_NAME.length;

  table_list.lock_type= TL_WRITE_CONCURRENT_INSERT;

  table_list.db= MYSQL_SCHEMA_NAME.str;
  table_list.db_length= MYSQL_SCHEMA_NAME.length;

  if (!(table= open_performance_schema_table(thd, & table_list,
                                             & open_tables_backup)))
    goto err;

  need_close= TRUE;

  if (table->file->extra(HA_EXTRA_MARK_AS_LOG_TABLE) ||
      table->file->ha_rnd_init(0))
    goto err;

  need_rnd_end= TRUE;

  /* Honor next number columns if present */
  table->next_number_field= table->found_next_number_field;

  /*
    Get defaults for new record.
  */
  restore_record(table, s->default_values); 

  /* check that all columns exist */
  if (table->s->fields < ET_OBP_FIELD_PROG_COUNT)
    goto err;

  /*
    Fill in the data.
  */
  table->field[ET_OBP_FIELD_BACKUP_ID_FK]->store(backup_id, TRUE);
  table->field[ET_OBP_FIELD_BACKUP_ID_FK]->set_notnull();

  if (object)
  {
    if (table->field[ET_OBP_FIELD_PROG_OBJECT]->store(object,
        strlen(object), system_charset_info))
      goto err;
    table->field[ET_OBP_FIELD_PROG_OBJECT]->set_notnull();
  }

  if (notes)
  {
    if (table->field[ET_OBP_FIELD_PROG_NOTES]->store(notes,
        strlen(notes), system_charset_info))
      goto err;
    table->field[ET_OBP_FIELD_PROG_NOTES]->set_notnull();
  }

  if (start)
  {
    MYSQL_TIME time;
    my_tz_OFFSET0->gmt_sec_to_TIME(&time, (my_time_t)start);

    table->field[ET_OBP_FIELD_PROG_START_TIME]->set_notnull();
    table->field[ET_OBP_FIELD_PROG_START_TIME]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
  }

  if (stop)
  {
    MYSQL_TIME time;
    my_tz_OFFSET0->gmt_sec_to_TIME(&time, (my_time_t)stop);

    table->field[ET_OBP_FIELD_PROG_STOP_TIME]->set_notnull();
    table->field[ET_OBP_FIELD_PROG_STOP_TIME]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
  }

  table->field[ET_OBP_FIELD_PROG_SIZE]->store(size, TRUE);
  table->field[ET_OBP_FIELD_PROG_SIZE]->set_notnull();
  table->field[ET_OBP_FIELD_PROGRESS]->store(progress, TRUE);
  table->field[ET_OBP_FIELD_PROGRESS]->set_notnull();
  table->field[ET_OBP_FIELD_PROG_ERROR_NUM]->store(error_num, TRUE);
  table->field[ET_OBP_FIELD_PROG_ERROR_NUM]->set_notnull();

  /* log table entries are not replicated */
  if (table->file->ha_write_row(table->record[0]))
    goto err;

  result= FALSE;

err:
  if (result && !thd->killed)
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_BACKUP_LOG_WRITE_ERROR,
                        ER(ER_BACKUP_LOG_WRITE_ERROR),
                        "mysql.online_backup_progress");

  if (need_rnd_end)
  {
    table->file->ha_rnd_end();
    table->file->ha_release_auto_increment();
  }
  if (need_close)
    close_performance_schema_table(thd, & open_tables_backup);

  thd->time_zone_used= save_time_zone_used;
  return result;
}

/**
   Find the row in the table that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed.

   @param TABLE      table      The open table.
   @param ulonglong  backup_id  The id of the row to locate.

   @retval 0  success
   @retval 1  failed to find row
  */
bool find_backup_history_row(TABLE *table, ulonglong backup_id)
{
  uchar key[MAX_KEY_LENGTH]; // key buffer for search
  /*
    Create key to find row. We have to use field->store() to be able to
    handle different field types (method is overloaded).
  */
  table->field[ET_OBH_FIELD_BACKUP_ID]->store(backup_id, TRUE);

  key_copy(key, table->record[0], table->key_info, table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0], 0, key, HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
    return true;

  return false;
}

/**
   report_ob_init()

   This method inserts a new row in the online_backup table populating it
   with the initial values passed. It returns the backup_id of the new row.

   @param THD                thd          The current thread class.
   @param int                process_id   The process id of the operation 
   @param enum_backup_state  state        The current state of the operation
   @param enum_backup_op     operation    The current operation 
                                          (backup or restore)
   @param int                error_num    The error number
   @param char *             user_comment The user's comment specified in the
                                          command (not implemented yet)
   @param char *             backup_file  The name of the target file
   @param char *             command      The actual command entered

   @retval long backup_id  The autoincrement value for the new row.
  */
ulonglong report_ob_init(THD *thd, 
                         int process_id,
                         enum_backup_state state,
                         enum_backup_operation operation,
                         int error_num,
                         const char *user_comment,
                         const char *backup_file,
                         const char *command)
{ 
  ulonglong backup_id= 0;
  int ret= 0;                                  // return value
  DBUG_ENTER("report_ob_init()");

  ret= backup_history_log_write(thd, &backup_id, process_id, state, operation,
                                error_num, user_comment, backup_file, command);
  /*
    Record progress update.
  */
  String str;
  get_state_string(state, &str);
  report_ob_progress(thd, backup_id, "backup kernel", 0, 
                     0, 0, 0, 0, str.c_ptr());
  DBUG_RETURN(backup_id);
}

/**
   Update the binlog information for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the binlog values.

   @param THD                  thd          The current thread class.
   @param ulonglong            backup_id    The id of the row to locate.
   @param int                  backup_pos   The id of the row to locate.
   @param char *               binlog_file  The filename of the binlog.

   @retval 0  success
   @retval 1  failed to find row
  */
int report_ob_binlog_info(THD *thd,
                          ulonglong backup_id,
                          int binlog_pos,
                          const char *binlog_file)
{
  int ret= 0;                           // return value
  DBUG_ENTER("report_ob_binlog_info()");

  ret= backup_history_log_update(thd, backup_id, ET_OBH_FIELD_BINLOG_POS, 
                                 binlog_pos, 0, 0, 0);
  ret= backup_history_log_update(thd, backup_id, ET_OBH_FIELD_BINLOG_FILE, 
                                 0, 0, binlog_file, 0);
  DBUG_RETURN(ret);
}

/**
   Update the error number for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the error number value.

   @param THD        thd        The current thread class.
   @param ulonglong  backup_id  The id of the row to locate.
   @param int        error_num  New error number.

   @retval 0  success
   @retval 1  failed to find row
  */
int report_ob_error(THD *thd,
                    ulonglong backup_id,
                    int error_num)
{
  int ret= 0;  // return value
  DBUG_ENTER("report_ob_error()");

  ret= backup_history_log_update(thd, backup_id, ET_OBH_FIELD_ERROR_NUM, 
                                 error_num, 0, 0, 0);

  DBUG_RETURN(ret);
}

/**
   Update the number of objects for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the number of objects value.

   @param THD        thd         The current thread class.
   @param ulonglong  backup_id    The id of the row to locate.
   @param int        num_objects  New error number.

   @retval 0  success
   @retval 1  failed to find row
  */
int report_ob_num_objects(THD *thd,
                          ulonglong backup_id,
                          int num_objects)
{
  int ret= 0;  // return value
  DBUG_ENTER("report_ob_num_objects()");

  ret= backup_history_log_update(thd, backup_id, ET_OBH_FIELD_NUM_OBJ, 
                                 num_objects, 0, 0, 0);

  DBUG_RETURN(ret);
}

/**
   Update the size for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the size value.

   @param THD        thd        The current thread class.
   @param ulonglong  backup_id  The id of the row to locate.
   @param int        size       New size value.

   @retval 0  success
   @retval 1  failed to find row
  */
int report_ob_size(THD *thd,
                   ulonglong backup_id,
                   longlong size)
{
  int ret= 0;  // return value
  DBUG_ENTER("report_ob_size()");

  ret= backup_history_log_update(thd, backup_id, ET_OBH_FIELD_TOTAL_BYTES, 
                                 size, 0, 0, 0);

  DBUG_RETURN(ret);
}

/**
   Update the start/stop time for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the start/stop values.

   @param THD        thd        The current thread class.
   @param ulonglong  backup_id  The id of the row to locate.
   @param my_time_t  start      Start datetime.
   @param my_time_t  stop       Stop datetime.

   @retval 0  success
   @retval 1  failed to find row
  */
int report_ob_time(THD *thd,
                   ulonglong backup_id,
                   time_t start,
                   time_t stop)
{
  int ret= 0;  // return value
  DBUG_ENTER("report_ob_time()");

  if (start)
    ret= backup_history_log_update(thd, backup_id, ET_OBH_FIELD_START_TIME, 
                                   0, start, 0, 0);

  if (stop)
    ret= backup_history_log_update(thd, backup_id, ET_OBH_FIELD_STOP_TIME, 
                                   0, stop, 0, 0);

  DBUG_RETURN(ret);
}

/**
   Update the validity point time for the row that matches the backup_id.

   This method updates the validity point time for the backup operation
   identified by backup_id.

   @param THD        thd        The current thread class.
   @param ulonglong  backup_id  The id of the row to locate.
   @param my_time_t  vp_time    Validity point datetime.

   @retval 0  success
   @retval 1  failed to find row
  */
int report_ob_vp_time(THD *thd,
                      ulonglong backup_id,
                      time_t vp_time)
{
  int ret= 0;  // return value
  DBUG_ENTER("report_ob_vp_time()");

  if (vp_time)
    ret= backup_history_log_update(thd, backup_id, ET_OBH_FIELD_VP, 
                                   0, vp_time, 0, 0);

  DBUG_RETURN(ret);
}

/**
   Update the engines string for the row that matches the backup_id.

   This method updates the engines information for the backup operation
   identified by backup_id. This method appends to the those listed in the
   table for the backup_id.

   @param THD        thd          The current thread class.
   @param ulonglong  backup_id    The id of the row to locate.
   @param char *     egnine_name  The name of the engine to add.

   @retval 0  success
   @retval 1  failed to find row
  */
int report_ob_engines(THD *thd,
                      ulonglong backup_id,
                      const char *engine_name)
{
  int ret= 0;  // return value
  DBUG_ENTER("report_ob_engines()");

  ret= backup_history_log_update(thd, backup_id, ET_OBH_FIELD_ENGINES, 
                                 0, 0, engine_name, 0);

  DBUG_RETURN(ret);
}

/**
   Update the state for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the state value.

   @param THD                thd        The current thread class.
   @param ulonglong          backup_id  The id of the row to locate.
   @param enum_backup_state  state      New state value.

   @retval 0  success
   @retval 1  failed to find row
  */
int report_ob_state(THD *thd, 
                    ulonglong backup_id,
                    enum_backup_state state)
{
  int ret= 0;  // return value
  String str;
  DBUG_ENTER("report_ob_state()");

  ret= backup_history_log_update(thd, backup_id, ET_OBH_FIELD_BACKUP_STATE, 
                                 0, 0, 0, state);
  /*
    Record progress update.
  */
  get_state_string(state, &str);
  report_ob_progress(thd, backup_id, "backup kernel", 0, 
                     0, 0, 0, 0, str.c_ptr());

  DBUG_RETURN(ret);
}

/**
   Creates a new progress row for the row that matches the backup_id.

   This method inserts a new row in the online backup progress table using
   the values passed. This method is used to insert progress information during
   the backup operation.

   @param THD        thd        The current thread class.
   @param ulonglong  backup_id  The id of the master table row.
   @param char *     object     The name of the object processed.
   @param my_time_t  start      Start datetime.
   @param my_time_t  stop       Stop datetime.
   @param longlong   size       Size value.
   @param longlong   progress   Progress (percent).
   @param int        error_num  Error number (should be 0 is success).
   @param char *     notes      Misc data from engine

   @retval 0  success
   @retval 1  failed to find row
  */
inline int report_ob_progress(THD *thd,
                       ulonglong backup_id,
                       const char *object,
                       time_t start,
                       time_t stop,
                       longlong size,
                       longlong progress,
                       int error_num,
                       const char *notes)
{
  int ret= 0;                           // return value
  DBUG_ENTER("report_ob_progress()");

  ret= backup_progress_log_write(thd, backup_id, object, start, stop, 
                                 size, progress, error_num, notes);

  DBUG_RETURN(ret);
}



