#include "../mysql_priv.h"

#include <backup_stream.h>
#include "backup_aux.h"
#include "catalog.h"
#include "be_snapshot.h"
#include "be_default.h"
#include "be_native.h"

/**
  @file

  @brief Implements @c Image_info class and friends.

  @todo Error reporting
  @todo Store endianess info in the image.
*/

namespace backup {

/* Image_info implementation */

Image_info::Image_info():
  backup_prog_id(0), table_count(0), data_size(0), m_items(32,128)
{
  /* initialize st_bstream_image_header members */
  version= 1;

  /*
    The arithmetic below assumes that MYSQL_VERSION_ID digits are arrenged
    as follows: HLLRR where
    H - major version number
    L - minor version number
    R - release

    TODO: check if this is correct
  */
  DBUG_PRINT("backup",("version %d",MYSQL_VERSION_ID));
  server_version.major= MYSQL_VERSION_ID / 10000;
  server_version.minor= (MYSQL_VERSION_ID % 10000) / 100;
  server_version.release= MYSQL_VERSION_ID % 100;
  server_version.extra.begin= (byte*)MYSQL_SERVER_VERSION;
  server_version.extra.end= server_version.extra.begin +
                            strlen((const char*)server_version.extra.begin);

  flags= 0;  // TODO: set BSTREAM_FLAG_BIG_ENDIAN flag accordingly
  snap_count= 0;

  bzero(&start_time,sizeof(start_time));
  bzero(&end_time,sizeof(end_time));
  bzero(&vp_time,sizeof(vp_time));
  bzero(&binlog_pos,sizeof(binlog_pos));
  bzero(&binlog_group,sizeof(binlog_group));
  bzero(m_snap, sizeof(m_snap));
}

Image_info::~Image_info()
{
  // Delete snapshot objects

  for (uint no=0; no<256; ++no)
  {
    Snapshot_info *snap= m_snap[no];
    
    if (!snap)
      continue;
    
    for (uint i=0; i < snap->table_count(); ++i)
    {
      Table_item *t= snap->get_table(i);
      
      if (!t)
        continue;
        
      delete t->obj_ptr();
    }
    
    delete snap;
    m_snap[no]= NULL;
  }
  
  // delete server object instances as we own them.

  for (uint i=0; i < db_count(); ++i)
  {
    Db_item *db= m_db[i];
    
    if (db)
      delete db->obj_ptr();
  }
  
  for (uint i=0; i < m_items.size(); ++i)
  {
    PerDb_item *it= m_items[i];
    
    if (it)
      delete it->obj_ptr();
  }
}


void Image_info::save_time(const time_t t, bstream_time_t &buf)
{
  struct tm time;
  gmtime_r(&t,&time);
  buf.year= time.tm_year;
  buf.mon= time.tm_mon;
  buf.mday= time.tm_mday;
  buf.hour= time.tm_hour;
  buf.min= time.tm_min;
  buf.sec= time.tm_sec;  
}

result_t Image_info::Item::get_serialization(THD *thd, ::String &buf)
{
  obs::Obj *obj= obj_ptr();
  
  DBUG_ASSERT(obj);
  
  if (!obj)
    return ERROR;
    
  return obj->serialize(thd, &buf) ? ERROR : OK;
}

/// Add table to database's table list.
result_t Image_info::Db_item::add_table(Table_item &t)
{
  t.next_table= NULL;
  t.base.db= this;

  if (!m_last_table)
  {
    m_tables= m_last_table= &t;
  }
  else
  {
    m_last_table->next_table= &t;
    m_last_table= &t;
  }

  table_count++;

  return OK;
}

/**
  Locate in the catalogue an object described by the @c st_bstream_item_info
  structure.

  @todo Handle unknown item types.
*/
Image_info::Item*
Image_info::locate_item(const st_bstream_item_info *item) const
{
  switch (item->type) {

  case BSTREAM_IT_DB:
    return get_db(item->pos);

  case BSTREAM_IT_TABLE:
  {
    const st_bstream_table_info *ti= reinterpret_cast<const st_bstream_table_info*>(item);
    return get_table(ti->snap_no,item->pos);
  }

  case BSTREAM_IT_VIEW:
  case BSTREAM_IT_SPROC:
  case BSTREAM_IT_SFUNC:
  case BSTREAM_IT_EVENT:
  case BSTREAM_IT_TRIGGER:
    return get_db_object(item->pos);

  default:
    // TODO: warn or report error
    return NULL;
  }
}

/**
  Store in Backup_image all objects enumerated by the iterator.
 */ 
int Image_info::add_objects(Db_item &dbi,
                            const enum_bstream_item_type type, 
                            obs::ObjIterator &it)
{
  using namespace obs;
  
  Obj *obj;
  
  while ((obj= it.next()))
    if (!add_db_object(dbi, type, *obj))
    {
      delete obj;
      return TRUE;
    }

  return FALSE;
}

Image_info::PerDb_item* Image_info::add_db_object(Db_item &dbi,
                                                  const enum_bstream_item_type type,
                                                  obs::Obj &obj)
{
  ulong pos= m_items.size();
  
  PerDb_item *it= m_items.get_entry(pos);
  
  if (!it)
    return NULL;
    
  *it= obj;
  it->base.type= type;
  it->base.pos= pos;
  it->db= &dbi;

  return it;
}

Image_info::PerDb_item* Image_info::add_db_object(Db_item &dbi,
                                                  const enum_bstream_item_type type,
                                                  const ::String &name)
{
  ulong pos= m_items.size();
  
  PerDb_item *it= m_items.get_entry(pos);
  
  if (!it)
    return NULL;
    
  *it= name;
  it->base.type= type;
  it->base.pos= pos;
  it->db= &dbi;
  it->m_db_name= dbi.name();

  return it;
}

obs::Obj* Image_info::PerDb_item::obj_ptr(uint ver, ::String &sdata)
{ 
  using namespace obs;

  Obj *obj;
  
  switch (base.type) {
  case BSTREAM_IT_VIEW:   
    obj= materialize_view(&m_db_name, &m_name, ver, &sdata); break;
  case BSTREAM_IT_SPROC:  
    obj= materialize_stored_procedure(&m_db_name, &m_name, ver, &sdata); break;
  case BSTREAM_IT_SFUNC:
    obj= materialize_stored_function(&m_db_name, &m_name, ver, &sdata); break;
  case BSTREAM_IT_EVENT:
    obj= materialize_event(&m_db_name, &m_name, ver, &sdata); break;
  case BSTREAM_IT_TRIGGER:   
    obj= materialize_trigger(&m_db_name, &m_name, ver, &sdata); break;
  default: obj= NULL;
  }

  return obj;
}

bool Image_info::Ditem_iterator::next()
{
  if (ptr)
    ptr= ptr->next_table;
    
  if (ptr)
    return TRUE;

  bool more=TRUE;

  // advance to next element in other list if we are inside it
  if (other_list)
     more= PerDb_iterator::next();

  other_list= TRUE; // mark that we are now inside the other list

  // return if there are no more elements in the other list
  if (!more)
    return FALSE;

  // find an element belonging to our database  
  do
  {
    PerDb_item *it= (PerDb_item*)PerDb_iterator::get_ptr();

    if (!it)
      return FALSE;
    
    if (it->m_db_name == db_name)
      return TRUE;
  }
  while (PerDb_iterator::next());
    
  // we haven't found any object belonging to our database
  return FALSE;
}

} // backup namespace


/* catalogue services for backup stream library */

extern "C" {

/* iterators */

static uint cset_iter;  ///< Used to implement trivial charset iterator.
static uint null_iter;  ///< Used to implement trivial empty iterator.

void* bcat_iterator_get(st_bstream_image_header *catalogue, unsigned int type)
{
  switch (type) {

  case BSTREAM_IT_PERDB:
    return
    new backup::Image_info::PerDb_iterator(*static_cast<backup::Image_info*>(catalogue));

  case BSTREAM_IT_PERTABLE:
    return &null_iter;

  case BSTREAM_IT_CHARSET:
    cset_iter= 0;
    return &cset_iter;

  case BSTREAM_IT_USER:
    return &null_iter;

  case BSTREAM_IT_GLOBAL:
    // only global items (for which meta-data is stored) are databases
  case BSTREAM_IT_DB:
    return
    new backup::Image_info::Db_iterator(*static_cast<backup::Image_info*>(catalogue));
    // TODO: report error if iterator could not be created

  default:
    return NULL;

  }
}

struct st_bstream_item_info*
bcat_iterator_next(st_bstream_image_header *catalogue, void *iter)
{
  /* If this is the null iterator, return NULL immediately */
  if (iter == &null_iter)
    return NULL;

  static bstream_blob name= {NULL, NULL};

  /*
    If it is cset iterator then cset_iter variable contains iterator position.
    We return only 2 charsets: the utf8 charset used to encode all strings and
    the default server charset.
  */
  if (iter == &cset_iter)
  {
    switch (cset_iter) {
      case 0: name.begin= (byte*)::my_charset_utf8_bin.csname; break;
      case 1: name.begin= (byte*)::system_charset_info->csname; break;
      default: name.begin= NULL; break;
    }

    name.end= name.begin ? name.begin + strlen((char*)name.begin) : NULL;
    cset_iter++;

    return name.begin ? (st_bstream_item_info*)&name : NULL;
  }

  /*
    In all other cases assume that iter points at instance of
    @c Image_info::Iterator and use this instance to get next item.
   */
  const backup::Image_info::Item *ptr= (*(backup::Image_info::Iterator*)iter)++;

  return ptr ? (st_bstream_item_info*)(ptr->info()) : NULL;
}

void  bcat_iterator_free(st_bstream_image_header *catalogue, void *iter)
{
  /*
    Do nothing for the null and cset iterators, but delete the
    @c Image_info::Iterator object otherwise.
  */
  if (iter == &null_iter)
    return;

  if (iter == &cset_iter)
    return;

  delete (backup::Image_info::Iterator*)iter;
}

/* db-items iterator */

void* bcat_db_iterator_get(st_bstream_image_header *catalogue, struct st_bstream_db_info *db)
{
  using namespace backup;

  Image_info::Db_item *dbi = static_cast<Image_info::Db_item*>(db);

  return
  new Image_info::Ditem_iterator(*static_cast<backup::Image_info*>(catalogue),
                                 *dbi);
}

struct st_bstream_dbitem_info*
bcat_db_iterator_next(st_bstream_image_header *catalogue,
                        struct st_bstream_db_info *db,
                        void *iter)
{
  const backup::Image_info::Item *ptr= (*(backup::Image_info::Iterator*)iter)++;

  return ptr ? (st_bstream_dbitem_info*)ptr->info() : NULL;
}

void  bcat_db_iterator_free(st_bstream_image_header *catalogue,
                              struct st_bstream_db_info *db,
                              void *iter)
{
  delete (backup::Image_info::Iterator*)iter;
}

}

