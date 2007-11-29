#include <string.h>
#include <stdlib.h>

#include "stream_v1.h"
#include "stream_v1_services.h"

/**
  @file

  @brief
  Implementation of the high-level functions for writing and reading backup
  image using version 1 of backup stream format.

  @todo handle errors when creating iterators in functions like bstream_wr_catalogue()
  @todo use data chunk sequence numbers to detect discontinuities in backup stream.
*/

#ifdef DBUG_OFF
# define ASSERT(X)
#else
# include <assert.h>
# define ASSERT(X) assert(X)
#endif

/**
 @page streamlib Backup Stream Library

 This library defines version 1 of backup stream format. It provides functions
 for writing and reading streams using this format. The functions are declared
 in the stream_v1.h header.
*/

/* local types */

typedef unsigned char bool;
#define TRUE    1
#define FALSE   0

typedef bstream_byte byte;
typedef bstream_blob blob;

/* this is needed for seamless compilation on windows */
#define bzero(A,B)  memset((A),0,(B))


/*
  Macros used to test results of functions writing/reading backup stream.

  To use these macros, function should define ret variable of type int and
  also wr_error/rd_error label to which these macros would jump in case of
  error.
*/

#define CHECK_WR_RES(X) \
  do{\
   if ((ret= X) != BSTREAM_OK) goto wr_error;\
  } while(0)

#define CHECK_RD_OK(X) \
 do{\
   if ((ret= X) != BSTREAM_OK)\
    { ret=BSTREAM_ERROR; goto rd_error; }\
 } while(0)

#define CHECK_RD_RES(X) \
 do{\
   if ((ret= X) == BSTREAM_ERROR) goto rd_error;\
 } while(0)


/* functions for writing basic types */

int bstream_wr_byte(backup_stream*, unsigned short int);
int bstream_wr_int2(backup_stream*, unsigned int);
int bstream_wr_int4(backup_stream*, unsigned long int);
int bstream_wr_num(backup_stream*, unsigned long int);
int bstream_wr_string(backup_stream*, bstream_blob);
int bstream_wr_time(backup_stream*, bstream_time_t*);

/* low level i/o operations on backup stream (defined in stream_v1_carrier.c) */

int bstream_write(backup_stream*, bstream_blob*);
int bstream_write_part(backup_stream*, bstream_blob*, bstream_blob);
int bstream_write_blob(backup_stream*, bstream_blob);
int bstream_end_chunk(backup_stream*);
int bstream_flush(backup_stream*);

int bstream_read(backup_stream*, bstream_blob*);
int bstream_read_part(backup_stream*, bstream_blob*, bstream_blob);
int bstream_read_blob(backup_stream*, bstream_blob);
int bstream_skip(backup_stream*, unsigned long int);


/*************************************************************************
 *
 *   IMAGE PREAMBLE AND SUMMARY
 *
 *************************************************************************/

/**
  @page stream_format Backup Stream Format (v1)

  Backup image consists of 3 main parts: preamble, table data and summary.
  @verbatim

  [backup image]= [ preamble | table data | 0x00 ! summary(*) ]
  @endverbatim

  The 0x00 byte separates table data chunks from the summary chunk. This works
  because no table data chunk can start with 0x00.

  Optionally, summary can be included in the preamble, instead of being stored
  at the end of the image. This is indicated by flags in the header.
  @verbatim

  [preamble]= [ header | summary (*) | catalogue | meta data ]
  @endverbatim
*/

int bstream_wr_header(backup_stream*, struct st_bstream_image_header*);
int bstream_wr_catalogue(backup_stream*, struct st_bstream_image_header*);
int bstream_wr_meta_data(backup_stream *, struct st_bstream_image_header*);

/** Write backup image preamble */
int bstream_wr_preamble(backup_stream *s, struct st_bstream_image_header *hdr)
{
  int ret= BSTREAM_OK;

  CHECK_WR_RES(bstream_wr_header(s,hdr));
  CHECK_WR_RES(bstream_end_chunk(s));

  CHECK_WR_RES(bstream_wr_catalogue(s,hdr));
  CHECK_WR_RES(bstream_end_chunk(s));

  CHECK_WR_RES(bstream_wr_meta_data(s,hdr));
  CHECK_WR_RES(bstream_end_chunk(s));

  wr_error:

  return ret;
}

/**
  Read backup image preamble creating all items stored in it

  @retval BSTREAM_ERROR  Error while reading preamble
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOS    Preamble has been read and there are no more chunks in
                         the stream.
*/
int bstream_rd_preamble(backup_stream *s, struct st_bstream_image_header *hdr)
{
  int ret= BSTREAM_OK;

  CHECK_RD_RES(bstream_rd_header(s,hdr));
  if (ret == BSTREAM_EOS)
    return BSTREAM_ERROR;
  CHECK_RD_OK(bstream_next_chunk(s));

  if (hdr->flags & BSTREAM_FLAG_INLINE_SUMMARY)
  {
    CHECK_RD_RES(bstream_rd_summary(s,hdr));
    if (ret == BSTREAM_EOS)
      return BSTREAM_ERROR;
    CHECK_RD_OK(bstream_next_chunk(s));
  }

  CHECK_RD_RES(bstream_rd_catalogue(s,hdr));
  if (ret == BSTREAM_EOS)
    return BSTREAM_ERROR;
  CHECK_RD_OK(bstream_next_chunk(s));

  CHECK_RD_RES(bstream_rd_meta_data(s,hdr));
  CHECK_RD_RES(bstream_next_chunk(s));

  rd_error:

  return ret;
}

/**
  @page stream_format

  @section summary Summary section

  @verbatim

  [summary]= [ vp time ! end time ! binlog pos ! binlog group pos ]
  @endverbatim

  Summary starts with 0x00 byte to distinguish it from table data chunks which
  never start with that value.
  @verbatim

  [binlog pos]= [ pos:4 ! binlog file name ]

  [binlog group pos] uses the same format as [binlog pos].
  @endverbatim
*/

/** Save binlog position. */
int bstream_wr_binlog_pos(backup_stream *s, struct st_bstream_binlog_pos pos)
{
  blob name;
  int ret= BSTREAM_OK;

  name.begin= (byte *)pos.file;
  name.end= name.begin + (pos.file ? strlen(pos.file) : 0);
  CHECK_WR_RES(bstream_wr_int4(s,pos.pos));
  CHECK_WR_RES(bstream_wr_string(s,name));

  wr_error:

  return ret;
}

/**
  Read binlog position.

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached
*/
int bstream_rd_binlog_pos(backup_stream *s, struct st_bstream_binlog_pos *pos)
{
  blob name= {NULL, NULL};
  int ret= BSTREAM_OK;

  CHECK_RD_OK(bstream_rd_int4(s,&pos->pos));
  CHECK_RD_RES(bstream_rd_string(s,&name));

  if (ret != BSTREAM_ERROR)
    pos->file= (char*)name.begin;

  rd_error:

  return ret;
}

/**
  Write backup image summary.

  This function assumes that all members of the header (such as @c vp_time) are
  already filled. It also stores the 0x00 byte separating summary from the
  preceding table data chunks.
*/
int bstream_wr_summary(backup_stream *s, struct st_bstream_image_header *hdr)
{
  int ret= BSTREAM_OK;

  CHECK_WR_RES(bstream_wr_byte(s,0x00));
  CHECK_WR_RES(bstream_wr_time(s,&hdr->vp_time));
  CHECK_WR_RES(bstream_wr_time(s,&hdr->end_time));
  CHECK_WR_RES(bstream_wr_binlog_pos(s,hdr->binlog_pos));
  CHECK_WR_RES(bstream_wr_binlog_pos(s,hdr->binlog_group));

  wr_error:

  return ret;
}

/**
  Read backup image summary

  The information stored in the summary section is read and stored in the image
  header structure.

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached
*/
int bstream_rd_summary(backup_stream *s, struct st_bstream_image_header *hdr)
{
  int ret= BSTREAM_OK;

  CHECK_RD_OK(bstream_rd_time(s,&hdr->vp_time));
  CHECK_RD_OK(bstream_rd_time(s,&hdr->end_time));
  CHECK_RD_OK(bstream_rd_binlog_pos(s,&hdr->binlog_pos));
  CHECK_RD_RES(bstream_rd_binlog_pos(s,&hdr->binlog_group));

  rd_error:

  return ret;
}


/*************************************************************************
 *
 *   IMAGE HEADER
 *
 *************************************************************************/

int bstream_wr_snapshot_info(backup_stream*, struct st_bstream_snapshot_info*);
int bstream_rd_image_info(backup_stream*, struct st_bstream_snapshot_info*);

/**
  @page stream_format

  @section header Header

  @verbatim

  [header]= [ flags:2 ! creation time ! #of snapshots:1 ! server version !
              extra data | snapshot descriptions ]
  @endverbatim

  [snapshot descriptions] contains descriptions of the table data snapshots
  present in the image. Each description is stored in a separate chunk
  (number of snapshots is given in the header).
  @verbatim

  [snapshot descriptions]= [ snapshot description | ... | snapshot description ]
  @endverbatim
*/

/** Write header of backup image */
int bstream_wr_header(backup_stream *s, struct st_bstream_image_header *hdr)
{
  int ret= BSTREAM_OK;
  unsigned int i;

  CHECK_WR_RES(bstream_wr_int2(s, hdr->flags));
  CHECK_WR_RES(bstream_wr_time(s, &hdr->start_time));
  CHECK_WR_RES(bstream_wr_byte(s, hdr->snap_count));

  CHECK_WR_RES(bstream_wr_byte(s, hdr->server_version.major));
  CHECK_WR_RES(bstream_wr_byte(s, hdr->server_version.minor));
  CHECK_WR_RES(bstream_wr_byte(s, hdr->server_version.release));
  CHECK_WR_RES(bstream_wr_string(s, hdr->server_version.extra));

  for (i=0; i < hdr->snap_count; ++i)
  {
    CHECK_WR_RES(bstream_end_chunk(s));
    CHECK_WR_RES(bstream_wr_snapshot_info(s, &hdr->snapshot[i]));
  }

  wr_error:

  return ret;
}

/**
  Read backup image header and fill @c st_bstream_image_header structure
  with the data read.

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached
*/
int bstream_rd_header(backup_stream *s, struct st_bstream_image_header *hdr)
{
  int ret= BSTREAM_OK;
  unsigned int i;

  CHECK_RD_OK(bstream_rd_int2(s, &hdr->flags));
  CHECK_RD_OK(bstream_rd_time(s, &hdr->start_time));
  CHECK_RD_OK(bstream_rd_byte(s, &hdr->snap_count));

  CHECK_RD_OK(bstream_rd_byte(s, &hdr->server_version.major));
  CHECK_RD_OK(bstream_rd_byte(s, &hdr->server_version.minor));
  CHECK_RD_OK(bstream_rd_byte(s, &hdr->server_version.release));
  CHECK_RD_RES(bstream_rd_string(s, &hdr->server_version.extra));

  for (i=0; i < hdr->snap_count; ++i)
  {
    if (ret != BSTREAM_EOC)
      return BSTREAM_ERROR;

    CHECK_RD_OK(bstream_next_chunk(s));
    CHECK_RD_RES(bstream_rd_image_info(s, &hdr->snapshot[i]));
  }

  rd_error:

  return ret;
}

/**
  @page stream_format

  @subsection snapshot Snapshot description entry

  @verbatim

  [snapshot description] = [ image type:1 ! format version: 2 ! global options:2 !
                             #of tables ! backup engine info ! extra data ]
  @endverbatim

  [image type] is encoded as follows:

  - 0 = snapshot created by native backup driver (BI_NATIVE),
  - 1 = snapshot created by built-in blocking driver (BI_DEFAULT),
  - 2 = snapshot created using created by built-in driver using consistent
      read transaction (BI_CS).

  Format of [backup engine info] depends on snapshot type. It is empty for the
  default and CS snapshots. For native snapshots it has format
  @verbatim

  [backup engine info (native)] = [ storage engine name ! storage engine version ]

  [server version] = [ major:1 ! minor:1 ! release:1 ! extra string ]

  [engine version] = [ major:1 ! minor:1 ]
  @endverbatim
*/

/** Save description of table data snapshot */
int bstream_wr_snapshot_info(backup_stream *s, struct st_bstream_snapshot_info *info)
{
  int ret= BSTREAM_OK;

  switch (info->type) {
  case BI_NATIVE:  ret= bstream_wr_byte(s,0); break;
  case BI_DEFAULT: ret= bstream_wr_byte(s,1); break;
  case BI_CS:      ret= bstream_wr_byte(s,2); break;
  default:         ret= bstream_wr_byte(s,3); break;
  }

  CHECK_WR_RES(bstream_wr_int2(s,info->version));

  if (ret != BSTREAM_OK)
    goto wr_error;

  CHECK_WR_RES(bstream_wr_int2(s,info->options));
  CHECK_WR_RES(bstream_wr_num(s,info->table_count));

  if (info->type == BI_NATIVE )
  {
    CHECK_WR_RES(bstream_wr_string(s,info->engine.name));
    CHECK_WR_RES(bstream_wr_byte(s,info->engine.major));
    CHECK_WR_RES(bstream_wr_byte(s,info->engine.minor));
  }

  wr_error:

  return ret;
}

/**
  Read description of table data snapshot and save it in
  @c st_bstream_snapshot_info structure.

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached

  @note
  This function allocates memory to store snapshot info. The caller is
  responsible for freeing this memory.
*/
int bstream_rd_image_info(backup_stream *s, struct st_bstream_snapshot_info *info)
{
  unsigned short int type;
  int ret= BSTREAM_OK;

  bzero(info,sizeof(struct st_bstream_snapshot_info));

  CHECK_RD_OK(bstream_rd_byte(s,&type));

  switch (type) {
  case 0: type= BI_NATIVE; break;
  case 1: type= BI_DEFAULT; break;
  case 2: type= BI_CS; break;
  default: return BSTREAM_ERROR;
  }

  info->type= type;

  CHECK_RD_OK(bstream_rd_int2(s,&info->version));
  CHECK_RD_OK(bstream_rd_int2(s,&info->options));
  CHECK_RD_RES(bstream_rd_num(s,&info->table_count));

  if (type == BI_NATIVE )
  {
    if (ret != BSTREAM_OK)
      return BSTREAM_ERROR;

    CHECK_RD_OK(bstream_rd_string(s,&info->engine.name));
    CHECK_RD_OK(bstream_rd_byte(s,&info->engine.major));
    CHECK_RD_RES(bstream_rd_byte(s,&info->engine.minor));
  }

  rd_error:

  return ret;
}

/*************************************************************************
 *
 *   CATALOGUE
 *
 *************************************************************************/

/**
  @page stream_format

  @section catalogue Image catalogue

  The catalogue describes what items are stored in the image. Note that it
  doesn't contain any meta-data, only item names and other information needed to
  identify and select them.
  @verbatim

  [catalogue]= [ charsets ! 0x00 ! users ! 0x00 ! databases |
                 db catalogue | ... | db catalogue ]
  @endverbatim

  Catalogue starts with list of charsets where each charset is identified by its
  name. In other places of the image, charsets can be identified by their
  positions in this list. Number of charsets is limited to 256 so that one byte
  is enough to identify a charset.
  @verbatim

  [charsets]= [ charset name ! ... ! charset name ]
  @endverbatim

  Two first entries in [charsets] have special meaning and should be always
  present.

  First charset is the charset used to encode all strings stored in
  the preamble. This should be a universal charset like UTF8, capable
  of representing any string.

  Second charset in the list is the default charset of the server on which
  image was created. It can be the same as the first charset.

  The following charsets are any charsets used by the items stored in the image
  and thus needed to restore these items.
  @verbatim

  [users]= [ user name ! ... ! user name ]
  @endverbatim

  User list contains users for which any privileges are stored in the image.

  After [users] a list of all databases follows. If the list is empty, it
  consists of a single null string. Otherwise it has format:
  @verbatim

  [databases]= [ db info ! ... ! db info ]

  [db info]= [ db name ! db flags:1 ! optional extra data ]
  [db flags]= [ has extra data:.1 ! unused:.7 ]
  [optional extra data]= [data len:2 ! the data:(data len) ]
  @endverbatim

  [optional extra data] is present only if indicated in the flags.

  If there are no databases in the image, the database list is empty and there
  are no database catalogues.
  @verbatim

  [catalogue (no databases)] = [ charsets ! 0x00 ! users ! 0x00 ]
  @endverbatim
*/

#define BSTREAM_FLAG_HAS_EXTRA_DATA   0x80

int bstream_wr_db_catalogue(backup_stream*, struct st_bstream_image_header*,
                            struct st_bstream_db_info*);
int bstream_rd_db_catalogue(backup_stream*, struct st_bstream_image_header*,
                            struct st_bstream_db_info*);

/**
  Save catalogue of backup image.

  The contents of the image is read from the @c cat object using iterators
  and @c bcat_*() functions defined by the program using this library.

  @see @c bcat_iterator_get(), @c bcat_iterator_next(), @c bcat_iterator_free()
*/
int bstream_wr_catalogue(backup_stream *s, struct st_bstream_image_header *cat)
{
  void *it;
  blob *name;
  struct st_bstream_db_info *db_info;
  int ret;

  /* charset list */

  it= bcat_iterator_get(cat,BSTREAM_IT_CHARSET);

  if (!it)
    return BSTREAM_ERROR;

  while ((name= (blob*) bcat_iterator_next(cat,it)))
  {
    CHECK_WR_RES(bstream_wr_string(s,*name));
  }

  CHECK_WR_RES(bstream_wr_byte(s,0x00));

  bcat_iterator_free(cat,it);

  /* list of users */

  it= bcat_iterator_get(cat,BSTREAM_IT_USER);

  if (!it)
    return BSTREAM_ERROR;

  while ((name= (blob*) bcat_iterator_next(cat,it)))
  {
    CHECK_WR_RES(bstream_wr_string(s,*name));
  }

  CHECK_WR_RES(bstream_wr_byte(s,0x00));

  bcat_iterator_free(cat,it);

  /* list of databases */

  it= bcat_iterator_get(cat,BSTREAM_IT_DB);

  if (!it)
    return BSTREAM_ERROR;

  while ((db_info= (struct st_bstream_db_info*) bcat_iterator_next(cat,it)))
  {
    CHECK_WR_RES(bstream_wr_string(s,db_info->base.name));
    CHECK_WR_RES(bstream_wr_byte(s,0x00)); /* flags */
  }

  bcat_iterator_free(cat,it);

  /* db catalogues */

  it= bcat_iterator_get(cat,BSTREAM_IT_DB);

  if (!it)
    return BSTREAM_ERROR;

  while ((db_info= (struct st_bstream_db_info*) bcat_iterator_next(cat,it)))
  {
    CHECK_WR_RES(bstream_end_chunk(s));
    CHECK_WR_RES(bstream_wr_db_catalogue(s,cat,db_info));
  }

  bcat_iterator_free(cat,it);

  wr_error:

  return ret;
}

/**
  Read backup image catalogue.

  The @c cat object is populated with items read from the stream using
  @c bcat_add_item() function defined by the program using this library.

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached

  @see @c bcat_add_item()
*/
int bstream_rd_catalogue(backup_stream *s, struct st_bstream_image_header *cat)
{
  int ret= BSTREAM_OK;
  unsigned short int flags;
  unsigned int len;
  void *iter;
  struct st_bstream_item_info item;
  struct st_bstream_db_info *db_info;

  ret= bcat_reset(cat);
  if (ret != BSTREAM_OK)
    return BSTREAM_ERROR;

  /* charset list */

  item.type= BSTREAM_IT_CHARSET;
  item.pos= 0;

  do{

    CHECK_RD_OK(bstream_rd_string(s,&item.name));

    /* empty string signals end of the list */
    if (item.name.begin == NULL)
      break;

    if (bcat_add_item(cat,&item) != BSTREAM_OK)
      return BSTREAM_ERROR;

    item.pos++;

  } while (ret == BSTREAM_OK);

  /* list of users */

  item.type= BSTREAM_IT_USER;

  do{

    CHECK_RD_RES(bstream_rd_string(s,&item.name));

    /* empty string signals end of the list */
    if (item.name.begin == NULL)
      break;

    if (bcat_add_item(cat,&item) != BSTREAM_OK)
      return BSTREAM_ERROR;

    item.pos++;

  } while (ret == BSTREAM_OK);

  /*
    If ret != BSTREAM_OK here, we hit end of chunk or stream. This means
    that there are no databases in the image and we are done reading the
    catalogue.
   */
  if (ret != BSTREAM_OK)
    return ret;

  /* list of databases */

  item.type= BSTREAM_IT_DB;
  item.pos= 0;

  do {

    CHECK_RD_OK(bstream_rd_string(s,&item.name));

    if (bcat_add_item(cat,&item) != BSTREAM_OK)
      return BSTREAM_ERROR;

    item.pos++;

    CHECK_RD_RES(bstream_rd_byte(s,&flags));

    if (flags & BSTREAM_FLAG_HAS_EXTRA_DATA)
    {
      if (ret != BSTREAM_OK)
        return BSTREAM_ERROR;

      CHECK_RD_OK(bstream_rd_int2(s,&len));
      CHECK_RD_RES(bstream_skip(s,len));
    }

  } while (ret == BSTREAM_OK);


  /* db catalogues */

  iter= bcat_iterator_get(cat,BSTREAM_IT_DB);

  if (!iter)
    return BSTREAM_ERROR;

  while ((db_info= (struct st_bstream_db_info*) bcat_iterator_next(cat,iter)))
  {
    if (ret != BSTREAM_EOC)
      return BSTREAM_ERROR;

    CHECK_RD_OK(bstream_next_chunk(s));
    CHECK_RD_RES(bstream_rd_db_catalogue(s,cat,db_info));
  }

  bcat_iterator_free(cat,iter);

  if (bcat_close(cat) != BSTREAM_OK)
    return BSTREAM_ERROR;

  rd_error:

  return ret;
}

/**
  @page stream_format

  Encoding of item types used in a backup image.

  - 1 = character set,
  - 2 = user,
  - 3 = privilege,
  - 4 = database,
  - 5 = table,
  - 6 = view.

  Value 0 doesn't encode a valid item type and is used as item list separator.
 */

/**
  Save item type.

  @retval BSTREAM_OK   type was saved successfully
  @retval BSTREAM_ERROR error writing or attempt to save unknown type.
*/
int bstream_wr_item_type(backup_stream *s, enum enum_bstream_item_type type)
{
  switch (type) {
  case BSTREAM_IT_CHARSET:   return bstream_wr_int2(s,1);
  case BSTREAM_IT_USER:      return bstream_wr_int2(s,2);
  case BSTREAM_IT_PRIVILEGE: return bstream_wr_int2(s,3);
  case BSTREAM_IT_DB:        return bstream_wr_int2(s,4);
  case BSTREAM_IT_TABLE:     return bstream_wr_int2(s,5);
  case BSTREAM_IT_VIEW:      return bstream_wr_int2(s,6);
  case BSTREAM_IT_LAST:      return bstream_wr_int2(s,0);
  default: return BSTREAM_ERROR;
  }
}

/**
  Read item type.

  @retval BSTREAM_ERROR  Error while reading or non-recognized type found.
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached
*/
int bstream_rd_item_type(backup_stream *s, enum enum_bstream_item_type *type)
{
  int ret;
  unsigned int x;

  ret= bstream_rd_int2(s,&x);

  if (ret == BSTREAM_ERROR)
    return BSTREAM_ERROR;

  switch (x) {
  case 0: *type= BSTREAM_IT_LAST; break;
  case 1: *type= BSTREAM_IT_CHARSET; break;
  case 2: *type= BSTREAM_IT_USER; break;
  case 3: *type= BSTREAM_IT_PRIVILEGE; break;
  case 4: *type= BSTREAM_IT_DB; break;
  case 5: *type= BSTREAM_IT_TABLE; break;
  case 6: *type= BSTREAM_IT_VIEW; break;
  default: return BSTREAM_ERROR;
  }

  return ret;
}

/*
  @page stream_format

  @subsection db_catalogue Database catalogue

  Database catalogue lists all tables and other per-db items belonging to that
  database.
  @verbatim

  [db catalogue]= [ db-item info ! ... ! db-item info ]
  @endverbatim

  Each entry in the catalogue describes a single item, which can be a table or
  of other kind.
  @verbatim

  [db-item info]= [ type:2 ! name ! optional item data ]

  [optional item data] is used only for tables:

  [optional item data (table)]= [ flags:1 ! snapshot no:1 ! optional extra data ]
  @endverbatim

  [snapshot no] tells which snapshot contains tables data.

  Presence of extra data is indicated by a flag.
  @verbatim

  [flags]= [ has_extra_data:.1 ! unused:.7 ]

  [optional extra data]= [ data_len:1 ! extra data:(data_len) ]
  @endverbatim

  If database is empty, it stores two 0x00 bytes.
  @verbatim

  [db catalogue (empty)] = [ 0x00 0x00 ]
  @endverbatim
*/


/** Save catalogue of items belonging to given database. */
int bstream_wr_db_catalogue(backup_stream *s, struct st_bstream_image_header *cat,
                            struct st_bstream_db_info *db_info)
{
  void *iter;
  struct st_bstream_dbitem_info *item;
  int ret;
  bool catalogue_empty= TRUE;

  iter= bcat_db_iterator_get(cat, db_info);

  if (!iter)
    return BSTREAM_ERROR;

  while ((item= bcat_db_iterator_next(cat, db_info, iter)))
  {
    catalogue_empty= FALSE;

    CHECK_WR_RES(bstream_wr_item_type(s,item->base.type));
    CHECK_WR_RES(bstream_wr_string(s, item->base.name));

    if (item->base.type == BSTREAM_IT_TABLE)
    {
      CHECK_WR_RES(bstream_wr_byte(s,0x00)); /* flags: we don't use extra data */
      CHECK_WR_RES(bstream_wr_byte(s,((struct st_bstream_table_info*)item)->snap_no));
      CHECK_WR_RES(bstream_wr_num(s,item->base.pos));
    }
  }

  bcat_db_iterator_free(cat, db_info, iter);

  if (catalogue_empty)
    CHECK_WR_RES(bstream_wr_item_type(s,BSTREAM_IT_LAST));

  wr_error:

  return ret;
}

/**
  Read catalogue of given database.

  Object @c cat is populated with the items read using @c bcat_add_item()
  function defined by the program using this library.


  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached

  @see @c bcat_add_item()
*/
int bstream_rd_db_catalogue(backup_stream *s, struct st_bstream_image_header *cat,
                            struct st_bstream_db_info *db_info)
{
  unsigned short int flags;
  struct st_bstream_table_info ti;
  int ret;

  bzero(&ti,sizeof(ti));
  ti.base.db= db_info;

  /* we read the first byte to see if the catalogue is empty */

  CHECK_RD_RES(bstream_rd_item_type(s,&ti.base.base.type));

  if (ti.base.base.type == BSTREAM_IT_LAST)
    return ret;

  /* we have some entries in the catalogue, we read them in the following loop */

  do {

    CHECK_RD_RES(bstream_rd_string(s,&ti.base.base.name));

    if (ti.base.base.type == BSTREAM_IT_TABLE)
    {
      if (ret != BSTREAM_OK)
        return BSTREAM_ERROR;

      CHECK_RD_OK(bstream_rd_byte(s,&flags)); /* flags are ignored currently */
      CHECK_RD_OK(bstream_rd_byte(s,&ti.snap_no));
      CHECK_RD_RES(bstream_rd_num(s,&ti.base.base.pos));
    }

    if (bcat_add_item(cat, &ti.base.base) != BSTREAM_OK)
      return BSTREAM_ERROR;

    /* read type of next item, if we haven't hit end of chunk/stream */

    if (ret == BSTREAM_OK)
      CHECK_RD_OK(bstream_rd_item_type(s,&ti.base.base.type));

  } while (ret == BSTREAM_OK);

  rd_error:

  return ret;
}

/*************************************************************************
 *
 *   META DATA
 *
 *************************************************************************/

/**
  @page stream_format

  @section meta_data Meta data section

  Meta data section contains meta-data for items which need to be created
  when restoring data. It is divided into three main parts, storing meta data
  for global items, tables and other items (per-db and per-table).
  @verbatim

  [meta data]= [ global items | tables | other items ]
  @endverbatim

  [Global items] include all databases. [Tables] section contains all tables
  which are grouped on per-database basis (this is for easier skipping of tables
  upon selective restore).
  @verbatim

  [tables] = [ tables from db1 | ... | tables from dbN ]
  @endverbatim

  [Other items] has two parts for all per-database items (except tables) and
  all per-table items.
  @verbatim

  [other items]= [ per-db items ! 0x00 0x00 ! per-table items ]
  @endverbatim

  The per-database items other than tables can not be grouped by database
  because of possible inter-database dependenciens. This is why they are stored
  in a separate section.

  If there are no databases in the image, [meta data] consists of [global items]
  only.
  @verbatim

  [meta data (no databases)]= [ global items ]
  @endverbatim

  Meta data item lists can be empty or consist of several item entries. Empty
  item list consist of two 0x00 bytes which can not start any valid
  [item entry].
  @verbatim

  [item list] = [ item entry ! ... ! item entry ]
  [item list (empty)]= [ 0x00 0x00 ]
  @endverbatim
*/

/** different formats in which item positions are stored */
enum enum_bstream_meta_item_kind {
  GLOBAL_ITEM,   /**< only item position is stored */
  TABLE_ITEM,    /**< only table position is stored (database is implicit) */
  PER_DB_ITEM,   /**< item position followed by it's database position */
  /**
    Item position followed by it's table's database position followed by the
    table's position inside that database
  */
  PER_TABLE_ITEM
};

int bstream_wr_meta_item(backup_stream*, enum enum_bstream_meta_item_kind,
                         unsigned short int, struct st_bstream_item_info*);

int bstream_rd_meta_item(backup_stream *s,
                         enum enum_bstream_meta_item_kind kind,
                         unsigned short int *flags,
                         struct st_bstream_item_info **item);

int bstream_wr_item_def(backup_stream*, struct st_bstream_image_header*,
                        enum enum_bstream_meta_item_kind,
                        struct st_bstream_item_info*);

int read_and_create_items(backup_stream *s, struct st_bstream_image_header *cat,
                          enum enum_bstream_meta_item_kind kind);

/** Write meta-data section of a backup image */
int bstream_wr_meta_data(backup_stream *s, struct st_bstream_image_header *cat)
{
  void *iter, *titer;
  struct st_bstream_item_info *item;
  struct st_bstream_db_info   *db_info;
  int ret= BSTREAM_OK;
  bool item_written= FALSE;
  bool has_db= FALSE;

  /* global items (this includes databases) */

  iter= bcat_iterator_get(cat,BSTREAM_IT_GLOBAL);

  if (!iter)
    return BSTREAM_ERROR;

  while ((item= bcat_iterator_next(cat,iter)))
  {
    item_written= TRUE;
    CHECK_WR_RES(bstream_wr_item_def(s,cat,GLOBAL_ITEM,item));
  }

  /* mark empty list if no items were written */
  if (!item_written)
    CHECK_WR_RES(bstream_wr_item_type(s,BSTREAM_IT_LAST));

  bcat_iterator_free(cat,iter);

  /* tables */

  iter= bcat_iterator_get(cat,BSTREAM_IT_DB);

  if (!iter)
    return BSTREAM_ERROR;

  while ((db_info= (struct st_bstream_db_info*)bcat_iterator_next(cat,iter)))
  {
    has_db= TRUE;
    CHECK_WR_RES(bstream_end_chunk(s));

    titer= bcat_db_iterator_get(cat,db_info);

    if (!titer)
      return BSTREAM_ERROR;

    item_written= FALSE;
    while ((item= (struct st_bstream_item_info*)
                  bcat_db_iterator_next(cat,db_info,titer)))
    {
      if (item->type != BSTREAM_IT_TABLE)
        continue;

      CHECK_WR_RES(bstream_wr_item_def(s,cat,TABLE_ITEM,item));
      item_written= TRUE;
    }

    /* mark empty list */
    if (!item_written)
      CHECK_WR_RES(bstream_wr_item_type(s,BSTREAM_IT_LAST));

    bcat_db_iterator_free(cat,db_info,titer);
  }

  bcat_iterator_free(cat,iter);

  /* if we found no databases in the catalogue, we are done */
  if (!has_db)
    return BSTREAM_OK;

  /* other per-db items */

  CHECK_WR_RES(bstream_end_chunk(s));

  iter= bcat_iterator_get(cat,BSTREAM_IT_PERDB);

  if (!iter)
    return BSTREAM_ERROR;

  while ((item= bcat_iterator_next(cat,iter)))
  {
    if (item->type == BSTREAM_IT_TABLE)
      continue;

    CHECK_WR_RES(bstream_wr_item_def(s,cat,PER_DB_ITEM,item));
  }

  bcat_iterator_free(cat,iter);

  /* per-table items */

  CHECK_WR_RES(bstream_wr_item_type(s,BSTREAM_IT_LAST));

  iter= bcat_iterator_get(cat,BSTREAM_IT_PERTABLE);

  if (!iter)
    return BSTREAM_ERROR;

  while ((item= bcat_iterator_next(cat,iter)))
  {
    if (item->type == BSTREAM_IT_TABLE)
      continue;

    CHECK_WR_RES(bstream_wr_item_def(s,cat,PER_TABLE_ITEM,item));
  }

  bcat_iterator_free(cat,iter);

  wr_error:

  return ret;
}

/**
  Read backup image meta-data section.

  All items read are created using @c bstream_create_item() function.

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached
*/
int bstream_rd_meta_data(backup_stream *s, struct st_bstream_image_header *cat)
{
  void *iter;
  struct st_bstream_db_info *db_info;
  int ret=BSTREAM_OK;
  bool has_db= FALSE;

  /* global items */

  CHECK_RD_RES(read_and_create_items(s,cat,GLOBAL_ITEM));

  /* tables */

  iter= bcat_iterator_get(cat,BSTREAM_IT_DB);

  if (!iter)
    return BSTREAM_ERROR;

  while ((db_info= (struct st_bstream_db_info*)bcat_iterator_next(cat,iter)))
  {
    has_db= TRUE;

    if (ret != BSTREAM_EOC)
      return BSTREAM_ERROR;

    CHECK_RD_OK(bstream_next_chunk(s));
    CHECK_RD_RES(read_and_create_items(s,cat,TABLE_ITEM));
  }

  bcat_iterator_free(cat,iter);

  /* if image has no databases, there is nothing more to read */

  if (!has_db)
    return ret;

  /* other per-db item */

  if (ret != BSTREAM_EOC)
    return BSTREAM_ERROR;

  CHECK_RD_OK(bstream_next_chunk(s));
  CHECK_RD_RES(read_and_create_items(s,cat,PER_DB_ITEM));

  /*
    If we hit end of chunk/stream, there is nothing more to read
    (no per-table items)
  */

  if (ret != BSTREAM_OK)
    return ret;

  /* per-table items */

  CHECK_RD_RES(read_and_create_items(s,cat,PER_TABLE_ITEM));

  rd_error:

  return ret;
}


/**
  @page stream_format

  @subsection item_entry Single item entry

  Item list is a sequence of meta data item entries, each having the
  following format:
  @verbatim

  [item entry]= [ type:2 ! flags:1 ! position in the catalogue !
                  optional extra data ! optional CREATE statement ]
  @endverbatim

  Item meta-data contains a CREATE statement or other data in unspecified format
  or both. [flags] inform about which meta-data elements are present in the
  entry.
  @verbatim

  [flags]= [ has_extra_data:.1 ! has_create_stmt:.1 ! unused:.6 ]
  @endverbatim

  The position in the catalogue is represented by 1 to 3 numbers, depending on
  in which part of catalogue the entry lies.
  @verbatim

  [item position (global)]= [db no]
  [item position (table)]= [ snap no ! pos in snapshot's table list ]
  [item position (other per-db item)]= [ pos in db item list ! db no ]
  [item position (per-table item)] = [ pos in table's item list ! db no ! table pos ]
  @endverbatim

  Note that table is identified by its position inside the snapshot to which it
  belongs.
  @verbatim

  [optional extra data]= [ data_len:2 ! extra data:(data_len) ]
  @endverbatim
*/

#define BSTREAM_FLAG_HAS_CREATE_STMT 0x40

/**
  Write entry describing single item but without CREATE statement or
  other meta data.
*/
int bstream_wr_meta_item(backup_stream *s,
                    enum enum_bstream_meta_item_kind kind,
                    unsigned short int flags,
                    struct st_bstream_item_info *item)
{
  int ret= BSTREAM_OK;

  /* save type and flags */

  CHECK_WR_RES(bstream_wr_item_type(s,item->type));
  CHECK_WR_RES(bstream_wr_byte(s,flags));

  /* save item's position in the catalogue */

  CHECK_WR_RES(bstream_wr_num(s,item->pos));

  if (kind == TABLE_ITEM)
  {
    CHECK_WR_RES(bstream_wr_byte(s,((struct st_bstream_table_info*)item)->snap_no));
    return ret;
  }

  if ((kind == PER_TABLE_ITEM) || (kind == PER_DB_ITEM))
    CHECK_WR_RES(bstream_wr_num(s,((struct st_bstream_dbitem_info*)item)->db->base.pos));

  if (kind == PER_TABLE_ITEM)
    CHECK_WR_RES(bstream_wr_num(s,
                   ((struct st_bstream_titem_info*)item)->table->base.base.pos));

  wr_error:

  return ret;
}

/**
  Read initial part of item entry and locate that item in the catalogue.

  Pointer to an appropriate structure describing the located item is stored in
  @c (*item). This description is not persistent - next call to this function
  can overwrite it with description of another item.

  @param cat  the catalogue object where items are located
  @param db   default database for per-db items for which database coordinates
              are not stored in the entry (i.e., for tables)
  @param kind  format in which item coordinates are stored
  @param flags the flags saved in the entry are stored in that location
  @param item  pointer to a structure describing item found is stored here.


  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached

  If function returns BSTREAM_OK and @c (*item) is set to NULL, it means that we
  are looking at an empty item list.
*/
int bstream_rd_meta_item(backup_stream *s,
                         enum enum_bstream_meta_item_kind kind,
                         unsigned short int *flags,
                         struct st_bstream_item_info **item)
{
  static struct st_bstream_db_info db;
  static struct st_bstream_table_info table;

  static union
  {
    struct st_bstream_item_info   any;
    struct st_bstream_db_info     db;
    struct st_bstream_table_info  table;
    struct st_bstream_dbitem_info per_db;
    struct st_bstream_titem_info  per_table;
  } item_buf;

  int ret= BSTREAM_OK;

  CHECK_RD_RES(bstream_rd_item_type(s,&item_buf.any.type));

  /* type == BSTREAM_IT_LAST means that we hit a no-item marker (0x00) */
  if (item_buf.any.type == BSTREAM_IT_LAST)
  {
    *item= NULL;
    return ret;
  }

  if (ret != BSTREAM_OK)
    return BSTREAM_ERROR;

  ASSERT(item);
  *item= &item_buf.any;

  CHECK_RD_OK(bstream_rd_byte(s,flags));

  /* read item's position */

  CHECK_RD_RES(bstream_rd_num(s,&item_buf.any.pos));

  if (kind == TABLE_ITEM)
  {
    if (ret != BSTREAM_OK)
      return BSTREAM_ERROR;

    CHECK_RD_RES(bstream_rd_byte(s,&item_buf.table.snap_no));
      return ret;
  }

  /* read db pos if present */

  if ((kind == PER_TABLE_ITEM) || (kind == PER_DB_ITEM))
  {
    if (ret != BSTREAM_OK)
      return BSTREAM_ERROR;

    db.base.type= BSTREAM_IT_DB;
    CHECK_RD_RES(bstream_rd_num(s,&db.base.pos));

    item_buf.per_db.db= &db;
  }

  /* read table pos if present */

  if (kind == PER_TABLE_ITEM)
  {
    if (ret != BSTREAM_OK)
      return BSTREAM_ERROR;

    table.base.base.type= BSTREAM_IT_TABLE;
    table.base.db= &db;
    CHECK_RD_RES(bstream_rd_num(s,&table.base.base.pos));

    item_buf.per_table.table= &table;
  }

  rd_error:

  return ret;
}


/**
  Write entry with given item's meta-data.

  @param kind  determines format in which item's coordinates are saved.
*/
int bstream_wr_item_def(backup_stream *s,
                   struct st_bstream_image_header *cat,
                   enum enum_bstream_meta_item_kind kind,
                   struct st_bstream_item_info *item)
{
  unsigned short int flags= 0x00;
  blob query;
  blob data;
  int ret=BSTREAM_OK;

  if (bcat_get_item_create_query(cat,item,&query) == BSTREAM_OK)
    flags |= BSTREAM_FLAG_HAS_CREATE_STMT;

  if (bcat_get_item_create_data(cat,item,&data) == BSTREAM_OK)
    flags |= BSTREAM_FLAG_HAS_EXTRA_DATA;

  ret= bstream_wr_meta_item(s,kind,flags,item);

  /* save create query and/or create data */

  if (flags & BSTREAM_FLAG_HAS_EXTRA_DATA)
    CHECK_WR_RES(bstream_wr_string(s,data));

  if (flags & BSTREAM_FLAG_HAS_CREATE_STMT)
    CHECK_WR_RES(bstream_wr_string(s,query));

  wr_error:

  return ret;
}

/**
  Read list of meta-data entries and create the corresponding items.

  The entries are read until the end of chunk or 0x00 marker is hit.
  After reading meta-data for each item (CREATE statement and/or extra meta-data)
  the item is created using @c bstream_create_item() function which should be
  implemented by the program using this library.

  @retval BSTREAM_ERROR error while reading
  @retval BSTREAM_EOC   either list was empty or all items were read and created
                        successfully
  @retval BSTREAM_EOS   all items read and created successfully and end of
                        stream has been reached
*/
int read_and_create_items(backup_stream *s, struct st_bstream_image_header *cat,
                          enum enum_bstream_meta_item_kind kind)
{
  unsigned short int flags;
  unsigned int ret;
  struct st_bstream_item_info *item;
  blob query,data;

  do {

    CHECK_RD_RES(bstream_rd_meta_item(s,kind,&flags,&item));

    /* if 0x00 marker was read, item == NULL */
    if (item == NULL)
      return ret;

    query.begin= query.end= NULL;
    data.begin= data.end= NULL;

    if (flags & BSTREAM_FLAG_HAS_EXTRA_DATA)
    {
      if (ret != BSTREAM_OK)
        return BSTREAM_ERROR;
      CHECK_RD_RES(bstream_rd_string(s,&data));
    }

    if (flags & BSTREAM_FLAG_HAS_CREATE_STMT)
    {
      if (ret != BSTREAM_OK)
        return BSTREAM_ERROR;
      CHECK_RD_RES(bstream_rd_string(s,&query));
    }

    if (bcat_create_item(cat,item,query,data) != BSTREAM_OK)
      return BSTREAM_ERROR;

    bstream_free(query.begin);
    bstream_free(data.begin);

  } while (ret == BSTREAM_OK);

  rd_error:

  return ret;
}


/*************************************************************************
 *
 *   TABLE DATA
 *
 *************************************************************************/

/**
  @page stream_format

  @section data Table data section

  Format of table data section of backup image.
  @verbatim

  [table data]= [ table data chunk | ... | table data chunk ]

  [table data chunk]= [ snapshot no:1 ! seq no:2 ! flags:1 ! table no ! data ]
  @endverbatim

  Data chunks of each snapshot are numbered by consecutive numbers. This can be
  used to detect discontinuities in a backup stream. Currently only one flag
  is used, indicating last data chunk for a given table.
  @verbatim

  [flags]= [ unused:.7 ! last data block:.1 ]
  @endverbatim
*/

/**
  Write chunk with data from backup driver.
*/
int bstream_wr_data_chunk(backup_stream *s,
                     struct st_bstream_data_chunk *chunk)
{
  int ret= BSTREAM_OK;

  ASSERT(chunk);

  CHECK_WR_RES(bstream_wr_byte(s,chunk->snap_no + 1));
  CHECK_WR_RES(bstream_wr_int2(s,0)); /* sequence number - not used now */
  CHECK_WR_RES(bstream_wr_byte(s,chunk->flags));
  CHECK_WR_RES(bstream_wr_num(s,chunk->table_no));
  CHECK_WR_RES(bstream_write_blob(s,chunk->data));
  CHECK_WR_RES(bstream_end_chunk(s));

  wr_error:

  return ret;
}

/**
  The amount by which input buffer is increased if whole data chunk can't fit
  into it.
*/
#define DATA_BUF_STEP   (1024*1024)


/**
  Read chunk of data for restore driver.

  Blob @c chunk->data describes memory area where data from the chunk is saved.
  If this blob is non-empty when function is called, the data will be stored
  in the given area if it fits. If chunk data doesn't fit, or the blob was
  empty, the data is read into an internal buffer and the blob is updated to
  indicate where to find it. In the latter case the data will be overwritten upon
  next call to this function.

  @retval BSTREAM_OK   data chunk has been read, stream moved to next chunk
  @retval BSTREAM_EOS  data chunk has been read, no more chunks in the stream.
  @retval BSTREAM_EOC  not a data chunk (0x00 read). Note: stream doesn't have to be
                  at end of chunk!

  Return value @c BSTREAM_EOC indicates that all table data chunks have been read.
  The rest of the backup stream can contain image summary block.
*/
int bstream_rd_data_chunk(backup_stream *s,
                     struct st_bstream_data_chunk *chunk)
{
  blob *buf= &s->data_buf;
  byte *new_buf;
  blob *envelope;
  blob to_read;
  unsigned long int howmuch;
  unsigned int seq_no;
  int ret= BSTREAM_OK;

  ASSERT(chunk);

  CHECK_RD_RES(bstream_rd_byte(s,&chunk->snap_no));

  /*
    Saved snapshot numbers start from 1 - if we read 0 it means that this is not
    a table data chunk
  */
  if (chunk->snap_no == 0)
    return BSTREAM_EOC;
  else if (ret != BSTREAM_OK)
    return BSTREAM_ERROR;

  (chunk->snap_no)--;

  CHECK_RD_OK(bstream_rd_int2(s,&seq_no));  /* FIxME: handle sequence numbers */
  CHECK_RD_OK(bstream_rd_byte(s,&chunk->flags));
  CHECK_RD_OK(bstream_rd_num(s,&chunk->table_no));

  /*
    read rest of the chunk data into provided buffer or the internal buffer
    @c s->data_buf if there is not enough space
  */

  envelope= &chunk->data; /* envelope indicates which buffer we are using:
                             provided or internal (initially provided) */
  to_read= *envelope; /* how much space is available for reading bytes */

  /*
    In the following loop we call bstream_read_part() until we hit end of the chunk.
    If there is no more space to fit the data, buffer is enlarged (reallocated)
    and the bytes which were read before are copied into the new buffer.
  */

  while (ret == BSTREAM_OK)
  {
    /*
      Read bytes until current buffer is full or end of chunk is reached.
     */
    while (ret == BSTREAM_OK && (to_read.end > to_read.begin))
      ret= bstream_read_part(s,&to_read,*envelope);

    if (ret == BSTREAM_OK)
    {
      /* we have filled-up the buffer - we need to enlarge it */

      howmuch= to_read.begin - envelope->begin; /* how much data have been read
                                                   so far */
      /*
        If there is not enough space in the internal buffer, enlarge it.
      */
      if ( (buf->begin + howmuch) >= buf->end )
      {
        new_buf= bstream_alloc(howmuch + DATA_BUF_STEP);

        if (!new_buf)
          return BSTREAM_ERROR;

        /* copy data from old buffer to the new one */
        if (buf->begin && (buf->end > buf->begin))
          memmove(new_buf, buf->begin, buf->end - buf->begin);
        bstream_free(buf->begin);

        buf->begin= new_buf;
        buf->end= buf->begin + howmuch + DATA_BUF_STEP;
      }

      /* if we were using the provided buffer, switch to internal one */
      if (envelope == &chunk->data)
      {
        memmove(buf->begin, chunk->data.begin, howmuch);
        envelope= buf;
        chunk->data= *buf;
      }

      /* update to_read blob to indicate free space left */
      to_read.begin= buf->begin + howmuch;
      to_read.end= buf->end;
    }
  }

  if (ret == BSTREAM_ERROR)
    return BSTREAM_ERROR;

  /* We have read all data from the chunk - record where it ends */

  chunk->data.end= to_read.begin;

  /* move to next chunk */

  CHECK_RD_RES(bstream_next_chunk(s));

  rd_error:

  return ret;
}

/*********************************************************************
 *
 *   WRITING/READING BASIC TYPES
 *
 *********************************************************************/

/** Write single byte to backup stream */
int bstream_wr_byte(backup_stream *s, unsigned short int x)
{
  byte buf= x & 0xFF;
  blob b= { &buf, &buf + 1 };
  return bstream_write_part(s,&b,b);
}

/**
  Read one byte from backup stream.

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached
*/
int bstream_rd_byte(backup_stream *s, unsigned short int *x)
{
  byte buf;
  blob b= { &buf, &buf+1 };
  int ret;

  ret= bstream_read_part(s,&b,b);

  if (b.begin == b.end)
  {
    *x= buf;
    return ret;
  }
  else return BSTREAM_ERROR;
}

/**
  Write 2 byte unsigned number. Least significant byte
  comes first.
*/
int bstream_wr_int2(backup_stream *s, unsigned int x)
{
  byte buf[2]= { x & 0xFF, (x >> 8) & 0xFF };
  blob b= {buf, buf+2};

  return bstream_write_blob(s,b);
}

/**
  Read 2 byte unsigned number.

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached
*/
int bstream_rd_int2(backup_stream *s, unsigned int *x)
{
  byte buf[2];
  blob b= {buf, buf+2};
  int ret;

  ret= bstream_read_blob(s,b);
  if (ret == BSTREAM_ERROR)
    return BSTREAM_ERROR;

  *x = buf[0] + (buf[1] << 8);

  return ret;
}

/**
  Write 4 byte unsigned number. Least significant bytes come first.
*/
int bstream_wr_int4(backup_stream *s, unsigned long int x)
{
  byte buf[4];
  blob b= {buf, buf+4};

  buf[0]= x & 0xFF; x >>= 8;
  buf[1]= x & 0xFF; x >>= 8;
  buf[2]= x & 0xFF; x >>= 8;
  buf[3]= x & 0xFF; x >>= 8;

  return bstream_write_blob(s,b);
}

/**
  Read 4 byte unsigned number.

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached
*/
int bstream_rd_int4(backup_stream *s, unsigned long int *x)
{
  byte buf[4];
  blob b= {buf, buf+4};
  int ret;

  ret= bstream_read_blob(s,b);
  if (ret == BSTREAM_ERROR)
    return BSTREAM_ERROR;

  *x = buf[0];
  *x += (buf[1] << 8);
  *x += (buf[1] << 2*8);
  *x += (buf[1] << 3*8);

  return ret;
}

/**
  Write number using variable length format.

  Number is stored as a sequence of bytes, each byte storing 7 bits. The most
  significant bit in a byte tells if there are more bytes to follow
  (if it is set) or if current byte is the last one (if it is not set).
  The bits are saved starting with least significant ones.
*/
int bstream_wr_num(backup_stream *s, unsigned long int x)
{
  int ret= BSTREAM_OK;

  do {
    CHECK_WR_RES(bstream_wr_byte(s, (x & 0x7F) | ( x>0x7F ? 0x80: 0)));
    x >>= 7;
  } while (x);

  wr_error:

  return ret;
}

/**
  Read number saved using variable length format.

  @retval BSTREAM_ERROR  Error while reading or number doesn't fit in unsigned
                         long variable
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached
*/
int bstream_rd_num(backup_stream *s, unsigned long int *x)
{
  unsigned short int b;
  unsigned int i=0;
  int ret= BSTREAM_OK;

  *x= 0;

  do {

    ret= bstream_rd_byte(s,&b);

    if (ret == BSTREAM_ERROR)
      return BSTREAM_ERROR;

    *x += (b & 0x7F) << (7*(i++));

  } while ((b & 0x80) && (i < sizeof(unsigned long int)));

  return (b & 0x80) ? BSTREAM_ERROR : ret;
}

/*
  String format.

  [string]= [ size ! string bytes:(size) ]

  All strings are stored using the same universal character set, which is listed
  in image's catalogue as the first entry.
*/

/**
  Write a string.
*/
int bstream_wr_string(backup_stream *s, bstream_blob str)
{
  int ret= BSTREAM_OK;

  CHECK_WR_RES(bstream_wr_num(s, str.end - str.begin));
  CHECK_WR_RES(bstream_write_blob(s, str));

  wr_error:

  return ret;
}

/**
  Read a string.

  New memory is allocated (with @c bstream_alloc() function) to accommodate the
  string. Blob @c str is updated to point at the memory where string is stored.
  The byte after last byte of the string is set to 0.

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached

  @note
  Caller of this function is responsible for freeing memory allocated to store
  the string.
*/
int bstream_rd_string(backup_stream *s, bstream_blob *str)
{
  int ret= BSTREAM_OK;
  unsigned long int len;

  ret= bstream_rd_num(s, &len);

  if (len == 0)
  {
    str->begin= str->end= NULL;
    return ret;
  }

  if (ret != BSTREAM_OK)
    return BSTREAM_ERROR;

  str->begin= bstream_alloc(len+1);
  if (!str->begin)
    return BSTREAM_ERROR;
  str->end= str->begin + len;
  *str->end= '\0';

  return bstream_read_blob(s, *str);
}

/*
 Time format:

 [time]= [ year and month:2 ! mday:1 ! hour:1 ! min:1 ! sec:1 ]

 [year and month]= [ year:.12 ! month:.4 ]

*/

/** Write time entry */
int bstream_wr_time(backup_stream *s, bstream_time_t *time)
{
  byte buf[6];
  blob b= {buf, buf+6};

  buf[0]= (time->year>>4) & 0xFF;
  buf[1]= ((time->year<<4) & 0xF0) | (time->mon &0x0F);
  buf[2]= time->mday;
  buf[3]= time->hour;
  buf[4]= time->min;
  buf[5]= time->sec;

  return bstream_write_blob(s,b);
}

/**
  Read time entry

  @retval BSTREAM_ERROR  Error while reading
  @retval BSTREAM_OK     Read successful
  @retval BSTREAM_EOC    Read successful and end of chunk has been reached
  @retval BSTREAM_EOS    Read successful and end of stream has been reached
*/
int bstream_rd_time(backup_stream *s, bstream_time_t *time)
{
  byte buf[6];
  blob b= {buf, buf+6};
  int ret= BSTREAM_OK;

  ret= bstream_read_blob(s,b);

  if (ret != BSTREAM_OK)
    return ret;

  time->year= (buf[0]<<4) + (buf[1]>>4);
  time->mon= buf[1] & 0x0F;
  time->mday= buf[2];
  time->hour= buf[3];
  time->min= buf[4];
  time->sec= buf[5];

  return BSTREAM_OK;
}
