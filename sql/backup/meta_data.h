#ifndef META_DATA_H_
#define META_DATA_H_

/**
  @brief Size of a buffer used to describe an object when including it
  in error and debug messages.
*/
#define META_ITEM_DESCRIPTION_LEN 128

namespace backup {

namespace meta {

/**
  Defines how to backup and restore an object.

  This class provides methods for generating object's meta-data and also to
  create an object from this data. The class defines default implementations
  of these methods, but for specific object types a derived class might
  overwrite them.
*/
class Item
{
 public:

  /// Possible types of objects.
  enum enum_type {
    DB= BSTREAM_IT_DB,
    TABLE= BSTREAM_IT_TABLE
  };

  virtual ~Item() {}

  /// Return type of the object.
  virtual const enum_type type() const =0;

  /**
    Obtain a create statement for an object and put it in the given
    @c Stream argument.
  */
  virtual result_t get_create_stmt(::String&)
  { return ERROR; }

  virtual result_t drop(THD*);

  /**
    Create the object from its meta-data.

    @param query  CREATE statement for the object.
    @param begin  First byte of object's extra meta-data (not used currently).
    @param end    One byte after the last byte of object's extra meta-data
                  (not used currently).
  */
  virtual result_t create(THD *thd, ::String &query, byte*, byte*)
  {
    int ret= silent_exec_query(thd,query);
    return ret ? ERROR : OK;
  }

  typedef char description_buf[META_ITEM_DESCRIPTION_LEN+1];

  /// Describe item for log purposes.
  virtual const char* describe(char *buf, size_t buf_len)
  {
    my_snprintf(buf,buf_len,"%s %s",sql_object_name(),sql_name());
    return buf;
  }

 protected:

  /// Return SQL name of the object's type such as "TABLE" or "DATABASE".
  virtual const char* sql_object_name() const
  { return NULL; }

  /**
    Return name under which this item is known to the server. In case of
    per-db items the name should *not* be qualified by db name.
   */
  virtual const char* sql_name() const =0;
};

/**
  Specialization of @c meta::Item for database objects.
 */
class Db: public Item
{
  const enum_type type() const
  { return DB; }

  const char* sql_object_name() const
  { return "DATABASE"; }

  result_t get_create_stmt(::String&);
  result_t create(THD*, ::String&, byte*, byte*);
};

/**
  Specialization of @c meta::Item for table objects.
 */
class Table: public Item
{
 public:

  const enum_type type() const
  { return TABLE; }

  const char* sql_object_name() const
  { return "TABLE"; }

  result_t get_create_stmt(::String&);

 private:

  virtual ::TABLE_LIST* get_table_list_entry() =0;
};

} // meta namespace

} // backup namespace


#endif /*META_DATA_H_*/
