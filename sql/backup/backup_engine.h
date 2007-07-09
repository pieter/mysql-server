#ifndef MYSQL_PRIV_H
  #error backup_engine.h must be included after mysql_priv.h
#endif

#ifndef _BACKUP_ENGINE_API_H
#define _BACKUP_ENGINE_API_H

/**
 @file backup_engine.h
 @brief Backup engine and backup/restore driver API

 The interface between online backup kernel and a backup solution has form of
 two abstract classes: @c Backup_driver implementing backup
 functionality and @c Restore_driver for restore functionality.
 Instances of these two classes are created by a factory class
 @c Backup_engine which encapsulates and represents the backup
 solution as a whole.
 */

#include <backup/api_types.h>

namespace backup {

// Forward declarations
class Backup_driver;
class Restore_driver;

/**
 @class Engine
 @brief Encapsulates online backup/restore functionality.

 Any backup solution is represented in the online backup kernel by an instance
 of this class, so called <em>backup engine</em>. This object is used to find
 out general information about the solution (e.g. version number). It also
 constructs backup and restore drivers (instances of @c Backup_driver
 and @c Restore_driver classes) to perform backup/restore operations
 for a given list of tables.

 @note The backup engine instance is created using get_backup function of
 handlerton structure. It should be cheap to create instances of @c Engine
 class as they might be used by the kernel to query backup capabilities of an
 engine. On the other hand, kernel will take care to create driver instances
 only when they are really needed.
 */

class Engine
{
 public:

  virtual ~Engine() {}

  /// Return version of backup images created by this engine.
  virtual const version_t version() const =0;

  /**
   Create a backup driver.

   Given a list of tables to be backed-up, create instance of backup
   driver which will create backup image of these tables.

   The @c flags parameter gives additional information about
   the backup process to be performed by the driver. Currently, we only set
   @c Driver::FULL flag if the driver is supposed to backup all the
   tables stored in a given storage engine.

   @param  flags  (in) additional info about backup operation.
   @param  tables (in) list of tables to be backed-up.
   @param  drv    (out) pointer to backup driver instance.

   @return  Error code or @c OK on success.
  */
  virtual result_t get_backup(const uint32      flags,
                              const Table_list  &tables,
                              Backup_driver*    &drv) =0;

  /**
   Create a restore driver.

   Given a list of tables to be restored, create instance of restore
   driver which will restore these tables from a backup image.

   The @c flags parameter gives additional information about
   the restore process to be performed by the driver. Currently, we only set
   @c Driver::FULL flag if the driver is supposed to replace all the
   tables stored in a given storage engine with the restored ones.

   @param  version  (in) version of the backup image.
   @param  flags    (in) additional info about restore operation.
   @param  tables   (in) list of tables to be restored.
   @param  drv      (out) pointer to restore driver instance.

   @return  Error code or @c OK on success.
  */
  virtual result_t get_restore(const version_t  version,
                               const uint32     flags,
                               const Table_list &tables,
                               Restore_driver*  &drv) =0;

  /**
   Free any resources allocated by the backup engine.

   It is possible to delete the instance here since backup kernel
   will never use an instance after a call to @c free() method.

   Backup kernel does not assume that backup engine is allocated
   dynamically and therefore will never delete an instance it has obtained.
   However, it will call @c free() method when done with the instance.
   If the instance is dynamically allocated it should be deleted in this method.
  */
  virtual void free() {};

}; // Engine class


/**
 @class Driver

 @brief  This class contains methods which are common to both types of drivers.

 A driver is created to backup or restore given list of tables. This list
 is passed as an argument when constructing a driver. A reference to
 the list is stored in @c m_tables member for future use (the memory
 to store the list is allocated/deallocated by the kernel).

 Driver methods return value of type result_t to inform backup kernel about the
 result of each operation. If ERROR is returned, it means that the driver is
 not able to proceed. The kernel will shutdown the driver by calling
 @c free() method. No other methods will be called after signalling
 error by a driver.
*/

class Driver
{
 public:

  /// Types of backup/restore operations.
  enum enum_flags { FULL    =0x1,  ///< concerns all tables from given storage engine
                    PARTIAL =0     ///< backup/restore only selected tables
                  };

  /// Construct from list of tables. The list is stored for future use.
  Driver(const Table_list &tables):m_tables(tables) {};

  virtual ~Driver() {}; // We want to inherit from this class.

  /**
   @brief Initialize backup/restore process.

   After return from @c begin() call, driver should be ready to
   serve requests for sending/receiving image data.

   @param buf_size (in)  this is the minimal size of buffers backup kernel
                         will provide in @c get_data(), @c send_data() methods. 
                         The buffer can be actually bigger (and its real size 
                         will be stored in buffers size member) but it will 
                         never be smaller.

   @return  Error code or @c OK on success.
  */

  virtual result_t  begin(const size_t buf_size) =0;


  /**
   @brief Finalize backup/restore process.

   This method is called when all data has been sent (from kernel
   to restore driver or from backup driver to kernel) so that the
   backup/restore process can be finalized inside the driver.

   @note All DDL operations on tables being backed-up are
   blocked in the server. An engine which can alter tables (e.g. NDB) should
   participate in this block by not allowing any such changes between calls to
   @c begin() and @c end().

   @return  Error code or @c OK on success.
  */

  virtual result_t  end()   =0;

  /// Cancel ongoing backup/restore process.
  virtual result_t  cancel() =0;

  /**
   @brief Free resources allocated by the driver.

   Driver can be deleted here. @see Engine::free()
  */
  virtual void  free() {};

  /// Unknown size constant used for backup image size estimates.
  static const size_t UNKNOWN_SIZE= static_cast<size_t>(-1);

 protected:

  /// Refers to the list of tables passed when the driver was created.
  const Table_list &m_tables;

}; // Driver class


/**
 @class Backup_driver

 @brief Represents backup driver for backing-up a given list of tables.

 This class provides all the methods used to implement the backup protocol
 for communication between backup kernel and the driver. The most important
 method is @c get_data() which is used by the kernel to poll the
 backup image data and at the same time learn about state of the backup
 process.

 Backup process consists of the following phases (not all
 phases are meaningful for all backup methods).

 -# <b>Idle</b>, after creation of the driver instance and before
    @c begin() call from kernel. Note that any resources should be
    allocated inside @c begin() method, not upon driver
    creation.
 -# <b>Initial transfer</b>, when initial data is sent before driver can create
    a <em>validity point</em>. "At end" backup drivers will send majority of
    their data in this phase.
 -# <b>Waiting for prelock</b>, when driver waits for other drivers to finish
    sending their initial transfer phase. This phase is ended by a call to
    @c prelock() method.
 -# <b>Preparing for lock</b>, when driver does necessary preparations (if any)
    to be able to instantly crate a validity point upon request from kernel.
 -# <b>Waiting for lock</b>, when driver waits for other drivers to finish their
    preparations. Phase is finished by a call to @c lock() method.
 -# <b>Synchronization</b>, when the validity point is created inside
    @c lock() method. For synchronization reasons, data in all tables
    being backed-up should be frozen during that phase. Phase is ended by a call
    to @c unlock() method.
 -# <b>Final transfer</b>, when final backup image data (if any) is sent to the
    kernel. "At begin" will send all their data in this phase. This phase is
    ended by a call to @c end() method.

 In each phase, except for the synchronization phase (6),  kernel is polling
 driver using @c get_data() method. Thus a driver has a chance to send
 data in each phase of the backup process. For example, when waiting in phase 3
 or 5, driver can send log recording changes which happen during that time.

 A driver informs the kernel about finishing the initial transfer phase (2) or
 the lock preparation phase (4) by the value returned from the
 @c get_data() method (see description of the method).

 Not all drivers will need all the phases to perform backup but they should
 still follow the protocol and give correct replies from @c get_data()
 method.

 @note The list of tables being backed-up is accessible via @c m_tables
 member inherited from @c Driver class

 @see Methods @c begin(), @c end(), @c get_data(), @c prelock(), @c lock(), 
 @c unlock() and @c Driver class.
*/

class Backup_driver: public Driver
{
 public:

  Backup_driver(const Table_list &tables):Driver(tables) {};

  virtual ~Backup_driver() {}; // Each specific implementation will derive from this class.

 /**
   @fn result_t get_data(Buffer &buf)

   @brief Accept a request for filling a buffer with next chunk of backup data
   or check status of a previously accepted request.

   Backup driver can implement its own policy of handling these requests. It can
   return immediately from the call and use a separate thread to fill the buffer
   or the calling thread can be used to do the job. It is also possible that
   a driver accepts new requests while processing old ones, implementing
   internal queue of requests.

   The kernel learns about what happened to the request from the value returned
   by the method (see below). The returned value is also used to inform the
   kernel that the driver has finished the initial transfer phase (2) or the
   prepare to lock phase (4) (see description of the class).

   When a request is completed, members of @c buf should be filled
   as described in the documentation of Buffer class. It is possible to complete
   a request without putting any data in the buffer. In that case
   @c buf.size should be set to zero. The return value (OK or READY)
   and the @c buf.table_no and @c buf.last members are
   interpreted as usual. However, no data is written to backup archive and such
   empty buffers are not sent back to restore driver.


   @param  buf (in/out) buffer to be filled with backup data. Its members are
               initialized by backup kernel: @c buf.data points to a memory area 
               where the data should be placed and @c buf.size is the size of 
               the area. Upon completion of the request (@c OK or @c READY
               returned), members of @c buf should be filled as described in 
               the documentation of Buffer class.

   @retval OK  The request is completed - new data is in the buffer and
               @c size, @c table_no and @c last members of the buffer structure
               are set accordingly.

   @retval READY Same as OK and additionally informs that the initial transfer
               phase (2) or the prepare to lock phase (4) are finished for that
               driver.

   @retval DONE Same as OK but also indicates that the backup process is
               completed. This result can be returned only in the final transfer
               phase (7).

   @retval PROCESSING The request was accepted but is not completed yet. Further
               calls to get_data() are needed to complete it (until it returns
               OK or READY). Kernel will not reuse the buffer before it knows
               that it is completely filled with data.

   @retval BUSY The request can not be accepted now. Kernel can try to place a
               request later. The buffer is not blocked and can be used for
               other transfers.

   @retval ERROR An error has happened while processing the request.

   @note
   If backup kernel calls @c get_data() when there is no more data
   to be sent, the driver should:
   -# set @c buf.size and @c buf.table_no to 0,
   -# set @c buf.last to TRUE,
   -# return @c DONE.

   @see @c Buffer class.
  */

  virtual result_t  get_data(Buffer &buf) =0;


  /**
   @fn result_t prelock()

   @brief Prepare for synchronization of backup image.

   This method is called by backup kernel when all engines participating in
   creation of the backup have finished their initial data transfer. After this
   call the driver should prepare for the following @c lock() call
   from the kernel.

   It can do the preparations inside the @c prelock() method if it
   doesn't require too long time. In that case it should return
   @c READY. If the preparations require longer time (waiting for
   ongoing operations to finish) or sending additional data to the kernel then
   @c prelock() should return @c OK. Later on, the kernel
   will call @c get_data() and driver can signal that it has finished
   the preparations by returning @c READY result.

   @retval READY The driver is ready for synchronization, i.e. it can accept the
              following @c lock() call.

   @retval OK The driver is preparing for synchronization. Kernel should call
              @c get_data() and wait until driver is ready.

   @retval ERROR The driver can not prepare for synchronization.
  */

  virtual result_t  prelock()
  {  return READY; };

  /**
   @brief Create validity point and freeze all backed-up data.

   After sending @c prelock() requests to all backup drivers and
   receiving @c READY confirmations, backup kernel calls
   @c lock() method of each driver. The driver is supposed to do two
   things in response:

   -# Create a validity point of its backup image. The whole backup image should
      describe data at this exact point in time.
   -# Freeze its state until the following @c unlock() call. This
      means that from now on the data stored in the backed-up tables should not
      change in any way, so that the validity point remains valid during the
      time other engines create their own validity points.

   When all drivers have locked, backup kernel will call @c unlock()
   on all of them. After this call the driver should unfreeze. Kernel will
   continue polling backup data using @c get_data() method until
   driver signals that there is no more data to be sent.

   @note <b>Important!</b>. A call to @c lock() should return as
   quickly as possible since it blocks other drivers. Ideally, only fast memory
   access and/or (non-blocking) mutex manipulations should happen but no disk
   operations. The backup kernel expects that this call will return in at most
   few seconds.

   @returns Error code or @c OK upon success.
  */

  virtual result_t  lock() =0;

  /**
   @brief Unlock data after the @c lock() call.

   After call to @c unlock() driver enters the final data transfer
   phase. Any remaining data should be sent in the following
   @c get_data() calls and all data streams should be closed. The
   process is ended by returning @c DONE from the last
   @c get_data() call.

   @note <b>Important!</b>. Similar as with @c lock(), a call to
   @c unlock() should return as quickly as possible to not block other
   drivers.
  */

  virtual result_t  unlock() =0;


  /**
   Return estimate (in bytes) of the size of the backup image.

   This estimate is used by backup kernel to give backup progress feedback to
   users.
   If estimating the size is impossible or very costly, the driver can return
   @c UNKNOWN_SIZE.
  */

  virtual size_t    size() =0;

  /**
   Estimate how much data will be sent in the initial phase of backup.

   This information is used by backup kernel to initialize backup drivers of
   different types at correct times. Roughly, drivers with biggest
   @c init_size() will be initialized and polled first. Drivers whose
   @c init_size() is zero, will be initialized and polled last in the
   process.

   Thus "at begin" drivers which send all backup data in the final phase
   of backup should return 0 here. Drivers of "at end" type should return
   estimate for the size of data to be sent before they are ready for validity
   point creation. If estimating this size is impossible or very expensive, the
   driver can return UNKNOWN_SIZE. In that case  the driver will be initialized
   and polled before any other drivers.
  */

  virtual size_t    init_size() =0;

}; // Backup_driver class

/**
 @class Restore_driver

 @brief Represents restore driver used for restoring a given list of tables.

 This class provides all the methods used to implement the restore protocol
 for communication between backup kernel and the driver. Apart from the common
 driver methods of @c Driver class it provides
 @c send_data() method which is used by the kernel to send
 backup image data to the driver.

 It is assumed that all tables are blocked during restore process.

 @note The list of tables being restored is accessible via @c m_tables
 member inherited from @c Driver class

 @see @c Driver class.
*/

class Restore_driver: public Driver
{
 public:

  Restore_driver(const Table_list &tables):Driver(tables) {};
  virtual ~Restore_driver() {};

  /**
   @fn result_t send_data(Buffer &buf)

   @brief Request processing of next block of backup image data or check
   status of a previously accepted request.

   Upon restore, backup kernel calls this method periodically sending
   consecutive blocks of data from the backup image. The @c table_no
   field in the buffer is set to indicate from which stream the data comes.
   Also, @c buf.last is TRUE if this is the last block
   in the stream.

   Blocks are sent to restore driver in the same order in which they were
   created by a backup driver. This is true also when only selected blocks are
   sent.

   Restore driver can implement its own policy of handling data processing
   requests. It is possible that it returns immediately from the call and uses
   a separate thread to process data in the buffer and it is also possible that
   the calling thread is used to do the job.

   Returning OK means that the data has been successfully processed
   and the buffer can be re-used for further transfers. If method returns
   PROCESSING, it means that the request was accepted but is not
   completed yet. The buffer will not be used for other purposes until a further
   call to @c get_data() with the same buffer as argument returns OK.

   @param  buf   (in) buffer filled with backup data. Fields @c size,
                 @c table_no and @c last are set
                 accordingly.

   @retval OK    The data has been successfully processed - the buffer can be
                 used for other transfers.

   @retval DONE  Same as OK but also indicates that the restore process is
                 completed.

   @retval PROCESSING  The request was accepted but data is not processed yet -
                 further calls to @c send_data() are needed to
                 complete it. The buffer is blocked and can't be used for other
                 transfers.

   @retval BUSY  The request can not be processed right now. A call to
                 @c send_data() should be repeated later.

   @retval ERROR An error has happened. The request is cancelled and the buffer
                 can be used for other transfers.

   @see @c Buffer class.
  */

  virtual result_t  send_data(Buffer &buf) =0;

}; // Restore_driver


} // backup namespace

// export Backup/Restore_driver classes to global namespace

using backup::Backup_driver;
using backup::Restore_driver;


#endif
