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

#define DBTUX_MAINT_CPP
#include "Dbtux.hpp"

/*
 * Maintain index.
 */

void
Dbtux::execTUX_MAINT_REQ(Signal* signal)
{
  jamEntry();
  TuxMaintReq* const sig = (TuxMaintReq*)signal->getDataPtrSend();
  // ignore requests from redo log
  if (c_internalStartPhase < 6 &&
      c_typeOfStart != NodeState::ST_NODE_RESTART &&
      c_typeOfStart != NodeState::ST_INITIAL_NODE_RESTART) {
    jam();
#ifdef VM_TRACE
    if (debugFlags & DebugMaint) {
      debugOut << "opInfo=" << hex << sig->opInfo;
      debugOut << " tableId=" << dec << sig->tableId;
      debugOut << " indexId=" << dec << sig->indexId;
      debugOut << " fragId=" << dec << sig->fragId;
      debugOut << " tupAddr=" << hex << sig->tupAddr;
      debugOut << " tupVersion=" << dec << sig->tupVersion;
      debugOut << " -- ignored at ISP=" << dec << c_internalStartPhase;
      debugOut << " TOS=" << dec << c_typeOfStart;
      debugOut << endl;
    }
#endif
    sig->errorCode = 0;
    return;
  }
  TuxMaintReq reqCopy = *sig;
  TuxMaintReq* const req = &reqCopy;
  const Uint32 opCode = req->opInfo & 0xFF;
  const Uint32 opFlag = req->opInfo >> 8;
  // get the index
  IndexPtr indexPtr;
  c_indexPool.getPtr(indexPtr, req->indexId);
  ndbrequire(indexPtr.p->m_tableId == req->tableId);
  // get base fragment id and extra bits
  const Uint32 fragOff = indexPtr.p->m_fragOff;
  const Uint32 fragId = req->fragId & ((1 << fragOff) - 1);
  const Uint32 fragBit = req->fragId >> fragOff;
  // get the fragment
  FragPtr fragPtr;
  fragPtr.i = RNIL;
  for (unsigned i = 0; i < indexPtr.p->m_numFrags; i++) {
    jam();
    if (indexPtr.p->m_fragId[i] == fragId) {
      jam();
      c_fragPool.getPtr(fragPtr, indexPtr.p->m_fragPtrI[i]);
      break;
    }
  }
  ndbrequire(fragPtr.i != RNIL);
  Frag& frag = *fragPtr.p;
  ndbrequire(frag.m_nodeList == RNIL);
  // set up index entry
  TreeEnt ent;
  ent.m_tupAddr = req->tupAddr;
  ent.m_tupVersion = req->tupVersion;
  ent.m_fragBit = fragBit;
  // read search key
  ReadPar readPar;
  readPar.m_ent = ent;
  readPar.m_first = 0;
  readPar.m_count = frag.m_numAttrs;
  // output goes here
  readPar.m_data = c_keyBuffer;
  tupReadAttrs(signal, frag, readPar);
  // check if all keys are null
  {
    bool allNull = true;
    ConstData data = readPar.m_data;
    for (unsigned i = 0; i < frag.m_numAttrs; i++) {
      if (! data.ah().isNULL()) {
        jam();
        allNull = false;
        break;
      }
      data += AttributeHeaderSize + data.ah().getDataSize();
    }
    if (allNull) {
      jam();
      req->errorCode = 0;
      *sig = *req;
      return;
    }
  }
  // find position in tree
  SearchPar searchPar;
  searchPar.m_data = c_keyBuffer;
  searchPar.m_ent = ent;
  TreePos treePos;
#ifdef VM_TRACE
  if (debugFlags & DebugMaint) {
    debugOut << "opCode=" << dec << opCode;
    debugOut << " opFlag=" << dec << opFlag;
    debugOut << " tableId=" << dec << req->tableId;
    debugOut << " indexId=" << dec << req->indexId;
    debugOut << " fragId=" << dec << req->fragId;
    debugOut << " entry=" << ent;
    debugOut << endl;
  }
#endif
  treeSearch(signal, frag, searchPar, treePos);
#ifdef VM_TRACE
  if (debugFlags & DebugMaint) {
    debugOut << treePos << endl;
  }
#endif
  // do the operation
  req->errorCode = 0;
  switch (opCode) {
  case TuxMaintReq::OpAdd:
    jam();
    if (treePos.m_match) {
      jam();
      // there is no "Building" state so this will have to do
      if (indexPtr.p->m_state == Index::Online) {
        jam();
        req->errorCode = TuxMaintReq::SearchError;
      }
      break;
    }
    /*
     * At most one new node is inserted in the operation.  We keep one
     * free node pre-allocated so the operation cannot fail.  This also
     * gives a real TupAddr for links to the new node.
     */
    if (frag.m_nodeFree == RNIL) {
      jam();
      preallocNode(signal, frag, req->errorCode);
      if (req->errorCode != 0) {
        jam();
        break;
      }
      ndbrequire(frag.m_nodeFree != RNIL);
    }
    treeAdd(signal, frag, treePos, ent);
    break;
  case TuxMaintReq::OpRemove:
    jam();
    if (! treePos.m_match) {
      jam();
      // there is no "Building" state so this will have to do
      if (indexPtr.p->m_state == Index::Online) {
        jam();
        req->errorCode = TuxMaintReq::SearchError;
      }
      break;
    }
    treeRemove(signal, frag, treePos);
    break;
  default:
    ndbrequire(false);
    break;
  }
  // commit and release nodes
  commitNodes(signal, frag, req->errorCode == 0);
#ifdef VM_TRACE
  if (debugFlags & DebugTree) {
    printTree(signal, frag, debugOut);
  }
#endif
  // copy back
  *sig = *req;
}

/*
 * Read index key attributes from TUP.  If buffer is provided the data
 * is copied to it.  Otherwise pointer is set to signal data.
 */
void
Dbtux::tupReadAttrs(Signal* signal, const Frag& frag, ReadPar& readPar)
{
  // define the direct signal
  const TreeEnt ent = readPar.m_ent;
  TupReadAttrs* const req = (TupReadAttrs*)signal->getDataPtrSend();
  req->errorCode = RNIL;
  req->requestInfo = 0;
  req->tableId = frag.m_tableId;
  req->fragId = frag.m_fragId | (ent.m_fragBit << frag.m_fragOff);
  req->fragPtrI = frag.m_tupTableFragPtrI[ent.m_fragBit];
  req->tupAddr = ent.m_tupAddr;
  req->tupVersion = ent.m_tupVersion;
  req->pageId = RNIL;
  req->pageOffset = 0;
  req->bufferId = 0;
  // add count and list of attribute ids
  Data data = (Uint32*)req + TupReadAttrs::SignalLength;
  data[0] = readPar.m_count;
  data += 1;
  const DescEnt& descEnt = getDescEnt(frag.m_descPage, frag.m_descOff);
  for (Uint32 i = 0; i < readPar.m_count; i++) {
    jam();
    const DescAttr& descAttr = descEnt.m_descAttr[readPar.m_first + i];
    data.ah() = AttributeHeader(descAttr.m_primaryAttrId, 0);
    data += 1;
  }
  // execute
  EXECUTE_DIRECT(DBTUP, GSN_TUP_READ_ATTRS, signal, TupReadAttrs::SignalLength);
  jamEntry();
  ndbrequire(req->errorCode == 0);
  // data is at output
  if (readPar.m_data == 0) {
    readPar.m_data = data;
  } else {
    jam();
    CopyPar copyPar;
    copyPar.m_items = readPar.m_count;
    copyPar.m_headers = true;
    copyAttrs(readPar.m_data, data, copyPar);
  }
}

/*
 * Read primary keys.  Copy the data without attribute headers into the
 * given buffer.   Number of words is returned in ReadPar argument.
 */
void
Dbtux::tupReadKeys(Signal* signal, const Frag& frag, ReadPar& readPar)
{
  // define the direct signal
  const TreeEnt ent = readPar.m_ent;
  TupReadAttrs* const req = (TupReadAttrs*)signal->getDataPtrSend();
  req->errorCode = RNIL;
  req->requestInfo = TupReadAttrs::ReadKeys;
  req->tableId = frag.m_tableId;
  req->fragId = frag.m_fragId | (ent.m_fragBit << frag.m_fragOff);
  req->fragPtrI = frag.m_tupTableFragPtrI[ent.m_fragBit];
  req->tupAddr = ent.m_tupAddr;
  req->tupVersion = RNIL; // not used
  req->pageId = RNIL;
  req->pageOffset = 0;
  req->bufferId = 0;
  // execute
  EXECUTE_DIRECT(DBTUP, GSN_TUP_READ_ATTRS, signal, TupReadAttrs::SignalLength);
  jamEntry();
  ndbrequire(req->errorCode == 0);
  // copy out in special format
  ConstData data = (Uint32*)req + TupReadAttrs::SignalLength;
  const Uint32 numKeys = data[0];
  data += 1 + numKeys;
  // copy out without headers
  ndbrequire(readPar.m_data != 0);
  CopyPar copyPar;
  copyPar.m_items = numKeys;
  copyPar.m_headers = false;
  copyAttrs(readPar.m_data, data, copyPar);
  // return counts
  readPar.m_count = numKeys;
  readPar.m_size = copyPar.m_numwords;
}
