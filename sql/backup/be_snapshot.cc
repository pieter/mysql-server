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
 * @brief Contains the snapshot backup algorithm driver.
 *
 * This file contains the snapshot backup algorithm (also called a "driver"
 * in the online backup terminology. The snapshot backup algorithm may be
 * used in place of an engine-specific driver if one does not exist or if
 * chosen by the user.
 *
 * The snapshot backup algorithm is a non-blocking algorithm that enables a
 * consistent read of the tables given at the start of the backup/restore 
 * process. This is accomplished by using a consistent snapshot transaction
 * and table locks. Once all of the data is backed up or restored, the locks 
 * are removed. The snapshot backup is a row-level backup and therefore does 
 * not backup the indexes or any of the engine-specific files.
 *
 * The classes in this file use the namespace "snapshot_backup" to distinguish
 * these classes from other backup drivers. The backup functionality is
 * contained in the backup class shown below. Similarly, the restore
 * functionality is contained in the restore class below.
 *
 * The format of the backup is the same as the default backup driver.
 * Please see <code> be_default.cc </code> for a complete description.
 */

#include "mysql_priv.h"
#include "backup_engine.h"
#include "be_snapshot.h"
#include "backup_aux.h"

namespace snapshot_backup {

using backup::byte;
using backup::result_t;
using backup::version_t;
using backup::Table_list;
using backup::Table_ref;
using backup::Buffer;
using namespace backup;

/**
 * Create a snapshot backup backup driver.
 *
 * Given a list of tables to be backed-up, create instance of backup
 * driver which will create backup image of these tables.
 *
 * @param  tables (in) list of tables to be backed-up.
 * @param  eng    (out) pointer to backup driver instance.
 *
 * @retval Error code or backup::OK on success.
 */
result_t Engine::get_backup(const uint32, const Table_list &tables, Backup_driver* &drv)
{
  DBUG_ENTER("Engine::get_backup");
  Backup *ptr= new snapshot_backup::Backup(tables, m_thd);
  if (!ptr)
    DBUG_RETURN(ERROR);
  drv= (backup::Backup_driver *)ptr;
  DBUG_RETURN(OK);
}

result_t Backup::lock()
{
  DBUG_ENTER("Snapshot_backup::lock()");
  /*
    We must fool the locking code to think this is a select because
    any other command type places the engine in a non-consistent read
    state. 
  */
  locking_thd->m_thd->lex->sql_command= SQLCOM_SELECT; 
  locking_thd->m_thd->lex->start_transaction_opt|=
    MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT;
  int res= begin_trans(locking_thd->m_thd);
  if (res)
    DBUG_RETURN(ERROR);
  locking_thd->lock_state= LOCK_ACQUIRED;
  BACKUP_BREAKPOINT("backup_cs_locked");
  DBUG_RETURN(OK);
}

result_t Backup::get_data(Buffer &buf)
{
  result_t res;

  if (!tables_open && (locking_thd->lock_state == LOCK_ACQUIRED))
  {
    BACKUP_BREAKPOINT("backup_cs_open_tables");
    open_and_lock_tables(locking_thd->m_thd, locking_thd->tables_in_backup);
    tables_open= TRUE;
  }
  if (locking_thd->lock_state == LOCK_ACQUIRED)
  {
    BACKUP_BREAKPOINT("backup_cs_reading");
  }

  res= default_backup::Backup::get_data(buf);

  /*
    If this is the last table to be read, close the transaction
    and unlock the tables. This is indicated by the lock state
    being set to LOCK_SIGNAL from parent::get_data(). This is set
    after the last table is finished reading.
  */
  if (locking_thd->lock_state == LOCK_SIGNAL)
  {
    locking_thd->lock_state= LOCK_DONE; // set lock done so destructor won't wait
    end_active_trans(locking_thd->m_thd);
    close_thread_tables(locking_thd->m_thd);
  }
  return(res);
}

/**
 * Create a snapshot backup restore driver.
 *
 * Given a list of tables to be restored, create instance of restore
 * driver which will restore these tables from a backup image.
 *
 * @param  version  (in) version of the backup image.
 * @param  tables   (in) list of tables to be restored.
 * @param  eng      (out) pointer to restore driver instance.
 *
 * @retval Error code or backup::OK on success.
 */
result_t Engine::get_restore(version_t, const uint32, const Table_list &tables,
Restore_driver* &drv)
{
  DBUG_ENTER("Engine::get_restore");
  Restore *ptr= new snapshot_backup::Restore(tables, m_thd);
  if (!ptr)
    DBUG_RETURN(ERROR);
  drv= ptr;
  DBUG_RETURN(OK);
}

} /* snapshot_backup namespace */


