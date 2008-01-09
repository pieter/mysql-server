#ifndef SI_OBJECTS_H_
#define SI_OBJECTS_H_

/**
   @file si_objects.h
 
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


#endif /*SI_OBJECTS_H_*/
