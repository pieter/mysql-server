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

#define DBTUX_SCAN_CPP
#include "Dbtux.hpp"

void
Dbtux::execACC_SCANREQ(Signal* signal)
{
  jamEntry();
  const AccScanReq reqCopy = *(const AccScanReq*)signal->getDataPtr();
  const AccScanReq* const req = &reqCopy;
  ScanOpPtr scanPtr;
  scanPtr.i = RNIL;
  do {
    // get the index
    IndexPtr indexPtr;
    c_indexPool.getPtr(indexPtr, req->tableId);
    // get the fragment
    FragPtr fragPtr;
    fragPtr.i = RNIL;
    for (unsigned i = 0; i < indexPtr.p->m_numFrags; i++) {
      jam();
      if (indexPtr.p->m_fragId[i] == req->fragmentNo) {
        jam();
        c_fragPool.getPtr(fragPtr, indexPtr.p->m_fragPtrI[i]);
        break;
      }
    }
    ndbrequire(fragPtr.i != RNIL);
    Frag& frag = *fragPtr.p;
    ndbrequire(frag.m_nodeList == RNIL);
    // must be normal DIH/TC fragment
    ndbrequire(frag.m_fragId < (1 << frag.m_fragOff));
    TreeHead& tree = frag.m_tree;
    // check for empty fragment
    if (tree.m_root == NullTupAddr) {
      jam();
      AccScanConf* const conf = (AccScanConf*)signal->getDataPtrSend();
      conf->scanPtr = req->senderData;
      conf->accPtr = RNIL;
      conf->flag = AccScanConf::ZEMPTY_FRAGMENT;
      sendSignal(req->senderRef, GSN_ACC_SCANCONF,
          signal, AccScanConf::SignalLength, JBB);
      return;
    }
    // seize from pool and link to per-fragment list
    if (! frag.m_scanList.seize(scanPtr)) {
      jam();
      break;
    }
    new (scanPtr.p) ScanOp(c_scanBoundPool);
    scanPtr.p->m_state = ScanOp::First;
    scanPtr.p->m_userPtr = req->senderData;
    scanPtr.p->m_userRef = req->senderRef;
    scanPtr.p->m_tableId = indexPtr.p->m_tableId;
    scanPtr.p->m_indexId = indexPtr.i;
    scanPtr.p->m_fragId = fragPtr.p->m_fragId;
    scanPtr.p->m_fragPtrI = fragPtr.i;
    scanPtr.p->m_transId1 = req->transId1;
    scanPtr.p->m_transId2 = req->transId2;
    scanPtr.p->m_savePointId = req->savePointId;
    scanPtr.p->m_readCommitted = AccScanReq::getReadCommittedFlag(req->requestInfo);
    scanPtr.p->m_lockMode = AccScanReq::getLockMode(req->requestInfo);
    scanPtr.p->m_keyInfo = AccScanReq::getKeyinfoFlag(req->requestInfo);
#ifdef VM_TRACE
    if (debugFlags & DebugScan) {
      debugOut << "Seize scan " << scanPtr.i << " " << *scanPtr.p << endl;
    }
#endif
    /*
     * readCommitted lockMode keyInfo
     * 1 0 0 - read committed (no lock)
     * 0 0 0 - read latest (read lock)
     * 0 1 1 - read exclusive (write lock)
     */
    // conf
    AccScanConf* const conf = (AccScanConf*)signal->getDataPtrSend();
    conf->scanPtr = req->senderData;
    conf->accPtr = scanPtr.i;
    conf->flag = AccScanConf::ZNOT_EMPTY_FRAGMENT;
    sendSignal(req->senderRef, GSN_ACC_SCANCONF,
        signal, AccScanConf::SignalLength, JBB);
    return;
  } while (0);
  if (scanPtr.i != RNIL) {
    jam();
    releaseScanOp(scanPtr);
  }
  // LQH does not handle REF
  signal->theData[0] = 0x313;
  sendSignal(req->senderRef, GSN_ACC_SCANREF,
      signal, 1, JBB);
}

/*
 * Receive bounds for scan in single direct call.  The bounds can arrive
 * in any order.  Attribute ids are those of index table.
 */
void
Dbtux::execTUX_BOUND_INFO(Signal* signal)
{
  struct BoundInfo {
    unsigned offset;
    unsigned size;
    int type;
  };
  TuxBoundInfo* const sig = (TuxBoundInfo*)signal->getDataPtrSend();
  const TuxBoundInfo reqCopy = *(const TuxBoundInfo*)sig;
  const TuxBoundInfo* const req = &reqCopy;
  // get records
  ScanOp& scan = *c_scanOpPool.getPtr(req->tuxScanPtrI);
  Index& index = *c_indexPool.getPtr(scan.m_indexId);
  // collect bound info for each index attribute
  BoundInfo boundInfo[MaxIndexAttributes][2];
  // largest attrId seen plus one
  Uint32 maxAttrId = 0;
  // skip 5 words
  if (req->boundAiLength < 5) {
    jam();
    scan.m_state = ScanOp::Invalid;
    sig->errorCode = TuxBoundInfo::InvalidAttrInfo;
    return;
  }
  const Uint32* const data = (Uint32*)sig + TuxBoundInfo::SignalLength;
  unsigned offset = 5;
  // walk through entries
  while (offset + 2 < req->boundAiLength) {
    jam();
    const unsigned type = data[offset];
    if (type > 4) {
      jam();
      scan.m_state = ScanOp::Invalid;
      sig->errorCode = TuxBoundInfo::InvalidAttrInfo;
      return;
    }
    const AttributeHeader* ah = (const AttributeHeader*)&data[offset + 1];
    const Uint32 attrId = ah->getAttributeId();
    const Uint32 dataSize = ah->getDataSize();
    if (attrId >= index.m_numAttrs) {
      jam();
      scan.m_state = ScanOp::Invalid;
      sig->errorCode = TuxBoundInfo::InvalidAttrInfo;
      return;
    }
    while (maxAttrId <= attrId) {
      BoundInfo* b = boundInfo[maxAttrId++];
      b[0].type = b[1].type = -1;
    }
    BoundInfo* b = boundInfo[attrId];
    if (type == 0 || type == 1  || type == 4) {
      if (b[0].type != -1) {
        jam();
        scan.m_state = ScanOp::Invalid;
        sig->errorCode = TuxBoundInfo::InvalidBounds;
        return;
      }
      b[0].offset = offset;
      b[0].size = 2 + dataSize;
      b[0].type = type;
    }
    if (type == 2 || type == 3 || type == 4) {
      if (b[1].type != -1) {
        jam();
        scan.m_state = ScanOp::Invalid;
        sig->errorCode = TuxBoundInfo::InvalidBounds;
        return;
      }
      b[1].offset = offset;
      b[1].size = 2 + dataSize;
      b[1].type = type;
    }
    // jump to next
    offset += 2 + dataSize;
  }
  if (offset != req->boundAiLength) {
    jam();
    scan.m_state = ScanOp::Invalid;
    sig->errorCode = TuxBoundInfo::InvalidAttrInfo;
    return;
  }
  // save the bounds in index attribute id order
  scan.m_boundCnt[0] = 0;
  scan.m_boundCnt[1] = 0;
  for (unsigned i = 0; i < maxAttrId; i++) {
    jam();
    const BoundInfo* b = boundInfo[i];
    // current limitation - check all but last is equality
    if (i + 1 < maxAttrId) {
      if (b[0].type != 4 || b[1].type != 4) {
        jam();
        scan.m_state = ScanOp::Invalid;
        sig->errorCode = TuxBoundInfo::InvalidBounds;
        return;
      }
    }
    for (unsigned j = 0; j <= 1; j++) {
      if (b[j].type != -1) {
        jam();
        bool ok = scan.m_bound[j]->append(&data[b[j].offset], b[j].size);
        if (! ok) {
          jam();
          scan.m_state = ScanOp::Invalid;
          sig->errorCode = TuxBoundInfo::OutOfBuffers;
          return;
        }
        scan.m_boundCnt[j]++;
      }
    }
  }
  // no error
  sig->errorCode = 0;
}

void
Dbtux::execNEXT_SCANREQ(Signal* signal)
{
  jamEntry();
  const NextScanReq reqCopy = *(const NextScanReq*)signal->getDataPtr();
  const NextScanReq* const req = &reqCopy;
  ScanOpPtr scanPtr;
  scanPtr.i = req->accPtr;
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "NEXT_SCANREQ scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  ndbrequire(frag.m_nodeList == RNIL);
  // handle unlock previous and close scan
  switch (req->scanFlag) {
  case NextScanReq::ZSCAN_NEXT:
    jam();
    break;
  case NextScanReq::ZSCAN_NEXT_COMMIT:
    jam();
  case NextScanReq::ZSCAN_COMMIT:
    jam();
    if (! scan.m_readCommitted) {
      jam();
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::Unlock;
      lockReq->accOpPtr = req->accOperationPtr;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      removeAccLockOp(scan, req->accOperationPtr);
    }
    if (req->scanFlag == NextScanReq::ZSCAN_COMMIT) {
      jam();
      NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
      conf->scanPtr = scan.m_userPtr;
      unsigned signalLength = 1;
      sendSignal(scanPtr.p->m_userRef, GSN_NEXT_SCANCONF,
          signal, signalLength, JBB);
      return;
    }
    break;
  case NextScanReq::ZSCAN_CLOSE:
    jam();
    // unlink from tree node first to avoid state changes
    if (scan.m_scanPos.m_addr != NullTupAddr) {
      jam();
      const TupAddr addr = scan.m_scanPos.m_addr;
      NodeHandlePtr nodePtr;
      selectNode(signal, frag, nodePtr, addr, AccHead);
      nodePtr.p->unlinkScan(scanPtr);
      scan.m_scanPos.m_addr = NullTupAddr;
    }
    if (scan.m_lockwait) {
      jam();
      ndbrequire(scan.m_accLockOp != RNIL);
      // use ACC_ABORTCONF to flush out any reply in job buffer
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::AbortWithConf;
      lockReq->accOpPtr = scan.m_accLockOp;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      scan.m_state = ScanOp::Aborting;
      commitNodes(signal, frag, true);
      return;
    }
    if (scan.m_state == ScanOp::Locked) {
      jam();
      ndbrequire(scan.m_accLockOp != RNIL);
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::Unlock;
      lockReq->accOpPtr = scan.m_accLockOp;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      scan.m_accLockOp = RNIL;
    }
    scan.m_state = ScanOp::Aborting;
    scanClose(signal, scanPtr);
    return;
  case NextScanReq::ZSCAN_NEXT_ABORT:
    jam();
  default:
    jam();
    ndbrequire(false);
    break;
  }
  // start looking for next scan result
  AccCheckScan* checkReq = (AccCheckScan*)signal->getDataPtrSend();
  checkReq->accPtr = scanPtr.i;
  checkReq->checkLcpStop = AccCheckScan::ZNOT_CHECK_LCP_STOP;
  EXECUTE_DIRECT(DBTUX, GSN_ACC_CHECK_SCAN, signal, AccCheckScan::SignalLength);
  jamEntry();
}

void
Dbtux::execACC_CHECK_SCAN(Signal* signal)
{
  jamEntry();
  const AccCheckScan reqCopy = *(const AccCheckScan*)signal->getDataPtr();
  const AccCheckScan* const req = &reqCopy;
  ScanOpPtr scanPtr;
  scanPtr.i = req->accPtr;
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "ACC_CHECK_SCAN scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  if (req->checkLcpStop == AccCheckScan::ZCHECK_LCP_STOP) {
    jam();
    signal->theData[0] = scan.m_userPtr;
    signal->theData[1] = true;
    EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
    jamEntry();
    commitNodes(signal, frag, true);
    return;   // stop
  }
  if (scan.m_lockwait) {
    jam();
    // LQH asks if we are waiting for lock and we tell it to ask again
    const TreeEnt ent = scan.m_scanPos.m_ent;
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    conf->accOperationPtr = RNIL;       // no tuple returned
    conf->fragId = frag.m_fragId | (ent.m_fragBit << frag.m_fragOff);
    unsigned signalLength = 3;
    // if TC has ordered scan close, it will be detected here
    sendSignal(scan.m_userRef, GSN_NEXT_SCANCONF,
        signal, signalLength, JBB);
    commitNodes(signal, frag, true);
    return;     // stop
  }
  if (scan.m_state == ScanOp::First) {
    jam();
    // search is done only once in single range scan
    scanFirst(signal, scanPtr);
#ifdef VM_TRACE
    if (debugFlags & DebugScan) {
      debugOut << "First scan " << scanPtr.i << " " << scan << endl;
    }
#endif
  }
  if (scan.m_state == ScanOp::Next) {
    jam();
    // look for next
    scanNext(signal, scanPtr);
  }
  // for reading tuple key in Current or Locked state
  ReadPar keyPar;
  keyPar.m_data = 0;   // indicates not yet done
  if (scan.m_state == ScanOp::Current) {
    // found an entry to return
    jam();
    ndbrequire(scan.m_accLockOp == RNIL);
    if (! scan.m_readCommitted) {
      jam();
      const TreeEnt ent = scan.m_scanPos.m_ent;
      // read tuple key
      keyPar.m_ent = ent;
      keyPar.m_data = c_keyBuffer;
      tupReadKeys(signal, frag, keyPar);
      // get read lock or exclusive lock
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo =
        scan.m_lockMode == 0 ? AccLockReq::LockShared : AccLockReq::LockExclusive;
      lockReq->accOpPtr = RNIL;
      lockReq->userPtr = scanPtr.i;
      lockReq->userRef = reference();
      lockReq->tableId = scan.m_tableId;
      lockReq->fragId = frag.m_fragId | (ent.m_fragBit << frag.m_fragOff);
      // should cache this at fragment create
      lockReq->fragPtrI = RNIL;
      const Uint32* const buf32 = static_cast<Uint32*>(keyPar.m_data);
      const Uint64* const buf64 = reinterpret_cast<const Uint64*>(buf32);
      lockReq->hashValue = md5_hash(buf64, keyPar.m_size);
      lockReq->tupAddr = ent.m_tupAddr;
      lockReq->transId1 = scan.m_transId1;
      lockReq->transId2 = scan.m_transId2;
      // execute
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::LockSignalLength);
      jamEntry();
      switch (lockReq->returnCode) {
      case AccLockReq::Success:
        jam();
        scan.m_state = ScanOp::Locked;
        scan.m_accLockOp = lockReq->accOpPtr;
#ifdef VM_TRACE
        if (debugFlags & DebugScan) {
          debugOut << "Lock immediate scan " << scanPtr.i << " " << scan << endl;
        }
#endif
        break;
      case AccLockReq::IsBlocked:
        jam();
        // normal lock wait
        scan.m_state = ScanOp::Blocked;
        scan.m_lockwait = true;
        scan.m_accLockOp = lockReq->accOpPtr;
#ifdef VM_TRACE
        if (debugFlags & DebugScan) {
          debugOut << "Lock wait scan " << scanPtr.i << " " << scan << endl;
        }
#endif
        // LQH will wake us up
        signal->theData[0] = scan.m_userPtr;
        signal->theData[1] = true;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        commitNodes(signal, frag, true);
        return;  // stop
        break;
      case AccLockReq::Refused:
        jam();
        // we cannot see deleted tuple (assert only)
        ndbassert(false);
        // skip it
        scan.m_state = ScanOp::Next;
        signal->theData[0] = scan.m_userPtr;
        signal->theData[1] = true;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        commitNodes(signal, frag, true);
        return;  // stop
        break;
      case AccLockReq::NoFreeOp:
        jam();
        // max ops should depend on max scans (assert only)
        ndbassert(false);
        // stay in Current state
        scan.m_state = ScanOp::Current;
        signal->theData[0] = scan.m_userPtr;
        signal->theData[1] = true;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        commitNodes(signal, frag, true);
        return;  // stop
        break;
      default:
        ndbrequire(false);
        break;
      }
    } else {
      scan.m_state = ScanOp::Locked;
    }
  }
  if (scan.m_state == ScanOp::Locked) {
    // we have lock or do not need one
    jam();
    // read keys if not already done (uses signal)
    const TreeEnt ent = scan.m_scanPos.m_ent;
    if (scan.m_keyInfo) {
      jam();
      if (keyPar.m_data == 0) {
        jam();
        keyPar.m_ent = ent;
        keyPar.m_data = c_keyBuffer;
        tupReadKeys(signal, frag, keyPar);
      }
    }
    // conf signal
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    // the lock is passed to LQH
    Uint32 accLockOp = scan.m_accLockOp;
    if (accLockOp != RNIL) {
      scan.m_accLockOp = RNIL;
      // remember it until LQH unlocks it
      addAccLockOp(scan, accLockOp);
    } else {
      ndbrequire(scan.m_readCommitted);
      // operation RNIL in LQH would signal no tuple returned
      accLockOp = (Uint32)-1;
    }
    conf->accOperationPtr = accLockOp;
    conf->fragId = frag.m_fragId | (ent.m_fragBit << frag.m_fragOff);
    conf->localKey[0] = ent.m_tupAddr;
    conf->localKey[1] = 0;
    conf->localKeyLength = 1;
    unsigned signalLength = 6;
    // add key info
    if (scan.m_keyInfo) {
      jam();
      conf->keyLength = keyPar.m_size;
      // piggy-back first 4 words of key data
      for (unsigned i = 0; i < 4; i++) {
        conf->key[i] = i < keyPar.m_size ? keyPar.m_data[i] : 0;
      }
      signalLength = 11;
    }
    if (! scan.m_readCommitted) {
      sendSignal(scan.m_userRef, GSN_NEXT_SCANCONF,
          signal, signalLength, JBB);
    } else {
      Uint32 blockNo = refToBlock(scan.m_userRef);
      EXECUTE_DIRECT(blockNo, GSN_NEXT_SCANCONF, signal, signalLength);
    }
    // send rest of key data
    if (scan.m_keyInfo && keyPar.m_size > 4) {
      unsigned total = 4;
      while (total < keyPar.m_size) {
        jam();
        unsigned length = keyPar.m_size - total;
        if (length > 20)
          length = 20;
        signal->theData[0] = scan.m_userPtr;
        signal->theData[1] = 0;
        signal->theData[2] = 0;
        signal->theData[3] = length;
        memcpy(&signal->theData[4], &keyPar.m_data[total], length << 2);
        sendSignal(scan.m_userRef, GSN_ACC_SCAN_INFO24,
            signal, 4 + length, JBB);
        total += length;
      }
    }
    // remember last entry returned
    scan.m_lastEnt = ent;
    // next time look for next entry
    scan.m_state = ScanOp::Next;
    commitNodes(signal, frag, true);
    return;
  }
  // XXX in ACC this is checked before req->checkLcpStop
  if (scan.m_state == ScanOp::Last ||
      scan.m_state == ScanOp::Invalid) {
    jam();
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    conf->accOperationPtr = RNIL;
    conf->fragId = RNIL;
    unsigned signalLength = 3;
    sendSignal(scanPtr.p->m_userRef, GSN_NEXT_SCANCONF,
        signal, signalLength, JBB);
    commitNodes(signal, frag, true);
    return;
  }
  ndbrequire(false);
}

/*
 * Lock succeeded (after delay) in ACC.  If the lock is for current
 * entry, set state to Locked.  If the lock is for an entry we were
 * moved away from, simply unlock it.  Finally, if we are closing the
 * scan, do nothing since we have already sent an abort request.
 */
void
Dbtux::execACCKEYCONF(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Lock obtained scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  ndbrequire(scan.m_lockwait && scan.m_accLockOp != RNIL);
  scan.m_lockwait = false;
  if (scan.m_state == ScanOp::Blocked) {
    // the lock wait was for current entry
    jam();
    scan.m_state = ScanOp::Locked;
    // LQH has the ball
    return;
  }
  if (scan.m_state != ScanOp::Aborting) {
    // we were moved, release lock
    jam();
    AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
    lockReq->returnCode = RNIL;
    lockReq->requestInfo = AccLockReq::Unlock;
    lockReq->accOpPtr = scan.m_accLockOp;
    EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
    jamEntry();
    ndbrequire(lockReq->returnCode == AccLockReq::Success);
    scan.m_accLockOp = RNIL;
    // LQH has the ball
    return;
  }
  // continue at ACC_ABORTCONF
}

/*
 * Lock failed (after delay) in ACC.  Probably means somebody ahead of
 * us in lock queue deleted the tuple.
 */
void
Dbtux::execACCKEYREF(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Lock refused scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  ndbrequire(scan.m_lockwait && scan.m_accLockOp != RNIL);
  scan.m_lockwait = false;
  if (scan.m_state != ScanOp::Aborting) {
    jam();
    // release the operation
    AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
    lockReq->returnCode = RNIL;
    lockReq->requestInfo = AccLockReq::Abort;
    lockReq->accOpPtr = scan.m_accLockOp;
    EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
    jamEntry();
    ndbrequire(lockReq->returnCode == AccLockReq::Success);
    scan.m_accLockOp = RNIL;
    // scan position should already have been moved (assert only)
    if (scan.m_state == ScanOp::Blocked) {
      jam();
      ndbassert(false);
      scan.m_state = ScanOp::Next;
    }
    // LQH has the ball
    return;
  }
  // continue at ACC_ABORTCONF
}

/*
 * Received when scan is closing.  This signal arrives after any
 * ACCKEYCON or ACCKEYREF which may have been in job buffer.
 */
void
Dbtux::execACC_ABORTCONF(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "ACC_ABORTCONF scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  ndbrequire(scan.m_state == ScanOp::Aborting);
  // most likely we are still in lock wait
  if (scan.m_lockwait) {
    jam();
    scan.m_lockwait = false;
    scan.m_accLockOp = RNIL;
  }
  scanClose(signal, scanPtr);
}

/*
 * Find start position for single range scan.  If it exists, sets state
 * to Next and links the scan to the node.  The first entry is returned
 * by scanNext.
 */
void
Dbtux::scanFirst(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
  TreeHead& tree = frag.m_tree;
  if (tree.m_root == NullTupAddr) {
    // tree may have become empty
    jam();
    scan.m_state = ScanOp::Last;
    return;
  }
  TreePos pos;
  pos.m_addr = tree.m_root;
  NodeHandlePtr nodePtr;
  // unpack lower bound
  const ScanBound& bound = *scan.m_bound[0];
  ScanBoundIterator iter;
  bound.first(iter);
  for (unsigned j = 0; j < bound.getSize(); j++) {
    jam();
    c_keyBuffer[j] = *iter.data;
    bound.next(iter);
  }
  // comparison parameters
  BoundPar boundPar;
  boundPar.m_data1 = c_keyBuffer;
  boundPar.m_count1 = scan.m_boundCnt[0];
  boundPar.m_dir = 0;
loop: {
    jam();
    selectNode(signal, frag, nodePtr, pos.m_addr, AccPref);
    const unsigned occup = nodePtr.p->getOccup();
    ndbrequire(occup != 0);
    for (unsigned i = 0; i <= 1; i++) {
      jam();
      // compare prefix
      boundPar.m_data2 = nodePtr.p->getPref(i);
      boundPar.m_len2 = tree.m_prefSize;
      int ret = cmpScanBound(frag, boundPar);
      if (ret == NdbSqlUtil::CmpUnknown) {
        jam();
        // read full value
        ReadPar readPar;
        readPar.m_ent = nodePtr.p->getMinMax(i);
        readPar.m_first = 0;
        readPar.m_count = frag.m_numAttrs;
        readPar.m_data = 0;     // leave in signal data
        tupReadAttrs(signal, frag, readPar);
        // compare full value
        boundPar.m_data2 = readPar.m_data;
        boundPar.m_len2 = ZNIL; // big
        ret = cmpScanBound(frag, boundPar);
        ndbrequire(ret != NdbSqlUtil::CmpUnknown);
      }
      if (i == 0 && ret < 0) {
        jam();
        const TupAddr tupAddr = nodePtr.p->getLink(i);
        if (tupAddr != NullTupAddr) {
          jam();
          // continue to left subtree
          pos.m_addr = tupAddr;
          goto loop;
        }
        // start scanning this node
        pos.m_pos = 0;
        pos.m_match = false;
        pos.m_dir = 3;
        scan.m_scanPos = pos;
        scan.m_state = ScanOp::Next;
        nodePtr.p->linkScan(scanPtr);
        return;
      }
      if (i == 1 && ret > 0) {
        jam();
        const TupAddr tupAddr = nodePtr.p->getLink(i);
        if (tupAddr != NullTupAddr) {
          jam();
          // continue to right subtree
          pos.m_addr = tupAddr;
          goto loop;
        }
        // start scanning upwards
        pos.m_dir = 1;
        scan.m_scanPos = pos;
        scan.m_state = ScanOp::Next;
        nodePtr.p->linkScan(scanPtr);
        return;
      }
    }
    // read rest of current node
    accessNode(signal, frag, nodePtr, AccFull);
    // look for first entry
    ndbrequire(occup >= 2);
    for (unsigned j = 1; j < occup; j++) {
      jam();
      ReadPar readPar;
      readPar.m_ent = nodePtr.p->getEnt(j);
      readPar.m_first = 0;
      readPar.m_count = frag.m_numAttrs;
      readPar.m_data = 0;       // leave in signal data
      tupReadAttrs(signal, frag, readPar);
      // compare
      boundPar.m_data2 = readPar.m_data;
      boundPar.m_len2 = ZNIL;   // big
      int ret = cmpScanBound(frag, boundPar);
      ndbrequire(ret != NdbSqlUtil::CmpUnknown);
      if (ret < 0) {
        jam();
        // start scanning this node
        pos.m_pos = j;
        pos.m_match = false;
        pos.m_dir = 3;
        scan.m_scanPos = pos;
        scan.m_state = ScanOp::Next;
        nodePtr.p->linkScan(scanPtr);
        return;
      }
    }
    ndbrequire(false);
  }
}

/*
 * Move to next entry.  The scan is already linked to some node.  When
 * we leave, if any entry was found, it will be linked to a possibly
 * different node.  The scan has a direction, one of:
 *
 * 0 - coming up from left child
 * 1 - coming up from right child (proceed to parent immediately)
 * 2 - coming up from root (the scan ends)
 * 3 - left to right within node
 * 4 - coming down from parent to left or right child
 */
void
Dbtux::scanNext(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Next in scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  if (scan.m_state == ScanOp::Locked) {
    jam();
    // version of a tuple locked by us cannot disappear (assert only)
    ndbassert(false);
    AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
    lockReq->returnCode = RNIL;
    lockReq->requestInfo = AccLockReq::Unlock;
    lockReq->accOpPtr = scan.m_accLockOp;
    EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
    jamEntry();
    ndbrequire(lockReq->returnCode == AccLockReq::Success);
    scan.m_accLockOp = RNIL;
    scan.m_state = ScanOp::Current;
  }
  // unpack upper bound
  const ScanBound& bound = *scan.m_bound[1];
  ScanBoundIterator iter;
  bound.first(iter);
  for (unsigned j = 0; j < bound.getSize(); j++) {
    jam();
    c_keyBuffer[j] = *iter.data;
    bound.next(iter);
  }
  // comparison parameters
  BoundPar boundPar;
  boundPar.m_data1 = c_keyBuffer;
  boundPar.m_count1 = scan.m_boundCnt[1];
  boundPar.m_dir = 1;
  // use copy of position
  TreePos pos = scan.m_scanPos;
  // get and remember original node
  NodeHandlePtr origNodePtr;
  selectNode(signal, frag, origNodePtr, pos.m_addr, AccHead);
  ndbrequire(origNodePtr.p->islinkScan(scanPtr));
  // current node in loop
  NodeHandlePtr nodePtr = origNodePtr;
  while (true) {
    jam();
    if (pos.m_dir == 2) {
      // coming up from root ends the scan
      jam();
      pos.m_addr = NullTupAddr;
      scan.m_state = ScanOp::Last;
      break;
    }
    if (nodePtr.p->m_addr != pos.m_addr) {
      jam();
      selectNode(signal, frag, nodePtr, pos.m_addr, AccHead);
    }
    if (pos.m_dir == 4) {
      // coming down from parent proceed to left child
      jam();
      TupAddr addr = nodePtr.p->getLink(0);
      if (addr != NullTupAddr) {
        jam();
        pos.m_addr = addr;
        pos.m_dir = 4;  // unchanged
        continue;
      }
      // pretend we came from left child
      pos.m_dir = 0;
    }
    if (pos.m_dir == 0) {
      // coming from left child scan current node
      jam();
      pos.m_pos = 0;
      pos.m_match = false;
      pos.m_dir = 3;
    }
    if (pos.m_dir == 3) {
      // within node
      jam();
      unsigned occup = nodePtr.p->getOccup();
      ndbrequire(occup >= 1);
      // access full node
      accessNode(signal, frag, nodePtr, AccFull);
      // advance position
      if (! pos.m_match)
        pos.m_match = true;
      else
        pos.m_pos++;
      if (pos.m_pos < occup) {
        jam();
        pos.m_ent = nodePtr.p->getEnt(pos.m_pos);
        pos.m_dir = 3;  // unchanged
        // XXX implement prefix optimization
        ReadPar readPar;
        readPar.m_ent = pos.m_ent;
        readPar.m_first = 0;
        readPar.m_count = frag.m_numAttrs;
        readPar.m_data = 0;     // leave in signal data
        tupReadAttrs(signal, frag, readPar);
        // compare
        boundPar.m_data2 = readPar.m_data;
        boundPar.m_len2 = ZNIL; // big
        int ret = cmpScanBound(frag, boundPar);
        ndbrequire(ret != NdbSqlUtil::CmpUnknown);
        if (ret < 0) {
          jam();
          // hit upper bound of single range scan
          pos.m_addr = NullTupAddr;
          scan.m_state = ScanOp::Last;
          break;
        }
        // can we see it
        if (! scanVisible(signal, scanPtr, pos.m_ent)) {
          jam();
          continue;
        }
        // found entry
        scan.m_state = ScanOp::Current;
        break;
      }
      // after node proceed to right child
      TupAddr addr = nodePtr.p->getLink(1);
      if (addr != NullTupAddr) {
        jam();
        pos.m_addr = addr;
        pos.m_dir = 4;
        continue;
      }
      // pretend we came from right child
      pos.m_dir = 1;
    }
    if (pos.m_dir == 1) {
      // coming from right child proceed to parent
      jam();
      pos.m_addr = nodePtr.p->getLink(2);
      pos.m_dir = nodePtr.p->getSide();
      continue;
    }
    ndbrequire(false);
  }
  // copy back position
  scan.m_scanPos = pos;
  // relink
  if (scan.m_state == ScanOp::Current) {
    ndbrequire(pos.m_addr == nodePtr.p->m_addr);
    if (origNodePtr.i != nodePtr.i) {
      jam();
      origNodePtr.p->unlinkScan(scanPtr);
      nodePtr.p->linkScan(scanPtr);
    }
  } else if (scan.m_state == ScanOp::Last) {
    jam();
    ndbrequire(pos.m_addr == NullTupAddr);
    origNodePtr.p->unlinkScan(scanPtr);
  } else {
    ndbrequire(false);
  }
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Next out scan " << scanPtr.i << " " << scan << endl;
  }
#endif
}

/*
 * Check if an entry is visible to the scan.
 *
 * There is a special check to never return same tuple twice in a row.
 * This is faster than asking TUP.  It also fixes some special cases
 * which are not analyzed or handled yet.
 */
bool
Dbtux::scanVisible(Signal* signal, ScanOpPtr scanPtr, TreeEnt ent)
{
  TupQueryTh* const req = (TupQueryTh*)signal->getDataPtrSend();
  const ScanOp& scan = *scanPtr.p;
  const Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
  /* Assign table, fragment, tuple address + version */
  Uint32 tableId = frag.m_tableId;
  Uint32 fragBit = ent.m_fragBit;
  Uint32 fragId = frag.m_fragId | (fragBit << frag.m_fragOff);
  Uint32 tupAddr = ent.m_tupAddr;
  Uint32 tupVersion = ent.m_tupVersion;
  /* Check for same tuple twice in row */
  if (scan.m_lastEnt.m_tupAddr == tupAddr &&
      scan.m_lastEnt.m_fragBit == fragBit) {
    jam();
    return false;
  }
  req->tableId = tableId;
  req->fragId = fragId;
  req->tupAddr = tupAddr;
  req->tupVersion = tupVersion;
  /* Assign transaction info, trans id + savepoint id */
  Uint32 transId1 = scan.m_transId1;
  Uint32 transId2 = scan.m_transId2;
  Uint32 savePointId = scan.m_savePointId;
  req->transId1 = transId1;
  req->transId2 = transId2;
  req->savePointId = savePointId;
  EXECUTE_DIRECT(DBTUP, GSN_TUP_QUERY_TH, signal, TupQueryTh::SignalLength);
  jamEntry();
  return (bool)req->returnCode;
}

/*
 * Finish closing of scan and send conf.  Any lock wait has been done
 * already.
 */
void
Dbtux::scanClose(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scanPtr.p->m_fragPtrI);
  ndbrequire(! scan.m_lockwait && scan.m_accLockOp == RNIL);
  // unlock all not unlocked by LQH
  for (unsigned i = 0; i < MaxAccLockOps; i++) {
    if (scan.m_accLockOps[i] != RNIL) {
      jam();
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::Abort;
      lockReq->accOpPtr = scan.m_accLockOps[i];
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      scan.m_accLockOps[i] = RNIL;
    }
  }
  // send conf
  NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
  conf->scanPtr = scanPtr.p->m_userPtr;
  conf->accOperationPtr = RNIL;
  conf->fragId = RNIL;
  unsigned signalLength = 3;
  sendSignal(scanPtr.p->m_userRef, GSN_NEXT_SCANCONF,
      signal, signalLength, JBB);
  releaseScanOp(scanPtr);
  commitNodes(signal, frag, true);
}

void
Dbtux::addAccLockOp(ScanOp& scan, Uint32 accLockOp)
{
  ndbrequire(accLockOp != RNIL);
  Uint32* list = scan.m_accLockOps;
  bool ok = false;
  for (unsigned i = 0; i < MaxAccLockOps; i++) {
    ndbrequire(list[i] != accLockOp);
    if (! ok && list[i] == RNIL) {
      list[i] = accLockOp;
      ok = true;
      // continue check for duplicates
    }
  }
  ndbrequire(ok);
}

void
Dbtux::removeAccLockOp(ScanOp& scan, Uint32 accLockOp)
{
  ndbrequire(accLockOp != RNIL);
  Uint32* list = scan.m_accLockOps;
  bool ok = false;
  for (unsigned i = 0; i < MaxAccLockOps; i++) {
    if (list[i] == accLockOp) {
      list[i] = RNIL;
      ok = true;
      break;
    }
  }
  ndbrequire(ok);
}

/*
 * Release allocated records.
 */
void
Dbtux::releaseScanOp(ScanOpPtr& scanPtr)
{
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Release scan " << scanPtr.i << " " << *scanPtr.p << endl;
  }
#endif
  Frag& frag = *c_fragPool.getPtr(scanPtr.p->m_fragPtrI);
  scanPtr.p->m_boundMin.release();
  scanPtr.p->m_boundMax.release();
  // unlink from per-fragment list and release from pool
  frag.m_scanList.release(scanPtr);
}
