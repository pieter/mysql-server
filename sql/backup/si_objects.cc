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

//
// Implementation: iterator impl classes.
//

///////////////////////////////////////////////////////////////////////////

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

//
// Implementation: object impl classes.
//

///////////////////////////////////////////////////////////////////////////

class DatabaseObj : public Obj
{
public:
  DatabaseObj(const LEX_STRING db_name);

public:
  virtual bool serialize(THD *thd, String *serialialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialialization);

  virtual bool execute();

private:
  // These attributes are to be used only for serialialization.
  LEX_STRING m_db_name;

private:
  // These attributes are to be used only for materialization.
  LEX_STRING m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

class TableObj : public Obj
{
public:
  TableObj(const LEX_STRING db_name,
           const LEX_STRING table_name,
           bool table_is_view);

public:
  virtual bool serialize(THD *thd, String *serialialization);

  virtual bool materialize(uint serialization_version,
                           const String *serialialization);

  virtual bool execute();

private:
  // These attributes are to be used only for serialialization.
  LEX_STRING m_db_name;
  LEX_STRING m_table_name;
  bool m_table_is_view;

private:
  // These attributes are to be used only for materialization.
  LEX_STRING m_create_stmt;

private:
  bool serialize_table(THD *thd, String *serialialization);
  bool serialize_view(THD *thd, String *serialialization);
};

///////////////////////////////////////////////////////////////////////////

class TriggerObj : public Obj
{
public:
  TriggerObj(const LEX_STRING db_name,
             const LEX_STRING trigger_name);

public:
  virtual bool serialize(THD *thd, String *serialialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialialization);

  virtual bool execute();

private:
  // These attributes are to be used only for serialialization.
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
  virtual bool serialize(THD *thd, String *serialialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialialization);

  virtual bool execute();

private:
  // These attributes are to be used only for serialialization.
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
  virtual bool serialize(THD *thd, String *serialialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialialization);

  virtual bool execute();

private:
  // These attributes are to be used only for serialialization.
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
  virtual bool serialize(THD *thd, String *serialialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialialization);

  virtual bool execute();

private:
  // These attributes are to be used only for serialialization.
  LEX_STRING m_db_name;
  LEX_STRING m_event_name;

private:
  // These attributes are to be used only for materialization.
  LEX_STRING m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

//
// Implementation: DatabaseObj class.
//

///////////////////////////////////////////////////////////////////////////

DatabaseObj::DatabaseObj(const LEX_STRING db_name) :
  m_db_name(*db_name)
{
  // TODO: return an error for pseudo-databases|tables or "mysql".
}

bool DatabaseObj::serialize(THD *thd, String *serialialization)
{
  // XXX: this is a copy & paste of mysql_show_create_database().

  HA_CREATE_INFO create;

// debug  if (!my_strcasecmp(system_charset_info, m_db_name.str,
// debug                     INFORMATION_SCHEMA_NAME.str))
// debug  {
// debug    m_db_name.str= INFORMATION_SCHEMA_NAME.str;
// debug    create.default_table_charset= system_charset_info;
// debug  }
// debug  else

    if (check_db_dir_existence(m_db_name.str))
    {
      my_error(ER_BAD_DB_ERROR, MYF(0), m_db_name.str);
      return -1;
    }

    load_db_opt_by_name(thd, m_db_name.str, &create);

  serialialization->append(STRING_WITH_LEN("CREATE DATABASE "));
  append_identifier(thd, serialialization, m_db_name.str, m_db_name.length);

  if (create.default_table_charset)
  {
    serialialization->append(STRING_WITH_LEN(" DEFAULT CHARACTER SET "));
    serialialization->append(create.default_table_charset->csname);
    if (!(create.default_table_charset->state & MY_CS_PRIMARY))
    {
      serialialization->append(STRING_WITH_LEN(" COLLATE "));
      serialialization->append(create.default_table_charset->name);
    }
  }

  return 0;
}

bool DatabaseObj::materialize(uint serialization_version,
                             const String *serialialization)
{
  // XXX: take serialization_version into account.

  m_create_stmt= *serialialization;
}

bool DatabaseObj::execute()
{
  // TODO.
}

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

bool TableObj::serialize(THD *thd, String *serialialization)
{
  // XXX: this is a copy & paste from mysqld_show_create() and
  // store_create_info().

  // NOTE: we don't care about 1) privilege checking; 2) meta-data locking
  // here.

  // XXX: we don't add version-specific comment here. materialize()
  // function should know how to materialize the information.

  return m_table_is_view ?
         serialize_table(thd, serialialization) :
         serialize_view(thd, serialialization);
}

bool TableObj::serialize_table(THD *thd, String *serialialization)
{
  return 0;
}

bool TableObj::serialize_view(THD *thd, String *serialialization)
{
  return 0;
}

bool TableObj::materialize(uint serialization_version,
                          const String *serialialization)
{
  // XXX: take serialization_version into account.

  m_create_stmt= *serialialization;
}

bool TableObj::execute()
{
  // TODO.
}
#endif

///////////////////////////////////////////////////////////////////////////
