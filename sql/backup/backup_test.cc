/**
  @file

  Implementation of the backup test function.

  @todo Implement code to test service interface(s).
 */

#include "../mysql_priv.h"
#include "si_objects.h"
#include "backup_aux.h"

using namespace obs;

/**
   Call backup kernel API to execute backup related SQL statement.

   @param[in] thd  current thread
   @param[in] lex  results of parsing the statement.
  */
int execute_backup_test_command(THD *thd, List<LEX_STRING> *db_list)
{
  int res= 0; 
  DBUG_ENTER("execute_backup_command");
  DBUG_ASSERT(thd);

  List_iterator<LEX_STRING> dbl(*db_list);
  LEX_STRING *db_name;
  String serialized;
  String db_query;
  Obj *dbo;

  // Database object test
  while (db_name= dbl++)
  {
    serialized.length(0);
    dbo= get_database(*db_name);
    dbo->serialize(thd, &serialized);
    printf("serialized string for database %s:\n%s\n", db_name->str, serialized.c_ptr());
    db_query.length(0);
    db_query.append("DROP DATABASE ");
    db_query.append(db_name->str);
    backup::silent_exec_query(thd, db_query);
    printf("database %s dropped.\n", db_name->str);
    dbo->materialize(0, &serialized);
    dbo->execute(thd);
    printf("database created.\n");
  }

  // Table object test
  serialized.length(0);
  LEX_STRING db, tbl;
  db.str= new char[128];
  memcpy(db.str, "sakila", 6);
  db.str[6]= 0;
  db.length= 6;
  tbl.str= new char[128];
  memcpy(tbl.str, "store", 5);
  tbl.str[5]= 0;
  tbl.length= 5;
  dbo= get_table(db, tbl, FALSE);
  dbo->serialize(thd, &serialized);
  printf("serialized string for table %s.%s:\n%s\n", db.str, tbl.str, serialized.c_ptr());
  
  delete db.str;
  delete tbl.str;
  send_ok(thd);
  DBUG_RETURN(res);
}

