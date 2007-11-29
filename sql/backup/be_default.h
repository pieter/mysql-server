#ifndef _DEFAULT_BACKUP_H
#define _DEFAULT_BACKUP_H

#include <backup/backup_engine.h>
#include "catalog.h"  // to define default backup image class
#include "buffer_iterator.h"
#include "backup_aux.h"
#include "mysql_priv.h"
#include "be_thread.h"

namespace default_backup {

using backup::byte;
using backup::result_t;
using backup::version_t;
using backup::Table_list;
using backup::Table_ref;
using backup::Buffer;

const size_t META_SIZE= 1;

/*
  The following are the flags for the first byte in the data layout for
  the default and consistent snapshot algorithms. They describe what is 
  included in the buffer going to the kernel.
*/
const byte RCD_ONCE=    1U;     // Single data block for record data
const byte RCD_FIRST=  (1U<<1); // First data block in buffer for record buffer
const byte RCD_DATA=   (1U<<2); // Intermediate data block for record buffer
const byte RCD_LAST=   (1U<<3); // Last data block in buffer for record buffer
const byte BLOB_ONCE=   3U;     // Single data block for blob data
const byte BLOB_FIRST= (3U<<1); // First data block in buffer for blob buffer
const byte BLOB_DATA=  (3U<<2); // Intermediate data block for blob buffer
const byte BLOB_LAST=  (3U<<3); // Last data block in buffer for blob buffer

/**
 * @class Engine
 *
 * @brief Encapsulates default online backup/restore functionality.
 *
 * This class is used to initiate the default backup algorithm, which is used
 * by the backup kernel to create a backup image of data stored in any
 * engine that does not have a native backup driver. It may also be used as
 * an option by the user.
 *
 * Using this class, the caller can create an instance of the default backup
 * backup and restore class. The backup class is used to backup data for a
 * list of tables. The restore class is used to restore data from a
 * previously created default backup image.
 */
class Engine: public Backup_engine
{
  public:
    Engine(THD *t_thd);

    /*
      Return version of backup images created by this engine.
    */
    const version_t version() const { return 0; };
    result_t get_backup(const uint32, const Table_list &tables, 
                        Backup_driver* &drv);
    result_t get_restore(const version_t ver, const uint32, const Table_list &tables,
                         Restore_driver* &drv);

    /*
     Free any resources allocated by the default backup engine.
    */
    void free() { delete this; }
  private:
    THD *m_thd; ///< Pointer to the current thread.
};

/**
 * @class Backup
 *
 * @brief Contains the default backup algorithm backup functionality.
 *
 * The backup class is a row-level backup mechanism designed to perform
 * a table scan on each table reading the rows and saving the data to the
 * buffer from the backup algorithm.
 *
 * @see <backup driver> and <backup thread driver>
 */
class Backup: public Backup_thread_driver
{
  public:
    enum has_data_info { YES, WAIT, EOD };
    Backup(const Table_list &tables, THD *t_thd, thr_lock_type lock_type);
    virtual ~Backup() { backup::free_table_list(all_tables); }; 
    size_t size()  { return UNKNOWN_SIZE; };
    size_t init_size() { return 0; };
    result_t  begin(const size_t) { return backup::OK; };
    result_t end() { return backup::OK; };
    result_t get_data(Buffer &buf);
    result_t lock() { return backup::OK; };
    result_t unlock() { return backup::OK; };
    result_t cancel() { return backup::OK; };
    TABLE_LIST *get_table_list() { return all_tables; }
    void free() { delete this; };
    result_t prelock(); 

 protected:
    TABLE *cur_table;              ///< The table currently being read.
    my_bool init_phase_complete;   ///< Used to identify end of init phase.
    my_bool locks_acquired;        ///< Used to help kernel synchronize drivers.

  private:
    /*
      We use an enum to control the flow of the algorithm. Each mode 
      invokes a different behavior through a large switch. The mode is
      set in the code as a response to conditions or flow of data.
    */
    typedef enum {
      INITIALIZE,                  ///< Indicates time to initialize read
      GET_NEXT_TABLE,              ///< Open next table in the list
      READ_RCD,                    ///< Reading rows from table mode
      READ_RCD_BUFFER,             ///< Buffer records mode
      CHECK_BLOBS,                 ///< See if record has blobs
      READ_BLOB,                   ///< Reading blobs from record mode
      READ_BLOB_BUFFER             ///< Buffer blobs mode
    } BACKUP_MODE;

    result_t start_tbl_read(TABLE *tbl);
    result_t end_tbl_read();
    int next_table();
    BACKUP_MODE mode;              ///< Indicates which mode the code is in
    int tbl_num;                   ///< The index of the current table.
    handler *hdl;                  ///< Pointer to table handler.
    uint *cur_blob;                ///< The current blob field.
    uint *last_blob_ptr;           ///< Position of last blob field.
    MY_BITMAP *read_set;           ///< The file read set.
    Buffer_iterator rec_buffer;    ///< Buffer iterator for windowing records
    Buffer_iterator blob_buffer;   ///< Buffer iterator for windowing BLOB fields
    byte *ptr;                     ///< Pointer to blob data from record.
    TABLE_LIST *all_tables;        ///< Reference to list of tables used.

    uint pack(byte *rcd, byte *packed_row);
};

/**
 * @class Restore
 *
 * @brief Contains the default backup algorithm restore functionality.
 *
 * The restore class is a row-level backup mechanism designed to restore
 * data for each table by writing the data for the rows from the
 * buffer given by the backup algorithm.
 *
 * @see <restore driver>
 */
class Restore: public Restore_driver
{
  public:
    enum has_data_info { YES, WAIT, EOD };
    Restore(const Table_list &tables, THD *t_thd);
    virtual ~Restore() { backup::free_table_list(all_tables); };
    result_t  begin(const size_t) { return backup::OK; };
    result_t  end();
    result_t  send_data(Buffer &buf);
    result_t  cancel() { return backup::OK; };
    TABLE_LIST *get_table_list() { return all_tables; }
    void free() { delete this; };

 private:
     /*
      We use an enum to control the flow of the algorithm. Each mode 
      invokes a different behavior through a large switch. The mode is
      set in the code as a response to conditions or flow of data.
    */
    typedef enum {
      INITIALIZE,                  ///< Indicates time to initialize read
      GET_NEXT_TABLE,              ///< Open next table in the list
      WRITE_RCD,                   ///< Writing rows from table mode
      CHECK_BLOBS,                 ///< See if record has blobs
      WRITE_BLOB,                  ///< Writing blobs from record mode
      WRITE_BLOB_BUFFER            ///< Buffer blobs mode
    } RESTORE_MODE;

    result_t truncate_table(TABLE *tbl);
    int next_table();
    RESTORE_MODE mode;             ///< Indicates which mode the code is in
    uint tbl_num;                  ///< The index of the current table.
    uint32 max_blob_size;          ///< The total size (sum of parts) for the blob.
    TABLE *cur_table;              ///< The table currently being read.
    handler *hdl;                  ///< Pointer to table handler.
    uint *cur_blob;                ///< The current blob field.
    uint *last_blob_ptr;           ///< Position of last blob field.
    Buffer_iterator rec_buffer;    ///< Buffer iterator for windowing records
    Buffer_iterator blob_buffer;   ///< Buffer iterator for windowing BLOB fields
    TABLE_LIST *tables_in_backup;  ///< List of tables used in backup.
    byte *blob_ptrs[MAX_FIELDS];   ///< List of blob pointers used
    int blob_ptr_index;            ///< Position in blob pointer list
    THD *m_thd;                    ///< Pointer to current thread struct.
    TABLE_LIST *all_tables;        ///< Reference to list of tables used.

    uint unpack(byte *packed_row);
};
} // default_backup namespace


/*********************************************************************

  Default image class

 *********************************************************************/

namespace backup {


class Default_image: public Image_info
{
 public:

  Default_image(Archive_info &info): Image_info(info)
  { ver= 1; }

  image_type type() const
  { return DEFAULT_IMAGE; }

  const char* name() const
  { return "Default"; }

  bool accept(const Table_ref&, const ::handlerton*)
  { return TRUE; }; // accept all tables

  result_t get_backup_driver(Backup_driver* &ptr)
  { return (ptr= new default_backup::Backup(tables,::current_thd, 
                                            TL_READ_NO_INSERT)) ? OK : ERROR; }

  result_t get_restore_driver(Restore_driver* &ptr)
  { return (ptr= new default_backup::Restore(tables,::current_thd)) ? OK : ERROR; }

  result_t do_write_description(OStream&)
  { return OK; } // nothing to write

  static result_t
  create_from_stream(version_t, Archive_info &info, IStream&,
                     Image_info* &ptr)
  {
    return (ptr= new Default_image(info)) ? OK : ERROR;
  }

  bool is_valid(){ return TRUE; };

};

} // backup namespace


#endif

