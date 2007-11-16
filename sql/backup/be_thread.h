#ifndef _BACKUP_THREAD_H
#define _BACKUP_THREAD_H

#include "../mysql_priv.h"
#include "archive.h"
#include "api_types.h"
#include "backup_engine.h"

/**
   Macro for error handling.
*/
#define SET_STATE_TO_ERROR_AND_DBUG_RETURN {                                 \
    int state= backup::ERROR;                                                \
    DBUG_PRINT("error",("driver got an error at %s:%d",__FILE__,__LINE__)); \
    DBUG_RETURN(backup::ERROR); }

/**
   Locking of tables goes through several states.
*/
typedef enum {
  LOCK_NOT_STARTED,
  LOCK_IN_PROGRESS,
  LOCK_ACQUIRED,
  LOCK_DONE,
  LOCK_ERROR,
  LOCK_SIGNAL
} LOCK_STATE;

using backup::result_t;
using backup::Table_list;

/**
   create_new_thd

   This method creates a new THD object.
*/
THD *create_new_thd();

/**
   backup_thread_for_locking

   This method creates a new thread and opens and locks the tables.
*/
pthread_handler_t backup_thread_for_locking(void *arg);

/**
 * @class Backup_thread_driver
 *
 * @brief Adds variables for using a locking thread for opening tables.
 *
 * The Backup_thread_driver class extends the Backup_driver class by adding
 * a mutex and condition variable for using a thread to open and lock the 
 * tables.
 *
 * @see <backup driver> and <backup thread driver>
 */
class Backup_thread_driver : public Backup_driver
{
public:

  Backup_thread_driver(const backup::Table_list &tables);
  ~Backup_thread_driver();

  pthread_mutex_t THR_LOCK_driver_thread; ///< mutex for thread variables
  pthread_cond_t COND_driver_thread_wait; ///< condition variable for wait
  pthread_mutex_t THR_LOCK_driver; ///< mutex for thread variables
  pthread_cond_t COND_driver_wait; ///< condition variable for wait
  TABLE_LIST *tables_in_backup;           ///< List of tables used in backup
  THD *lock_thd;                          ///< Locking thread pointer
  LOCK_STATE lock_state;                  ///< Current state of the lock call
  THD *m_thd;                    ///< Pointer to current thread struct.

  backup::result_t start_locking_thread();
  void kill_locking_thread();

}; // Backup_thread_driver class

#endif

