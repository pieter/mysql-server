#ifndef _BACKUP_KERNEL_API_H
#define _BACKUP_KERNEL_API_H

#include <backup/api_types.h>
#include <backup/catalog.h>
#include <backup/stream.h>
#include <backup/logger.h>


/*
  Called from the big switch in mysql_execute_command() to execute
  backup related statement
 */
int execute_backup_command(THD*, LEX*);

/**
  @file

  Functions and types forming the backup kernel API

 */

namespace backup {

class Archive_info;
class Backup_info;
class Restore_info;

} // backup namespace

// Backup kernel API

int mysql_show_archive(THD*,const backup::Archive_info&);
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
  Specialization of @c Archive_info which adds methods for selecting items
  to backup.

  When Backup_info object is created it is empty and ready for adding items
  to it. Methods @c add_table() @c add_db(), @c add_dbs() and @c add_all_dbs()
  can be used for that purpose (currently only databases and tables are
  supported). After populating info object with items it should be "closed"
  with a call to @c close() method. After that it is ready for use as a
  description of backup archive to be created.

  A linked list of all meta-data items is pointed by @c m_items member. It
  consists of three parts: first all the global items, then all per-database
  items and finally all per-table items. Inside each part, items are stored in
  dependency order so that if item A depends on B then B is before A in the
  list (currently dependencies are not checked). One should iterate through the
  meta-data item list using @c Backup_info::Item_iterator class.
 */

class Backup_info: public Archive_info, public Logger
{
  class Table_ref;
  class Db_ref;

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

  int save(OStream&);

  int add_dbs(List< ::LEX_STRING >&);
  int add_all_dbs();

  bool close();

   class Item_iterator;  // for iterating over all meta-data items

 private:

  /// State of the info structure.
  enum {INIT,   // structure ready for filling
        READY,  // structure ready for backup (tables opened)
        DONE,   // tables are closed
        ERROR
       }   m_state;

  int find_image(const Table_ref&);

  int default_image_no; ///< Position of the default image in @c images list, -1 if not used.
  int snapshot_image_no; ///< Position of the snapshot image in @c images list, -1 if not used.

  Db_item*    add_db(const backup::Db_ref&);
  Table_item* add_table(const Table_ref&);

  /// Value returned by @c add_table if it decides that the table should be skipped.
  static const Table_item *const skip_table;

  int add_db_items(Db_item&);
  int add_table_items(Table_item&);

  THD    *m_thd;
  TABLE  *i_s_tables;

  Item   *m_items;
  Item   *m_last_item;
  Item   *m_last_db;

  friend class Item_iterator;
};

class Backup_info::Item_iterator: public Archive_info::Item::Iterator
{
 public:
  Item_iterator(const Backup_info &info):
    Archive_info::Item::Iterator(info.m_items)
  {}
};

/**
  Specialization of @c Archive_info which is used to select and restore items
  from a backup archive.

  An instance of this class is created by reading backup archive header and it
  describes contents of the archive. @c Restore_info methods select which items
  should be restored. Instances of @c Restore_info::Item class are created when
  reading meta-data info stored in the archive. They are used to restore the
  meta-data items (but not the table data, which is done by restore drivers).

  @note This class is not fully implemented. Right now it is not possible to
  select items to restore - always all items are restored.
 */

class Restore_info: public Archive_info, public Logger
{
  bool m_valid;

 public:

  Restore_info(IStream &s): Logger(Logger::RESTORE), m_valid(TRUE)
  {
    result_t res= read(s);
    if (res == ERROR)
    {
      report_error(ER_BACKUP_READ_HEADER);
      m_valid= FALSE;
    }
  }

  bool is_valid() const
  { return m_valid; }

  int restore_all_dbs()
  { return 0; }

  /// Determine if given item is selected for restore.
  bool selected(const Archive_info::Item&)
  { return TRUE; }
};

} // backup namespace

#endif
