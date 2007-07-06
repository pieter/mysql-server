#ifndef _BACKUP_DEBUG_H
#define _BACKUP_DEBUG_H

#define BACKUP_SYNC_TIMEOUT 300

/*
  TODO
  - decide how to configure DEBUG_BACKUP
 */

#ifndef DBUG_OFF
# define DBUG_BACKUP
#endif

#ifdef DBUG_BACKUP

/*
  Macros for debugging error (or other) conditions. Usage:

  TEST_ERROR_IF(<condition deciding if TEST_ERROR should be true>);

  if (<other conditions> || TEST_ERROR)
  {
    <report error>
  }

  The additional TEST_ERROR condition will be set only if "backup_error_test"
  error injection is set in the server.

  Notes:
   - Whenever TEST_ERROR is used in a condition, TEST_ERROR_IF() should
     be called before - otherwise TEST_ERROR might be unintentionally TRUE.
   - This mechanism is not thread safe.
 */

namespace backup {
 extern bool test_error_flag;
}

#define TEST_ERROR  backup::test_error_flag
// FIXME: DBUG_EXECUTE_IF below doesn't work
#define TEST_ERROR_IF(X) \
 do { \
   backup::test_error_flag= FALSE; \
   DBUG_EXECUTE_IF("backup_error_test", backup::test_error_flag= (X);); \
 } while(0)

/*
  Macros for creating synchronization points in tests.

  Usage

  In the backup code:
    BACKUP_SYNC("<synchronization point name>");

  In a client:
    SELECT get_lock("<synchronization point name>",<timeout>);
    ...
    SELECT release_lock("<synchronization point name>");

  If the lock is kept by a client, server code will wait on the corresponding
  BACKUP_SYNC() until it is released.

  Consider: set thd->proc_info when waiting on lock
 */

#define BACKUP_SYNC(S) \
 do { \
  DBUG_PRINT("backup",("== synchronization on '%s' ==",(S))); \
  DBUG_SYNC_POINT((S),BACKUP_SYNC_TIMEOUT); \
 } while (0)

#else

#define BACKUP_SYNC(S)
#define TEST_ERROR  FALSE
#define TEST_ERROR_IF(X)

#endif

#endif
