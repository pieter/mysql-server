#ifndef CATALOG_H_
#define CATALOG_H_

#include "stream_services.h"
#include "stream.h"
#include "backup_aux.h"
#include <meta_data.h>

namespace backup {

class Snapshot_info;

/**
  Describes contents of a backup image.

  This class stores a catalogue of a backup image, that is, description of
  all items stored in it (currently only databases and tables).

  Only item names are stored in the catalogue. Other item data is stored
  in the meta-data part of the image and in case of tables, their data is
  stored in table data snapshots created by backup drivers.

  For each snapshot present in the image there is a @c Snapshot_info object
  stored in @c m_snap[] array. This object contains list of tables whose
  data is stored in it. Note that each table must belong to exactly one
  snapshot.

  Contents of the catalogue can be browsed using the iterator classes.

  Info about each object is stored in an instance of a class derived from
  @c Image_info::Item. This class determines how to obtain meta-data for the
  object and how to create it from the saved meta-data.
 */
class Image_info: public st_bstream_image_header
{
 public:

   ulonglong  backup_prog_id; ///< id of the BACKUP/RESTORE operation
   uint  table_count;     ///< total number of tables in the archive
   size_t data_size;      ///< how much of table data is in the image.

   // Classes representing various types of meta-data items.

   class Item;          ///< base class for all item types
   class Db_item;
   class Table_item;

   class Iterator;       ///< base for all iterators
   class Db_iterator;    ///< iterates over databases in archive
   class Ditem_iterator; ///< iterates over per-db items

   virtual ~Image_info();

   void save_start_time(const time_t time)
   { save_time(time, start_time); }
   
   void save_end_time(const time_t time)
   { save_time(time, end_time); }

   void save_vp_time(const time_t time)
   { save_time(time, vp_time); }

 protected:

  class Db_ref;
  class Table_ref;

  /**
   Provides storage for the list of databases stored in the catalogue.
  */
  class Databases
  {
    Dynamic_array<Db_item> m_dbs;

   public:

    Databases(): m_dbs(16,128)
    {}

    Db_item* operator[](uint pos) const
    { return m_dbs[pos]; }

    uint count() const
    { return m_dbs.size(); }

    /// Insert database at given location
    Image_info::Db_item* add_db(const Db_ref &db, uint pos);

    /// Insert database at first available position.
    Image_info::Db_item* add_db(const Db_ref &db)
    {
      return add_db(db,count());
    }

  };

  Databases m_db; ///< list of databases
  Snapshot_info *m_snap[256];   ///< list of snapshots

  Image_info();

 public:

  uint db_count() const
  { return m_db.count(); }

  /*
    Methods for populating backup catalogue (just wrappers which access m_db
    member)
  */

  Db_item* add_db(const Db_ref &db, uint pos)
  {
    return m_db.add_db(db,pos);
  }

  Db_item* add_db(const Db_ref &db)
  {
    return add_db(db,m_db.count());
  }

  Db_item* get_db(uint pos) const
  {
    return m_db[pos];
  }

  /*
    Inline methods for adding/accessing table items are defined after
    Snapshot_info class.
   */

  Table_item* add_table(Db_item&, const Table_ref&, uint, unsigned long int);
  Table_item* add_table(Db_item&, const Table_ref&, uint);
  Table_item* get_table(uint, unsigned long int) const;

  Item* locate_item(const st_bstream_item_info*) const;

 private:

  /**
    Buffer for CREATE statement used in @c bcat_get_item_create_query()

    FIXME: find a better solution.
  */
  String create_stmt_buf;


  void save_time(const time_t, bstream_time_t&);
  
  // friends

  friend int ::bcat_add_item(st_bstream_image_header*, struct st_bstream_item_info*);
  friend int ::bcat_reset(st_bstream_image_header*);
  friend int ::bcat_get_item_create_query(st_bstream_image_header*,
                                          struct st_bstream_item_info*,
                                          bstream_blob *);
};


/*
 Provides storage for list of tables of a snapshot.

 Implements the Table_list interface.
*/

class Tables: public Table_list
{
  Dynamic_array<Image_info::Table_item> m_tables;

 public:

  Tables(): m_tables(1024,1024)
  {}

  backup::Table_ref operator[](uint) const;

  uint count() const
  { return m_tables.size(); }

  Image_info::Table_item* add_table(const Table_ref&, unsigned long int);

  Image_info::Table_item* add_table(const Table_ref &t)
  {
    return add_table(t,count());
  }

  Image_info::Table_item* get_table(unsigned long int pos)
  {
    return m_tables[pos];
  }
};


/*
  Describes table data snapshot stored inside backup image.

  Such snapshot is created by a backup driver and read by a restore driver.
 */
class Snapshot_info
{
 protected:

  Tables m_tables;  ///< list of tables whose data is stored in the snapshot

 public:

  enum enum_snap_type {
    NATIVE_SNAPSHOT= BI_NATIVE,   ///< snapshot created by native backup driver
    DEFAULT_SNAPSHOT= BI_DEFAULT, ///< snapshot created by built-in, blocking driver
    CS_SNAPSHOT= BI_CS            ///< snapshot created by CS backup driver
  };

  version_t version;  ///< version of snapshot's format

  virtual enum_snap_type type() const =0;

  /// Check if instance was correctly constructed
  virtual bool is_valid() =0;

  /// Tell how many tables are stored in the snapshot.
  unsigned long int table_count() const
  { return m_tables.count(); }

  /// Determine if a table stored in given engine can be saved in this image.
  virtual bool accept(const Table_ref&, const ::handlerton*) =0;

  /// Create backup driver for the image.
  virtual result_t get_backup_driver(Backup_driver*&) =0;

  /// Create restore driver for the image.
  virtual result_t get_restore_driver(Restore_driver*&) =0;

  /// Set snapshot's data format version
  virtual result_t set_version(version_t ver)
  {
    version= ver;
    return OK;
  }

  /**
    Position inside image's snapshot list.

    Starts with 1. M_no == 0 means that this snapshot is not included in the
    list.
  */
  ushort m_no;

  /**
    Size of the initial data transfer (estimate). This is
    meaningful only after a call to get_backup_driver().
  */
  size_t init_size;

  /**
    Return name identifying the snapshot in debug messages.

    The name should fit into "%s backup/restore driver" pattern.
   */
  virtual const char* name() const
  { return "<Unknown>"; }

  virtual ~Snapshot_info()
  {}

 protected:

  Snapshot_info(): m_no(0), version(0)
  {}

  // Methods for adding and accessing tables stored in the table list.

  Image_info::Table_item* add_table(const Table_ref &t, unsigned long int pos)
  {
    return m_tables.add_table(t,pos);
  }

  Image_info::Table_item* add_table(const Table_ref &t)
  {
    return add_table(t,m_tables.count());
  }

  Image_info::Table_item* get_table(unsigned long int pos)
  {
    return m_tables.get_table(pos);
  }

 friend class Image_info;
};


/*
  Classes @c Image_info::Db_ref and @c Image_info::Table give access to
  protected constructors so that instances of these objects can be created.

  In the future they can be extended with helper methods and any features
  needed to implement the base classes.
 */

class Image_info::Db_ref: public backup::Db_ref
{
  public:

  Db_ref(const String &name): backup::Db_ref(name)
  {}
};

class Image_info::Table_ref: public backup::Table_ref
{
  public:

  Table_ref(const backup::Db_ref &db, const String &name):
   backup::Table_ref(db.name(),name)
  {}
};

/**
  Represents a meta-data item in a backup image.

  Instances of this class:

  - identify a meta-data item inside backup image,
  - provide storage for a corresponding meta::Item instance,

  For each type of meta-data there is a specialized subclass of
  @c Archive_info::Item implementing the above tasks. The subclass also stores
  all the item data required by the backup stream library.
*/
class Image_info::Item
{
 public:

  virtual ~Item() {}

  virtual const st_bstream_item_info* info() const =0;

  /**
    Tells to which database belongs this object.

    Returns NULL for global objects which don't belong to any database.
  */
  virtual const Db_ref *in_db(void)
  { return NULL; }

  virtual result_t get_create_stmt(::String &buf)
  { return meta().get_create_stmt(buf); }

  result_t drop(THD *thd)
  { return meta().drop(thd); }

  result_t create(THD *thd, ::String &query, byte *begin, byte *end)
  { return meta().create(thd,query,begin,end); }

 protected:

  String m_name;  ///< For storing object's name.

  Item() {}

 private:

  /// Returns reference to the corresponding @c meta::Item instance.
  virtual meta::Item& meta() =0;

  friend class Image_info;
};

/**
  Specialization of @c Image_info::Item for storing info about a database.
*/
class Image_info::Db_item
 : public st_bstream_db_info,
   public Image_info::Item,
   public meta::Db,
   public Db_ref
{
  Table_item *m_tables;
  Table_item *m_last_table;
  unsigned long int table_count;

 public:

  Db_item():
    Db_ref(Image_info::Item::m_name), m_tables(NULL), m_last_table(NULL)
  {
    bzero(&base,sizeof(base));
    base.type= BSTREAM_IT_DB;
  }

  const char* sql_name() const
  { return (const char*)base.name.begin; }

  /// Store information about database @c db.
  Db_item& operator=(const Db_ref &db)
  {
    Image_info::Item::m_name= db.name();   // save name of the db
    /*
      setup the name member (inherited from bstream_item_info) to point
      at the db name
     */
    base.name.begin= (byte*) Image_info::Item::m_name.ptr();
    base.name.end= base.name.begin + Image_info::Item::m_name.length();
    return *this;
  }

  meta::Item& meta() { return *this; }

  const st_bstream_item_info* info() const { return &base; }
  const st_bstream_db_info* db_info() const { return this; }

  result_t add_table(Table_item &t);

  friend class Ditem_iterator;

  friend int ::bcat_add_item(st_bstream_image_header*, struct st_bstream_item_info*);
};


/**
  Specialization of @c Image_info::Item for storing info about a table.
*/
class Image_info::Table_item
 : public st_bstream_table_info,
   public Image_info::Item,
   public meta::Table,
   public backup::Table_ref
{
  Table_item *next_table;
  String m_db_name;  // FIXME

 public:

  Table_item():
    Table_ref(m_db_name,Image_info::Item::m_name),
    next_table(NULL), tl_entry(NULL)
  {
    bzero(&base,sizeof(base));
    base.base.type= BSTREAM_IT_TABLE;
  }

  const char* sql_name() const
  { return (const char*)base.base.name.begin; }

  meta::Item& meta() { return *this; }

  /// Store information about table @c t.
  Table_item& operator=(const Table_ref &t)
  {
    m_db_name= t.db().name();
    Image_info::Item::m_name= t.name();   // save name of the db
    /*
      setup the name member (inherited from bstream_item_info) to point
      at the db name
     */
    base.base.name.begin= (byte*) Image_info::Item::m_name.ptr();
    base.base.name.end= base.base.name.begin + Image_info::Item::m_name.length();
    return *this;
  }

  const st_bstream_item_info* info() const { return &base.base; }
  const st_bstream_table_info* t_info() const { return this; }

  const Db_ref *in_db(void)
  {
    return static_cast<Db_item*>(base.db);
  }

  String  create_stmt;    ///< buffer to store table's CREATE statement

  /// Build CREATE statement for the table and store it in @c create_stmt
  result_t get_create_stmt()
  {
    if (tl_entry)
      return meta::Table::get_create_stmt(create_stmt);
    return ERROR;
  }

  /// Put in @c buf a CREATE statement for the table.
  result_t get_create_stmt(::String &buf)
  {
    if (create_stmt.is_empty() && tl_entry)
      get_create_stmt();

    buf= create_stmt;
    return OK;
  }

 private:

  /**
    Stores pointer to a filled TABLE_LIST structure when the table is opened.
    Otherwise should contain NULL.
  */
  ::TABLE_LIST *tl_entry;

  /// this method is used by meta::Table class to get the CREATE statement
  ::TABLE_LIST* get_table_list_entry()
  { return tl_entry; }

  friend class Db_item;
  friend class Ditem_iterator;
  friend class Tables;
  friend class Backup_info;

  friend
  int ::bcat_add_item(st_bstream_image_header*, struct st_bstream_item_info*);
};

/**
  Add table to given snapshot at the indicated location.

  @param  db  Database to which this table belongs.
  @param  t   Table description.
  @param  no  Snapshot to which it should be added (its position in
              @c m_snap[] array)
  @param  pos Position in snapshot's table list.

  @note Table is also added to the database's table list.

  @todo Report errors.
*/
inline
Image_info::Table_item*
Image_info::add_table(Db_item &db, const Table_ref &t, uint no, unsigned long int pos)
{
  Snapshot_info *snap= m_snap[no];

  DBUG_ASSERT(snap);

  Table_item *ti= snap->add_table(t,pos);
  if (!ti)
  {
    // TODO: report error
    return NULL;
  }

  // If the snapshot was not yet used, assign a new number to it
  if (snap->m_no == 0)
   snap->m_no= ++snap_count;

  ti->snap_no= snap->m_no-1;
  table_count++;
  db.add_table(*ti);

  return ti;
}

/**
  Add table to given snapshot at first available location.

  @param  db  Database to which this table belongs.
  @param  t   Table description.
  @param  no  Snapshot to which it should be added (its position in
              @c m_snap[] array)

  @note Table is also added to the database's table list.
*/
inline
Image_info::Table_item*
Image_info::add_table(Db_item &db, const Table_ref &t, uint no)
{
  return add_table(db,t,no,m_snap[no]->table_count());
}

/**
  Get @c Table_item object for the table in the given position.

  @param no  Snapshot to which the table belongs (its position in @c m_snap[]
             array)
  @param pos Tables position in the snapshot's table list.
*/
inline
Image_info::Table_item*
Image_info::get_table(uint no, unsigned long int pos) const
{
  uint i;

  // FIXME: avoid the loop
  for (i=0; i < 256; ++i)
    if (m_snap[i] && m_snap[i]->m_no == no+1)
      return m_snap[i]->get_table(pos);
  return NULL;
}

/**
  Add database to the catalogue, storing it at given position.

  @param db    Database to add.
  @param pos   Position where it should be stored in the database list.

  The position should be not occupied.

  @todo Report errors.
*/
inline
Image_info::Db_item*
Image_info::Databases::add_db(const Db_ref &db, uint pos)
{
  Db_item *di= m_dbs.get_entry(pos);
  if (!di)
  {
    // TODO: report error
    return NULL;
  }
  di->base.pos= pos;
  *di= db;
  return di;
}

/**
  Add table to the list storing it at given position.

  The position should not be occupied.

  @todo Report errors.
*/
inline
Image_info::Table_item*
Tables::add_table(const Table_ref &t, unsigned long int pos)
{
  Image_info::Table_item *it= m_tables.get_entry(pos);

  if (!it)
  {
    // TODO: report error
    return NULL;
  }

  it->base.base.pos= pos;
  *it= t; // store table info in the item
  return it;
}

/**
  Return table at given position.

  The position should not be empty.
*/
inline
Table_ref Tables::operator[](uint pos) const
{
  DBUG_ASSERT(pos < m_tables.size());

  const Image_info::Table_item *ti= m_tables[pos];
  DBUG_ASSERT(ti);

  return *ti;
}


class Image_info::Iterator
{
 protected:

  const Image_info &m_info;

 public:

  Iterator(const Image_info &info): m_info(info) {}

  const Item* operator++(int)
  {
    const Item *ptr= get_ptr();
    next();
    return ptr;
  }

  virtual ~Iterator() {}

 private:

  virtual const Item* get_ptr() const =0;
  virtual bool next() =0;
};


class Image_info::Db_iterator
 : public Image_info::Iterator
{
  uint pos;

 public:

  Db_iterator(const Image_info &info): Iterator(info)
  {
    pos= 0;
    find_non_null_pos();
  }

 private:

  const Item* get_ptr() const
  { return m_info.m_db[pos]; }

  bool next()
  {
    pos++;
    find_non_null_pos();
    return pos < m_info.m_db.count();
  }

  void find_non_null_pos()
  {
    for(; pos < m_info.m_db.count() && m_info.m_db[pos] == NULL; ++pos);
  }
};


class Image_info::Ditem_iterator
 : public Image_info::Iterator
{
  Table_item *ptr;

 public:

  Ditem_iterator(const Image_info &info, const Db_item &db): Iterator(info)
  {
    ptr= db.m_tables;
  }

 private:

  const Item* get_ptr() const
  { return ptr; }

  bool next()
  {
    if (ptr)
      ptr= ptr->next_table;

    return ptr != NULL;
  }

};

} // backup namespace

namespace backup {

/*
 Wrappers around backup stream functions which perform necessary type conversions.

 TODO: report errors
*/

inline
result_t
write_preamble(const Image_info &info, OStream &s)
{
  const st_bstream_image_header *hdr= static_cast<const st_bstream_image_header*>(&info);
  int ret= bstream_wr_preamble(&s, const_cast<st_bstream_image_header*>(hdr));
  return ret == BSTREAM_ERROR ? ERROR : OK;
}

inline
result_t
write_summary(const Image_info &info, OStream &s)
{
  const st_bstream_image_header *hdr= static_cast<const st_bstream_image_header*>(&info);
  int ret= bstream_wr_summary(&s, const_cast<st_bstream_image_header*>(hdr));
  return ret == BSTREAM_ERROR ? ERROR : OK;
}

inline
result_t
read_header(Image_info &info, IStream &s)
{
  int ret= bstream_rd_header(&s, static_cast<st_bstream_image_header*>(&info));
  return ret == BSTREAM_ERROR ? ERROR : OK;
}

inline
result_t
read_catalog(Image_info &info, IStream &s)
{
  int ret= bstream_rd_catalogue(&s, static_cast<st_bstream_image_header*>(&info));
  return ret == BSTREAM_ERROR ? ERROR : OK;
}

inline
result_t
read_meta_data(Image_info &info, IStream &s)
{
  int ret= bstream_rd_meta_data(&s, static_cast<st_bstream_image_header*>(&info));
  return ret == BSTREAM_ERROR ? ERROR : OK;
}

inline
result_t
read_summary(Image_info &info, IStream &s)
{
  int ret= bstream_rd_summary(&s, static_cast<st_bstream_image_header*>(&info));
  return ret == BSTREAM_ERROR ? ERROR : OK;
}

} // backup namespace

#endif /*CATALOG_H_*/
