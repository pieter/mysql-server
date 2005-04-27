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

#ifndef NODE_FAILREP_HPP
#define NODE_FAILREP_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

/**
 * This signals is sent by Qmgr to NdbCntr
 *   and then from NdbCntr sent to: dih, dict, lqh, tc & API
 */
class NodeFailRep {
  /**
   * Sender(s)
   */
  friend class Qmgr;
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  friend class Dbdict;
  
  /**
   * Reciver(s)
   */
  friend class Dbdih;
  friend class Dblqh;
  friend class Dbtc;
  friend class ClusterMgr;
  friend class Trix;
  friend class Backup;
  friend class Suma;
  friend class Grep;
  friend class SafeCounterManager;

public:
  STATIC_CONST( SignalLength = 3 + NodeBitmask::Size );
private:
  
  Uint32 failNo;

  /**
   * Note: This field is only set when signals is sent FROM Ndbcntr
   *       (not when signal is sent from Qmgr)
   */
  Uint32 masterNodeId;

  Uint32 noOfNodes;
  Uint32 theNodes[NodeBitmask::Size];
};

#endif
