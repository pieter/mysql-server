/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef CONSUMER_RESTORE_HPP
#define CONSUMER_RESTORE_HPP

#include "consumer.hpp"

struct restore_callback_t {
  class BackupRestore *restore;
  class TupleS tup;
  class NdbTransaction *connection;
  int    retries;
  int error_code;
  restore_callback_t *next;
};


class BackupRestore : public BackupConsumer 
{
public:
  BackupRestore(Uint32 parallelism=1) 
  {
    m_ndb = 0;
    m_cluster_connection = 0;
    m_logCount = m_dataCount = 0;
    m_restore = false;
    m_restore_meta = false;
    m_parallelism = parallelism;
    m_callback = 0;
    m_free_callback = 0;
    m_temp_error = false;
    m_transactions = 0;
    m_cache.m_old_table = 0;
  }
  
  virtual ~BackupRestore();
  virtual bool init();
  virtual void release();
  virtual bool table(const TableS &);
  virtual bool endOfTables();
  virtual void tuple(const TupleS &);
  virtual void tuple_free();
  virtual void tuple_a(restore_callback_t *cb);
  virtual void cback(int result, restore_callback_t *cb);
  virtual bool errorHandler(restore_callback_t *cb);
  virtual void exitHandler();
  virtual void endOfTuples();
  virtual void logEntry(const LogEntry &);
  virtual void endOfLogEntrys();
  virtual bool finalize_table(const TableS &);
  virtual bool has_temp_error();
  void connectToMysql();
  Ndb * m_ndb;
  Ndb_cluster_connection * m_cluster_connection;
  bool m_restore;
  bool m_restore_meta;
  Uint32 m_logCount;
  Uint32 m_dataCount;

  Uint32 m_parallelism;
  volatile Uint32 m_transactions;

  restore_callback_t *m_callback;
  restore_callback_t *m_free_callback;
  bool m_temp_error;

  /**
   * m_new_table_ids[X] = Y;
   *   X - old table id
   *   Y != 0  - new table
   */
  Vector<const NdbDictionary::Table*> m_new_tables;
  struct {
    const NdbDictionary::Table* m_old_table;
    const NdbDictionary::Table* m_new_table;
  } m_cache;
  const NdbDictionary::Table* get_table(const NdbDictionary::Table* );

  Vector<const NdbDictionary::Table*> m_indexes;
};

#endif
