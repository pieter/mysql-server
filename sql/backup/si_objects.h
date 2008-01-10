#ifndef SI_OBJECTS_H_
#define SI_OBJECTS_H_

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

/*
  Object Service: Create info methods

  Provide create info for different objects

  Assumptions:
    - We get the stings in UTF8.

  Open issues:
    - What type should the string be (LEX_STRING or char*)?
    - Should get_create_info_table have substrings for constraints and index?
    - What to do about patterns in grants?

  Comments:
    - get_create_info_table and get_create_info_view use same code
    - get_create_info_procedure, get_create_info_function use same code
*/

/**
   Get the CREATE statement for a table.
   
   This method generates a SQL statement that permits the creation of a
   table.

   @param[in]  LEX_STRING db      database name
   @param[in]  LEX_STRING table   table name
   @param[out] STRING *string     the SQL for the CREATE command

   @note Currently generates a SQL command like what is returned by 
         SHOW CREATE TABLE.

   @returns int 0 = success, error code if error.
  */
int get_create_info_table(LEX_STRING db,
                          LEX_STRING table,
                          String *string);

/**
   Get the CREATE statement for a view.
   
   This method generates a SQL statement that permits the creation of a
   view.

   @param[in]  LEX_STRING db      database name
   @param[in]  LEX_STRING view    table name
   @param[out] STRING *string     the SQL for the CREATE command
  
   @note Currently generates a SQL command like what is returned by 
         SHOW CREATE VIEW.

   @returns int 0 = success, error code if error.
  */
int get_create_info_view(LEX_STRING db,
                         LEX_STRING view,
                         String *string);

/**
   Get the CREATE statement for a database.
   
   This method generates a SQL statement that permits the creation of a
   database.

   @param[in]  LEX_STRING db      database name
   @param[out] STRING *string     the SQL for the CREATE command
  
   @note Currently generates a SQL command like what is returned by 
         SHOW CREATE DATABASE.

   @returns int 0 = success, error code if error.
  */
int get_create_info_database(LEX_STRING db,
                             String *string);

/**
   Get the CREATE statement for a stored procedure.
   
   This method generates a SQL statement that permits the creation of a
   procedure.

   @param[in]  LEX_STRING db      database name
   @param[in]  LEX_STRING proc    procedure name
   @param[out] STRING *string     the SQL for the CREATE command
  
   @note Currently generates a SQL command like what is returned by 
         SHOW CREATE PROCEDURE.

   @returns int 0 = success, error code if error.
  */
int get_create_info_procedure(LEX_STRING db,
                              LEX_STRING proc,
                              String *string);

/**
   Get the CREATE statement for a function.
   
   This method generates a SQL statement that permits the creation of a
   function.

   @param[in]  LEX_STRING db      database name
   @param[in]  LEX_STRING func    function name
   @param[out] STRING *string     the SQL for the CREATE command
  
   @note Currently generates a SQL command like what is returned by 
         SHOW CREATE FUNCTION.

   @returns int 0 = success, error code if error.
  */
int get_create_info_function(LEX_STRING db,
                             LEX_STRING func,
                             String *string);

/**
   Get the CREATE statement for a trigger.
   
   This method generates a SQL statement that permits the creation of a
   trigger.

   @param[in]  LEX_STRING db      database name
   @param[in]  LEX_STRING trigger trigger name
   @param[out] STRING *string     the SQL for the CREATE command
  
   @note Currently generates a SQL command like what is returned by 
         SHOW CREATE TRIGGER.

   @returns int 0 = success, error code if error.
  */
int get_create_info_trigger(LEX_STRING db,
                            LEX_STRING trigger,
                            String *string);

/**
   Get the CREATE statement for an event.
   
   This method generates a SQL statement that permits the creation of a
   event.

   @param[in]  LEX_STRING db      database name
   @param[in]  LEX_STRING ev      event name
   @param[out] STRING *string     the SQL for the CREATE command
  
   @note Currently generates a SQL command like what is returned by 
         SHOW CREATE EVENT.

   @returns int 0 = success, error code if error.
  */
int get_create_info_event(LEX_STRING db,
                          LEX_STRING ev,
                          String *string);

/**
   Get the CREATE statement for an index.
   
   This method generates a CREATE INDEX command.

   @param[in]  LEX_STRING db      database name
   @param[in]  LEX_STRING table   table name
   @param[in]  LEX_STRING index   index name
   @param[out] STRING *string     the SQL for the CREATE command

   @Note There is no corresponding SHOW CREATE INDEX command.

   @returns int 0 = success, error code if error.
  */
int get_create_info_index(LEX_STRING db,
                          LEX_STRING table,
                          LEX_STRING index,
                          String *string);

/**
   Get the CREATE statement for a constraint.
   
   This method generates the ALTER TABLE statement that would create the
   constraint on the table.

   @param[in]  LEX_STRING db         database name
   @param[in]  LEX_STRING table      table name
   @param[in]  LEX_STRING constraint constraint name
   @param[out] STRING *string        the SQL for the CREATE command

   @Note There is no corresponding SHOW CREATE CONSTRAINT command.

   @returns int 0 = success, error code if error.
  */
int get_create_info_constraint(LEX_STRING db,
                               LEX_STRING table,
                               LEX_STRING constraint,
                               String *string);

/*
  Object Service: Enumerating

  List various kinds of objects present in the server instance. The methods
  collect list of object names and store it in the DYNAMIC_ARRAY provided by 
  the caller. The array consists of LEX_STRING entries and should be deallocated
  by the caller.
*/


/**
  List all databases present in the instance.
  
  @param[out]  dbs  array where names should be stored.
  
  @return 0 on success, error code if error.
 */
int get_databases(DYNAMIC_ARRAY *dbs);

/**
  List all tables belonging to a given database.
  
  @param[in]  db     database name
  @param[out] tables array where table names should be stored
 
  @note The returned list should contain only base tables, not views.  

  @return 0 on success, error code if error.
 */
int get_tables(LEX_STRING db, DYNAMIC_ARRAY *tables);

/**
  List all views belonging to a given database.
  
  @param[in]  db    database name
  @param[out] views array where view names should be stored

  @return 0 on success, error code if error.
 */
int get_views(LEX_STRING db, DYNAMIC_ARRAY *views);

/**
  List all stored procedures belonging to a given database.
  
  @param[in]  db    database name
  @param[out] procs array where procedure names should be stored

  @return 0 on success, error code if error.
 */
int get_procedures(LEX_STRING db, DYNAMIC_ARRAY *procs);

/**
  List all stored functions belonging to a given database.
  
  @param[in]  db    database name
  @param[out] funcs array where function names should be stored

  @return 0 on success, error code if error.
 */
int get_functions(LEX_STRING db, DYNAMIC_ARRAY *funcs);

/**
  List all triggers belonging to a given database.
  
  @param[in]  db       database name
  @param[out] triggers array where trigger names should be stored

  @return 0 on success, error code if error.
 */
int get_triggers(LEX_STRING db, DYNAMIC_ARRAY *triggers);

/**
  List all events belonging to a given database.
  
  @param[in]  db      database name
  @param[out] events  array where event names should be stored

  @return 0 on success, error code if error.
 */
int get_events(LEX_STRING db, DYNAMIC_ARRAY *events);

/**
  List indexes of a given tbale.
  
  @param[in]  db      database name
  @param[in]  table   table name
  @param[out] indexes array where index names should be stored

  @return 0 on success, error code if error.
 */
int get_indexes(LEX_STRING db, LEX_STRING table, DYNAMIC_ARRAY *indexes);

/**
  List constraints defined in a given tbale.
  
  This should list names of all constraints explicitly created using CONSTRAINT
  clauses.
  
  @param[in]  db          database name
  @param[in]  table       table name
  @param[out] constraints array where constraint names should be stored

  @return 0 on success, error code if error.
 */
int get_constraints(LEX_STRING db, LEX_STRING table, DYNAMIC_ARRAY *constraints);

/*
 Object service: Dependencies for views

 Notes:
 
 - The functions only return the direct dependencies, they do not calculate the 
   transitive closure.
 - One needs to call both functions to get the complete list of views and tables.
*/

/**
  Return list of tables used by a view.
  
  @param[in]  db     database name
  @param[in]  view   view name
  @param[out] tables array where table names should be stored
  
  @note Only base tables are listed, not views.
  
  @return 0 on success, error code if error.
 */ 
int get_underlying_tables(LEX_STRING db, LEX_STRING view, DYNAMIC_ARRAY *tables);

/**
  Return list of other views used by a given view.
  
  @param[in]  db     database name
  @param[in]  view   view name
  @param[out] views  array where view names should be stored
  
  @return 0 on success, error code if error.
 */ 
int get_underlying_views(LEX_STRING db, LEX_STRING view, DYNAMIC_ARRAY *views);

/*
 Object service: Executor (mainly for create/drop)

 Notes:
 - This will probably be improved in the future
 - We need to ensure that error handling from mysql_parse is handled
   correctly.
*/

/*
  Execute given SQL statement ignoring the result set (if any).
  
  @param[in,out]  thd     execution context
  @param[in]      stmt    SQL statement to execute 
 
  @return 0 on success, error code if error.
 */ 
int execute_sql(THD *thd, LEX_STRING stmt);

#endif /*SI_OBJECTS_H_*/
