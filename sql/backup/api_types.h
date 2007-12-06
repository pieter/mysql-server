#ifndef _BACKUP_API_TYPES_H
#define _BACKUP_API_TYPES_H

/**
  @file

  Declarations of common data types used in the backup API's
 */

/*
 Note: Structures defined in this file use String class which introduces
 dependency on its implementation defined in sql/ directory. Thus to correctly
 compile any code using the backup API one needs to:
 1) include sql/mysql_priv.h header
 2) link with library ...

 This seems to be not a problem for storage engine plugins as they use String
 class anyway.
 */

extern const String my_null_string;

namespace backup {

typedef unsigned char byte;

/**
  Values returned by backup/restore driver methods and other backup functions.

  @see @c Backup_driver::get_data and @c Restore_driver::send_data
 */

enum result_t { OK=0, READY, PROCESSING, BUSY, DONE, ERROR };

typedef uint  version_t;

//@{

/**
   Classes @c Db_ref and @c Table_ref are used to identify databases and tables
   inside mysql server instance.

   These classes abstract the way a table or database is identified inside mysqld,
   so that when this changes (introduction of global db/table ids, introduction
   of catalogues) it is easy to adapt backup code to the new identification schema.

   Regardless of the internal representation, classes provide methods returning
   db/table name as a @c String object. Also, each table belongs to some database
   and a method returning @c Db_ref object identifying this database is present. 
   For @c Db_ref objects there is @c catalog() method returning name of the 
   catalogue, but currently it always returns null string.

   Classes are implemented so that the memory for storing names can be allocated
   outside an instance. This allows for sharing space used e.g., to store 
   database names among several @c Table_ref instances.

   Instances of @c Table_ref and @c Db_ref should be considered cheap to use, 
   equivalent to using pointers or other base types. Currently, single instance 
   of each class uses as much memory as a single pointer (+some external memory 
   to store names which can be shared among different instances). The methods 
   are inlined to avoid function call costs.
 */

class Db_ref
{
  const String *m_name;

 public:

  // Construct invalid reference
  Db_ref(): m_name(NULL)
  {}

  const bool is_valid() const
  { return m_name != NULL; }

  const String& name() const
  { return *m_name; }

  const String& catalog() const
  { return my_null_string; }

  bool operator==(const Db_ref &db) const
  { return stringcmp(m_name,&db.name())==0; }

  bool operator!=(const Db_ref &db) const
  { return ! this->operator==(db); }

 protected:

  // Constructors are made protected as clients of this class are
  // not supposed to create instances (see comment inside Table_ref)

  Db_ref(const String &name): m_name(&name)
  {}

  friend class Table_ref;
};


class Table_ref
{
  const Db_ref  m_db;
  const String  *m_name;

 public:

  // Construct invalid reference
  Table_ref(): m_name(NULL)
  {}

  const bool is_valid() const
  { return m_name != NULL; }

  const Db_ref& db() const
  { return m_db; }

  const String& name() const
  { return *m_name; }

  bool operator==(const Table_ref &t) const
  {
    return m_db == t.db() &&
           stringcmp(m_name,&t.name()) == 0;
  }

  bool operator!=(const Table_ref &db) const
  { return ! this->operator==(db); }

  typedef char describe_buf[512];

  /// Produce string identifying the table (e.g. for error reporting)
  const char* describe(char *buf, size_t len) const
  {
    my_snprintf(buf,len,"%s.%s",db().name().ptr(),name().ptr());
    return buf;
  }

  const char* describe(describe_buf &buf) const
  { return describe(buf,sizeof(buf)); }
  
 protected:

  /*
    Constructor is made protected as it should not be used by
    clients of this class -- they obtain already constructed
    instances from the backup kernel via Table_list object passed
    when creating backup/restore driver.
  */

  Table_ref(const String &db, const String &name):
    m_db(db), m_name(&name)
  {}
};


//@}


/**
   @class Table_list

   @brief This abstract class defines interface used to access a list of
   tables (e.g. when such a list is passed to a backup/restore driver).

   Elements of the list can be accessed by index, counting from 0. E.g.
   @code
    Table_list &tables;
    Table_ref  t2 = tables[1];  // t2 refers to the second element of the list.
   @endcode

   Interface is made abstract, so that different implementations can be
   used in the backup code. For example it is possible to create a class which
   adds this interface to a list of tables represented by a linked list of
   @c TABLE_LIST structures as used elsewhere in the code. On the other hand, 
   much more space efficient implementations are possible, as for each table we 
   need to store only table's identity (db/table name). In any case, the interface
   to the list remains the same, as defined by this class.

   TODO: add iterators.
 */

class Table_list
{
  public:

    virtual ~Table_list() {}

    /// Return reference to given list element. Elements are counted from 0.
    virtual Table_ref operator[](uint pos) const =0;

    /// Return number of elements in the list.
    virtual uint  count() const =0;
};


/**
  @class Buffer

  @brief Used for data transfers between backup kernel and backup/restore
  drivers.

  Apart from allocated memory a @c Buffer structure contains fields informing about
  its size and holding other information about contained data. Buffers are
  created and memory is allocated by backup kernel. It is also kernel's
  responsibility to write contents of buffers to a backup stream.

  Data created by a backup driver is opaque to the kernel. However, to support
  selective restores, each block of data can be assigned to one of the tables
  being backed-up. This is done by setting @c table_no member of the
  buffer structure to the number of the table to which this data belongs. Tables
  are numbered from 1 according to their position in the list passed when driver
  is created (@c m_tables member of @c Driver class). If
  some of the data doesn't correspond to any particular table, then
  @c table_no should be set to 0.

  This way, driver can create several "streams" of data blocks. For each table
  there is a stream corresponding to that table and there is one "shared stream"
  consisting of blocks with @c table_no set to 0. Upon restore, kernel
  sends to a restore driver only blocks corresponding to the tables being
  restored plus all the blocks from the shared stream.

  For example, consider backing-up three tables t1, t2 and t3. Data blocks
  produced by a backup driver are divided into four streams:
  @verbatim
  #0: shared data
  #1: data for table t1
  #2: data for table t2
  #3: data for table t3
  @endverbatim
  When a user restores tables t1 and t3, only blocks from streams #0, #1 and #3
  will be sent to a restore driver, but not the ones from stream #2.

  Using this approach, backup engine can arrange its backup image data in the
  way which best suits its internal data representation. If needed, all data can
  be put in the shared stream #0, so that all of it will be sent back to
  a restore driver. On the other hand, if possible, backup data can be
  distributed into per table streams to reduce the amount of data transferred
  upon a selective restore.

  Backup driver signals end of data in a given stream by setting
  @c buf.last flag to TRUE when get_data(buf) fills the last block of
  data from that stream (otherwise @c buf.last should be FALSE). This
  should be done for each stream used by the driver. Upon restore, kernel sets
  @c buf.last to TRUE when sending to a restore driver the last block
  of data from a stream.

  A driver learns about the size of a buffer provided by the kernel from its
  @c size member. It does not have to fill the buffer completely.
  It should update the @c size member to reflect the actual size
  of the data in the buffer. It is possible to return no data in which case
  @c size should be zero. Such empty buffers are ignored by the
  kernel (no data is written to the archive).
 */

struct Buffer
{
  size_t  size;       ///< size of the buffer (of memory block pointed by data).
  uint    table_no;   ///< Number of the table to which data in the buffer belongs.
  bool    last;       ///< TRUE if this is last block of data in the stream.
  byte    *data;      ///< Pointer to data area.

  Buffer(): size(0),table_no(0),last(FALSE), data(NULL)
  {}

  void reset(size_t len)
  {
    size= len;
    table_no= 0;
    last= FALSE;
  }
};

// forward declaration
class Engine;
class Backup_driver;
class Restore_driver;

} // backup namespace

typedef backup::result_t Backup_result_t;
typedef backup::Engine   Backup_engine;
typedef Backup_result_t backup_factory(::handlerton *,Backup_engine*&);

#endif

