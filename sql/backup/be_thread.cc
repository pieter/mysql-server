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
  /*
    Making this thread visible to SHOW PROCESSLIST is useful for
    troubleshooting a backup job (why does it stall etc).
  */
  pthread_mutex_lock(&LOCK_thread_count);
  threads.append(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
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
  Backup_thread_driver *drv= static_cast<Backup_thread_driver *>(arg);

  DBUG_PRINT("info", ("Default_backup - lock_tables_in_separate_thread"));

  /*
    Turn off condition variable check for lock.
  */
  drv->lock_state= LOCK_NOT_STARTED;

#if !defined( __WIN__) /* Win32 calls this in pthread_create */
  my_thread_init();
#endif

  pthread_detach_this_thread();

  /*
    First, create a new THD object.
  */
  DBUG_PRINT("info",("Online backup creating THD struct for thread"));
  THD *thd= create_new_thd();
  drv->lock_thd= thd;
  if (thd == 0)
  {
    drv->lock_state= LOCK_ERROR;
    goto end2;
  }

  if (thd->killed)
  {
    drv->lock_state= LOCK_ERROR;
    goto end2;
  }

  /* 
    Now open and lock the tables.
  */
  DBUG_PRINT("info",("Online backup open tables in thread"));
  if (!drv->tables_in_backup)
  {
    DBUG_PRINT("info",("Online backup locking error no tables to lock"));
    drv->lock_state= LOCK_ERROR;
    goto end2;
  }

  /*
    As locking tables can be a long operation, we need to support
    killing the thread. In this case, we need to close the tables 
    and exit.
  */
  if (!thd->killed && open_and_lock_tables(thd, drv->tables_in_backup))
  {
    DBUG_PRINT("info",("Online backup locking thread dying"));
    drv->lock_state= LOCK_ERROR;
    goto end;
  }

  if (thd->killed)
  {
    drv->lock_state= LOCK_ERROR;
    goto end;
  }

  /*
    Part of work is done. Rest until woken up.
    We wait if the thread is not killed and the driver has not signaled us.
  */
  pthread_mutex_lock(&drv->THR_LOCK_driver_thread);
  drv->lock_state= LOCK_ACQUIRED;
  thd->enter_cond(&drv->COND_driver_thread_wait, &drv->THR_LOCK_driver_thread,
                  "Online backup driver thread: holding table locks");
  while (!thd->killed && (drv->lock_state != LOCK_SIGNAL))
    pthread_cond_wait(&drv->COND_driver_thread_wait, &drv->THR_LOCK_driver_thread);
  thd->exit_cond("Online backup driver thread: terminating");

  DBUG_PRINT("info",("Online backup driver thread locking thread terminating"));

  /*
    Cleanup and return.
  */
end:
  close_thread_tables(thd);

end2:
  pthread_mutex_lock(&drv->THR_LOCK_driver);
  net_end(&thd->net);
  my_thread_end();
  delete thd;
  drv->lock_thd= NULL;
  if (drv->lock_state != LOCK_ERROR)
    drv->lock_state= LOCK_DONE;

  /*
    Signal the driver thread that it's ok to proceed with destructor.
  */
  pthread_cond_signal(&drv->COND_driver_wait);
  pthread_mutex_unlock(&drv->THR_LOCK_driver);
  pthread_exit(0);
  return (0);
}

/*
  Constructor for backup_thread_driver class.
*/
Backup_thread_driver::Backup_thread_driver(const Table_list &tables):
                                           Backup_driver(tables)
{
  /*
    Initialize the thread mutex and cond variable.
  */
  pthread_mutex_init(&THR_LOCK_driver_thread, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_driver_thread_wait, NULL);
  pthread_mutex_init(&THR_LOCK_driver, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_driver_wait, NULL);
  lock_state= LOCK_NOT_STARTED;
  lock_thd= NULL; // set to 0 as precaution for get_data being called too soon
};

/*
  Destructor for backup_thread_driver class.
*/
Backup_thread_driver::~Backup_thread_driver()
{
  /*
    If the locking thread is not finished, we need to wait until
    it is finished so that we can destroy the mutexes safely knowing
    the locking thread won't access them.
  */
  if (lock_state != LOCK_DONE)
  {
    kill_locking_thread();
    pthread_mutex_lock(&THR_LOCK_driver);
    m_thd->enter_cond(&COND_driver_wait, &THR_LOCK_driver,
                    "Online backup driver: waiting until locking thread is done");
    while (lock_state != LOCK_DONE)
      pthread_cond_wait(&COND_driver_wait, &THR_LOCK_driver);
    m_thd->exit_cond("Online backup driver: terminating");

    DBUG_PRINT("info",("Online backup driver's locking thread terminated"));
  }

  /*
    Destroy the thread mutexes and cond variables.
  */
  pthread_mutex_destroy(&THR_LOCK_driver_thread);
  pthread_cond_destroy(&COND_driver_thread_wait);
  pthread_mutex_destroy(&THR_LOCK_driver);
  pthread_cond_destroy(&COND_driver_wait);
}

/**
   Start the driver's lock thread.

   Launches a separate thread ("locking thread") which will lock
   tables.
 */
result_t Backup_thread_driver::start_locking_thread()
{
  DBUG_ENTER("Backup_thread_driver::start_locking_thread");
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
void Backup_thread_driver::kill_locking_thread()
{
  DBUG_ENTER("Backup_thread_driver::kill_locking_thread");
  pthread_mutex_lock(&THR_LOCK_driver);
  if (lock_thd && (lock_state != LOCK_DONE) && (lock_state != LOCK_SIGNAL))
  {
    lock_state= LOCK_SIGNAL;
    pthread_mutex_lock(&lock_thd->LOCK_delete);
    lock_thd->awake(THD::KILL_CONNECTION);
    pthread_mutex_unlock(&lock_thd->LOCK_delete);
    pthread_cond_signal(&COND_driver_thread_wait);
  }
  pthread_mutex_unlock(&THR_LOCK_driver);

  /*
    This tells the CS driver that we're finished with the tables.
  */
  if (!lock_thd && (lock_state == LOCK_ACQUIRED))
    lock_state= LOCK_SIGNAL;
  DBUG_VOID_RETURN;
}

