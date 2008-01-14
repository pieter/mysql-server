/**
   @file

   This file defines the API for the following object services:
     - serialize database objects into a string;
     - materialize (deserialize) object from a string;
     - enumerating objects;
     - finding dependencies for objects;
     - executor for SQL statements;
     - wrappers for controlling the DDL Blocker;

  The methods defined below are used to provide server functionality to
  and permitting an isolation layer for the client (caller).
*/

#include "mysql_priv.h"
#include "si_objects.h"
#include "ddl_blocker.h"
#include "sql_show.h"
#include "events.h"
#include "event_data_objects.h"
#include "event_db_repository.h"
#include "sql_trigger.h"
#include "sp.h"
#include "sp_head.h" // for sp_add_to_query_tables().

TABLE *create_schema_table(THD *thd, TABLE_LIST *table_list); // defined in sql_show.cc

DDL_blocker_class *DDL_blocker= NULL;

///////////////////////////////////////////////////////////////////////////

namespace {

// Helper methods

/**
  Execute the SQL string passed.

  This is a private helper function to the implementation.
*/
int silent_exec(THD *thd, String *query)
{
  Vio *save_vio= thd->net.vio;

  DBUG_PRINT("si_objects",("executing %s",query->c_ptr()));

  /*
    Note: the change net.vio idea taken from execute_init_command in
    sql_parse.cc
   */
  thd->net.vio= 0;
  thd->net.no_send_error= 0;

  thd->query=         query->c_ptr();
  thd->query_length=  query->length();

  thd->set_time(time(NULL));
  pthread_mutex_lock(&::LOCK_thread_count);
  thd->query_id= ::next_query_id();
  pthread_mutex_unlock(&::LOCK_thread_count);

  /*
    @todo The following is a work around for online backup and the DDL blocker.
          It should be removed when the generalized solution is in place.
          This is needed to ensure the restore (which uses DDL) is not blocked
          when the DDL blocker is engaged.
  */
  thd->DDL_exception= TRUE;

  /*
    Note: This is a copy and paste from the code in sql_parse.cc. 
          See "case COM_QUERY:".
  */
  const char *found_semicolon= thd->query;
  char *packet_end= thd->query + thd->query_length;
  mysql_parse(thd, thd->query, thd->query_length, &found_semicolon);
  while (!thd->killed && found_semicolon && !thd->is_error())
  {
    char *next_packet= (char*) found_semicolon;
    thd->net.no_send_error= 0;
    /*
      Multiple queries exits, execute them individually
    */
    close_thread_tables(thd);
    ulong length= (ulong)(packet_end - next_packet);

    /* Remove garbage at start of query */
    while (my_isspace(thd->charset(), *next_packet) && length > 0)
    {
      next_packet++;
      length--;
    }
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    thd->query_length= length;
    thd->query= next_packet;
    thd->query_id= next_query_id();
    thd->set_time(); /* Reset the query start time. */
    /* TODO: set thd->lex->sql_command to SQLCOM_END here */
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    mysql_parse(thd, next_packet, length, & found_semicolon);
  }

  thd->net.vio= save_vio;

  if (thd->is_error())
  {
    DBUG_PRINT("restore",
              ("error executing query %s!", thd->query));
    DBUG_PRINT("restore",("last error (%d): %s",thd->net.last_errno
                                               ,thd->net.last_error));
    return thd->net.last_errno ? (int)thd->net.last_errno : -1;
  }

  return 0;
}

/*
  This method gets the create statement for a procedure or function.
*/
int serialize_routine(THD *thd,
                     int type,
                      String db_name,
                      String r_name,
                      String *string)
{
  bool ret= false;
  sp_head *sp;
  sp_name *routine_name;
  LEX_STRING sql_mode;
  DBUG_ENTER("serialize_routine");
  DBUG_PRINT("serialize_routine", ("name: %s@%s", db_name.c_ptr(), 
             r_name.c_ptr()));

  DBUG_ASSERT(type == TYPE_ENUM_PROCEDURE || type == TYPE_ENUM_FUNCTION);
  sp_cache **cache = type == TYPE_ENUM_PROCEDURE ?
                     &thd->sp_proc_cache : &thd->sp_func_cache;
  string->length(0);
  LEX_STRING db;
  db.str= db_name.c_ptr();
  db.length= db_name.length();
  LEX_STRING name;
  name.str= r_name.c_ptr();
  name.length= r_name.length();
  routine_name= new sp_name(db, name, true);
  routine_name->init_qname(thd);
  if (type == TYPE_ENUM_PROCEDURE)
    thd->variables.max_sp_recursion_depth++;
  if ((sp= sp_find_routine(thd, type, routine_name, cache, FALSE)))
  {
    sys_var_thd_sql_mode::symbolic_mode_representation(thd, 
      sp->m_sql_mode, &sql_mode);
    Stored_program_creation_ctx *sp_ctx= sp->get_creation_ctx();

    /*
      Prepend sql_mode command.
    */
    string->append("SET SQL_MODE = '");
    string->append(sql_mode.str);
    string->append("'; ");

    /*
      append character set client charset information
    */
    string->append("SET CHARACTER_SET_CLIENT = '");
    string->append(sp_ctx->get_client_cs()->csname);
    string->append("'; ");

    /*
      append collation_connection information
    */
    string->append("SET COLLATION_CONNECTION = '");
    string->append(sp_ctx->get_connection_cl()->name);
    string->append("'; ");

    /*
      append collation_connection information
    */
    string->append("SET COLLATION_DATABASE = '");
    string->append(sp_ctx->get_db_cl()->name);
    string->append("'; ");

    string->append(sp->m_defstr.str);
  }
  else
  {
    string->length(0);
    ret= -1;
  }
  if (type == TYPE_ENUM_PROCEDURE)
    thd->variables.max_sp_recursion_depth--;
  DBUG_RETURN(ret);
}

/*
  This method calls silent_exec() while saving the context
  information before the call and restoring after the call.

  If save_timezone, it also saves and restores the timezone.
*/
bool execute_with_ctx(THD *thd, String *query, bool save_timezone)
{
  bool ret= false;
  ulong orig_sql_mode;
  CHARSET_INFO *orig_char_set_client;
  CHARSET_INFO *orig_coll_conn;
  CHARSET_INFO *orig_coll_db;
  Time_zone *tm_zone;
  DBUG_ENTER("Obj::execute_with_ctx()");

  /*
    Preserve SQL_MODE, CHARACTER_SET_CLIENT, COLLATION_CONNECTION,
    and COLLATION_DATABASE.
  */
  orig_sql_mode= thd->variables.sql_mode;
  orig_char_set_client= thd->variables.character_set_client;
  orig_coll_conn= thd->variables.collation_connection;
  orig_coll_db= thd->variables.collation_database;

  /*
    Preserve timezone.
  */
  if (save_timezone)
    tm_zone= thd->variables.time_zone;

  ret= silent_exec(thd, query);

  /*
    Restore SQL_MODE, CHARACTER_SET_CLIENT, COLLATION_CONNECTION,
    and COLLATION_DATABASE.
  */
  thd->variables.sql_mode= orig_sql_mode;
  thd->variables.character_set_client= orig_char_set_client;
  thd->variables.collation_connection= orig_coll_conn;
  thd->variables.collation_database= orig_coll_db;

  /*
    Restore timezone.
  */
  if (save_timezone)
    thd->variables.time_zone= tm_zone;

  DBUG_RETURN(ret);
}

/*
  Drops an object.

  obj_name is the name of the object e.g., DATABASE, PROCEDURE, etc.
  name1 is the db name  (blank for database objects)
  name2 is the name of the object
*/
bool drop_object(THD *thd, char *obj_name, String *name1, String *name2)
{
  DBUG_ENTER("Obj::drop_object()");
  String cmd;
  cmd.length(0);
  cmd.append("DROP ");
  cmd.append(obj_name);
  cmd.append(" IF EXISTS ");
  if (name1->length() > 0)
  {
    cmd.append(name1->c_ptr());
    cmd.append(".");
  }
  cmd.append(name2->c_ptr());
  DBUG_RETURN(silent_exec(thd, &cmd));
}

/**
  Open given table in @c INFORMATION_SCHEMA database.

  This is a private helper function to the implementation.
*/
TABLE* open_schema_table(THD *thd, ST_SCHEMA_TABLE *st)
{
  TABLE *t;
  TABLE_LIST arg;
  my_bitmap_map *old_map;

  bzero( &arg, sizeof(TABLE_LIST) );

  // set context for create_schema_table call
  arg.schema_table= st;
  arg.alias=        NULL;
  arg.select_lex=   NULL;

  t= create_schema_table(thd,&arg); // Note: callers must free t.

  if( !t ) return NULL; // error!

  /*
   Temporarily set thd->lex->wild to NULL to keep st->fill_table
   happy.
  */
  ::String *wild= thd->lex->wild;
  ::enum_sql_command command= thd->lex->sql_command;

  thd->lex->wild = NULL;
  thd->lex->sql_command = enum_sql_command(0);

  // context for fill_table
  arg.table= t;

  old_map= tmp_use_all_columns(t, t->read_set);

  /*
    Question: is it correct to fill I_S table each time we use it or should it
    be filled only once?
   */
  st->fill_table(thd,&arg,NULL);  // NULL = no select condition

  tmp_restore_column_map(t->read_set, old_map);

  // undo changes to thd->lex
  thd->lex->wild= wild;
  thd->lex->sql_command= command;

  return t;
}

///////////////////////////////////////////////////////////////////////////

struct Table_name_key
{
  Table_name_key(const char *db_name_str,
                 uint db_name_length,
                 const char *table_name_str,
                 uint table_name_length)
  {
    db_name.copy(db_name_str, db_name_length, system_charset_info);
    table_name.copy(table_name_str, table_name_length, system_charset_info);

    key.length(0);
    key.append(db_name);
    key.append(".");
    key.append(table_name);
  }

  String db_name;
  String table_name;

  String key;
};

uchar *
get_table_name_key(const uchar *record,
                   size_t *key_length,
                   my_bool not_used __attribute__((unused)))
{
  Table_name_key *tnk= (Table_name_key *) record;
  *key_length= tnk->key.length();
  return (uchar *) tnk->key.c_ptr_safe();
}

void delete_table_name_key(void *data)
{
  Table_name_key *tnk= (Table_name_key *) data;
  delete tnk;
}

}

///////////////////////////////////////////////////////////////////////////

namespace obs {

///////////////////////////////////////////////////////////////////////////

//
// Implementation: object impl classes.
//

///////////////////////////////////////////////////////////////////////////

/**
   @class DatabaseObj

   This class provides an abstraction to a database object for creation and
   capture of the creation data.
*/
class DatabaseObj : public Obj
{
public:
  DatabaseObj(const String *db_name);

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                           const String *serialization);

  virtual bool execute(THD *thd);

  const String* get_name()
  { return &m_db_name; }

  const String *get_db_name()
  {
    return &m_db_name;
  }

private:
  // These attributes are to be used only for serialization.
  String m_db_name;

  bool drop(THD *thd);

private:
  // These attributes are to be used only for materialization.
  String m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

/**
   @class TableObj

   This class provides an abstraction to a table object for creation and
   capture of the creation data.
*/
class TableObj : public Obj
{
public:
  TableObj(const String *db_name,
           const String *table_name,
           bool table_is_view);

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                           const String *serialization);

  virtual bool execute(THD *thd);

  const String* get_name()
  { return &m_table_name; }

  const String *get_db_name()
  {
    return &m_db_name;
  }

private:
  // These attributes are to be used only for serialization.
  String m_db_name;
  String m_table_name;
  bool m_table_is_view;

  bool drop(THD *thd);

private:
  // These attributes are to be used only for materialization.
  String m_create_stmt;

private:
  bool serialize_table(THD *thd, String *serialization);
  bool serialize_view(THD *thd, String *serialization);
};

///////////////////////////////////////////////////////////////////////////

/**
  @class TriggerObj

  This class provides an abstraction to a trigger object for creation and
  capture of the creation data.
*/
class TriggerObj : public Obj
{
public:
  TriggerObj(const String *db_name,
             const String *trigger_name);

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                           const String *serialization);

  virtual bool execute(THD *thd);

  const String* get_name()
  { return &m_trigger_name; }

  const String *get_db_name()
  {
    return &m_db_name;
  }

private:
  // These attributes are to be used only for serialization.
  String m_db_name;
  String m_trigger_name;

  bool drop(THD *thd);

private:
  // These attributes are to be used only for materialization.
  String m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

/**
  @class StoredProcObj

  This class provides an abstraction to a stored procedure object for creation
  and capture of the creation data.
*/
class StoredProcObj : public Obj
{
public:
  StoredProcObj(const String *db_name,
                const String *stored_proc_name);

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                           const String *serialization);

  virtual bool execute(THD *thd);

  const String* get_name()
  { return &m_stored_proc_name; }

  const String *get_db_name()
  {
    return &m_db_name;
  }

private:
  // These attributes are to be used only for serialization.
  String m_db_name;
  String m_stored_proc_name;

  bool drop(THD *thd);

private:
  // These attributes are to be used only for materialization.
  String m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

/**
  @class StoredFuncObj

  This class provides an abstraction to a stored function object for creation
  and capture of the creation data.
*/
class StoredFuncObj : public Obj
{
public:
  StoredFuncObj(const String *db_name,
                const String *stored_func_name);

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                           const String *serialization);

  virtual bool execute(THD *thd);

  const String* get_name()
  { return &m_stored_func_name; }

  const String *get_db_name()
  {
    return &m_db_name;
  }

private:
  // These attributes are to be used only for serialization.
  String m_db_name;
  String m_stored_func_name;

  bool drop(THD *thd);

private:
  // These attributes are to be used only for materialization.
  String m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

/**
  @class EventObj

  This class provides an abstraction to a event object for creation and capture
  of the creation data.
*/
class EventObj : public Obj
{
public:
  EventObj(const String *db_name,
           const String *event_name);

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                           const String *serialization);

  virtual bool execute(THD *thd);

  const String* get_name()
  { return &m_event_name; }

  const String *get_db_name()
  {
    return &m_db_name;
  }

private:
  // These attributes are to be used only for serialization.
  String m_db_name;
  String m_event_name;

  bool drop(THD *thd);

private:
  // These attributes are to be used only for materialization.
  String m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

//
// Implementation: iterator impl classes.
//

///////////////////////////////////////////////////////////////////////////

class DatabaseIterator : public ObjIterator
{
public:
  virtual ~DatabaseIterator();

public:
  virtual DatabaseObj *next();

private:
  THD *m_thd;
  TABLE *m_is_schemata;
  handler *m_ha;
  my_bitmap_map *m_old_map;

private:
  friend ObjIterator* get_databases(THD*);

private:
  static DatabaseIterator *create(THD *thd);

  DatabaseIterator(THD *thd,
                   TABLE *is_schemata,
                   handler *ha,
                   my_bitmap_map *old_map);
};

///////////////////////////////////////////////////////////////////////////

class DbTablesIterator : public ObjIterator
{
public:
public:
  virtual ~DbTablesIterator();

public:
  virtual TableObj *next();

protected:
  DbTablesIterator(THD *thd,
                   const String *db_name,
                   TABLE *is_tables,
                   handler *ha,
                   my_bitmap_map *old_map);

protected:
  virtual bool is_type_accepted(const String *type) const;

  virtual TableObj *create_table_obj(const String *db_name,
                                     const String *table_name) const;

private:
  THD *m_thd;
  String m_db_name;
  TABLE *m_is_tables;
  handler *m_ha;
  my_bitmap_map *m_old_map;

private:
  template<typename T>
  static T *create(THD *thd, const String *db_name);

private:
  friend ObjIterator *get_db_tables(THD*, const String*);
  friend ObjIterator *get_db_views(THD*, const String*);
};

///////////////////////////////////////////////////////////////////////////

class DbViewsIterator : public DbTablesIterator
{
protected:
  DbViewsIterator(THD *thd,
                  const String *db_name,
                  TABLE *is_tables,
                  handler *ha,
                  my_bitmap_map *old_map)
    : DbTablesIterator(thd, db_name, is_tables, ha, old_map)
  { }

protected:
  virtual bool is_type_accepted(const String *type) const;

  virtual TableObj *create_table_obj(const String *db_name,
                                     const String *table_name) const;

private:
  friend class DbTablesIterator;
};

///////////////////////////////////////////////////////////////////////////

class ViewBaseObjectsIterator : public ObjIterator
{
public:
  enum IteratorType
  {
    GET_BASE_TABLES,
    GET_BASE_VIEWS
  };

public:
  virtual ~ViewBaseObjectsIterator();

public:
  virtual TableObj *next();

private:
  static ViewBaseObjectsIterator *create(THD *thd,
                                         const String *db_name,
                                         const String *view_name,
                                         IteratorType iterator_type );

private:
  ViewBaseObjectsIterator(HASH *table_names);

private:
  HASH *m_table_names;
  uint m_cur_idx;

private:
  friend ObjIterator *get_view_base_tables(THD *,
                                           const String *,
                                           const String *);

  friend ObjIterator *get_view_base_views(THD *,
                                          const String *,
                                          const String *);
};

///////////////////////////////////////////////////////////////////////////

//
// Implementation: DatabaseIterator class.
//

///////////////////////////////////////////////////////////////////////////

DatabaseIterator *DatabaseIterator::create(THD *thd)
{
  DBUG_ASSERT(thd);

  TABLE *is_schemata = open_schema_table(thd, get_schema_table(SCH_SCHEMATA));

  if (!is_schemata)
    return NULL;

  handler *ha = is_schemata->file;

  if (!ha)
  {
    free_tmp_table(thd, is_schemata);
    return NULL;
  }

  my_bitmap_map *old_map=
    dbug_tmp_use_all_columns(is_schemata, is_schemata->read_set);

  if (ha->ha_rnd_init(TRUE))
  {
    dbug_tmp_restore_column_map(is_schemata->read_set, old_map);
    free_tmp_table(thd, is_schemata);
    return NULL;
  }

  return new DatabaseIterator(thd, is_schemata, ha, old_map);
}

DatabaseIterator::DatabaseIterator(THD *thd,
                                   TABLE *is_schemata,
                                   handler *ha,
                                   my_bitmap_map *old_map) :
  m_thd(thd),
  m_is_schemata(is_schemata),
  m_ha(ha),
  m_old_map(old_map)
{
  DBUG_ASSERT(m_thd);
  DBUG_ASSERT(m_is_schemata);
  DBUG_ASSERT(m_ha);
  DBUG_ASSERT(m_old_map);
}

DatabaseIterator::~DatabaseIterator()
{
  m_ha->ha_rnd_end();

  dbug_tmp_restore_column_map(m_is_schemata->read_set, m_old_map);
  free_tmp_table(m_thd, m_is_schemata);
}

DatabaseObj* DatabaseIterator::next()
{
  if (m_ha->rnd_next(m_is_schemata->record[0]))
    return NULL;

  String name;

  m_is_schemata->field[1]->val_str(&name);

  DBUG_PRINT("DatabaseIterator::next", (" Found database %s", name.ptr()));

  return new DatabaseObj(&name);
}

///////////////////////////////////////////////////////////////////////////

//
// Implementation: DbTablesIterator class.
//

///////////////////////////////////////////////////////////////////////////

template DbTablesIterator *
DbTablesIterator::create(THD *thd, const String *db_name);

template <typename T>
T *DbTablesIterator::create(THD *thd, const String *db_name)
{
  TABLE *is_tables= open_schema_table(thd, get_schema_table(SCH_TABLES));

  if (!is_tables)
    return NULL;

  handler *ha= is_tables->file;

  if (!ha)
  {
    free_tmp_table(thd, is_tables);
    return NULL;
  }

  my_bitmap_map *old_map=
    dbug_tmp_use_all_columns(is_tables, is_tables->read_set);

  if (ha->ha_rnd_init(TRUE))
  {
    dbug_tmp_restore_column_map(is_tables->read_set, old_map);
    free_tmp_table(thd, is_tables);
    return NULL;
  }

  return new T(thd, db_name, is_tables, ha, old_map);
}

DbTablesIterator::DbTablesIterator(THD *thd,
                                   const String *db_name,
                                   TABLE *is_tables,
                                   handler *ha,
                                   my_bitmap_map *old_map) :
  m_thd(thd),
  m_is_tables(is_tables),
  m_ha(ha),
  m_old_map(old_map)
{
  DBUG_ASSERT(thd);
  DBUG_ASSERT(db_name);
  DBUG_ASSERT(is_tables);
  DBUG_ASSERT(ha);
  DBUG_ASSERT(old_map);

  m_db_name.copy(*db_name);
}

DbTablesIterator::~DbTablesIterator()
{
  m_ha->ha_rnd_end();

  dbug_tmp_restore_column_map(m_is_tables->read_set, m_old_map);
  free_tmp_table(m_thd, m_is_tables);
}

TableObj* DbTablesIterator::next()
{
  while (!m_ha->rnd_next(m_is_tables->record[0]))
  {
    String table_name;
    String db_name;
    String type;

    m_is_tables->field[1]->val_str(&db_name);
    m_is_tables->field[2]->val_str(&table_name);
    m_is_tables->field[3]->val_str(&type);

    // skip tables not from the given database
    if (db_name != m_db_name)
      continue;

    // skip tables/views depending on enumerate_views flag
    if (!is_type_accepted(&type))
      continue;

    DBUG_PRINT("DbTablesIterator::next", (" Found table %s.%s",
                                          db_name.ptr(), table_name.ptr()));
    return create_table_obj(&db_name, &table_name);
  }

  return NULL;
}

bool DbTablesIterator::is_type_accepted(const String *type) const
{
  return my_strcasecmp(system_charset_info,
                       ((String *) type)->c_ptr_safe(), "BASE TABLE");
}

TableObj *DbTablesIterator::create_table_obj(const String *db_name,
                                             const String *table_name) const
{
  return new TableObj(db_name, table_name, false);
}

///////////////////////////////////////////////////////////////////////////

//
// Implementation: DbViewsIterator class.
//

///////////////////////////////////////////////////////////////////////////

template DbViewsIterator *
DbTablesIterator::create(THD *thd, const String *db_name);

bool DbViewsIterator::is_type_accepted(const String *type) const
{
  return my_strcasecmp(system_charset_info,
                       ((String *) type)->c_ptr_safe(), "VIEW");
}

TableObj *DbViewsIterator::create_table_obj(const String *db_name,
                                            const String *table_name) const
{
  return new TableObj(db_name, table_name, true);
}

///////////////////////////////////////////////////////////////////////////

//
// Implementation: ViewBaseObjectsIterator class.
//

///////////////////////////////////////////////////////////////////////////

ViewBaseObjectsIterator *
ViewBaseObjectsIterator::create(THD *thd,
                               const String *db_name,
                               const String *view_name,
                               IteratorType iterator_type)
{
  THD *my_thd= new THD();

  my_thd->security_ctx= thd->security_ctx;

  my_thd->thread_stack= (char*) &my_thd;
  my_thd->store_globals();
  lex_start(my_thd);

  TABLE_LIST *tl =
    sp_add_to_query_tables(my_thd,
                           my_thd->lex,
                           ((String *) db_name)->c_ptr_safe(),
                           ((String *) view_name)->c_ptr_safe(),
                           TL_READ);

  if (open_and_lock_tables(my_thd, tl))
  {
    delete my_thd;
    thd->store_globals();

    return NULL;
  }

  HASH *table_names = new HASH();

  hash_init(table_names, system_charset_info, 16, 0, 0,
            get_table_name_key,
            delete_table_name_key,
            MYF(0));

  if (tl->view_tables)
  {
    List_iterator_fast<TABLE_LIST> it(*tl->view_tables);
    TABLE_LIST *tl2;

    while ((tl2 = it++))
    {
      Table_name_key *tnk=
        new Table_name_key(tl2->db, tl2->db_length,
                           tl2->table_name, tl2->table_name_length);

      if (iterator_type == GET_BASE_TABLES && tl2->view ||
          iterator_type == GET_BASE_VIEWS && !tl2->view)
        continue;

      if (!hash_search(table_names,
                       (uchar *) tnk->key.c_ptr_safe(),
                       tnk->key.length()))
      {
        my_hash_insert(table_names, (uchar *) tnk);
      }
      else
      {
        delete tnk;
      }
    }
  }

  delete my_thd;

  thd->store_globals();

  return new ViewBaseObjectsIterator(table_names);
}

ViewBaseObjectsIterator::ViewBaseObjectsIterator(HASH *table_names) :
  m_table_names(table_names),
  m_cur_idx(0)
{
}

ViewBaseObjectsIterator::~ViewBaseObjectsIterator()
{
  hash_free(m_table_names);
  delete m_table_names;
}

TableObj *ViewBaseObjectsIterator::next()
{
  if (m_cur_idx >= m_table_names->records)
    return NULL;

  Table_name_key *tnk=
    (Table_name_key *) hash_element(m_table_names, m_cur_idx);

  ++m_cur_idx;

  return new TableObj(&tnk->db_name, &tnk->table_name, false);
}

///////////////////////////////////////////////////////////////////////////

//
// Implementation: enumeration functions.
//

///////////////////////////////////////////////////////////////////////////

ObjIterator *get_databases(THD *thd)
{
  return DatabaseIterator::create(thd);
}

ObjIterator *get_db_tables(THD *thd, const String *db_name)
{
  return DbTablesIterator::create<DbTablesIterator>(thd, db_name);
}

ObjIterator *get_db_views(THD *thd, const String *db_name)
{
  return DbTablesIterator::create<DbViewsIterator>(thd, db_name);
}

ObjIterator *get_db_triggers(THD *thd, const String *db_name)
{
  return NULL;
}

ObjIterator *get_db_stored_procedures(THD *thd, const String *db_name)
{
  return NULL;
}

ObjIterator *get_db_stored_functions(THD *thd, const String *db_name)
{
  return NULL;
}

ObjIterator *get_db_events(THD *thd, const String *db_name)
{
  return NULL;
}

///////////////////////////////////////////////////////////////////////////

//
// Implementation: dependency functions.
//

///////////////////////////////////////////////////////////////////////////

ObjIterator* get_view_base_tables(THD *thd,
                                  const String *db_name,
                                  const String *view_name)
{
  return ViewBaseObjectsIterator::create(
    thd, db_name, view_name, ViewBaseObjectsIterator::GET_BASE_TABLES);
}

ObjIterator* get_view_base_views(THD *thd,
                                 const String *db_name,
                                 const String *view_name)
{
  return ViewBaseObjectsIterator::create(
    thd, db_name, view_name, ViewBaseObjectsIterator::GET_BASE_VIEWS);
}

///////////////////////////////////////////////////////////////////////////

//
// Implementation: DatabaseObj class.
//

///////////////////////////////////////////////////////////////////////////

/*
 Implementation: DatabaseObj class.
*/
DatabaseObj::DatabaseObj(const String *db_name)
:
  m_db_name(*db_name)
{
  m_db_name.copy(*db_name); // copy name string to newly allocated memory
}

/**
   serialize the object

   This method produces the data necessary for materializing the object
   on restore (creates object). 
   
   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @note this method will return an error if the db_name is either
         mysql or information_schema as these are not objects that 
         should be recreated using this interface.

   @returns bool 0 = SUCCESS
  */
bool DatabaseObj::serialize(THD *thd, String *serialization)
{
  HA_CREATE_INFO create;
  DBUG_ENTER("DatabaseObj::serialize()");
  DBUG_PRINT("DatabaseObj::serialize", ("name: %s", m_db_name.c_ptr()));

  if ((m_db_name == String (INFORMATION_SCHEMA_NAME.str, system_charset_info))
      ||
      (my_strcasecmp(system_charset_info, m_db_name.c_ptr(), "mysql") == 0))
  {
    DBUG_PRINT("backup",(" Skipping internal database %s", m_db_name.c_ptr()));
    DBUG_RETURN(TRUE);
  }
  create.default_table_charset= system_charset_info;

  if (check_db_dir_existence(m_db_name.c_ptr()))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), m_db_name.c_ptr());
    DBUG_RETURN(FALSE);
  }

  load_db_opt_by_name(thd, m_db_name.c_ptr(), &create);

  serialization->append(STRING_WITH_LEN("CREATE DATABASE "));
  append_identifier(thd, serialization, m_db_name.c_ptr(), m_db_name.length());

  if (create.default_table_charset)
  {
    serialization->append(STRING_WITH_LEN(" DEFAULT CHARACTER SET "));
    serialization->append(create.default_table_charset->csname);
    if (!(create.default_table_charset->state & MY_CS_PRIMARY))
    {
      serialization->append(STRING_WITH_LEN(" COLLATE "));
      serialization->append(create.default_table_charset->name);
    }
  }
  DBUG_RETURN(FALSE);
}

/**
   Materialize the serialization string.

   This method saves serialization string into a member variable.

   @param[in]  serialization_version   version number of this interface
   @param[in]  serialization           the string from serialize()

   @todo take serialization_version into account

   @returns bool 0 = SUCCESS
  */
bool DatabaseObj::materialize(uint serialization_version,
                             const String *serialization)
{
  DBUG_ENTER("DatabaseObj::materialize()");
  m_create_stmt.copy(*serialization);
  DBUG_RETURN(0);
}

/**
   Create the object.
   
   This method uses serialization string in a query and executes it.

   @param[in]  thd  current thread

   @returns bool 0 = SUCCESS
  */
bool DatabaseObj::execute(THD *thd)
{
  DBUG_ENTER("DatabaseObj::execute()");
  DBUG_RETURN(silent_exec(thd, &m_create_stmt));
}

/**
   Drops the object.

   This method calls the silent_exec method to execute the query.

   @note This uses "IF EXISTS" and does not return error if
         object does not exist.

   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @returns bool 0 = SUCCESS
  */
bool DatabaseObj::drop(THD *thd)
{
  DBUG_ENTER("DatabaseObj::drop()");
  DBUG_RETURN(drop_object(thd, "DATABASE", 0, &m_db_name));
}

/*
 Implementation: TableObj class.
*/
TableObj::TableObj(const String *db_name,
                   const String *table_name,
                   bool table_is_view) :
  m_table_is_view(table_is_view)
{
  m_db_name.copy(*db_name);
  m_table_name.copy(*table_name);
}

bool TableObj::serialize_table(THD *thd, String *serialization)
{
  return 0;
}

bool TableObj::serialize_view(THD *thd, String *serialization)
{
  return 0;
}

/**
   serialize the object

   This method produces the data necessary for materializing the object
   on restore (creates object). 
   
   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @returns bool 0 = SUCCESS
  */
bool TableObj::serialize(THD *thd, String *serialization)
{
  bool ret= 0;
  LEX_STRING tname, dbname;
  DBUG_ENTER("TableObj::serialize()");
  DBUG_PRINT("TableObj::serialize", ("name: %s@%s", m_db_name.c_ptr(),
             m_table_name.c_ptr()));

  serialization->length(0);
  tname.str= m_table_name.c_ptr();
  tname.length= m_table_name.length();
  dbname.str= m_db_name.c_ptr();
  dbname.length= m_db_name.length();
  Table_ident *name_id= new Table_ident(tname);
  name_id->db= dbname;

  /*
    Add the view to the table list and set the thd to look at views only.
    Note: derived from sql_yacc.yy.
  */
  thd->lex->select_lex.add_table_to_list(thd, name_id, NULL, 0);
  TABLE_LIST *table_list= (TABLE_LIST*)thd->lex->select_lex.table_list.first;
  thd->lex->sql_command = SQLCOM_SHOW_CREATE; 

  /*
    Setup view specific variables and settings
  */
  if (m_table_is_view)
  {
    thd->lex->only_view= 1;
    thd->lex->view_prepare_mode= TRUE; // use prepare mode
    table_list->skip_temporary= 1;     // skip temporary tables
  }

  /*
    Open the view and its base tables or views
  */
  if (open_normal_and_derived_tables(thd, table_list, 0))
    DBUG_RETURN(FALSE);

  /*
    Setup view specific variables and settings
  */
  if (m_table_is_view)
  {
    table_list->view_db= dbname;
    serialization->set_charset(table_list->view_creation_ctx->get_client_cs());
  }

  /*
    Get the create statement and close up shop.
  */
  ret= m_table_is_view ? 
    view_store_create_info(thd, table_list, serialization) :
    store_create_info(thd, table_list, serialization, NULL);
  close_thread_tables(thd);
  thd->lex->select_lex.table_list.empty();
  DBUG_RETURN(FALSE);
}

/**
   Materialize the serialization string.

   This method saves serialization string into a member variable.

   @param[in]  serialization_version   version number of this interface
   @param[in]  serialization           the string from serialize()

   @todo take serialization_version into account

   @returns bool 0 = SUCCESS
  */
bool TableObj::materialize(uint serialization_version,
                             const String *serialization)
{
  DBUG_ENTER("TableObj::materialize()");
  m_create_stmt.copy(*serialization);
  DBUG_RETURN(0);
}

/**
   Create the object.
   
   This method uses serialization string in a query and executes it.

   @param[in]  thd  current thread

   @returns bool 0 = SUCCESS
  */
bool TableObj::execute(THD *thd)
{
  DBUG_ENTER("TableObj::execute()");
  drop(thd);
  DBUG_RETURN(silent_exec(thd, &m_create_stmt));
}

/**
   Drops the object.

   This method calls the silent_exec method to execute the query.

   @note This uses "IF EXISTS" and does not return error if
         object does not exist.

   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @returns bool 0 = SUCCESS
  */
bool TableObj::drop(THD *thd)
{
  DBUG_ENTER("TableObj::drop()");
  DBUG_RETURN(drop_object(thd, "TABLE", &m_db_name, &m_table_name));
}

/*
 Implementation: TriggerObj class.
*/
TriggerObj::TriggerObj(const String *db_name,
                             const String *trigger_name)
{
  // copy strings to newly allocated memory
  m_db_name.copy(*db_name); 
  m_trigger_name.copy(*trigger_name); 
}

/**
   serialize the object

   This method produces the data necessary for materializing the object
   on restore (creates object). 
   
   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @note this method will return an error if the db_name is either
         mysql or information_schema as these are not objects that 
         should be recreated using this interface.

   @returns bool 0 = SUCCESS
  */
bool TriggerObj::serialize(THD *thd, String *serialization)
{
  bool ret= false;
  uint num_tables;
  sp_name *trig_name;
  LEX_STRING trg_name;
  ulonglong trg_sql_mode;
  LEX_STRING trg_sql_mode_str;
  LEX_STRING trg_sql_original_stmt;
  LEX_STRING trg_client_cs_name;
  LEX_STRING trg_connection_cl_name;
  LEX_STRING trg_db_cl_name;
  CHARSET_INFO *trg_client_cs;
  DBUG_ENTER("TriggerObj::serialize()");

  DBUG_PRINT("TriggerObj::serialize", ("name: %s in %s", 
             m_trigger_name.c_ptr(), m_db_name.c_ptr()));

  serialization->length(0);
  LEX_STRING db;
  db.str= m_db_name.c_ptr();
  db.length= m_db_name.length();
  LEX_STRING t_name; 
  t_name.str= m_trigger_name.c_ptr();
  t_name.length= m_trigger_name.length(); 
  trig_name= new sp_name(db, t_name, true);
  trig_name->init_qname(thd);
  TABLE_LIST *lst= get_trigger_table(thd, trig_name);
  if (!lst)
    DBUG_RETURN(FALSE);
  
  if (open_tables(thd, &lst, &num_tables, 0))
    DBUG_RETURN(FALSE);

  DBUG_ASSERT(num_tables == 1);
  Table_triggers_list *triggers= lst->table->triggers;
  if (!triggers)
    DBUG_RETURN(FALSE);

  int trigger_idx= triggers->find_trigger_by_name(&trig_name->m_name);
  if (trigger_idx < 0)
    DBUG_RETURN(FALSE);

  triggers->get_trigger_info(thd,
                             trigger_idx,
                             &trg_name,
                             &trg_sql_mode,
                             &trg_sql_original_stmt,
                             &trg_client_cs_name,
                             &trg_connection_cl_name,
                             &trg_db_cl_name);
  sys_var_thd_sql_mode::symbolic_mode_representation(thd,
                                                     trg_sql_mode,
                                                     &trg_sql_mode_str);

  /*
    prepend SQL Mode
  */
  serialization->append("SET SQL_MODE = '");
  serialization->append(trg_sql_mode_str.str);
  serialization->append("'; ");

  /*
    append character set client charset information
  */
  serialization->append("SET CHARACTER_SET_CLIENT = '");
  serialization->append(trg_client_cs_name.str);
  serialization->append("'; ");

  /*
    append collation_connection information
  */
  serialization->append("SET COLLATION_CONNECTION = '");
  serialization->append(trg_connection_cl_name.str);
  serialization->append("'; ");

  /*
    append collation_connection information
  */
  serialization->append("SET COLLATION_DATABASE = '");
  serialization->append(trg_db_cl_name.str);
  serialization->append("'; ");

  if (resolve_charset(trg_client_cs_name.str, NULL, &trg_client_cs))
    ret= false;
  else
    serialization->append(trg_sql_original_stmt.str);
  close_thread_tables(thd);
  thd->lex->select_lex.table_list.empty();
  DBUG_RETURN(ret);
}

/**
   Materialize the serialization string.

   This method saves serialization string into a member variable.

   @param[in]  serialization_version   version number of this interface
   @param[in]  serialization           the string from serialize()

   @todo take serialization_version into account

   @returns bool 0 = SUCCESS
  */
bool TriggerObj::materialize(uint serialization_version,
                             const String *serialization)
{
  DBUG_ENTER("TriggerObj::materialize()");
  m_create_stmt.copy(*serialization);
  DBUG_RETURN(0);
}

/**
   Create the object.
   
   This method uses serialization string in a query and executes it.

   @param[in]  thd  current thread

   @returns bool 0 = SUCCESS
  */
bool TriggerObj::execute(THD *thd)
{
  DBUG_ENTER("TriggerObj::execute()");
  drop(thd);
  DBUG_RETURN(execute_with_ctx(thd, &m_create_stmt, false));
}

/**
   Drops the object.

   This method calls the silent_exec method to execute the query.

   @note This uses "IF EXISTS" and does not return error if
         object does not exist.

   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @returns bool 0 = SUCCESS
  */
bool TriggerObj::drop(THD *thd)
{
  DBUG_ENTER("TriggerObj::drop()");
  DBUG_RETURN(drop_object(thd, "TRIGGER", &m_db_name, &m_trigger_name));
}

/*
 Implementation: StoredProcObj class.
*/
StoredProcObj::StoredProcObj(const String *db_name,
                             const String *stored_proc_name)
{
  // copy strings to newly allocated memory
  m_db_name.copy(*db_name);
  m_stored_proc_name.copy(*stored_proc_name);
}

/**
   serialize the object

   This method produces the data necessary for materializing the object
   on restore (creates object). 
   
   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @note this method will return an error if the db_name is either
         mysql or information_schema as these are not objects that 
         should be recreated using this interface.

   @returns bool 0 = SUCCESS
  */
bool StoredProcObj::serialize(THD *thd, String *serialization)
{
  bool ret= false;
  DBUG_ENTER("StoredProcObj::serialize()");
  DBUG_PRINT("StoredProcObj::serialize", ("name: %s in %s", 
             m_stored_proc_name.c_ptr(), m_db_name.c_ptr()));
  serialization->length(0);
  ret= serialize_routine(thd, TYPE_ENUM_PROCEDURE, m_db_name,
                         m_stored_proc_name, serialization);
  DBUG_RETURN(ret);
}

/**
   Materialize the serialization string.

   This method saves serialization string into a member variable.

   @param[in]  serialization_version   version number of this interface
   @param[in]  serialization           the string from serialize()

   @todo take serialization_version into account

   @returns bool 0 = SUCCESS
  */
bool StoredProcObj::materialize(uint serialization_version,
                             const String *serialization)
{
  DBUG_ENTER("StoredProcObj::materialize()");
  m_create_stmt.copy(*serialization);
  DBUG_RETURN(0);
}

/**
   Create the object.
   
   This method uses serialization string in a query and executes it.

   @param[in]  thd  current thread

   @returns bool 0 = SUCCESS
  */
bool StoredProcObj::execute(THD *thd)
{
  DBUG_ENTER("StoredProcObj::execute()");
  drop(thd);
  DBUG_RETURN(execute_with_ctx(thd, &m_create_stmt, false));
}

/**
   Drops the object.

   This method calls the silent_exec method to execute the query.

   @note This uses "IF EXISTS" and does not return error if
         object does not exist.

   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @returns bool 0 = SUCCESS
  */
bool StoredProcObj::drop(THD *thd)
{
  DBUG_ENTER("StoredProcObj::drop()");
  DBUG_RETURN(drop_object(thd, "PROCEDURE", &m_db_name, &m_stored_proc_name));
}

/*
 Implementation: StoredFuncObj class.
*/
StoredFuncObj::StoredFuncObj(const String *db_name,
                             const String *stored_func_name)
{
  // copy strings to newly allocated memory
  m_db_name.copy(*db_name);
  m_stored_func_name.copy(*stored_func_name); 
}

/**
   serialize the object

   This method produces the data necessary for materializing the object
   on restore (creates object). 
   
   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @note this method will return an error if the db_name is either
         mysql or information_schema as these are not objects that 
         should be recreated using this interface.

   @returns bool 0 = SUCCESS
  */
bool StoredFuncObj::serialize(THD *thd, String *serialization)
{
  bool ret= false;
  DBUG_ENTER("StoredFuncObj::serialize()");
  DBUG_PRINT("StoredProcObj::serialize", ("name: %s in %s", 
             m_stored_func_name.c_ptr(), m_db_name.c_ptr()));
  serialization->length(0);
  ret= serialize_routine(thd, TYPE_ENUM_FUNCTION, m_db_name,
                         m_stored_func_name, serialization);
  DBUG_RETURN(ret);
}

/**
   Materialize the serialization string.

   This method saves serialization string into a member variable.

   @param[in]  serialization_version   version number of this interface
   @param[in]  serialization           the string from serialize()

   @todo take serialization_version into account

   @returns bool 0 = SUCCESS
  */
bool StoredFuncObj::materialize(uint serialization_version,
                             const String *serialization)
{
  DBUG_ENTER("StoredFuncObj::materialize()");
  m_create_stmt.copy(*serialization);
  DBUG_RETURN(0);
}

/**
   Create the object.
   
   This method uses serialization string in a query and executes it.

   @param[in]  thd  current thread

   @returns bool 0 = SUCCESS
  */
bool StoredFuncObj::execute(THD *thd)
{
  DBUG_ENTER("StoredFuncObj::execute()");
  drop(thd);
  DBUG_RETURN(execute_with_ctx(thd, &m_create_stmt, false));
}

/**
   Drops the object.

   This method calls the silent_exec method to execute the query.

   @note This uses "IF EXISTS" and does not return error if
         object does not exist.

   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @returns bool 0 = SUCCESS
  */
bool StoredFuncObj::drop(THD *thd)
{
  DBUG_ENTER("StoredFuncObj::drop()");
  DBUG_RETURN(drop_object(thd, "FUNCTION", &m_db_name, &m_stored_func_name));
}

/*
 Implementation: EventObj class.
*/
EventObj::EventObj(const String *db_name,
                   const String *event_name)
{
  // copy strings to newly allocated memory
  m_db_name.copy(*db_name); 
  m_event_name.copy(*event_name); 
}

/**
   serialize the object

   This method produces the data necessary for materializing the object
   on restore (creates object). 
   
   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @note this method will return an error if the db_name is either
         mysql or information_schema as these are not objects that 
         should be recreated using this interface.

   @returns bool 0 = SUCCESS
  */
bool EventObj::serialize(THD *thd, String *serialization)
{
  bool ret= false;
  Open_tables_state open_tables_backup;
  Event_timed et;
  LEX_STRING sql_mode;
  DBUG_ENTER("EventObj::serialize()");
  DBUG_PRINT("EventObj::serialize", ("name: %s.%s", m_db_name.c_ptr(),
             m_event_name.c_ptr()));

  serialization->length(0);
  Event_db_repository *db_repository= Events::get_db_repository();
  thd->reset_n_backup_open_tables_state(&open_tables_backup);
  LEX_STRING db;
  db.str= m_db_name.c_ptr();
  db.length= m_db_name.length();
  LEX_STRING ev;
  ev.str= m_event_name.c_ptr();
  ev.length= m_event_name.length();
  ret= db_repository->load_named_event(thd, db, ev, &et);
  thd->restore_backup_open_tables_state(&open_tables_backup);
  if (sys_var_thd_sql_mode::symbolic_mode_representation(thd, 
    et.sql_mode, &sql_mode))
    DBUG_RETURN(TRUE);
  if (!ret)
  {
    /*
      Prepend sql_mode command.
    */
    serialization->append("SET SQL_MODE = '");
    serialization->append(sql_mode.str);
    serialization->append("'; ");

    /* 
      append time zone information
    */
    serialization->append("SET TIME_ZONE = '");
    const String *tz= et.time_zone->get_name();
    serialization->append(tz->ptr());
    serialization->append("'; ");

    /*
      append character set client charset information
    */
    serialization->append("SET CHARACTER_SET_CLIENT = '");
    serialization->append(et.creation_ctx->get_client_cs()->csname);
    serialization->append("'; ");

    /*
      append collation_connection information
    */
    serialization->append("SET COLLATION_CONNECTION = '");
    serialization->append(et.creation_ctx->get_connection_cl()->name);
    serialization->append("'; ");

    /*
      append collation_connection information
    */
    serialization->append("SET COLLATION_DATABASE = '");
    serialization->append(et.creation_ctx->get_db_cl()->name);
    serialization->append("'; ");

    if (et.get_create_event(thd, serialization))
      DBUG_RETURN(0);
  }
  DBUG_RETURN(0);
}

/**
   Materialize the serialization string.

   This method saves serialization string into a member variable.

   @param[in]  serialization_version   version number of this interface
   @param[in]  serialization           the string from serialize()

   @todo take serialization_version into account

   @returns bool 0 = SUCCESS
  */
bool EventObj::materialize(uint serialization_version,
                             const String *serialization)
{
  DBUG_ENTER("EventObj::materialize()");
  m_create_stmt.copy(*serialization);
  DBUG_RETURN(0);
}

/**
   Create the object.
   
   This method uses serialization string in a query and executes it.

   @param[in]  thd  current thread

   @returns bool 0 = SUCCESS
  */
bool EventObj::execute(THD *thd)
{
  DBUG_ENTER("EventObj::execute()");
  drop(thd);
  DBUG_RETURN(execute_with_ctx(thd, &m_create_stmt, true));
}

/**
   Drops the object.

   This method calls the silent_exec method to execute the query.

   @note This uses "IF EXISTS" and does not return error if
         object does not exist.

   @param[in]  thd            current thread
   @param[out] serialization  the data needed to recreate this object

   @returns bool 0 = SUCCESS
  */
bool EventObj::drop(THD *thd)
{
  DBUG_ENTER("EventObj::drop()");
  DBUG_RETURN(drop_object(thd, "EVENT", &m_db_name, &m_event_name));
}

///////////////////////////////////////////////////////////////////////////

Obj *get_database(const String *db_name)
{
  return new DatabaseObj(db_name);
}

Obj *get_table(const String *db_name,
               const String *table_name)
{
  return new TableObj(db_name, table_name, false);
}

Obj *get_view(const String *db_name,
              const String *view_name)
{
  return new TableObj(db_name, view_name, true);
}

Obj *get_trigger(const String *db_name,
                 const String *trigger_name)
{
  return new TriggerObj(db_name, trigger_name);
}

Obj *get_stored_procedure(const String *db_name,
                          const String *sp_name)
{
  return new StoredProcObj(db_name, sp_name);
}

Obj *get_stored_function(const String *db_name,
                         const String *sf_name)
{
  return new StoredFuncObj(db_name, sf_name);
}

Obj *get_event(const String *db_name,
               const String *event_name)
{
  return new EventObj(db_name, event_name);
}

///////////////////////////////////////////////////////////////////////////

Obj *materialize_database(const String *db_name,
                          uint serialization_version,
                          const String *serialialization)
{
  Obj *obj= new DatabaseObj(db_name);
  obj->materialize(serialization_version, serialialization);

  return obj;
}

Obj *materialize_table(const String *db_name,
                       const String *table_name,
                       uint serialization_version,
                       const String *serialialization)
{
  Obj *obj= new TableObj(db_name, table_name, false);
  obj->materialize(serialization_version, serialialization);

  return obj;
}

Obj *materialize_view(const String *db_name,
                      const String *view_name,
                      uint serialization_version,
                      const String *serialialization)
{
  Obj *obj= new TableObj(db_name, view_name, true);
  obj->materialize(serialization_version, serialialization);

  return obj;
}

Obj *materialize_trigger(const String *db_name,
                         const String *trigger_name,
                         uint serialization_version,
                         const String *serialialization)
{
  Obj *obj= new TriggerObj(db_name, trigger_name);
  obj->materialize(serialization_version, serialialization);

  return obj;
}

Obj *materialize_stored_procedure(const String *db_name,
                                  const String *stored_proc_name,
                                  uint serialization_version,
                                  const String *serialialization)
{
  Obj *obj= new StoredProcObj(db_name, stored_proc_name);
  obj->materialize(serialization_version, serialialization);

  return obj;
}

Obj *materialize_stored_function(const String *db_name,
                                 const String *stored_func_name,
                                 uint serialization_version,
                                 const String *serialialization)
{
  Obj *obj= new StoredFuncObj(db_name, stored_func_name);
  obj->materialize(serialization_version, serialialization);

  return obj;
}

Obj *materialize_event(const String *db_name,
                       const String *event_name,
                       uint serialization_version,
                       const String *serialialization)
{
  Obj *obj= new EventObj(db_name, event_name);
  obj->materialize(serialization_version, serialialization);

  return obj;
}

///////////////////////////////////////////////////////////////////////////

//
// Implementation: DDL Blocker.
//

///////////////////////////////////////////////////////////////////////////

/*
  DDL Blocker methods
*/

/**
   Turn on the ddl blocker

   This method is used to start the ddl blocker blocking DDL commands.

   @param[in] thd  current thread

   @retval my_bool success = TRUE, error = FALSE
  */
bool ddl_blocker_enable(THD *thd)
{
  DBUG_ENTER("ddl_blocker_enable()");
  if (!DDL_blocker->block_DDL(thd))
    DBUG_RETURN(FALSE);
  DBUG_RETURN(TRUE);
}

/**
   Turn off the ddl blocker

   This method is used to stop the ddl blocker from blocking DDL commands.
  */
void ddl_blocker_disable()
{
  DBUG_ENTER("ddl_blocker_disable()");
  DDL_blocker->unblock_DDL();
  DBUG_VOID_RETURN;
}

/**
   Turn on the ddl blocker exception

   This method is used to allow the exception allowing a restore operation to
   perform DDL operations while the ddl blocker blocking DDL commands.

   @param[in] thd  current thread
  */
void ddl_blocker_exception_on(THD *thd)
{
  DBUG_ENTER("ddl_blocker_exception_on()");
  thd->DDL_exception= TRUE;
  DBUG_VOID_RETURN;
}

/**
   Turn off the ddl blocker exception

   This method is used to suspend the exception allowing a restore operation to
   perform DDL operations while the ddl blocker blocking DDL commands.

   @param[in] thd  current thread
  */
void ddl_blocker_exception_off(THD *thd)
{
  DBUG_ENTER("ddl_blocker_exception_off()");
  thd->DDL_exception= FALSE;
  DBUG_VOID_RETURN;
}


///////////////////////////////////////////////////////////////////////////

#if 0


class DbTriggerIterator : public ObjIterator
{
public:
  DbTriggerIterator(const LEX_STRING db_name);

publbic:
  virtual TriggerObj *next();
};

class DbStoredProcIterator : public ObjIterator
{
public:
  DbStoredProcIterator(const LEX_STRING db_name);

public:
  virtual StoredProcObj *next();
};

class DbStoredFuncIterator : public ObjIterator
{
public:
  DbStoredFuncIterator(const LEX_STRING db_name);

public:
  virtual StoredFuncObj *next();
};

class DbEventIterator : public ObjIterator
{
public:
  DbEventbIterator(const LEX_STRING db_name);

public:
  virtual EventObj *next();
};

///////////////////////////////////////////////////////////////////////////

#endif

} // obs namespace

///////////////////////////////////////////////////////////////////////////
