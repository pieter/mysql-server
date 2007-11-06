#ifndef _BACKUP_ARCHIVE_H
#define _BACKUP_ARCHIVE_H

/**
  @file

  Data types used to represent contents of a backup archive and to read/write
  its description (catalogue)
 */

#if defined(USE_PRAGMA_INTERFACE) || defined(__APPLE_CC__)
/*
  #pragma interface is needed on powermac platform as otherwise compiler 
  doesn't create/export vtable for Image_info::Tables class (if you know a 
  better way for fixing this issue let me know! /Rafal).
  
  Apparently, configuration macro USE_PRAGMA_INTERFACE is not set by ./configure,
  on powermac platform - this is why __APPLE_CC__ is also checked.
 */ 
#pragma interface
#endif

#include <backup/api_types.h>
#include <backup/string_pool.h>
#include <backup/stream.h>
#include <backup/backup_engine.h>
#include <backup/meta_backup.h>

namespace backup {

// Forward declaration for a class describing an image inside backup archive.
class Image_info;

#define MAX_IMAGES  256
typedef Image_info* Img_list[MAX_IMAGES]; ///< List (vector) of image descriptions.

/**
  Describes contents of a backup archive.

  This class stores a catalogue of a backup archive, that is, description of
  all items stored in the archive (currently only databases and tables). It also
  determines how to save and read the catalogue to/from a backup stream.

  Only item names are stored in the catalogue. Other item data is stored
  in the meta-data part of an archive and in case of tables, their data is
  stored in images created by backup drivers.

  The @c images member stores a list of @c Image_info objects describing the
  images included in the archive. Each image description contains a list of
  tables stored in that image (note that no table can be stored in more than
  one image).

  To save space, we have a separate pool of database names (@c db_names member).
  In table references, only the key of the database name is stored, not the
  whole name.

  When reading or writing backup archive, statistics about the size of its parts
  is stored in the members of this class for later reporting.
 */
class Archive_info
{
 public:
 
   static const version_t  ver=1;
   uint  img_count;       ///< number of images in the archive
   uint  table_count;     ///< total number of tables in the archive

   size_t total_size;   ///< size of processed backup archive
   size_t header_size;  ///< size of archive's header (after reading or writing an archive)
   size_t meta_size;    ///< size of archive's meta-data (after reading or writing an archive)
   size_t data_size;    ///< size of archive's table data images (after reading or writing an archive)

   // Classes representing various types of meta-data items.

   class Item;
   class Db_item;
   class Table_item;

  /*
    Classes which might be used to implement contents browsing.

   class Item_iterator;  // for iterating over all meta-data items
   class Db_iterator;    // iterates over databases in archive
   class Ditem_iterator; // iterates over per-db items
  */

   Img_list images;  ///< list of archive's images

   /// Write archive's header and save the catalogue.
   result_t save(OStream&);
   /// Read the header and catalogue from a stream.
   result_t read(IStream&);

   virtual ~Archive_info();

 protected:

   Archive_info():
     img_count(0), table_count(0),
     total_size(0), header_size(0), meta_size(0), data_size(0)
   {
     for (uint i=0; i<256; ++i)
       images[i]= NULL;
   }

   // storage for meta-data items

   StringPool      db_names; ///< Pool of database names.

 private:

  friend class Image_info;
  friend class Db_item;
  friend class Table_item;
};

/**
  Describes an image of table data stored in a backup archive.

  An instance of this class:
  - informs about the type of image,
  - stores list of tables whose data is kept in the image,
  - provides methods for creating backup and restore drivers to write/read the
    image,
  - determines which tables can be stored in the image,
  - defines how image's format is described inside backup archive
    (via @c do_write_description() method)
 */
class Image_info
{
 public:
 
  enum image_type {NATIVE_IMAGE, DEFAULT_IMAGE, SNAPSHOT_IMAGE};

  virtual image_type type() const =0; ///< Return type of the image.
  version_t ver;  ///< Image format version.

  /// Check if instance was correctly constructed
  virtual bool is_valid() =0;
  /// Create backup driver for the image.
  virtual result_t get_backup_driver(Backup_driver*&) =0;
  /// Create restore driver for the image.
  virtual result_t get_restore_driver(Restore_driver*&) =0;

  size_t init_size; ///< Size of the initial data transfer (estimate). This is
                    ///< meaningful only after a call to get_backup_driver().

  /// Write header entry describing the image.
  result_t write_description(OStream&);

  /**
    Create instance of @c Image_info described by an entry in backup stream.

    @retval OK    entry successfully read
    @retval DONE  end of chunk or stream has been reached.
    @retval ERROR an error was detected
   */
  static result_t create_from_stream(Archive_info&, IStream&, Image_info*&);

  /// Determine if a table stored in given engine can be saved in this image.
  virtual bool accept(const Table_ref&, const ::handlerton*) =0;

  /** 
    Return name identifying the image in debug messages.
   
    The name should fit into "%s backup/restore driver" pattern.
   */
  virtual const char* name() const
  { return "<Unknown>"; }

  virtual ~Image_info()
  {}

   /*
     Implementation of Table_list interface used to store the
     list of tables of an image. Database names are stored in
     external StringPool
    */
   class Tables: public Table_list
   {
    public:

     Tables(StringPool &db_names):
       m_db_names(db_names),
       m_head(NULL),m_last(NULL),m_count(0)
     {}

     ~Tables() { clear(); }

     int add(const backup::Table_ref&);
     void clear();

     backup::Table_ref operator[](uint pos) const;
     //::TABLE_LIST* get_table_ptr(uint pos) const;

     uint count() const
     { return m_count; }

     result_t save(OStream&);
     result_t read(IStream&);

    private:

     struct node;

     int add(const StringPool::Key&, const String&);
     node* find_table(uint pos) const;

     StringPool &m_db_names;
     node *m_head, *m_last;
     uint m_count;

     friend class Table_ref;
     friend class  Archive_info::Table_item;
   };

  Tables tables; ///< List of tables stored in the image.

 protected:

  Image_info(Archive_info &info):
    init_size(Driver::UNKNOWN_SIZE), tables(info.db_names)
  {}

  /**
    Write image specific data describing it.

    Method redefined in subclasses corresponding to different image types.
   */
  virtual result_t do_write_description(OStream&) =0;
};

/**
  Represents a meta-data item in a backup archive.

  Instances of this class:

  - identify a meta-data item inside backup archive,
  - provide storage for a corresponding meta::Item instance,
  - write item identification data to a backup stream.

  For each type of meta-data there is a specialized subclass of
  @c Archive_info::Item implementing the above tasks. Each subclass has static
  @c create_from_stream() method which can create class instance using an
  identity stored in a stream. For examples, see @c Archive_info::Table_item
  class.

  Class @c Archive_info::Item defines the format of an entry describing a
  meta-data item inside the meta-data part of an archive. Such entry is created
  by @c Archive_info::save() method. These entries are read by
  @c Restore_info::read_item() method.
 */

class Archive_info::Item
{
 protected:

  /// Pointer to @c Archive_info instance to which this item belongs.
  const Archive_info *const m_info;
  Item  *next; ///< Used to create a linked list of all meta-data items.

 public:

  virtual ~Item() {}

  /// Returns reference to the corresponding @c meta::Item instance.
  virtual meta::Item& meta() =0;

  result_t save(THD*,OStream&); ///< Write entry describing the item.

  // Create item from a saved entry.
  static result_t create_from_stream(const Archive_info&, IStream&, Item*&);

  class Iterator;

 protected:

  Item(const Archive_info &i): m_info(&i), next(NULL)
  {}

  /// Save data identifying the item inside the archive.
  virtual result_t save_id(OStream&) =0;

  friend class Archive_info;
  friend class Backup_info;
  friend class Restore_info;
  friend class Iterator;
};


/**
  Used to iterate over meta-data items.

  Usage:
  @code
   Item *head;
   for (Item::Iterator it(head); it ; it++)
   {
     it->archive_item_method()
   }
  @endcode
  or
  @code
   Item *head, *p;
   Item::Iterator it(head);

   while ((p=it++))
   {
     @<use p here>
   }
  @endcode
 */
class Archive_info::Item::Iterator
{
  Item *m_curr;
  Item *m_prev;

 public:

  Iterator(Item *const head): m_curr(head), m_prev(NULL)
  {}

  operator bool() const
  { return m_curr != NULL; }

  Item* operator++(int)
  {
    m_prev= m_curr;

    if (m_curr)
     m_curr= m_curr->next;

    return m_prev;
  }

  Item* operator->()
  { DBUG_ASSERT(m_curr);
    return m_curr; }
};



/**
  Specialization of @c Archive_info::Item representing a database.

  A database is identified by a key into Archive_info::db_names string pool.
  Using the key one can read database name from the pool. The key is saved
  as a var-length coded integer.
 */
class Archive_info::Db_item:
  public Archive_info::Item, public meta::Db, public Db_ref
{
  StringPool::Key   key;

  Db_item(const Archive_info &i, const StringPool::Key &key):
    Archive_info::Item(i), Db_ref(m_info->db_names[key]), key(key)
  {}

  meta::Item& meta()
  { return *this; }

  /// Get the name from @c db_names pool.
  const char* sql_name() const
  { return m_info->db_names[key].ptr(); }

 public:

  result_t save_id(OStream&);
  /// Create instance reading its identity from a stream.
  static
  result_t create_from_stream(const Archive_info&,IStream&, Archive_info::Item*&);

  friend class Archive_info;
  friend class Backup_info;
};


/**
  Specialization of @c Archive_info::Item representing a table.

  A table is identified by its position inside the table list of
  one of archive's images. Its identity is saved as two var-length coded
  integers: first being the image number and second the table position inside
  image's table list.
 */
class Archive_info::Table_item:
  public Archive_info::Item, public meta::Table, public Table_ref
{
  uint  img;  ///< Image in which this table is saved.
  uint  pos;  ///< Position of the table in image's table list.

  Table_item(const Archive_info &i, uint no, uint tno):
    Archive_info::Item(i), Table_ref(i.images[no]->tables[tno]),
    img(no), pos(tno)
  {}

 public:

  meta::Item& meta()
  { return *this; }

  const char* sql_name() const
  { return m_info->images[img]->tables[pos].name().ptr(); }

  /// Table is a per-db item -- indicate to which database it belongs.
  const Db_ref in_db()
  { return m_info->images[img]->tables[pos].db(); }

  result_t save_id(OStream&);
  /// Create instance reading its identity from a stream.
  static
  result_t create_from_stream(const Archive_info&,IStream&, Archive_info::Item*&);

  friend class Backup_info;
};

} // backup namespace


/************************************************************

   Class describing native backup image

 ************************************************************/

namespace backup {

/**
  Specialization of @c Image_info for images created by native backup drivers.
 */
class Native_image: public Image_info
{
  const ::handlerton  *m_hton; ///< Pointer to storage engine.
  Engine   *m_be;  ///< Pointer to the native backup engine.

  const char *m_name;  ///< Used to identify image in debug messages.

 public:

  Native_image(Archive_info &info, const ::handlerton *hton):
    Image_info(info), m_hton(hton)
  {
    DBUG_ASSERT(hton);
    DBUG_ASSERT(hton->get_backup_engine);

    hton->get_backup_engine(const_cast< ::handlerton* >(hton),m_be);

    if(m_be)
    {
      ver= m_be->version();
      m_name= ::ha_resolve_storage_engine_name(hton);
    }
  }

  bool is_valid()
  { return m_be != NULL; }

  image_type type() const
  { return NATIVE_IMAGE; }

  const char* name() const
  { return m_name; }

  result_t get_backup_driver(Backup_driver* &drv)
  {
    DBUG_ASSERT(m_be);
    return m_be->get_backup(Driver::PARTIAL,tables,drv);
  }

  result_t get_restore_driver(Restore_driver* &drv)
  {
    DBUG_ASSERT(m_be);
    return m_be->get_restore(ver,Driver::PARTIAL,tables,drv);
  }

  result_t do_write_description(OStream&);
  static result_t create_from_stream(version_t,Archive_info&,IStream&,Image_info*&);

  bool accept(const Table_ref&, const ::handlerton *hton)
  { return hton == m_hton; }; // this assumes handlertons are single instance objects!
};

} // backup namespace


#endif
