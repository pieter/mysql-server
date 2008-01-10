#ifndef SI_OBJECTS_H_
#define SI_OBJECTS_H_

namespace obs {

///////////////////////////////////////////////////////////////////////////

//
// Public interface.
//

///////////////////////////////////////////////////////////////////////////

// TODO:
//  - document it (doxygen);
//  - merge with the existing comments.

class Obj
{
public:
  virtual bool serialize(THD *thd, String *serialialization) = 0;

  virtual bool materialize(uint serialization_version,
                          const String *serialialization) = 0;

  virtual bool execute() = 0;

public:
  virtual ~Obj()
  { }
};

///////////////////////////////////////////////////////////////////////////

class ObjIterator
{
public:
  virtual Obj *next() = 0;
  // User is responsible for destroying the returned object.

public:
  virtual ~ObjIterator()
  { }
};

///////////////////////////////////////////////////////////////////////////

// User is responsible for destroying the returned object.

Obj *get_database(const LEX_STRING db_name);
Obj *get_table(const LEX_STRING db_name, const LEX_STRING table_name);
Obj *get_view(const LEX_STRING db_name, const LEX_STRING view_name);
Obj *get_trigger(const LEX_STRING db_name, const LEX_STRING trigger_name);
Obj *get_stored_procedure(const LEX_STRING db_name, const LEX_STRING sp_name);
Obj *get_stored_function(const LEX_STRING db_name, const LEX_STRING sf_name);
Obj *get_event(const LEX_STRING db_name, const LEX_STRING event_name);

///////////////////////////////////////////////////////////////////////////

//
// Enumeration.
//

// User is responsible for destroying the returned iterator.

ObjIterator *get_databases();
ObjIterator *get_db_tables(const LEX_STRING db_name);
ObjIterator *get_db_views(const LEX_STRING db_name);
ObjIterator *get_db_triggers(const LEX_STRING db_name);
ObjIterator *get_db_stored_procedures(const LEX_STRING db_name);
ObjIterator *get_db_stored_functions(const LEX_STRING db_name);
ObjIterator *get_db_events(const LEX_STRING db_name);

///////////////////////////////////////////////////////////////////////////

//
// Dependecies.
//

ObjIterator* get_view_base_tables(const LEX_STRING db_name,
                                  const LEX_STRING view_name);

///////////////////////////////////////////////////////////////////////////

enum ObjectType
{
  OT_DATABASE,
  OT_TABLE,
  OT_VIEW,
  OT_TRIGGER,
  OT_STORED_PROCEDURE,
  OT_STORED_FUNCTION,
  OT_EVENT
};

Obj *create_object(ObjectType object_type,
                   const String *serialialization);

///////////////////////////////////////////////////////////////////////////


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

}

#endif // SI_OBJECTS_H_
