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
  backup_prog_id(0), table_count(0), data_size(0)
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
   if (m_snap[no])
    delete m_snap[no];
   m_snap[no]= NULL;
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

  default:
    // TODO: warn or report error
    return NULL;
  }
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
  delete (backup::Image_info::Ditem_iterator*)iter;
}

}

