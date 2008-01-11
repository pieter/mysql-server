/**
   @file
 
   This file defines the API for the following object services:
     - getting CREATE statements for objects
     - generating GRANT statments for objects
     - enumerating objects
     - finding dependencies for objects
     - executor for SQL statments
     - wrappers for controlling the DDL Blocker

  The methods defined below are used to provide server functionality to
  and permitting an isolation layer for the client (caller).
 */ 

#include "mysql_priv.h"
#include "si_objects.h"
#include <ddl_blocker.h>
#include "sql_show.h"

extern DDL_blocker_class *DDL_blocker;

namespace obs {

/*
  Executes the SQL string passed.
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

  const char *ptr;
  ::mysql_parse(thd,thd->query,thd->query_length,&ptr);

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

/**
   @class DatabaseObj

   This class provides an abstraction to a database object for creation and
   capture of the creation data.  
  */
class DatabaseObj : public Obj
{
public:
  DatabaseObj(const LEX_STRING db_name) : m_db_name(db_name) {}

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialization);

  virtual bool execute(THD *thd);

private:
  // These attributes are to be used only for serialization.
  LEX_STRING m_db_name;

private:
  // These attributes are to be used only for materialization.
  String *m_create_stmt;
};

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
  DBUG_PRINT("DatabaseObj::serialize", ("name: %s", m_db_name.str));

  if ((my_strcasecmp(system_charset_info, m_db_name.str, 
      INFORMATION_SCHEMA_NAME.str) == 0) ||
      (my_strcasecmp(system_charset_info, m_db_name.str, "mysql") == 0))
  {
    DBUG_PRINT("backup",(" Skipping internal database %s", m_db_name.str));
    DBUG_RETURN(1);
  }
  create.default_table_charset= system_charset_info;

  if (check_db_dir_existence(m_db_name.str))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), m_db_name.str);
    return -1;
  }

  load_db_opt_by_name(thd, m_db_name.str, &create);

  serialization->append(STRING_WITH_LEN("CREATE DATABASE "));
  append_identifier(thd, serialization, m_db_name.str, m_db_name.length);

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
bool DatabaseObj::materialize(uint serialization_version,
                             const String *serialization)
{
  DBUG_ENTER("DatabaseObj::materialize()");
  m_create_stmt= (String *)serialization;
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
  DBUG_RETURN(silent_exec(thd, m_create_stmt));
}

Obj *get_database(const LEX_STRING db_name)
{
  DatabaseObj *dbo= new DatabaseObj(db_name);
  return dbo;
}

/**
   @class TableObj

   This class provides an abstraction to a table object for creation and
   capture of the creation data.  
  */
class TableObj : public Obj
{
public:
  TableObj(const LEX_STRING db_name,
           const LEX_STRING table_name,
           bool table_is_view) : 
    m_db_name(db_name), 
    m_table_name(table_name), 
    m_table_is_view(table_is_view) {}

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                           const String *serialization);

  virtual bool execute(THD *thd);

private:
  // These attributes are to be used only for serialization.
  LEX_STRING m_db_name;
  LEX_STRING m_table_name;
  bool m_table_is_view;

private:
  // These attributes are to be used only for materialization.
  String *m_create_stmt;

private:
  bool serialize_table(THD *thd, String *serialization);
  bool serialize_view(THD *thd, String *serialization);
};

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
  DBUG_ENTER("TableObj::serialize()");
  DBUG_PRINT("TableObj::serialize", ("name: %s@%s", m_db_name.str,
             m_table_name.str));

  DBUG_ASSERT(serialization);
  serialization->length(0);
  Table_ident *name_id= new Table_ident(m_table_name);
  name_id->db= m_db_name;

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
    DBUG_RETURN(-1);

  /*
    Setup view specific variables and settings
  */
  if (m_table_is_view)
  {
    table_list->view_db= m_db_name;
    serialization->set_charset(table_list->view_creation_ctx->get_client_cs());
  }

  /*
    Get the create statement and close up shop.
  */
  ret= m_table_is_view ? 
    view_store_create_info(thd, table_list, serialization) :
    store_create_info(thd, table_list, serialization, NULL);
  close_thread_tables(thd);

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
bool TableObj::materialize(uint serialization_version,
                             const String *serialization)
{
  DBUG_ENTER("TableObj::materialize()");
  m_create_stmt= (String *)serialization;
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
  DBUG_RETURN(silent_exec(thd, m_create_stmt));
}

Obj *get_table(const LEX_STRING db_name, const LEX_STRING table_name, bool view)
{
  TableObj *dbo= new TableObj(db_name, table_name, view);
  return dbo;
}

/*
  DDL Blocker methods
*/

/**
   Turn on the ddl blocker

   This method is used to start the ddl blocker blocking DDL commands.

   @param[in] thd  current thread

   @retval my_bool success = TRUE, error = FALSE
  */
my_bool ddl_blocker_enable(THD *thd)
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

} // obs namespace

//
// Implementation: iterator impl classes.
//

#if 0
class DatabaseListIterator : public ObjIterator
{
public:
  DatabaseIterator();

public:
  virtual DatabaseObj *next();
};

class DbTablesIterator : public ObjIterator
{
public:
  DbTablesIterator(const LEX_STRING db_name);

public:
  virtual TableObj *next();
};

class DbViewIterator : public ObjIterator
{
public:
  DbViewIterator(const LEX_STRING db_name);

public:
  virtual TableObj *next();
}

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
}

class DbEventIterator : public ObjIterator
{
public:
  DbEventbIterator(const LEX_STRING db_name);

public:
  virtual EventObj *next();
};

///////////////////////////////////////////////////////////////////////////

//
// Implementation: DatabaseIterator class.
//

///////////////////////////////////////////////////////////////////////////

ObjIterator *get_databases()
{
  return new DatabaseIterator();
}

///////////////////////////////////////////////////////////////////////////

class TriggerObj : public Obj
{
public:
  TriggerObj(const LEX_STRING db_name,
             const LEX_STRING trigger_name);

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialization);

  virtual bool execute();

private:
  // These attributes are to be used only for serialization.
  LEX_STRING m_db_name;
  LEX_STRING m_trigger_name;

private:
  // These attributes are to be used only for materialization.
  LEX_STRING m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

class StoredProcObj : public Obj
{
public:
  StoredProcObj(const LEX_STRING db_name,
                const LEX_STRING stored_proc_name);

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialization);

  virtual bool execute();

private:
  // These attributes are to be used only for serialization.
  LEX_STRING m_db_name;
  LEX_STRING m_stored_proc_name;

private:
  // These attributes are to be used only for materialization.
  LEX_STRING m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

class StoredFuncObj : public Obj
{
public:
  StoredFuncObj(const LEX_STRING db_name,
                const LEX_STRING stored_func_name);

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialization);

  virtual bool execute();

private:
  // These attributes are to be used only for serialization.
  LEX_STRING m_db_name;
  LEX_STRING m_stored_func_name;

private:
  // These attributes are to be used only for materialization.
  LEX_STRING m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

class EventObj : public Obj
{
public:
  EventObj(const LEX_STRING db_name,
           const LEX_STRING event_name);

public:
  virtual bool serialize(THD *thd, String *serialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialization);

  virtual bool execute();

private:
  // These attributes are to be used only for serialization.
  LEX_STRING m_db_name;
  LEX_STRING m_event_name;

private:
  // These attributes are to be used only for materialization.
  LEX_STRING m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

//
// Implementation: TableObj class.
//

///////////////////////////////////////////////////////////////////////////

TableObj::TableObj(const LEX_STRING db_name.
                   const LEX_STRING table_name,
                   bool table_is_view) :
  m_db_name(*db_name),
  m_table_name(*table_name),
  m_table_is_view(table_is_view)
{
  // TODO: return an error for pseudo-databases|tables or "mysql".
}

bool TableObj::serialize(THD *thd, String *serialization)
{
  // XXX: this is a copy & paste from mysqld_show_create() and
  // store_create_info().

  // NOTE: we don't care about 1) privilege checking; 2) meta-data locking
  // here.

  // XXX: we don't add version-specific comment here. materialize()
  // function should know how to materialize the information.

  return m_table_is_view ?
         serialize_table(thd, serialization) :
         serialize_view(thd, serialization);
}

bool TableObj::serialize_table(THD *thd, String *serialization)
{
  return 0;
}

bool TableObj::serialize_view(THD *thd, String *serialization)
{
  return 0;
}

bool TableObj::materialize(uint serialization_version,
                          const String *serialization)
{
  // XXX: take serialization_version into account.

  m_create_stmt= *serialization;
}

bool TableObj::execute()
{
  // TODO.
}
#endif

///////////////////////////////////////////////////////////////////////////
