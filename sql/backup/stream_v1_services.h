#ifndef STREAM_V1_SERVICES_H_
#define STREAM_V1_SERVICES_H_

#include <stream_v1.h>

/**
  @file

  @brief
  Service API for backup stream library (format version 1)

  This header declares functions which should be implemented by an user of
  the backup stream library. The library will call these functions to browse
  the catalogue of a backup image and perform other tasks.
*/

/*********************************************************************
 *
 *   CATALOGUE SERVICES
 *
 *********************************************************************/

/*
  Functions for manipulating backup image's catalogue.

  Backup stream library doesn't make any assumptions about how the catalogue
  of a backup image is stored internally. Instead, program using backup stream
  library must define the following functions which are used by the library
  to populate backup catalogue with items or to enumerate and access items
  stored there.
*/

/**
  Clear catalogue and prepare it for populating with items.
*/
int bcat_reset(struct st_bstream_image_header *catalogue);

/**
  Close catalogue after all items have been added to it.
*/
int bcat_close(struct st_bstream_image_header *catalogue);

/**
  Add item to the catalogue.

  The @c item->pos field should be set to indicate position of the item
  in the catalogue.

  @note
  Global, per-table and per-database items can have independent address
  spaces. Thus item belonging to a database is identified by its position inside
  that database's item list. Similar for items belonging to tables.
*/
int bcat_add_item(struct st_bstream_image_header *catalogue,
                  struct st_bstream_item_info *item);

/**
  Get global item stored in the catalogue at given position.
*/
struct st_bstream_item_info*
bcat_get_item(struct st_bstream_image_header *catalogue,
              unsigned long int pos);

/**
  Get per-database item stored in the catalogue at given position.
*/
struct st_bstream_dbitem_info*
bcat_get_db_item(struct st_bstream_image_header *catalogue,
                 struct st_bstream_db_info *db,
                 unsigned long int pos);
/**
  Get per-table item stored in the catalogue at given position.
*/
struct st_bstream_titem_info*
bcat_get_table_item(struct st_bstream_image_header *catalogue,
                    struct st_bstream_table_info *table,
                    unsigned long int pos);

/*
 Iterators are used to iterate over items inside backup catalogue.

 The internal implementation of an iterator is hidden behind this API which
 should be implemented by the program using backup stream library. An iterator
 is represented by a void* pointer pointing at a memory area containing its
 internal state.
*/

/**
  Create global iterator of a given type.

  Possible iterator types.

  - BSTREAM_IT_CHARSET: all charsets
  - BSTREAM_IT_USER:    all users
  - BSTREAM_IT_DB:      all databases

  The following types of iterators iterate only over items for which
  some meta-data should be saved in the image.

  - BSTREAM_IT_GLOBAL: all global items in create-dependency order
  - BSTREAM_IT_PERDB: all per-db items except tables which are enumerated by
                      a table iterator (see below)
  - BSTREAM_IT_PERTABLE: all per-table items in create-dependency orders.

  @return Pointer to the iterator or NULL in case of error.
*/
void* bcat_iterator_get(struct st_bstream_image_header *catalogue,
                        unsigned int type);

/**
  Return next item pointed by iterator.

  @return NULL if there are no more items in the set.
*/
struct st_bstream_item_info*
bcat_iterator_next(struct st_bstream_image_header *catalogue, void *iter);

/**
  Free iterator resources.

  @note
  The iterator can not be used after call to this function.
*/
void  bcat_iterator_free(struct st_bstream_image_header *catalogue, void *iter);

/**
  Create iterator for items belonging to a given database.
*/
void* bcat_db_iterator_get(struct st_bstream_image_header *catalogue,
                           struct st_bstream_db_info *db);

/** Return next item from database items iterator */
struct st_bstream_dbitem_info*
bcat_db_iterator_next(struct st_bstream_image_header *catalogue,
                      struct st_bstream_db_info *db,
                      void *iter);

/** Free database items iterator resources */
void  bcat_db_iterator_free(struct st_bstream_image_header *catalogue,
                            struct st_bstream_db_info *db,
                            void *iter);


/*********************************************************************
 *
 *   SAVING ITEM META DATA AND RESTORING ITEM FROM IT
 *
 *********************************************************************/

/**
  Produce CREATE statement for a given item.

  Backup stream library calls that function when saving item's
  meta-data. If function successfully produces the statement, it becomes
  part of meta-data.

  @retval BSTREAM_OK    blob @c stmt contains the CREATE query
  @retval BSTREAM_ERROR no CREATE statement for that item
*/
int bcat_get_item_create_query(struct st_bstream_image_header *catalogue,
                               struct st_bstream_item_info *item,
                               bstream_blob *stmt);

/**
  Return meta-data (other than CREATE statement) for a given item.

  Backup stream library calls that function when saving item's
  meta-data. If function returns successfully, the bytes returned become
  part of meta-data.

  @retval BSTREAM_OK    blob @c data contains the meta-data
  @retval BSTREAM_ERROR no extra meta-data for that item
*/
int bcat_get_item_create_data(struct st_bstream_image_header *catalogue,
                              struct st_bstream_item_info *item,
                              bstream_blob *data);

/**
  Create item from its meta-data.

  When the meta-data section of backup image is read, items are created
  as their meta-data is read (so that there is no need to store these
  meta-data). Backup stream library calls this function to create an item.

  @note
  Either @c create_stmt or @c other_meta_data or both can be empty, depending
  on what was stored in the image.

  @retval BSTREAM_OK     item created
  @retval BSTREAM_ERROR  error while creating item
*/
int bcat_create_item(struct st_bstream_image_header *catalogue,
                     struct st_bstream_item_info *item,
                     bstream_blob create_stmt,
                     bstream_blob other_meta_data);

/*********************************************************************
 *
 *   ABSTRACT STREAM INTERFACE
 *
 *********************************************************************/

/*
  The following typedefs define signatures of functions implementing basic
  I/O operations on the output stream. Application using backup stream stores
  pointers to appropriate functions inside backup_stream::stream structure.
*/

/**
  Function writing bytes to the underlying media.

  For specification, see @c bstream_write_part() function.
*/
typedef int (*as_write_m)(void*, bstream_blob*, bstream_blob);

/**
  Function reading bytes from the underlying media.

  For specification, see @c bstream_read_part() function.
*/
typedef int (*as_read_m)(void *, bstream_blob*, bstream_blob);

/**
  Function skipping bytes in the underlying stream.

  This function should move the read/write "head" of the underlying stream by
  the amount stored in variable pointed by @c offset. The variable is updated
  to show how much the head was actually moved.

  @retval BSTREAM_OK     operation successful
  @retval BSTREAM_ERROR  error has happened, @c *offset informs how many bytes
                         have been skipped.
*/
typedef int (*as_forward_m)(void *, unsigned long int *offest);


/*********************************************************************
 *
 *   MEMORY ALLOCATOR
 *
 *********************************************************************/

/*
 Programs which doesn't need a specialized memory allocator can define
 BSTREAM_USE_MALLOC macro before including this file and then the standard
 allocating functions will be used.

 Note: the BSTREAM_USE_MALLOC macro should be used in at most one compilation
 module - otherwise several copies of bstream_{malloc,free} will be defined.
*/

#ifdef BSTREAM_USE_MALLOC

#include <stdlib.h>

bstream_byte* bstream_alloc(unsigned long int size)
{ return (bstream_byte*)malloc(size); }

void bstream_free(bstream_byte *ptr)
{ free((bstream_byte*)ptr); }

#else

/** Allocate given amount of memory and return pointer to it */
bstream_byte* bstream_alloc(unsigned long int size);

/** Free previously allocated memory */
void bstream_free(bstream_byte *ptr);

#endif


#endif /*STREAM_V1_SERVICES_H_*/
