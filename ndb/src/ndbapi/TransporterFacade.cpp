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

#include <ndb_global.h>
#include <ndb_limits.h>
#include "TransporterFacade.hpp"
#include "ClusterMgr.hpp"
#include <IPCConfig.hpp>
#include <TransporterCallback.hpp>
#include <TransporterRegistry.hpp>
#include "NdbApiSignal.hpp"
#include <NdbOut.hpp>
#include <NdbEnv.h>
#include <NdbSleep.h>

#include "API.hpp"
#include <ConfigRetriever.hpp>
#include <mgmapi_config_parameters.h>
#include <mgmapi_configuration.hpp>
#include <NdbConfig.h>
#include <ndb_version.h>
#include <SignalLoggerManager.hpp>

//#define REPORT_TRANSPORTER
//#define API_TRACE;

#if defined DEBUG_TRANSPORTER
#define TRP_DEBUG(t) ndbout << __FILE__ << ":" << __LINE__ << ":" << t << endl;
#else
#define TRP_DEBUG(t)
#endif

TransporterFacade* TransporterFacade::theFacadeInstance = NULL;
ConfigRetriever *TransporterFacade::s_config_retriever= 0;


/*****************************************************************************
 * Call back functions
 *****************************************************************************/

void
reportError(void * callbackObj, NodeId nodeId, TransporterError errorCode){
#ifdef REPORT_TRANSPORTER
  ndbout_c("REPORT_TRANSP: reportError (nodeId=%d, errorCode=%d)", 
	   (int)nodeId, (int)errorCode);
#endif
  if(errorCode & 0x8000) {
    ndbout_c("reportError (%d, %d)\n", (int)nodeId, (int)errorCode);
    ((TransporterFacade*)(callbackObj))->doDisconnect(nodeId);
  }
}

/**
 * Report average send length in bytes (4096 last sends)
 */
void
reportSendLen(void * callbackObj, NodeId nodeId, Uint32 count, Uint64 bytes){
#ifdef REPORT_TRANSPORTER
  ndbout_c("REPORT_TRANSP: reportSendLen (nodeId=%d, bytes/count=%d)", 
	   (int)nodeId, (Uint32)(bytes/count));
#endif
  (void)nodeId;
  (void)count;
  (void)bytes;
}

/** 
 * Report average receive length in bytes (4096 last receives)
 */
void
reportReceiveLen(void * callbackObj, 
		 NodeId nodeId, Uint32 count, Uint64 bytes){
#ifdef REPORT_TRANSPORTER
  ndbout_c("REPORT_TRANSP: reportReceiveLen (nodeId=%d, bytes/count=%d)", 
	   (int)nodeId, (Uint32)(bytes/count));
#endif
  (void)nodeId;
  (void)count;
  (void)bytes;
}

/**
 * Report connection established
 */
void
reportConnect(void * callbackObj, NodeId nodeId){
#ifdef REPORT_TRANSPORTER
  ndbout_c("REPORT_TRANSP: API reportConnect (nodeId=%d)", (int)nodeId);
#endif
  ((TransporterFacade*)(callbackObj))->reportConnected(nodeId);
  //  TransporterFacade::instance()->reportConnected(nodeId);
}

/**
 * Report connection broken
 */
void
reportDisconnect(void * callbackObj, NodeId nodeId, Uint32 error){
#ifdef REPORT_TRANSPORTER
  ndbout_c("REPORT_TRANSP: API reportDisconnect (nodeId=%d)", (int)nodeId);
#endif
  ((TransporterFacade*)(callbackObj))->reportDisconnected(nodeId);
  //TransporterFacade::instance()->reportDisconnected(nodeId);
}


/****************************************************************************
 * 
 *****************************************************************************/

/**
 * Report connection broken
 */
int checkJobBuffer() {
  return 0;
}

#ifdef API_TRACE
static const char * API_SIGNAL_LOG = "API_SIGNAL_LOG";
static const char * apiSignalLog   = 0;
static SignalLoggerManager signalLogger;
static
inline
bool
setSignalLog(){
  signalLogger.flushSignalLog();

  const char * tmp = NdbEnv_GetEnv(API_SIGNAL_LOG, (char *)0, 0);
  if(tmp != 0 && apiSignalLog != 0 && strcmp(tmp,apiSignalLog) == 0){
    return true;
  } else if(tmp == 0 && apiSignalLog == 0){
    return false;
  } else if(tmp == 0 && apiSignalLog != 0){
    signalLogger.setOutputStream(0);
    apiSignalLog = tmp;
    return false;
  } else if(tmp !=0){
    if (strcmp(tmp, "-") == 0)
        signalLogger.setOutputStream(stdout);
    else
        signalLogger.setOutputStream(fopen(tmp, "w"));
    apiSignalLog = tmp;
    return true;
  }
  return false;
}
#ifdef TRACE_APIREGREQ
#define TRACE_GSN(gsn) true
#else
#define TRACE_GSN(gsn) (gsn != GSN_API_REGREQ && gsn != GSN_API_REGCONF)
#endif
#endif

/**
 * The execute function : Handle received signal
 */
void
execute(void * callbackObj, SignalHeader * const header, 
	Uint8 prio, Uint32 * const theData,
	LinearSectionPtr ptr[3]){

  TransporterFacade * theFacade = (TransporterFacade*)callbackObj;
  TransporterFacade::ThreadData::Object_Execute oe; 
  Uint32 tRecBlockNo = header->theReceiversBlockNumber;
  
#ifdef API_TRACE
  if(setSignalLog() && TRACE_GSN(header->theVerId_signalNumber)){
    signalLogger.executeSignal(* header, 
			       prio,
                               theData,
			       theFacade->ownId(), 
                               ptr, header->m_noOfSections);
    signalLogger.flushSignalLog();
  }
#endif  

  if (tRecBlockNo >= MIN_API_BLOCK_NO) {
    oe = theFacade->m_threads.get(tRecBlockNo);
    if (oe.m_object != 0 && oe.m_executeFunction != 0) {
      /**
       * Handle received signal immediately to avoid any unnecessary
       * copying of data, allocation of memory and other things. Copying
       * of data could be interesting to support several priority levels
       * and to support a special memory structure when executing the
       * signals. Neither of those are interesting when receiving data
       * in the NDBAPI. The NDBAPI will thus read signal data directly as
       * it was written by the sender (SCI sender is other node, Shared
       * memory sender is other process and TCP/IP sender is the OS that
       * writes the TCP/IP message into a message buffer).
       */
      NdbApiSignal tmpSignal(*header);
      NdbApiSignal * tSignal = &tmpSignal;
      tSignal->setDataPtr(theData);
      (* oe.m_executeFunction) (oe.m_object, tSignal, ptr);
    }//if
  } else if (tRecBlockNo == API_PACKED) {
    /**
     * Block number == 2047 is used to signal a signal that consists of
     * multiple instances of the same signal. This is an effort to
     * package the signals so as to avoid unnecessary communication
     * overhead since TCP/IP has a great performance impact.
     */
    Uint32 Tlength = header->theLength;
    Uint32 Tsent = 0;
    /**
     * Since it contains at least two data packets we will first
     * copy the signal data to safe place.
     */
    while (Tsent < Tlength) {
      Uint32 Theader = theData[Tsent];
      Tsent++;
      Uint32 TpacketLen = (Theader & 0x1F) + 3;
      tRecBlockNo = Theader >> 16;
      if (TpacketLen <= 25) {
	if ((TpacketLen + Tsent) <= Tlength) {
	  /**
	   * Set the data length of the signal and the receivers block
	   * reference and then call the API.
	   */
	  header->theLength = TpacketLen;
	  header->theReceiversBlockNumber = tRecBlockNo;
	  Uint32* tDataPtr = &theData[Tsent];
	  Tsent += TpacketLen;
	  if (tRecBlockNo >= MIN_API_BLOCK_NO) {
	    oe = theFacade->m_threads.get(tRecBlockNo);
	    if(oe.m_object != 0 && oe.m_executeFunction != 0){
	      NdbApiSignal tmpSignal(*header);
	      NdbApiSignal * tSignal = &tmpSignal;
	      tSignal->setDataPtr(tDataPtr);
	      (*oe.m_executeFunction)(oe.m_object, tSignal, 0);
	    }
	  }
	}
      }
    }
    return;
  } else if (tRecBlockNo == API_CLUSTERMGR) {
     /**
      * The signal was aimed for the Cluster Manager. 
      * We handle it immediately here.
      */     
     ClusterMgr * clusterMgr = theFacade->theClusterMgr;
     const Uint32 gsn = header->theVerId_signalNumber;

     switch (gsn){
     case GSN_API_REGREQ:
       clusterMgr->execAPI_REGREQ(theData);
       break;

     case GSN_API_REGCONF:
       clusterMgr->execAPI_REGCONF(theData);
       break;
     
     case GSN_API_REGREF:
       clusterMgr->execAPI_REGREF(theData);
       break;

     case GSN_NODE_FAILREP:
       clusterMgr->execNODE_FAILREP(theData);
       break;
       
     case GSN_NF_COMPLETEREP:
       clusterMgr->execNF_COMPLETEREP(theData);
       break;

     case GSN_ARBIT_STARTREQ:
       if (theFacade->theArbitMgr != NULL)
	 theFacade->theArbitMgr->doStart(theData);
       break;
       
     case GSN_ARBIT_CHOOSEREQ:
       if (theFacade->theArbitMgr != NULL)
	 theFacade->theArbitMgr->doChoose(theData);
       break;
       
     case GSN_ARBIT_STOPORD:
       if(theFacade->theArbitMgr != NULL)
	 theFacade->theArbitMgr->doStop(theData);
       break;

     default:
       break;
       
     }
     return;
  } else {
    ; // Ignore all other block numbers.
    if(header->theVerId_signalNumber!=3) {
      TRP_DEBUG( "TransporterFacade received signal to unknown block no." );
      ndbout << "BLOCK NO: "  << tRecBlockNo << " sig " 
	     << header->theVerId_signalNumber  << endl;
      abort();
    }
  }
}

// These symbols are needed, but not used in the API
void 
SignalLoggerManager::printSegmentedSection(FILE *, const SignalHeader &,
					   const SegmentedSectionPtr ptr[3],
					   unsigned i){
  abort();
}

void 
copy(Uint32 * & insertPtr, 
     class SectionSegmentPool & thePool, const SegmentedSectionPtr & _ptr){
  abort();
}

extern "C"
void 
atexit_stop_instance(){
  TransporterFacade::stop_instance();
}

/**
 * Note that this function need no locking since its
 * only called from the constructor of Ndb (the NdbObject)
 * 
 * Which is protected by a mutex
 */


TransporterFacade* 
TransporterFacade::start_instance(const char * connectString){

  // TransporterFacade used from API get config from mgmt srvr
  s_config_retriever= new ConfigRetriever(NDB_VERSION, NODE_TYPE_API);

  s_config_retriever->setConnectString(connectString);
  const char* error = 0;
  do {
    if(s_config_retriever->init() == -1)
      break;
    
    if(s_config_retriever->do_connect() == -1)
      break;
    
    const Uint32 nodeId = s_config_retriever->allocNodeId();
    if(nodeId == 0)
      break;
    
    
    ndb_mgm_configuration * props = s_config_retriever->getConfig();
    if(props == 0)
      break;
    
    TransporterFacade * tf = start_instance(nodeId, props);
    
    free(props);
    return tf;
  } while(0);
  
  ndbout << "Configuration error: ";
  const char* erString = s_config_retriever->getErrorString();
  if (erString == 0) {
    erString = "No error specified!";
  }
  ndbout << erString << endl;
  return 0;
}

TransporterFacade* 
TransporterFacade::start_instance(int nodeId, 
				  const ndb_mgm_configuration* props)
{
  TransporterFacade* tf = new TransporterFacade(); 
  if (! tf->init(nodeId, props)) {
    delete tf;
    return NULL;
  }
  
  /**
   * Install atexit handler
   */
  atexit(atexit_stop_instance);

  /**
   * Install signal handler for SIGPIPE
   *
   * This due to the fact that a socket connection might have
   * been closed in between a select and a corresponding send
   */
#if !defined NDB_OSE && !defined NDB_SOFTOSE && !defined NDB_WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  if(theFacadeInstance == NULL){
    theFacadeInstance = tf;
  }
  
  return tf;
}

void
TransporterFacade::close_configuration(){
  if (s_config_retriever) {
    delete s_config_retriever;
    s_config_retriever= 0;
  }
}

/**
 * Note that this function need no locking since its
 * only called from the destructor of Ndb (the NdbObject)
 * 
 * Which is protected by a mutex
 */
void
TransporterFacade::stop_instance(){

  close_configuration();

  if(theFacadeInstance == NULL){
    /**
     * We are called from atexit function
     */
    return;
  }

  theFacadeInstance->doStop();
  
  delete theFacadeInstance; theFacadeInstance = NULL;
}

void
TransporterFacade::doStop(){
  /**
   * First stop the ClusterMgr because it needs to send one more signal
   * and also uses theFacadeInstance to lock/unlock theMutexPtr
   */
  if (theClusterMgr != NULL) theClusterMgr->doStop();
  if (theArbitMgr != NULL) theArbitMgr->doStop(NULL);
  
  /**
   * Now stop the send and receive threads
   */
  void *status;
  theStopReceive = 1;
  NdbThread_WaitFor(theReceiveThread, &status);
  NdbThread_WaitFor(theSendThread, &status);
  NdbThread_Destroy(&theReceiveThread);
  NdbThread_Destroy(&theSendThread);
}

extern "C" 
void* 
runSendRequest_C(void * me)
{
  ((TransporterFacade*) me)->threadMainSend();
  NdbThread_Exit(0);
  return me;
}

void TransporterFacade::threadMainSend(void)
{
  SocketServer socket_server;

  theTransporterRegistry->startSending();
  if (!theTransporterRegistry->start_service(socket_server)){
    ndbout_c("Unable to start theTransporterRegistry->start_service");
    exit(0);
  }

  if (!theTransporterRegistry->start_clients()){
    ndbout_c("Unable to start theTransporterRegistry->start_clients");
    exit(0);
  }

  socket_server.startServer();

  while(!theStopReceive) {
    NdbSleep_MilliSleep(10);
    NdbMutex_Lock(theMutexPtr);
    if (sendPerformedLastInterval == 0) {
      theTransporterRegistry->performSend();
    }
    sendPerformedLastInterval = 0;
    NdbMutex_Unlock(theMutexPtr);
  }
  theTransporterRegistry->stopSending();

  socket_server.stopServer();
  socket_server.stopSessions();

  theTransporterRegistry->stop_clients();
}

extern "C" 
void* 
runReceiveResponse_C(void * me)
{
  ((TransporterFacade*) me)->threadMainReceive();
  NdbThread_Exit(0);
  return me;
}

void TransporterFacade::threadMainReceive(void)
{
  theTransporterRegistry->startReceiving();
  NdbMutex_Lock(theMutexPtr);
  theTransporterRegistry->update_connections();
  NdbMutex_Unlock(theMutexPtr);
  while(!theStopReceive) {
    for(int i = 0; i<10; i++){
      const int res = theTransporterRegistry->pollReceive(10);
      if(res > 0){
        NdbMutex_Lock(theMutexPtr);
        theTransporterRegistry->performReceive();
        NdbMutex_Unlock(theMutexPtr);
      }
    }
    NdbMutex_Lock(theMutexPtr);
    theTransporterRegistry->update_connections();
    NdbMutex_Unlock(theMutexPtr);
  }//while
  theTransporterRegistry->stopReceiving();
}

TransporterFacade::TransporterFacade() :
  theTransporterRegistry(0),
  theStopReceive(0),
  theSendThread(NULL),
  theReceiveThread(NULL)
{
  theMutexPtr = NdbMutex_Create();
  sendPerformedLastInterval = 0;

  checkCounter = 4;
  currentSendLimit = 1;
  theClusterMgr = NULL;
  theArbitMgr = NULL;
  theStartNodeId = 1;
  m_open_count = 0;
}

bool
TransporterFacade::init(Uint32 nodeId, const ndb_mgm_configuration* props)
{
  theOwnId = nodeId;
  theTransporterRegistry = new TransporterRegistry(this);

  const int res = IPCConfig::configureTransporters(nodeId, 
						   * props, 
						   * theTransporterRegistry);
  if(res <= 0){
    TRP_DEBUG( "configureTransporters returned 0 or less" );
    return false;
  }
  
  ndb_mgm_configuration_iterator iter(* props, CFG_SECTION_NODE);
  iter.first();
  theClusterMgr = new ClusterMgr(* this);
  theClusterMgr->init(iter);
  
  /**
   * Unless there is a "Name", the initiated transporter is within 
   * an NDB Cluster.  (If "Name" is defined, then the transporter
   * is used to connect to a different system, i.e. NDB Cluster.)
   */
#if 0  
  if (!props->contains("Name")) {
#endif
    iter.first();
    if(iter.find(CFG_NODE_ID, nodeId)){
      TRP_DEBUG( "Node info missing from config." );
      return false;
    }
    
    Uint32 rank = 0;
    if(!iter.get(CFG_NODE_ARBIT_RANK, &rank) && rank>0){
      theArbitMgr = new ArbitMgr(* this);
      theArbitMgr->setRank(rank);
      Uint32 delay = 0;
      iter.get(CFG_NODE_ARBIT_DELAY, &delay);
      theArbitMgr->setDelay(delay);
    }
    
#if 0
  }
#endif
  
  theReceiveThread = NdbThread_Create(runReceiveResponse_C,
                                      (void**)this,
                                      32768,
                                      "ndb_receive",
                                      NDB_THREAD_PRIO_LOW);

  theSendThread = NdbThread_Create(runSendRequest_C,
                                   (void**)this,
                                   32768,
                                   "ndb_send",
                                   NDB_THREAD_PRIO_LOW);

  theClusterMgr->startThread();
  
#ifdef API_TRACE
  signalLogger.logOn(true, 0, SignalLoggerManager::LogInOut);
#endif
  
  return true;
}


void
TransporterFacade::ReportNodeDead(NodeId tNodeId)
{
  /**
   * When a node fails we must report this to each Ndb object. 
   * The function that is used for communicating node failures is called.
   * This is to ensure that the Ndb objects do not think their connections 
   * are correct after a failure followed by a restart. 
   * After the restart the node is up again and the Ndb object 
   * might not have noticed the failure.
   */
  Uint32 sz = m_threads.m_statusNext.size();
  for (Uint32 i = 0; i < sz ; i ++) {
    if (m_threads.getInUse(i)){
      void * obj = m_threads.m_objectExecute[i].m_object;
      NodeStatusFunction RegPC = m_threads.m_statusFunction[i];
      (*RegPC) (obj, tNodeId, false, false);
    }
  }
}

void
TransporterFacade::ReportNodeFailureComplete(NodeId tNodeId)
{
  /**
   * When a node fails we must report this to each Ndb object. 
   * The function that is used for communicating node failures is called.
   * This is to ensure that the Ndb objects do not think their connections 
   * are correct after a failure followed by a restart. 
   * After the restart the node is up again and the Ndb object 
   * might not have noticed the failure.
   */
  Uint32 sz = m_threads.m_statusNext.size();
  for (Uint32 i = 0; i < sz ; i ++) {
    if (m_threads.getInUse(i)){
      void * obj = m_threads.m_objectExecute[i].m_object;
      NodeStatusFunction RegPC = m_threads.m_statusFunction[i];
      (*RegPC) (obj, tNodeId, false, true);
    }
  }
}

void
TransporterFacade::ReportNodeAlive(NodeId tNodeId)
{
  /**
   * When a node fails we must report this to each Ndb object. 
   * The function that is used for communicating node failures is called.
   * This is to ensure that the Ndb objects do not think there connections 
   * are correct after a failure
   * followed by a restart. 
   * After the restart the node is up again and the Ndb object 
   * might not have noticed the failure.
   */
  Uint32 sz = m_threads.m_statusNext.size();
  for (Uint32 i = 0; i < sz ; i ++) {
    if (m_threads.getInUse(i)){
      void * obj = m_threads.m_objectExecute[i].m_object;
      NodeStatusFunction RegPC = m_threads.m_statusFunction[i];
      (*RegPC) (obj, tNodeId, true, false);
    }
  }
}

int 
TransporterFacade::close(BlockNumber blockNumber)
{
  NdbMutex_Lock(theMutexPtr);
  close_local(blockNumber);
  NdbMutex_Unlock(theMutexPtr);
  return 0;
}

int 
TransporterFacade::close_local(BlockNumber blockNumber){
  m_threads.close(blockNumber);
  return 0;
}

int
TransporterFacade::open(void* objRef, 
                        ExecuteFunction fun, 
                        NodeStatusFunction statusFun)
{
  m_open_count++;
  return m_threads.open(objRef, fun, statusFun);
}

TransporterFacade::~TransporterFacade(){
  
  NdbMutex_Lock(theMutexPtr);
  delete theClusterMgr;  
  delete theArbitMgr;
  delete theTransporterRegistry;
  NdbMutex_Unlock(theMutexPtr);
  NdbMutex_Destroy(theMutexPtr);
#ifdef API_TRACE
  signalLogger.setOutputStream(0);
#endif
}

void 
TransporterFacade::calculateSendLimit()
{
  Uint32 Ti;
  Uint32 TthreadCount = 0;
  
  Uint32 sz = m_threads.m_statusNext.size();
  for (Ti = 0; Ti < sz; Ti++) {
    if (m_threads.m_statusNext[Ti] == (ThreadData::ACTIVE)){
      TthreadCount++;
      m_threads.m_statusNext[Ti] = ThreadData::INACTIVE;
    }
  }
  currentSendLimit = TthreadCount;
  if (currentSendLimit == 0) {
    currentSendLimit = 1;
  }
  checkCounter = currentSendLimit << 2;
}


//-------------------------------------------------
// Force sending but still report the sending to the
// adaptive algorithm.
//-------------------------------------------------
void TransporterFacade::forceSend(Uint32 block_number) {
  checkCounter--;
  m_threads.m_statusNext[block_number - MIN_API_BLOCK_NO] = ThreadData::ACTIVE;
  sendPerformedLastInterval = 1;
  if (checkCounter < 0) {
    calculateSendLimit();
  }
  theTransporterRegistry->forceSendCheck(0);
}

//-------------------------------------------------
// Improving API performance
//-------------------------------------------------
void
TransporterFacade::checkForceSend(Uint32 block_number) {  
  m_threads.m_statusNext[block_number - MIN_API_BLOCK_NO] = ThreadData::ACTIVE;
  //-------------------------------------------------
  // This code is an adaptive algorithm to discover when
  // the API should actually send its buffers. The reason
  // is that the performance is highly dependent on the
  // size of the writes over the communication network.
  // Thus we try to ensure that the send size is as big
  // as possible. At the same time we don't want response
  // time to increase so therefore we have to keep track of
  // how the users are performing adaptively.
  //-------------------------------------------------
  
  if (theTransporterRegistry->forceSendCheck(currentSendLimit) == 1) {
    sendPerformedLastInterval = 1;
  }
  checkCounter--;
  if (checkCounter < 0) {
    calculateSendLimit();
  }
}


/******************************************************************************
 * SEND SIGNAL METHODS
 *****************************************************************************/
int
TransporterFacade::sendSignal(NdbApiSignal * aSignal, NodeId aNode){
  Uint32* tDataPtr = aSignal->getDataPtrSend();
  Uint32 Tlen = aSignal->theLength;
  Uint32 TBno = aSignal->theReceiversBlockNumber;
  if(getIsNodeSendable(aNode) == true){
#ifdef API_TRACE
    if(setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)){
      Uint32 tmp = aSignal->theSendersBlockRef;
      aSignal->theSendersBlockRef = numberToRef(tmp, theOwnId);
      LinearSectionPtr ptr[3];
      signalLogger.sendSignal(* aSignal,
			      1,
			      aSignal->getDataPtr(),
			      aNode, ptr, 0);
      signalLogger.flushSignalLog();
      aSignal->theSendersBlockRef = tmp;
    }
#endif
    if ((Tlen != 0) && (Tlen <= 25) && (TBno != 0)) {
      SendStatus ss = theTransporterRegistry->prepareSend(aSignal, 
							  1, // JBB
							  tDataPtr, 
							  aNode, 
							  0);
      //if (ss != SEND_OK) ndbout << ss << endl;
      return (ss == SEND_OK ? 0 : -1);
    } else {
      ndbout << "ERR: SigLen = " << Tlen << " BlockRec = " << TBno;
      ndbout << " SignalNo = " << aSignal->theVerId_signalNumber << endl;
      assert(0);
    }//if
  }
  //const ClusterMgr::Node & node = theClusterMgr->getNodeInfo(aNode);
  //const Uint32 startLevel = node.m_state.startLevel;
  return -1; // Node Dead
}

int
TransporterFacade::sendSignalUnCond(NdbApiSignal * aSignal, NodeId aNode){
#ifdef API_TRACE
  if(setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)){
    Uint32 tmp = aSignal->theSendersBlockRef;
    aSignal->theSendersBlockRef = numberToRef(tmp, theOwnId);
    LinearSectionPtr ptr[3];
    signalLogger.sendSignal(* aSignal,
			    0,
			    aSignal->getDataPtr(),
			    aNode, ptr, 0);
    signalLogger.flushSignalLog();
    aSignal->theSendersBlockRef = tmp;
  }
#endif
  assert((aSignal->theLength != 0) &&
         (aSignal->theLength <= 25) &&
         (aSignal->theReceiversBlockNumber != 0));
  SendStatus ss = theTransporterRegistry->prepareSend(aSignal, 
						      0, 
						      aSignal->getDataPtr(), 
						      aNode, 
						      0);
  
  return (ss == SEND_OK ? 0 : -1);
}

int
TransporterFacade::sendFragmentedSignal(NdbApiSignal* aSignal, NodeId aNode, 
					LinearSectionPtr ptr[3], Uint32 secs){
  aSignal->m_noOfSections = secs;
  if(getIsNodeSendable(aNode) == true){
#ifdef API_TRACE
    if(setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)){
      Uint32 tmp = aSignal->theSendersBlockRef;
      aSignal->theSendersBlockRef = numberToRef(tmp, theOwnId);
      signalLogger.sendSignal(* aSignal,
			      1,
			      aSignal->getDataPtrSend(),
			      aNode,
                              ptr, secs);
      signalLogger.flushSignalLog();
      aSignal->theSendersBlockRef = tmp;
    }
#endif
    SendStatus ss = theTransporterRegistry->prepareSend
      (aSignal, 
       1, // JBB
       aSignal->getDataPtrSend(),
       aNode, 
       ptr);
    assert(ss != SEND_MESSAGE_TOO_BIG);
    aSignal->m_noOfSections = 0;
    return (ss == SEND_OK ? 0 : -1);
  }
  aSignal->m_noOfSections = 0;
  return -1;
}



int
TransporterFacade::sendFragmentedSignalUnCond(NdbApiSignal* aSignal, 
					      NodeId aNode, 
					      LinearSectionPtr ptr[3], 
					      Uint32 secs){
  aSignal->m_noOfSections = secs;
  
#ifdef API_TRACE
  if(setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)){
    Uint32 tmp = aSignal->theSendersBlockRef;
    aSignal->theSendersBlockRef = numberToRef(tmp, theOwnId);
    signalLogger.sendSignal(* aSignal,
			    1,
			    aSignal->getDataPtrSend(),
			    aNode,
			    ptr, secs);
      aSignal->theSendersBlockRef = tmp;
  }
#endif
  SendStatus ss = theTransporterRegistry->prepareSend(aSignal, 
						      1, // JBB
						      aSignal->getDataPtrSend(),
						      aNode, 
						      ptr);
  assert(ss != SEND_MESSAGE_TOO_BIG);
  aSignal->m_noOfSections = 0;
  return (ss == SEND_OK ? 0 : -1);
}



/******************************************************************************
 * CONNECTION METHODS  Etc
 ******************************************************************************/

void
TransporterFacade::doConnect(int aNodeId){
  theTransporterRegistry->setIOState(aNodeId, NoHalt);
  theTransporterRegistry->do_connect(aNodeId);
}

void
TransporterFacade::doDisconnect(int aNodeId)
{
  theTransporterRegistry->do_disconnect(aNodeId);
}

void
TransporterFacade::reportConnected(int aNodeId)
{
  theClusterMgr->reportConnected(aNodeId);
  return;
}

void
TransporterFacade::reportDisconnected(int aNodeId)
{
  theClusterMgr->reportDisconnected(aNodeId);
  return;
}

NodeId
TransporterFacade::ownId() const
{
  return theOwnId;
}

bool
TransporterFacade::isConnected(NodeId aNodeId){
  return theTransporterRegistry->is_connected(aNodeId);
}

NodeId
TransporterFacade::get_an_alive_node()
{
#ifdef VM_TRACE
  const char* p = NdbEnv_GetEnv("NDB_ALIVE_NODE_ID", (char*)0, 0);
  if (p != 0 && *p != 0)
    return atoi(p);
#endif
  NodeId i;
  for (i = theStartNodeId; i < MAX_NDB_NODES; i++) {
    if (get_node_alive(i)){
      theStartNodeId = ((i + 1) % MAX_NDB_NODES);
      return i;
    }
  }
  for (i = 1; i < theStartNodeId; i++) {
    if (get_node_alive(i)){
      theStartNodeId = ((i + 1) % MAX_NDB_NODES);
      return i;
    }
  }
  return (NodeId)0;
}

TransporterFacade::ThreadData::ThreadData(Uint32 size){
  m_firstFree = END_OF_LIST;
  expand(size);
}

void
TransporterFacade::ThreadData::expand(Uint32 size){
  Object_Execute oe = { 0 ,0 };
  NodeStatusFunction fun = 0;

  const Uint32 sz = m_statusNext.size();
  m_objectExecute.fill(sz + size, oe);
  m_statusFunction.fill(sz + size, fun);
  for(Uint32 i = 0; i<size; i++){
    m_statusNext.push_back(sz + i + 1);
  }

  m_statusNext.back() = m_firstFree;
  m_firstFree = m_statusNext.size() - size;
}

int
TransporterFacade::ThreadData::open(void* objRef, 
					   ExecuteFunction fun, 
					   NodeStatusFunction fun2){

  Uint32 nextFree = m_firstFree;

  if(m_statusNext.size() >= MAX_NO_THREADS && nextFree == END_OF_LIST){
    return -1;
  }
  
  if(nextFree == END_OF_LIST){
    expand(10);
    nextFree = m_firstFree;
  }
  
  m_firstFree = m_statusNext[nextFree];
  
  Object_Execute oe = { objRef , fun };

  m_statusNext[nextFree] = INACTIVE;
  m_objectExecute[nextFree] = oe;
  m_statusFunction[nextFree] = fun2;

  return nextFree + MIN_API_BLOCK_NO;
}

int
TransporterFacade::ThreadData::close(int number){
  number -= MIN_API_BLOCK_NO;
  assert(getInUse(number));
  m_statusNext[number] = m_firstFree;
  m_firstFree = number;
  Object_Execute oe = { 0, 0 };
  m_objectExecute[number] = oe;
  m_statusFunction[number] = 0;
  return 0;
}

template class Vector<NodeStatusFunction>;
template class Vector<TransporterFacade::ThreadData::Object_Execute>;
