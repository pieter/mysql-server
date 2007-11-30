/**
  @file

  Implementation of the backup kernel API.

  @todo Do not overwrite existing backup locations.
  @todo Add more error messages.
  @todo Use internal table name representation when passing tables to
        backup/restore drivers.
  @todo Implement meta-data freeze during backup/restore.
  @todo Handle other types of meta-data in Backup_info methods.
  @todo Handle item dependencies when adding new items.
  @todo Lock I_S tables when reading table list and similar (is it needed?)
  @todo When reading table list from I_S tables, use select conditions to
        limit amount of data read. (check prepare_select_* functions in sql_help.cc)
  @todo Handle other kinds of backup locations (far future).
 */

#include "../mysql_priv.h"

#include "backup_aux.h"
#include "stream.h"
#include "backup_kernel.h"
#include "catalog.h"
#include "debug.h"
#include "be_native.h"
#include "be_default.h"
#include "be_snapshot.h"
#include "ddl_blocker.h"
#include "backup_progress.h"

namespace backup {

// Helper functions

static IStream* open_for_read(const Location&);
static OStream* open_for_write(const Location&);

/*
  Report errors. The main error code and optional arguments for its description
  are given plus a logger object which can contain stored errors.
 */
static int report_errors(THD*, Logger&, int, ...);

/*
  Check if info object is valid. If not, report error to client.
 */
static int check_info(THD*,Backup_info&);
static int check_info(THD*,Restore_info&);

static bool send_summary(THD*,const Backup_info&);
static bool send_summary(THD*,const Restore_info&);

#ifdef DBUG_BACKUP
// Flag used for testing error reporting
bool test_error_flag= FALSE;
#endif

/*
  (De)initialize memory allocator for backup stream library.
 */
void prepare_stream_memory();
void free_stream_memory();

}

extern pthread_mutex_t THR_LOCK_DDL_blocker;
extern pthread_cond_t COND_backup_blocked;
extern pthread_cond_t COND_DDL_blocker;

/**
  Call backup kernel API to execute backup related SQL statement.

  @param lex  results of parsing the statement.

  @note This function sends response to the client (ok, result set or error).
 */

int
execute_backup_command(THD *thd, LEX *lex)
{
  ulonglong backup_prog_id= 0;
  my_time_t start=0, stop=0;
  
  DBUG_ENTER("execute_backup_command");
  DBUG_ASSERT(thd && lex);

  BACKUP_BREAKPOINT("backup_command");

  using namespace backup;

  /*
    Check access for SUPER rights. If user does not have SUPER, fail with error.
  */
  if (check_global_access(thd, SUPER_ACL))
    DBUG_RETURN(ER_SPECIFIC_ACCESS_DENIED_ERROR);

  Location *loc= Location::find(lex->backup_dir);

  if (!loc)
  {
    my_error(ER_BACKUP_INVALID_LOC,MYF(0),lex->backup_dir.str);
    DBUG_RETURN(ER_BACKUP_INVALID_LOC);
  }

  prepare_stream_memory();
  int res= 0;

  switch (lex->sql_command) {

  case SQLCOM_SHOW_ARCHIVE:
  case SQLCOM_RESTORE:
  {
    IStream *stream= open_for_read(*loc);

    if (!stream)
    {
      my_error(ER_BACKUP_READ_LOC,MYF(0),loc->describe());
      goto restore_error;
    }
    else
    {
      if (lex->sql_command == SQLCOM_SHOW_ARCHIVE)
      {
        my_error(ER_NOT_ALLOWED_COMMAND,MYF(0));
        goto restore_error;
      }

      start= my_time(0);
      
      backup_prog_id= report_ob_init(thd->id, BUP_STARTING, OP_RESTORE, 
                                     0, "", lex->backup_dir.str, thd->query);
      report_ob_time(backup_prog_id, start, 0);
      BACKUP_BREAKPOINT("bp_starting_state");

      Restore_info info(thd,*stream);

      info.backup_prog_id= backup_prog_id;

      if (check_info(thd,info))
      {
        stop= my_time(0);
        goto restore_error;
      }

      info.report_error(log_level::INFO,ER_BACKUP_RESTORE_START);
      info.save_start_time(start);

      report_ob_state(backup_prog_id, BUP_RUNNING);
      BACKUP_BREAKPOINT("bp_running_state");

      /*
        Freeze all DDL operations by turning on DDL blocker.
      */
      if (!block_DDL(thd))
      {
        stop= my_time(0); 
        info.save_end_time(stop);
        goto restore_error;
      }

      info.save_errors();
      info.restore_all_dbs();

      if (check_info(thd,info))
      {
        stop= my_time(0);
        info.save_end_time(stop);
        goto restore_error;
      }
      
      info.clear_saved_errors();

      res= mysql_restore(thd,info,*stream);      
      stop= my_time(0);
      info.save_end_time(stop);

      if (res)
      {
        report_errors(thd,info,ER_BACKUP_RESTORE);
        goto restore_error;
      }

      report_ob_num_objects(backup_prog_id, info.table_count);
      report_ob_size(backup_prog_id, info.data_size);
      report_ob_time(backup_prog_id, 0, stop);
      report_ob_state(backup_prog_id, BUP_COMPLETE);
      BACKUP_BREAKPOINT("bp_complete_state");

      info.report_error(log_level::INFO,ER_BACKUP_RESTORE_DONE);
      send_summary(thd,info);
    } // if (!stream)

    goto finish_restore;

   restore_error:

    res= res ? res : ERROR;

    report_ob_error(backup_prog_id, res);
    
    if (stop)
      report_ob_time(backup_prog_id, 0, stop);

    report_ob_state(backup_prog_id, BUP_ERRORS);
    BACKUP_BREAKPOINT("bp_error_state");
   
   finish_restore:

    /*
      Unfreeze all DDL operations by turning off DDL blocker.
    */
    unblock_DDL();
    BACKUP_BREAKPOINT("DDL_unblocked");
    
    if (stream)
      stream->close();

    break;
  }

  case SQLCOM_BACKUP:
  {
    OStream *stream= open_for_write(*loc);

    // TODO: report error if location exists

    if (!stream)
    {
      my_error(ER_BACKUP_WRITE_LOC,MYF(0),loc->describe());
      goto backup_error;
    }
    else
    {
      start= my_time(0);
      
      /*
        Freeze all DDL operations by turning on DDL blocker.

        Note: The block_ddl() call must occur before the information_schema
              is read so that any new tables (e.g. CREATE in progress) can
              be counted. Waiting until after this step caused backup to
              skip new or dropped tables.
      */
      if (!block_DDL(thd))
        goto backup_error;

      Backup_info info(thd);

      backup_prog_id= report_ob_init(thd->id, BUP_STARTING, OP_BACKUP,
                                     0, "", lex->backup_dir.str, thd->query);
      report_ob_time(backup_prog_id, start, 0);
      BACKUP_BREAKPOINT("bp_starting_state");

      info.backup_prog_id= backup_prog_id;

      if (check_info(thd,info))
        goto backup_error;

      info.report_error(log_level::INFO,ER_BACKUP_BACKUP_START);
      info.save_start_time(start);
      report_ob_state(backup_prog_id, BUP_RUNNING);
      BACKUP_BREAKPOINT("bp_running_state");

      info.save_errors();

      if (lex->db_list.is_empty())
      {
        info.write_message(log_level::INFO,"Backing up all databases");
        info.add_all_dbs(); // backup all databases
      }
      else
      {
        info.write_message(log_level::INFO,"Backing up selected databases");
        info.add_dbs(lex->db_list); // backup databases specified by user
      }

      report_ob_num_objects(backup_prog_id, info.table_count);

      if (check_info(thd,info))
      {
        stop= my_time(0); 
        info.save_end_time(stop);
        goto backup_error;
      }

      info.close();

      if (check_info(thd,info))
      {
        stop= my_time(0); 
        info.save_end_time(stop);
        goto backup_error;
      }

      info.clear_saved_errors();

      res= mysql_backup(thd,info,*stream);
      stop= my_time(0);
      info.save_end_time(stop);
 
      if (res)
      {
        report_errors(thd,info,ER_BACKUP_BACKUP);
        goto backup_error;
      }

      report_ob_size(info.backup_prog_id, info.data_size);
      report_ob_time(info.backup_prog_id, 0, stop);
      report_ob_state(info.backup_prog_id, BUP_COMPLETE);
      BACKUP_BREAKPOINT("bp_complete_state");

      info.report_error(log_level::INFO,ER_BACKUP_BACKUP_DONE);
      send_summary(thd,info);

    } // if (!stream)

    goto finish_backup;

   backup_error:

    res= res ? res : ERROR;

    report_ob_error(backup_prog_id, res);
    report_ob_state(backup_prog_id, BUP_ERRORS);

    if (stop)
      report_ob_time(backup_prog_id, 0, stop);

    BACKUP_BREAKPOINT("bp_error_state");

   finish_backup:

    /*
      Unfreeze all DDL operations by turning off DDL blocker.
    */
    unblock_DDL();
    BACKUP_BREAKPOINT("DDL_unblocked");

    if (stream)
      stream->close();

    break;
  }

  default:
     /*
       execute_backup_command() should be called with correct command id
       from the parser. If not, we fail on this assertion.
      */
     DBUG_ASSERT(FALSE);

  } // switch(lex->sql_command)

  loc->free();
  free_stream_memory();

  DBUG_RETURN(res);
}


/*************************************************

                   BACKUP

 *************************************************/

// Declarations for functions used in backup operation

namespace backup {

// defined in data_backup.cc
int write_table_data(THD*, Backup_info&, OStream&);

} // backup namespace


/**
  Create backup archive.
*/

int mysql_backup(THD *thd,
                 backup::Backup_info &info,
                 backup::OStream &s)
{
  DBUG_ENTER("mysql_backup");

  using namespace backup;

  // This function should not be called with invalid backup info.
  DBUG_ASSERT(info.is_valid());

  BACKUP_BREAKPOINT("backup_meta");

  DBUG_PRINT("backup",("Writing preamble"));

  if (write_preamble(info,s))
    goto error;

  DBUG_PRINT("backup",("Writing table data"));

  BACKUP_BREAKPOINT("backup_data");

  if (write_table_data(thd,info,s))
    goto error;

  DBUG_PRINT("backup",("Writing summary"));

  if (write_summary(info,s))
    goto error;

  DBUG_PRINT("backup",("Backup done."));
  BACKUP_BREAKPOINT("backup_done");

  DBUG_RETURN(0);

 error:

  unblock_DDL();
  DBUG_RETURN(ERROR);
}

namespace backup {


/**
  Find backup engine which can backup data of a given table.

  When necessary, a @c Snapshot_info object is created and added to the
  @c m_snap[] table.

  @param t    pointer to table's opened TABLE structure
  @param tbl  Table_ref describing the table

  @return position in @c m_snap[] of the @c Snapshot_info object to which the
  table has been added or -1 on error.

  @todo Add error messages.
 */
int Backup_info::find_backup_engine(const ::TABLE *const t,
                                    const Table_ref &tbl)
{
   handlerton *hton= t->s->db_type();
   Table_ref::describe_buf buf;
   int no;

   DBUG_ENTER("Backup_info::find_backup_engine");
   DBUG_ASSERT(t);

   DBUG_PRINT("backup",("Locating backup engine for table %s which uses"
                        " storage engine %s%s",
                        tbl.describe(buf),
                        hton ? ::ha_resolve_storage_engine_name(hton) : "(unknown engine)",
                        hton->get_backup_engine ? " (has native backup)." : "."));

   /*
     Note: at backup time the native and CS snapshot info objects are always
     located at m_snap[0] and m_snap[1], respectively. They are created in
     the Backup_info constructor.
   */

  // try native driver if table has native backup engine

  if (hton->get_backup_engine)
  {
    // see if the snapshot exists already (skip default ones)
    for (no=2; no < 256 && m_snap[no] ; ++no)
     if (m_snap[no]->accept(tbl,hton))
       DBUG_RETURN(no);

    if (no == 256)
    {
      // TODO: report error
      DBUG_RETURN(-1);
    }

    // We need to create native snapshot for this table

    m_snap[no]= new Native_snapshot(t->s->db_plugin);

    if (!m_snap[no])
    {
      // TODO: report error
      DBUG_RETURN(-1);
    }

    if (!m_snap[no]->accept(tbl,hton))
    {
      // TODO: report error
      DBUG_RETURN(-1);
    }

    DBUG_RETURN(no);
  }

  /*
    Try default drivers in decreasing order of preferrence
    (first snapshot, then default)
  */
  for (no=1; no >=0; --no)
  {
    if (!m_snap[no]->accept(tbl,hton))
     continue;

    DBUG_RETURN(no);
  }

  report_error(ER_BACKUP_NO_BACKUP_DRIVER,tbl.describe(buf,sizeof(buf)));
  DBUG_RETURN(-1);
}

} // backup namespace


/****************************

  Backup_info implementation

 ****************************/

namespace backup {

//  Returns tmp table containing records from a given I_S table
TABLE* get_schema_table(THD *thd, ST_SCHEMA_TABLE *st);


/**
  Create @c Backup_info structure and prepare it for populating with meta-data
  items.

  When adding a complete database to the archive, all its tables are added.
  These are found by reading INFORMATION_SCHEMA.TABLES table. The table is
  opened here so that it is ready for use in @c add_db_items() method. It is
  closed when the structure is closed with the @c close() method.

  @todo Report errors.
 */
Backup_info::Backup_info(THD *thd):
  Logger(Logger::BACKUP),
  m_state(INIT),
  m_thd(thd), i_s_tables(NULL)
{
  i_s_tables= get_schema_table(m_thd, ::get_schema_table(SCH_TABLES));
  if (!i_s_tables)
  {
    report_error(ER_BACKUP_LIST_TABLES);
    m_state= ERROR;
  }

  // create default and CS snapshot objects

  m_snap[0]= new Default_snapshot();
  if (!m_snap[0])
  {
    // TODO: report error
    close();
    m_state= ERROR;
  }

  m_snap[1]= new CS_snapshot();
  if (!m_snap[1])
  {
    // TODO: report error
    close();
    m_state= ERROR;
  }
}

Backup_info::~Backup_info()
{
  close();
  m_state= DONE;
  name_strings.delete_elements();
  // Note: snapshot objects are deleted in ~Image_info()
}

/**
  Store information about table data snapshot inside @c st_bstream_snapshot_info
  structure.
*/
void save_snapshot_info(const Snapshot_info &snap, st_bstream_snapshot_info &info)
{
  bzero(&info,sizeof(st_bstream_snapshot_info));
  info.type= enum_bstream_snapshot_type(snap.type());
  info.version= snap.version;
  info.table_count= snap.table_count();

  if (snap.type() == Snapshot_info::NATIVE_SNAPSHOT)
  {
    const Native_snapshot &nsnap= static_cast<const Native_snapshot&>(snap);

    info.engine.major= nsnap.se_ver >> 8;
    info.engine.minor= nsnap.se_ver & 0xFF;
    info.engine.name.begin= (byte*)nsnap.m_name;
    info.engine.name.end= info.engine.name.begin + strlen(nsnap.m_name);
  }
}

/**
  Close @c Backup_info object after populating it with items.

  After this call the @c Backup_info object is ready for use as a catalogue
  for backup stream functions such as @c bstream_wr_preamble().
 */
bool Backup_info::close()
{
  bool ok= is_valid();

  if(i_s_tables)
    ::free_tmp_table(m_thd,i_s_tables);
  i_s_tables= NULL;

  /*
    Go through snapshots and save their descriptions inside snapshots[] table.
  */
  for (uint no=0; no < 256; ++no)
  {
    Snapshot_info *snap= m_snap[no];

    if (!snap)
      continue;

    if (snap->m_no == 0 || snap->table_count() == 0)
    {
      DBUG_ASSERT(snap->m_no == 0);
      DBUG_ASSERT(snap->table_count() == 0);
      delete snap;
      m_snap[no]= NULL;
      continue;
    }

    save_snapshot_info(*snap,snapshot[snap->m_no-1]);
  }

  if (m_state == INIT)
    m_state= READY;

  return ok;
}


/**
  Add to backup image all databases in the list.

  For each database, all objects stored in that database are also added to
  the image.

  @todo Report errors.
 */
int Backup_info::add_dbs(List< ::LEX_STRING > &dbs)
{
  List_iterator< ::LEX_STRING > it(dbs);
  ::LEX_STRING *s;
  String unknown_dbs; // comma separated list of databases which don't exist

  while ((s= it++))
  {
    String db_name(*s);
    Db_ref db(db_name);

    if (db_exists(db))
    {
      if (!unknown_dbs.is_empty()) // we just compose unknown_dbs list
        continue;

      Db_item *it= add_db(db);

      if (!it)
      {
        // TODO: report error
        goto error;
      }

      if (add_db_items(*it))
        goto error;
    }
    else
    {
      if (!unknown_dbs.is_empty())
        unknown_dbs.append(",");
      unknown_dbs.append(db.name());
    }
  }

  if (!unknown_dbs.is_empty())
  {
    report_error(ER_BAD_DB_ERROR,unknown_dbs.c_ptr());
    goto error;
  }

  return 0;

 error:

  m_state= ERROR;
  return backup::ERROR;
}

/**
  Add all databases to backup image (except the internal ones).

  For each database, all objects stored in that database are also added to
  the image.

  @todo Report errors.
*/
int Backup_info::add_all_dbs()
{
  my_bitmap_map *old_map;

  DBUG_PRINT("backup", ("Reading databases from I_S"));

  ::TABLE *db_table = get_schema_table(m_thd, ::get_schema_table(SCH_SCHEMATA));

  if (!db_table)
  {
    report_error(ER_BACKUP_LIST_DBS);
    return ERROR;
  }

  ::handler *ha = db_table->file;

  // TODO: locking

  int res= 0;

  old_map= dbug_tmp_use_all_columns(db_table, db_table->read_set);

  if (ha->ha_rnd_init(TRUE))
  {
    res= -2;
    report_error(ER_BACKUP_LIST_DBS);
    goto finish;
  }

  while (!ha->rnd_next(db_table->record[0]))
  {
    String *db_name= new (m_thd->mem_root) String();

    if (!db_name)
    {
      report_error(ER_OUT_OF_RESOURCES);
      res= ERROR;
      goto finish;
    }

    db_table->field[1]->val_str(db_name);

    // skip internal databases
    if (*db_name == String("information_schema")
        || *db_name == String("mysql"))
    {
      DBUG_PRINT("backup",(" Skipping internal database %s",db_name->ptr()));
      delete db_name;
      continue;
    }

    db_name->copy();
    Db_ref db(*db_name);

    DBUG_PRINT("backup", (" Found database %s", db.name().ptr()));

    Db_item *it= add_db(db);

    if (!it)
    {
      res= -3;
      delete db_name;
      goto finish;
    }

    /*
      Add name to name_strings list so that it is freed in the destructor.
    */
    name_strings.push_back(db_name);

    if (add_db_items(*it))
    {
      res= -4;
      goto finish;
    }
  }

  DBUG_PRINT("backup", ("No more databases in I_S"));

  ha->ha_rnd_end();

 finish:

  if (db_table)
  {
    dbug_tmp_restore_column_map(db_table->read_set, old_map);
    ::free_tmp_table(m_thd, db_table);
  }

  if (res)
    m_state= ERROR;

  return res;
}


/**
  Add to archive all objects belonging to a given database.

  @todo Handle other types of objects - not only tables.
  @todo Use WHERE clauses when reading I_S.TABLES
 */
int Backup_info::add_db_items(Db_item &dbi)
{
  my_bitmap_map *old_map;

  DBUG_ASSERT(m_state == INIT);  // should be called before structure is closed
  DBUG_ASSERT(is_valid());       // should be valid
  DBUG_ASSERT(i_s_tables->file); // i_s_tables should be opened

  ::handler *ha= i_s_tables->file;

  /*
    If error debugging is switched on (see debug.h) then I_S.TABLES access
    error will be triggered when backing up database whose name starts with 'a'.
   */
  TEST_ERROR_IF(dbi.name().ptr()[0]=='a');

  old_map= dbug_tmp_use_all_columns(i_s_tables, i_s_tables->read_set);

  if (ha->ha_rnd_init(TRUE) || TEST_ERROR)
  {
    dbug_tmp_restore_column_map(i_s_tables->read_set, old_map);
    report_error(ER_BACKUP_LIST_DB_TABLES,dbi.name().ptr());
    return ERROR;
  }

  int res= 0;

  /*
   TODO: Instead of looping through all records in the I_S.TABLES table
   we could execute an SQL query with appropriate WHERE clause.
  */
  while (!ha->rnd_next(i_s_tables->record[0]))
  {
    String db_name;
    String type;
    String engine;

    String *name= new (m_thd->mem_root) String();

    /*
      Read info about next table/view

      Note: this should be synchronized with the definition of
      INFORMATION_SCHEMA.TABLES table.
     */
    i_s_tables->field[1]->val_str(&db_name);
    i_s_tables->field[2]->val_str(name);
    i_s_tables->field[3]->val_str(&type);
    i_s_tables->field[4]->val_str(&engine);

    // skip tables not from the given database
    if (db_name != dbi.name())
    {
      delete name;
      continue;
    }

    // FIXME: right now, we handle only tables
    if (type != String("BASE TABLE"))
    {
      report_error(log_level::WARNING,ER_BACKUP_SKIP_VIEW,
                   name->c_ptr(),db_name.c_ptr());
      delete name;
      continue;
    }

    name->copy();
    Backup_info::Table_ref t(dbi,*name);

    if (engine.is_empty())
    {
      Table_ref::describe_buf buf;
      report_error(log_level::WARNING,ER_BACKUP_NO_ENGINE,t.describe(buf));
      delete name;
      continue;
    }

    DBUG_PRINT("backup", ("Found table %s for database %s",
                           t.name().ptr(), t.db().name().ptr()));

    /*
      add_table() method selects/creates a snapshot to which this table is added.
      The backup engine is chooden in Backup_info::find_backup_engine() method.
    */
    Table_item *ti= add_table(dbi,t);

    if (!ti)
    {
      delete name;
      goto error;
    }

    /*
      We store pointers to created String objects so that they all will
      be deleted in ~Backup_info().
    */
    name_strings.push_back(name);

    if (add_table_items(*ti))
      goto error;
  }

  goto finish;

 error:

  res= res ? res : ERROR;

 finish:

  ha->ha_rnd_end();

  dbug_tmp_restore_column_map(i_s_tables->read_set, old_map);

  return res;
}


/**
  Add table to archive's list of meta-data items.

  @todo Correctly handle temporary tables.
  @todo Avoid opening tables here - open them only in bcat_get_create_stmt().
*/
Image_info::Table_item*
Backup_info::add_table(Db_item &dbi, const Table_ref &t)
{
  Table_ref::describe_buf buf;
  // TODO: skip table if it is a tmp one

  Table_item *ti= NULL;

  /*
    open table temporarily to:
     - get its handlerton
     - get a CREATE statement for it
  */

  TABLE_LIST entry, *tl= &entry;
  bzero(&entry,sizeof(entry));

  // FIXME: table/db name mangling
  entry.db= const_cast<char*>(t.db().name().ptr());
  entry.alias= entry.table_name= const_cast<char*>(t.name().ptr());

  uint cnt;
  int res= ::open_tables(m_thd,&tl,&cnt,0);

  if (res || !tl->table)
  {
    report_error(ER_BACKUP_TABLE_OPEN,t.describe(buf));
    return NULL;
  }

  /*
    alternative way of opening a single tmp table - but it
    doesn't initialize TABLE_LIST structure which we need for getting
    CREATE statement.

    char path[FN_REFLEN];
    const char *db= t.db().name().ptr();
    const char *name= t.name().ptr();

    ::build_table_filename(path, sizeof(path), db, name, "", 0);

    ::TABLE *table= ::open_temporary_table(m_thd, path, db, name,
                      FALSE /=* don't link to thd->temporary_tables *=/);

    ...

    ::intern_close_table(table);
    my_free(table, MYF(0));
  */

  int no= find_backup_engine(tl->table,t); // Note: reports errors

  DBUG_PRINT("backup",(" table %s backed-up with %s engine",
                       t.describe(buf),
                       m_snap[no]->name()));

  /*
    If error debugging is switched on (see debug.h) then any table whose
    name starts with 'a' will trigger "no backup driver" error.
   */
  TEST_ERROR_IF(t.name().ptr()[0]=='a');

  if (no < 0 || TEST_ERROR)
    goto end;

  // add table to the catalogue

  ti= Image_info::add_table(dbi,t,no);

  /*
    If error debugging is switched on (see debug.h) then any table whose
    name starts with 'b' will trigger error when added to backup image.
   */
  TEST_ERROR_IF(t.name().ptr()[0]=='b');

  if (!ti || TEST_ERROR)
  {
    report_error(ER_OUT_OF_RESOURCES);
    goto end;
  }

  /*
    Note: we obtain and store CREATE statement here because that requires
    the table to be opened. Otherwise we could do that when meta-data is
    written.
    We could still do this (and save memory used to store these statements)
    at an expense of opening table again during meta-data write phase.

    TODO: don't open table here, but locate its storage engine based on its
    name (which is read from I_S.TABLES table). Then CREATE statement could
    be constructed inside meta::Table::get_create_stmt() method.
   */

  ti->tl_entry= tl;
  ti->get_create_stmt();
  ti->tl_entry= NULL;

 end:

  ::close_thread_tables(m_thd);

  return ti;
}

/**
  Add to archive all items belonging to a given table.

  @todo Implement this.
*/
int Backup_info::add_table_items(Table_item&)
{
  // TODO: Implement when we handle per-table meta-data.
  return 0;
}

} // backup namespace


/*************************************************

                   RESTORE

 *************************************************/

// Declarations of functions used in restore operation

namespace backup {

// defined in data_backup.cc
int restore_table_data(THD*, Restore_info&, IStream&);

} // backup namespace

/**
   Toggle foreign key constraints on and off.

   @param THD thd          Current thread structure.
   @param my_bool turn_on  TRUE = turn on, FALSE = turn off.

   @returns TRUE if foreign key contraints are turned on already
   @returns FALSE if foreign key contraints are turned off
  */
my_bool fkey_constr(THD *thd, my_bool turn_on)
{
  my_bool fk_status= FALSE;

  DBUG_ENTER("mysql_restore");
  if (turn_on)
    thd->options&= ~OPTION_NO_FOREIGN_KEY_CHECKS;
  else
  {
    fk_status= (thd->options & OPTION_NO_FOREIGN_KEY_CHECKS)? FALSE : TRUE;
    thd->options|= OPTION_NO_FOREIGN_KEY_CHECKS;
  }
  DBUG_RETURN(fk_status);
}

/**
  Restore objects saved in backup image.

  @pre The header and catalogue of backup image has been already read with
  @c bstream_rd_header() function and stored inside the @c info object.
*/
int mysql_restore(THD *thd, backup::Restore_info &info, backup::IStream &s)
{
  my_bool using_fkey_constr= FALSE;

  DBUG_ENTER("mysql_restore");

  using namespace backup;

  s.next_chunk();

  DBUG_PRINT("restore",("Restoring meta-data"));

  /*
    Turn off foreign key constraints (if turned on)
  */
  using_fkey_constr= fkey_constr(thd, FALSE);

  if (read_meta_data(info, s) == ERROR)
  {
    fkey_constr(thd, using_fkey_constr);  
    DBUG_RETURN(ERROR);
  }

  s.next_chunk();

  DBUG_PRINT("restore",("Restoring table data"));

  // Here restore drivers are created to restore table data
  if (restore_table_data(thd,info,s) == ERROR)
  {
    fkey_constr(thd, using_fkey_constr);
    DBUG_RETURN(ERROR);
  }

  /*
    Turn on foreign key constraints (if previously turned on)
  */
  fkey_constr(thd, using_fkey_constr);

  DBUG_PRINT("restore",("Done."));

  if (read_summary(info,s) == ERROR)
    DBUG_RETURN(ERROR);

  DBUG_RETURN(0);
}

/****************************

  Restore_info implementation

 ****************************/

namespace backup {

/**
  Initialize @c Restore_info instance and load the catalogue from
  the given backup stream.
*/
Restore_info::Restore_info(THD *thd, IStream &s):
  Logger(Logger::RESTORE), m_valid(TRUE), m_thd(thd), curr_db(NULL),
  system_charset(NULL), same_sys_charset(TRUE)
{
  int ret= BSTREAM_OK;

  ret= read_header(*this,s);

  if (!(m_valid= (ret != BSTREAM_ERROR)))
    return;

  ret= s.next_chunk();

  if (!(m_valid= (ret == BSTREAM_OK)))
    return;

  ret= read_catalog(*this,s);
  m_valid= (ret != BSTREAM_ERROR);
}

Restore_info::~Restore_info()
{}

/**
  Restore an object given the metadata read from backup stream.

  @param it    @c Item object describing the object to be restored,
  @param stmt  that object's CREATE statement stored in the backup image
               (can be empty),
  @param begin first byte of extra metadata stored in the image,
  @param end   one after the last byte of the extra metadat.

  If no extra metadata was stored for that object, @c begin and @c end are NULL.

  @note The actual job is done by @c Item::create()  and @c Item::drop()
  methods, here we only create and maintain correct context for these methods
  to work.
*/
result_t Restore_info::restore_item(Item &it, ::String &stmt,
                                    byte *begin, byte *end)
{
  /*
    change the current database if we are going to create a per-db item
    and we are not already in the correct one.
  */
  const Db_ref *db= it.in_db();

  if (db && (!curr_db || *db != *curr_db))
  {
    DBUG_PRINT("restore",("  changing current db to %s",db->name().ptr()));
    curr_db= db;
    change_db(m_thd,*curr_db);
  }

  /*
    note: This is destructive restore, so we drop each object before
    creating it.
  */

  result_t ret= it.drop(::current_thd);

  if (ret != OK)
    return ret;

  ret= it.create(m_thd, stmt, begin, end);

  return ret;
}

} // backup namespace

/*************************************************

               CATALOGUE SERVICES

 *************************************************/

/**
  Prepare @c Restore_info object for populating the catalogue with items to
  restore.

  At this point we know the list of table data snapshots present in the image
  (it was read from image's header). Here we create @c Snapshot_info object
  for each of them.

  @todo Report errors.
*/
extern "C"
int bcat_reset(st_bstream_image_header *catalogue)
{
  using namespace backup;
  uint no;

  Restore_info *info= static_cast<Restore_info*>(catalogue);

  for (no=0; no < info->snap_count; ++no)
  {
    st_bstream_snapshot_info *snap= &info->snapshot[no];

    DBUG_PRINT("restore",("Creating info for snapshot no %d",no));

    switch (snap->type) {

    case BI_NATIVE:
    {
      backup::LEX_STRING name_lex(snap->engine.name.begin, snap->engine.name.end);
      plugin_ref se= ::ha_resolve_by_name(::current_thd,&name_lex);
      handlerton *hton= plugin_data(se,handlerton*);

      if (!hton)
      {
        // TODO: report error
        return BSTREAM_ERROR;
      }

      if (!hton->get_backup_engine)
      {
        // TODO: report error
        return BSTREAM_ERROR;
      }

      info->m_snap[no]= new Native_snapshot(se);

      break;
    }

    case BI_CS:
      info->m_snap[no]= new CS_snapshot();
      break;

    case BI_DEFAULT:
      info->m_snap[no]= new Default_snapshot();
      break;

    default:
      DBUG_PRINT("restore",("Unknown snapshot type %d",
                            info->snapshot[no].type));
      return BSTREAM_ERROR;
    }

    if (!info->m_snap[no])
    {
      // TODO: report error
      return BSTREAM_ERROR;
    }

    if (info->m_snap[no]->set_version(snap->version) != OK)
    {
      // TODO: report error
      return BSTREAM_ERROR;
    }

    info->m_snap[no]->m_no= no+1;

    DBUG_PRINT("restore",(" snapshot uses %s engine",info->m_snap[no]->name()));
  }

  return BSTREAM_OK;
}

/**
  Called after reading backup image's catalogue and before processing
  metadata and table data.

  Nothing to do here.
*/
extern "C"
int bcat_close(st_bstream_image_header *catalogue)
{ return BSTREAM_OK; }

/**
  Add item to restore catalogue.

  @todo Report errors.
*/
extern "C"
int bcat_add_item(st_bstream_image_header *catalogue, struct st_bstream_item_info *item)
{
  using namespace backup;

  Restore_info *info= static_cast<Restore_info*>(catalogue);

  backup::String name_str(item->name.begin, item->name.end);

  DBUG_PRINT("restore",("Adding item %s of type %d (pos=%ld)",
                        item->name.begin,
                        item->type,
                        item->pos));

  switch (item->type) {

  case BSTREAM_IT_DB:
  {
    Image_info::Db_ref db(name_str);

    Image_info::Db_item *dbi= info->add_db(db,item->pos);

    if (!dbi)
    {
      // TODO: report error
      return BSTREAM_ERROR;
    }

    return BSTREAM_OK;
  }

  case BSTREAM_IT_TABLE:
  {
    st_bstream_table_info *it= (st_bstream_table_info*)item;

    DBUG_PRINT("restore",(" table's snapshot no is %d",it->snap_no));

    Image_info::Db_item *db= info->get_db(it->base.db->base.pos);

    if (!db)
    {
      // TODO: report error
      return BSTREAM_ERROR;
    }

    DBUG_PRINT("restore",(" table's database is %s",db->name().ptr()));

    Image_info::Table_ref t(*db,name_str);

    Image_info::Table_item *ti= info->add_table(*db,t,it->snap_no,
                                                      item->pos);
    if (!ti)
    {
      // TODO: report error
      return BSTREAM_ERROR;
    }

    return BSTREAM_OK;
  }

  default:
    return BSTREAM_OK;

  } // switch (item->type)
}

/*************************************************

    MEMORY ALLOCATOR FOR BACKUP STREAM LIBRARY

 *************************************************/

namespace backup {

/**
  This calss provides memory allocation services for backup stream library.

  An instance of this class must be created and pointer to it stored in the
  static @c instance variable during BACKUP/RESTORE operation. This assumes
  only one BACKUP/RESTORE operation is running at a time.
*/
class Mem_allocator
{
 public:

  Mem_allocator();
  ~Mem_allocator();

  void* alloc(size_t);
  void  free(void*);

  static Mem_allocator *instance;

 private:

  /// All allocated memory segments are linked into a list using this structure.
  struct node
  {
    node *prev;
    node *next;
  };

  node *first;  ///< Pointer to the first segment in the list.
};


Mem_allocator::Mem_allocator(): first(NULL)
{}

/// Deletes all allocated segments which have not been freed.
Mem_allocator::~Mem_allocator()
{
  node *n= first;

  while (n)
  {
    first= n->next;
    my_free(n,MYF(0));
    n= first;
  }
}

/**
  Allocate memory segment of given size.

  Extra memory is allocated for a @c node structure which holds pointers
  to previous and next segment in the segments list. This is used when
  deallocating allocated memory in the destructor.
*/
void* Mem_allocator::alloc(size_t howmuch)
{
  void *ptr= my_malloc(sizeof(node)+howmuch, MYF(0));

  if (!ptr)
    return NULL;

  node *n= (node*)ptr;
  ptr= n + 1;

  n->prev= NULL;
  n->next= first;
  if (first)
    first->prev= n;
  first= n;

  return ptr;
}

/**
  Explicit deallocation of previously allocated segment.

  The @c ptr should contain an address which was obtained from
  @c Mem_allocator::alloc().

  The deallocated fragment is removed from the allocated fragments list.
*/
void Mem_allocator::free(void *ptr)
{
  if (!ptr)
    return;

  node *n= ((node*)ptr) - 1;

  if (first == n)
    first= n->next;

  if (n->prev)
    n->prev->next= n->next;

  if (n->next)
    n->next->prev= n->prev;

  my_free(n,MYF(0));
}

Mem_allocator *Mem_allocator::instance= NULL;

/**
  This function must be called before @c bstream_alloc() can be used.
*/
void prepare_stream_memory()
{
  if (Mem_allocator::instance)
    delete Mem_allocator::instance;

  Mem_allocator::instance= new Mem_allocator();
}

/**
  This function should be called when @c bstream_alloc()/ @c bstream_free()
  are no longer to be used.

  It destroys the Mem_allocator instance which frees all memory which was
  allocated but not explicitly freed.
*/
void free_stream_memory()
{
  delete Mem_allocator::instance;
  Mem_allocator::instance= NULL;
}

}

extern "C" {

/**
  Memory allocator for backup stream library.

  @pre @c prepare_stream_memory() has been called (i.e., the Mem_allocator
  instance is created.
 */
bstream_byte* bstream_alloc(unsigned long int size)
{
  using namespace backup;

  DBUG_ASSERT(Mem_allocator::instance);

  return (bstream_byte*)Mem_allocator::instance->alloc(size);
}

/**
  Memory deallocator for backup stream library.
*/
void bstream_free(bstream_byte *ptr)
{
  using namespace backup;

  if (Mem_allocator::instance)
    Mem_allocator::instance->free(ptr);
}

}

/*************************************************

               BACKUP LOCATIONS

 *************************************************/

namespace backup {

/**
  Specialization of @c Location class representing a file in the local
  filesystem.
*/
struct File_loc: public Location
{
  ::String path;

  enum_type type() const
  { return SERVER_FILE; }

  File_loc(const char *p)
  { path.append(p); }

  const char* describe()
  { return path.c_ptr(); }

};


template<class S>
S* open_stream(const Location &loc)
{
  switch (loc.type()) {

  case Location::SERVER_FILE:
  {
    const File_loc &f= static_cast<const File_loc&>(loc);
    S *s= new S(f.path);

    if (s && s->open())
      return s;

    delete s;
    return NULL;
  }

  default:
    return NULL;

  }
}

template IStream* open_stream<IStream>(const Location&);
template OStream* open_stream<OStream>(const Location&);

IStream* open_for_read(const Location &loc)
{
  return open_stream<IStream>(loc);
}

OStream* open_for_write(const Location &loc)
{
  return open_stream<OStream>(loc);
}

/**
  Find location described by a string.

  The string is taken from the "TO ..." clause of BACKUP/RESTORE commands.
  This function parses the string and creates instance of @c Location class
  describing the location or NULL if string doesn't describe any valid location.

  Currently the only supported type of location is a file on the server host.
  The string is supposed to contain a path to such file.

  @note No checks on the location are made at this stage. In particular the
  location might not physically exist. In the future methods performing such
  checks can be added to @Location class.
 */
Location*
Location::find(const LEX_STRING &where)
{
  return where.str && where.length ? new File_loc(where.str) : NULL;
}

} // backup namespace


/*************************************************

                 Helper functions

 *************************************************/

TABLE *create_schema_table(THD *thd, TABLE_LIST *table_list); // defined in sql_show.cc

namespace backup {

/**
  Report errors.

  Current implementation reports the last error saved in the logger if it exist.
  Otherwise it reports error given by @c error_code.
 */
int report_errors(THD *thd, Logger &log, int error_code, ...)
{
  MYSQL_ERROR *error= log.last_saved_error();

  if (error && !util::report_mysql_error(thd,error,error_code))
  {
    if (error->code)
      error_code= error->code;
  }
  else // there are no error information in the logger - report error_code
  {
    char buf[ERRMSGSIZE + 20];
    va_list args;
    va_start(args,error_code);

    my_vsnprintf(buf,sizeof(buf),ER_SAFE(error_code),args);
    my_printf_error(error_code,buf,MYF(0));

    va_end(args);
  }

  return error_code;
}

inline
int check_info(THD *thd, Backup_info &info)
{
  return info.is_valid() ? OK : report_errors(thd,info,ER_BACKUP_BACKUP_PREPARE);
}

inline
int check_info(THD *thd, Restore_info &info)
{
  return info.is_valid() ? OK : report_errors(thd,info,ER_BACKUP_RESTORE_PREPARE);
}

/**
  Send a summary of the backup/restore operation to the client.

  The data about the operation is taken from filled @c Archive_info
  structure. Parameter @c backup determines if this was backup or
  restore operation.
*/
static
bool send_summary(THD *thd, const Image_info &info, bool backup)
{
  Protocol *protocol= thd->protocol;    // client comms
  List<Item> field_list;                // list of fields to send
  String     op_str("backup_id");       // operations string
  int ret= 0;                           // return value
  char buf[255];                        // buffer for summary information
  String str;

  DBUG_ENTER("backup::send_summary");

  DBUG_PRINT(backup?"backup":"restore", ("sending summary"));

  /*
    Send field list.
  */
  field_list.push_back(new Item_empty_string(op_str.c_ptr(), op_str.length()));
  protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);

  /*
    Send field data.
  */
  protocol->prepare_for_resend();
  llstr(info.backup_prog_id,buf);
  protocol->store(buf, system_charset_info);
  protocol->write();

  send_eof(thd);
  DBUG_RETURN(ret);
}

inline
bool send_summary(THD *thd, const Backup_info &info)
{ return send_summary(thd,info,TRUE); }

inline
bool send_summary(THD *thd, const Restore_info &info)
{ return send_summary(thd,info,FALSE); }


/// Open given table in @c INFORMATION_SCHEMA database.
TABLE* get_schema_table(THD *thd, ST_SCHEMA_TABLE *st)
{
  TABLE *t;
  TABLE_LIST arg;
  my_bitmap_map *old_map;

  bzero( &arg, sizeof(TABLE_LIST) );

  // set context for create_schema_table call
  arg.schema_table= st;
  arg.alias=        NULL;
  arg.select_lex=   NULL;

  t= ::create_schema_table(thd,&arg); // Note: callers must free t.

  if( !t ) return NULL; // error!

  /*
   Temporarily set thd->lex->wild to NULL to keep st->fill_table
   happy.
  */
  ::String *wild= thd->lex->wild;
  ::enum_sql_command command= thd->lex->sql_command;

  thd->lex->wild = NULL;
  thd->lex->sql_command = enum_sql_command(0);

  // context for fill_table
  arg.table= t;

  old_map= tmp_use_all_columns(t, t->read_set);

  /*
    Question: is it correct to fill I_S table each time we use it or should it
    be filled only once?
   */
  st->fill_table(thd,&arg,NULL);  // NULL = no select condition

  tmp_restore_column_map(t->read_set, old_map);

  // undo changes to thd->lex
  thd->lex->wild= wild;
  thd->lex->sql_command= command;

  return t;
}

/// Build linked @c TABLE_LIST list from a list stored in @c Table_list object.

/*
  FIXME: build list with the same order as in input
  Actually, should work fine with reversed list as long as we use the reversed
  list both in table writing and reading.
 */
TABLE_LIST *build_table_list(const Table_list &tables, thr_lock_type lock)
{
  TABLE_LIST *tl= NULL;

  for( uint tno=0; tno < tables.count() ; tno++ )
  {
    TABLE_LIST *ptr= (TABLE_LIST*)my_malloc(sizeof(TABLE_LIST), MYF(MY_WME));
    DBUG_ASSERT(ptr);  // FIXME: report error instead
    bzero(ptr,sizeof(TABLE_LIST));

    Table_ref tbl= tables[tno];

    ptr->alias= ptr->table_name= const_cast<char*>(tbl.name().ptr());
    ptr->db= const_cast<char*>(tbl.db().name().ptr());
    ptr->lock_type= lock;

    // and add it to the list

    ptr->next_global= ptr->next_local=
      ptr->next_name_resolution_table= tl;
    tl= ptr;
    tl->table= ptr->table;
  }

  return tl;
}


/// Execute SQL query without sending anything to client.

int silent_exec_query(THD *thd, ::String &query)
{
  Vio *save_vio= thd->net.vio;

  DBUG_PRINT("restore",("executing query %s",query.c_ptr()));

  /*
    Note: the change net.vio idea taken from execute_init_command in
    sql_parse.cc
   */
  thd->net.vio= 0;
  thd->net.no_send_error= 0;

  thd->query=         query.c_ptr();
  thd->query_length=  query.length();

  thd->set_time(time(NULL));
  pthread_mutex_lock(&::LOCK_thread_count);
  thd->query_id= ::next_query_id();
  pthread_mutex_unlock(&::LOCK_thread_count);

  /*
    @todo The following is a work around for online backup and the DDL blocker.
          It should be removed when the generalized solution is in place.
          This is needed to ensure the restore (which uses DDL) is not blocked
          when the DDL blocker is engaged.
  */
  thd->DDL_exception= TRUE;

  const char *ptr;
  ::mysql_parse(thd,thd->query,thd->query_length,&ptr);

  thd->net.vio= save_vio;

  if (thd->is_error())
  {
    DBUG_PRINT("restore",
              ("error executing query %s!", thd->query));
    DBUG_PRINT("restore",("last error (%d): %s",thd->net.last_errno
                                               ,thd->net.last_error));
    return thd->net.last_errno ? (int)thd->net.last_errno : -1;
  }

  return 0;
}

} // backup namespace
