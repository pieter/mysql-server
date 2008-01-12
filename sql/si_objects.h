#ifndef SI_OBJECTS_H_
#define SI_OBJECTS_H_

/**
   @file

   This file defines the API for the following object services:
     - serialize database objects into a string;
     - materialize (deserialize) object from a string;
     - enumerating objects;
     - finding dependencies for objects;
     - executor for SQL statements;
     - wrappers for controlling the DDL Blocker;
*/

namespace obs {

/**
  Obj defines the basic set of operations for each database object.
*/

class Obj { public:
  /**
    Serialize object state into a buffer. The buffer actually should be a
    binary buffer. String class is used here just because we don't have
    convenient primitive for binary buffers.

    Serialization format is opaque to the client, i.e. the client should
    not make any assumptions about the format or the content of the
    returned buffer.

    Serialization format can be changed in the future versions. However,
    the server must be able to materialize objects coded in any previous
    formats.

    @param[in] thd              Server thread context.
    @param[in] serialialization Buffer to serialize the object

    @return error status.
      @retval FALSE on success.
      @retval TRUE on error.
  */
  virtual bool serialize(THD *thd, String *serialialization) = 0;


  /**
    Return the name of the object.

    @return object name.
  */
  virtual const String *get_name() = 0;

  /**
    Return the database name of the object.

    @note this is a subject to remove.
  */
  virtual const String *get_db_name() = 0;

  /**
    Create the object in the database.

    @param[in] thd              Server thread context.

    @return error status.
      @retval FALSE on success.
      @retval TRUE on error.
  */
  virtual bool execute(THD *thd) = 0;

public:
  virtual ~Obj()
  { }

private:
  /**
    Read the object state from a given buffer and restores object state to
    the point, where it can be executed.

    @param[in] serialialization_version The version of the serialization format.
    @param[in] serialialization         Buffer contained serialized object.

    @return error status.
      @retval FALSE on success.
      @retval TRUE on error.
  */
  virtual bool materialize(uint serialization_version,
                           const String *serialialization) = 0;

private:
  friend Obj *materialize_database(const String *,
                                   uint,
                                   const String *);

  friend Obj *materialize_table(const String *,
                                const String *,
                                uint,
                                const String *);

  friend Obj *materialize_view(const String *,
                               const String *,
                               uint,
                               const String *);

  friend Obj *materialize_trigger(const String *,
                                  const String *,
                                  uint,
                                  const String *);

  friend Obj *materialize_stored_procedure(const String *,
                                           const String *,
                                           uint,
                                           const String *);

  friend Obj *materialize_stored_function(const String *,
                                          const String *,
                                          uint,
                                          const String *);
};

///////////////////////////////////////////////////////////////////////////

/**
  ObjIterator is a basic interface to enumerate the objects.
*/

class ObjIterator
{
public:

  ObjIterator()
  { }

  /**
    This operation returns a pointer to the next object in an enumeration.
    It returns NULL if there is no more objects.

    The client is responsible to destroy the returned object.

    @return a pointer to the object
      @retval NULL if there is no more objects in an enumeration.
  */
  virtual Obj *next() = 0;

public:
  virtual ~ObjIterator()
  { }

};

///////////////////////////////////////////////////////////////////////////

// The functions in this section are intended to construct an instance of
// Obj class for any particular database object. These functions do not
// interact with the server to validate requested names. So, it is possible
// to construct instances for non-existing objects.
//
// The client is responsible for destroying the returned object.

/**
  Construct an instance of Obj representing a database.

  No actual actions are performed in the server. An object can be created
  even for invalid database name or for non-existing database. One check however
  is performed: an object can not be created for the system database
  (mysql) and for pseudo-databases like INFORMATION_SCHEMA or
  PERFORMANCE_SCHEMA.

  The client is responsible to destroy the created object.

  @param[in] db_name Database name.

  @return a pointer to an instance of Obj representing given database.
    @retval NULL if db_name specifies the system database or the
            pseudo-database.
*/

Obj *get_database(const LEX_STRING db_name);

/**
  Construct an instance of Obj representing a table.

  No actual actions are performed in the server. An object can be created
  even for invalid database/table name or for non-existing table. One check
  however is performed: an object can not be created for a table in the
  system database (mysql) or in pseudo-databases like INFORMATION_SCHEMA or
  PERFORMANCE_SCHEMA.

  The client is responsible to destroy the created object.

  @param[in] db_name    Database name.
  @param[in] table_name Table name.

  @return a pointer to an instance of Obj representing given table.
    @retval NULL if db_name specifies the system database or the
            pseudo-database.
*/

Obj *get_table(const LEX_STRING db_name, const LEX_STRING table_name);

/**
  Construct an instance of Obj representing a view.

  No actual actions are performed in the server. An object can be created
  even for invalid database/view name or for non-existing view. One check
  however is performed: an object can not be created for a view in the
  system database (mysql) or in pseudo-databases like INFORMATION_SCHEMA or
  PERFORMANCE_SCHEMA.

  The client is responsible to destroy the created object.

  @param[in] db_name   Database name.
  @param[in] view_name View name.

  @return a pointer to an instance of Obj representing given view.
    @retval NULL if db_name specifies the system database or the
            pseudo-database.
*/

Obj *get_view(const LEX_STRING db_name, const LEX_STRING view_name);

/**
  Construct an instance of Obj representing a trigger.

  No actual actions are performed in the server. An object can be created
  even for invalid database/trigger name or for non-existing trigger. One
  check however is performed: an object can not be created for a trigger in
  the system database (mysql) or in pseudo-databases like
  INFORMATION_SCHEMA or PERFORMANCE_SCHEMA.

  The client is responsible to destroy the created object.

  @param[in] db_name      Database name.
  @param[in] trigger_name Trigger name.

  @return a pointer to an instance of Obj representing given trigger.
    @retval NULL if db_name specifies the system database or the
            pseudo-database.
*/

Obj *get_trigger(const LEX_STRING db_name, const LEX_STRING trigger_name);

/**
  Construct an instance of Obj representing a stored procedure.

  No actual actions are performed in the server. An object can be created
  even for invalid database/procedure name or for non-existing stored
  procedure. One check however is performed: an object can not be created
  for a stored procedure in the system database (mysql) or in
  pseudo-databases like INFORMATION_SCHEMA or PERFORMANCE_SCHEMA.

  The client is responsible to destroy the created object.

  @param[in] db_name Database name.
  @param[in] sp_name Stored procedure name.

  @return a pointer to an instance of Obj representing given stored
  procedure.
    @retval NULL if db_name specifies the system database or the
            pseudo-database.
*/

Obj *get_stored_procedure(const LEX_STRING db_name, const LEX_STRING sp_name);

/**
  Construct an instance of Obj representing a stored function.

  No actual actions are performed in the server. An object can be created
  even for invalid database/function name or for non-existing stored
  function. One check however is performed: an object can not be created
  for a stored function in the system database (mysql) or in
  pseudo-databases like INFORMATION_SCHEMA or PERFORMANCE_SCHEMA.

  The client is responsible to destroy the created object.

  @param[in] db_name Database name.
  @param[in] sf_name Stored function name.

  @return a pointer to an instance of Obj representing given stored
  function.
    @retval NULL if db_name specifies the system database or the
            pseudo-database.
*/

Obj *get_stored_function(const LEX_STRING db_name, const LEX_STRING sf_name);

/**
  Construct an instance of Obj representing an event.

  No actual actions are performed in the server. An object can be created
  even for invalid database/event name or for non-existing event.  One
  check however is performed: an object can not be created for a event in
  the system database (mysql) or in pseudo-databases like
  INFORMATION_SCHEMA or PERFORMANCE_SCHEMA.

  The client is responsible to destroy the created object.

  @param[in] db_name    Database name.
  @param[in] event_name Event name.

  @return a pointer to an instance of Obj representing given event.
    @retval NULL if db_name specifies the system database or the
            pseudo-database.
*/

Obj *get_event(const LEX_STRING db_name, const LEX_STRING event_name);

///////////////////////////////////////////////////////////////////////////

// The functions in this section provides a way to iterator over all
// objects in the server or in the particular database.
//
// The client is responsible for destroying the returned iterator.

/**
  Create an iterator over all databases in the server excluding the system
  database (mysql) and pseudo-databases like INFORMATION_SCHEMA and
  PERFORMANCE_SCHEMA.

  The client is responsible to destroy the returned iterator.

  @return a pointer to an iterator object.
*/

ObjIterator *get_databases(THD *thd);

/**
  Create an iterator over all tables in the particular database.

  The client is responsible to destroy the returned iterator.

  @return a pointer to an iterator object.
    @retval NULL if db_name specifies the system database or
            a pseudo-database.
*/

ObjIterator *get_db_tables(THD *thd, const String *db_name);

/**
  Create an iterator over all views in the particular database.

  The client is responsible to destroy the returned iterator.

  @return a pointer to an iterator object.
    @retval NULL if db_name specifies the system database or
            a pseudo-database.
*/

ObjIterator *get_db_views(THD *thd, const String *db_name);

/**
  Create an iterator over all triggers in the particular database.

  The client is responsible to destroy the returned iterator.

  @return a pointer to an iterator object.
    @retval NULL if db_name specifies the system database or
            a pseudo-database.
*/

ObjIterator *get_db_triggers(const LEX_STRING db_name);

/**
  Create an iterator over all stored procedures in the particular database.

  The client is responsible to destroy the returned iterator.

  @return a pointer to an iterator object.
    @retval NULL if db_name specifies the system database or
            a pseudo-database.
*/

ObjIterator *get_db_stored_procedures(const LEX_STRING db_name);

/**
  Create an iterator over all stored functions in the particular database.

  The client is responsible to destroy the returned iterator.

  @return a pointer to an iterator object.
    @retval NULL if db_name specifies the system database or
            a pseudo-database.
*/

ObjIterator *get_db_stored_functions(const LEX_STRING db_name);

/**
  Create an iterator over all events in the particular database.

  The client is responsible to destroy the returned iterator.

  @return a pointer to an iterator object.
    @retval NULL if db_name specifies the system database or
            a pseudo-database.
*/

ObjIterator *get_db_events(const LEX_STRING db_name);

///////////////////////////////////////////////////////////////////////////

// The functions are intended to enumerate dependent objects.
//
// The client is responsible for destroying the returned iterator.

/**
  Create an iterator overl all base tables in the particular view.

  The client is responsible to destroy the returned iterator.

  @return a pointer to an iterator object.
    @retval NULL if db_name specifies the system database or
            a pseudo-database.
*/

ObjIterator* get_view_base_tables(THD *thd,
                                  const String *db_name,
                                  const String *view_name);

/**
  Create an iterator overl all base tables in the particular view.

  The client is responsible to destroy the returned iterator.

  @return a pointer to an iterator object.
    @retval NULL if db_name specifies the system database or
            a pseudo-database.
*/

ObjIterator* get_view_base_views(THD *thd,
                                 const String *db_name,
                                 const String *view_name);

///////////////////////////////////////////////////////////////////////////

// The functions in this section provides a way to materialize objects from
// the serialized form.
//
// The client is responsible for destroying the returned iterator.

Obj *materialize_database(const String *db_name,
                          uint serialization_version,
                          const String *serialialization);

Obj *materialize_table(const String *db_name,
                       const String *table_name,
                       uint serialization_version,
                       const String *serialialization);

Obj *materialize_view(const String *db_name,
                      const String *view_name,
                      uint serialization_version,
                      const String *serialialization);

Obj *materialize_trigger(const String *db_name,
                         const String *trigger_name,
                         uint serialization_version,
                         const String *serialialization);

Obj *materialize_stored_procedure(const String *db_name,
                                  const String *stored_proc_name,
                                  uint serialization_version,
                                  const String *serialialization);

Obj *materialize_stored_function(const String *db_name,
                                 const String *stored_func_name,
                                 uint serialization_version,
                                 const String *serialialization);

Obj *materialize_event(const String *db_name,
                       const String *event_name,
                       uint serialization_version,
                       const String *serialialization);

///////////////////////////////////////////////////////////////////////////

//
// DDL blocker methods.
//

/**
  Turn on the ddl blocker.

  This method is used to start the ddl blocker blocking DDL commands.

  @param[in] thd  Thread context.

  @return error status.
    @retval FALSE on success.
    @retval TRUE on error.
*/
bool ddl_blocker_enable(THD *thd);

/**
  Turn off the ddl blocker.

  This method is used to stop the ddl blocker from blocking DDL commands.
*/
void ddl_blocker_disable();

/**
  Turn on the ddl blocker exception

  This method is used to allow the exception allowing a restore operation to
  perform DDL operations while the ddl blocker blocking DDL commands.

  @param[in] thd  Thread context.
*/
void ddl_blocker_exception_on(THD *thd);

/**
  Turn off the ddl blocker exception.

  This method is used to suspend the exception allowing a restore operation to
  perform DDL operations while the ddl blocker blocking DDL commands.

  @param[in] thd  Thread context.
*/
void ddl_blocker_exception_off(THD *thd);

} // obs namespace

#endif // SI_OBJECTS_H_
