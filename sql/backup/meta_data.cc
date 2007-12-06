/**
  @file

  @brief Code for handling metadata.

  @todo Handle objects of unknown type
  @todo Get and save full CREATE statements also for databases.
  @todo Decide how to singal errors in bcat_get_item_create_query.
*/

#include <mysql_priv.h>
#include <sql_show.h>

#include <backup_stream.h>
#include <backup_kernel.h>
#include "stream_services.h"

/*****************************************************************

   Services for backup stream library related to meta-data
   manipulation.

 *****************************************************************/

extern "C"
int bcat_create_item(st_bstream_image_header *catalogue,
                     struct st_bstream_item_info *item,
                     bstream_blob create_stmt,
                     bstream_blob other_meta_data)
{
  using namespace backup;

  DBUG_ASSERT(catalogue);
  DBUG_ASSERT(item);

  Restore_info *info= static_cast<Restore_info*>(catalogue);

  Image_info::Item *it= info->locate_item(item);

  /*
    TODO: Decide what to do when we come across unknown item (locate_item()
    returns NULL): break the restore process as it is done now or continue
    with a warning?
  */

  if (!it)
    return BSTREAM_ERROR; // locate_item should report errors

  backup::String stmt(create_stmt.begin, create_stmt.end);

  DBUG_PRINT("restore",("Creating item of type %d pos %ld: %s",
                         item->type, item->pos, stmt.ptr()));

  result_t ret= info->restore_item(*it,stmt,
                                   other_meta_data.begin,
                                   other_meta_data.end);

  return ret == OK ? BSTREAM_OK : BSTREAM_ERROR;
}

extern "C"
int bcat_get_item_create_query(st_bstream_image_header *catalogue,
                               struct st_bstream_item_info *item,
                               bstream_blob *stmt)
{
  using namespace backup;

  DBUG_ASSERT(catalogue);
  DBUG_ASSERT(item);
  DBUG_ASSERT(stmt);

  Image_info *info= static_cast<Image_info*>(catalogue);

  Image_info::Item *it= info->locate_item(item);

  if (!it)
  {
    // TODO: warn that object was not found (?)
    return BSTREAM_ERROR;
  }

  result_t res= it->get_create_stmt(info->create_stmt_buf);

  if (res != OK)
    return BSTREAM_ERROR;

  stmt->begin= (backup::byte*)info->create_stmt_buf.ptr();
  stmt->end= stmt->begin + info->create_stmt_buf.length();

  return BSTREAM_OK;
}


extern "C"
int bcat_get_item_create_data(st_bstream_image_header *catalogue,
                            struct st_bstream_item_info *item,
                            bstream_blob *data)
{
  /* We don't use any extra data now */
  return BSTREAM_ERROR;
}

/*****************************************************************

   Implementation of meta::Item and derived classes.

 *****************************************************************/

namespace backup {

/**
  Destroy object if it exists.

  Default implementation executes SQL statement of the form:
  @verbatim
     DROP @<object type> IF EXISTS @<name>
  @endverbatim
  strings @<object type> and @<name> are returned by @c X::sql_object_name()
  and @c X::sql_name() methods of the class @c meta::X representing the item.
  If necessary, method @c drop() can be overwritten in a specialized class
  corresponding to a given type of meta-data item.

  @returns OK or ERROR
 */
result_t meta::Item::drop(THD *thd)
{
  const char *ob= sql_object_name();

  /*
    The caller should define object name for DROP statement
    or redefine drop() method.
   */
  DBUG_ASSERT(ob);

  String drop_stmt;

  drop_stmt.append("DROP ");
  drop_stmt.append(ob);
  drop_stmt.append(" IF EXISTS ");
  drop_stmt.append(sql_name());

  return silent_exec_query(thd,drop_stmt) ? ERROR : OK;
}


result_t meta::Db::get_create_stmt(::String &stmt)
{
  // TODO: get a full CREATE statement for a database
  return ERROR;
}

result_t meta::Db::create(THD *thd, ::String&, byte*, byte*)
{
  String stmt;

  // TODO: CREATE DATABASE statement should be taken from the backup image.

  stmt.append("CREATE DATABASE ");
  stmt.append(sql_name());

  return silent_exec_query(thd,stmt) ? ERROR : OK;
}

result_t meta::Table::get_create_stmt(::String &stmt)
{
  stmt.free();

  TABLE_LIST *tl= get_table_list_entry();

  // Table should be opened when this method is called
  DBUG_ASSERT(tl);

  int res= ::store_create_info(::current_thd,tl,&stmt,NULL);

  if (res)
    DBUG_PRINT("backup",("Can't get CREATE statement for table %s (error=%d)",
                         tl->alias,res));

  stmt.copy();

  /*
    TODO: returning error here will just make upper layer think that
    the CREATE statement is empty. Change the interface so that real error
    can be detected.
  */
  return res ? ERROR : OK;
}

} // backup namespace
