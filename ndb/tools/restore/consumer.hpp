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

#ifndef CONSUMER_HPP
#define CONSUMER_HPP

#include "Restore.hpp"

class BackupConsumer {
public:
  BackupConsumer() {}
  virtual ~BackupConsumer() { }
  virtual bool init() { return true;}
  virtual bool table(const TableS &){return true;}
  virtual bool endOfTables() { return true; }
  virtual void tuple(const TupleS &){}
  virtual void tuple_free(){}
  virtual void endOfTuples(){}
  virtual void logEntry(const LogEntry &){}
  virtual void endOfLogEntrys(){}
  virtual bool finalize_table(const TableS &){return true;}
  virtual bool has_temp_error() {return false;}
};

#endif
