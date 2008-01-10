/**
  @file

  Implementation of the backup test function.

  @todo Implement code to test service interface(s).
 */

#include "../mysql_priv.h"

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
  send_ok(thd);
  DBUG_RETURN(res);
}

