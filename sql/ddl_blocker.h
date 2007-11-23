/**
  @file

  Header file for DDL blocker code.
 */
#include "mysql_priv.h"
#include "debug.h"

/*
  Mutexes and condition variables -- see mysqld.cc.
*/
extern pthread_mutex_t THR_LOCK_DDL_blocker; 
extern pthread_mutex_t THR_LOCK_DDL_is_blocked; 
extern pthread_cond_t COND_DDL_blocker;
extern pthread_cond_t COND_process_blocked;

/*
  Increments the backup's counter to indicate a DDL is in progress.
*/
void start_DDL();

/*
  Decrements the backup's counter to indicate a DDL is done.
  Signals backup process if counter == 0.
*/
void end_DDL();

/*
  Check to see if we are blocked from continuing. If so,
  wait until the backup process signals the condition.
*/
my_bool check_DDL_blocker(THD *thd);

/*
  This method is used to block all DDL commands. It checks the counter
  DDL_blocks and if > 0 it blocks the backup until all DDL operations are
  complete and the condition variable has been signaled. 

  The method also sets the boolean DDL_blocked to TRUE to tell the DDL
  operations that they must block until the backup operation is complete.
*/
my_bool block_DDL(THD *thd);

/*
  This method is used to unblock all DDL commands. It sets the boolean
  DDL_blocked to FALSE to tell the DDL operations that they can proceed.
*/
void unblock_DDL();

