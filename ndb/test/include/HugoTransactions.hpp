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

#ifndef HUGO_TRANSACTIONS_HPP
#define HUGO_TRANSACTIONS_HPP


#include <NDBT.hpp>
#include <HugoCalculator.hpp>
#include <HugoOperations.hpp>


class HugoTransactions : public HugoOperations {
public:
  HugoTransactions(const NdbDictionary::Table&);
  ~HugoTransactions();
  int createEvent(Ndb*);
  int eventOperation(Ndb*, void* stats,
		     int records);
  int loadTable(Ndb*, 
		int records,
		int batch = 512,
		bool allowConstraintViolation = true,
		int doSleep = 0);
  int scanReadRecords(Ndb*, 
		      int records,
		      int abort = 0,
		      int parallelism = 0,
		      bool committed = false);
  int scanReadCommittedRecords(Ndb*, 
			      int records,
			      int abort = 0,
			      int parallelism = 0);
  int pkReadRecords(Ndb*, 
		    int records,
		    int batchsize = 1,
		    bool dirty = false);

  int scanUpdateRecords(Ndb*, 
			int records,
			int abort = 0,
			int parallelism = 0);

  int scanUpdateRecords1(Ndb*, 
			 int records,
			 int abort = 0,
			 int parallelism = 0);
  int scanUpdateRecords2(Ndb*, 
			 int records,
			 int abort = 0,
			 int parallelism = 0);
  int scanUpdateRecords3(Ndb*, 
			 int records,
			 int abort = 0,
			 int parallelism = 0);

  int pkUpdateRecords(Ndb*, 
		      int records,
		      int batchsize = 1,
		      int doSleep = 0);
  int pkInterpretedUpdateRecords(Ndb*, 
				 int records,
				 int batchsize = 1);
  int pkDelRecords(Ndb*, 
		   int records = 0,
		   int batch = 1,
		   bool allowConstraintViolation = true,
		   int doSleep = 0);
  int lockRecords(Ndb*,
		  int records,
		  int percentToLock = 1,
		  int lockTime = 1000);
  int fillTable(Ndb*,
		int batch=512);

  /**
   * Reading using UniqHashIndex with key = pk
   */
  int indexReadRecords(Ndb*, 
		       const char * idxName,
		       int records,
		       int batchsize = 1);

  int indexUpdateRecords(Ndb*,
			 const char * idxName,
			 int records,
			 int batchsize = 1);
  
protected:  
  NDBT_ResultRow row;
  int m_defaultScanUpdateMethod;
};




#endif

