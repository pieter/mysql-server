/**
  @file

  Header file for DDL blocker code.
 */
#include "mysql_priv.h"
#include "backup/debug.h"

/**
   @class DDL_blocker_class
 
   @brief Implements a simple DDL blocker mechanism.
 
   The DDL_blocker_class is a singleton class designed to allow DDL
   operations to be blocked while an operation that uses this class
   executes. The class is designed to allow any number of DDL operations
   to execute but only one blocking operation can block DDL operations
   at a time. 

   If an operation has blocked DDL operations and another operation attempts
   to block DDL operations, the second blocking operation will wait
   until the first blocking operation is complete.
 
   Checking for Block
   For any DDL operation that needs to be blocked while another 
   operation is executing, you can check the status of the DDL blocker
   by calling @c check_DDL_blocker(). This method will return when there
   is no block or wait while the blocking operation is running. Once the
   method returns, the DDL operation that called the method is registered
   so that any other blocking operation will wait while the DDL operation
   is running. When the DDL operation is complete, you must unregister the
   DDL operation by calling end_DDL().

   Blocking DDL Operations
   To block a DDL operation, call block_DDL(). This registers the blocking
   operation and prevents any DDL operations that use check_DDL_blocker()
   to block while the blocking operation is running. When the blocking
   operation is complete, you must call unblock_DDL() to unregister
   the blocking operation.

   Singleton Methods
   The creation of the singleton is accomplished using 
   get_DDL_blocker_class_instance(). This method is called from mysqld.cc
   and creates and initializes all of the private mutex, condition, and
   controlling variables. The method destroy_DDL_blocker_class_instance()
   destroys the mutex and condition variables. 

   Calling the Singleton
   To call the singleton class, you must declare an external variable
   to the global variable DDL_blocker as shown below.

   @c extern DDL_blocker_class *DDL_blocker;

   Calling methods on the singleton is accomplished using the DDL_blocker
   variable such as: @c DDL_blocker->block_DDL().

   @note: This class is currently only used in online backup. If you would
          like to use it elsewhere and have questions, please contact
          Chuck Bell (cbell@mysql.com) for more details and how to setup
          a test case to test the DDL blocking mechanism for your use.
  */
class DDL_blocker_class
{
  public:

    /*
      Singleton class
    */
    static DDL_blocker_class *get_DDL_blocker_class_instance();
    static void destroy_DDL_blocker_class_instance();

    /*
      Check to see if we are blocked from continuing. If so,
      wait until the backup process signals the condition.
    */
    my_bool check_DDL_blocker(THD *thd);

    /*
      Decrements the backup's counter to indicate a DDL is done.
      Signals backup process if counter == 0.
    */
    void end_DDL();

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

  private:

    DDL_blocker_class();
    ~DDL_blocker_class();

    /*
      Increments the backup's counter to indicate a DDL is in progress.
    */
    void start_DDL();

    /*
      These variables are used to implement the metadata freeze "DDL blocker"
      for online backup.
    */
    pthread_mutex_t THR_LOCK_DDL_blocker;    ///< Mutex for blocking DDL   
    pthread_mutex_t THR_LOCK_DDL_is_blocked; ///< Mutex for checking block DDL 
    pthread_mutex_t THR_LOCK_DDL_blocker_blocked; ///< One blocker at a time 
    pthread_cond_t COND_DDL_blocker;         ///< cond for blocking DDL
    pthread_cond_t COND_process_blocked;     ///< cond for checking block DDL
    pthread_cond_t COND_DDL_blocker_blocked; ///< cond for blocker blocked
    my_bool DDL_blocked;                     ///< Is blocking operation running
    int DDL_blocks;              ///< Number of DDL operations in progress.
    static DDL_blocker_class *m_instance;    ///< instance var for singleton 
};
