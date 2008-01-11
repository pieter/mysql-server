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

TABLE *create_schema_table(THD *thd, TABLE_LIST *table_list); // defined in sql_show.cc
static TABLE* open_schema_table(THD *thd, ST_SCHEMA_TABLE *st);

namespace obs {
//
// Implementation: object impl classes.
//

///////////////////////////////////////////////////////////////////////////

class DatabaseObj : public Obj
{
public:
  DatabaseObj(const String *db_name);

public:
  virtual bool serialize(THD *thd, String *serialialization);

  virtual bool materialize(uint serialization_version,
                          const String *serialialization);

  virtual bool execute();
  
  const String* get_name()
  { return &m_db_name; } 

private:
  // These attributes are to be used only for serialialization.
  String m_db_name;

private:
  // These attributes are to be used only for materialization.
  LEX_STRING m_create_stmt;
};

///////////////////////////////////////////////////////////////////////////

class TableObj : public Obj
{
public:
  TableObj(const String *db_name,
           const String *table_name,
           bool table_is_view);

public:
  virtual bool serialize(THD *thd, String *serialialization);

  virtual bool materialize(uint serialization_version,
                           const String *serialialization);

  virtual bool execute();

  const String* get_name()
  { return &m_table_name; } 

private:
  // These attributes are to be used only for serialialization.
  String m_db_name;
  String m_table_name;
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
// Implementation: iterator impl classes.
//

///////////////////////////////////////////////////////////////////////////

class DatabaseIterator : public ObjIterator
{
public:
  DatabaseIterator(THD *thd);
  ~DatabaseIterator();

public:
  virtual DatabaseObj *next();
  
private:

  THD    *m_thd;
  TABLE  *is_schemata;
  handler *ha;
  my_bitmap_map *old_map;

  friend ObjIterator* get_databases(THD*);
};

class DbTablesIterator : public ObjIterator
{
public:
  DbTablesIterator(THD *thd, const String *db_name);
  ~DbTablesIterator();

public:
  virtual TableObj *next();

private:

  THD   *m_thd;
  String m_db_name;
  TABLE *is_tables;
  handler *ha;
  my_bitmap_map *old_map;
  
protected:

  bool enumerate_views;
  
  friend ObjIterator *get_db_tables(THD*, const String*);
};

class DbViewsIterator : public DbTablesIterator
{
public:
  DbViewsIterator(THD *thd, const String *db_name): DbTablesIterator(thd,db_name)
  { enumerate_views= TRUE; }

  friend ObjIterator *get_db_views(THD*, const String*);
};


//
// Implementation: DatabaseIterator class.
//

///////////////////////////////////////////////////////////////////////////

ObjIterator *get_databases(THD *thd)
{
  DatabaseIterator *it= new DatabaseIterator(thd);

  return it && it->is_valid ? it : NULL;
}

DatabaseIterator::DatabaseIterator(THD *thd):
  m_thd(thd), is_schemata(NULL)
{
  DBUG_ASSERT(m_thd);
  
  is_schemata = open_schema_table(m_thd, get_schema_table(SCH_SCHEMATA));

  if (!is_schemata)
    return;

  ha = is_schemata->file;

  if (!ha)
    return;

  old_map= dbug_tmp_use_all_columns(is_schemata, is_schemata->read_set);

  if (ha->ha_rnd_init(TRUE))
  {
    ha= NULL;
    return;
  }
  
  is_valid= TRUE;
}

DatabaseIterator::~DatabaseIterator()
{
  if (ha)
   ha->ha_rnd_end();

  DBUG_ASSERT(m_thd);
  
  if (is_schemata)
  {
    dbug_tmp_restore_column_map(is_schemata->read_set, old_map);
    free_tmp_table(m_thd, is_schemata);
  }
}

DatabaseObj* DatabaseIterator::next()
{
  if (!is_valid)
    return NULL;
    
  DBUG_ASSERT(ha);
  DBUG_ASSERT(is_schemata);
  
  if (ha->rnd_next(is_schemata->record[0]))
    return NULL;
    
  String name;

  is_schemata->field[1]->val_str(&name);

  DBUG_PRINT("DatabaseIterator::next", (" Found database %s", name.ptr()));

  return new DatabaseObj(&name);
}

//
// Implementation: DbTablesIterator class.
//

///////////////////////////////////////////////////////////////////////////

ObjIterator *get_db_tables(THD *thd, const String *db_name)
{
  DbTablesIterator *it= new DbTablesIterator(thd, db_name);

  return it && it->is_valid ? it : NULL;
}

ObjIterator *get_db_views(THD *thd, const String *db_name)
{
  DbViewsIterator *it= new DbViewsIterator(thd, db_name);

  return it && it->is_valid ? it : NULL;
}

DbTablesIterator::DbTablesIterator(THD *thd, const String *db_name):
  m_thd(thd), m_db_name(*db_name), 
  is_tables(NULL), ha(NULL), old_map(NULL),
  enumerate_views(FALSE)
{
  DBUG_ASSERT(m_thd);
  m_db_name.copy();
  
  is_tables = open_schema_table(m_thd, get_schema_table(SCH_TABLES));

  if (!is_tables)
    return;

  ha = is_tables->file;

  if (!ha)
    return;

  old_map= dbug_tmp_use_all_columns(is_tables, is_tables->read_set);

  if (ha->ha_rnd_init(TRUE))
  {
    ha= NULL;
    return;
  }
  
  is_valid= TRUE;
}

DbTablesIterator::~DbTablesIterator()
{
  if (ha)
   ha->ha_rnd_end();

  DBUG_ASSERT(m_thd);
  
  if (is_tables)
  {
    dbug_tmp_restore_column_map(is_tables->read_set, old_map);
    free_tmp_table(m_thd, is_tables);
  }
}

TableObj* DbTablesIterator::next()
{
  if (!is_valid)
    return NULL;
    
  DBUG_ASSERT(ha);
  DBUG_ASSERT(is_tables);
  
  while (!ha->rnd_next(is_tables->record[0]))
  {
    String table_name;
    String db_name;
    String type;

    is_tables->field[1]->val_str(&db_name);
    is_tables->field[2]->val_str(&table_name);
    is_tables->field[3]->val_str(&type);

    // skip tables not from the given database
    if (db_name != m_db_name)
      continue;
      
    // skip tables/views depending on enumerate_views flag
    if (type != ( enumerate_views ? String("VIEW", system_charset_info) :
                                    String("BASE TABLE", system_charset_info)) )
      continue;

    DBUG_PRINT("DbTablesIterator::next", (" Found table %s.%s", 
                                          db_name.ptr(), table_name.ptr()));
    return new TableObj(&db_name,&table_name,enumerate_views);
  }
  
  return NULL;
}

///////////////////////////////////////////////////////////////////////////

//
// Implementation: DatabaseObj class.
//

///////////////////////////////////////////////////////////////////////////

DatabaseObj::DatabaseObj(const String *db_name) :
  m_db_name(*db_name)
{
  m_db_name.copy(); // copy name string to newly allocated memory
}

bool DatabaseObj::serialize(THD *thd, String *serialialization)
{
  return FALSE;
}

bool DatabaseObj::materialize(uint serialization_version,
                             const String *serialialization)
{
  return FALSE;
}

bool DatabaseObj::execute()
{
  return FALSE;
}

///////////////////////////////////////////////////////////////////////////


//
// Implementation: TableObj class.
//

///////////////////////////////////////////////////////////////////////////

TableObj::TableObj(const String *db_name,
                   const String *table_name,
                   bool table_is_view) :
  m_db_name(*db_name),
  m_table_name(*table_name),
  m_table_is_view(table_is_view)
{
  m_db_name.copy();
  m_table_name.copy();
}

bool TableObj::serialize(THD *thd, String *serialialization)
{
  return FALSE;
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
  return 0;
}

bool TableObj::execute()
{
  return 0;
}


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


/// Open given table in @c INFORMATION_SCHEMA database.
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
