#ifndef SI_OBJECTS_H_
#define SI_OBJECTS_H_

/**
 @file
 
 This file defines the API for the following object services:
 
 - enumerating objects,
 - getting CREATE statements for different types of objects,
 - finding dependencies between objects,

 */ 

/*
  Object Service: Enumerating

  List various kinds of objects present in the server instance. The methods
  collect list of object names and store it in the provided DYNAMIC_ARRAY, 
  which consists of LEX_STRING entries.

*/


/**
  List all databases present in the instance.
  
  @param[out]  dbs  Pointer to an array where names should be stored.
 */ 
get_databases(DYNAMIC_ARRAY *dbs);

/**
  List all tables belonging to a given database.
  
 */ 
get_tables(LEX_STRING db, DYNAMIC_ARRAY *tables);
get_views(LEX_STRING db, DYNAMIC_ARRAY *views);
get_procedures(LEX_STRING db, DYNAMIC_ARRAY *procs);
get_functions(LEX_STRING db, DYNAMIC_ARRAY *funcs);
get_triggers(LEX_STRING db, DYNAMIC_ARRAY *triggers);

get_events(LEX_STRING db, DYNAMIC_ARRAY *events);
get_indexes(LEX_STRING db, LEX_STRING table, DYNAMIC_ARRAY *indexes);

get_constraints(LEX_STRING db, LEX_STRING table, DYNAMIC_ARRAY *constraints);


#endif /*SI_OBJECTS_H_*/
