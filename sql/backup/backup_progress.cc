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

extern bool check_if_table_exists(THD *thd, TABLE_LIST *table, bool *exists);

/**
   Open backup progress table.

   This method opens the online backup table specified. It uses the locking
   thread mechanism in be_thread.cc to open the table in a separate thread.

   @param char *         table_name  The name of the table to open.
   @param thr_lock_type  lock        The lock type -- TL_WRITE or TL_READ.

   @returns 0 = success
   @returns 1 = failed to open table

   @todo : Replace poling loop with signal.
  */
Locking_thread_st *open_backup_progress_table(const char *table_name,
                                              enum thr_lock_type lock)
{
  TABLE_LIST tables;                    // List of tables (1 in this case)
  Locking_thread_st *locking_thd;       // The locking thread

  DBUG_ENTER("open_backup_progress_table()");

  tables.init_one_table("mysql", table_name, lock);

  /*
    The locking thread will, via open_table(), fail if the table does not
    exist.
  */

  /*
    Create a new thread to open and lock the tables.
  */
  locking_thd= new Locking_thread_st();
  if (locking_thd == NULL)
    DBUG_RETURN(locking_thd);    
  locking_thd->tables_in_backup= &tables;

  /*
    Start the locking thread and wait until it is ready.
  */
  locking_thd->start_locking_thread();

  /*
    Poll the locking thread until ready.
  */
  while (locking_thd && (locking_thd->lock_state != LOCK_ACQUIRED) &&
         (locking_thd->lock_state != LOCK_ERROR))
    sleep(0);
  if (locking_thd->lock_state == LOCK_ERROR)
  {
    delete locking_thd;
    locking_thd= NULL;
  }
  DBUG_RETURN(locking_thd);
}

/**
   Find the row in the table that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed.

   @param TABLE *    table      The table to search.
   @param ulonglong  backup_id  The id of the row to locate.

   @returns 0 = success
   @returns 1 = failed to find row
  */
my_bool find_online_backup_row(TABLE *table, ulonglong backup_id)
{
  uchar key[MAX_KEY_LENGTH]; // key buffer for search

  DBUG_ENTER("find_online_backup_row()");

  /*
    Create key to find row. We have to use field->store() to be able to
    handle VARCHAR and CHAR fields.
    Assumption here is that the two first fields in the table are
    'db' and 'name' and the first key is the primary key over the
    same fields.
  */
  table->field[ET_FIELD_BACKUP_ID]->store(backup_id, TRUE);

  key_copy(key, table->record[0], table->key_info, table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0], 0, key, HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    DBUG_PRINT("info", ("Row not found"));
    DBUG_RETURN(TRUE);
  }

  DBUG_PRINT("info", ("Row found!"));
  DBUG_RETURN(FALSE);
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
   Update an integer field for the row in the table that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the field specified.

   @param ulonglong            backup_id   The id of the row to locate.
   @param char *               table_name  The name of the table to open.
   @param enum_ob_table_field  fld         Field to update.
   @param ulonglong            value       Value to set field to.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int update_online_backup_int_field(ulonglong backup_id, 
                                   const char *table_name,
                                   enum_ob_table_field fld, 
                                   ulonglong value)
{
  TABLE *table= NULL;                   // table to open
  TABLE_LIST tables;                    // List of tables (1 in this case)
  int ret= 0;                           // return value
  Locking_thread_st *locking_thd= NULL; // The locking thread

  DBUG_ENTER("update_int_field()");

  locking_thd= open_backup_progress_table(table_name, TL_WRITE);
  if (!locking_thd)
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(0);
  }

  table= locking_thd->tables_in_backup->table;
  table->use_all_columns();

  if (find_online_backup_row(table, backup_id))
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(1);
  }

  store_record(table, record[1]);

  /*
    Fill in the data.
  */
  table->field[fld]->store(value, TRUE);
  table->field[fld]->set_notnull();

  /*
    Update the row.
  */
  if ((ret= table->file->ha_update_row(table->record[1], table->record[0])))
    table->file->print_error(ret, MYF(0));

  locking_thd->kill_locking_thread();
  DBUG_RETURN(ret);
}

/**
   Update a datetime field for the row in the table that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the field specified.

   @param ulonglong            backup_id   The id of the row to locate.
   @param char *               table_name  The name of the table to open.
   @param enum_ob_table_field  fld         Field to update.
   @param my_time_t            value       Value to set field to.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int update_online_backup_datetime_field(ulonglong backup_id, 
                                        const char *table_name,
                                        enum_ob_table_field fld, 
                                        time_t value)
{
  TABLE *table= NULL;                   // table to open
  TABLE_LIST tables;                    // List of tables (1 in this case)
  int ret= 0;                           // return value
  Locking_thread_st *locking_thd= NULL; // The locking thread

  DBUG_ENTER("update_int_field()");

  locking_thd= open_backup_progress_table(table_name, TL_WRITE);
  if (!locking_thd)
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(0);
  }

  table= locking_thd->tables_in_backup->table;
  table->use_all_columns();

  if (find_online_backup_row(table, backup_id))
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(1);
  }

  store_record(table, record[1]);

  /*
    Fill in the data.
  */
  if (value)
  {
    MYSQL_TIME time;
    my_tz_OFFSET0->gmt_sec_to_TIME(&time, (my_time_t)value);

    table->field[fld]->set_notnull();
    table->field[fld]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
  }

  /*
    Update the row.
  */
  if ((ret= table->file->ha_update_row(table->record[1], table->record[0])))
    table->file->print_error(ret, MYF(0));

  locking_thd->kill_locking_thread();
  DBUG_RETURN(ret);
}

/**
   report_ob_init()

   This method inserts a new row in the online_backup table populating it
   with the initial values passed. It returns the backup_id of the new row.

   @param int                process_id   The process id of the operation 
   @param enum_backup_state  state        The current state of the operation
   @param enum_backup_op     operation    The current state of the operation
   @param int                error_num    The error number
   @param char *             user_comment The user's comment specified in the
                                          command (not implemented yet)
   @param char *             backup_file  The name of the target file
   @param char *             command      The actual command entered

   @returns long backup_id  The autoincrement value for the new row.
  */
ulonglong report_ob_init(int process_id,
                    enum_backup_state state,
                    enum_backup_op operation,
                    int error_num,
                    const char *user_comment,
                    const char *backup_file,
                    const char *command)
{
  ulonglong backup_id= 0;
  int ret= 0;                                  // return value
  TABLE *table= NULL;                          // table to open
  TABLE_LIST tables;                           // List of tables (1 in this case)
  char *host= current_thd->security_ctx->host; // host name
  char *user= current_thd->security_ctx->user; // user name
  Locking_thread_st *locking_thd= NULL;        // The locking thread

  DBUG_ENTER("report_ob_init()");

  locking_thd= open_backup_progress_table("online_backup", TL_WRITE);
  if (!locking_thd)
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(0);
  }

  table= locking_thd->tables_in_backup->table;
  table->use_all_columns();

  THD *t= table->in_use;
  table->in_use= current_thd;

  /*
    Get defaults for new record.
  */
  restore_record(table, s->default_values); 

  /*
    Fill in the data.
  */
  table->field[ET_FIELD_PROCESS_ID]->store(process_id, TRUE);
  table->field[ET_FIELD_PROCESS_ID]->set_notnull();
  table->field[ET_FIELD_BACKUP_STATE]->store(state, TRUE);
  table->field[ET_FIELD_BACKUP_STATE]->set_notnull();
  table->field[ET_FIELD_OPER]->store(operation, TRUE);
  table->field[ET_FIELD_OPER]->set_notnull();
  table->field[ET_FIELD_ERROR_NUM]->store(error_num, TRUE);
  table->field[ET_FIELD_ERROR_NUM]->set_notnull();

  if (host)
  {
    if(table->field[ET_FIELD_HOST_OR_SERVER]->store(host, 
       strlen(host), system_charset_info))
      goto end;
    table->field[ET_FIELD_HOST_OR_SERVER]->set_notnull();
  }

  if (user)
  {
    if (table->field[ET_FIELD_USERNAME]->store(user,
        strlen(user), system_charset_info))
      goto end;
    table->field[ET_FIELD_USERNAME]->set_notnull();
  }

  if (user_comment)
  {
    if (table->field[ET_FIELD_COMMENT]->store(user_comment,
        strlen(user_comment), system_charset_info))
      goto end;
    table->field[ET_FIELD_COMMENT]->set_notnull();
  }

  if (backup_file)
  {
    if (table->field[ET_FIELD_BACKUP_FILE]->store(backup_file, 
        strlen(backup_file), system_charset_info))
      goto end;
    table->field[ET_FIELD_BACKUP_FILE]->set_notnull();
  }

  if (command)
  {
    if (table->field[ET_FIELD_COMMAND]->store(command,
        strlen(command), system_charset_info))
      goto end;
    table->field[ET_FIELD_COMMAND]->set_notnull();
  }
  table->in_use= t;

  table->next_number_field=table->found_next_number_field;

  /*
    Write the row.
  */
  if ((ret= table->file->ha_write_row(table->record[0])))
    table->file->print_error(ret, MYF(0));

  /*
    Get last insert id for row.
  */
  backup_id= table->file->insert_id_for_cur_row;
  table->file->ha_release_auto_increment();

end:

  locking_thd->kill_locking_thread();
  /*
    Record progress update.
  */
  String str;
  get_state_string(state, &str);
  report_ob_progress(backup_id, "backup kernel", 0, 
                     0, 0, 0, 0, str.c_ptr());
  DBUG_RETURN(backup_id);
}

/**
   Update the binlog information for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the binlog values.

   @param ulonglong            backup_id    The id of the row to locate.
   @param int                  backup_pos   The id of the row to locate.
   @param char *               binlog_file  The filename of the binlog.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int report_ob_binlog_info(ulonglong backup_id,
                          int binlog_pos,
                          const char *binlog_file)
{
  TABLE *table= NULL;                   // table to open
  TABLE_LIST tables;                    // List of tables (1 in this case)
  int ret= 0;                           // return value
  Locking_thread_st *locking_thd= NULL; // The locking thread

  DBUG_ENTER("report_ob_binlog_info()");

  locking_thd= open_backup_progress_table("online_backup", TL_WRITE);
  if (!locking_thd)
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(0);
  }

  table= locking_thd->tables_in_backup->table;
  table->use_all_columns();

  if (find_online_backup_row(table, backup_id))
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(1);
  }

  store_record(table, record[1]);

  /*
    Fill in the data.
  */
  table->field[ET_FIELD_BINLOG_POS]->store(binlog_pos, TRUE);
  table->field[ET_FIELD_BINLOG_POS]->set_notnull();

  THD *t= table->in_use;
  table->in_use= current_thd;

  if (binlog_file)
  {
    if(table->field[ET_FIELD_BINLOG_FILE]->store(binlog_file, 
       strlen(binlog_file), system_charset_info))
      goto end;
    table->field[ET_FIELD_BINLOG_FILE]->set_notnull();
  }
  table->in_use= t;

  /*
    Update the row.
  */
  if ((ret= table->file->ha_update_row(table->record[1], table->record[0])))
    table->file->print_error(ret, MYF(0));

end:

  locking_thd->kill_locking_thread();
  DBUG_RETURN(ret);
}

/**
   Update the error number for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the error number value.

   @param ulonglong  backup_id  The id of the row to locate.
   @param int        error_num  New error number.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int report_ob_error(ulonglong backup_id,
                    int error_num)
{
  int ret= 0;  // return value

  DBUG_ENTER("report_ob_error()");
  update_online_backup_int_field(backup_id, "online_backup", 
                                 ET_FIELD_ERROR_NUM, error_num);
  DBUG_RETURN(ret);
}

/**
   Update the number of objects for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the number of objects value.

   @param ulonglong  backup_id   The id of the row to locate.
   @param int        num_objects  New error number.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int report_ob_num_objects(ulonglong backup_id,
                          int num_objects)
{
  int ret= 0;  // return value

  DBUG_ENTER("report_ob_num_objects()");
  update_online_backup_int_field(backup_id, "online_backup",
                                 ET_FIELD_NUM_OBJ, num_objects);
  DBUG_RETURN(ret);
}

/**
   Update the size for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the size value.

   @param ulonglong  backup_id  The id of the row to locate.
   @param int        size       New size value.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int report_ob_size(ulonglong backup_id,
                   longlong size)
{
  int ret= 0;  // return value

  DBUG_ENTER("report_ob_size()");
  update_online_backup_int_field(backup_id, "online_backup",
                                 ET_FIELD_TOTAL_BYTES, size);
  DBUG_RETURN(ret);
}

/**
   Update the start/stop time for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the start/stop values.

   @param ulonglong  backup_id  The id of the row to locate.
   @param my_time_t  start      Start datetime.
   @param my_time_t  stop       Stop datetime.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int report_ob_time(ulonglong backup_id,
                   time_t start,
                   time_t stop)
{
  int ret= 0;  // return value

  DBUG_ENTER("report_ob_time()");
  if (start)
    update_online_backup_datetime_field(backup_id, "online_backup",
                                        ET_FIELD_START_TIME, start);
  if (stop)
    update_online_backup_datetime_field(backup_id, "online_backup",
                                        ET_FIELD_STOP_TIME, stop);
  DBUG_RETURN(ret);
}

/**
   Update the validity point time for the row that matches the backup_id.

   This method updates the validity point time for the backup operation
   identified by backup_id.

   @param ulonglong  backup_id  The id of the row to locate.
   @param my_time_t  vp_time    Validity point datetime.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int report_ob_vp_time(ulonglong backup_id,
                      time_t vp_time)
{
  int ret= 0;  // return value

  DBUG_ENTER("report_ob_vp_time()");
  if (vp_time)
    update_online_backup_datetime_field(backup_id, "online_backup",
                                        ET_FIELD_VP, vp_time);
  DBUG_RETURN(ret);
}

/**
   Update the engines string for the row that matches the backup_id.

   This method updates the engines information for the backup operation
   identified by backup_id. This method appends to the those listed in the
   table for the backup_id.

   @param ulonglong  backup_id    The id of the row to locate.
   @param char *     egnine_name  The name of the engine to add.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int report_ob_engines(ulonglong backup_id,
                      const char *engine_name)
{
  TABLE *table= NULL;                   // table to open
  TABLE_LIST tables;                    // List of tables (1 in this case)
  int ret= 0;                           // return value
  String str;                           // engines string
  Locking_thread_st *locking_thd= NULL; // The locking thread

  DBUG_ENTER("report_ob_engines()");

  locking_thd= open_backup_progress_table("online_backup", TL_WRITE);
  if (!locking_thd)
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(0);
  }

  table= locking_thd->tables_in_backup->table;
  table->use_all_columns();

  if (find_online_backup_row(table, backup_id))
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(1);
  }

  store_record(table, record[1]);

  /*
    Fill in the data.
  */
  THD *t= table->in_use;
  table->in_use= current_thd;

  str.length(0);
  table->field[ET_FIELD_ENGINES]->val_str(&str);
  if (str.length() > 0)
    str.append(", ");
  str.append(engine_name);
  if (str.length() > 0)
  {
    if(table->field[ET_FIELD_ENGINES]->store(str.c_ptr(), 
       str.length(), system_charset_info))
      goto end;
    table->field[ET_FIELD_ENGINES]->set_notnull();
  }
  table->in_use= t;

  /*
    Update the row.
  */
  if ((ret= table->file->ha_update_row(table->record[1], table->record[0])))
    table->file->print_error(ret, MYF(0));

end:

  locking_thd->kill_locking_thread();
  DBUG_RETURN(ret);
}

/**
   Update the state for the row that matches the backup_id.

   This method locates the row in the online backup table that matches the
   backup_id passed and updates the state value.

   @param ulonglong          backup_id  The id of the row to locate.
   @param enum_backup_state  state      New state value.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int report_ob_state(ulonglong backup_id,
                    enum_backup_state state)
{
  int ret= 0;  // return value
  String str;

  DBUG_ENTER("report_ob_state()");
  update_online_backup_int_field(backup_id, "online_backup", 
                                 ET_FIELD_BACKUP_STATE, state);
  /*
    Record progress update.
  */
  get_state_string(state, &str);
  report_ob_progress(backup_id, "backup kernel", 0, 
                     0, 0, 0, 0, str.c_ptr());

  DBUG_RETURN(ret);
}

/**
   Creates a new progress row for the row that matches the backup_id.

   This method inserts a new row in the online backup progress table using
   the values passed. This method is used to insert progress information during
   the backup operation.

   @param ulonglong  backup_id  The id of the master table row.
   @param char *     object     The name of the object processed.
   @param my_time_t  start      Start datetime.
   @param my_time_t  stop       Stop datetime.
   @param longlong   size       Size value.
   @param longlong   progress   Progress (percent).
   @param int        error_num  Error number (should be 0 is success).
   @param char *     notes      Misc data from engine

   @returns 0 = success
   @returns 1 = failed to write row
  */
int report_ob_progress(ulonglong backup_id,
                       const char *object,
                       time_t start,
                       time_t stop,
                       longlong size,
                       longlong progress,
                       int error_num,
                       const char *notes)
{
  int ret= 0;                           // return value
  TABLE *table= NULL;                   // table to open
  TABLE_LIST tables;                    // List of tables (1 in this case)
  Locking_thread_st *locking_thd= NULL; // The locking thread

  DBUG_ENTER("report_ob_progress()");

  locking_thd= open_backup_progress_table("online_backup_progress", TL_WRITE);
  if (!locking_thd)
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(0);
  }

  table= locking_thd->tables_in_backup->table;
  table->use_all_columns();

  THD *t= table->in_use;
  table->in_use= current_thd;

  /*
    Get defaults for new record.
  */
  restore_record(table, s->default_values); 

  /*
    Fill in the data.
  */
  table->field[ET_FIELD_BACKUP_ID_FK]->store(backup_id, TRUE);
  table->field[ET_FIELD_BACKUP_ID_FK]->set_notnull();

  if (object)
  {
    if (table->field[ET_FIELD_PROG_OBJECT]->store(object,
        strlen(object), system_charset_info))
      goto end;
    table->field[ET_FIELD_PROG_OBJECT]->set_notnull();
  }

  if (notes)
  {
    if (table->field[ET_FIELD_PROG_NOTES]->store(notes,
        strlen(notes), system_charset_info))
      goto end;
    table->field[ET_FIELD_PROG_NOTES]->set_notnull();
  }
  table->in_use= t;

  if (start)
  {
    MYSQL_TIME time;
    my_tz_OFFSET0->gmt_sec_to_TIME(&time, (my_time_t)start);

    table->field[ET_FIELD_PROG_START_TIME]->set_notnull();
    table->field[ET_FIELD_PROG_START_TIME]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
  }

  if (stop)
  {
    MYSQL_TIME time;
    my_tz_OFFSET0->gmt_sec_to_TIME(&time, (my_time_t)stop);

    table->field[ET_FIELD_PROG_STOP_TIME]->set_notnull();
    table->field[ET_FIELD_PROG_STOP_TIME]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
  }

  table->field[ET_FIELD_PROG_SIZE]->store(size, TRUE);
  table->field[ET_FIELD_PROG_SIZE]->set_notnull();
  table->field[ET_FIELD_PROGRESS]->store(progress, TRUE);
  table->field[ET_FIELD_PROGRESS]->set_notnull();
  table->field[ET_FIELD_PROG_ERROR_NUM]->store(error_num, TRUE);
  table->field[ET_FIELD_PROG_ERROR_NUM]->set_notnull();

  /*
    Write the row.
  */
  if ((ret= table->file->ha_write_row(table->record[0])))
    table->file->print_error(ret, MYF(0));

end:

  locking_thd->kill_locking_thread();
  DBUG_RETURN(ret);
}

/**
   Sums the sizes for the row that matches the backup_id.

   This method sums the size entries from the online backup progress rows
   for the backup operation identified by backup_id.

   @param ulonglong          backup_id  The id of the row to locate.

   @returns  ulonglong  Total size of all backup progress rows
  */
ulonglong sum_progress_rows(ulonglong backup_id)
{
  int last_read_res;                    // result of last read
  TABLE *table= NULL;                   // table to open
  TABLE_LIST tables;                    // List of tables (1 in this case)
  ulonglong size= 0;                    // total size
  handler *hdl;                         // handler pointer
  Locking_thread_st *locking_thd= NULL; // The locking thread

  DBUG_ENTER("sum_progress_rows()");

  locking_thd= open_backup_progress_table("online_backup_progress", TL_READ);
  if (!locking_thd)
  {
    locking_thd->kill_locking_thread();
    DBUG_RETURN(0);
  }

  table= locking_thd->tables_in_backup->table;
  table->use_all_columns();

  hdl= table->file;
  last_read_res= hdl->ha_rnd_init(1);
  THD *t= table->in_use;
  table->in_use= current_thd;
  while (!hdl->rnd_next(table->record[0]))
    if ((table->field[ET_FIELD_PROGRESS]->val_int() == 100) &&
        (table->field[ET_FIELD_PROG_ERROR_NUM]->val_int() == 0) &&
        ((ulonglong)table->field[ET_FIELD_BACKUP_ID_FK]->val_int() == backup_id))
      size+= table->field[ET_FIELD_PROG_SIZE]->val_int();
  table->in_use= t;

  hdl->ha_rnd_end();

  locking_thd->kill_locking_thread();
  DBUG_RETURN(size);
}

/**
   Print summary for the row that matches the backup_id.

   This method prints the summary information for the backup operation
   identified by backup_id.

   @param ulonglong  backup_id  The id of the row to locate.

   @returns 0 = success
   @returns 1 = failed to find row
  */
int print_backup_summary(THD *thd, ulonglong backup_id)
{
  Protocol *protocol= thd->protocol;    // client comms
  List<Item> field_list;                // list of fields to send
  String     op_str;                    // operations string
  int ret= 0;                           // return value
  char buf[255];                        // buffer for summary information
  String str;

  DBUG_ENTER("print_backup_summary()");

  /*
    Send field list.
  */
  op_str.length(0);
  op_str.append("backup_id");
  field_list.push_back(new Item_empty_string(op_str.c_ptr(), op_str.length()));
  protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);

  /*
    Send field data.
  */
  protocol->prepare_for_resend();
  llstr(backup_id,buf);
  protocol->store(buf, system_charset_info);
  protocol->write();

  send_eof(thd);
  DBUG_RETURN(ret);
}

