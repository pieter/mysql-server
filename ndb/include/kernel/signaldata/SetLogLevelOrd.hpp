/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef SET_LOGLEVEL_ORD_HPP
#define SET_LOGLEVEL_ORD_HPP

#include <LogLevel.hpp>

/**
 * 
 */
class SetLogLevelOrd {
  /**
   * Sender(s)
   */
  friend class MgmtSrvr; /* XXX can probably be removed */
  friend class MgmApiSession;
  friend class CommandInterpreter;
  
  /**
   * Reciver(s)
   */
  friend class Cmvmi;

  friend class NodeLogLevel;
  
private:
  STATIC_CONST( SignalLength = 25 );

  Uint32 noOfEntries;
  Uint32 theCategories[12];
  Uint32 theLevels[12];
  
  void clear();
  
  /**
   * Note level is valid as 0-15
   */
  void setLogLevel(LogLevel::EventCategory ec, int level = 7);
};

inline
void
SetLogLevelOrd::clear(){
  noOfEntries = 0;
}

inline
void
SetLogLevelOrd::setLogLevel(LogLevel::EventCategory ec, int level){
  assert(noOfEntries < 12);
  theCategories[noOfEntries] = ec;
  theLevels[noOfEntries] = level;
  noOfEntries++;
}

#endif
