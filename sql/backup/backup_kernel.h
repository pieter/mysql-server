#ifndef _BACKUP_KERNEL_API_H
#define _BACKUP_KERNEL_API_H

#include <backup/api_types.h>
#include <backup/catalog.h>
#include <backup/logger.h>

/**
  @file

  Functions and types forming the backup kernel API
*/


/**
  @brief Size of the buffer used for transfers between backup kernel and
  backup/restore drivers.
*/
#define DATA_BUFFER_SIZE  (1024*1024)

/*
  Called from the big switch in mysql_execute_command() to execute
  backup related statement
*/
int execute_backup_command(THD*, LEX*);

namespace backup {

class Image_info;
class Backup_info;
class Restore_info;

} // backup namespace

// Backup kernel API

int mysql_backup(THD*, backup::Backup_info&, backup::OStream&);
int mysql_restore(THD*, backup::Restore_info&, backup::IStream&);


namespace backup {

/**
  Represents a location where backup archive can be stored.

  The class is supposed to represent the location on the abstract level
  so that it is easier to add new types of locations.

  Currently we support only files on the server's file system. Thus the
  only type of location is a path to a file.
 */

struct Location
{
  /// Type of location
  enum enum_type {SERVER_FILE, INVALID};
  bool read;

  virtual enum_type type() const
  { return INVALID; }

  virtual ~Location() {}  // we want to inherit this class

  /// Deallocate any resources used by Location object.
  virtual void free()
  { delete this; }  // use destructor to free resources

  /// Describe location for debug purposes
  virtual const char* describe()
  { return "Invalid location"; }

  /**
    Interpret string passed to BACKUP/RESTORE statement as backup location
    and construct corresponding Location object.

    @returns NULL if the string doesn't denote a valid location
   */
  static Location* find(const LEX_STRING&);
};


/**
  Specialization of @c Image_info which adds methods for selecting items
  to backup.

  When Backup_info object is created it is empty and ready for adding items
  to it. Methods @c add_table() @c add_db(), @c add_dbs() and @c add_all_dbs()
  can be used for that purpose (currently only databases and tables are
  supported). After populating info object with items it should be "closed"
  with a call to @c close() method. After that it is ready for use as a
  description of backup archive to be created.
*/
class Backup_info: public Image_info, public Logger
{
 public:

  Backup_info(THD*);
  ~Backup_info();

  bool is_valid()
  {
    bool ok= TRUE;

    switch (m_state) {

    case ERROR:
      ok= FALSE;
      break;

    case INIT:
      ok= (i_s_tables != NULL);
      break;

    default:
      ok= TRUE;
    }

    if (!ok)
      m_state= ERROR;

    return ok;
  }

  int add_dbs(List< ::LEX_STRING >&);
  int add_all_dbs();

  bool close();

 private:

  /// State of the info structure.
  enum {INIT,   // structure ready for filling
        READY,  // structure ready for backup (tables opened)
        DONE,   // tables are closed
        ERROR
       }   m_state;

  int find_backup_engine(const ::TABLE *const, const Table_ref&);

  Table_item* add_table(Db_item&, const Table_ref&);

  int add_db_items(Db_item&);
  int add_table_items(Table_item&);

  THD    *m_thd;
  TABLE  *i_s_tables;
  String binlog_file_name; ///< stores name of the binlog at VP time

  /**
    @brief Storage for table and database names.

    When adding tables or databases to the backup catalogue, their names
    are stored in String objects, and these objects are appended to this
    list so that they can be freed when Backup_info object is destroyed.
  */
  // FIXME: use better solution, e.g., MEM_ROOT
  List<String>  name_strings;

  void save_binlog_pos(const ::LOG_INFO &li)
  {
    binlog_file_name= li.log_file_name;
    binlog_file_name.copy();
    binlog_pos.pos= li.pos;
    binlog_pos.file= binlog_file_name.c_ptr();
  }

  friend int write_table_data(THD*, Backup_info&, OStream&);
};


/**
  Specialization of @c Image_info which is used to select and restore items
  from a backup image.

  An instance of this class is created by reading backup image header and it
  describes its contents. @c Restore_info methods select which items
  should be restored.

  @note This class is not fully implemented. Right now it is not possible to
  select items to restore - always all items are restored.
 */

class Restore_info: public Image_info, public Logger
{
  bool m_valid;
  THD  *m_thd;
  const Db_ref *curr_db;

  CHARSET_INFO *system_charset;
  bool same_sys_charset;

 public:

  Restore_info(THD*, IStream&);
  ~Restore_info();

  bool is_valid() const
  { return m_valid; }

  int restore_all_dbs()
  { return 0; }

  /// Determine if given item is selected for restore.
  bool selected(const Image_info::Item&)
  { return TRUE; }

  result_t restore_item(Item&, ::String&, byte*, byte*);

  friend int restore_table_data(THD*, Restore_info&, IStream&);
  friend int ::bcat_add_item(st_bstream_image_header*,
                             struct st_bstream_item_info*);
};

} // backup namespace

#endif
