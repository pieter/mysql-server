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

#ifndef NdbScanOperation_H
#define NdbScanOperation_H

#include <NdbOperation.hpp>

class NdbBlob;
class NdbResultSet;
class PollGuard;

/**
 * @class NdbScanOperation
 * @brief Class of scan operations for use in transactions.  
 */
class NdbScanOperation : public NdbOperation {
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class Ndb;
  friend class NdbTransaction;
  friend class NdbResultSet;
  friend class NdbOperation;
  friend class NdbBlob;
#endif

public:
  /**
   * Scan flags.  OR-ed together and passed as second argument to
   * readTuples.
   */
  enum ScanFlag {
    /* Scan TUP order */
    SF_TupScan = (1 << 16),
    /* Scan in DISK order */
    SF_DiskScan = (2 << 16),
    /*
      Return rows from an index scan sorted, ordered on the index key.
      This flag makes the API perform a merge-sort among the ordered scans of
      each fragment, to get a single sorted result set.
    */
    SF_OrderBy = (1 << 24),
    /* Index scan in descending order, instead of default ascending. */
    SF_Descending = (2 << 24),
    /*
      Enable @ref get_range_no (index scan only).
      When this flag is set, NdbIndexScanOperation::get_range_no() can be
      called to read back the range_no defined in
      NdbIndexScanOperation::end_of_bound(). See @ref end_of_bound() for
      explanation.
    */
    SF_ReadRangeNo = (4 << 24),
    /*
      Request KeyInfo to be sent back.
      This enables the option to take over row lock taken by the scan using
      lockCurrentTuple(), by making sure that the kernel sends back the
      information needed to identify the row and the lock.
      It is enabled by default for scans using LM_Exclusive, but must be
      explicitly specified to enable the taking-over of LM_Read locks.
    */
    SF_KeyInfo = 1
  };

  /**
   * readTuples
   * 
   * @param lock_mode Lock mode
   * @param scan_flags see @ref ScanFlag
   * @param parallel Number of fragments to scan in parallel (0=max)
   * @param batch Number of rows to fetch in each batch
   */ 
  virtual
  int readTuples(LockMode lock_mode = LM_Read, 
                 Uint32 scan_flags = 0, 
		 Uint32 parallel = 0,
		 Uint32 batch = 0);

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * readTuples
   * @param lock_mode Lock mode
   * @param batch No of rows to fetch from each fragment at a time
   * @param parallel No of fragments to scan in parallell
   * @note specifying 0 for batch and parallell means max performance
   */ 
#ifdef ndb_readtuples_impossible_overload
  int readTuples(LockMode lock_mode = LM_Read, 
		 Uint32 batch = 0, Uint32 parallel = 0, bool keyinfo = false);
#endif
  
  inline int readTuples(int parallell){
    return readTuples(LM_Read, 0, parallell);
  }
  
  inline int readTuplesExclusive(int parallell = 0){
    return readTuples(LM_Exclusive, 0, parallell);
  }
#endif
  
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  NdbBlob* getBlobHandle(const char* anAttrName);
  NdbBlob* getBlobHandle(Uint32 anAttrId);
#endif

  /**
   * Get the next tuple in a scan transaction. 
   * 
   * After each call to nextResult
   * the buffers and NdbRecAttr objects defined in 
   * NdbOperation::getValue are updated with values 
   * from the scanned tuple. 
   *
   * @param fetchAllowed  If set to false, then fetching is disabled
   * @param forceSend If true send will occur immediately (see @ref secAdapt)
   *
   * The NDB API will receive tuples from each fragment in batches, and
   * needs to explicitly request from the NDB Kernel the sending of each new
   * batch. When a new batch is requested, the NDB Kernel will remove any
   * locks taken on rows in the previous batch, unless they have been already
   * taken over by the application executing updateCurrentTuple(),
   * lockCurrentTuple(), etc.
   *
   * The fetchAllowed parameter is used to control this release of
   * locks from the application. When fetchAllowed is set to false,
   * the NDB API will not request new batches from the NDB Kernel when
   * all received rows have been exhausted, but will instead return 2
   * from nextResult(), indicating that new batches must be
   * requested. You must then call nextResult with fetchAllowed = true
   * in order to contact the NDB Kernel for more records, after taking over
   * locks as appropriate.
   *
   * fetchAllowed = false is useful when you want to update or 
   * delete all the records fetched in one transaction(This will save a
   *  lot of round trip time and make updates or deletes of scanned 
   * records a lot faster).
   *
   * While nextResult(false) returns 0, take over the record to
   * another transaction. When nextResult(false) returns 2 you must
   * execute and commit the other transaction. This will cause the
   * locks to be transferred to the other transaction, updates or
   * deletes will be made and then the locks will be released.
   *
   * After that, call nextResult(true) which will fetch new records and
   * cache them in the NdbApi. 
   * 
   * @note  If you don't take over the records to another transaction the 
   *        locks on those records will be released the next time NDB Kernel
   *        is contacted for more records.
   *
   * @note  Please contact for examples of efficient scan
   *        updates and deletes.
   * 
   * @note  See ndb/examples/ndbapi_scan_example for usage.
   *
   * @return 
   * -  -1: if unsuccessful,<br>
   * -   0: if another tuple was received, and<br> 
   * -   1: if there are no more tuples to scan.
   * -   2: if there are no more cached records in NdbApi
   */
  int nextResult(bool fetchAllowed = true, bool forceSend = false);

  /**
   * Close scan
   */
  void close(bool forceSend = false, bool releaseOp = false);

  /**
   * Lock current tuple
   *
   * @return an NdbOperation or NULL.
   */
  NdbOperation* lockCurrentTuple();
  /**
   * Lock current tuple
   *
   * @param lockTrans Transaction that should perform the lock
   *
   * @return an NdbOperation or NULL.
   */
  NdbOperation*	lockCurrentTuple(NdbTransaction* lockTrans);
  /**
   * Update current tuple
   *
   * @return an NdbOperation or NULL.
   */
  NdbOperation* updateCurrentTuple();
  /**
   * Update current tuple
   *
   * @param updateTrans Transaction that should perform the update
   *
   * @return an NdbOperation or NULL.
   */
  NdbOperation*	updateCurrentTuple(NdbTransaction* updateTrans);

  /**
   * Delete current tuple
   * @return 0 on success or -1 on failure
   */
  int deleteCurrentTuple();
  /**
   * Delete current tuple
   *
   * @param takeOverTransaction Transaction that should perform the delete
   *
   * @return 0 on success or -1 on failure
   */
  int deleteCurrentTuple(NdbTransaction* takeOverTransaction);
  
  /**
   * Restart scan with exactly the same
   *   getValues and search conditions
   */
  int restart(bool forceSend = false);
  
protected:
  NdbScanOperation(Ndb* aNdb,
                   NdbOperation::Type aType = NdbOperation::TableScan);
  virtual ~NdbScanOperation();

  int nextResultImpl(bool fetchAllowed = true, bool forceSend = false);
  virtual void release();
  
  int close_impl(class TransporterFacade*, bool forceSend,
                 PollGuard *poll_guard);

  // Overloaded methods from NdbCursorOperation
  int executeCursor(int ProcessorId);

  // Overloaded private methods from NdbOperation
  int init(const NdbTableImpl* tab, NdbTransaction*);
  int prepareSend(Uint32  TC_ConnectPtr, Uint64  TransactionId);
  int doSend(int ProcessorId);

  virtual void setErrorCode(int aErrorCode);
  virtual void setErrorCodeAbort(int aErrorCode);

  NdbTransaction *m_transConnection;

  // Scan related variables
  Uint32 theParallelism;
  /*
    Whether keyInfo is requested from Kernel.
    KeyInfo is requested by application (using the SF_KeyInfo scan flag), and
    also enabled automatically when using exclusive locking (lockmode
    LM_Exclusive), or when requesting blobs (getBlobHandle()).
  */
  Uint32 m_keyInfo;

  int getFirstATTRINFOScan();
  int doSendScan(int ProcessorId);
  int prepareSendScan(Uint32 TC_ConnectPtr, Uint64 TransactionId);
  
  int fix_receivers(Uint32 parallel);
  void reset_receivers(Uint32 parallel, Uint32 ordered);
  Uint32* m_array; // containing all arrays below
  Uint32 m_allocated_receivers;
  NdbReceiver** m_receivers;      // All receivers

  Uint32* m_prepared_receivers;   // These are to be sent

  /*
    Owned by API/user thread.

    These receivers, stored in the m_api_receivers array, have all attributes
    from the current batch fully received, and the API thread has moved them
    here (under mutex protection) from m_conf_receivers, so that all further
    nextResult() can access them without extra mutex contention.

    The m_current_api_receiver member is the index (into m_api_receivers) of
    the receiver that delivered the last row to the application in
    nextResult(). If no rows have been delivered yet, it is set to 0 for table
    scans and to one past the end of the array for ordered index scans.

    For ordered index scans, the m_api_receivers array is further kept sorted.
    The entries from (m_current_api_receiver+1) to the end of the array are
    kept in the order that their first row will be returned in nextResult().

    Note also that for table scans, the entries available to the API thread
    are stored in entries 0..(m_api_receivers_count-1), while for ordered
    index scans, they are stored in entries m_current_api_receiver..array end.
   */
  Uint32 m_current_api_receiver;
  Uint32 m_api_receivers_count;
  NdbReceiver** m_api_receivers;  // These are currently used by api
  
  /*
    Shared by receiver thread and API thread.
    These are receivers that the receiver thread has obtained all attribute
    data for (of the current batch).
    API thread will move them (under mutex protection) to m_api_receivers on
    first access with nextResult().
   */
  Uint32 m_conf_receivers_count;  // NOTE needs mutex to access
  NdbReceiver** m_conf_receivers; // receive thread puts them here
  
  /*
   Owned by receiver thread
   These are the receivers that the receiver thread is currently receiving
   attribute data for (of the current batch).
   Once all is received, they will be moved to m_conf_receivers.
  */
  Uint32 m_sent_receivers_count;  // NOTE needs mutex to access
  NdbReceiver** m_sent_receivers; // receive thread puts them here
  
  int send_next_scan(Uint32 cnt, bool close);
  void receiver_delivered(NdbReceiver*);
  void receiver_completed(NdbReceiver*);
  void execCLOSE_SCAN_REP();

  int getKeyFromKEYINFO20(Uint32* data, Uint32 & size);
  NdbOperation*	takeOverScanOp(OperationType opType, NdbTransaction*);
  
  bool m_ordered;
  bool m_descending;
  Uint32 m_read_range_no;
  /*
    m_curr_row: Pointer to last returned row (linked list of NdbRecAttr
    objects).
    First comes keyInfo, if requested (explicitly with SF_KeyInfo, or
    implicitly when using LM_Exclusive).
    Then comes range_no, if requested with SF_ReadRangeNo, included first in
    the list of sort columns to get sorting of multiple range scans right.
    Then the 'real' columns that are participating in the scan.    
  */
  NdbRecAttr *m_curr_row;
  bool m_executed; // Marker if operation should be released at close
};

inline
NdbOperation* 
NdbScanOperation::lockCurrentTuple(){
  return lockCurrentTuple(m_transConnection);
}

inline
NdbOperation* 
NdbScanOperation::lockCurrentTuple(NdbTransaction* takeOverTrans){
  return takeOverScanOp(NdbOperation::ReadRequest, 
			takeOverTrans);
}

inline
NdbOperation* 
NdbScanOperation::updateCurrentTuple(){
  return updateCurrentTuple(m_transConnection);
}

inline
NdbOperation* 
NdbScanOperation::updateCurrentTuple(NdbTransaction* takeOverTrans){
  return takeOverScanOp(NdbOperation::UpdateRequest, 
			takeOverTrans);
}

inline
int
NdbScanOperation::deleteCurrentTuple(){
  return deleteCurrentTuple(m_transConnection);
}

inline
int
NdbScanOperation::deleteCurrentTuple(NdbTransaction * takeOverTrans){
  void * res = takeOverScanOp(NdbOperation::DeleteRequest, 
			      takeOverTrans);
  if(res == 0)
    return -1;
  return 0;
}

#endif
