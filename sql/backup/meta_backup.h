#ifndef _META_BACKUP_H
#define _META_BACKUP_H

/**
  @file

  Declarations of classes used to handle meta-data items.
 */

#include <backup/api_types.h>
#include <backup/stream.h>

#define META_ITEM_DESCRIPTION_LEN 128

namespace backup {

namespace meta {

/**
  Defines how to backup and restore a meta-data item.

  This class provides @c create() and @c drop() methods used to create and
  destroy items. The default implementation uses SQL "CREATE ..." and "DROP ..."
  statements for that purpose. Its instances determine how to save and read data
  needed for item creation and provide any other item specific data. For each
  type of meta-data there is a specialized subclass of @c meta::Item which
  implements these tasks.
 */

class Item
{
 public:

  /// Possible types of meta-data items.
  enum enum_type {DB, TABLE};

  virtual ~Item() {}

  /// Return type of the item.
  virtual const enum_type type() const =0;

  /**
    For per-db items return the database to which this item belongs.
    For other items returns an invalid db reference.
   */
  virtual const Db_ref in_db()
  { return Db_ref(); }

  /// Save data needed to create the item.
  virtual result_t save(THD*,OStream&);

  /// Read data saved by @c save() method.
  virtual result_t read(IStream&);

  /// Destroy the item if it exists.
  virtual result_t drop(THD*);

  /// Create the item.
  virtual result_t create(THD*);

  typedef char description_buf[META_ITEM_DESCRIPTION_LEN+1];

  /// Describe item for log purposes.
  virtual const char* describe(char *buf, size_t buf_len)
  {
    my_snprintf(buf,buf_len,"%s %s",sql_object_name(),sql_name());
    return buf;
  }

 protected:

  String create_stmt;  /// Storage for a create statement of the item.

  /**
    Return SQL name of the object represented by this item like TABLE
    or DATABASE. This is used to construct a "DROP ..." statement for
    the item.
   */
  virtual const char* sql_object_name() const
  { return NULL; }

  /**
    Return name under which this item is known to the server. In case of
    per-db items the name should *not* be qualified by db name.
   */
  virtual const char* sql_name() const =0;

  /// Store in @c create_stmt a DDL statement which will create the item.
  /*
    We give a default implementation because an item can not use create
    statements and then it doesn't have to worry about this method.
  */
  virtual int build_create_stmt(THD*)
  { return ERROR; }

};

/**
  Specialization of @c meta::Item representing a database.
 */
class Db: public Item
{
  const enum_type type() const
  { return DB; }

  const char* sql_object_name() const
  { return "DATABASE"; }

  // Overwrite default implementations.
  result_t save(THD*,OStream&);
  result_t read(IStream&);

};

/**
  Specialization of @c meta::Item representing a table.
 */
class Table: public Item
{
  const enum_type type() const
  { return TABLE; }

  const char* sql_object_name() const
  { return "TABLE"; }

  int build_create_stmt(THD*);
};

} // meta namespace

} // backup namespace

#endif
