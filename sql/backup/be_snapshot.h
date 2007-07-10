#ifndef _SNAPSHOT_BACKUP_H
#define _SNAPSHOT_BACKUP_H

#include "archive.h"        
#include "buffer_iterator.h"
#include "be_default.h"

namespace snapshot_backup {

using backup::byte;
using backup::result_t;
using backup::version_t;
using backup::Table_list;
using backup::Table_ref;
using backup::Buffer;

/**
 * @class Engine
 *
 * @brief Encapsulates snapshot online backup/restore functionality.
 *
 * This class is used to initiate the snapshot backup algorithm, which is used
 * by the backup kernel to create a backup image of data stored in any
 * engine that does not have a native backup driver but supports consisten reads.
 * It may also be used as an option by the user.
 *
 * Using this class, the caller can create an instance of the snapshot backup
 * backup and restore class. The backup class is used to backup data for a
 * list of tables. The restore class is used to restore data from a
 * previously created snapshot backup image.
 */
class Engine: public Backup_engine
{
  public:
    Engine(THD *t_thd) { m_thd= t_thd; }

    /// Return version of backup images created by this engine.
    const version_t version() const { return 0; };
    result_t get_backup(const uint32, const Table_list &tables, Backup_driver*
&drv);
    result_t get_restore(const version_t ver, const uint32, const Table_list &tables,
                         Restore_driver* &drv);
  private:
    THD *m_thd;     ///< Pointer to the current thread.
};

/**
 * @class Backup
 *
 * @brief Contains the snapshot backup algorithm backup functionality.
 *
 * The backup class is a row-level backup mechanism designed to perform
 * a table scan on each table reading the rows and saving the data to the
 * buffer from the backup algorithm using a consistent read transaction.
 *
 * @see <backup driver>
 */
class Backup: public default_backup::Backup
{
  public:
    Backup(const Table_list &tables, THD *t_thd): 
      default_backup::Backup(tables, t_thd, TL_READ) {};
    virtual ~Backup() {};
    result_t begin(const size_t) { return backup::OK; };
    result_t end();
    result_t prelock() { return backup::READY; }
    result_t lock();
    result_t unlock();
};

/**
 * @class Restore
 *
 * @brief Contains the snapshot backup algorithm restore functionality.
 *
 * The restore class is a row-level backup mechanism designed to restore
 * data for each table by writing the data for the rows from the
 * buffer given by the backup algorithm.
 *
 * @see <restore driver>
 */
class Restore: public default_backup::Restore
{
  public:
    Restore(const Table_list &tables, THD *t_thd):
      default_backup::Restore(tables, t_thd){};
    virtual ~Restore(){};
    void free() { delete this; };
};
} // snapshot_backup namespace


/*********************************************************************

  Snapshot image class

 *********************************************************************/

namespace backup {


class Snapshot_image: public Image_info
{
 public:

  Snapshot_image(Archive_info &info): Image_info(info)
  { ver= 1; }

  image_type type() const
  { return SNAPSHOT_IMAGE; }

  const char* name() const
  { return "Snapshot"; }

  bool accept(const Table_ref&, const ::handlerton* h)
  { 
    return (h->start_consistent_snapshot != NULL); 
  }; // accept all tables that support consistent read

  result_t get_backup_driver(Backup_driver* &ptr)
  { return (ptr= new snapshot_backup::Backup(tables,::current_thd)) ? OK : ERROR; }

  result_t get_restore_driver(Restore_driver* &ptr)
  { return (ptr= new snapshot_backup::Restore(tables,::current_thd)) ? OK : ERROR; }

  result_t do_write_description(OStream&)
  { return OK; } // nothing to write

  static result_t
  create_from_stream(version_t, Archive_info &info, IStream&,
                     Image_info* &ptr)
  {
    return (ptr= new Snapshot_image(info)) ? OK : ERROR;
  }

  bool is_valid(){ return TRUE; };

};

} // backup namespace


#endif

