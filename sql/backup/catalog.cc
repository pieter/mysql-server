#include "../mysql_priv.h"

/**
  @file

  Implementation of @c Archive_info and related classes.
 */

/*
  TODO:

  - Add to Archive_info storage for other meta-data items.
  - Make existing storage solutions more rational (e.g., string pool).
  - Make reading code resistant to unknown image formats or meta-data types
    (or, assume it is handled by format version number).
  - Improve Image_info::Tables implementation (use some existing data structure).
  - Add more information to backup archive header , for example server's version
    string.
  - Handle backward compatibility (new code reads archive with earlier version
    number)
  - Add to Archive_info methods for browsing contents of the archive.
 */

#if defined(USE_PRAGMA_IMPLEMENTATION) || defined(__APPLE_CC__)
/*
  #pragma implementation is needed on powermac platform as otherwise compiler
  doesn't create/export vtable for Image_info::Tables class (if you know a
  better way for fixing this issue let me know! /Rafal).

  Apparently, configuration macro USE_PRAGMA_IMPLEMENTATION is not set by
  ./configure on powermac platform - this is why __APPLE_CC__ is also checked.
 */
#pragma implementation
#endif

#include "backup_engine.h"
#include "backup_aux.h"
#include "catalog.h"
#include "be_default.h"
#include "be_snapshot.h"


/***************************************

   Implementation of Archive_info class

 ***************************************/

namespace backup {

Archive_info::~Archive_info()
{
  for (uint i=0; i<256; ++i)
   if (images[i])
   {
     delete images[i];
     images[i]= NULL;
   }
}


/**
  Write header and catalogue of a backup archive.

  Header forms the first chunk of archive. Currently it contains archive's
  format version number followed by a list of table data images used in the
  archive.

  Next chunk contains the pool of database names. After that there is one chunk
  per image containing list of tables whose data is saved in that image.
  @verbatim
  =====================
   version number        }
  ---------------------  }  header
   image descriptions    }
  =====================
   db names              }
  =====================  }
   tables of image 1     }
  =====================  }
                         }  catalogue
          ...            }
                         }
  =====================  }
   tables of image N     }
  =====================
  @endverbatim
  In the picture "====" denotes chunk boundaries. Number of images is known
  from the header.

  The format in which image descriptions are saved is determined by
  @c Image_info::write_description() method.

  For list of databases and tables the format in which they are saved is
  defined by @c StringPool::save() and @c Archive_info::Tables::save() methods
  respectively.
 */

result_t Archive_info::save(OStream &s)
{
  DBUG_ENTER("Archive_info::save");

  size_t start_bytes= s.bytes;
  stream_result::value res;

  res= s.write2int(ver);
  if (res != stream_result::OK)
  {
    DBUG_PRINT("backup",("Can't write archive version number (stream_res=%d)",(int)res));
    DBUG_RETURN(ERROR);
  }

  // write list of images
  DBUG_PRINT("backup",(" writing image list"));

  uint ino;
  Image_info *img;

  for (ino=0; ino < img_count ; ++ino)
    if ((img= images[ino]))
    {
      DBUG_PRINT("backup",(" %2d: %s image",ino,img->name()));
      if (ERROR == img->write_description(s))
      {
        DBUG_PRINT("backup",("Can't write description of %s image (#%d)",
                             img->name(),ino));
        DBUG_RETURN(ERROR);
      }
    }

  // close the header chunk
  res= s.end_chunk();
  if (res != stream_result::OK)
  {
    DBUG_PRINT("backup",("Error when closing image list chunk (stream_res=%d)",(int)res));
    DBUG_RETURN(ERROR);
  }


  // write catalogue
  DBUG_PRINT("backup",(" writing archive catalogue"));

  // db names (one chunk)
  if (ERROR == db_names.save(s)) // note: this closes the chunk
  {
    DBUG_PRINT("backup",("Error saving pool of db names (stream_res=%d)",(int)res));
    DBUG_RETURN(ERROR);
  }

  // table lists (one chunk per image/list)
  for (ino=0; ino < img_count ; ++ino)
    if ((img= images[ino]))
    {
      DBUG_PRINT("backup",("  saving %s image's tables",img->name()));
      if (ERROR == img->tables.save(s))
      {
        DBUG_PRINT("backup",("Error saving tables (stream_res=%d)",(int)res));
        DBUG_RETURN(ERROR);
      }
    }

  header_size= s.bytes - start_bytes;

  DBUG_RETURN(OK);
}

/**
  Fill @c Archive_info structure reading data from backup archive header and
  catalogue.

  @returns OK or ERROR
 */
result_t Archive_info::read(IStream &s)
{
  DBUG_ENTER("Archive_info::read");

  size_t start_bytes= s.bytes;
  result_t res;
  version_t ver;

  /*
    We read archive's header which starts with archive format version number.
    If we can't read the version number (end of stream or no data in the chunk)
    there is something wrong and we signal error.
   */

  stream_result::value rres= s.read2int(ver);
  if ( rres != stream_result::OK)
  {
    DBUG_PRINT("restore",("Error reading archive version number"
                          " (stream_res=%d)",(int)rres));
    DBUG_RETURN(ERROR);
  }

  if (ver != Archive_info::ver)
  {
    DBUG_PRINT("restore",("Backup archive version %d not supported",ver));
    DBUG_RETURN(ERROR);
  }

  /*
    What follows (until the end of the data chunk) is a list of entries
    describing data images of the archive. It is read using
    Image_info::create_from_stream() function which returns DONE when end of
    chunk is reached.
   */

  DBUG_PRINT("restore",(" reading image list"));

  uint ino= 0;

  do
  {
    Image_info *img;

    res= Image_info::create_from_stream(*this,s,img);

    if (res == OK)
    {
      DBUG_ASSERT(img);
      DBUG_PRINT("restore",("  %2d: %s image",ino,img->name()));
      images[ino++]= img;
    }

  } while (res == OK && ino < MAX_IMAGES);

  img_count= ino;

  // If res != DONE we haven't reached end of the chunk - something is wrong
  if (res != DONE)
  {
    DBUG_PRINT("restore",("Error when reading image list (%d images read)",
                          img_count));
    DBUG_RETURN(ERROR);
  }

  /*
    Next chunk starts archive's catalogue. We proceed with reading it.
    Note that the catalogue should always contain at least one chunk (db names
    pool) and hence we should not hit end of stream here.
   */

  table_count= 0;

  if (s.next_chunk() != stream_result::OK)
  {
    DBUG_PRINT("restore",("Can't proceed to the catalogue"));
    DBUG_RETURN(ERROR);
  }

  DBUG_PRINT("restore",(" reading catalogue (%d images)",img_count));

  /*
    First chunk of the catalogue contains db names pool - we read it and
    proceed to the next chunk. We should never hit end of stream here and
    so the only acceptable result of db.names.read() is OK.
   */
  res= db_names.read(s);
  if (res != OK)
  {
    DBUG_PRINT("restore",("Can't read db names pool (res=%d)",(int)res));
    DBUG_RETURN(ERROR);
  }

  /*
    The following chunks contain lists of tables for each image. There are
    as many lists as there are images (possibly 0) and each list occupies
    one chunk.
   */
  for (uint ino=0; ino < img_count; ++ino)
  {
    Image_info *img= images[ino];

    DBUG_PRINT("restore",(" reading %s image's tables (#%d)",img->name(),ino));

    /*
      There should be as many lists in the stream as there were images in the
      header. Thus we should never hit end of stream here.
     */
    res= img->tables.read(s); // note: proceeds to the next chunk in the stream

    if (res != OK)
    {
      DBUG_PRINT("restore",("Can't read table list for %s image (#%d)",
                            img->name(),ino));
      DBUG_RETURN(ERROR); // neither stream nor chunk should end here
    }

    table_count+= img->tables.count();

    DBUG_PRINT("restore",(" finished reading tables"));
  }

  header_size= s.bytes - start_bytes;

  DBUG_RETURN(OK);
}

} // backup namespace


/**********************************

  Write/read image descriptions

 **********************************/

namespace backup {

/**
  Write entry describing (format of) a backup driver's image.

  Entry has the form:
  @verbatim
  | type | version | image description |
  @endverbatim
  where type is a byte holding Image_info::image_type value, version is 2 byte
  integer holding image format version. The format of optional image description
  is determined by @c X::do_write_description() method where X is a subclass of
  Image_info corresponding to given image type.
 */
result_t
Image_info::write_description(OStream &s)
{
  // TODO: to handle unknown description formats, write description length here

  stream_result::value res= s.writebyte(type());

  if (res != stream_result::OK)
    return ERROR;

  res= s.write2int(ver);

  if (res != stream_result::OK)
    return ERROR;

  return do_write_description(s);
}

/**
  Create @c Image_info instance from a saved entry describing it.

  @retval OK
  @retval DONE  end of chunk/stream hit
  @retval ERROR
 */
result_t
Image_info::create_from_stream(Archive_info &info, IStream &s, Image_info* &ptr)
{
  uint ver;
  byte t;

  stream_result::value res= s.readbyte(t);

  // if we are at end of data chunk or stream, we should tell the caller
  if (res != stream_result::OK)
    return report_stream_result(res);

  res= s.read2int(ver);

  if (res != stream_result::OK)
    return ERROR;

  switch (image_type(t)) {

  case NATIVE_IMAGE:
    return Native_image::create_from_stream(ver,info,s,ptr);

  case DEFAULT_IMAGE:
    return Default_image::create_from_stream(ver,info,s,ptr);

  case SNAPSHOT_IMAGE:
    return Snapshot_image::create_from_stream(ver,info,s,ptr);

  default:
    DBUG_PRINT("restore",("Unknown image type %d",t));
    return ERROR;
  }
}

} // backup namespace


/*******************

  Serialization of meta-data items

 *******************/

namespace backup {

/**
  Write an entry describing single meta-data item.

  Entry has format:
  @verbatim
  | type | id data | create data |
  @endverbatim
  Type is a single byte holding meta::Item::enum_type value. Id data is
  used to determine which item (from the archive catalogue) the entry
  corresponds to. Create data is used to create the item.

  The format of id data and create data for item of type X is determined
  by methods @c Archive_info::X_item::save_id() and @c meta::X::save(),
  respectively.

  @see @c write_meta_data() for information about the format of the meta-data
  section of backup archive.
*/
result_t
Archive_info::Item::save(THD *thd, OStream &s)
{
  byte b= meta().type();

  stream_result::value res=s.writebyte(b);

  if (res != stream_result::OK)
    return ERROR;

  if (ERROR == save_id(s))
    return ERROR;

  return meta().save(thd,s);
}

/**
  Create meta-data item from a saved entry.

  This function reads the type byte and calls @c create_from_stream method of
  corresponding class to create the item. It stores pointer to the created
  item in @c ptr argument.

  @retval OK    if new item was created
  @retval DONE  if end of chunk/stream was reached
  @retval ERROR if error has happened
 */
result_t
Archive_info::Item::create_from_stream(const Archive_info &info,
                                       IStream &s, Item* &ptr)
{
  byte b;

  stream_result::value res= s.readbyte(b);

  if (res != stream_result::OK)
    return report_stream_result(res);

  ptr= NULL;

  result_t res1;

  switch (meta::Item::enum_type(b)) {

  case meta::Item::DB:
    res1= Db_item::create_from_stream(info,s,ptr);
    break;

  case meta::Item::TABLE:
    res1= Table_item::create_from_stream(info,s,ptr);
    break;

  default: return ERROR;

  }

  /*
    Note that create_from_stream() should return OK - end of data should not
    happen here.
   */
  if (res1 != OK || ptr == NULL)
    return ERROR;

  return ptr->meta().read(s);
}

// Db items

result_t Archive_info::Db_item::save_id(OStream &s)
{
  uint k= key;
  DBUG_PRINT("backup",(" saving db-item (%d)",k));
  return stream_result::OK == s.writeint(k) ? OK : ERROR;
}

result_t
Archive_info::Db_item::create_from_stream(const Archive_info &i,
                                          IStream &s,
                                          Archive_info::Item* &ptr)
{
  uint k;
  stream_result::value res= s.readint(k);

  if (res != stream_result::OK)
    return report_stream_result(res);

  return (ptr= new Db_item(i,k)) ? OK : ERROR;
}

// Table items

result_t Archive_info::Table_item::save_id(OStream &s)
{
  DBUG_PRINT("backup",(" saving table-item (%d,%d)",img,pos));
  stream_result::value res= s.writeint(img);

  if (res != stream_result::OK)
    return ERROR;

  res= s.writeint(pos);

  if (res != stream_result::OK)
    return ERROR;

  return OK;
}

result_t
Archive_info::Table_item::create_from_stream(const Archive_info &i,
                                             IStream &s,
                                             Archive_info::Item* &ptr)
{
  uint img,no;
  stream_result::value res= s.readint(img);

  if (res != stream_result::OK)
    return report_stream_result(res);

  res= s.readint(no);

  if (res != stream_result::OK)
    return ERROR;

  return (ptr= new Table_item(i,img,no)) ? OK : ERROR;
}

} // backup namespace


/**********************************

  Implementation of Image_info::Tables

 **********************************/

namespace backup {

// TODO: use better implementation (red-black tree from mysys?)

struct Image_info::Tables::node {
  StringPool::Key db;
  String          name;
  node            *next;

  node(const Image_info::Tables&,
       const StringPool::Key &k,
       const String &nm): db(k), next(NULL)
  {
    name.copy(nm);
  }
};

/// Empty the list.
void Image_info::Tables::clear()
{
  for (node *ptr= m_head; ptr;)
  {
    node *n=ptr;
    ptr= n->next;
    delete n;
  }

  m_head= m_last= NULL;
  m_count= 0;
}

/**
  Add a table to the list.

  @returns Position of the table or -1 if error
 */
int Image_info::Tables::add(const backup::Table_ref &t)
{
  StringPool::Key k= m_db_names.add(t.db().name());

  if (!k.is_valid())
    return -1;

  return add(k,t.name());
}

/// Add table at given position.
int Image_info::Tables::add(const StringPool::Key &k, const String &name)
{
  node *n= new node(*this,k,name);

  if (!n)
    return -1;

  if (m_head == NULL)
  {
    m_count=1;
    m_head= m_last= n;
  }
  else
  {
    m_count++;
    m_last->next= n;
    m_last= n;
  };

  return m_count-1;
}

/**
  Locate table at given position.

  @returns Pointer to table's list node or NULL if position is not occupied
 */
Image_info::Tables::node*
Image_info::Tables::find_table(uint pos) const
{
  DBUG_ASSERT(pos < m_count);

  node *ptr;

  for (ptr= m_head; ptr && pos; ptr= ptr->next)
   pos--;

  //if( !ptr ) ptr= m_last;

  return ptr;
}

/// Return table at a given position.
inline
Table_ref Image_info::Tables::operator[](uint pos) const
{
  // Get access to backup::Table_ref protected constructor

  struct Table_ref: public backup::Table_ref
  {
    Table_ref(const StringPool &db_names, Image_info::Tables::node &n):
      backup::Table_ref(db_names[n.db],n.name)
    {}
  };

  node *ptr= find_table(pos);
  DBUG_ASSERT(ptr);

  return Table_ref(m_db_names,*ptr);
}

/******************

   Serialization for Image_info::Tables class

 ******************/

/**
  Save list of tables in a backup stream.

  The format used assumes that a pool of database names is stored elsewhere.
  Thus for each table only the key of the database is stored as var-length
  integer followed by table name. Empty list is saved as single NIL value.

  The list is stored in a single stream chunk which determines its end.

  @returns OK or ERROR
 */
result_t
Image_info::Tables::save(OStream &s)
{
  DBUG_ENTER("Image_info::Tables::save");
  stream_result::value res;

  if (count() == 0)
  {
    res= s.writenil();
    if (res != stream_result::OK)
      DBUG_RETURN(ERROR);
  }
  else
    for (Tables::node *n= m_head ; n ; n= n->next)
    {
      res= s.writeint(n->db);
      if (res != stream_result::OK)
        DBUG_RETURN(ERROR);

      res= s.writestr(n->name);
      if (res != stream_result::OK)
        DBUG_RETURN(ERROR);
    };

  res= s.end_chunk();
  DBUG_RETURN(res == stream_result::ERROR ? ERROR : OK);
}

/**
  Read a list from a backup stream.

  @pre Stream is positioned at the first entry of the saved list.
  @post Stream is positioned at the beginning of next chunk or at its end.

  @retval OK
  @retval DONE  end of stream or chunk hit (nothing has been read)
  @retval ERROR
 */
result_t
Image_info::Tables::read(IStream &s)
{
  DBUG_ENTER("Image_info::Tables::read");

  stream_result::value res;
  uint k,tno=0;

  /*
    Read first entry - if it is NIL, we have empty list. Otherwise it should
    be db index of the first table.
   */

  res= s.readint(k);

  // If unexpected result, report an error or end of stream/chunk
  if (res != stream_result::OK && res != stream_result::NIL)
    DBUG_RETURN(report_stream_result(res));

  // empty the list
  clear();

  if (res == stream_result::OK) // this is non-empty list
    do
    {
      String                name;

      res= s.readstr(name);
      if (res != stream_result::OK)
        break;

      tno= add(k,name);
      DBUG_PRINT("restore",("got next table %s.%s (pos %d, dbkey %d)",
                             (*this)[tno].db().name().ptr(),
                             (*this)[tno].name().ptr(),tno,k));
      res= s.readint(k);
    }
    while (res == stream_result::OK);
  else
   /*
     If we have read NIL value, pretend we are at the end of chunk so that
     no errors are reported below.
    */
   res= stream_result::EOC;

  // we should be now at end of chunk/stream
  if (res != stream_result::EOC && res != stream_result::EOS)
  {
    DBUG_PRINT("restore",("Error when reading table no %d in table list",tno));
    DBUG_RETURN(ERROR);
  }

  res= s.next_chunk();
  DBUG_RETURN(res == stream_result::ERROR ? ERROR : OK);
}

} // backup namespace

/**************************************

     Native image type definition

 **************************************/

namespace backup {

/*
  For native image its format (apart from the version number) is determined
  by the storage engine whose backup driver created it. Thus we save the name
  of storage engine.

  TODO: add more information here. E.g. the version number of the storage engine.
 */
result_t
Native_image::do_write_description(OStream &s)
{
  String name(::ha_resolve_storage_engine_name(m_hton),&::my_charset_bin);
  return stream_result::OK == s.writestr(name) ? OK : ERROR;
}

result_t
Native_image::create_from_stream(version_t ver,
                                 Archive_info &info,
                                 IStream &s, Image_info* &img)
{
  String name;
  stream_result::value res= s.readstr(name);

  if (res != stream_result::OK)
    return report_stream_result(res);

  LEX_STRING name_lex= name;


  ::handlerton *hton= plugin_data(::ha_resolve_by_name(::current_thd,&name_lex),
                                  handlerton*);
  if (!hton)
    return ERROR;

  img= new Native_image(info,hton);
  if (!img)
    return ERROR;

  if (ver > img->ver)
  {
    DBUG_PRINT("restore",("Restore diver version %d can't read image version %d",
                          img->ver,ver));
    return ERROR;
  }

  img->ver= ver;

  return OK;
}

} // backup namespace
