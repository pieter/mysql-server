#ifndef _BACKUP_DEBUG_H
#define _BACKUP_DEBUG_H

#define BACKUP_BREAKPOINT_TIMEOUT 300

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

/**
  @page BACKUP_BREAKPOINT Online Backup Breakpoints
  Macros for creating breakpoints during testing.

  @section WHAT What are breakpoints?
  Breakpoints are devices used to pause the execution of the backup system
  at a certain point in the code. There is a timeout that you can specify
  when you set the lock for the breakpoint (from get_lock() see below)
  which will enable execution to continue after the period in seconds
  expires.

  The best use of these breakpoints is for pausing execution at critical
  points in the backup code to allow proper testing of certain features.
  For example, suppose you wanted to ensure the Consistent Snapshot driver
  was working properly. To do so, you would need to ensure no new @INSERT
  statements are executed while the data is being backed up. If you use
  a breakpoint, you can set the breakpoint to pause the backup kernel at
  the point where it has set the consistent read and is reading rows.
  You can then insert some rows and release the breakpoint. The result
  should contain all of the rows in the table except those that were
  inserted once the consistent read was set.

  @section USAGE How to use breakpoints.
  To make a breakpoint available, you must add a macro call to the code.
  Simply insert the macro call as follows. The @c breakpoint_name is a 
  text string that must be unique among the breakpoints. It is used in 
  the macro as a means of tagging the code for pausing and resuming
  execution. Once the code is compiled, you can use a client connection
  to set and release the breakpoint.

  <b><c>BACKUP_BREAKPOINT("<breakpoint_name>");</c></b>
  
  Breakpoints use the user-defined locking functions @c get_lock() to set
  the breakpoint and @c release_lock() to release it.

  @subsection SET Setting breakpoints.
  To set an existing breakpoint, issue the following command where @c
  timeout is the number of seconds execution will pause once the breakpoint
  is reached before continuing execution.

  <b><c>SELECT get_lock("<breakpoint_name>",<timeout>);</c></b>

  @subsection RELEASE Releasing breakpoints.
  To release an existing breakpoint, issue the following command. This
  releases execution allow the system to continue.

  <b><c>SELECT release_lock("<breakpoint_name>");</c></b>

  @subsection EXAMPLE Example - Testing the Consistent Snapshot Driver
  To test the consistent snapshot driver, we can make use of the @c
  backup_cs_locked breakpoint to pause execution after the consistent read
  is initiated and before all of the rows from the table have been read.
  Consider an InnoDB table with the following structure as our test table.

  <c>CREATE TABLE t1 (a INT) ENGINE=INNODB;</c>

  To perform this test using breakpoints, we need two client connections.
  One will be used to execute the backup command and another to set and
  release the breakpoint. In the first client, we set the breakpoint with
  the <c>SELECT get_lock("backup_cs_locked", 100);</c> command. In the 
  second client, we start the execution of the backup. We can return to
  the first client and issue several @INSERT statements then issue the
  <c>SELECT release_lock("backup_cs_locked");</c> command to release the
  breakpoint.

  We can then return to the second client, select all of the rows from the
  table to verify the rows were inserted. We can verify that the consistent
  snapshot worked by restoring the database (which is a destructive restore)
  and then select all of the rows. This will show that the new rows 
  inserted while the backup was running were not inserted into the table.
  The following shows the output of the commands as described.

  <b>First Client</b>
  @code mysql> SELECT * FROM t1;
  +---+
  | a |
  +---+
  | 1 |
  | 2 |
  | 3 |
  +---+
  3 rows in set (0.00 sec)

  mysql> SELECT get_lock("backup_cs_locked", 100);
  +-----------------------------------+
  | get_lock("backup_cs_locked", 100) |
  +-----------------------------------+
  |                                 1 |
  +-----------------------------------+
  1 row in set (0.00 sec) @endcode

  <b>Second Client</b>
  @code mysql> BACKUP DATABASE test TO 'test.bak'; @endcode

  Note: The backup will pause while the breakpoint is set (the lock is held).

  <b>First Client</b>
  @code mysql> INSERT INTO t1 VALUES (101), (102), (103);
  Query OK, 3 rows affected (0.02 sec)
  Records: 3  Duplicates: 0  Warnings: 0

  mysql> SELECT * FROM t1;
  +-----+
  | a   |
  +-----+
  |   1 |
  |   2 |
  |   3 |
  | 101 |
  | 102 |
  | 103 |
  +-----+
  6 rows in set (0.00 sec)
  
  mysql> SELECT release_lock("backup_cs_locked");
  +----------------------------------+
  | release_lock("backup_cs_locked") |
  +----------------------------------+
  |                                1 |
  +----------------------------------+
  1 row in set (0.01 sec) @endcode

  <b>Second Client</b>
  @code +------------------------------+
  | Backup Summary               |
  +------------------------------+
  |  header     =       14 bytes |
  |  meta-data  =      120 bytes |
  |  data       =       30 bytes |
  |               -------------- |
  |  total             164 bytes |
  +------------------------------+
  5 rows in set (33.45 sec)

  mysql> SELECT * FROM t1;
  +-----+
  | a   |
  +-----+
  |   1 |
  |   2 |
  |   3 |
  | 101 |
  | 102 |
  | 103 |
  +-----+
  6 rows in set (0.00 sec)
  
  mysql> RESTORE FROM 'test.bak';
  +------------------------------+
  | Restore Summary              |
  +------------------------------+
  |  header     =       14 bytes |
  |  meta-data  =      120 bytes |
  |  data       =       30 bytes |
  |               -------------- |
  |  total             164 bytes |
  +------------------------------+
  5 rows in set (0.08 sec)
  
  mysql> SELECT * FROM t1;
  +---+
  | a |
  +---+
  | 1 |
  | 2 |
  | 3 |
  +---+
  3 rows in set (0.00 sec)@endcode

  Note: The backup will complete once breakpoint is released (the lock is
  released).

  @section BREAKPOINTS Breakpoints
  The following are the available breakpoints included in the code.

  - <b>backup_command</b>  Occurs at the start of the backup operation.
  - <b>data_init</b>  Occurs at the start of the <b>INITIALIZE PHASE</b>.
  - <b>data_prepare</b>  Occurs at the start of the <b>PREPARE PHASE</b>.
  - <b>data_lock</b>  Occurs at the start of the <b>SYNC PHASE</b>.
  - <b>data_unlock</b>  Occurs before the unlock calls.
  - <b>data_finish</b>  Occurs at the start of the <b>FINISH PHASE</b>.
  - <b>backup_meta</b>  Occurs before the call to write_meta_data().
  - <b>backup_data</b>  Occurs before the call to write_table_data().
  - <b>backup_done</b>  Occurs after the call to write_table_data() returns.
  - <b>backup_cs_locked</b>  Consistent Snapshot - after the consistent
                             read has been initiated but before rows are read.
  - <b>backup_cs_open_tables</b>  Consistent Snapshot - before the call to
                             open and lock tables.
  - <b>backup_cs_reading</b>  Consistent Snapshot - occurs during read.

  @section NOTES Developer Notes
  - Breakpoints can be used in debug builds only. You must compile
  the code using the @c DEBUG_EXTRA preprocessor directive. 
  - When adding breakpoints, you must add a list item for each breakpoint
  to the documentation for breakpoints. See the code for the macro
  definition in @ref debug.h for details.

 */

/*
  Consider: set thd->proc_info when waiting on lock
*/
#define BACKUP_BREAKPOINT(S) \
 do { \
  DBUG_PRINT("backup",("== breakpoint on '%s' ==",(S))); \
  DBUG_SYNC_POINT((S),BACKUP_BREAKPOINT_TIMEOUT); \
 } while (0)

#else

#define BACKUP_BREAKPOINT(S)
#define TEST_ERROR  FALSE
#define TEST_ERROR_IF(X)

#endif

#endif
