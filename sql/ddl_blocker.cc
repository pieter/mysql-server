/* Copyright (C) 2004-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, 
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

/**
   @file

   @brief Contains methods to implement a basic DDL blocker.

   This file contains methods that allow DDL statements to register and 
   another process (such as backup) to check to see if there are any DDL
   statements running and block or exit if so. 

   It also allows a process (such as backup) to register itself to block
   all DDL methods until the process is complete.
  */

#include "ddl_blocker.h"

my_bool DDL_blocked= FALSE;   ///< Assume DDL is not blocked.
int DDL_blocks= 0;            ///< Number of DDL operations in progress.

/**
   start_DDL()

   Increments the DDL_blocks counter to indicate a DDL is in progress.
  */
void start_DDL()
{
  DBUG_ENTER("start_DDL()");
  pthread_mutex_lock(&THR_LOCK_DDL_blocker);
  DDL_blocks++;
  pthread_mutex_unlock(&THR_LOCK_DDL_blocker);
  DBUG_VOID_RETURN;
}

/**
   end_DDL()

   Decrements the DDL_blocks counter to indicate a DDL is done.
   Signals blocked process if counter == 0.
  */
void end_DDL()
{
  DBUG_ENTER("end_DDL()");
  pthread_mutex_lock(&THR_LOCK_DDL_blocker);
  if (DDL_blocks > 0)
    DDL_blocks--;
  if (DDL_blocks == 0)
    pthread_cond_broadcast(&COND_process_blocked);
  pthread_mutex_unlock(&THR_LOCK_DDL_blocker);
  DBUG_VOID_RETURN;
}

/**
    check_ddl_blocker

    Check to see if we are blocked from continuing. If so,
    wait until the blocked process signals the condition.

    @param thd The THD object from the caller.
    @returns TRUE
  */
my_bool check_DDL_blocker(THD *thd)
{
  DBUG_ENTER("check_DDL_blocker()");
  BACKUP_BREAKPOINT("DDL_not_blocked");

  /*
    Check the ddl blocker condition. Rest until ddl blocker is released.
  */
  pthread_mutex_lock(&THR_LOCK_DDL_is_blocked);
  thd->enter_cond(&COND_DDL_blocker, &THR_LOCK_DDL_is_blocked,
                  "DDL blocker: DDL is blocked");
  while (DDL_blocked && !thd->DDL_exception)
    pthread_cond_wait(&COND_DDL_blocker, &THR_LOCK_DDL_is_blocked);
  start_DDL();
  thd->exit_cond("DDL blocker: Ok to run DDL");
  BACKUP_BREAKPOINT("DDL_in_progress");
  DBUG_RETURN(TRUE);
}

/**
   block_DDL

   This method is used to block all DDL commands. It checks the counter
   DDL_blocks and if > 0 it blocks the process until all DDL operations are
   complete and the condition variable has been signaled. 

   The method also sets the boolean DDL_blocked to TRUE to tell the DDL
   operations that they must block until the blocking operation is complete.

   @params thd THD object.
   @returns TRUE
  */
my_bool block_DDL(THD *thd)
{
  DBUG_ENTER("block_DDL()");

  BACKUP_BREAKPOINT("DDL_in_progress");
  /*
    Only 1 DDL blocking operation can run at a time.
  */
  if (DDL_blocked)
    DBUG_RETURN(FALSE);
  /*
    Check the ddl blocker condition. Rest until ddl blocker is released.
  */
  pthread_mutex_lock(&THR_LOCK_DDL_blocker);
  thd->enter_cond(&COND_process_blocked, &THR_LOCK_DDL_blocker,
                  "DDL blocker: Checking block on DDL changes");
  while (DDL_blocks != 0)
    pthread_cond_wait(&COND_process_blocked, &THR_LOCK_DDL_blocker);
  DDL_blocked= TRUE;
  thd->exit_cond("DDL blocker: Ok to run operation - no DDL in progress");
  BACKUP_BREAKPOINT("DDL_blocked");
  DBUG_RETURN(TRUE);
}

/**
   unblock_DDL

   This method is used to unblock all DDL commands. It sets the boolean
   DDL_blocked to FALSE to tell the DDL operations that they can proceed.
  */
void unblock_DDL()
{
  pthread_mutex_lock(&THR_LOCK_DDL_blocker);
  DDL_blocked= FALSE;
  pthread_cond_broadcast(&COND_DDL_blocker);
  pthread_mutex_unlock(&THR_LOCK_DDL_blocker);
}

