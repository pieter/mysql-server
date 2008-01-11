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

  virtual bool execute(THD *thd) = 0;

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

/*
  DDL blocker methods.
*/

/**
   Turn on the ddl blocker

   This method is used to start the ddl blocker blocking DDL commands.

   @param[in] thd  current thread

   @retval my_bool success = TRUE, error = FALSE
  */
my_bool ddl_blocker_enable(THD *thd); 

/**
   Turn off the ddl blocker

   This method is used to stop the ddl blocker from blocking DDL commands.
  */
void ddl_blocker_disable(); 

/**
   Turn on the ddl blocker exception

   This method is used to allow the exception allowing a restore operation to
   perform DDL operations while the ddl blocker blocking DDL commands.

   @param[in] thd  current thread
  */
void ddl_blocker_exception_on(THD *thd);

/**
   Turn off the ddl blocker exception

   This method is used to suspend the exception allowing a restore operation to
   perform DDL operations while the ddl blocker blocking DDL commands.

   @param[in] thd  current thread
  */
void ddl_blocker_exception_off(THD *thd);

}

#endif // SI_OBJECTS_H_
