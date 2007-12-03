/* Copyright (C) 2004-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, 
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

/**
 * @file 
 *
 * @brief Contains the default backup algorithm driver.
 *
 * This file contains the default backup algorithm (also called a "driver"
 * in the online backup terminology. The default backup algorithm may be
 * used in place of an engine-specific driver if one does not exist or if
 * chosen by the user.
 *
 * The default backup algorithm is a blocking algorithm that locks all of
 * the tables given at the start of the backup/restore process. Once all of
 * the data is backed up or restored, the locks are removed. The default
 * backup is a row-level backup and therefore does not backup the indexes
 * or any of the engine-specific files.
 *
 * The classes in this file use the namespace @c default_backup to distinguish
 * these classes from other backup drivers. The backup functionality is
 * contained in the backup class shown below. Similarly, the restore
 * functionality is contained in the restore class below.
 *
 * The format of the backup is written as a series of data blocks where each
 * block contains a flag indicating what kind of data is in the block. The 
 * flags are:
 *
 * <code>
 *   RCD_ONCE   - Single data block for record data
 *   RCD_FIRST  - First data block in buffer for record buffer
 *   RCD_DATA   - Intermediate data block for record buffer
 *   RCD_LAST   - Last data block in buffer for record buffer
 *   BLOB_ONCE  - Single data block for blob data
 *   BLOB_FIRST - First data block in buffer for blob buffer
 *   BLOB_DATA  - Intermediate data block for blob buffer
 *   BLOB_LAST  - Last data block in buffer for blob buffer
 * </code>
 *
 * The flag is the first byte in the block. The remaining space in the block
 * is the data -- either record data or blob fields.
 *
 * The block flagged as BLOB_FIRST also contains a 4-byte field which 
 * contains the total size of the blob field. This is necessary for restore
 * because the size of the blob field is unknown and the size is needed to 
 * allocate memory for the buffer_iterator used to buffer large data from
 * the kernel.
 *
 * TODO 
 *  - Consider making the enums for BACKUP_MODE and RESTORE_MODE bit fields.
 *  - Change code to ignore blobs with no data (NULL).
 */
#include "../mysql_priv.h"
#include "backup_engine.h"
#include "be_default.h"
#include "backup_aux.h"
#include "rpl_record.h"

namespace default_backup {

using backup::byte;
using backup::result_t;
using backup::version_t;
using backup::Table_list;
using backup::Table_ref;
using backup::Buffer;

using namespace backup;

Engine::Engine(THD *t_thd)
{
  m_thd= t_thd;
}

/**
  * Create a default backup backup driver.
  *
  * Given a list of tables to be backed-up, create instance of backup
  * driver which will create backup image of these tables.
  *
  * @param  tables (in) list of tables to be backed-up.
  * @param  eng    (out) pointer to backup driver instance.
  *
  * @retval  ERROR  if cannot create backup driver class.
  * @retval  OK     on success.
  */
result_t Engine::get_backup(const uint32, const Table_list &tables,
                            Backup_driver* &drv)
{
  DBUG_ENTER("Engine::get_backup");
  Backup *ptr= new default_backup::Backup(tables, m_thd, TL_READ_NO_INSERT);
  if (!ptr)
    DBUG_RETURN(ERROR);
  drv= ptr;
  DBUG_RETURN(OK);
}

Backup::Backup(const Table_list &tables, THD *t_thd, thr_lock_type lock_type): 
               Backup_thread_driver(tables)
{
  DBUG_PRINT("default_backup",("Creating backup driver"));
  locking_thd->m_thd= t_thd;  /* save current thread */
  cur_table= NULL;      /* flag current table as null */
  tbl_num= 0;           /* set table number to 0 */
  mode= INITIALIZE;     /* initialize read */
  locking_thd->lock_thd= NULL;  /* set lock thread to 0 */

  /*
     Create a TABLE_LIST * list for iterating through the tables.
     Initialize the list for opening the tables in read mode.
  */
  locking_thd->tables_in_backup= build_table_list(tables, lock_type);
  all_tables= locking_thd->tables_in_backup;
  init_phase_complete= FALSE;
  locks_acquired= FALSE;
}

/**
  * @brief Prelock call to setup locking.
  *
  * Launches a separate thread ("locking thread") which will lock
  * tables. Locking in a separate thread is needed to have a non-blocking
  * prelock() (given that thr_lock() is blocking).
  */
result_t Backup::prelock()
{
  DBUG_ENTER("Default_backup::prelock()");
  DBUG_RETURN(locking_thd->start_locking_thread());
}

/**
  * @brief Start table read.
  *
  * This method saves the handler for the table and initializes the
  * handler for reading.
  *
  * @retval OK     handler initialized properly.
  * @retval ERROR  problem with hander initialization.
  */
result_t Backup::start_tbl_read(TABLE *tbl)
{
  int last_read_res;  

  DBUG_ENTER("Default_backup::start_tbl_read)");
  DBUG_ASSERT(tbl->file);
  hdl= tbl->file;
  last_read_res= hdl->ha_rnd_init(1);
  if (last_read_res != 0)
    DBUG_RETURN(ERROR);
  DBUG_RETURN(OK);
}

/**
  * @brief End table read.
  *
  * This method signals the handler that the reading process is complete.
  *
  * @retval OK     handler read stopped properly.
  * @retval ERROR  problem with hander.
  */
result_t Backup::end_tbl_read()
{
  int last_read_res;

  DBUG_ENTER("Default_backup::end_tbl_read)");
  last_read_res= hdl->ha_rnd_end();
  if (last_read_res != 0)
    DBUG_RETURN(ERROR);
  DBUG_RETURN(OK);
}

/**
  * @brief Get next table in the list.
  *
  * This method iterates through the list of tables selecting the
  * next table in the list and starting the read process.
  *
  * @retval 0   no errors.
  * @retval -1  no more tables in list.
  */
int Backup::next_table()
{
  DBUG_ENTER("Backup::next_table()");
  if (cur_table == NULL)
  {
    cur_table= locking_thd->tables_in_backup->table;
    read_set= cur_table->read_set;
  }
  else
  {
    locking_thd->tables_in_backup= locking_thd->tables_in_backup->next_global;
    if (locking_thd->tables_in_backup != NULL)
    {
      cur_table= locking_thd->tables_in_backup->table;
      read_set= cur_table->read_set;
    }
    else
    {
      cur_table= NULL;
      DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(0);
}

/* Potential buffer on the stack for the bitmap */
#define BITMAP_STACKBUF_SIZE (128/8)

/**
  * @brief Pack the data for a row in the table.
  *
  * This method uses the binary log methods to pack a row from the
  * internal row format to the binary log format.
  *
  * @returns  Size of packed row.
  */
uint Backup::pack(byte *rcd, byte *packed_row)
{
  uint size= 0;
  int error= 0;

  DBUG_ENTER("Default_backup::pack_row(byte *rcd, byte *packed_row)");
  if (cur_table)
  {
    MY_BITMAP cols;
    /* Potential buffer on the stack for the bitmap */
    uint32 bitbuf[BITMAP_STACKBUF_SIZE/sizeof(uint32)];
    uint n_fields= cur_table->s->fields;
    my_bool use_bitbuf= n_fields <= sizeof(bitbuf) * 8;
    error= bitmap_init(&cols, use_bitbuf ? bitbuf : NULL, (n_fields + 7) & ~7UL, FALSE);
    bitmap_set_all(&cols);
    size= pack_row(cur_table, &cols, packed_row, rcd);
    if (!use_bitbuf)
      bitmap_free(&cols);
  }
  DBUG_RETURN(size);
}

/**
  * @brief Get the data for a row in the table.
  * This method is the main method used in the backup operation. It is
  * responsible for reading a row from the table and placing the data in
  * the buffer (buf.data) and setting the correct attributes for processing
  * (e.g., buf.size = size of record data).
  *
  * Control of the method is accomplished by using several modes that
  * signal portions of the method to run. These modes are:
  *
  * <code>
  * INITIALIZE          Indicates time to initialize read
  * GET_NEXT_TABLE      Open next table in the list
  * READ_RCD            Reading rows from table mode
  * READ_RCD_BUFFER     Buffer records mode
  * CHECK_BLOBS         See if record has blobs
  * READ_BLOB           Reading blobs from record mode
  * READ_BLOB_BUFFER    Buffer blobs mode
  * </code>
  *
  * @retval READY   initialization phase complete.
  * @retval OK      data read.
  * @retval ERROR   problem with reading data.
  * @retval DONE    driver finished reading from all tables.
  */
result_t Backup::get_data(Buffer &buf)
{
  int last_read_res;  

  DBUG_ENTER("Default_backup::get_data(Buffer &buf)");

  /*
    Check the lock state. Take action based on the availability of the lock.

    @todo Refactor the following code to make this a new mode for the driver,
          e.g. INIT_PHASE, SYNCH_PHASE, ACQUIRING_LOCKS, etc.
  */
  if (!locks_acquired)
  {
    buf.size= 0;
    buf.table_no= 0; 
    buf.last= TRUE;
    switch (locking_thd->lock_state) {
    case LOCK_ERROR:             // Something ugly happened in locking
      DBUG_RETURN(ERROR);
    case LOCK_ACQUIRED:          // First time lock ready for validity point
    {
      locks_acquired= TRUE;
      DBUG_RETURN(READY);
    }
    default:                     // If first call, signal end of init phase
      if (init_phase_complete)
        DBUG_RETURN(OK);
      else
      {
        init_phase_complete= TRUE;
        DBUG_RETURN(READY);
      }
    }
  }

  buf.table_no= tbl_num;
  buf.last= FALSE;

  /* 
    Determine mode of operation and execute mode.
  */
  switch (mode) {

  /*
    Nothing to do in Initialize, continue to GET_NEXT_TABLE.
  */
  case INITIALIZE:

  /*
    If class has been initialized and we need to read the next table,
    advance the current table pointer and initialize read process.
  */
  case GET_NEXT_TABLE:
  {
    mode= READ_RCD;
    int res= next_table();
    /*
      If no more tables in list, tell backup algorithm we're done else
      fall through to reading mode.
    */
    if (res)
    {
      buf.last= TRUE;
      buf.size= 0;
      buf.table_no= 0;
      DBUG_RETURN(OK);
    }
    else
    {
      start_tbl_read(cur_table);
      tbl_num++;
      buf.table_no= tbl_num;
    }
  }

  /*
    Read a row from the table and save the data in the buffer.
  */
  case READ_RCD:
  {
    uint32 size= cur_table->s->reclength;
    buf.last= FALSE;

    cur_blob= 0;
    cur_table->use_all_columns();
    last_read_res = hdl->rnd_next(cur_table->record[0]);
    DBUG_EXECUTE_IF("SLEEP_DRIVER", sleep(4););
    /*
      If we are end of file, stop the read process and signal the
      backup algorithm that we're done. Turn get_next_table mode on.
    */
    if (last_read_res == HA_ERR_END_OF_FILE)
    {
      end_tbl_read();
      buf.size= 0;
      buf.last= TRUE;
      mode= GET_NEXT_TABLE;

      /*
        Optimization: If this is the last table to read, close the tables and
        kill the lock thread. This only applies iff we are using the thread.
      */
      if (locking_thd->tables_in_backup->next_global == NULL)
        locking_thd->kill_locking_thread();
    }
    else if (last_read_res != 0)
      DBUG_RETURN(ERROR);
    else
    {
      /*
        Check size of buffer to ensure data fits in the buffer. If it does
        not fit, create new blob_buffer object.
      */
      if ((size + META_SIZE) <= buf.size)
      {
        *buf.data= RCD_ONCE; //only part 1 of 1
        int packed_size= 0;
        packed_size= pack(cur_table->record[0], buf.data + META_SIZE);
        buf.size = packed_size + META_SIZE;
        mode= CHECK_BLOBS;
      }
      else
      {
        size_t rec_size= 0;
        byte *rec_ptr= 0;
        byte *packed_ptr= 0;
        int packed_size= 0;

        rec_buffer.initialize(size);
        packed_ptr= rec_buffer.get_base_ptr();
        packed_size= pack(cur_table->record[0], packed_ptr);
        rec_size= rec_buffer.get_next((byte **)&rec_ptr, 
          (buf.size - META_SIZE));
        *buf.data= RCD_FIRST; // first part
        memcpy((byte *)buf.data + META_SIZE, rec_ptr, rec_size);
        buf.size = rec_size + META_SIZE;
        mode= READ_RCD_BUFFER;
      }
    }
    break;
  }

  /*
    Read data from the record buffer and write to the kernel buffer.
  */
  case READ_RCD_BUFFER:
  {
    size_t rec_size= 0; 

    rec_size= rec_buffer.get_next((byte **)&ptr, (buf.size - META_SIZE));
    memcpy((byte *)buf.data + META_SIZE, ptr, rec_size);
    buf.size = rec_size + META_SIZE;
    if (rec_buffer.num_windows(buf.size - META_SIZE) == 0)
    {
      *buf.data= RCD_LAST;
      mode= CHECK_BLOBS;   // Check for blobs.
      rec_buffer.reset();  // dump the memory 
    }
    else
      *buf.data= RCD_DATA;
    break;
  }

  /*
    Check for the existence of blobs. If no blobs, we're finished
    reading data for this row. If there are blobs, turn read_blob
    mode on and get the first blob in the list.
  */
  case CHECK_BLOBS:
  {
    buf.size= 0;
    if (cur_table->s->blob_fields > 0)
    {
      mode= READ_BLOB;
      if (!cur_blob)
      {
        cur_blob= cur_table->s->blob_field;
        last_blob_ptr = cur_blob + cur_table->s->blob_fields;
      }
      /*
        Iterate to the next blob. If no more blobs, we're finished reading
        the row.
      */
      else 
      {
        cur_blob++;
        if (cur_blob == last_blob_ptr)
          mode= READ_RCD;
      }
    }
    else
      mode= READ_RCD;
    break;
  }

  /*
    Get next blob. Use blob buffer if blob field is too large for buffer.data.
  */
  case READ_BLOB:
  {
    uint32 size= ((Field_blob*) cur_table->field[*cur_blob])->get_length();
    /*
      Check size of buffer to ensure data fits in the buffer. If it does
      not fit, create new blob_buffer object.
    */
    if ((size + META_SIZE) <= buf.size)
    {
      *buf.data= BLOB_ONCE;
      ((Field_blob*) cur_table->field[*cur_blob])->get_ptr((uchar **)&ptr);
      memcpy((byte *)buf.data + META_SIZE, ptr, size);
      buf.size = size + META_SIZE;
      mode= CHECK_BLOBS;
    }
    else
    {
      size_t bb_size= 0;
      byte *blob_ptr= 0;

      ((Field_blob*) cur_table->field[*cur_blob])->get_ptr((uchar **)&ptr);
      blob_buffer.initialize((byte *)ptr, size);
      *buf.data= BLOB_FIRST;   //first block
      uint32 field_size= 
        ((Field_blob*) cur_table->field[*cur_blob])->get_length();
      int4store(buf.data + META_SIZE, field_size);     //save max size
      bb_size= blob_buffer.get_next((byte **)&blob_ptr, 
        (buf.size - META_SIZE - 4));
      memcpy((byte *)buf.data + META_SIZE + 4, blob_ptr, bb_size);
      buf.size = bb_size + META_SIZE + 4;
      mode= READ_BLOB_BUFFER;
    }
    break;
  }

/*
  Read data from the blob buffer.
*/
  case READ_BLOB_BUFFER:
  {
    size_t bb_size= 0;

    bb_size= blob_buffer.get_next((byte **)&ptr, (buf.size - META_SIZE));
    memcpy((byte *)buf.data + META_SIZE, ptr, bb_size);
    buf.size = bb_size + META_SIZE;
    if (blob_buffer.num_windows(buf.size - META_SIZE) == 0)
    {
      *buf.data= BLOB_LAST;
      mode= CHECK_BLOBS;
      blob_buffer.reset();     // dump the memory 
    }
    else
      *buf.data= BLOB_DATA;
    break;
  }

  default:
    DBUG_RETURN(ERROR);
  }
  DBUG_RETURN(OK); 
}

/**
  * Create a default backup restore driver.
  *
  * Given a list of tables to be restored, create instance of restore
  * driver which will restore these tables from a backup image.
  *
  * @param  version  (in) version of the backup image.
  * @param  tables   (in) list of tables to be restored.
  * @param  eng      (out) pointer to restore driver instance.
  *
  * @retval ERROR  if cannot create restore driver class.
  * @retval OK     on success.
  */
result_t Engine::get_restore(version_t, const uint32, 
                             const Table_list &tables, Restore_driver* &drv)
{
  DBUG_ENTER("Engine::get_restore");
  Restore *ptr= new default_backup::Restore(tables, m_thd);
  if (!ptr)
    DBUG_RETURN(ERROR);
  drv= ptr;
  DBUG_RETURN(OK);
}

Restore::Restore(const Table_list &tables, THD *t_thd): Restore_driver(tables)
{
  DBUG_PRINT("default_backup",("Creating restore driver"));
  m_thd= t_thd;         /* save current thread */
  cur_table= NULL;      /* flag current table as null */
  tbl_num= 0;           /* set table number to 0 */
  mode= INITIALIZE;     /* initialize write */

  /*
     Create a TABLE_LIST * list for iterating through the tables.
     Initialize the list for opening the tables in write mode.
  */
  tables_in_backup= build_table_list(tables, TL_WRITE);
  all_tables= tables_in_backup;
  for (int i=0; i < MAX_FIELDS; i++)
    blob_ptrs[i]= 0;
  blob_ptr_index= 0;
}

/**
  * @brief Truncate table.
  *
  * This method saves the handler for the table and deletes all rows in
  * the table.
  *
  * @retval OK     rows deleted.
  * @retval ERROR  problem with deleting rows.
  */
result_t Restore::truncate_table(TABLE *tbl)
{
  int last_write_res; 

  DBUG_ENTER("Default_backup::truncate_table)");
  DBUG_ASSERT(tbl->file);
  hdl= tbl->file;
  last_write_res= cur_table->file->delete_all_rows();
  /*
    Check to see if delete all rows was ok. Ignore if the handler
    doesn't support it.
  */
  if ((last_write_res != 0) && (last_write_res != HA_ERR_WRONG_COMMAND))
    DBUG_RETURN(ERROR);
  num_rows= 0;
  DBUG_RETURN(OK);
}

/**
  * @brief End restore process.
  *
  * This method unlocks and closes all of the tables.
  *
  * @retval OK    all tables unlocked.
  */
result_t Restore::end()
{
  DBUG_ENTER("Restore::end");
  close_thread_tables(m_thd);
  DBUG_RETURN(OK);
}

/**
  * @brief Get next table in the list.
  *
  * This method iterates through the list of tables selecting the
  *  next table in the list and starting the write process.
  *
  * @retval 0   no errors.
  * @retval -1  no more tables in list.
  */
int Restore::next_table()
{
  DBUG_ENTER("Restore::next_table()");
  if (cur_table == NULL)
    cur_table= tables_in_backup->table;
  else
  {
    tables_in_backup= tables_in_backup->next_global;
    if (tables_in_backup != NULL)
    {
      DBUG_ASSERT(tables_in_backup->table);
      cur_table= tables_in_backup->table;
    }
    else
    {
      cur_table= NULL;
      DBUG_RETURN(-1);
    }
  } 
  DBUG_RETURN(0);
}

/**
  * @brief Unpack the data for a row in the table.
  *
  * This method uses the binary log methods to unpack a row from the
  * binary log format to the internal row format.
  *
  * @retval 0   no errors.
  * @retval !0  errors during unpack_row().
  */
uint Restore::unpack(byte *packed_row)
{
  int error= 0;
  const uchar *cur_row_end;

  DBUG_ENTER("Default_backup::unpack(byte *packed_row, byte *rcd)");
  if (cur_table)
  {
    MY_BITMAP cols;
    /* Potential buffer on the stack for the bitmap */
    uint32 bitbuf[BITMAP_STACKBUF_SIZE/sizeof(uint32)];
    uint n_fields= cur_table->s->fields;
    my_bool use_bitbuf= n_fields <= sizeof(bitbuf) * 8;
    error= bitmap_init(&cols, use_bitbuf ? bitbuf : NULL, (n_fields + 7) & ~7UL, FALSE);
    bitmap_set_all(&cols);
    ulong length;
    error= unpack_row(NULL, cur_table, n_fields, packed_row, &cols, &cur_row_end, &length);
    if (!use_bitbuf)
      bitmap_free(&cols);
    num_rows++;
  }
  DBUG_RETURN(error);
}

/**
  * @brief Restore the data for a row in the table.
  *
  * This method is the main method used in the restore operation. It is
  * responsible for writing a row to the table.
  *
  * Control of the method is accomplished by using several modes that
  * signal portions of the method to run. These modes are:
  *
  * <code>
  * INITIALIZE          Indicates time to initialize read
  * GET_NEXT_TABLE      Open next table in the list
  * WRITE_RCD           Writing rows from table mode
  * CHECK_BLOBS         See if record has blobs
  * WRITE_BLOB          Writing blobs from record mode
  * WRITE_BLOB_BUFFER   Buffer blobs mode
  * </code>
  *
  * @retval READY       initialization phase complete.
  * @retval OK          data written.
  * @retval ERROR       problem with writing data.
  * @retval PROCESSING  switching modes -- do not advance stream.
  * @retval DONE        driver finished writing to all tables.
  */
result_t Restore::send_data(Buffer &buf)
{
  byte *ptr= 0;
  int last_write_res; 
  byte block_type= 0;

  DBUG_ENTER("Restore::send_data");
  DBUG_PRINT("default/restore",("Got packet with %lu bytes from stream %u",
                                (unsigned long)buf.size, buf.table_no));
  
  /* 
    Determine mode of operation and execute mode.
  */
  switch (mode) {

  /*
    Nothing to do in Initialize, continue to GET_NEXT_TABLE.
  */
  case INITIALIZE:

  /*
    If class has been initialized and we need to read the next table,
    advance the current table pointer and initialize read process.
  */
  case GET_NEXT_TABLE:
  {
    int res;

    mode= WRITE_RCD;
    /*
      It is possible for the backup system to send data to this
      engine out of sequence from the table list. When a non-sequential
      access is detected, start the table list at the beginning and
      find the table in question. This is needed if any tables (more
      than MAX_RETRIES are empty!
    */
    if ((tbl_num + 1) == buf.table_no) //do normal sequential lookup
      res= next_table();
    else                                //do linear search
    {
      uint i= 0;

      cur_table= NULL;
      tables_in_backup= all_tables;
      do
      {
        i++;
        res= next_table();
      }
      while ((i != buf.table_no) && !res);
      tbl_num= i - 1;
    }
    if (res)
    {
      buf.last= TRUE;
      buf.size= 0;
      buf.table_no= 0;
      DBUG_RETURN(OK);
    }
    else
    {
      truncate_table(cur_table); /* delete all rows from table */
      tbl_num++;
    }
  }

  /*
    Write a row to the table from the data in the buffer.
  */
  case WRITE_RCD:
  {
    cur_blob= 0;
    max_blob_size= 0;

    /*
      If the table number is different from the stream number, we're
      receiving data from the backup for a different table. Set the mode to
      get the next table in the list.
    */
    if (tbl_num != buf.table_no)
    {
      mode= GET_NEXT_TABLE;
      DBUG_RETURN(PROCESSING);
    }
    else
    {
      uint32 size= buf.size - META_SIZE;
      block_type= *buf.data;
      cur_table->use_all_columns();
      /*
         Now we're reconstructing the rec from multiple parts.
      */
      switch (block_type) {

      /*
        Buffer iterator not needed, just write the data.
      */
      case RCD_ONCE:
      {
        uint error= unpack((byte *)buf.data + META_SIZE);
        if (error)
          DBUG_RETURN(ERROR);
        else 
        {
          mode= CHECK_BLOBS;
          DBUG_RETURN(PROCESSING);
        }
      }

      /*
        This is the first part of several, create new iterator.
      */
      case RCD_FIRST:
      {
        rec_buffer.initialize(cur_table->s->reclength);
        rec_buffer.put_next((byte *)buf.data + META_SIZE, size);
        mode= WRITE_RCD;
        break;
      }

      /*
        Save the part and keep reading.
      */
      case RCD_DATA:
      {
        rec_buffer.put_next((byte *)buf.data + META_SIZE, size);
        mode= WRITE_RCD;
        break;

      }
      /*
        If this is the last part, assemble and write.
      */
      case RCD_LAST:
      {
        rec_buffer.put_next((byte *)buf.data + META_SIZE, size);
        ptr= (byte *)rec_buffer.get_base_ptr();
        unpack(ptr);
        rec_buffer.reset();
        mode= CHECK_BLOBS;
      }
      default:
        DBUG_RETURN(ERROR);
      }
    }
    if (mode != CHECK_BLOBS)
      break;
  }

  /*
    Check for the existence of blobs. If no blobs, we're finished
    writing data for this row so just run the write_row(). If there
    are blobs, turn write_blob mode on and get the first blob in
    the list. The write_row() call will be delayed until after all
    blobs are written.
  */
  case CHECK_BLOBS:
  {
    my_bool write_row= (cur_table->s->blob_fields == 0);
    if (cur_table->s->blob_fields > 0)
    {
      mode= WRITE_BLOB;
      if (!cur_blob)
      {
        cur_blob= cur_table->s->blob_field;
        last_blob_ptr = cur_blob + cur_table->s->blob_fields;
      }
      /*
        Iterate to the next blob. If no more blobs, we're finished writing 
        the row.
      */
      else 
      {
        cur_blob++;
        if (cur_blob == last_blob_ptr)
          write_row= TRUE;
      }
    }
    if (write_row)
    {
      last_write_res = hdl->write_row(cur_table->record[0]);
      /*
        Free the blob pointers used.
      */
      for (int i=0; i < blob_ptr_index; i++)
        if (blob_ptrs[i])
        {
          my_free(blob_ptrs[i], MYF(0));
          blob_ptrs[i]= 0;
        }
      blob_ptr_index= 0;
      if (last_write_res == 0)
        mode= WRITE_RCD;
      else
        DBUG_RETURN(ERROR);
    }
    break;
  }

  /*
    Write the blobs for the row. Get the size from the buffer and write
    the buffer to the blob field for the specified number of bytes.
  */
  case WRITE_BLOB:
  {
    uint32 size= buf.size - META_SIZE;

    block_type= *buf.data;
    switch (block_type) {

    /*
      Buffer iterator not needed, just write the data.
    */
    case BLOB_ONCE:
    {
      blob_ptrs[blob_ptr_index]= (byte *)my_malloc(size, MYF(MY_WME));
      memcpy(blob_ptrs[blob_ptr_index], (byte *)buf.data + META_SIZE, size);
      ((Field_blob*) cur_table->field[*cur_blob])->set_ptr(size, 
        (uchar *)blob_ptrs[blob_ptr_index]);
      blob_ptr_index++;
      mode= CHECK_BLOBS;
      DBUG_RETURN(PROCESSING);
    }

    /*
      This is the first part of several, create new iterator.
    */
    case BLOB_FIRST:
    {
      max_blob_size= uint4korr(buf.data + META_SIZE);
      blob_ptrs[blob_ptr_index]= (byte *)my_malloc(max_blob_size, MYF(MY_WME));
      blob_buffer.initialize(blob_ptrs[blob_ptr_index], max_blob_size);
      size= buf.size - META_SIZE - 4;
      blob_buffer.put_next((byte *)buf.data + META_SIZE + 4, size);
      mode= WRITE_BLOB;
      break;
    }
 
    /*
      Save the part and keep reading.
    */
    case BLOB_DATA:
    {
      blob_buffer.put_next((byte *)buf.data + META_SIZE, size);
      mode= WRITE_BLOB;
      break;
    }

    /*
      If this is the last part, assemble and write.
    */
    case BLOB_LAST:
    {
      blob_buffer.put_next((byte *)buf.data + META_SIZE, size);
      ptr= (byte *)blob_buffer.get_base_ptr();
      ((Field_blob*) cur_table->field[*cur_blob])->set_ptr(max_blob_size, 
        (uchar *)ptr);
      blob_ptr_index++;
      mode= CHECK_BLOBS;
      DBUG_RETURN(PROCESSING);
    }
    default:
      DBUG_RETURN(ERROR);
    }
    break;
  }

  default:
    DBUG_RETURN(ERROR);
  }
  DBUG_RETURN(OK); 
}

} /* default_backup namespace */


