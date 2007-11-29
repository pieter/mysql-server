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

#include "backup_aux.h"
#include "backup_kernel.h"
#include "meta_data.h"


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
      DBUG_RETURN(ERROR);
    }
  }

  if (stream_result::ERROR == s.end_chunk())
    DBUG_RETURN(ERROR);

  info.meta_size= s.bytes - start_bytes;

  DBUG_RETURN(0);
}


/**
  Read meta-data items from a stream and create them if they are selected
  for restore.

  @pre  Stream is at the beginning of a saved meta-data chunk.
  @post Stream is at the beginning of the next chunk.
 */
int restore_meta_data(THD *thd, Restore_info &info, IStream &s)
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
       DBUG_RETURN(ERROR);
     }

     delete it;
    }
  }

  // We should reach end of chunk now - if not something went wrong

  if (res != DONE)
  {
    info.report_error(ER_BACKUP_READ_META);
    DBUG_RETURN(ERROR);
  }

  DBUG_ASSERT(res == DONE);

  if (stream_result::ERROR == s.next_chunk())
  {
    info.report_error(ER_BACKUP_NEXT_CHUNK);
    DBUG_RETURN(ERROR);
  }

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
  return res == stream_result::NIL ? ERROR : report_stream_result(res);
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
  @verbatim
     DROP @<object type> IF EXISTS @<name>
  @endverbatim
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
}

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

  if (thd->is_slave_error)
  {
    DBUG_PRINT("restore",
              ("error executing query %s!", thd->query));
    DBUG_PRINT("restore",("last error (%d): %s",thd->net.last_errno
                                               ,thd->net.last_error));
    return thd->net.last_errno ? (int)thd->net.last_errno : -1;
  }

  return 0;
}

} // backup namespace
