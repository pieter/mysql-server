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
  * @file
  *
  * @brief Contains the thread methods for online backup.
  *
  * The methods in this class are used to initialize the mutexes
  * for the backup threads. Helper methods are included to make thread
  * calls easier for the driver code.
  */

#include "be_thread.h"

/**
  *  @brief Creates a new THD object.
  *
  * Creates a new THD object for use in running as a separate thread.
  *
  * @returns Pointer to new THD object or 0 if error.
  *
  * @TODO Move this method to a location where ha_ndbcluster_binlog.cc can
  *       use it and replace code in ndb_binlog_thread_func(void *arg) to
  *       call this function.
  *
  * @TODO Make this thread visible to SHOW PROCESSLIST. The following code
  *       can be used to do this. See BUG#32970 for more details.
  *
  *       pthread_mutex_lock(&LOCK_thread_count);
  *       threads.append(thd);
  *       pthread_mutex_unlock(&LOCK_thread_count);
  *
  * @note my_net_init() this should be paired with my_net_end() on 
  *       close/kill of thread.
  */
THD *create_new_thd()
{
  THD *thd;
  DBUG_ENTER("Create new THD object");

  thd= new THD;
  if (unlikely(!thd))
  {
    delete thd;
    DBUG_RETURN(0);
  }

  thd->thread_stack = (char*)&thd; // remember where our stack is  
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id= thread_id++;
  pthread_mutex_unlock(&LOCK_thread_count);
  if (unlikely(thd->store_globals())) // for a proper MEM_ROOT  
  {
    delete thd;
    DBUG_RETURN(0);
  }

  thd->init_for_queries(); // opening tables needs a proper LEX
  thd->command= COM_DAEMON;
  thd->system_thread= SYSTEM_THREAD_BACKUP;
  thd->version= refresh_version;
  thd->set_time();
  thd->main_security_ctx.host_or_ip= "";
  thd->client_capabilities= 0;
  my_net_init(&thd->net, 0);
  thd->main_security_ctx.master_access= ~0;
  thd->main_security_ctx.priv_user= 0;
  thd->real_id= pthread_self();

  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);
  DBUG_RETURN(thd);
}

/**
  * @brief Lock tables in driver.
  *
  * This method creates a new THD for use in the new thread. It calls
  * the method to open and lock the tables.
  *
  * @note my_thread_init() should be paired with my_thread_end() on 
  *       close/kill of thread.
  */
pthread_handler_t backup_thread_for_locking(void *arg)
{
  Locking_thread_st *locking_thd= static_cast<Locking_thread_st *>(arg);

  DBUG_PRINT("info", ("Default_backup - lock_tables_in_separate_thread"));

  /*
    Turn off condition variable check for lock.
  */
  locking_thd->lock_state= LOCK_NOT_STARTED;

#if !defined( __WIN__) /* Win32 calls this in pthread_create */
  my_thread_init();
#endif

  pthread_detach_this_thread();

  /*
    First, create a new THD object.
  */
  DBUG_PRINT("info",("Online backup creating THD struct for thread"));
  THD *thd= create_new_thd();
  locking_thd->lock_thd= thd;
  if (thd == 0)
  {
    locking_thd->lock_state= LOCK_ERROR;
    goto end2;
  }

  if (thd->killed)
  {
    locking_thd->lock_state= LOCK_ERROR;
    goto end2;
  }

  /* 
    Now open and lock the tables.
  */
  DBUG_PRINT("info",("Online backup open tables in thread"));
  if (!locking_thd->tables_in_backup)
  {
    DBUG_PRINT("info",("Online backup locking error no tables to lock"));
    locking_thd->lock_state= LOCK_ERROR;
    goto end2;
  }

  /*
    As locking tables can be a long operation, we need to support
    killing the thread. In this case, we need to close the tables 
    and exit.
  */
  if (!thd->killed && open_and_lock_tables(thd, locking_thd->tables_in_backup))
  {
    DBUG_PRINT("info",("Online backup locking thread dying"));
    locking_thd->lock_state= LOCK_ERROR;
    goto end;
  }

  if (thd->killed)
  {
    locking_thd->lock_state= LOCK_ERROR;
    goto end;
  }

  /*
    Part of work is done. Rest until woken up.
    We wait if the thread is not killed and the driver has not signaled us.
  */
  pthread_mutex_lock(&locking_thd->THR_LOCK_thread);
  locking_thd->lock_state= LOCK_ACQUIRED;
  thd->enter_cond(&locking_thd->COND_thread_wait,
                  &locking_thd->THR_LOCK_thread,
                  "Locking thread: holding table locks");
  while (!thd->killed && (locking_thd->lock_state != LOCK_SIGNAL))
    pthread_cond_wait(&locking_thd->COND_thread_wait,
                      &locking_thd->THR_LOCK_thread);
  thd->exit_cond("Locking thread: terminating");

  DBUG_PRINT("info",("Locking thread locking thread terminating"));

  /*
    Cleanup and return.
  */
end:
  close_thread_tables(thd);

end2:
  pthread_mutex_lock(&locking_thd->THR_LOCK_caller);
  net_end(&thd->net);
  my_thread_end();
  delete thd;
  locking_thd->lock_thd= NULL;
  if (locking_thd->lock_state != LOCK_ERROR)
    locking_thd->lock_state= LOCK_DONE;

  /*
    Signal the driver thread that it's ok to proceed with destructor.
  */
  pthread_cond_signal(&locking_thd->COND_caller_wait);
  pthread_mutex_unlock(&locking_thd->THR_LOCK_caller);
  pthread_exit(0);
  return (0);
}

/*
  Constructor for Locking_thread_st structure.
*/
Locking_thread_st::Locking_thread_st()
{
  /*
    Initialize the thread mutex and cond variable.
  */
  pthread_mutex_init(&THR_LOCK_thread, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_thread_wait, NULL);
  pthread_mutex_init(&THR_LOCK_caller, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_caller_wait, NULL);
  lock_state= LOCK_NOT_STARTED;
  lock_thd= NULL; // set to 0 as precaution for get_data being called too soon
};

/*
  Destructor for Locking_thread_st structure.
*/
Locking_thread_st::~Locking_thread_st()
{
  /*
    If the locking thread is not finished, we need to wait until
    it is finished so that we can destroy the mutexes safely knowing
    the locking thread won't access them.
  */
  if (lock_state != LOCK_DONE)
  {
    kill_locking_thread();
    pthread_mutex_lock(&THR_LOCK_caller);
    m_thd->enter_cond(&COND_caller_wait, &THR_LOCK_caller,
                    "Locking thread: waiting until locking thread is done");
    while (lock_state != LOCK_DONE)
      pthread_cond_wait(&COND_caller_wait, &THR_LOCK_caller);
    m_thd->exit_cond("Locking thread: terminating");

    DBUG_PRINT("info",("Locking thread's locking thread terminated"));
  }

  /*
    Destroy the thread mutexes and cond variables.
  */
  pthread_mutex_destroy(&THR_LOCK_thread);
  pthread_cond_destroy(&COND_thread_wait);
  pthread_mutex_destroy(&THR_LOCK_caller);
  pthread_cond_destroy(&COND_caller_wait);
}

/**
   Start the driver's lock thread.

   Launches a separate thread ("locking thread") which will lock
   tables.
 */
result_t Locking_thread_st::start_locking_thread()
{
  DBUG_ENTER("Locking_thread_st::start_locking_thread");
  pthread_t th;
  if (pthread_create(&th, &connection_attrib,
                     backup_thread_for_locking, this))
    SET_STATE_TO_ERROR_AND_DBUG_RETURN;
  DBUG_RETURN(backup::OK);
}

/**
   Kill the driver's lock thread.

   This method issues the awake and broadcast to kill the locking thread.
   A mutex is used to prevent the locking thread from deleting the THD
   structure until this operation is complete.
 */
void Locking_thread_st::kill_locking_thread()
{
  DBUG_ENTER("Locking_thread_st::kill_locking_thread");
  pthread_mutex_lock(&THR_LOCK_caller);
  if (lock_thd && (lock_state != LOCK_DONE) && (lock_state != LOCK_SIGNAL))
  {
    lock_state= LOCK_SIGNAL;
    pthread_mutex_lock(&lock_thd->LOCK_delete);
    lock_thd->awake(THD::KILL_CONNECTION);
    pthread_mutex_unlock(&lock_thd->LOCK_delete);
    pthread_cond_signal(&COND_thread_wait);
  }
  pthread_mutex_unlock(&THR_LOCK_caller);

  /*
    This tells the CS driver that we're finished with the tables.
  */
  if (!lock_thd && (lock_state == LOCK_ACQUIRED))
    lock_state= LOCK_SIGNAL;
  DBUG_VOID_RETURN;
}

