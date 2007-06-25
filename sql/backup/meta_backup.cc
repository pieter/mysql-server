/**
  @file

  Functions used by kernel to save/restore meta-data
 */

/*
  TODO:

  - Handle events, routines, triggers and other item types (use Chuck's code).

  - When saving database info, save its default charset and collation.
    Alternatively, save a complete "CREATE DATABASE ..." statement with all
    clauses which should work even when new clauses are added in the future.

  - In silent_exec_query() - reset, collect and report errors from statement
    execution.

  - In the same function: investigate what happens if query triggers error -
    there were signals that the code might hang in that case (some clean-up
    needed?)
 */

#include <mysql_priv.h>
#include <sql_show.h>
/*
  Might be needed for other meta-data items

#include <event_scheduler.h>
#include <sp.h>
#include <sql_trigger.h>
#include <log.h>
*/

#include "backup_aux.h"
#include "backup_kernel.h"
#include "meta_backup.h"


namespace backup {

/**
  Write meta-data description to backup stream.

  Meta-data information is stored as a sequence of entries, each describing a
  single meta-data item. The format of a single entry depends on the type of
  item (see @c Archive_info::Item::save() method). The entries are saved in a
  single chunk.

  The order of entries is important. Items on which other items depend should
  be saved first in the sequence. This order is determined by the implementation
  of @c Backup_info::Item_iterator class.

  @note Meta-data description is added to the current chunk of the stream which
  is then closed.

  @returns 0 on success, error code otherwise.
 */
int write_meta_data(THD *thd, Backup_info &info, OStream &s)
{
  DBUG_ENTER("backup::write_meta_data");

  size_t start_bytes= s.bytes;

  for (Backup_info::Item_iterator it(info); it; it++)
  {
    result_t res= it->save(thd,s); // this calls Archive_info::Item::save() method

    if (res != OK)
    {
      meta::Item::description_buf buf;
      info.report_error(ER_BACKUP_WRITE_META,
                         it->meta().describe(buf,sizeof(buf)));
      DBUG_RETURN(-1);
    }
  }

  if (stream_result::ERROR == s.end_chunk())
    DBUG_RETURN(-2);

  info.meta_size= s.bytes - start_bytes;

  DBUG_RETURN(0);
}


/**
  Read meta-data items from a stream and create them if they are selected
  for restore.

  @pre  Stream is at the beginning of a saved meta-data chunk.
  @post Stream is at the beginning of the next chunk.
 */
int
restore_meta_data(THD *thd, Restore_info &info, IStream &s)
{
  DBUG_ENTER("restore_meta_data");
  Archive_info::Item *it; // save pointer to item read form the stream
  Db_ref  curr_db;        // remember the current database
  result_t res;

  size_t start_bytes= s.bytes;

  // read items from the stream until error or end of data is reached.
  while (OK == (res= Archive_info::Item::create_from_stream(info,s,it)))
  {
    DBUG_PRINT("restore",(" got next meta-item."));

    if (info.selected(*it)) // if the item was selected for restore ...
    {
     DBUG_PRINT("restore",("  creating it!"));

     /*
       change the current database if we are going to create a per-db item
       and we are not already in the correct one.
      */
     const Db_ref db= it->meta().in_db();

     if (db.is_valid() && (!curr_db.is_valid() || db != curr_db))
     {
       DBUG_PRINT("restore",("  changing current db to %s",db.name().ptr()));
       curr_db= db;
       change_db(thd,db);
     }

     if (OK != (res= it->meta().create(thd)))
     {
       meta::Item::description_buf buf;
       info.report_error(ER_BACKUP_CREATE_META,
                          it->meta().describe(buf,sizeof(buf)));
       DBUG_RETURN(-1);
     }

     delete it;
    }
  }

  // We should reach end of chunk now - if not something went wrong

  if (res != DONE)
  {
    info.report_error(ER_BACKUP_READ_META);
    DBUG_RETURN(-2);
  }

  DBUG_ASSERT(res == DONE);

  if (stream_result::ERROR == s.next_chunk())
  {
    info.report_error(ER_BACKUP_NEXT_CHUNK);
    DBUG_RETURN(-3);
  };

  info.meta_size= s.bytes - start_bytes;

  DBUG_RETURN(0);
}

} // backup namespace


/*********************************************

  Save/restore for different meta-data items.

 *********************************************/

namespace backup {

int silent_exec_query(THD*, String&);

/**
  Write data needed to restore an item.

  By default, a complete DDL CREATE statement for the item is saved.
  This statement is constructed using @c meta::X::build_create_stmt() method
  where meta::X is the class representing the item. The method stores statement
  in the @c create_stmt member.

  @returns OK or ERROR
 */

result_t
meta::Item::save(THD *thd, OStream &s)
{
  create_stmt.free();

  if (build_create_stmt(thd))
    return ERROR;

  return stream_result::OK == s.writestr(create_stmt) ? OK : ERROR;
}

/**
  Read data written by @c save().

  By default, the CREATE statement is read and stored in the @c create_stmt
  member.

  @retval OK    everything went ok
  @retval DONE  end of data chunk detected
  @retval ERROR error has happened
 */
result_t
meta::Item::read(IStream &s)
{
  stream_result::value res= s.readstr(create_stmt);

  // Saved string should not be NIL
  return res == stream_result::NIL ? ERROR : (result_t)res;
}

/**
  Create item.

  Default implementation executes the statement stored in the @c create_stmt
  member.

  @returns OK or ERROR
 */
result_t
meta::Item::create(THD *thd)
{
  if (create_stmt.is_empty())
  { return ERROR; }

  if (ERROR == drop(thd))
    return ERROR;

  return silent_exec_query(thd,create_stmt) ? ERROR : OK;
}

/**
  Destroy item if it exists.

  Default implementation executes SQL statement of the form:
  <pre>
     DROP @<object type> IF EXISTS @<name>
  </pre>
  strings @<object type> and @<name> are returned by @c X::sql_object_name()
  and @c X::sql_name() methods of the class @c meta::X representing the item.
  If necessary, method @c drop() can be overwritten in a specialized class
  corresponding to a given type of meta-data item.

  @returns OK or ERROR
 */
result_t
meta::Item::drop(THD *thd)
{
  const char *ob= sql_object_name();

  /*
    An item class should define object name for DROP statement
    or redefine drop() method.
   */
  DBUG_ASSERT(ob);

  String drop_stmt;

  drop_stmt.append("DROP ");
  drop_stmt.append(ob);
  drop_stmt.append(" IF EXISTS ");
  drop_stmt.append(sql_name());

  return silent_exec_query(thd,drop_stmt) ? ERROR : OK;
};

/**** SAVE/RESTORE DATABASES ***********************************/

/**
  Save data needed to create a database.

  Currently we don't save anything. A database is always created using
  "CREATE DATABASE @<name>" statement.
 */
result_t
meta::Db::save(THD*,OStream&)
{
  return OK;
}

/**
  Read data needed to create a database.

  Nothing to read. We just build the "CREATE DATABASE ..." statement in
  @c create_stmt.
 */
result_t
meta::Db::read(IStream&)
{
  create_stmt.append("CREATE DATABASE ");
  create_stmt.append(sql_name());
  return OK;
}


/**** SAVE/RESTORE TABLES ***************************************/

/**
  Build a CREATE statement for a table.

  We use @c store_create_info() function defined in the server. For that
  we need to open the table. After building the statement the table is closed to
  save resources. Actually, all tables of the thread are closed as we use
  @c close_thread_tables() function.
 */
int meta::Table::build_create_stmt(THD *thd)
{
  TABLE_LIST t, *tl= &t;

  bzero(&t,sizeof(TABLE_LIST));

  t.db= const_cast<char*>(in_db().name().ptr());
  t.alias= t.table_name= const_cast<char*>(sql_name());

  uint cnt;
  int res= ::open_tables(thd,&tl,&cnt,0);

  if (res)
  {
    DBUG_PRINT("backup",("Can't open table %s to save its description (error=%d)",
                         t.alias,res));
    return res;
  }

  res= ::store_create_info(thd,&t,&create_stmt,NULL);

  if (res)
    DBUG_PRINT("backup",("Can't get CREATE statement for table %s (error=%d)",
                         t.alias,res));

  ::close_thread_tables(thd);

  return res;
}

} // backup namespace


/************************************************

              Helper functions

 ************************************************/

namespace backup {

/// Execute SQL query without sending anything to client.

/*
  Note: the change net.vio idea taken from execute_init_command in
  sql_parse.cc
 */

int silent_exec_query(THD *thd, String &query)
{
  Vio *save_vio= thd->net.vio;

  DBUG_PRINT("restore",("executing query %s",query.c_ptr()));

  thd->net.vio= 0;
  thd->net.no_send_error= 0;

  thd->query=         query.c_ptr();
  thd->query_length=  query.length();

  thd->set_time(time(NULL));
  pthread_mutex_lock(&::LOCK_thread_count);
  thd->query_id= ::next_query_id();
  pthread_mutex_unlock(&::LOCK_thread_count);

  const char *ptr;
  ::mysql_parse(thd,thd->query,thd->query_length,&ptr);

  thd->net.vio= save_vio;

  if (thd->query_error)
  {
    DBUG_PRINT("restore",
              ("error executing query %s!", thd->query));
    DBUG_PRINT("restore",("last error (%d): %s",thd->net.last_errno
                                               ,thd->net.last_error));
    return thd->net.last_errno ? thd->net.last_errno : -1;
  }

  return 0;
}

} // backup namespace


/**********************************************************

  Old code to incorporate into above framework: handles
  events, routines and triggers

 **********************************************************/

namespace backup {

#if FALSE

/*
  Get the metadata for the table specified.

  SYNOPSIS
    get_table_metadata()
    THD *thd        - The current thread instance.
    TABLE_LIST *tbl - The list of tables.
    List<String>    - A string list for storing the SQL strings.

  DESCRIPTION
    This procedure loads the string list with the SQL statements for
    creating the table and all of the triggers associated with it.

  NOTES
    Method is designed to process one table at a time thereby only
    locking one table at a time.

  RETURNS
    0  - no errors.
    -1 - error reading table or trigger metadata
*/
bool get_table_metadata(THD *thd, TABLE_LIST *table,
                        List<String> *buffer)
{
  String *tbl_sql;

  DBUG_ENTER("get_table_metadata");
  /*
    Check to see if tables exist and open them. Abort if
    something is wrong.
  */
  table->lock_type= TL_READ;
  DBUG_PRINT("metadata_backup", ("opening the tables"));

  uint counter;
  if (open_tables(thd, &table, &counter, 0))
  {
    DBUG_PRINT("metadata_backup", ( "error opening tables!" ));
    DBUG_RETURN(-1);
  }

  tbl_sql= new (current_thd->mem_root) String();
  tbl_sql->length(0);
  /*
    Get the CREATE statement for the table and store it in the buffer.
  */
  DBUG_PRINT("metadata_backup", ("constructing CREATE TABLE statement"));
  store_create_info(thd, table, tbl_sql, NULL);
  insert_db_in_create(table, tbl_sql); // patch SQL statement
  DBUG_PRINT("metadata_backup", ("query size = %d", tbl_sql->length()));
  DBUG_PRINT("metadata_backup", ("query = %s", tbl_sql->c_ptr()));
  buffer->push_back(tbl_sql);

  /*
    If triggers exist for this table, open the trigger list
    and construct the CREATE TRIGGER statements placing them in
    the buffer.
  */
  if (table->table->triggers)
  {
    Table_triggers_list *triggers= table->table->triggers;
    int event, timing;
    for (event= 0; event < (int)TRG_EVENT_MAX; event++)
    {
      for (timing= 0; timing < (int)TRG_ACTION_MAX; timing++)
      {
        LEX_STRING trigger_name;
        LEX_STRING trigger_stmt;
        ulong sql_mode;
        char definer_holder[USER_HOST_BUFF_SIZE];
        LEX_STRING definer_buffer;
        definer_buffer.str= definer_holder;
        if (!triggers->get_trigger_info(thd, (enum trg_event_type) event,
                                       (enum trg_action_time_type)timing,
                                       &trigger_name, &trigger_stmt,
                                       &sql_mode,
                                       &definer_buffer))
        {
          DBUG_PRINT("metadata_backup",
                    ("constructing the CREATE TRIGGER statement"));
          tbl_sql= new (current_thd->mem_root) String();
          tbl_sql->length(0);
          if (definer_buffer.length)
          {
            tbl_sql->append("CREATE DEFINER = ");
            tbl_sql->append(definer_buffer.str);
          }
          else
            tbl_sql->append("CREATE");
          tbl_sql->append(" TRIGGER ");
          tbl_sql->append(trigger_name.str);
          tbl_sql->append(" ");
          tbl_sql->append(trg_action_time_type_names[timing].str);
          tbl_sql->append(" ");
          tbl_sql->append(trg_event_type_names[event].str);
          tbl_sql->append(" ON ");
          tbl_sql->append(table->db);
          tbl_sql->append(".");
          tbl_sql->append(table->table_name);
          tbl_sql->append(" \nFOR EACH ROW ");
          tbl_sql->append(trigger_stmt.str);
          /*
            Store the size of the string in the buffer followed by the
            SQL string. Adjust pointer for next SQL string.
          */
          DBUG_PRINT("metadata_backup", ("trg query size = %d",
                     tbl_sql->length()));
          DBUG_PRINT("metadata_backup", ("trg query = %s", tbl_sql->c_ptr()));
          buffer->push_back(tbl_sql);
        }
        /*
          Skip this trigger if it isn't for this table.
        */
        else
          continue;
      }
    }
  }
  /*
    Remove read lock on the tables
  */
  DBUG_PRINT("metadata_backup", ("closing the tables"));
  close_thread_tables(thd);
  DBUG_RETURN(0);
}

/*
  Get the metadata for the database specified.

  SYNOPSIS
    get_db_metadata()
    THD *thd     - The current thread instance.
    char *db     - The name of the database.
    List<String> - A string list for storing the SQL strings.

  DESCRIPTION
    This procedure loads the string list with the SQL statements for
    creating the database and all of the stored procedures and
    functions associated with it.

  RETURNS
    0  - no errors.
    -1 - database cannot be read or doesn't exist.
*/
bool get_db_metadata(THD *thd, const char *db,
                     List<String> *buffer)
{
  TABLE *proc_table;
  TABLE_LIST proc_tables;
  String *db_sql;
  String tmp;
  LEX_STRING db_name;

  DBUG_ENTER("get_db_metadata");
  /*
    Check to see if database is valid name.
  */
  DBUG_PRINT("metadata_backup", ("checking the database"));
  db_name.str= thd->alloc(strlen(db) + 2);
  db_name.length= strlen(db);
  strcpy(db_name.str, db);
  if (check_db_name(&db_name))
  {
    DBUG_PRINT("metadata_backup", ("error with database name"));
    DBUG_RETURN(-1);
  }

  /*
    Write the CREATE statement.
  */
  db_sql= new (current_thd->mem_root) String();
  db_sql->length(0);
  db_sql->append("CREATE DATABASE IF NOT EXISTS ");
  db_sql->append(db);
  buffer->push_back(db_sql);

  /*
    Setup a new table list that contains the proc table.
  */
  bzero((char*) &proc_tables,sizeof(proc_tables));
  proc_tables.db= (char*) db;
  proc_tables.db_length= strlen(db);
  proc_tables.table_name= proc_tables.alias= (char*) "proc";
  proc_tables.table_name_length= 4;
  proc_tables.lock_type= TL_READ;
  Open_tables_state open_tables_state_backup;

  /*
    Check to see if the proc table can be opened for reading.
  */
  DBUG_PRINT("metadata_backup", ("opening the proc table"));
  if (!(proc_table= open_proc_table_for_read(thd, &open_tables_state_backup)))
  {
    DBUG_PRINT("metadata_backup", ("error with proc table"));
    DBUG_RETURN(-1);
  }

  /*
    Loop through all of the results in the proc table.
    Construct the CREATE PROCEDURE and CREATE FUNCTION SQL commands
    for this database.
  */
  proc_table->file->ha_index_init(0, 1);
  while (!proc_table->file->index_next(proc_table->record[0]))
  {
    db_sql= new (current_thd->mem_root) String();
    get_field(thd->mem_root, proc_table->field[0], &tmp);
    if (my_strcasecmp(system_charset_info, tmp.c_ptr(), db) == 0)
    {
      /*
        Setup preamble for create statements including definer or
        invoker access options.
      */
      DBUG_PRINT("metadata_backup",
                ("constructing the CREATE PROCEDURE/FUNCION statement"));
      db_sql->length(0);
      db_sql->append("CREATE ");
      get_field(thd->mem_root, proc_table->field[11], &tmp);
      if (tmp.length())
      {
        get_field(thd->mem_root, proc_table->field[7], &tmp);
        db_sql->append(tmp);
        get_field(thd->mem_root, proc_table->field[11], &tmp);
        db_sql->append(" = ");
        db_sql->append(tmp);
        db_sql->append(" ");
      }
      get_field(thd->mem_root, proc_table->field[2], &tmp);
      db_sql->append(tmp);
      db_sql->append(" ");
      get_field(thd->mem_root, proc_table->field[0], &tmp);
      db_sql->append(tmp);
      db_sql->append(".");
      get_field(thd->mem_root, proc_table->field[1], &tmp);
      db_sql->append(tmp);
      db_sql->append(" ");
      get_field(thd->mem_root, proc_table->field[2], &tmp);
      if (!my_strcasecmp(system_charset_info, tmp.c_ptr(), "PROCEDURE"))
      {
        /* It's a procedure */
        get_field(thd->mem_root, proc_table->field[8], &tmp);
        db_sql->append("(");
        db_sql->append(tmp);
        get_field(thd->mem_root, proc_table->field[10], &tmp);
        db_sql->append(") \n");
        db_sql->append(tmp);
      }
      else
      {
        /* It's a function */
        get_field(thd->mem_root, proc_table->field[8], &tmp);
        db_sql->append("(");
        db_sql->append(tmp);
        db_sql->append(") \nRETURNS ");
        get_field(thd->mem_root, proc_table->field[9], &tmp);
        db_sql->append(tmp);
        get_field(thd->mem_root, proc_table->field[6], &tmp);
        if (!my_strcasecmp(system_charset_info, tmp.c_ptr(), "YES"))
          db_sql->append(" DETERMINISTIC ");
        else
          db_sql->append(" ");
        get_field(thd->mem_root, proc_table->field[10], &tmp);
        db_sql->append(tmp);
      }
      DBUG_PRINT("metadata_backup", ("db query = %s", db_sql->c_ptr()));
      buffer->push_back(db_sql);
    }
  }
  DBUG_PRINT("metadata_backup", ("closing the proc table"));
  proc_table->file->ha_index_end();
  close_proc_table(thd, &open_tables_state_backup);
  DBUG_RETURN(0);
}


/*
  Use the catalog read to get the metadata for the databases
  in the archive and execute them.

  SYNOPSIS
    restore_metadata()
    THD *thd          - The current thread instance.
    Restore_info info - Restore information.

  DESCRIPTION
    This procedure iterates through the catalog and executes
    the metadata strings for each database and table in the
    catalog.

  NOTES
    - Algorithm:
        For each database, drop the db then execute SQL strings
          For each table in table_info, execute SQL strings
    - Currently processes all databases and tables in the catalog.

  RETURNS
    0  - no errors.
    -1 - error on execute SQL string.
*/
bool restore_metadata(THD *thd, const Restore_info &info)
{
  String     sql;
  schema_ref *tmp;

  DBUG_ENTER("restore::restore_metadata");
  DBUG_ASSERT(thd);

  List<schema_ref> cat= info.catalog;
  List_iterator<schema_ref> catalog(cat);
  while ((tmp= catalog++))
  {
    /*
      DROP DATABASE: Execute the DROP DATABASE IF EXISTS
    */
    sql.length(0);
    sql.append("DROP DATABASE IF EXISTS ");
    sql.append(tmp->db_name.c_ptr());
    bool res= silent_exec_query(thd, sql);
    DBUG_PRINT("restore",("  res=%d after executing %s", res, sql.c_ptr()));

    /*
      Run the DB metadata strings
    */
    String *dbstr;
    List_iterator<String> db_meta(tmp->db_metadata);
    while ((dbstr= db_meta++))
    {
      res= silent_exec_query(thd, *dbstr);
      DBUG_PRINT("restore",("  res=%d after executing %s", res, dbstr->c_ptr()));
    }
    table_info *t;
    List_iterator<table_info> tbls(tmp->tables);
    while ((t= tbls++))
    {
      /*
        Run the table metadata strings
      */
      String *s;
      List_iterator<String> tbl_meta(t->table_metadata);
      while ((s= tbl_meta++))
      {
        res= silent_exec_query(thd, *s);
        DBUG_PRINT("restore",("  res=%d after executing %s", res, s->c_ptr()));
      }
    }
  }

  DBUG_PRINT("backup",("databases dropped then created"));
  DBUG_PRINT("backup",("tables created"));
  DBUG_RETURN(TRUE);
}

#endif

} // backup namespace
