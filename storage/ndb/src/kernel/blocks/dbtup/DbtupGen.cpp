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


#define DBTUP_C
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <AttributeHeader.hpp>
#include <Interpreter.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/TupCommit.hpp>
#include <signaldata/TupKey.hpp>

#include <signaldata/DropTab.hpp>
#include <SLList.hpp>

#define DEBUG(x) { ndbout << "TUP::" << x << endl; }

#define ljam() { jamLine(24000 + __LINE__); }
#define ljamEntry() { jamEntryLine(24000 + __LINE__); }

void Dbtup::initData() 
{
  cnoOfAttrbufrec = ZNO_OF_ATTRBUFREC;
  cnoOfFragrec = MAX_FRAG_PER_NODE;
  cnoOfFragoprec = MAX_FRAG_PER_NODE;
  cnoOfPageRangeRec = ZNO_OF_PAGE_RANGE_REC;
  c_maxTriggersPerTable = ZDEFAULT_MAX_NO_TRIGGERS_PER_TABLE;
  c_noOfBuildIndexRec = 32;

  // Records with constant sizes
  init_list_sizes();
}//Dbtup::initData()

Dbtup::Dbtup(Block_context& ctx, Pgman* pgman)
  : SimulatedBlock(DBTUP, ctx),
    c_lqh(0),
    m_pgman(this, pgman),
    c_extent_hash(c_extent_pool),
    c_storedProcPool(),
    c_buildIndexList(c_buildIndexPool),
    c_undo_buffer(this)
{
  BLOCK_CONSTRUCTOR(Dbtup);

  addRecSignal(GSN_DEBUG_SIG, &Dbtup::execDEBUG_SIG);
  addRecSignal(GSN_CONTINUEB, &Dbtup::execCONTINUEB);
  addRecSignal(GSN_LCP_FRAG_ORD, &Dbtup::execLCP_FRAG_ORD);

  addRecSignal(GSN_DUMP_STATE_ORD, &Dbtup::execDUMP_STATE_ORD);
  addRecSignal(GSN_SEND_PACKED, &Dbtup::execSEND_PACKED);
  addRecSignal(GSN_ATTRINFO, &Dbtup::execATTRINFO);
  addRecSignal(GSN_STTOR, &Dbtup::execSTTOR);
  addRecSignal(GSN_MEMCHECKREQ, &Dbtup::execMEMCHECKREQ);
  addRecSignal(GSN_TUPKEYREQ, &Dbtup::execTUPKEYREQ);
  addRecSignal(GSN_TUPSEIZEREQ, &Dbtup::execTUPSEIZEREQ);
  addRecSignal(GSN_TUPRELEASEREQ, &Dbtup::execTUPRELEASEREQ);
  addRecSignal(GSN_STORED_PROCREQ, &Dbtup::execSTORED_PROCREQ);
  addRecSignal(GSN_TUPFRAGREQ, &Dbtup::execTUPFRAGREQ);
  addRecSignal(GSN_TUP_ADD_ATTRREQ, &Dbtup::execTUP_ADD_ATTRREQ);
  addRecSignal(GSN_TUP_COMMITREQ, &Dbtup::execTUP_COMMITREQ);
  addRecSignal(GSN_TUP_ABORTREQ, &Dbtup::execTUP_ABORTREQ);
  addRecSignal(GSN_NDB_STTOR, &Dbtup::execNDB_STTOR);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbtup::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_SET_VAR_REQ,  &Dbtup::execSET_VAR_REQ);

  // Trigger Signals
  addRecSignal(GSN_CREATE_TRIG_REQ, &Dbtup::execCREATE_TRIG_REQ);
  addRecSignal(GSN_DROP_TRIG_REQ,  &Dbtup::execDROP_TRIG_REQ);

  addRecSignal(GSN_DROP_TAB_REQ, &Dbtup::execDROP_TAB_REQ);

  addRecSignal(GSN_TUP_DEALLOCREQ, &Dbtup::execTUP_DEALLOCREQ);
  addRecSignal(GSN_TUP_WRITELOG_REQ, &Dbtup::execTUP_WRITELOG_REQ);

  // Ordered index related
  addRecSignal(GSN_BUILDINDXREQ, &Dbtup::execBUILDINDXREQ);

  // Tup scan
  addRecSignal(GSN_ACC_SCANREQ, &Dbtup::execACC_SCANREQ);
  addRecSignal(GSN_NEXT_SCANREQ, &Dbtup::execNEXT_SCANREQ);
  addRecSignal(GSN_ACC_CHECK_SCAN, &Dbtup::execACC_CHECK_SCAN);
  addRecSignal(GSN_ACCKEYCONF, &Dbtup::execACCKEYCONF);
  addRecSignal(GSN_ACCKEYREF, &Dbtup::execACCKEYREF);
  addRecSignal(GSN_ACC_ABORTCONF, &Dbtup::execACC_ABORTCONF);

  attrbufrec = 0;
  fragoperrec = 0;
  fragrecord = 0;
  hostBuffer = 0;
  pageRange = 0;
  tablerec = 0;
  tableDescriptor = 0;
  totNoOfPagesAllocated = 0;
  cnoOfAllocatedPages = 0;
  
  initData();
}//Dbtup::Dbtup()

Dbtup::~Dbtup() 
{
  // Records with dynamic sizes
  deallocRecord((void **)&attrbufrec,"Attrbufrec", 
		sizeof(Attrbufrec), 
		cnoOfAttrbufrec);
  
  deallocRecord((void **)&fragoperrec,"Fragoperrec",
		sizeof(Fragoperrec),
		cnoOfFragoprec);
  
  deallocRecord((void **)&fragrecord,"Fragrecord",
		sizeof(Fragrecord), 
		cnoOfFragrec);
  
  deallocRecord((void **)&hostBuffer,"HostBuffer",
		sizeof(HostBuffer), 
		MAX_NODES);
  
  deallocRecord((void **)&pageRange,"PageRange",
		sizeof(PageRange), 
		cnoOfPageRangeRec);

  deallocRecord((void **)&tablerec,"Tablerec",
		sizeof(Tablerec), 
		cnoOfTablerec);
  
  deallocRecord((void **)&tableDescriptor, "TableDescriptor",
		sizeof(TableDescriptor),
		cnoOfTabDescrRec);
  
}//Dbtup::~Dbtup()

BLOCK_FUNCTIONS(Dbtup)

void Dbtup::execCONTINUEB(Signal* signal) 
{
  ljamEntry();
  Uint32 actionType = signal->theData[0];
  Uint32 dataPtr = signal->theData[1];
  switch (actionType) {
  case ZINITIALISE_RECORDS:
    ljam();
    initialiseRecordsLab(signal, dataPtr, 
			 signal->theData[2], signal->theData[3]);
    break;
  case ZREL_FRAG:
    ljam();
    releaseFragment(signal, dataPtr);
    break;
  case ZREPORT_MEMORY_USAGE:{
    ljam();
    static int c_currentMemUsed = 0;
    Uint32 tmp = c_page_pool.getSize();
    int now = tmp ? (cnoOfAllocatedPages * 100)/tmp : 0;
    const int thresholds[] = { 100, 90, 80, 0 };
    
    Uint32 i = 0;
    const Uint32 sz = sizeof(thresholds)/sizeof(thresholds[0]);
    for(i = 0; i<sz; i++){
      if(now >= thresholds[i]){
	now = thresholds[i];
	break;
      }
    }

    if(now != c_currentMemUsed){
      reportMemoryUsage(signal, now > c_currentMemUsed ? 1 : -1);
      c_currentMemUsed = now;
    }
    signal->theData[0] = ZREPORT_MEMORY_USAGE;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 2000, 1);    
    return;
  }
  case ZBUILD_INDEX:
    ljam();
    buildIndex(signal, dataPtr);
    break;
  case ZTUP_SCAN:
    ljam();
    {
      ScanOpPtr scanPtr;
      c_scanOpPool.getPtr(scanPtr, dataPtr);
      scanCont(signal, scanPtr);
    }
    return;
  case ZFREE_EXTENT:
  {
    ljam();
    
    TablerecPtr tabPtr;
    tabPtr.i= dataPtr;
    FragrecordPtr fragPtr;
    fragPtr.i= signal->theData[2];
    ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
    ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
    drop_fragment_free_exent(signal, tabPtr, fragPtr, signal->theData[3]);
    return;
  }
  case ZUNMAP_PAGES:
  {
    ljam();
    
    TablerecPtr tabPtr;
    tabPtr.i= dataPtr;
    FragrecordPtr fragPtr;
    fragPtr.i= signal->theData[2];
    ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
    ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
    drop_fragment_unmap_pages(signal, tabPtr, fragPtr, signal->theData[3]);
    return;
  }
  case ZFREE_VAR_PAGES:
  {
    ljam();
    drop_fragment_free_var_pages(signal);
    return;
  }
  default:
    ndbrequire(false);
    break;
  }//switch
}//Dbtup::execTUP_CONTINUEB()

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* ------------------- SYSTEM RESTART MODULE ---------------------- */
/* ---------------------------------------------------------------- */
/* **************************************************************** */
void Dbtup::execSTTOR(Signal* signal) 
{
  ljamEntry();
  Uint32 startPhase = signal->theData[1];
  Uint32 sigKey = signal->theData[6];
  switch (startPhase) {
  case ZSTARTPHASE1:
    ljam();
    CLEAR_ERROR_INSERT_VALUE;
    ndbrequire((c_lqh= (Dblqh*)globalData.getBlock(DBLQH)) != 0);
    ndbrequire((c_tsman= (Tsman*)globalData.getBlock(TSMAN)) != 0);
    ndbrequire((c_lgman= (Lgman*)globalData.getBlock(LGMAN)) != 0);
    cownref = calcTupBlockRef(0);
    break;
  default:
    ljam();
    break;
  }//switch
  signal->theData[0] = sigKey;
  signal->theData[1] = 3;
  signal->theData[2] = 2;
  signal->theData[3] = ZSTARTPHASE1;
  signal->theData[4] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
  return;
}//Dbtup::execSTTOR()

/************************************************************************************************/
// SIZE_ALTREP INITIALIZE DATA STRUCTURES, FILES AND DS VARIABLES, GET READY FOR EXTERNAL 
// CONNECTIONS.
/************************************************************************************************/
void Dbtup::execREAD_CONFIG_REQ(Signal* signal) 
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);
  
  ljamEntry();

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_FRAG, &cnoOfFragrec));
  
  Uint32 noOfTriggers= 0;
  
  Uint32 tmp= 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_PAGE_RANGE, &tmp));
  initPageRangeSize(tmp);
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_TABLE, &cnoOfTablerec));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_TABLE_DESC, 
					&cnoOfTabDescrRec));
  Uint32 noOfStoredProc;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_STORED_PROC, 
					&noOfStoredProc));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_TRIGGERS, 
					&noOfTriggers));

  cnoOfTabDescrRec = (cnoOfTabDescrRec & 0xFFFFFFF0) + 16;

  initRecords();

  c_storedProcPool.setSize(noOfStoredProc);
  c_buildIndexPool.setSize(c_noOfBuildIndexRec);
  c_triggerPool.setSize(noOfTriggers, false, true, true, CFG_DB_NO_TRIGGERS);

  c_extent_hash.setSize(1024); // 4k
  
  Pool_context pc;
  pc.m_block = this;
  c_page_request_pool.wo_pool_init(RT_DBTUP_PAGE_REQUEST, pc);
  c_extent_pool.init(RT_DBTUP_EXTENT_INFO, pc);
  
  Uint32 nScanOp;       // use TUX config for now
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_SCAN_OP, &nScanOp));
  c_scanOpPool.setSize(nScanOp + 1);
  Uint32 nScanBatch;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_BATCH_SIZE, &nScanBatch));
  c_scanLockPool.setSize(nScanOp * nScanBatch);

  ScanOpPtr lcp;
  ndbrequire(c_scanOpPool.seize(lcp));
  new (lcp.p) ScanOp();
  c_lcp_scan_op= lcp.i;

  czero = 0;
  cminusOne = czero - 1;
  clastBitMask = 1;
  clastBitMask = clastBitMask << 31;

  initialiseRecordsLab(signal, 0, ref, senderData);
}//Dbtup::execSIZEALT_REP()

void Dbtup::initRecords() 
{
  unsigned i;
  Uint32 tmp;
  Uint32 tmp1 = 0;
  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_PAGE, &tmp));

  // Records with dynamic sizes
  Page* ptr =(Page*)allocRecord("Page", sizeof(Page), tmp, false, CFG_DB_DATA_MEM);
  c_page_pool.set(ptr, tmp);
  
  attrbufrec = (Attrbufrec*)allocRecord("Attrbufrec", 
					sizeof(Attrbufrec), 
					cnoOfAttrbufrec);

  fragoperrec = (Fragoperrec*)allocRecord("Fragoperrec",
					  sizeof(Fragoperrec),
					  cnoOfFragoprec);

  fragrecord = (Fragrecord*)allocRecord("Fragrecord",
					sizeof(Fragrecord), 
					cnoOfFragrec);
  
  hostBuffer = (HostBuffer*)allocRecord("HostBuffer",
					sizeof(HostBuffer), 
					MAX_NODES);

  tableDescriptor = (TableDescriptor*)allocRecord("TableDescriptor",
						  sizeof(TableDescriptor),
						  cnoOfTabDescrRec);

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_OP_RECS, &tmp));
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_LOCAL_OPS, &tmp1);
  c_operation_pool.setSize(tmp, false, true, true, 
      tmp1 == 0 ? CFG_DB_NO_OPS : CFG_DB_NO_LOCAL_OPS);
  
  pageRange = (PageRange*)allocRecord("PageRange",
				      sizeof(PageRange), 
				      cnoOfPageRangeRec);
  
  tablerec = (Tablerec*)allocRecord("Tablerec",
				    sizeof(Tablerec), 
				    cnoOfTablerec);

  for (i = 0; i<cnoOfTablerec; i++) {
    void * p = &tablerec[i];
    new (p) Tablerec(c_triggerPool);
  }
}//Dbtup::initRecords()

void Dbtup::initialiseRecordsLab(Signal* signal, Uint32 switchData,
				 Uint32 retRef, Uint32 retData) 
{
  switch (switchData) {
  case 0:
    ljam();
    initializeHostBuffer();
    break;
  case 1:
    ljam();
    initializeOperationrec();
    break;
  case 2:
    ljam();
    initializePage();
    break;
  case 3:
    ljam();
    break;
  case 4:
    ljam();
    initializeTablerec();
    break;
  case 5:
    ljam();
    break;
  case 6:
    ljam();
    initializeFragrecord();
    break;
  case 7:
    ljam();
    initializeFragoperrec();
    break;
  case 8:
    ljam();
    initializePageRange();
    break;
  case 9:
    ljam();
    initializeTabDescr();
    break;
  case 10:
    ljam();
    break;
  case 11:
    ljam();
    break;
  case 12:
    ljam();
    initializeAttrbufrec();
    break;
  case 13:
    ljam();
    break;
  case 14:
    ljam();

    {
      ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->senderData = retData;
      sendSignal(retRef, GSN_READ_CONFIG_CONF, signal, 
		 ReadConfigConf::SignalLength, JBB);
    }
    return;
  default:
    ndbrequire(false);
    break;
  }//switch
  signal->theData[0] = ZINITIALISE_RECORDS;
  signal->theData[1] = switchData + 1;
  signal->theData[2] = retRef;
  signal->theData[3] = retData;
  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
  return;
}//Dbtup::initialiseRecordsLab()

void Dbtup::execNDB_STTOR(Signal* signal) 
{
  ljamEntry();
  cndbcntrRef = signal->theData[0];
  Uint32 ownNodeId = signal->theData[1];
  Uint32 startPhase = signal->theData[2];
  switch (startPhase) {
  case ZSTARTPHASE1:
    ljam();
    cownNodeId = ownNodeId;
    cownref = calcTupBlockRef(ownNodeId);
    break;
  case ZSTARTPHASE2:
    ljam();
    break;
  case ZSTARTPHASE3:
    ljam();
    startphase3Lab(signal, ~0, ~0);
    break;
  case ZSTARTPHASE4:
    ljam();
    break;
  case ZSTARTPHASE6:
    ljam();
/*****************************************/
/*       NOW SET THE DISK WRITE SPEED TO */
/*       PAGES PER TICK AFTER SYSTEM     */
/*       RESTART.                        */
/*****************************************/
    signal->theData[0] = ZREPORT_MEMORY_USAGE;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 2000, 1);    
    break;
  default:
    ljam();
    break;
  }//switch
  signal->theData[0] = cownref;
  sendSignal(cndbcntrRef, GSN_NDB_STTORRY, signal, 1, JBB);
}//Dbtup::execNDB_STTOR()

void Dbtup::startphase3Lab(Signal* signal, Uint32 config1, Uint32 config2) 
{
}//Dbtup::startphase3Lab()

void Dbtup::initializeAttrbufrec() 
{
  AttrbufrecPtr attrBufPtr;
  for (attrBufPtr.i = 0;
       attrBufPtr.i < cnoOfAttrbufrec; attrBufPtr.i++) {
    refresh_watch_dog();
    ptrAss(attrBufPtr, attrbufrec);
    attrBufPtr.p->attrbuf[ZBUF_NEXT] = attrBufPtr.i + 1;
  }//for
  attrBufPtr.i = cnoOfAttrbufrec - 1;
  ptrAss(attrBufPtr, attrbufrec);
  attrBufPtr.p->attrbuf[ZBUF_NEXT] = RNIL;
  cfirstfreeAttrbufrec = 0;
  cnoFreeAttrbufrec = cnoOfAttrbufrec;
}//Dbtup::initializeAttrbufrec()

void Dbtup::initializeFragoperrec() 
{
  FragoperrecPtr fragoperPtr;
  for (fragoperPtr.i = 0; fragoperPtr.i < cnoOfFragoprec; fragoperPtr.i++) {
    ptrAss(fragoperPtr, fragoperrec);
    fragoperPtr.p->nextFragoprec = fragoperPtr.i + 1;
  }//for
  fragoperPtr.i = cnoOfFragoprec - 1;
  ptrAss(fragoperPtr, fragoperrec);
  fragoperPtr.p->nextFragoprec = RNIL;
  cfirstfreeFragopr = 0;
}//Dbtup::initializeFragoperrec()

void Dbtup::initializeFragrecord() 
{
  FragrecordPtr regFragPtr;
  for (regFragPtr.i = 0; regFragPtr.i < cnoOfFragrec; regFragPtr.i++) {
    refresh_watch_dog();
    ptrAss(regFragPtr, fragrecord);
    new (regFragPtr.p) Fragrecord();
    regFragPtr.p->nextfreefrag = regFragPtr.i + 1;
    regFragPtr.p->fragStatus = IDLE;
  }//for
  regFragPtr.i = cnoOfFragrec - 1;
  ptrAss(regFragPtr, fragrecord);
  regFragPtr.p->nextfreefrag = RNIL;
  cfirstfreefrag = 0;
}//Dbtup::initializeFragrecord()

void Dbtup::initializeHostBuffer() 
{
  Uint32 hostId;
  cpackedListIndex = 0;
  for (hostId = 0; hostId < MAX_NODES; hostId++) {
    hostBuffer[hostId].inPackedList = false;
    hostBuffer[hostId].noOfPacketsTA = 0;
    hostBuffer[hostId].packetLenTA = 0;
  }//for
}//Dbtup::initializeHostBuffer()


void Dbtup::initializeOperationrec() 
{
  refresh_watch_dog();
}//Dbtup::initializeOperationrec()

void Dbtup::initializeTablerec() 
{
  TablerecPtr regTabPtr;
  for (regTabPtr.i = 0; regTabPtr.i < cnoOfTablerec; regTabPtr.i++) {
    ljam();
    refresh_watch_dog();
    ptrAss(regTabPtr, tablerec);
    initTab(regTabPtr.p);
  }//for
}//Dbtup::initializeTablerec()

void
Dbtup::initTab(Tablerec* const regTabPtr)
{
  for (Uint32 i = 0; i < MAX_FRAG_PER_NODE; i++) {
    regTabPtr->fragid[i] = RNIL;
    regTabPtr->fragrec[i] = RNIL;
  }//for
  regTabPtr->readFunctionArray = NULL;
  regTabPtr->updateFunctionArray = NULL;
  regTabPtr->charsetArray = NULL;

  regTabPtr->tabDescriptor = RNIL;
  regTabPtr->readKeyArray = RNIL;

  regTabPtr->m_bits = 0;

  regTabPtr->m_no_of_attributes = 0;
  regTabPtr->noOfKeyAttr = 0;

  regTabPtr->m_dropTable.tabUserPtr = RNIL;
  regTabPtr->m_dropTable.tabUserRef = 0;
  regTabPtr->tableStatus = NOT_DEFINED;

  // Clear trigger data
  if (!regTabPtr->afterInsertTriggers.isEmpty())
    regTabPtr->afterInsertTriggers.release();
  if (!regTabPtr->afterDeleteTriggers.isEmpty())
    regTabPtr->afterDeleteTriggers.release();
  if (!regTabPtr->afterUpdateTriggers.isEmpty())
    regTabPtr->afterUpdateTriggers.release();
  if (!regTabPtr->subscriptionInsertTriggers.isEmpty())
    regTabPtr->subscriptionInsertTriggers.release();
  if (!regTabPtr->subscriptionDeleteTriggers.isEmpty())
    regTabPtr->subscriptionDeleteTriggers.release();
  if (!regTabPtr->subscriptionUpdateTriggers.isEmpty())
    regTabPtr->subscriptionUpdateTriggers.release();
  if (!regTabPtr->constraintUpdateTriggers.isEmpty())
    regTabPtr->constraintUpdateTriggers.release();
  if (!regTabPtr->tuxCustomTriggers.isEmpty())
    regTabPtr->tuxCustomTriggers.release();
}//Dbtup::initTab()

void Dbtup::initializeTabDescr() 
{
  TableDescriptorPtr regTabDesPtr;
  for (Uint32 i = 0; i < 16; i++) {
    cfreeTdList[i] = RNIL;
  }//for
  for (regTabDesPtr.i = 0; regTabDesPtr.i < cnoOfTabDescrRec; regTabDesPtr.i++) {
    refresh_watch_dog();
    ptrAss(regTabDesPtr, tableDescriptor);
    regTabDesPtr.p->tabDescr = RNIL;
  }//for
  freeTabDescr(0, cnoOfTabDescrRec);
}//Dbtup::initializeTabDescr()

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* --------------- CONNECT/DISCONNECT MODULE ---------------------- */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execTUPSEIZEREQ(Signal* signal)
{
  OperationrecPtr regOperPtr;
  ljamEntry();
  Uint32 userPtr = signal->theData[0];
  BlockReference userRef = signal->theData[1];
  if (!c_operation_pool.seize(regOperPtr))
  {
    ljam();
    signal->theData[0] = userPtr;
    signal->theData[1] = ZGET_OPREC_ERROR;
    sendSignal(userRef, GSN_TUPSEIZEREF, signal, 2, JBB);
    return;
  }//if

  new (regOperPtr.p) Operationrec();
  regOperPtr.p->firstAttrinbufrec = RNIL;
  regOperPtr.p->lastAttrinbufrec = RNIL;
  regOperPtr.p->op_struct.op_type = ZREAD;
  regOperPtr.p->op_struct.in_active_list = false;
  set_trans_state(regOperPtr.p, TRANS_DISCONNECTED);
  regOperPtr.p->storedProcedureId = ZNIL;
  regOperPtr.p->prevActiveOp = RNIL;
  regOperPtr.p->nextActiveOp = RNIL;
  regOperPtr.p->tupVersion = ZNIL;
  regOperPtr.p->op_struct.delete_insert_flag = false;
  
  initOpConnection(regOperPtr.p);
  regOperPtr.p->userpointer = userPtr;
  signal->theData[0] = regOperPtr.p->userpointer;
  signal->theData[1] = regOperPtr.i;
  sendSignal(userRef, GSN_TUPSEIZECONF, signal, 2, JBB);
  return;
}//Dbtup::execTUPSEIZEREQ()

#define printFragment(t){ for(Uint32 i = 0; i < MAX_FRAG_PER_NODE;i++){\
  ndbout_c("table = %d fragid[%d] = %d fragrec[%d] = %d", \
           t.i, t.p->fragid[i], i, t.p->fragrec[i]); }}

void Dbtup::execTUPRELEASEREQ(Signal* signal) 
{
  OperationrecPtr regOperPtr;
  ljamEntry();
  regOperPtr.i = signal->theData[0];
  c_operation_pool.getPtr(regOperPtr);
  set_trans_state(regOperPtr.p, TRANS_DISCONNECTED);
  c_operation_pool.release(regOperPtr);
  
  signal->theData[0] = regOperPtr.p->userpointer;
  sendSignal(DBLQH_REF, GSN_TUPRELEASECONF, signal, 1, JBB);
  return;
}//Dbtup::execTUPRELEASEREQ()

void Dbtup::releaseFragrec(FragrecordPtr regFragPtr) 
{
  regFragPtr.p->nextfreefrag = cfirstfreefrag;
  cfirstfreefrag = regFragPtr.i;
}//Dbtup::releaseFragrec()

void Dbtup::execSET_VAR_REQ(Signal* signal) 
{
#if 0
  SetVarReq* const setVarReq = (SetVarReq*)signal->getDataPtrSend();
  ConfigParamId var = setVarReq->variable();
  int val = setVarReq->value();

  switch (var) {

  case NoOfDiskPagesToDiskAfterRestartTUP:
    clblPagesPerTick = val;
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  case NoOfDiskPagesToDiskDuringRestartTUP:
    // Valid only during start so value not set.
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  default:
    sendSignal(CMVMI_REF, GSN_SET_VAR_REF, signal, 1, JBB);
  } // switch
#endif

}//execSET_VAR_REQ()




