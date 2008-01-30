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

#include "Backup.hpp"

#include <ndb_version.h>

#include <NdbTCP.h>
#include <Bitmask.hpp>

#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>

#include <signaldata/ScanFrag.hpp>

#include <signaldata/GetTabInfo.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/ListTables.hpp>

#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsAppendReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsRemoveReq.hpp>

#include <signaldata/BackupImpl.hpp>
#include <signaldata/BackupSignalData.hpp>
#include <signaldata/BackupContinueB.hpp>
#include <signaldata/EventReport.hpp>

#include <signaldata/UtilSequence.hpp>

#include <signaldata/CreateTrig.hpp>
#include <signaldata/AlterTrig.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <AttributeHeader.hpp>

#include <signaldata/WaitGCP.hpp>

#include <NdbTick.h>

static NDB_TICKS startTime;

static const Uint32 BACKUP_SEQUENCE = 0x1F000000;

#ifdef VM_TRACE
#define DEBUG_OUT(x) ndbout << x << endl
#else
#define DEBUG_OUT(x) 
#endif

//#define DEBUG_ABORT

static Uint32 g_TypeOfStart = NodeState::ST_ILLEGAL_TYPE;

#define SEND_BACKUP_STARTED_FLAG(A) (((A) & 0x3) > 0)
#define SEND_BACKUP_COMPLETED_FLAG(A) (((A) & 0x3) > 1)

void 
Backup::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();

  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    theConfiguration.getOwnConfigIterator();
  ndbrequire(p != 0);

  c_nodePool.setSize(MAX_NDB_NODES);

  Uint32 noBackups = 0, noTables = 0, noAttribs = 0, noFrags = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_DISCLESS, &m_diskless));
  ndb_mgm_get_int_parameter(p, CFG_DB_PARALLEL_BACKUPS, &noBackups);
  //  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_TABLES, &noTables));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DICT_TABLE, &noTables));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_ATTRIBUTES, &noAttribs));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DIH_FRAG_CONNECT, &noFrags));

  noAttribs++; //RT 527 bug fix

  c_backupPool.setSize(noBackups);
  c_backupFilePool.setSize(3 * noBackups);
  c_tablePool.setSize(noBackups * noTables);
  c_attributePool.setSize(noBackups * noAttribs);
  c_triggerPool.setSize(noBackups * 3 * noTables);
  c_fragmentPool.setSize(noBackups * noFrags);
  
  Uint32 szMem = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_MEM, &szMem);
  Uint32 noPages = (szMem + sizeof(Page32) - 1) / sizeof(Page32);
  // We need to allocate an additional of 2 pages. 1 page because of a bug in
  // ArrayPool and another one for DICTTAINFO.
  c_pagePool.setSize(noPages + NO_OF_PAGES_META_FILE + 2); 

  Uint32 szDataBuf = (2 * 1024 * 1024);
  Uint32 szLogBuf = (2 * 1024 * 1024);
  Uint32 szWrite = 32768, maxWriteSize = (256 * 1024);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_DATA_BUFFER_MEM, &szDataBuf);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_LOG_BUFFER_MEM, &szLogBuf);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_WRITE_SIZE, &szWrite);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_MAX_WRITE_SIZE, &maxWriteSize);
  
  c_defaults.m_logBufferSize = szLogBuf;
  c_defaults.m_dataBufferSize = szDataBuf;
  c_defaults.m_minWriteSize = szWrite;
  c_defaults.m_maxWriteSize = maxWriteSize;
  
  { // Init all tables
    ArrayList<Table> tables(c_tablePool);
    TablePtr ptr;
    while(tables.seize(ptr)){
      new (ptr.p) Table(c_attributePool, c_fragmentPool);
    }
    tables.release();
  }

  {
    ArrayList<BackupFile> ops(c_backupFilePool);
    BackupFilePtr ptr;
    while(ops.seize(ptr)){
      new (ptr.p) BackupFile(* this, c_pagePool);
    }
    ops.release();
  }
  
  {
    ArrayList<BackupRecord> recs(c_backupPool);
    BackupRecordPtr ptr;
    while(recs.seize(ptr)){
      new (ptr.p) BackupRecord(* this, c_pagePool, c_tablePool, 
			       c_backupFilePool, c_triggerPool);
    }
    recs.release();
  }

  // Initialize BAT for interface to file system
  {
    Page32Ptr p;
    ndbrequire(c_pagePool.seizeId(p, 0));
    c_startOfPages = (Uint32 *)p.p;
    c_pagePool.release(p);
    
    NewVARIABLE* bat = allocateBat(1);
    bat[0].WA = c_startOfPages;
    bat[0].nrr = c_pagePool.getSize()*sizeof(Page32)/sizeof(Uint32);
  }
  
  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

void
Backup::execSTTOR(Signal* signal) 
{
  jamEntry();                            

  const Uint32 startphase  = signal->theData[1];
  const Uint32 typeOfStart = signal->theData[7];

  if (startphase == 3) {
    jam();
    g_TypeOfStart = typeOfStart;
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    return;
  }//if

  if(startphase == 7 && g_TypeOfStart == NodeState::ST_INITIAL_START &&
     c_masterNodeId == getOwnNodeId()){
    jam();
    createSequence(signal);
    return;
  }//if
  
  sendSTTORRY(signal);  
  return;
}//Dbdict::execSTTOR()

void
Backup::execREAD_NODESCONF(Signal* signal)
{
  jamEntry();
  ReadNodesConf * conf = (ReadNodesConf *)signal->getDataPtr();
 
  c_aliveNodes.clear();

  Uint32 count = 0;
  for (Uint32 i = 0; i<MAX_NDB_NODES; i++) {
    jam();
    if(NodeBitmask::get(conf->allNodes, i)){
      jam();
      count++;

      NodePtr node;
      ndbrequire(c_nodes.seize(node));
      
      node.p->nodeId = i;
      if(NodeBitmask::get(conf->inactiveNodes, i)) {
        jam();
	node.p->alive = 0;
      } else {
        jam();
	node.p->alive = 1;
	c_aliveNodes.set(i);
      }//if
    }//if
  }//for
  c_masterNodeId = conf->masterNodeId;
  ndbrequire(count == conf->noOfNodes);
  sendSTTORRY(signal);
}

void
Backup::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 7;
  signal->theData[6] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 7, JBB);
}

void
Backup::createSequence(Signal* signal)
{
  UtilSequenceReq * req = (UtilSequenceReq*)signal->getDataPtrSend();
  
  req->senderData  = RNIL;
  req->sequenceId  = BACKUP_SEQUENCE;
  req->requestType = UtilSequenceReq::Create;
  
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
}

void
Backup::execCONTINUEB(Signal* signal)
{
  jamEntry();
  const Uint32 Tdata0 = signal->theData[0];
  const Uint32 Tdata1 = signal->theData[1];
  const Uint32 Tdata2 = signal->theData[2];
  
  switch(Tdata0) {
  case BackupContinueB::BACKUP_FRAGMENT_INFO:
  {
    const Uint32 ptr_I = Tdata1;
    Uint32 tabPtr_I = Tdata2;
    Uint32 fragPtr_I = signal->theData[3];

    BackupRecordPtr ptr LINT_SET_PTR;
    c_backupPool.getPtr(ptr, ptr_I);

    if (tabPtr_I == RNIL)
    {
      closeFiles(signal, ptr);
      return;
    }
    jam();
    TablePtr tabPtr;
    ptr.p->tables.getPtr(tabPtr, tabPtr_I);
    jam();
    if(tabPtr.p->fragments.getSize())
    {
      FragmentPtr fragPtr;
      tabPtr.p->fragments.getPtr(fragPtr, fragPtr_I);

      BackupFilePtr filePtr;
      ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);

      const Uint32 sz = sizeof(BackupFormat::CtlFile::FragmentInfo) >> 2;
      Uint32 * dst;
      if (!filePtr.p->operation.dataBuffer.getWritePtr(&dst, sz))
      {
        sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 4);
        return;
      }

      BackupFormat::CtlFile::FragmentInfo * fragInfo = 
        (BackupFormat::CtlFile::FragmentInfo*)dst;
      fragInfo->SectionType = htonl(BackupFormat::FRAGMENT_INFO);
      fragInfo->SectionLength = htonl(sz);
      fragInfo->TableId = htonl(fragPtr.p->tableId);
      fragInfo->FragmentNo = htonl(fragPtr_I);
      fragInfo->NoOfRecordsLow = htonl(fragPtr.p->noOfRecords & 0xFFFFFFFF);
      fragInfo->NoOfRecordsHigh = htonl(fragPtr.p->noOfRecords >> 32);
      fragInfo->FilePosLow = htonl(0 & 0xFFFFFFFF);
      fragInfo->FilePosHigh = htonl(0);

      filePtr.p->operation.dataBuffer.updateWritePtr(sz);

      fragPtr_I++;
    }

    if (fragPtr_I == tabPtr.p->fragments.getSize())
    {
      signal->theData[0] = tabPtr.p->tableId;
      signal->theData[1] = 0; // unlock
      EXECUTE_DIRECT(DBDICT, GSN_BACKUP_FRAGMENT_REQ, signal, 2);

      fragPtr_I = 0;
      ptr.p->tables.next(tabPtr);
      if ((tabPtr_I = tabPtr.i) == RNIL)
      {
        closeFiles(signal, ptr);
        return;
      }
    }
    signal->theData[0] = BackupContinueB::BACKUP_FRAGMENT_INFO;
    signal->theData[1] = ptr_I;
    signal->theData[2] = tabPtr_I;
    signal->theData[3] = fragPtr_I;
    sendSignal(BACKUP_REF, GSN_CONTINUEB, signal, 4, JBB);
    return;
  }
  case BackupContinueB::START_FILE_THREAD:
  case BackupContinueB::BUFFER_UNDERFLOW:
  {
    jam();
    BackupFilePtr filePtr LINT_SET_PTR;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    checkFile(signal, filePtr);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_SCAN:
  {
    jam();
    BackupFilePtr filePtr LINT_SET_PTR;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    checkScan(signal, filePtr);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_FRAG_COMPLETE:
  {
    jam();
    BackupFilePtr filePtr LINT_SET_PTR;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    fragmentCompleted(signal, filePtr);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_META:
  {
    jam();
    BackupRecordPtr ptr LINT_SET_PTR;
    c_backupPool.getPtr(ptr, Tdata1);
    
    BackupFilePtr filePtr;
    ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
    FsBuffer & buf = filePtr.p->operation.dataBuffer;
    
    if(buf.getFreeSize() < buf.getMaxWrite()) {
      jam();
      TablePtr tabPtr LINT_SET_PTR;
      c_tablePool.getPtr(tabPtr, Tdata2);
      
      DEBUG_OUT("Backup - Buffer full - " 
                << buf.getFreeSize()
		<< " < " << buf.getMaxWrite()
                << " (sz: " << buf.getUsableSize()
                << " getMinRead: " << buf.getMinRead()
		<< ") - tableId = " << tabPtr.p->tableId);
      
      signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
      signal->theData[1] = Tdata1;
      signal->theData[2] = Tdata2;
      sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 3);
      return;
    }//if
    
    TablePtr tabPtr LINT_SET_PTR;
    c_tablePool.getPtr(tabPtr, Tdata2);
    GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->requestType = GetTabInfoReq::RequestById |
      GetTabInfoReq::LongSignalConf;
    req->tableId = tabPtr.p->tableId;
    sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal, 
	       GetTabInfoReq::SignalLength, JBB);
    return;
  }
  default:
    ndbrequire(0);
  }//switch
}

void
Backup::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();
  
  if(signal->theData[0] == 20){
    if(signal->length() > 1){
      c_defaults.m_dataBufferSize = (signal->theData[1] * 1024 * 1024);
    }
    if(signal->length() > 2){
      c_defaults.m_logBufferSize = (signal->theData[2] * 1024 * 1024);
    }
    if(signal->length() > 3){
      c_defaults.m_minWriteSize = signal->theData[3] * 1024;
    }
    if(signal->length() > 4){
      c_defaults.m_maxWriteSize = signal->theData[4] * 1024;
    }
    
    infoEvent("Backup: data: %d log: %d min: %d max: %d",
	      c_defaults.m_dataBufferSize,
	      c_defaults.m_logBufferSize,
	      c_defaults.m_minWriteSize,
	      c_defaults.m_maxWriteSize);
    return;
  }
  if(signal->theData[0] == 21){
    BackupReq * req = (BackupReq*)signal->getDataPtrSend();
    req->senderData = 23;
    req->backupDataLen = 0;
    sendSignal(BACKUP_REF, GSN_BACKUP_REQ,signal,BackupReq::SignalLength, JBB);
    startTime = NdbTick_CurrentMillisecond();
    return;
  }

  if(signal->theData[0] == 22){
    const Uint32 seq = signal->theData[1];
    FsRemoveReq * req = (FsRemoveReq *)signal->getDataPtrSend();
    req->userReference = reference();
    req->userPointer = 23;
    req->directory = 1;
    req->ownDirectory = 1;
    FsOpenReq::setVersion(req->fileNumber, 2);
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
    FsOpenReq::v2_setSequence(req->fileNumber, seq);
    FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
    sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
	       FsRemoveReq::SignalLength, JBA);
    return;
  }

  if(signal->theData[0] == 23){
    /**
     * Print records
     */
    BackupRecordPtr ptr;
    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)){
      infoEvent("BackupRecord %d: BackupId: %d MasterRef: %x ClientRef: %x",
		ptr.i, ptr.p->backupId, ptr.p->masterRef, ptr.p->clientRef);
      infoEvent(" State: %d", ptr.p->slaveState.getState());
      BackupFilePtr filePtr;
      for(ptr.p->files.first(filePtr); filePtr.i != RNIL; 
	  ptr.p->files.next(filePtr)){
	jam();
	infoEvent(" file %d: type: %d open: %d running: %d done: %d scan: %d",
		  filePtr.i, filePtr.p->fileType, filePtr.p->fileOpened,
		  filePtr.p->fileRunning, 
		  filePtr.p->fileClosing, filePtr.p->scanRunning);
      }
    }
  }
  if(signal->theData[0] == 24){
    /**
     * Print size of records etc.
     */
    infoEvent("Backup - dump pool sizes");
    infoEvent("BackupPool: %d BackupFilePool: %d TablePool: %d",
	      c_backupPool.getSize(), c_backupFilePool.getSize(), 
	      c_tablePool.getSize());
    infoEvent("AttrPool: %d TriggerPool: %d FragmentPool: %d",
	      c_backupPool.getSize(), c_backupFilePool.getSize(), 
	      c_tablePool.getSize());
    infoEvent("PagePool: %d",
	      c_pagePool.getSize());


    if(signal->getLength() == 2 && signal->theData[1] == 2424)
    {
      ndbrequire(c_tablePool.getSize() == c_tablePool.getNoOfFree());
      ndbrequire(c_attributePool.getSize() == c_attributePool.getNoOfFree());
      ndbrequire(c_backupPool.getSize() == c_backupPool.getNoOfFree());
      ndbrequire(c_backupFilePool.getSize() == c_backupFilePool.getNoOfFree());
      ndbrequire(c_pagePool.getSize() == c_pagePool.getNoOfFree());
      ndbrequire(c_fragmentPool.getSize() == c_fragmentPool.getNoOfFree());
      ndbrequire(c_triggerPool.getSize() == c_triggerPool.getNoOfFree());
    }
  }
}

bool
Backup::findTable(const BackupRecordPtr & ptr, 
		  TablePtr & tabPtr, Uint32 tableId) const
{
  for(ptr.p->tables.first(tabPtr); 
      tabPtr.i != RNIL; 
      ptr.p->tables.next(tabPtr)) {
    jam();
    if(tabPtr.p->tableId == tableId){
      jam();
      return true;
    }//if
  }//for
  tabPtr.i = RNIL;
  tabPtr.p = 0;
  return false;
}

static Uint32 xps(Uint64 x, Uint64 ms)
{
  float fx = x;
  float fs = ms;
  
  if(ms == 0 || x == 0) {
    jam();
    return 0;
  }//if
  jam();
  return ((Uint32)(1000.0f * (fx + fs/2.1f))) / ((Uint32)fs);
}

struct Number {
  Number(Uint64 r) { val = r;}
  Number & operator=(Uint64 r) { val = r; return * this; }
  Uint64 val;
};

NdbOut &
operator<< (NdbOut & out, const Number & val){
  char p = 0;
  Uint32 loop = 1;
  while(val.val > loop){
    loop *= 1000;
    p += 3;
  }
  if(loop != 1){
    p -= 3;
    loop /= 1000;
  }

  switch(p){
  case 0:
    break;
  case 3:
    p = 'k';
    break;
  case 6:
    p = 'M';
    break;
  case 9:
    p = 'G';
    break;
  default:
    p = 0;
  }
  char str[2];
  str[0] = p;
  str[1] = 0;
  Uint32 tmp = (val.val + (loop >> 1)) / loop;
#if 1
  if(p > 0)
    out << tmp << str;
  else
    out << tmp;
#else
  out << val.val;
#endif

  return out;
}

void
Backup::execBACKUP_CONF(Signal* signal)
{
  jamEntry();
  BackupConf * conf = (BackupConf*)signal->getDataPtr();
  
  ndbout_c("Backup %d has started", conf->backupId);
}

void
Backup::execBACKUP_REF(Signal* signal)
{
  jamEntry();
  BackupRef * ref = (BackupRef*)signal->getDataPtr();

  ndbout_c("Backup (%d) has NOT started %d", ref->senderData, ref->errorCode);
}

void
Backup::execBACKUP_COMPLETE_REP(Signal* signal)
{
  jamEntry();
  BackupCompleteRep* rep = (BackupCompleteRep*)signal->getDataPtr();
 
  startTime = NdbTick_CurrentMillisecond() - startTime;
  
  ndbout_c("Backup %d has completed", rep->backupId);
  const Uint64 bytes =
    rep->noOfBytesLow + (((Uint64)rep->noOfBytesHigh) << 32);
  const Uint64 records =
    rep->noOfRecordsLow + (((Uint64)rep->noOfRecordsHigh) << 32);

  Number rps = xps(records, startTime);
  Number bps = xps(bytes, startTime);

  ndbout << " Data [ "
	 << Number(records) << " rows " 
	 << Number(bytes) << " bytes " << startTime << " ms ] " 
	 << " => "
	 << rps << " row/s & " << bps << "b/s" << endl;

  bps = xps(rep->noOfLogBytes, startTime);
  rps = xps(rep->noOfLogRecords, startTime);

  ndbout << " Log [ "
	 << Number(rep->noOfLogRecords) << " log records " 
	 << Number(rep->noOfLogBytes) << " bytes " << startTime << " ms ] " 
	 << " => "
	 << rps << " records/s & " << bps << "b/s" << endl;

}

void
Backup::execBACKUP_ABORT_REP(Signal* signal)
{
  jamEntry();
  BackupAbortRep* rep = (BackupAbortRep*)signal->getDataPtr();
  
  ndbout_c("Backup %d has been aborted %d", rep->backupId, rep->reason);
}

const TriggerEvent::Value triggerEventValues[] = {
  TriggerEvent::TE_INSERT,
  TriggerEvent::TE_UPDATE,
  TriggerEvent::TE_DELETE
};

const char* triggerNameFormat[] = {
  "NDB$BACKUP_%d_%d_INSERT",
  "NDB$BACKUP_%d_%d_UPDATE",
  "NDB$BACKUP_%d_%d_DELETE"
};

const Backup::State 
Backup::validSlaveTransitions[] = {
  INITIAL,  DEFINING,
  DEFINING, DEFINED,
  DEFINED,  STARTED,
  STARTED,  STARTED, // Several START_BACKUP_REQ is sent
  STARTED,  SCANNING,
  SCANNING, STARTED,
  STARTED,  STOPPING,
  STOPPING, CLEANING,
  CLEANING, INITIAL,
  
  INITIAL,  ABORTING, // Node fail
  DEFINING, ABORTING,
  DEFINED,  ABORTING,
  STARTED,  ABORTING,
  SCANNING, ABORTING,
  STOPPING, ABORTING,
  CLEANING, ABORTING, // Node fail w/ master takeover
  ABORTING, ABORTING, // Slave who initiates ABORT should have this transition
  
  ABORTING, INITIAL,
  INITIAL,  INITIAL
};

const Uint32
Backup::validSlaveTransitionsCount = 
sizeof(Backup::validSlaveTransitions) / sizeof(Backup::State);

void
Backup::CompoundState::setState(State newState){
  bool found = false;
  const State currState = state;
  for(unsigned i = 0; i<noOfValidTransitions; i+= 2) {
    jam();
    if(validTransitions[i]   == currState &&
       validTransitions[i+1] == newState){
      jam();
      found = true;
      break;
    }
  }

  //ndbrequire(found);
  
  if (newState == INITIAL)
    abortState = INITIAL;
  if(newState == ABORTING && currState != ABORTING) {
    jam();
    abortState = currState;
  }
  state = newState;
#ifdef DEBUG_ABORT
  if (newState != currState) {
    ndbout_c("%u: Old state = %u, new state = %u, abort state = %u",
	     id, currState, newState, abortState);
  }
#endif
}

void
Backup::CompoundState::forceState(State newState)
{
  const State currState = state;
  if (newState == INITIAL)
    abortState = INITIAL;
  if(newState == ABORTING && currState != ABORTING) {
    jam();
    abortState = currState;
  }
  state = newState;
#ifdef DEBUG_ABORT
  if (newState != currState) {
    ndbout_c("%u: FORCE: Old state = %u, new state = %u, abort state = %u",
	     id, currState, newState, abortState);
  }
#endif
}

Backup::Table::Table(ArrayPool<Attribute> & ah, 
		     ArrayPool<Fragment> & fh) 
  : attributes(ah), fragments(fh)
{
  triggerIds[0] = ILLEGAL_TRIGGER_ID;
  triggerIds[1] = ILLEGAL_TRIGGER_ID;
  triggerIds[2] = ILLEGAL_TRIGGER_ID;
  triggerAllocated[0] = false;
  triggerAllocated[1] = false;
  triggerAllocated[2] = false;
}

/*****************************************************************************
 * 
 * Node state handling
 *
 *****************************************************************************/
void
Backup::execNODE_FAILREP(Signal* signal)
{
  jamEntry();

  NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();
  
  bool doStuff = false;
  /*
  Start by saving important signal data which will be destroyed before the
  process is completed.
  */
  NodeId new_master_node_id = rep->masterNodeId;
  Uint32 theFailedNodes[NodeBitmask::Size];
  for (Uint32 i = 0; i < NodeBitmask::Size; i++)
    theFailedNodes[i] = rep->theNodes[i];
  
  c_masterNodeId = new_master_node_id;

  NodePtr nodePtr;
  for(c_nodes.first(nodePtr); nodePtr.i != RNIL; c_nodes.next(nodePtr)) {
    jam();
    if(NodeBitmask::get(theFailedNodes, nodePtr.p->nodeId)){
      if(nodePtr.p->alive){
	jam();
	ndbrequire(c_aliveNodes.get(nodePtr.p->nodeId));
	doStuff = true;
      } else {
        jam();
	ndbrequire(!c_aliveNodes.get(nodePtr.p->nodeId));
      }//if
      nodePtr.p->alive = 0;
      c_aliveNodes.clear(nodePtr.p->nodeId);
    }//if
  }//for

  if(!doStuff){
    jam();
    return;
  }//if
  
#ifdef DEBUG_ABORT
  ndbout_c("****************** Node fail rep ******************");
#endif

  NodeId newCoordinator = c_masterNodeId;
  BackupRecordPtr ptr;
  for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
    jam();
    checkNodeFail(signal, ptr, newCoordinator, theFailedNodes);
  }
}

bool
Backup::verifyNodesAlive(BackupRecordPtr ptr,
			 const NdbNodeBitmask& aNodeBitMask)
{
  Uint32 version = getNodeInfo(getOwnNodeId()).m_version;
  for (Uint32 i = 0; i < MAX_NDB_NODES; i++) {
    jam();
    if(aNodeBitMask.get(i)) {
      if(!c_aliveNodes.get(i)){
        jam();
	ptr.p->setErrorCode(AbortBackupOrd::BackupFailureDueToNodeFail);
        return false;
      }//if
      if(getNodeInfo(i).m_version != version)
      {
	jam();
	ptr.p->setErrorCode(AbortBackupOrd::IncompatibleVersions);
	return false;
      }
    }//if
  }//for
  return true;
}

void
Backup::checkNodeFail(Signal* signal,
		      BackupRecordPtr ptr,
		      NodeId newCoord,
		      Uint32 theFailedNodes[NodeBitmask::Size])
{
  NdbNodeBitmask mask;
  mask.assign(2, theFailedNodes);

  /* Update ptr.p->nodes to be up to date with current alive nodes
   */
  NodePtr nodePtr;
  bool found = false;
  for(c_nodes.first(nodePtr); nodePtr.i != RNIL; c_nodes.next(nodePtr)) {
    jam();
    if(NodeBitmask::get(theFailedNodes, nodePtr.p->nodeId)) {
      jam();
      if (ptr.p->nodes.get(nodePtr.p->nodeId)) {
	jam();
	ptr.p->nodes.clear(nodePtr.p->nodeId); 
	found = true;
      }
    }//if
  }//for

  if(!found) {
    jam();
    return; // failed node is not part of backup process, safe to continue
  }

  if(mask.get(refToNode(ptr.p->masterRef)))
  {
    /**
     * Master died...abort
     */
    ptr.p->masterRef = reference();
    ptr.p->nodes.clear();
    ptr.p->nodes.set(getOwnNodeId());
    ptr.p->setErrorCode(AbortBackupOrd::BackupFailureDueToNodeFail);
    switch(ptr.p->m_gsn){
    case GSN_DEFINE_BACKUP_REQ:
    case GSN_START_BACKUP_REQ:
    case GSN_BACKUP_FRAGMENT_REQ:
    case GSN_STOP_BACKUP_REQ:
      // I'm currently processing...reply to self and abort...
      ptr.p->masterData.gsn = ptr.p->m_gsn;
      ptr.p->masterData.sendCounter = ptr.p->nodes;
      return;
    case GSN_DEFINE_BACKUP_REF:
    case GSN_DEFINE_BACKUP_CONF:
    case GSN_START_BACKUP_REF:
    case GSN_START_BACKUP_CONF:
    case GSN_BACKUP_FRAGMENT_REF:
    case GSN_BACKUP_FRAGMENT_CONF:
    case GSN_STOP_BACKUP_REF:
    case GSN_STOP_BACKUP_CONF:
      ptr.p->masterData.gsn = GSN_DEFINE_BACKUP_REQ;
      masterAbort(signal, ptr);
      return;
    case GSN_ABORT_BACKUP_ORD:
      // Already aborting
      return;
    }
  }
  else if (newCoord == getOwnNodeId())
  {
    /**
     * I'm master for this backup
     */
    jam();
    CRASH_INSERTION((10001));
#ifdef DEBUG_ABORT
    ndbout_c("**** Master: Node failed: Master id = %u", 
	     refToNode(ptr.p->masterRef));
#endif

    Uint32 gsn, len, pos;
    LINT_INIT(gsn);
    LINT_INIT(len);
    LINT_INIT(pos);
    ptr.p->nodes.bitANDC(mask);
    switch(ptr.p->masterData.gsn){
    case GSN_DEFINE_BACKUP_REQ:
    {
      DefineBackupRef * ref = (DefineBackupRef*)signal->getDataPtr();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      gsn= GSN_DEFINE_BACKUP_REF;
      len= DefineBackupRef::SignalLength;
      pos= &ref->nodeId - signal->getDataPtr();
      break;
    }
    case GSN_START_BACKUP_REQ:
    {
      StartBackupRef * ref = (StartBackupRef*)signal->getDataPtr();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      ref->signalNo = ptr.p->masterData.startBackup.signalNo;
      gsn= GSN_START_BACKUP_REF;
      len= StartBackupRef::SignalLength;
      pos= &ref->nodeId - signal->getDataPtr();
      break;
    }
    case GSN_BACKUP_FRAGMENT_REQ:
    {
      BackupFragmentRef * ref = (BackupFragmentRef*)signal->getDataPtr();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      gsn= GSN_BACKUP_FRAGMENT_REF;
      len= BackupFragmentRef::SignalLength;
      pos= &ref->nodeId - signal->getDataPtr();
      break;
    }
    case GSN_STOP_BACKUP_REQ:
    {
      StopBackupRef * ref = (StopBackupRef*)signal->getDataPtr();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      gsn= GSN_STOP_BACKUP_REF;
      len= StopBackupRef::SignalLength;
      pos= &ref->nodeId - signal->getDataPtr();
      break;
    }
    case GSN_WAIT_GCP_REQ:
    case GSN_DROP_TRIG_REQ:
    case GSN_CREATE_TRIG_REQ:
    case GSN_ALTER_TRIG_REQ:
      ptr.p->setErrorCode(AbortBackupOrd::BackupFailureDueToNodeFail);
      return;
    case GSN_UTIL_SEQUENCE_REQ:
    case GSN_UTIL_LOCK_REQ:
      return;
    default:
      ndbrequire(false);
    }
    
    for(Uint32 i = 0; (i = mask.find(i+1)) != NdbNodeBitmask::NotFound; )
    {
      signal->theData[pos] = i;
      sendSignal(reference(), gsn, signal, len, JBB);
#ifdef DEBUG_ABORT
      ndbout_c("sending %d to self from %d", gsn, i);
#endif
    }
    return;
  }//if
  
  /**
   * I abort myself as slave if not master
   */
  CRASH_INSERTION((10021));
} 

void
Backup::execINCL_NODEREQ(Signal* signal)
{
  jamEntry();
  
  const Uint32 senderRef = signal->theData[0];
  const Uint32 inclNode  = signal->theData[1];

  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)) {
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(inclNode == nodeId){
      jam();
      
      ndbrequire(node.p->alive == 0);
      ndbrequire(!c_aliveNodes.get(nodeId));
      
      node.p->alive = 1;
      c_aliveNodes.set(nodeId);
      
      break;
    }//if
  }//for
  signal->theData[0] = reference();
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 1, JBB);
}

/*****************************************************************************
 * 
 * Master functionallity - Define backup
 *
 *****************************************************************************/

void
Backup::execBACKUP_REQ(Signal* signal)
{
  jamEntry();
  BackupReq * req = (BackupReq*)signal->getDataPtr();
  
  const Uint32 senderData = req->senderData;
  const BlockReference senderRef = signal->senderBlockRef();
  const Uint32 dataLen32 = req->backupDataLen; // In 32 bit words
  const Uint32 flags = signal->getLength() > 2 ? req->flags : 2;

  if(getOwnNodeId() != getMasterNodeId()) {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData, BackupRef::IAmNotMaster);
    return;
  }//if

  if (m_diskless)
  {
    sendBackupRef(senderRef, flags, signal, senderData, 
		  BackupRef::CannotBackupDiskless);
    return;
  }
  
  if(dataLen32 != 0) {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData, 
		  BackupRef::BackupDefinitionNotImplemented);
    return;
  }//if
  
#ifdef DEBUG_ABORT
  dumpUsedResources();
#endif
  /**
   * Seize a backup record
   */
  BackupRecordPtr ptr;
  c_backups.seize(ptr);
  if(ptr.i == RNIL) {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData, BackupRef::OutOfBackupRecord);
    return;
  }//if

  ndbrequire(ptr.p->pages.empty());
  ndbrequire(ptr.p->tables.isEmpty());
  
  ptr.p->m_gsn = 0;
  ptr.p->errorCode = 0;
  ptr.p->clientRef = senderRef;
  ptr.p->clientData = senderData;
  ptr.p->flags = flags;
  ptr.p->masterRef = reference();
  ptr.p->nodes = c_aliveNodes;
  ptr.p->backupId = 0;
  ptr.p->backupKey[0] = 0;
  ptr.p->backupKey[1] = 0;
  ptr.p->backupDataLen = 0;
  ptr.p->masterData.errorCode = 0;
  ptr.p->masterData.dropTrig.tableId = RNIL;
  ptr.p->masterData.alterTrig.tableId = RNIL;
  
  UtilSequenceReq * utilReq = (UtilSequenceReq*)signal->getDataPtrSend();
    
  ptr.p->masterData.gsn = GSN_UTIL_SEQUENCE_REQ;
  utilReq->senderData  = ptr.i;
  utilReq->sequenceId  = BACKUP_SEQUENCE;
  utilReq->requestType = UtilSequenceReq::NextVal;
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
}

void
Backup::execUTIL_SEQUENCE_REF(Signal* signal)
{
  BackupRecordPtr ptr LINT_SET_PTR;
  jamEntry();
  UtilSequenceRef * utilRef = (UtilSequenceRef*)signal->getDataPtr();
  ptr.i = utilRef->senderData;
  c_backupPool.getPtr(ptr);
  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_SEQUENCE_REQ);
  sendBackupRef(signal, ptr, BackupRef::SequenceFailure);
}//execUTIL_SEQUENCE_REF()


void
Backup::sendBackupRef(Signal* signal, BackupRecordPtr ptr, Uint32 errorCode)
{
  jam();
  sendBackupRef(ptr.p->clientRef, ptr.p->flags, signal, ptr.p->clientData, errorCode);
  cleanup(signal, ptr);
}

void
Backup::sendBackupRef(BlockReference senderRef, Uint32 flags, Signal *signal,
		      Uint32 senderData, Uint32 errorCode)
{
  jam();
  if (SEND_BACKUP_STARTED_FLAG(flags))
  {
    BackupRef* ref = (BackupRef*)signal->getDataPtrSend();
    ref->senderData = senderData;
    ref->errorCode = errorCode;
    ref->masterRef = numberToRef(BACKUP, getMasterNodeId());
    sendSignal(senderRef, GSN_BACKUP_REF, signal, BackupRef::SignalLength, JBB);
  }

  if(errorCode != BackupRef::IAmNotMaster){
    signal->theData[0] = NDB_LE_BackupFailedToStart;
    signal->theData[1] = senderRef;
    signal->theData[2] = errorCode;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }
}

void
Backup::execUTIL_SEQUENCE_CONF(Signal* signal)
{
  jamEntry();

  UtilSequenceConf * conf = (UtilSequenceConf*)signal->getDataPtr();
  
  if(conf->requestType == UtilSequenceReq::Create) 
  {
    jam();
    sendSTTORRY(signal); // At startup in NDB
    return;
  }

  BackupRecordPtr ptr LINT_SET_PTR;
  ptr.i = conf->senderData;
  c_backupPool.getPtr(ptr);

  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_SEQUENCE_REQ);

  if (ptr.p->checkError())
  {
    jam();
    sendBackupRef(signal, ptr, ptr.p->errorCode);
    return;
  }//if

  if (ERROR_INSERTED(10023)) 
  {
    sendBackupRef(signal, ptr, 323);
    return;
  }//if


  {
    Uint64 backupId;
    memcpy(&backupId,conf->sequenceValue,8);
    ptr.p->backupId= (Uint32)backupId;
  }
  ptr.p->backupKey[0] = (getOwnNodeId() << 16) | (ptr.p->backupId & 0xFFFF);
  ptr.p->backupKey[1] = NdbTick_CurrentMillisecond();

  ptr.p->masterData.gsn = GSN_UTIL_LOCK_REQ;
  Mutex mutex(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
  Callback c = { safe_cast(&Backup::defineBackupMutex_locked), ptr.i };
  ndbrequire(mutex.lock(c));

  return;
}

void
Backup::defineBackupMutex_locked(Signal* signal, Uint32 ptrI, Uint32 retVal){
  jamEntry();
  ndbrequire(retVal == 0);
  
  BackupRecordPtr ptr LINT_SET_PTR;
  ptr.i = ptrI;
  c_backupPool.getPtr(ptr);
  
  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_LOCK_REQ);

  ptr.p->masterData.gsn = GSN_UTIL_LOCK_REQ;
  Mutex mutex(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
  Callback c = { safe_cast(&Backup::dictCommitTableMutex_locked), ptr.i };
  ndbrequire(mutex.lock(c));
}

void
Backup::dictCommitTableMutex_locked(Signal* signal, Uint32 ptrI,Uint32 retVal)
{
  jamEntry();
  ndbrequire(retVal == 0);
  
  /**
   * We now have both the mutexes
   */
  BackupRecordPtr ptr LINT_SET_PTR;
  ptr.i = ptrI;
  c_backupPool.getPtr(ptr);

  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_LOCK_REQ);

  if (ERROR_INSERTED(10031)) {
    ptr.p->setErrorCode(331);
  }//if

  if (ptr.p->checkError())
  {
    jam();
    
    /**
     * Unlock mutexes
     */
    jam();
    Mutex mutex1(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
    jam();
    mutex1.unlock(); // ignore response
    
    jam();
    Mutex mutex2(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
    jam();
    mutex2.unlock(); // ignore response
    
    sendBackupRef(signal, ptr, ptr.p->errorCode);
    return;
  }//if
  
  sendDefineBackupReq(signal, ptr);
}

/*****************************************************************************
 * 
 * Master functionallity - Define backup cont'd (from now on all slaves are in)
 *
 *****************************************************************************/

bool
Backup::haveAllSignals(BackupRecordPtr ptr, Uint32 gsn, Uint32 nodeId)
{ 
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == gsn);
  ndbrequire(!ptr.p->masterData.sendCounter.done());
  ndbrequire(ptr.p->masterData.sendCounter.isWaitingFor(nodeId));
  
  ptr.p->masterData.sendCounter.clearWaitingFor(nodeId);
  return ptr.p->masterData.sendCounter.done();
}

void
Backup::sendDefineBackupReq(Signal *signal, BackupRecordPtr ptr)
{
  /**
   * Sending define backup to all participants
   */
  DefineBackupReq * req = (DefineBackupReq*)signal->getDataPtrSend();
  req->backupId = ptr.p->backupId;
  req->clientRef = ptr.p->clientRef;
  req->clientData = ptr.p->clientData;
  req->senderRef = reference();
  req->backupPtr = ptr.i;
  req->backupKey[0] = ptr.p->backupKey[0];
  req->backupKey[1] = ptr.p->backupKey[1];
  req->nodes = ptr.p->nodes;
  req->backupDataLen = ptr.p->backupDataLen;
  req->flags = ptr.p->flags;
  
  ptr.p->masterData.gsn = GSN_DEFINE_BACKUP_REQ;
  ptr.p->masterData.sendCounter = ptr.p->nodes;
  NodeReceiverGroup rg(BACKUP, ptr.p->nodes);
  sendSignal(rg, GSN_DEFINE_BACKUP_REQ, signal, 
	     DefineBackupReq::SignalLength, JBB);
  
  /**
   * Now send backup data
   */
  const Uint32 len = ptr.p->backupDataLen;
  if(len == 0){
    /**
     * No data to send
     */
    jam();
    return;
  }//if
  
  /**
   * Not implemented
   */
  ndbrequire(0);
}

void
Backup::execDEFINE_BACKUP_REF(Signal* signal)
{
  jamEntry();

  DefineBackupRef* ref = (DefineBackupRef*)signal->getDataPtr();
  
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);
  
  ptr.p->setErrorCode(ref->errorCode);
  defineBackupReply(signal, ptr, nodeId);
}

void
Backup::execDEFINE_BACKUP_CONF(Signal* signal)
{
  jamEntry();

  DefineBackupConf* conf = (DefineBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  if (ERROR_INSERTED(10024))
  {
    ptr.p->setErrorCode(324);
  }

  defineBackupReply(signal, ptr, nodeId);
}

void
Backup::defineBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId)
{
  if (!haveAllSignals(ptr, GSN_DEFINE_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  /**
   * Unlock mutexes
   */
  jam();
  Mutex mutex1(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
  jam();
  mutex1.unlock(); // ignore response

  jam();
  Mutex mutex2(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
  jam();
  mutex2.unlock(); // ignore response

  if(ptr.p->checkError())
  {
    jam();
    masterAbort(signal, ptr);
    return;
  }
  
  /**
   * Reply to client
   */
  CRASH_INSERTION((10034));

  if (SEND_BACKUP_STARTED_FLAG(ptr.p->flags))
  {
    BackupConf * conf = (BackupConf*)signal->getDataPtrSend();
    conf->backupId = ptr.p->backupId;
    conf->senderData = ptr.p->clientData;
    conf->nodes = ptr.p->nodes;
    sendSignal(ptr.p->clientRef, GSN_BACKUP_CONF, signal, 
	       BackupConf::SignalLength, JBB);
  }

  signal->theData[0] = NDB_LE_BackupStarted;
  signal->theData[1] = ptr.p->clientRef;
  signal->theData[2] = ptr.p->backupId;
  ptr.p->nodes.copyto(NdbNodeBitmask::Size, signal->theData+3);
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3+NdbNodeBitmask::Size, JBB);
  
  /**
   * Prepare Trig
   */
  TablePtr tabPtr;
  ndbrequire(ptr.p->tables.first(tabPtr));
  sendCreateTrig(signal, ptr, tabPtr);
}

/*****************************************************************************
 * 
 * Master functionallity - Prepare triggers
 *
 *****************************************************************************/
void
Backup::createAttributeMask(TablePtr tabPtr, 
			    Bitmask<MAXNROFATTRIBUTESINWORDS> & mask)
{
  mask.clear();
  Table & table = * tabPtr.p;
  for(Uint32 i = 0; i<table.noOfAttributes; i++) {
    jam();
    AttributePtr attr;
    table.attributes.getPtr(attr, i);
    mask.set(i);
  }
}

void
Backup::sendCreateTrig(Signal* signal, 
			   BackupRecordPtr ptr, TablePtr tabPtr)
{
  CreateTrigReq * req =(CreateTrigReq *)signal->getDataPtrSend();
  
  ptr.p->masterData.gsn = GSN_CREATE_TRIG_REQ;
  ptr.p->masterData.sendCounter = 3;
  ptr.p->masterData.createTrig.tableId = tabPtr.p->tableId;

  req->setUserRef(reference());
  req->setConnectionPtr(ptr.i);
  req->setRequestType(CreateTrigReq::RT_USER);
  
  Bitmask<MAXNROFATTRIBUTESINWORDS> attrMask;
  createAttributeMask(tabPtr, attrMask);
  req->setAttributeMask(attrMask);
  req->setTableId(tabPtr.p->tableId);
  req->setIndexId(RNIL);        // not used
  req->setTriggerId(RNIL);      // to be created
  req->setTriggerType(TriggerType::SUBSCRIPTION);
  req->setTriggerActionTime(TriggerActionTime::TA_DETACHED);
  req->setMonitorReplicas(true);
  req->setMonitorAllAttributes(false);
  req->setOnline(false);        // leave trigger offline

  char triggerName[MAX_TAB_NAME_SIZE];
  Uint32 nameBuffer[2 + ((MAX_TAB_NAME_SIZE + 3) >> 2)];  // SP string
  LinearWriter w(nameBuffer, sizeof(nameBuffer) >> 2);
  LinearSectionPtr lsPtr[3];
  
  for (int i=0; i < 3; i++) {
    req->setTriggerEvent(triggerEventValues[i]);
    BaseString::snprintf(triggerName, sizeof(triggerName), triggerNameFormat[i],
	     ptr.p->backupId, tabPtr.p->tableId);
    w.reset();
    w.add(CreateTrigReq::TriggerNameKey, triggerName);
    lsPtr[0].p = nameBuffer;
    lsPtr[0].sz = w.getWordsUsed();
    sendSignal(DBDICT_REF, GSN_CREATE_TRIG_REQ, 
	       signal, CreateTrigReq::SignalLength, JBB, lsPtr, 1);
  }
}

void
Backup::execCREATE_TRIG_CONF(Signal* signal)
{
  jamEntry();
  CreateTrigConf * conf = (CreateTrigConf*)signal->getDataPtr();
  
  const Uint32 ptrI = conf->getConnectionPtr();
  const Uint32 tableId = conf->getTableId();
  const TriggerEvent::Value type = conf->getTriggerEvent();
  const Uint32 triggerId = conf->getTriggerId();

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  /**
   * Verify that I'm waiting for this conf
   */
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_CREATE_TRIG_REQ);
  ndbrequire(ptr.p->masterData.sendCounter.done() == false);
  ndbrequire(ptr.p->masterData.createTrig.tableId == tableId);
  
  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));
  ndbrequire(type < 3); // if some decides to change the enums

  ndbrequire(tabPtr.p->triggerIds[type] == ILLEGAL_TRIGGER_ID);
  tabPtr.p->triggerIds[type] = triggerId;
  
  createTrigReply(signal, ptr);
}

void
Backup::execCREATE_TRIG_REF(Signal* signal)
{
  CreateTrigRef* ref = (CreateTrigRef*)signal->getDataPtr();

  const Uint32 ptrI = ref->getConnectionPtr();
  const Uint32 tableId = ref->getTableId();

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  /**
   * Verify that I'm waiting for this ref
   */
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_CREATE_TRIG_REQ);
  ndbrequire(ptr.p->masterData.sendCounter.done() == false);
  ndbrequire(ptr.p->masterData.createTrig.tableId == tableId);

  ptr.p->setErrorCode(ref->getErrorCode());
  
  createTrigReply(signal, ptr);
}

void
Backup::createTrigReply(Signal* signal, BackupRecordPtr ptr)
{
  CRASH_INSERTION(10003);

  /**
   * Check finished with table
   */
  ptr.p->masterData.sendCounter--;
  if(ptr.p->masterData.sendCounter.done() == false){
    jam();
    return;
  }//if

  if (ERROR_INSERTED(10025)) 
  {
    ptr.p->errorCode = 325;
  }

  if(ptr.p->checkError()) {
    jam();
    masterAbort(signal, ptr);
    return;
  }//if

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, ptr.p->masterData.createTrig.tableId));
  
  /**
   * Next table
   */
  ptr.p->tables.next(tabPtr);
  if(tabPtr.i != RNIL){
    jam();
    sendCreateTrig(signal, ptr, tabPtr);
    return;
  }//if

  /**
   * Finished with all tables, send StartBackupReq
   */
  ptr.p->tables.first(tabPtr);
  ptr.p->masterData.startBackup.signalNo = 0;
  ptr.p->masterData.startBackup.noOfSignals = 
    (ptr.p->tables.noOfElements() + StartBackupReq::MaxTableTriggers - 1) / 
    StartBackupReq::MaxTableTriggers;
  sendStartBackup(signal, ptr, tabPtr);
}

/*****************************************************************************
 * 
 * Master functionallity - Start backup
 *
 *****************************************************************************/
void
Backup::sendStartBackup(Signal* signal, BackupRecordPtr ptr, TablePtr tabPtr)
{

  ptr.p->masterData.startBackup.tablePtr = tabPtr.i;
  
  StartBackupReq* req = (StartBackupReq*)signal->getDataPtrSend();
  req->backupId = ptr.p->backupId;
  req->backupPtr = ptr.i;
  req->signalNo = ptr.p->masterData.startBackup.signalNo;
  req->noOfSignals = ptr.p->masterData.startBackup.noOfSignals;
  Uint32 i;
  for(i = 0; i<StartBackupReq::MaxTableTriggers; i++) {
    jam();
    req->tableTriggers[i].tableId = tabPtr.p->tableId;
    req->tableTriggers[i].triggerIds[0] = tabPtr.p->triggerIds[0];
    req->tableTriggers[i].triggerIds[1] = tabPtr.p->triggerIds[1];
    req->tableTriggers[i].triggerIds[2] = tabPtr.p->triggerIds[2];
    if(!ptr.p->tables.next(tabPtr)){
      jam();
      i++;
      break;
    }//if
  }//for
  req->noOfTableTriggers = i;

  ptr.p->masterData.gsn = GSN_START_BACKUP_REQ;
  ptr.p->masterData.sendCounter = ptr.p->nodes;
  NodeReceiverGroup rg(BACKUP, ptr.p->nodes);
  sendSignal(rg, GSN_START_BACKUP_REQ, signal, 
	     StartBackupReq::HeaderLength + 
	     (i * StartBackupReq::TableTriggerLength), JBB);
}

void
Backup::execSTART_BACKUP_REF(Signal* signal)
{
  jamEntry();

  StartBackupRef* ref = (StartBackupRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 signalNo = ref->signalNo;
  const Uint32 nodeId = ref->nodeId;

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->setErrorCode(ref->errorCode);
  startBackupReply(signal, ptr, nodeId, signalNo);
}

void
Backup::execSTART_BACKUP_CONF(Signal* signal)
{
  jamEntry();
  
  StartBackupConf* conf = (StartBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 signalNo = conf->signalNo;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  startBackupReply(signal, ptr, nodeId, signalNo);
}

void
Backup::startBackupReply(Signal* signal, BackupRecordPtr ptr, 
			 Uint32 nodeId, Uint32 signalNo)
{

  CRASH_INSERTION((10004));

  ndbrequire(ptr.p->masterData.startBackup.signalNo == signalNo);
  if (!haveAllSignals(ptr, GSN_START_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  if (ERROR_INSERTED(10026))
  {
    ptr.p->errorCode = 326;
  }

  if(ptr.p->checkError()){
    jam();
    masterAbort(signal, ptr);
    return;
  }

  TablePtr tabPtr LINT_SET_PTR;
  c_tablePool.getPtr(tabPtr, ptr.p->masterData.startBackup.tablePtr);
  for(Uint32 i = 0; i<StartBackupReq::MaxTableTriggers; i++) {
    jam();
    if(!ptr.p->tables.next(tabPtr)) {
      jam();
      break;
    }//if
  }//for
  
  if(tabPtr.i != RNIL) {
    jam();
    ptr.p->masterData.startBackup.signalNo++;
    sendStartBackup(signal, ptr, tabPtr);
    return;
  }

  sendAlterTrig(signal, ptr);
}

/*****************************************************************************
 * 
 * Master functionallity - Activate triggers
 *
 *****************************************************************************/
void
Backup::sendAlterTrig(Signal* signal, BackupRecordPtr ptr)
{
  AlterTrigReq * req =(AlterTrigReq *)signal->getDataPtrSend();
  
  ptr.p->masterData.gsn = GSN_ALTER_TRIG_REQ;
  ptr.p->masterData.sendCounter = 0;
  
  req->setUserRef(reference());
  req->setConnectionPtr(ptr.i);
  req->setRequestType(AlterTrigReq::RT_USER);
  req->setTriggerInfo(0);       // not used on ALTER via DICT
  req->setOnline(true);
  req->setReceiverRef(reference());

  TablePtr tabPtr;

  if (ptr.p->masterData.alterTrig.tableId == RNIL) {
    jam();
    ptr.p->tables.first(tabPtr);
  } else {
    jam();
    ndbrequire(findTable(ptr, tabPtr, ptr.p->masterData.alterTrig.tableId));
    ptr.p->tables.next(tabPtr);
  }//if
  if (tabPtr.i != RNIL) {
    jam();
    ptr.p->masterData.alterTrig.tableId = tabPtr.p->tableId;
    req->setTableId(tabPtr.p->tableId);

    req->setTriggerId(tabPtr.p->triggerIds[0]);
    sendSignal(DBDICT_REF, GSN_ALTER_TRIG_REQ, 
	       signal, AlterTrigReq::SignalLength, JBB);
    
    req->setTriggerId(tabPtr.p->triggerIds[1]);
    sendSignal(DBDICT_REF, GSN_ALTER_TRIG_REQ, 
	       signal, AlterTrigReq::SignalLength, JBB);

    req->setTriggerId(tabPtr.p->triggerIds[2]);
    sendSignal(DBDICT_REF, GSN_ALTER_TRIG_REQ, 
	       signal, AlterTrigReq::SignalLength, JBB);

    ptr.p->masterData.sendCounter += 3;
    return;
  }//if
  ptr.p->masterData.alterTrig.tableId = RNIL;

  /**
   * Finished with all tables
   */
  ptr.p->masterData.gsn = GSN_WAIT_GCP_REQ;
  ptr.p->masterData.waitGCP.startBackup = true;
  
  WaitGCPReq * waitGCPReq = (WaitGCPReq*)signal->getDataPtrSend();
  waitGCPReq->senderRef = reference();
  waitGCPReq->senderData = ptr.i;
  waitGCPReq->requestType = WaitGCPReq::CompleteForceStart;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	     WaitGCPReq::SignalLength,JBB);
}

void
Backup::execALTER_TRIG_CONF(Signal* signal)
{
  jamEntry();

  AlterTrigConf* conf = (AlterTrigConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->getConnectionPtr();
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);
  
  alterTrigReply(signal, ptr);
}

void
Backup::execALTER_TRIG_REF(Signal* signal)
{
  jamEntry();

  AlterTrigRef* ref = (AlterTrigRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->getConnectionPtr();
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->setErrorCode(ref->getErrorCode());
  
  alterTrigReply(signal, ptr);
}

void
Backup::alterTrigReply(Signal* signal, BackupRecordPtr ptr)
{

  CRASH_INSERTION((10005));

  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_ALTER_TRIG_REQ);
  ndbrequire(ptr.p->masterData.sendCounter.done() == false);

  ptr.p->masterData.sendCounter--;

  if(ptr.p->masterData.sendCounter.done() == false){
    jam();
    return;
  }//if

  if(ptr.p->checkError()){
    jam();
    masterAbort(signal, ptr);
    return;
  }//if

  sendAlterTrig(signal, ptr);
}

void
Backup::execWAIT_GCP_REF(Signal* signal)
{
  jamEntry();
  
  CRASH_INSERTION((10006));

  WaitGCPRef * ref = (WaitGCPRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->senderData;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_WAIT_GCP_REQ);

  WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ptr.i;
  req->requestType = WaitGCPReq::CompleteForceStart;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	     WaitGCPReq::SignalLength,JBB);
}

void
Backup::execWAIT_GCP_CONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION((10007));

  WaitGCPConf * conf = (WaitGCPConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->senderData;
  const Uint32 gcp = conf->gcp;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);
  
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_WAIT_GCP_REQ);
  
  if(ptr.p->checkError()) {
    jam();
    masterAbort(signal, ptr);
    return;
  }//if
  
  if(ptr.p->masterData.waitGCP.startBackup) {
    jam();
    CRASH_INSERTION((10008));
    ptr.p->startGCP = gcp;
    ptr.p->masterData.sendCounter= 0;
    ptr.p->masterData.gsn = GSN_BACKUP_FRAGMENT_REQ;
    nextFragment(signal, ptr);
    return;
  } else {
    jam();
    if(gcp >= ptr.p->startGCP + 3)
    {
      CRASH_INSERTION((10009));
      ptr.p->stopGCP = gcp;
      sendDropTrig(signal, ptr); // regular dropping of triggers
      return;
    }//if
    
    /**
     * Make sure that we got entire stopGCP 
     */
    WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->requestType = WaitGCPReq::CompleteForceStart;
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	       WaitGCPReq::SignalLength,JBB);
    return;
  }
}

/*****************************************************************************
 * 
 * Master functionallity - Backup fragment
 *
 *****************************************************************************/
void
Backup::nextFragment(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  BackupFragmentReq* req = (BackupFragmentReq*)signal->getDataPtrSend();
  req->backupPtr = ptr.i;
  req->backupId = ptr.p->backupId;

  NodeBitmask nodes = ptr.p->nodes;
  Uint32 idleNodes = nodes.count();
  Uint32 saveIdleNodes = idleNodes;
  ndbrequire(idleNodes > 0);

  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  for(; tabPtr.i != RNIL && idleNodes > 0; ptr.p->tables.next(tabPtr)) {
    jam();
    FragmentPtr fragPtr;
    Array<Fragment> & frags = tabPtr.p->fragments;
    const Uint32 fragCount = frags.getSize();
    
    for(Uint32 i = 0; i<fragCount && idleNodes > 0; i++) {
      jam();
      tabPtr.p->fragments.getPtr(fragPtr, i);
      const Uint32 nodeId = fragPtr.p->node;
      if(fragPtr.p->scanning != 0) {
        jam();
	ndbrequire(nodes.get(nodeId));
	nodes.clear(nodeId);
	idleNodes--;
      } else if(fragPtr.p->scanned == 0 && nodes.get(nodeId)){
	jam();
	fragPtr.p->scanning = 1;
	nodes.clear(nodeId);
	idleNodes--;
	
	req->tableId = tabPtr.p->tableId;
	req->fragmentNo = i;
	req->count = 0;

	ptr.p->masterData.sendCounter++;
	const BlockReference ref = numberToRef(BACKUP, nodeId);
	sendSignal(ref, GSN_BACKUP_FRAGMENT_REQ, signal,
		   BackupFragmentReq::SignalLength, JBB);
      }//if
    }//for
  }//for
  
  if(idleNodes != saveIdleNodes){
    jam();
    return;
  }//if

  /**
   * Finished with all tables
   */
  {
    ptr.p->masterData.gsn = GSN_WAIT_GCP_REQ;
    ptr.p->masterData.waitGCP.startBackup = false;
    
    WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->requestType = WaitGCPReq::CompleteForceStart;
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	       WaitGCPReq::SignalLength, JBB);
  }
}

void
Backup::execBACKUP_FRAGMENT_CONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10010));
  
  BackupFragmentConf * conf = (BackupFragmentConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 tableId = conf->tableId;
  const Uint32 fragmentNo = conf->fragmentNo;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  const Uint64 noOfBytes =
    conf->noOfBytesLow + (((Uint64)conf->noOfBytesHigh) << 32);
  const Uint64 noOfRecords =
    conf->noOfRecordsLow + (((Uint64)conf->noOfRecordsHigh) << 32);

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->noOfBytes += noOfBytes;
  ptr.p->noOfRecords += noOfRecords;
  ptr.p->masterData.sendCounter--;

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  tabPtr.p->noOfRecords += noOfRecords;

  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragmentNo);

  fragPtr.p->noOfRecords = noOfRecords;

  ndbrequire(fragPtr.p->scanned == 0);
  ndbrequire(fragPtr.p->scanning == 1);
  ndbrequire(fragPtr.p->node == nodeId);

  fragPtr.p->scanned = 1;
  fragPtr.p->scanning = 0;

  if (ERROR_INSERTED(10028)) 
  {
    ptr.p->errorCode = 328;
  }

  if(ptr.p->checkError()) 
  {
    if(ptr.p->masterData.sendCounter.done())
    {
      jam();
      masterAbort(signal, ptr);
      return;
    }//if
  }
  else
  {
    NodeBitmask nodes = ptr.p->nodes;
    nodes.clear(getOwnNodeId());
    if (!nodes.isclear())
    {
      BackupFragmentCompleteRep *rep =
        (BackupFragmentCompleteRep*)signal->getDataPtrSend();
      rep->backupId = ptr.p->backupId;
      rep->backupPtr = ptr.i;
      rep->tableId = tableId;
      rep->fragmentNo = fragmentNo;
      rep->noOfTableRowsLow = (Uint32)(tabPtr.p->noOfRecords & 0xFFFFFFFF);
      rep->noOfTableRowsHigh = (Uint32)(tabPtr.p->noOfRecords >> 32);
      rep->noOfFragmentRowsLow = (Uint32)(noOfRecords & 0xFFFFFFFF);
      rep->noOfFragmentRowsHigh = (Uint32)(noOfRecords >> 32);
      NodeReceiverGroup rg(BACKUP, ptr.p->nodes);
      sendSignal(rg, GSN_BACKUP_FRAGMENT_COMPLETE_REP, signal,
                 BackupFragmentCompleteRep::SignalLength, JBB);
    }
    nextFragment(signal, ptr);
  }
}

void
Backup::execBACKUP_FRAGMENT_REF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10011));

  BackupFragmentRef * ref = (BackupFragmentRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  for(; tabPtr.i != RNIL; ptr.p->tables.next(tabPtr)) {
    jam();
    FragmentPtr fragPtr;
    Array<Fragment> & frags = tabPtr.p->fragments;
    const Uint32 fragCount = frags.getSize();
    
    for(Uint32 i = 0; i<fragCount; i++) {
      jam();
      tabPtr.p->fragments.getPtr(fragPtr, i);
        if(fragPtr.p->scanning != 0 && nodeId == fragPtr.p->node) 
      {
        jam();
	ndbrequire(fragPtr.p->scanned == 0);
	fragPtr.p->scanned = 1;
	fragPtr.p->scanning = 0;
	goto done;
      }
    }
  }
  goto err;

done:
  ptr.p->masterData.sendCounter--;
  ptr.p->setErrorCode(ref->errorCode);
  
  if(ptr.p->masterData.sendCounter.done())
  {
    jam();
    masterAbort(signal, ptr);
    return;
  }//if

err:
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->requestType = AbortBackupOrd::LogBufferFull;
  ord->senderData= ptr.i;
  execABORT_BACKUP_ORD(signal);
}

void
Backup::execBACKUP_FRAGMENT_COMPLETE_REP(Signal* signal)
{
  jamEntry();
  BackupFragmentCompleteRep * rep =
    (BackupFragmentCompleteRep*)signal->getDataPtr();

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, rep->backupPtr);

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, rep->tableId));

  tabPtr.p->noOfRecords =
    rep->noOfTableRowsLow + (((Uint64)rep->noOfTableRowsHigh) << 32);

  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, rep->fragmentNo);

  fragPtr.p->noOfRecords =
    rep->noOfFragmentRowsLow + (((Uint64)rep->noOfFragmentRowsHigh) << 32);
}

/*****************************************************************************
 * 
 * Master functionallity - Drop triggers
 *
 *****************************************************************************/

void
Backup::sendDropTrig(Signal* signal, BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  if (ptr.p->masterData.dropTrig.tableId == RNIL) {
    jam();
    ptr.p->tables.first(tabPtr);
  } else {
    jam();
    ndbrequire(findTable(ptr, tabPtr, ptr.p->masterData.dropTrig.tableId));
    ptr.p->tables.next(tabPtr);
  }//if
  if (tabPtr.i != RNIL) {
    jam();
    sendDropTrig(signal, ptr, tabPtr);
  } else {
    jam();
    ptr.p->masterData.dropTrig.tableId = RNIL;

    sendStopBackup(signal, ptr);
  }//if
}

void
Backup::sendDropTrig(Signal* signal, BackupRecordPtr ptr, TablePtr tabPtr)
{
  jam();
  DropTrigReq * req = (DropTrigReq *)signal->getDataPtrSend();

  ptr.p->masterData.gsn = GSN_DROP_TRIG_REQ;
  ptr.p->masterData.sendCounter = 0;
    
  req->setConnectionPtr(ptr.i);
  req->setUserRef(reference()); // Sending to myself
  req->setRequestType(DropTrigReq::RT_USER);
  req->setIndexId(RNIL);
  req->setTriggerInfo(0);       // not used on DROP via DICT

  char triggerName[MAX_TAB_NAME_SIZE];
  Uint32 nameBuffer[2 + ((MAX_TAB_NAME_SIZE + 3) >> 2)];  // SP string
  LinearWriter w(nameBuffer, sizeof(nameBuffer) >> 2);
  LinearSectionPtr lsPtr[3];
  
  ptr.p->masterData.dropTrig.tableId = tabPtr.p->tableId;
  req->setTableId(tabPtr.p->tableId);

  for (int i = 0; i < 3; i++) {
    Uint32 id = tabPtr.p->triggerIds[i];
    req->setTriggerId(id);
    if (id != ILLEGAL_TRIGGER_ID) {
      sendSignal(DBDICT_REF, GSN_DROP_TRIG_REQ, 
		 signal, DropTrigReq::SignalLength, JBB);
    } else {
      BaseString::snprintf(triggerName, sizeof(triggerName), triggerNameFormat[i],
	       ptr.p->backupId, tabPtr.p->tableId);
      w.reset();
      w.add(CreateTrigReq::TriggerNameKey, triggerName);
      lsPtr[0].p = nameBuffer;
      lsPtr[0].sz = w.getWordsUsed();
      sendSignal(DBDICT_REF, GSN_DROP_TRIG_REQ, 
		 signal, DropTrigReq::SignalLength, JBB, lsPtr, 1);
    }
    ptr.p->masterData.sendCounter ++;
  }
}

void
Backup::execDROP_TRIG_REF(Signal* signal)
{
  jamEntry();

  DropTrigRef* ref = (DropTrigRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->getConnectionPtr();
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);
 
  //ndbrequire(ref->getErrorCode() == DropTrigRef::NoSuchTrigger);
  dropTrigReply(signal, ptr);
}

void
Backup::execDROP_TRIG_CONF(Signal* signal)
{
  jamEntry();
  
  DropTrigConf* conf = (DropTrigConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->getConnectionPtr();
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);
  
  dropTrigReply(signal, ptr);
}

void
Backup::dropTrigReply(Signal* signal, BackupRecordPtr ptr)
{

  CRASH_INSERTION((10012));

  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_DROP_TRIG_REQ);
  ndbrequire(ptr.p->masterData.sendCounter.done() == false);
  
  ptr.p->masterData.sendCounter--;
  if(ptr.p->masterData.sendCounter.done() == false){
    jam();
    return;
  }//if
  
  sendDropTrig(signal, ptr); // recursive next
}

/*****************************************************************************
 * 
 * Master functionallity - Stop backup
 *
 *****************************************************************************/
void
Backup::execSTOP_BACKUP_REF(Signal* signal)
{
  jamEntry();

  StopBackupRef* ref = (StopBackupRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->setErrorCode(ref->errorCode);
  stopBackupReply(signal, ptr, nodeId);
}

void
Backup::sendStopBackup(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  StopBackupReq* stop = (StopBackupReq*)signal->getDataPtrSend();
  stop->backupPtr = ptr.i;
  stop->backupId = ptr.p->backupId;
  stop->startGCP = ptr.p->startGCP;
  stop->stopGCP = ptr.p->stopGCP;

  ptr.p->masterData.gsn = GSN_STOP_BACKUP_REQ;
  ptr.p->masterData.sendCounter = ptr.p->nodes;
  NodeReceiverGroup rg(BACKUP, ptr.p->nodes);
  sendSignal(rg, GSN_STOP_BACKUP_REQ, signal, 
	     StopBackupReq::SignalLength, JBB);
}

void
Backup::execSTOP_BACKUP_CONF(Signal* signal)
{
  jamEntry();
  
  StopBackupConf* conf = (StopBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->noOfLogBytes += conf->noOfLogBytes;
  ptr.p->noOfLogRecords += conf->noOfLogRecords;
  
  stopBackupReply(signal, ptr, nodeId);
}

void
Backup::stopBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId)
{
  CRASH_INSERTION((10013));

  if (!haveAllSignals(ptr, GSN_STOP_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  sendAbortBackupOrd(signal, ptr, AbortBackupOrd::BackupComplete);
  
  if(!ptr.p->checkError())
  {
    if (SEND_BACKUP_COMPLETED_FLAG(ptr.p->flags))
    {
      BackupCompleteRep * rep = (BackupCompleteRep*)signal->getDataPtrSend();
      rep->backupId = ptr.p->backupId;
      rep->senderData = ptr.p->clientData;
      rep->startGCP = ptr.p->startGCP;
      rep->stopGCP = ptr.p->stopGCP;
      rep->noOfBytesLow = (Uint32)(ptr.p->noOfBytes & 0xFFFFFFFF);
      rep->noOfRecordsLow = (Uint32)(ptr.p->noOfRecords & 0xFFFFFFFF);
      rep->noOfBytesHigh = (Uint32)(ptr.p->noOfBytes >> 32);
      rep->noOfRecordsHigh = (Uint32)(ptr.p->noOfRecords >> 32);
      rep->noOfLogBytes = ptr.p->noOfLogBytes;
      rep->noOfLogRecords = ptr.p->noOfLogRecords;
      rep->nodes = ptr.p->nodes;
      sendSignal(ptr.p->clientRef, GSN_BACKUP_COMPLETE_REP, signal,
		 BackupCompleteRep::SignalLength, JBB);
    }

    signal->theData[0] = NDB_LE_BackupCompleted;
    signal->theData[1] = ptr.p->clientRef;
    signal->theData[2] = ptr.p->backupId;
    signal->theData[3] = ptr.p->startGCP;
    signal->theData[4] = ptr.p->stopGCP;
    signal->theData[5] = (Uint32)(ptr.p->noOfBytes & 0xFFFFFFFF);
    signal->theData[6] = (Uint32)(ptr.p->noOfRecords & 0xFFFFFFFF);
    signal->theData[7] = ptr.p->noOfLogBytes;
    signal->theData[8] = ptr.p->noOfLogRecords;
    ptr.p->nodes.copyto(NdbNodeBitmask::Size, signal->theData+9);
    signal->theData[9+NdbNodeBitmask::Size] = (Uint32)(ptr.p->noOfBytes >> 32);
    signal->theData[10+NdbNodeBitmask::Size] = (Uint32)(ptr.p->noOfRecords >> 32);
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 11+NdbNodeBitmask::Size, JBB);
  }
  else
  {
    masterAbort(signal, ptr);
  }
}

/*****************************************************************************
 * 
 * Master functionallity - Abort backup
 *
 *****************************************************************************/
void
Backup::masterAbort(Signal* signal, BackupRecordPtr ptr)
{
  jam();
#ifdef DEBUG_ABORT
  ndbout_c("************ masterAbort");
#endif
  if(ptr.p->masterData.errorCode != 0)
  {
    jam();
    return;
  }

  if (SEND_BACKUP_COMPLETED_FLAG(ptr.p->flags))
  {
    BackupAbortRep* rep = (BackupAbortRep*)signal->getDataPtrSend();
    rep->backupId = ptr.p->backupId;
    rep->senderData = ptr.p->clientData;
    rep->reason = ptr.p->errorCode;
    sendSignal(ptr.p->clientRef, GSN_BACKUP_ABORT_REP, signal, 
	       BackupAbortRep::SignalLength, JBB);
  }
  signal->theData[0] = NDB_LE_BackupAborted;
  signal->theData[1] = ptr.p->clientRef;
  signal->theData[2] = ptr.p->backupId;
  signal->theData[3] = ptr.p->errorCode;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  ndbrequire(ptr.p->errorCode);
  ptr.p->masterData.errorCode = ptr.p->errorCode;

  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->senderData= ptr.i;
  NodeReceiverGroup rg(BACKUP, ptr.p->nodes);
  
  switch(ptr.p->masterData.gsn){
  case GSN_DEFINE_BACKUP_REQ:
    ord->requestType = AbortBackupOrd::BackupFailure;
    sendSignal(rg, GSN_ABORT_BACKUP_ORD, signal, 
	       AbortBackupOrd::SignalLength, JBB);
    return;
  case GSN_CREATE_TRIG_REQ:
  case GSN_START_BACKUP_REQ:
  case GSN_ALTER_TRIG_REQ:
  case GSN_WAIT_GCP_REQ:
  case GSN_BACKUP_FRAGMENT_REQ:
    jam();
    ptr.p->stopGCP= ptr.p->startGCP + 1;
    sendDropTrig(signal, ptr); // dropping due to error
    return;
  case GSN_UTIL_SEQUENCE_REQ:
  case GSN_UTIL_LOCK_REQ:
  case GSN_DROP_TRIG_REQ:
    ndbrequire(false);
    return;
  case GSN_STOP_BACKUP_REQ:
    return;
  }
}

void
Backup::abort_scan(Signal * signal, BackupRecordPtr ptr)
{
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->senderData= ptr.i;
  ord->requestType = AbortBackupOrd::AbortScan;

  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  for(; tabPtr.i != RNIL; ptr.p->tables.next(tabPtr)) {
    jam();
    FragmentPtr fragPtr;
    Array<Fragment> & frags = tabPtr.p->fragments;
    const Uint32 fragCount = frags.getSize();
    
    for(Uint32 i = 0; i<fragCount; i++) {
      jam();
      tabPtr.p->fragments.getPtr(fragPtr, i);
      const Uint32 nodeId = fragPtr.p->node;
      if(fragPtr.p->scanning != 0 && ptr.p->nodes.get(nodeId)) {
        jam();
	
	const BlockReference ref = numberToRef(BACKUP, nodeId);
	sendSignal(ref, GSN_ABORT_BACKUP_ORD, signal,
		   AbortBackupOrd::SignalLength, JBB);
	
      }
    }
  }
}

/*****************************************************************************
 * 
 * Slave functionallity: Define Backup 
 *
 *****************************************************************************/
void
Backup::defineBackupRef(Signal* signal, BackupRecordPtr ptr, Uint32 errCode)
{
  ptr.p->m_gsn = GSN_DEFINE_BACKUP_REF;
  ptr.p->setErrorCode(errCode);
  ndbrequire(ptr.p->errorCode != 0);
  
  DefineBackupRef* ref = (DefineBackupRef*)signal->getDataPtrSend();
  ref->backupId = ptr.p->backupId;
  ref->backupPtr = ptr.i;
  ref->errorCode = ptr.p->errorCode;
  ref->nodeId = getOwnNodeId();
  sendSignal(ptr.p->masterRef, GSN_DEFINE_BACKUP_REF, signal, 
	     DefineBackupRef::SignalLength, JBB);
}

void
Backup::execDEFINE_BACKUP_REQ(Signal* signal)
{
  jamEntry();

  DefineBackupReq* req = (DefineBackupReq*)signal->getDataPtr();
  
  BackupRecordPtr ptr LINT_SET_PTR;
  const Uint32 ptrI = req->backupPtr;
  const Uint32 backupId = req->backupId;
  const BlockReference senderRef = req->senderRef;

  if(senderRef == reference()){
    /**
     * Signal sent from myself -> record already seized
     */
    jam();
    c_backupPool.getPtr(ptr, ptrI);
  } else { // from other node
    jam();
#ifdef DEBUG_ABORT
    dumpUsedResources();
#endif
    if(!c_backups.seizeId(ptr, ptrI)) {
      jam();
      ndbrequire(false); // If master has succeeded slave should succed
    }//if
  }//if

  CRASH_INSERTION((10014));
  
  ptr.p->m_gsn = GSN_DEFINE_BACKUP_REQ;
  ptr.p->slaveState.forceState(INITIAL);
  ptr.p->slaveState.setState(DEFINING);
  ptr.p->errorCode = 0;
  ptr.p->clientRef = req->clientRef;
  ptr.p->clientData = req->clientData;
  if(senderRef == reference())
    ptr.p->flags = req->flags;
  else
    ptr.p->flags = req->flags & ~((Uint32)0x3); /* remove waitCompleted flags
						 * as non master should never
						 * reply
						 */
  ptr.p->masterRef = senderRef;
  ptr.p->nodes = req->nodes;
  ptr.p->backupId = backupId;
  ptr.p->backupKey[0] = req->backupKey[0];
  ptr.p->backupKey[1] = req->backupKey[1];
  ptr.p->backupDataLen = req->backupDataLen;
  ptr.p->masterData.dropTrig.tableId = RNIL;
  ptr.p->masterData.alterTrig.tableId = RNIL;
  ptr.p->masterData.errorCode = 0;
  ptr.p->noOfBytes = 0;
  ptr.p->noOfRecords = 0;
  ptr.p->noOfLogBytes = 0;
  ptr.p->noOfLogRecords = 0;
  ptr.p->currGCP = 0;
  
  /**
   * Allocate files
   */
  BackupFilePtr files[3];
  Uint32 noOfPages[] = {
    NO_OF_PAGES_META_FILE,
    2,   // 32k
    0    // 3M
  };
  const Uint32 maxInsert[] = {
    MAX_WORDS_META_FILE,
    4096,    // 16k
    16*3000, // Max 16 tuples
  };
  Uint32 minWrite[] = {
    8192,
    8192,
    32768
  };
  Uint32 maxWrite[] = {
    8192,
    8192,
    32768
  };
  
  minWrite[1] = c_defaults.m_minWriteSize;
  maxWrite[1] = c_defaults.m_maxWriteSize;
  noOfPages[1] = (c_defaults.m_logBufferSize + sizeof(Page32) - 1) / 
    sizeof(Page32);
  minWrite[2] = c_defaults.m_minWriteSize;
  maxWrite[2] = c_defaults.m_maxWriteSize;
  noOfPages[2] = (c_defaults.m_dataBufferSize + sizeof(Page32) - 1) / 
    sizeof(Page32);
  
  for(Uint32 i = 0; i<3; i++) {
    jam();
    if(!ptr.p->files.seize(files[i])) {
      jam();
      defineBackupRef(signal, ptr, 
		      DefineBackupRef::FailedToAllocateFileRecord);
      return;
    }//if

    files[i].p->tableId = RNIL;
    files[i].p->backupPtr = ptr.i;
    files[i].p->filePointer = RNIL;
    files[i].p->fileClosing = 0;
    files[i].p->fileOpened = 0;
    files[i].p->fileRunning = 0;    
    files[i].p->scanRunning = 0;
    files[i].p->errorCode = 0;
    
    if(files[i].p->pages.seize(noOfPages[i]) == false) {
      jam();
      DEBUG_OUT("Failed to seize " << noOfPages[i] << " pages");
      defineBackupRef(signal, ptr, DefineBackupRef::FailedToAllocateBuffers);
      return;
    }//if
    Page32Ptr pagePtr;
    files[i].p->pages.getPtr(pagePtr, 0);
    
    const char * msg = files[i].p->
      operation.dataBuffer.setup((Uint32*)pagePtr.p, 
				 noOfPages[i] * (sizeof(Page32) >> 2),
				 128,
				 minWrite[i] >> 2,
				 maxWrite[i] >> 2,
				 maxInsert[i]);
    if(msg != 0) {
      jam();
      defineBackupRef(signal, ptr, DefineBackupRef::FailedToSetupFsBuffers);
      return;
    }//if
  }//for
  files[0].p->fileType = BackupFormat::CTL_FILE;
  files[1].p->fileType = BackupFormat::LOG_FILE;
  files[2].p->fileType = BackupFormat::DATA_FILE;
  
  ptr.p->ctlFilePtr = files[0].i;
  ptr.p->logFilePtr = files[1].i;
  ptr.p->dataFilePtr = files[2].i;
  
  if (!verifyNodesAlive(ptr, ptr.p->nodes)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::Undefined);
    return;
  }//if
  if (ERROR_INSERTED(10027)) {
    jam();
    defineBackupRef(signal, ptr, 327);
    return;
  }//if

  if(ptr.p->backupDataLen == 0) {
    jam();
    backupAllData(signal, ptr);
    return;
  }//if
  
  /**
   * Not implemented
   */
  ndbrequire(0);
}

void
Backup::backupAllData(Signal* signal, BackupRecordPtr ptr)
{
  /**
   * Get all tables from dict
   */
  ListTablesReq * req = (ListTablesReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ptr.i;
  req->requestData = 0;
  sendSignal(DBDICT_REF, GSN_LIST_TABLES_REQ, signal, 
	     ListTablesReq::SignalLength, JBB);
}

void
Backup::execLIST_TABLES_CONF(Signal* signal)
{
  jamEntry();
  
  ListTablesConf* conf = (ListTablesConf*)signal->getDataPtr();

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, conf->senderData);
  
  const Uint32 len = signal->length() - ListTablesConf::HeaderLength;
  for(unsigned int i = 0; i<len; i++) {
    jam();
    Uint32 tableId = ListTablesConf::getTableId(conf->tableData[i]);
    Uint32 tableType = ListTablesConf::getTableType(conf->tableData[i]);
    Uint32 state= ListTablesConf::getTableState(conf->tableData[i]);
    if (!DictTabInfo::isTable(tableType) && !DictTabInfo::isIndex(tableType)){
      jam();
      continue;
    }//if
    if (state != DictTabInfo::StateOnline)
    {
      jam();
      continue;
    }//if
    TablePtr tabPtr;
    ptr.p->tables.seize(tabPtr);
    if(tabPtr.i == RNIL) {
      jam();
      defineBackupRef(signal, ptr, DefineBackupRef::FailedToAllocateTables);
      return;
    }//if
    tabPtr.p->tableId = tableId;
    tabPtr.p->tableType = tableType;
  }//for
  
  if(len == ListTablesConf::DataLength) {
    jam();
    /**
     * Not finished...
     */
    return;
  }//if

  /**
   * All tables fetched
   */
  openFiles(signal, ptr);
}

void
Backup::openFiles(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  BackupFilePtr filePtr LINT_SET_PTR;

  FsOpenReq * req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = 
    FsOpenReq::OM_WRITEONLY | 
    FsOpenReq::OM_TRUNCATE |
    FsOpenReq::OM_CREATE | 
    FsOpenReq::OM_APPEND |
    FsOpenReq::OM_SYNC;
  FsOpenReq::v2_setCount(req->fileNumber, 0xFFFFFFFF);
  
  /**
   * Ctl file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->ctlFilePtr);
  ndbrequire(filePtr.p->fileRunning == 0);
  filePtr.p->fileRunning = 1;

  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);

  /**
   * Log file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->logFilePtr);
  ndbrequire(filePtr.p->fileRunning == 0);
  filePtr.p->fileRunning = 1;
  
  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_LOG);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);

  /**
   * Data file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);
  ndbrequire(filePtr.p->fileRunning == 0);
  filePtr.p->fileRunning = 1;

  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  FsOpenReq::v2_setCount(req->fileNumber, 0);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Backup::execFSOPENREF(Signal* signal)
{
  jamEntry();

  FsRef * ref = (FsRef *)signal->getDataPtr();
  
  const Uint32 userPtr = ref->userPointer;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, userPtr);
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  ptr.p->setErrorCode(ref->errorCode);
  openFilesReply(signal, ptr, filePtr);
}

void
Backup::execFSOPENCONF(Signal* signal)
{
  jamEntry();
  
  FsConf * conf = (FsConf *)signal->getDataPtr();
  
  const Uint32 userPtr = conf->userPointer;
  const Uint32 filePointer = conf->filePointer;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, userPtr);
  filePtr.p->filePointer = filePointer; 
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  ndbrequire(filePtr.p->fileOpened == 0);
  filePtr.p->fileOpened = 1;
  openFilesReply(signal, ptr, filePtr);
}

void
Backup::openFilesReply(Signal* signal, 
		       BackupRecordPtr ptr, BackupFilePtr filePtr)
{
  jam();

  /**
   * Mark files as "opened"
   */
  ndbrequire(filePtr.p->fileRunning == 1);
  filePtr.p->fileRunning = 0;
  
  /**
   * Check if all files have recived open_reply
   */
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL;ptr.p->files.next(filePtr)) 
  {
    jam();
    if(filePtr.p->fileRunning == 1) {
      jam();
      return;
    }//if
  }//for

  /**
   * Did open succeed for all files
   */
  if(ptr.p->checkError()) {
    jam();
    defineBackupRef(signal, ptr);
    return;
  }//if

  /**
   * Insert file headers
   */
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  if(!insertFileHeader(BackupFormat::CTL_FILE, ptr.p, filePtr.p)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
    return;
  }//if

  ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
  if(!insertFileHeader(BackupFormat::LOG_FILE, ptr.p, filePtr.p)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
    return;
  }//if

  ptr.p->files.getPtr(filePtr, ptr.p->dataFilePtr);
  if(!insertFileHeader(BackupFormat::DATA_FILE, ptr.p, filePtr.p)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
    return;
  }//if

  /**
   * Start CTL file thread
   */
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  filePtr.p->fileRunning = 1;
  
  signal->theData[0] = BackupContinueB::START_FILE_THREAD;
  signal->theData[1] = ptr.p->ctlFilePtr;
  sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 2);

  /**
   * Insert table list in ctl file
   */
  FsBuffer & buf = filePtr.p->operation.dataBuffer;

  const Uint32 sz = 
    (sizeof(BackupFormat::CtlFile::TableList) >> 2) +
    ptr.p->tables.noOfElements() - 1;
  
  Uint32 * dst;
  ndbrequire(sz < buf.getMaxWrite());
  if(!buf.getWritePtr(&dst, sz)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertTableList);
    return;
  }//if
  
  BackupFormat::CtlFile::TableList* tl = 
    (BackupFormat::CtlFile::TableList*)dst;
  tl->SectionType   = htonl(BackupFormat::TABLE_LIST);
  tl->SectionLength = htonl(sz);

  TablePtr tabPtr;
  Uint32 count = 0;
  for(ptr.p->tables.first(tabPtr); 
      tabPtr.i != RNIL;
      ptr.p->tables.next(tabPtr)){
    jam();
    tl->TableIds[count] = htonl(tabPtr.p->tableId);
    count++;
  }//for
  
  buf.updateWritePtr(sz);
  
  /**
   * Start getting table definition data
   */
  ndbrequire(ptr.p->tables.first(tabPtr));

  signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
  signal->theData[1] = ptr.i;
  signal->theData[2] = tabPtr.i;
  sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 3);
  return;
}

bool
Backup::insertFileHeader(BackupFormat::FileType ft, 
			 BackupRecord * ptrP,
			 BackupFile * filePtrP){
  FsBuffer & buf = filePtrP->operation.dataBuffer;

  const Uint32 sz = sizeof(BackupFormat::FileHeader) >> 2;

  Uint32 * dst;
  ndbrequire(sz < buf.getMaxWrite());
  if(!buf.getWritePtr(&dst, sz)) {
    jam();
    return false;
  }//if
  
  BackupFormat::FileHeader* header = (BackupFormat::FileHeader*)dst;
  ndbrequire(sizeof(header->Magic) == sizeof(BACKUP_MAGIC));
  memcpy(header->Magic, BACKUP_MAGIC, sizeof(BACKUP_MAGIC));
  header->NdbVersion    = htonl(NDB_VERSION);
  header->SectionType   = htonl(BackupFormat::FILE_HEADER);
  header->SectionLength = htonl(sz - 3);
  header->FileType      = htonl(ft);
  header->BackupId      = htonl(ptrP->backupId);
  header->BackupKey_0   = htonl(ptrP->backupKey[0]);
  header->BackupKey_1   = htonl(ptrP->backupKey[1]);
  header->ByteOrder     = 0x12345678;
  
  buf.updateWritePtr(sz);
  return true;
}

void
Backup::execGET_TABINFOREF(Signal* signal)
{
  GetTabInfoRef * ref = (GetTabInfoRef*)signal->getDataPtr();
  
  const Uint32 senderData = ref->senderData;
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, senderData);

  defineBackupRef(signal, ptr, ref->errorCode);
}

void
Backup::execGET_TABINFO_CONF(Signal* signal)
{
  jamEntry();

  if(!assembleFragments(signal)) {
    jam();
    return;
  }//if

  GetTabInfoConf * const conf = (GetTabInfoConf*)signal->getDataPtr();
  //const Uint32 senderRef = info->senderRef;
  const Uint32 len = conf->totalLen;
  const Uint32 senderData = conf->senderData;

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, senderData);
  
  SegmentedSectionPtr dictTabInfoPtr;
  signal->getSection(dictTabInfoPtr, GetTabInfoConf::DICT_TAB_INFO);
  ndbrequire(dictTabInfoPtr.sz == len);

  /**
   * No of pages needed
   */
  const Uint32 noPages = (len + sizeof(Page32) - 1) / sizeof(Page32);
  if(ptr.p->pages.getSize() < noPages) {
    jam();
    ptr.p->pages.release();
    if(ptr.p->pages.seize(noPages) == false) {
      jam();
      ptr.p->setErrorCode(DefineBackupRef::FailedAllocateTableMem);
      ndbrequire(false);
      releaseSections(signal);
      defineBackupRef(signal, ptr);
      return;
    }//if
  }//if
  
  BackupFilePtr filePtr;
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  FsBuffer & buf = filePtr.p->operation.dataBuffer;
  { // Write into ctl file
    Uint32* dst, dstLen = len + 2;
    if(!buf.getWritePtr(&dst, dstLen)) {
      jam();
      ndbrequire(false);
      ptr.p->setErrorCode(DefineBackupRef::FailedAllocateTableMem);
      releaseSections(signal);
      defineBackupRef(signal, ptr);
      return;
    }//if
    if(dst != 0) {
      jam();

      BackupFormat::CtlFile::TableDescription * desc = 
        (BackupFormat::CtlFile::TableDescription*)dst;
      desc->SectionType = htonl(BackupFormat::TABLE_DESCRIPTION);
      desc->SectionLength = htonl(len + 2);
      dst += 2;

      copy(dst, dictTabInfoPtr);
      buf.updateWritePtr(dstLen);
    }//if
  }
  
  ndbrequire(ptr.p->pages.getSize() >= noPages);
  Page32Ptr pagePtr;
  ptr.p->pages.getPtr(pagePtr, 0);
  copy(&pagePtr.p->data[0], dictTabInfoPtr);
  releaseSections(signal);
  
  if(ptr.p->checkError()) {
    jam();
    defineBackupRef(signal, ptr);
    return;
  }//if

  TablePtr tabPtr = parseTableDescription(signal, ptr, len);
  if(tabPtr.i == RNIL) {
    jam();
    defineBackupRef(signal, ptr);
    return;
  }//if

  TablePtr tmp = tabPtr;
  ptr.p->tables.next(tabPtr);
  if(DictTabInfo::isIndex(tmp.p->tableType))
  {
    jam();
    ptr.p->tables.release(tmp);
  }
  else
  {
    jam();
    signal->theData[0] = tmp.p->tableId;
    signal->theData[1] = 1; // lock
    EXECUTE_DIRECT(DBDICT, GSN_BACKUP_FRAGMENT_REQ, signal, 2);
  }

  if(tabPtr.i == RNIL) {
    jam();
    
    ptr.p->pages.release();
    
    ndbrequire(ptr.p->tables.first(tabPtr));
    signal->theData[0] = RNIL;
    signal->theData[1] = tabPtr.p->tableId;
    signal->theData[2] = ptr.i;
    sendSignal(DBDIH_REF, GSN_DI_FCOUNTREQ, signal, 3, JBB);
    return;
  }//if

  signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
  signal->theData[1] = ptr.i;
  signal->theData[2] = tabPtr.i;
  sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 3);
  return;
}

Backup::TablePtr
Backup::parseTableDescription(Signal* signal, BackupRecordPtr ptr, Uint32 len)
{

  Page32Ptr pagePtr;
  ptr.p->pages.getPtr(pagePtr, 0);
  
  SimplePropertiesLinearReader it(&pagePtr.p->data[0], len);
  
  it.first();
  
  DictTabInfo::Table tmpTab; tmpTab.init();
  SimpleProperties::UnpackStatus stat;
  stat = SimpleProperties::unpack(it, &tmpTab, 
				  DictTabInfo::TableMapping, 
				  DictTabInfo::TableMappingSize, 
				  true, true);
  ndbrequire(stat == SimpleProperties::Break);
  
  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tmpTab.TableId));
  if(DictTabInfo::isIndex(tabPtr.p->tableType)){
    jam();
    return tabPtr;
  }
  
  /**
   * Initialize table object
   */
  tabPtr.p->noOfRecords = 0;
  tabPtr.p->schemaVersion = tmpTab.TableVersion;
  tabPtr.p->noOfAttributes = tmpTab.NoOfAttributes;
  tabPtr.p->noOfNull = 0;
  tabPtr.p->noOfVariable = 0; // Computed while iterating over attribs
  tabPtr.p->sz_FixedAttributes = 0; // Computed while iterating over attribs
  tabPtr.p->triggerIds[0] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerIds[1] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerIds[2] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerAllocated[0] = false;
  tabPtr.p->triggerAllocated[1] = false;
  tabPtr.p->triggerAllocated[2] = false;

  if(tabPtr.p->attributes.seize(tabPtr.p->noOfAttributes) == false) {
    jam();
    ptr.p->setErrorCode(DefineBackupRef::FailedToAllocateAttributeRecord);
    tabPtr.i = RNIL;
    return tabPtr;
  }//if
  
  const Uint32 count = tabPtr.p->noOfAttributes;
  for(Uint32 i = 0; i<count; i++) {
    jam();
    DictTabInfo::Attribute tmp; tmp.init();
    stat = SimpleProperties::unpack(it, &tmp, 
				    DictTabInfo::AttributeMapping, 
				    DictTabInfo::AttributeMappingSize,
				    true, true);
    
    ndbrequire(stat == SimpleProperties::Break);

    const Uint32 arr = tmp.AttributeArraySize;
    const Uint32 sz = 1 << tmp.AttributeSize;
    const Uint32 sz32 = (sz * arr + 31) >> 5;

    AttributePtr attrPtr;
    tabPtr.p->attributes.getPtr(attrPtr, tmp.AttributeId);
    
    attrPtr.p->data.nullable = tmp.AttributeNullableFlag;
    attrPtr.p->data.fixed = (tmp.AttributeArraySize != 0);
    attrPtr.p->data.sz32 = sz32;

    /**
     * Either
     * 1) Fixed
     * 2) Nullable
     * 3) Variable
     */
    if(attrPtr.p->data.fixed == true && attrPtr.p->data.nullable == false) {
      jam();
      attrPtr.p->data.offset = tabPtr.p->sz_FixedAttributes;
      tabPtr.p->sz_FixedAttributes += sz32;
    }//if
    
    if(attrPtr.p->data.fixed == true && attrPtr.p->data.nullable == true) {
      jam();
      attrPtr.p->data.offset = 0;
      
      attrPtr.p->data.offsetNull = tabPtr.p->noOfNull;
      tabPtr.p->noOfNull++;
      tabPtr.p->noOfVariable++;
    }//if
    
    if(attrPtr.p->data.fixed == false) {
      jam();
      tabPtr.p->noOfVariable++;
      ndbrequire(0);
    }//if
    
    it.next(); // Move Past EndOfAttribute
  }//for
  return tabPtr;
}

void
Backup::execDI_FCOUNTCONF(Signal* signal)
{
  jamEntry();
  
  const Uint32 userPtr = signal->theData[0];
  const Uint32 fragCount = signal->theData[1];
  const Uint32 tableId = signal->theData[2];
  const Uint32 senderData = signal->theData[3];

  ndbrequire(userPtr == RNIL && signal->length() == 5);
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, senderData);

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));
  
  ndbrequire(tabPtr.p->fragments.seize(fragCount) != false);
  for(Uint32 i = 0; i<fragCount; i++) {
    jam();
    FragmentPtr fragPtr;
    tabPtr.p->fragments.getPtr(fragPtr, i);
    fragPtr.p->scanned = 0;
    fragPtr.p->scanning = 0;
    fragPtr.p->tableId = tableId;
    fragPtr.p->node = 0;
  }//for
  
  /**
   * Next table
   */
  if(ptr.p->tables.next(tabPtr)) {
    jam();
    signal->theData[0] = RNIL;
    signal->theData[1] = tabPtr.p->tableId;
    signal->theData[2] = ptr.i;
    sendSignal(DBDIH_REF, GSN_DI_FCOUNTREQ, signal, 3, JBB);    
    return;
  }//if
  
  ptr.p->tables.first(tabPtr);
  getFragmentInfo(signal, ptr, tabPtr, 0);
}

void
Backup::getFragmentInfo(Signal* signal, 
			BackupRecordPtr ptr, TablePtr tabPtr, Uint32 fragNo)
{
  jam();
  
  for(; tabPtr.i != RNIL; ptr.p->tables.next(tabPtr)) {
    jam();
    const Uint32 fragCount = tabPtr.p->fragments.getSize();
    for(; fragNo < fragCount; fragNo ++) {
      jam();
      FragmentPtr fragPtr;
      tabPtr.p->fragments.getPtr(fragPtr, fragNo);
      
      if(fragPtr.p->scanned == 0 && fragPtr.p->scanning == 0) {
	jam();
	signal->theData[0] = RNIL;
	signal->theData[1] = ptr.i;
	signal->theData[2] = tabPtr.p->tableId;
	signal->theData[3] = fragNo;
	sendSignal(DBDIH_REF, GSN_DIGETPRIMREQ, signal, 4, JBB);
	return;
      }//if
    }//for
    fragNo = 0;
  }//for
  
  getFragmentInfoDone(signal, ptr);
}

void
Backup::execDIGETPRIMCONF(Signal* signal)
{
  jamEntry();
  
  const Uint32 userPtr = signal->theData[0];
  const Uint32 senderData = signal->theData[1];
  const Uint32 nodeCount = signal->theData[6];
  const Uint32 tableId = signal->theData[7];
  const Uint32 fragNo = signal->theData[8];

  ndbrequire(userPtr == RNIL && signal->length() == 9);
  ndbrequire(nodeCount > 0 && nodeCount <= MAX_REPLICAS);
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, senderData);

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragNo);

  fragPtr.p->node = signal->theData[2];

  getFragmentInfo(signal, ptr, tabPtr, fragNo + 1);
}

void
Backup::getFragmentInfoDone(Signal* signal, BackupRecordPtr ptr)
{
  ptr.p->m_gsn = GSN_DEFINE_BACKUP_CONF;
  ptr.p->slaveState.setState(DEFINED);
  DefineBackupConf * conf = (DefineBackupConf*)signal->getDataPtr();
  conf->backupPtr = ptr.i;
  conf->backupId = ptr.p->backupId;
  sendSignal(ptr.p->masterRef, GSN_DEFINE_BACKUP_CONF, signal,
	     DefineBackupConf::SignalLength, JBB);
}


/*****************************************************************************
 * 
 * Slave functionallity: Start backup
 *
 *****************************************************************************/
void
Backup::execSTART_BACKUP_REQ(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10015));
  
  StartBackupReq* req = (StartBackupReq*)signal->getDataPtr();
  const Uint32 ptrI = req->backupPtr;
  //const Uint32 backupId = req->backupId;
  const Uint32 signalNo = req->signalNo;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);
  
  ptr.p->slaveState.setState(STARTED);
  ptr.p->m_gsn = GSN_START_BACKUP_REQ;

  for(Uint32 i = 0; i<req->noOfTableTriggers; i++) {
    jam();
    TablePtr tabPtr;
    ndbrequire(findTable(ptr, tabPtr, req->tableTriggers[i].tableId));
    for(Uint32 j = 0; j<3; j++) {
      jam();
      const Uint32 triggerId = req->tableTriggers[i].triggerIds[j];
      tabPtr.p->triggerIds[j] = triggerId;
      
      TriggerPtr trigPtr;
      if(!ptr.p->triggers.seizeId(trigPtr, triggerId)) {
        jam();
	ptr.p->m_gsn = GSN_START_BACKUP_REF;
	StartBackupRef* ref = (StartBackupRef*)signal->getDataPtrSend();
	ref->backupPtr = ptr.i;
	ref->backupId = ptr.p->backupId;
	ref->signalNo = signalNo;
	ref->errorCode = StartBackupRef::FailedToAllocateTriggerRecord;
	ref->nodeId = getOwnNodeId();
	sendSignal(ptr.p->masterRef, GSN_START_BACKUP_REF, signal,
		   StartBackupRef::SignalLength, JBB);
	return;
      }//if

      tabPtr.p->triggerAllocated[j] = true;
      trigPtr.p->backupPtr = ptr.i;
      trigPtr.p->tableId = tabPtr.p->tableId;
      trigPtr.p->tab_ptr_i = tabPtr.i;
      trigPtr.p->logEntry = 0;
      trigPtr.p->event = j;
      trigPtr.p->maxRecordSize = 4096;
      trigPtr.p->operation = 
	&ptr.p->files.getPtr(ptr.p->logFilePtr)->operation;
      trigPtr.p->operation->noOfBytes = 0;
      trigPtr.p->operation->noOfRecords = 0;
      trigPtr.p->errorCode = 0;
    }//for
  }//for
  
  /**
   * Start file threads...
   */
  BackupFilePtr filePtr;
  for(ptr.p->files.first(filePtr); 
      filePtr.i!=RNIL; 
      ptr.p->files.next(filePtr)){
    jam();
    if(filePtr.p->fileRunning == 0) {
      jam();
      filePtr.p->fileRunning = 1;
      signal->theData[0] = BackupContinueB::START_FILE_THREAD;
      signal->theData[1] = filePtr.i;
      sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 2);
    }//if
  }//for
  
  ptr.p->m_gsn = GSN_START_BACKUP_CONF;
  StartBackupConf* conf = (StartBackupConf*)signal->getDataPtrSend();
  conf->backupPtr = ptr.i;
  conf->backupId = ptr.p->backupId;
  conf->signalNo = signalNo;
  sendSignal(ptr.p->masterRef, GSN_START_BACKUP_CONF, signal,
	     StartBackupConf::SignalLength, JBB);
}

/*****************************************************************************
 * 
 * Slave functionallity: Backup fragment
 *
 *****************************************************************************/
void
Backup::execBACKUP_FRAGMENT_REQ(Signal* signal)
{
  jamEntry();
  BackupFragmentReq* req = (BackupFragmentReq*)signal->getDataPtr();

  CRASH_INSERTION((10016));

  const Uint32 ptrI = req->backupPtr;
  //const Uint32 backupId = req->backupId;
  const Uint32 tableId = req->tableId;
  const Uint32 fragNo = req->fragmentNo;
  const Uint32 count = req->count;
  
  /**
   * Get backup record
   */
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->slaveState.setState(SCANNING);
  ptr.p->m_gsn = GSN_BACKUP_FRAGMENT_REQ;

  /**
   * Get file
   */
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);
  
  ndbrequire(filePtr.p->backupPtr == ptrI);
  ndbrequire(filePtr.p->fileOpened == 1);
  ndbrequire(filePtr.p->fileRunning == 1);
  ndbrequire(filePtr.p->scanRunning == 0);
  ndbrequire(filePtr.p->fileClosing == 0);
  
  /**
   * Get table
   */
  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  /**
   * Get fragment
   */
  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragNo);

  ndbrequire(fragPtr.p->scanned == 0);
  ndbrequire(fragPtr.p->scanning == 0 || 
	     refToNode(ptr.p->masterRef) == getOwnNodeId());
  
  /**
   * Init operation
   */
  if(filePtr.p->tableId != tableId) {
    jam();
    filePtr.p->operation.init(tabPtr);
    filePtr.p->tableId = tableId;
  }//if
  
  /**
   * Check for space in buffer
   */
  if(!filePtr.p->operation.newFragment(tableId, fragNo)) {
    jam();
    req->count = count + 1;
    sendSignalWithDelay(BACKUP_REF, GSN_BACKUP_FRAGMENT_REQ, signal, 50,
			signal->length());
    ptr.p->slaveState.setState(STARTED);
    return;
  }//if
  
  /**
   * Mark things as "in use"
   */
  fragPtr.p->scanning = 1;
  filePtr.p->fragmentNo = fragNo;

  /**
   * Start scan
   */
  {
    filePtr.p->scanRunning = 1;
    
    Table & table = * tabPtr.p;
    ScanFragReq * req = (ScanFragReq *)signal->getDataPtrSend();
    const Uint32 parallelism = 16;
    const Uint32 attrLen = 5 + table.noOfAttributes;

    req->senderData = filePtr.i;
    req->resultRef = reference();
    req->schemaVersion = table.schemaVersion;
    req->fragmentNoKeyLen = fragNo;
    req->requestInfo = 0;
    req->savePointId = 0;
    req->tableId = table.tableId;
    ScanFragReq::setReadCommittedFlag(req->requestInfo, 1);
    ScanFragReq::setLockMode(req->requestInfo, 0);
    ScanFragReq::setHoldLockFlag(req->requestInfo, 0);
    ScanFragReq::setKeyinfoFlag(req->requestInfo, 0);
    ScanFragReq::setAttrLen(req->requestInfo,attrLen); 
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    req->clientOpPtr= filePtr.i;
    req->batch_size_rows= parallelism;
    req->batch_size_bytes= 0;
    sendSignal(DBLQH_REF, GSN_SCAN_FRAGREQ, signal,
               ScanFragReq::SignalLength, JBB);
    
    signal->theData[0] = filePtr.i;
    signal->theData[1] = 0;
    signal->theData[2] = (BACKUP << 20) + (getOwnNodeId() << 8);
    
    // Return all
    signal->theData[3] = table.noOfAttributes;
    signal->theData[4] = 0;
    signal->theData[5] = 0;
    signal->theData[6] = 0;
    signal->theData[7] = 0;
    
    Uint32 dataPos = 8;
    Uint32 i;
    for(i = 0; i<table.noOfAttributes; i++) {
      jam();
      AttributePtr attr;
      table.attributes.getPtr(attr, i);
      
      AttributeHeader::init(&signal->theData[dataPos], i, 0);
      dataPos++;
      if(dataPos == 25) {
        jam();
	sendSignal(DBLQH_REF, GSN_ATTRINFO, signal, 25, JBB);
	dataPos = 3;
      }//if
    }//for
    if(dataPos != 3) {
      jam();
      sendSignal(DBLQH_REF, GSN_ATTRINFO, signal, dataPos, JBB);
    }//if
  }
}

void
Backup::execSCAN_HBREP(Signal* signal)
{
  jamEntry();
}

void
Backup::execTRANSID_AI(Signal* signal)
{
  jamEntry();

  const Uint32 filePtrI = signal->theData[0];
  //const Uint32 transId1 = signal->theData[1];
  //const Uint32 transId2 = signal->theData[2];
  const Uint32 dataLen  = signal->length() - 3;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  OperationRecord & op = filePtr.p->operation;
  
  TablePtr tabPtr LINT_SET_PTR;
  c_tablePool.getPtr(tabPtr, op.tablePtr);
  
  Table & table = * tabPtr.p;
  
  /**
   * Unpack data
   */
  op.attrSzTotal += dataLen;

  Uint32 srcSz = dataLen;
  const Uint32 * src = &signal->theData[3];

  Uint32 * dst = op.dst;
  Uint32 dstSz = op.attrSzLeft;
  
  while(srcSz > 0) {
    jam();

    if(dstSz == 0) {
      jam();

      /**
       * Finished with one attribute now find next
       */
      const AttributeHeader attrHead(* src);
      const Uint32 attrId = attrHead.getAttributeId();
      const bool null = attrHead.isNULL();
      const Attribute::Data attr = table.attributes.getPtr(attrId)->data;
      
      srcSz -= attrHead.getHeaderSize();
      src   += attrHead.getHeaderSize();
      
      if(null) {
	jam();
	ndbrequire(attr.nullable);
	op.nullAttribute(attr.offsetNull);
	dstSz = 0;
	continue;
      }//if
      
      dstSz = attrHead.getDataSize();
      ndbrequire(dstSz == attr.sz32);
      if(attr.fixed && ! attr.nullable) {
	jam();
	dst = op.newAttrib(attr.offset, dstSz);
      } else if (attr.fixed && attr.nullable) {
	jam();
	dst = op.newNullable(attrId, dstSz);
      } else {
	ndbrequire(false);
	//dst = op.newVariable(attrId, attrSize);
      }//if
    }//if
    
    const Uint32 szCopy = (dstSz > srcSz) ? srcSz : dstSz;
    memcpy(dst, src, (szCopy << 2));

    srcSz -= szCopy;
    dstSz -= szCopy;
    src   += szCopy;
    dst   += szCopy;
  }//while
  op.dst        = dst;
  op.attrSzLeft = dstSz;
  
  if(op.finished()){
    jam();
    op.newRecord(op.dst);
  }
}

void 
Backup::OperationRecord::init(const TablePtr & ptr)
{
  
  tablePtr = ptr.i;
  noOfAttributes = ptr.p->noOfAttributes;
  
  sz_Bitmask = (ptr.p->noOfNull + 31) >> 5;
  sz_FixedAttribs = ptr.p->sz_FixedAttributes;

  if(ptr.p->noOfVariable == 0) {
    jam();
    maxRecordSize = 1 + sz_Bitmask + sz_FixedAttribs;
  } else {
    jam();
    maxRecordSize = 
      1 + sz_Bitmask + 2048 /* Max tuple size */ + 2 * ptr.p->noOfVariable;
  }//if
}

bool
Backup::OperationRecord::newFragment(Uint32 tableId, Uint32 fragNo)
{
  Uint32 * tmp;
  const Uint32 headSz = (sizeof(BackupFormat::DataFile::FragmentHeader) >> 2);
  const Uint32 sz = headSz + 16 * maxRecordSize;
  
  ndbrequire(sz < dataBuffer.getMaxWrite());
  if(dataBuffer.getWritePtr(&tmp, sz)) {
    jam();
    BackupFormat::DataFile::FragmentHeader * head = 
      (BackupFormat::DataFile::FragmentHeader*)tmp;

    head->SectionType   = htonl(BackupFormat::FRAGMENT_HEADER);
    head->SectionLength = htonl(headSz);
    head->TableId       = htonl(tableId);
    head->FragmentNo    = htonl(fragNo);
    head->ChecksumType  = htonl(0);

    opNoDone = opNoConf = opLen = 0;
    newRecord(tmp + headSz);
    scanStart = tmp;
    scanStop  = (tmp + headSz);
    
    noOfRecords = 0;
    noOfBytes = 0;
    return true;
  }//if
  return false;
}

bool
Backup::OperationRecord::fragComplete(Uint32 tableId, Uint32 fragNo)
{
  Uint32 * tmp;
  const Uint32 footSz = sizeof(BackupFormat::DataFile::FragmentFooter) >> 2;

  if(dataBuffer.getWritePtr(&tmp, footSz + 1)) {
    jam();
    * tmp = 0; // Finish record stream
    tmp++;
    BackupFormat::DataFile::FragmentFooter * foot = 
      (BackupFormat::DataFile::FragmentFooter*)tmp;
    foot->SectionType   = htonl(BackupFormat::FRAGMENT_FOOTER);
    foot->SectionLength = htonl(footSz);
    foot->TableId       = htonl(tableId);
    foot->FragmentNo    = htonl(fragNo);
    foot->NoOfRecords   = htonl(noOfRecords);
    foot->Checksum      = htonl(0);
    dataBuffer.updateWritePtr(footSz + 1);
    return true;
  }//if
  return false;
}

bool
Backup::OperationRecord::newScan()
{
  Uint32 * tmp;
  ndbrequire(16 * maxRecordSize < dataBuffer.getMaxWrite());
  if(dataBuffer.getWritePtr(&tmp, 16 * maxRecordSize)) {
    jam();
    opNoDone = opNoConf = opLen = 0;
    newRecord(tmp);
    scanStart = tmp;
    scanStop = tmp;
    return true;
  }//if
  return false;
}

bool
Backup::OperationRecord::closeScan()
{
  opNoDone = opNoConf = opLen = 0;
  return true;
}

bool 
Backup::OperationRecord::scanConf(Uint32 noOfOps, Uint32 total_len)
{
  const Uint32 done = opNoDone-opNoConf;
  
  ndbrequire(noOfOps == done);
  ndbrequire(opLen == total_len);
  opNoConf = opNoDone;
  
  const Uint32 len = (scanStop - scanStart);
  ndbrequire(len < dataBuffer.getMaxWrite());
  dataBuffer.updateWritePtr(len);
  noOfBytes += (len << 2);
  return true;
}

void
Backup::execSCAN_FRAGREF(Signal* signal)
{
  jamEntry();

  ScanFragRef * ref = (ScanFragRef*)signal->getDataPtr();
  
  const Uint32 filePtrI = ref->senderData;
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);
  
  filePtr.p->errorCode = ref->errorCode;
  filePtr.p->scanRunning = 0;
  
  backupFragmentRef(signal, filePtr);
}

void
Backup::execSCAN_FRAGCONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10017));

  ScanFragConf * conf = (ScanFragConf*)signal->getDataPtr();
  
  const Uint32 filePtrI = conf->senderData;
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  OperationRecord & op = filePtr.p->operation;
  
  op.scanConf(conf->completedOps, conf->total_len);
  const Uint32 completed = conf->fragmentCompleted;
  if(completed != 2) {
    jam();
    
    checkScan(signal, filePtr);
    return;
  }//if

  fragmentCompleted(signal, filePtr);
}

void
Backup::fragmentCompleted(Signal* signal, BackupFilePtr filePtr)
{
  jam();

  if(filePtr.p->errorCode != 0)
  {
    jam();    
    filePtr.p->scanRunning = 0;
    backupFragmentRef(signal, filePtr); // Scan completed
    return;
  }//if
    
  OperationRecord & op = filePtr.p->operation;
  if(!op.fragComplete(filePtr.p->tableId, filePtr.p->fragmentNo)) {
    jam();
    signal->theData[0] = BackupContinueB::BUFFER_FULL_FRAG_COMPLETE;
    signal->theData[1] = filePtr.i;
    sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 50, 2);
    return;
  }//if
  
  filePtr.p->scanRunning = 0;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  BackupFragmentConf * conf = (BackupFragmentConf*)signal->getDataPtrSend();
  conf->backupId = ptr.p->backupId;
  conf->backupPtr = ptr.i;
  conf->tableId = filePtr.p->tableId;
  conf->fragmentNo = filePtr.p->fragmentNo;
  conf->noOfRecordsLow = (Uint32)(op.noOfRecords & 0xFFFFFFFF);
  conf->noOfRecordsHigh = (Uint32)(op.noOfRecords >> 32);
  conf->noOfBytesLow = (Uint32)(op.noOfBytes & 0xFFFFFFFF);
  conf->noOfBytesHigh = (Uint32)(op.noOfBytes >> 32);
  sendSignal(ptr.p->masterRef, GSN_BACKUP_FRAGMENT_CONF, signal,
	     BackupFragmentConf::SignalLength, JBB);
  
  ptr.p->m_gsn = GSN_BACKUP_FRAGMENT_CONF;
  ptr.p->slaveState.setState(STARTED);
  return;
}

void
Backup::backupFragmentRef(Signal * signal, BackupFilePtr filePtr)
{
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  ptr.p->m_gsn = GSN_BACKUP_FRAGMENT_REF;
  
  BackupFragmentRef * ref = (BackupFragmentRef*)signal->getDataPtrSend();
  ref->backupId = ptr.p->backupId;
  ref->backupPtr = ptr.i;
  ref->nodeId = getOwnNodeId();
  ref->errorCode = filePtr.p->errorCode;
  sendSignal(ptr.p->masterRef, GSN_BACKUP_FRAGMENT_REF, signal,
	     BackupFragmentRef::SignalLength, JBB);
}
 
void
Backup::checkScan(Signal* signal, BackupFilePtr filePtr)
{  
  OperationRecord & op = filePtr.p->operation;

  if(filePtr.p->errorCode != 0)
  {
    jam();

    /**
     * Close scan
     */
    op.closeScan();
    ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
    req->senderData = filePtr.i;
    req->closeFlag = 1;
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    sendSignal(DBLQH_REF, GSN_SCAN_NEXTREQ, signal, 
	       ScanFragNextReq::SignalLength, JBB);
    return;
  }//if
  
  if(op.newScan()) {
    jam();
    
    ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
    req->senderData = filePtr.i;
    req->closeFlag = 0;
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    req->batch_size_rows= 16;
    req->batch_size_bytes= 0;
    if(ERROR_INSERTED(10032))
      sendSignalWithDelay(DBLQH_REF, GSN_SCAN_NEXTREQ, signal, 
			  100, ScanFragNextReq::SignalLength);
    else if(ERROR_INSERTED(10033))
    {
      SET_ERROR_INSERT_VALUE(10032);
      sendSignalWithDelay(DBLQH_REF, GSN_SCAN_NEXTREQ, signal, 
			  10000, ScanFragNextReq::SignalLength);
      
      BackupRecordPtr ptr LINT_SET_PTR;
      c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
      AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
      ord->backupId = ptr.p->backupId;
      ord->backupPtr = ptr.i;
      ord->requestType = AbortBackupOrd::FileOrScanError;
      ord->senderData= ptr.i;
      sendSignal(ptr.p->masterRef, GSN_ABORT_BACKUP_ORD, signal, 
		 AbortBackupOrd::SignalLength, JBB);
    }
    else
      sendSignal(DBLQH_REF, GSN_SCAN_NEXTREQ, signal, 
		 ScanFragNextReq::SignalLength, JBB);
    return;
  }//if
  
  signal->theData[0] = BackupContinueB::BUFFER_FULL_SCAN;
  signal->theData[1] = filePtr.i;
  sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 50, 2);
}

void
Backup::execFSAPPENDREF(Signal* signal)
{
  jamEntry();
  
  FsRef * ref = (FsRef *)signal->getDataPtr();

  const Uint32 filePtrI = ref->userPointer;
  const Uint32 errCode = ref->errorCode;
  

  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  filePtr.p->fileRunning = 0;  
  filePtr.p->errorCode = errCode;

  checkFile(signal, filePtr);
}

void
Backup::execFSAPPENDCONF(Signal* signal)
{
  jamEntry();
  
  CRASH_INSERTION((10018));

  //FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 filePtrI = signal->theData[0]; //conf->userPointer;
  const Uint32 bytes = signal->theData[1]; //conf->bytes;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);
  
  OperationRecord & op = filePtr.p->operation;
  
  op.dataBuffer.updateReadPtr(bytes >> 2);

  checkFile(signal, filePtr);
}

void
Backup::checkFile(Signal* signal, BackupFilePtr filePtr)
{

#ifdef DEBUG_ABORT
  //  ndbout_c("---- check file filePtr.i = %u", filePtr.i);
#endif

  OperationRecord & op = filePtr.p->operation;
  
  Uint32 * tmp, sz; bool eof;
  if(op.dataBuffer.getReadPtr(&tmp, &sz, &eof)) 
  {
    jam();
    
    jam();
    FsAppendReq * req = (FsAppendReq *)signal->getDataPtrSend();
    req->filePointer   = filePtr.p->filePointer;
    req->userPointer   = filePtr.i;
    req->userReference = reference();
    req->varIndex      = 0;
    req->offset        = tmp - c_startOfPages;
    req->size          = sz;
    
    sendSignal(NDBFS_REF, GSN_FSAPPENDREQ, signal, 
	       FsAppendReq::SignalLength, JBA);
    return;
  }
  
  if(!eof) {
    jam();
    signal->theData[0] = BackupContinueB::BUFFER_UNDERFLOW;
    signal->theData[1] = filePtr.i;
    sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 50, 2);
    return;
  }//if
  
  if(sz > 0) {
    jam();
    FsAppendReq * req = (FsAppendReq *)signal->getDataPtrSend();
    req->filePointer   = filePtr.p->filePointer;
    req->userPointer   = filePtr.i;
    req->userReference = reference();
    req->varIndex      = 0;
    req->offset        = tmp - c_startOfPages;
    req->size          = sz; // Round up
    
    sendSignal(NDBFS_REF, GSN_FSAPPENDREQ, signal, 
	       FsAppendReq::SignalLength, JBA);
    return;
  }//if
  
  filePtr.p->fileRunning = 0;
  filePtr.p->fileClosing = 1;
  
  FsCloseReq * req = (FsCloseReq *)signal->getDataPtrSend();
  req->filePointer = filePtr.p->filePointer;
  req->userPointer = filePtr.i;
  req->userReference = reference();
  req->fileFlag = 0;
#ifdef DEBUG_ABORT
  ndbout_c("***** a FSCLOSEREQ filePtr.i = %u", filePtr.i);
#endif
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, FsCloseReq::SignalLength, JBA);
}


/****************************************************************************
 * 
 * Slave functionallity: Perform logging
 *
 ****************************************************************************/
void
Backup::execBACKUP_TRIG_REQ(Signal* signal)
{
  /*
  TUP asks if this trigger is to be fired on this node.
  */
  TriggerPtr trigPtr LINT_SET_PTR;
  TablePtr tabPtr LINT_SET_PTR;
  FragmentPtr fragPtr;
  Uint32 trigger_id = signal->theData[0];
  Uint32 frag_id = signal->theData[1];
  Uint32 result;

  jamEntry();
  c_triggerPool.getPtr(trigPtr, trigger_id);
  c_tablePool.getPtr(tabPtr, trigPtr.p->tab_ptr_i);
  tabPtr.p->fragments.getPtr(fragPtr, frag_id);
  if (fragPtr.p->node != getOwnNodeId()) {
    jam();
    result = ZFALSE;
  } else {
    jam();
    result = ZTRUE;
  }//if
  signal->theData[0] = result;
}

void
Backup::execTRIG_ATTRINFO(Signal* signal) {
  jamEntry();

  CRASH_INSERTION((10019));

  TrigAttrInfo * trg = (TrigAttrInfo*)signal->getDataPtr();

  TriggerPtr trigPtr LINT_SET_PTR;
  c_triggerPool.getPtr(trigPtr, trg->getTriggerId());
  ndbrequire(trigPtr.p->event != ILLEGAL_TRIGGER_ID); // Online...
  
  if(trigPtr.p->errorCode != 0) {
    jam();
    return;
  }//if
  
  if(trg->getAttrInfoType() == TrigAttrInfo::BEFORE_VALUES) {
    jam();
    /**
     * Backup is doing REDO logging and don't need before values
     */
    return;
  }//if

  BackupFormat::LogFile::LogEntry * logEntry = trigPtr.p->logEntry;
  if(logEntry == 0) 
  {
    jam();
    Uint32 * dst;
    FsBuffer & buf = trigPtr.p->operation->dataBuffer;
    ndbrequire(trigPtr.p->maxRecordSize <= buf.getMaxWrite());

    if(ERROR_INSERTED(10030) ||
       !buf.getWritePtr(&dst, trigPtr.p->maxRecordSize)) 
    {
      jam();
      Uint32 save[TrigAttrInfo::StaticLength];
      memcpy(save, signal->getDataPtr(), 4*TrigAttrInfo::StaticLength);
      BackupRecordPtr ptr LINT_SET_PTR;
      c_backupPool.getPtr(ptr, trigPtr.p->backupPtr);
      trigPtr.p->errorCode = AbortBackupOrd::LogBufferFull;
      AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
      ord->backupId = ptr.p->backupId;
      ord->backupPtr = ptr.i;
      ord->requestType = AbortBackupOrd::LogBufferFull;
      ord->senderData= ptr.i;
      sendSignal(ptr.p->masterRef, GSN_ABORT_BACKUP_ORD, signal, 
		 AbortBackupOrd::SignalLength, JBB);

      memcpy(signal->getDataPtrSend(), save, 4*TrigAttrInfo::StaticLength);
      return;
    }//if
        
    logEntry = (BackupFormat::LogFile::LogEntry *)dst;
    trigPtr.p->logEntry = logEntry;
    logEntry->Length       = 0;
    logEntry->TableId      = htonl(trigPtr.p->tableId);
    logEntry->TriggerEvent = htonl(trigPtr.p->event);
  } else {
    ndbrequire(logEntry->TableId == htonl(trigPtr.p->tableId));
    ndbrequire(logEntry->TriggerEvent == htonl(trigPtr.p->event));
  }//if
  
  const Uint32 pos = logEntry->Length; 
  const Uint32 dataLen = signal->length() - TrigAttrInfo::StaticLength;
  memcpy(&logEntry->Data[pos], trg->getData(), dataLen << 2);

  logEntry->Length = pos + dataLen;
}

void
Backup::execFIRE_TRIG_ORD(Signal* signal)
{
  jamEntry();
  FireTrigOrd* trg = (FireTrigOrd*)signal->getDataPtr();

  const Uint32 gci = trg->getGCI();
  const Uint32 trI = trg->getTriggerId();

  TriggerPtr trigPtr LINT_SET_PTR;
  c_triggerPool.getPtr(trigPtr, trI);
  
  ndbrequire(trigPtr.p->event != ILLEGAL_TRIGGER_ID);

  if(trigPtr.p->errorCode != 0) {
    jam();
    return;
  }//if

  ndbrequire(trigPtr.p->logEntry != 0);
  Uint32 len = trigPtr.p->logEntry->Length;

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, trigPtr.p->backupPtr);
  if(gci != ptr.p->currGCP) 
  {
    jam();
    
    trigPtr.p->logEntry->TriggerEvent = htonl(trigPtr.p->event | 0x10000);
    trigPtr.p->logEntry->Data[len] = htonl(gci);
    len ++;
    ptr.p->currGCP = gci;
  }//if
  
  len += (sizeof(BackupFormat::LogFile::LogEntry) >> 2) - 2;
  trigPtr.p->logEntry->Length = htonl(len);

  ndbrequire(len + 1 <= trigPtr.p->operation->dataBuffer.getMaxWrite());
  trigPtr.p->operation->dataBuffer.updateWritePtr(len + 1);
  trigPtr.p->logEntry = 0;
  
  trigPtr.p->operation->noOfBytes += (len + 1) << 2;
  trigPtr.p->operation->noOfRecords += 1;
}

void
Backup::sendAbortBackupOrd(Signal* signal, BackupRecordPtr ptr, 
			   Uint32 requestType)
{
  jam();
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->requestType = requestType;
  ord->senderData= ptr.i;
  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)) {
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(node.p->alive && ptr.p->nodes.get(nodeId)) {
      jam();
      sendSignal(numberToRef(BACKUP, nodeId), GSN_ABORT_BACKUP_ORD, signal, 
		 AbortBackupOrd::SignalLength, JBB);
    }//if
  }//for
}

/*****************************************************************************
 * 
 * Slave functionallity: Stop backup
 *
 *****************************************************************************/
void
Backup::execSTOP_BACKUP_REQ(Signal* signal)
{
  jamEntry();
  StopBackupReq * req = (StopBackupReq*)signal->getDataPtr();
  
  CRASH_INSERTION((10020));

  const Uint32 ptrI = req->backupPtr;
  //const Uint32 backupId = req->backupId;
  const Uint32 startGCP = req->startGCP;
  const Uint32 stopGCP = req->stopGCP;

  /**
   * At least one GCP must have passed
   */
  ndbrequire(stopGCP > startGCP);
  
  /**
   * Get backup record
   */
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->slaveState.setState(STOPPING);
  ptr.p->m_gsn = GSN_STOP_BACKUP_REQ;

  /**
   * Insert footers
   */
  {
    BackupFilePtr filePtr;
    ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
    Uint32 * dst;
    LINT_INIT(dst);
    ndbrequire(filePtr.p->operation.dataBuffer.getWritePtr(&dst, 1));
    * dst = 0;
    filePtr.p->operation.dataBuffer.updateWritePtr(1);
  }

  {
    BackupFilePtr filePtr;
    ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
    
    const Uint32 gcpSz = sizeof(BackupFormat::CtlFile::GCPEntry) >> 2;
    
    Uint32 * dst;
    LINT_INIT(dst);
    ndbrequire(filePtr.p->operation.dataBuffer.getWritePtr(&dst, gcpSz));
    
    BackupFormat::CtlFile::GCPEntry * gcp = 
      (BackupFormat::CtlFile::GCPEntry*)dst;
    
    gcp->SectionType   = htonl(BackupFormat::GCP_ENTRY);
    gcp->SectionLength = htonl(gcpSz);
    gcp->StartGCP      = htonl(startGCP);
    gcp->StopGCP       = htonl(stopGCP - 1);
    filePtr.p->operation.dataBuffer.updateWritePtr(gcpSz);

    {
      TablePtr tabPtr;
      ptr.p->tables.first(tabPtr);

      if (tabPtr.i == RNIL)
      {
        closeFiles(signal, ptr);
        return;
      }

      signal->theData[0] = BackupContinueB::BACKUP_FRAGMENT_INFO;
      signal->theData[1] = ptr.i;
      signal->theData[2] = tabPtr.i;
      signal->theData[3] = 0;
      sendSignal(BACKUP_REF, GSN_CONTINUEB, signal, 4, JBB);
    }
  }
}

void
Backup::closeFiles(Signal* sig, BackupRecordPtr ptr)
{
  /**
   * Close all files
   */
  BackupFilePtr filePtr;
  int openCount = 0;
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL; ptr.p->files.next(filePtr))
  {
    if(filePtr.p->fileOpened == 0) {
      jam();
      continue;
    }
    
    jam();
    openCount++;
    
    if(filePtr.p->fileClosing == 1){
      jam();
      continue;
    }//if
    
    filePtr.p->fileClosing = 1;
    
    if(filePtr.p->fileRunning == 1){
      jam();
#ifdef DEBUG_ABORT
      ndbout_c("Close files fileRunning == 1, filePtr.i=%u", filePtr.i);
#endif
      filePtr.p->operation.dataBuffer.eof();
    } else {
      jam();
      
      FsCloseReq * req = (FsCloseReq *)sig->getDataPtrSend();
      req->filePointer = filePtr.p->filePointer;
      req->userPointer = filePtr.i;
      req->userReference = reference();
      req->fileFlag = 0;
#ifdef DEBUG_ABORT
      ndbout_c("***** b FSCLOSEREQ filePtr.i = %u", filePtr.i);
#endif
      sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, sig, 
		 FsCloseReq::SignalLength, JBA);
    }//if
  }//for
  
  if(openCount == 0){
    jam();
    closeFilesDone(sig, ptr);
  }//if
}

void
Backup::execFSCLOSEREF(Signal* signal)
{
  jamEntry();
  
  FsRef * ref = (FsRef*)signal->getDataPtr();
  const Uint32 filePtrI = ref->userPointer;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  
  filePtr.p->fileOpened = 1;
  FsConf * conf = (FsConf*)signal->getDataPtr();
  conf->userPointer = filePtrI;
  
  execFSCLOSECONF(signal);
}

void
Backup::execFSCLOSECONF(Signal* signal)
{
  jamEntry();

  FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 filePtrI = conf->userPointer;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

#ifdef DEBUG_ABORT
  ndbout_c("***** FSCLOSECONF filePtrI = %u", filePtrI);
#endif

  ndbrequire(filePtr.p->fileClosing == 1);
  ndbrequire(filePtr.p->fileOpened == 1);
  ndbrequire(filePtr.p->fileRunning == 0);
  ndbrequire(filePtr.p->scanRunning == 0);	     
  
  filePtr.p->fileOpened = 0;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL;ptr.p->files.next(filePtr)) 
  {
    jam();
    if(filePtr.p->fileOpened == 1) {
      jam();
#ifdef DEBUG_ABORT
      ndbout_c("waiting for more FSCLOSECONF's filePtr.i = %u", filePtr.i);
#endif
      return; // we will be getting more FSCLOSECONF's
    }//if
  }//for
  closeFilesDone(signal, ptr);
}

void
Backup::closeFilesDone(Signal* signal, BackupRecordPtr ptr)
{
  jam();
  
  jam();
  BackupFilePtr filePtr;
  ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
  
  StopBackupConf* conf = (StopBackupConf*)signal->getDataPtrSend();
  conf->backupId = ptr.p->backupId;
  conf->backupPtr = ptr.i;
  conf->noOfLogBytes = filePtr.p->operation.noOfBytes;
  conf->noOfLogRecords = filePtr.p->operation.noOfRecords;
  sendSignal(ptr.p->masterRef, GSN_STOP_BACKUP_CONF, signal,
	     StopBackupConf::SignalLength, JBB);
  
  ptr.p->m_gsn = GSN_STOP_BACKUP_CONF;
  ptr.p->slaveState.setState(CLEANING);
}

/*****************************************************************************
 * 
 * Slave functionallity: Abort backup
 *
 *****************************************************************************/
/*****************************************************************************
 * 
 * Slave functionallity: Abort backup
 *
 *****************************************************************************/
void
Backup::execABORT_BACKUP_ORD(Signal* signal)
{
  jamEntry();
  AbortBackupOrd* ord = (AbortBackupOrd*)signal->getDataPtr();

  const Uint32 backupId = ord->backupId;
  const AbortBackupOrd::RequestType requestType = 
    (AbortBackupOrd::RequestType)ord->requestType;
  const Uint32 senderData = ord->senderData;
  
#ifdef DEBUG_ABORT
  ndbout_c("******** ABORT_BACKUP_ORD ********* nodeId = %u", 
	   refToNode(signal->getSendersBlockRef()));
  ndbout_c("backupId = %u, requestType = %u, senderData = %u, ",
	   backupId, requestType, senderData);
  dumpUsedResources();
#endif

  BackupRecordPtr ptr LINT_SET_PTR;
  if(requestType == AbortBackupOrd::ClientAbort) {
    if (getOwnNodeId() != getMasterNodeId()) {
      jam();
      // forward to master
#ifdef DEBUG_ABORT
      ndbout_c("---- Forward to master nodeId = %u", getMasterNodeId());
#endif
      sendSignal(calcBackupBlockRef(getMasterNodeId()), GSN_ABORT_BACKUP_ORD, 
		 signal, AbortBackupOrd::SignalLength, JBB);
      return;
    }
    jam();
    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
      jam();
      if(ptr.p->backupId == backupId && ptr.p->clientData == senderData) {
        jam();
	break;
      }//if
    }//for
    if(ptr.i == RNIL) {
      jam();
      return;
    }//if
  } else {
    if (c_backupPool.findId(senderData)) {
      jam();
      c_backupPool.getPtr(ptr, senderData);
    } else { 
      jam();
#ifdef DEBUG_ABORT
      ndbout_c("Backup: abort request type=%u on id=%u,%u not found",
	       requestType, backupId, senderData);
#endif
      return;
    }
  }//if
  
  ptr.p->m_gsn = GSN_ABORT_BACKUP_ORD;
  const bool isCoordinator = (ptr.p->masterRef == reference());
  
  bool ok = false;
  switch(requestType){

    /**
     * Requests sent to master
     */
  case AbortBackupOrd::ClientAbort:
    jam();
    // fall through
  case AbortBackupOrd::LogBufferFull:
    jam();
    // fall through
  case AbortBackupOrd::FileOrScanError:
    jam();
    ndbrequire(isCoordinator);
    ptr.p->setErrorCode(requestType);
    if(ptr.p->masterData.gsn == GSN_BACKUP_FRAGMENT_REQ)
    {
      /**
       * Only scans are actively aborted
       */
      abort_scan(signal, ptr);
    }
    return;
    
    /**
     * Requests sent to slave
     */
  case AbortBackupOrd::AbortScan:
    jam();
    ptr.p->setErrorCode(requestType);
    return;
    
  case AbortBackupOrd::BackupComplete:
    jam();
    cleanup(signal, ptr);
    return;
  case AbortBackupOrd::BackupFailure:
  case AbortBackupOrd::BackupFailureDueToNodeFail:
  case AbortBackupOrd::OkToClean:
  case AbortBackupOrd::IncompatibleVersions:
#ifndef VM_TRACE
  default:
#endif
    ptr.p->setErrorCode(requestType);
    ok= true;
  }
  ndbrequire(ok);
  
  Uint32 ref= ptr.p->masterRef;
  ptr.p->masterRef = reference();
  ptr.p->nodes.clear();
  ptr.p->nodes.set(getOwnNodeId());
  
  if(ref == reference())
  {
    ptr.p->stopGCP= ptr.p->startGCP + 1;
    sendDropTrig(signal, ptr);
  }
  else
  {
    ptr.p->masterData.gsn = GSN_STOP_BACKUP_REQ;
    ptr.p->masterData.sendCounter.clearWaitingFor();
    ptr.p->masterData.sendCounter.setWaitingFor(getOwnNodeId());
    closeFiles(signal, ptr);
  }
}


void
Backup::dumpUsedResources()
{
  jam();
  BackupRecordPtr ptr;

  for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
    ndbout_c("Backup id=%u, slaveState.getState = %u, errorCode=%u",
	     ptr.p->backupId,
	     ptr.p->slaveState.getState(),
	     ptr.p->errorCode);

    TablePtr tabPtr;
    for(ptr.p->tables.first(tabPtr);
	tabPtr.i != RNIL;
	ptr.p->tables.next(tabPtr)) {
      jam();
      for(Uint32 j = 0; j<3; j++) {
	jam();
	TriggerPtr trigPtr LINT_SET_PTR;
	if(tabPtr.p->triggerAllocated[j]) {
	  jam();
	  c_triggerPool.getPtr(trigPtr, tabPtr.p->triggerIds[j]);
	  ndbout_c("Allocated[%u] Triggerid = %u, event = %u",
		 j,
		 tabPtr.p->triggerIds[j],
		 trigPtr.p->event);
	}//if
      }//for
    }//for
    
    BackupFilePtr filePtr;
    for(ptr.p->files.first(filePtr);
	filePtr.i != RNIL;
	ptr.p->files.next(filePtr)) {
      jam();
      ndbout_c("filePtr.i = %u, filePtr.p->fileOpened=%u fileRunning=%u "
	       "scanRunning=%u",
	       filePtr.i,
	       filePtr.p->fileOpened,
	       filePtr.p->fileRunning,
	       filePtr.p->scanRunning);
    }//for
  }
}

void
Backup::cleanup(Signal* signal, BackupRecordPtr ptr)
{

  TablePtr tabPtr;
  for(ptr.p->tables.first(tabPtr); tabPtr.i != RNIL;ptr.p->tables.next(tabPtr))
  {
    jam();
    tabPtr.p->attributes.release();
    tabPtr.p->fragments.release();
    for(Uint32 j = 0; j<3; j++) {
      jam();
      TriggerPtr trigPtr LINT_SET_PTR;
      if(tabPtr.p->triggerAllocated[j]) {
        jam();
	c_triggerPool.getPtr(trigPtr, tabPtr.p->triggerIds[j]);
	trigPtr.p->event = ILLEGAL_TRIGGER_ID;
        tabPtr.p->triggerAllocated[j] = false;
      }//if
      tabPtr.p->triggerIds[j] = ILLEGAL_TRIGGER_ID;
    }//for
    {
      signal->theData[0] = tabPtr.p->tableId;
      signal->theData[1] = 0; // unlock
      EXECUTE_DIRECT(DBDICT, GSN_BACKUP_FRAGMENT_REQ, signal, 2);
    }
  }//for

  BackupFilePtr filePtr;
  for(ptr.p->files.first(filePtr);
      filePtr.i != RNIL; 
      ptr.p->files.next(filePtr)) {
    jam();
    ndbrequire(filePtr.p->fileOpened == 0);
    ndbrequire(filePtr.p->fileRunning == 0);
    ndbrequire(filePtr.p->scanRunning == 0);
    filePtr.p->pages.release();
  }//for

  ptr.p->files.release();
  ptr.p->tables.release();
  ptr.p->triggers.release();
  ptr.p->pages.release();
  ptr.p->backupId = ~0;
  
  if(ptr.p->checkError())
    removeBackup(signal, ptr);
  else
    c_backups.release(ptr);
}


void
Backup::removeBackup(Signal* signal, BackupRecordPtr ptr)
{
  jam();
  
  FsRemoveReq * req = (FsRemoveReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->directory = 1;
  req->ownDirectory = 1;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
	     FsRemoveReq::SignalLength, JBA);
}

void
Backup::execFSREMOVEREF(Signal* signal)
{
  jamEntry();
  FsRef * ref = (FsRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->userPointer;

  FsConf * conf = (FsConf*)signal->getDataPtr();
  conf->userPointer = ptrI;
  execFSREMOVECONF(signal);
}

void
Backup::execFSREMOVECONF(Signal* signal){
  jamEntry();

  FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->userPointer;
  
  /**
   * Get backup record
   */
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);
  c_backups.release(ptr);
}

