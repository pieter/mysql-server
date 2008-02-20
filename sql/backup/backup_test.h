#ifndef _BACKUP_TEST_H
#define _BACKUP_TEST_H

/*
  Called from the big switch in mysql_execute_command() to execute
  backup related demo
*/
int execute_backup_test_command(THD*, List<LEX_STRING> *);

#endif
