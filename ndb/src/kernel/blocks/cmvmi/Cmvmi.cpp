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

#include "Cmvmi.hpp"

#include <ClusterConfiguration.hpp>
#include <Configuration.hpp>
#include <kernel_types.h>
#include <TransporterRegistry.hpp>
#include <NdbOut.hpp>
#include <NdbMem.h>

#include <SignalLoggerManager.hpp>
#include <FastScheduler.hpp>

#define DEBUG(x) { ndbout << "CMVMI::" << x << endl; }

#include <signaldata/TestOrd.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/TamperOrd.hpp>
#include <signaldata/SetVarReq.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/CmvmiCfgConf.hpp>
#include <signaldata/CmInit.hpp>
#include <signaldata/CloseComReqConf.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <signaldata/EventSubscribeReq.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/ArbitSignalData.hpp>
#include <signaldata/DisconnectRep.hpp>

#include <EventLogger.hpp>
#include <TimeQueue.hpp>
#include <new>

#include <SafeCounter.hpp>

// Used here only to print event reports on stdout/console.
static EventLogger eventLogger;

Cmvmi::Cmvmi(const Configuration & conf) :
  SimulatedBlock(CMVMI, conf)
  ,theConfig((Configuration&)conf)
  ,theCConfig(conf.clusterConfiguration()),
  subscribers(subscriberPool)
{
  BLOCK_CONSTRUCTOR(Cmvmi);

  // Add received signals
  addRecSignal(GSN_CONNECT_REP, &Cmvmi::execCONNECT_REP);
  addRecSignal(GSN_DISCONNECT_REP, &Cmvmi::execDISCONNECT_REP);

  addRecSignal(GSN_NDB_TAMPER,  &Cmvmi::execNDB_TAMPER, true);
  addRecSignal(GSN_SET_LOGLEVELORD,  &Cmvmi::execSET_LOGLEVELORD);
  addRecSignal(GSN_EVENT_REP,  &Cmvmi::execEVENT_REP);
  addRecSignal(GSN_STTOR,  &Cmvmi::execSTTOR_Local);
  addRecSignal(GSN_CM_RUN,  &Cmvmi::execCM_RUN);
  addRecSignal(GSN_CM_INFOREQ,  &Cmvmi::execCM_INFOREQ);
  addRecSignal(GSN_CMVMI_CFGREQ,  &Cmvmi::execCMVMI_CFGREQ);
  addRecSignal(GSN_CLOSE_COMREQ,  &Cmvmi::execCLOSE_COMREQ);
  addRecSignal(GSN_ENABLE_COMORD,  &Cmvmi::execENABLE_COMORD);
  addRecSignal(GSN_OPEN_COMREQ,  &Cmvmi::execOPEN_COMREQ);
  addRecSignal(GSN_SIZEALT_ACK,  &Cmvmi::execSIZEALT_ACK);
  addRecSignal(GSN_TEST_ORD,  &Cmvmi::execTEST_ORD);

  addRecSignal(GSN_STATISTICS_REQ,  &Cmvmi::execSTATISTICS_REQ);
  addRecSignal(GSN_TAMPER_ORD,  &Cmvmi::execTAMPER_ORD);
  addRecSignal(GSN_SET_VAR_REQ,  &Cmvmi::execSET_VAR_REQ);
  addRecSignal(GSN_SET_VAR_CONF,  &Cmvmi::execSET_VAR_CONF);
  addRecSignal(GSN_SET_VAR_REF,  &Cmvmi::execSET_VAR_REF);
  addRecSignal(GSN_STOP_ORD,  &Cmvmi::execSTOP_ORD);
  addRecSignal(GSN_START_ORD,  &Cmvmi::execSTART_ORD);
  addRecSignal(GSN_EVENT_SUBSCRIBE_REQ, 
               &Cmvmi::execEVENT_SUBSCRIBE_REQ);

  addRecSignal(GSN_DUMP_STATE_ORD, &Cmvmi::execDUMP_STATE_ORD);

  addRecSignal(GSN_TESTSIG, &Cmvmi::execTESTSIG);
  
  subscriberPool.setSize(5);

  // Print to stdout/console
  eventLogger.createConsoleHandler();
  eventLogger.setCategory("NDB");
  eventLogger.enable(Logger::LL_INFO, Logger::LL_ALERT); // Log INFO to ALERT

  const ClusterConfiguration::ClusterData & clData = 
    theConfig.clusterConfigurationData() ;
  
  clogLevel = clData.SizeAltData.logLevel;

  for(Uint32 i= 0; i< clData.SizeAltData.noOfNodes; i++ ){
    jam();
    const Uint32 nodeId = clData.nodeData[i].nodeId;
    switch(clData.nodeData[i].nodeType){
    case NodeInfo::DB:
    case NodeInfo::API:
    case NodeInfo::MGM:
    case NodeInfo::REP:
      break;
    default:
      ndbrequire(false);
    }
    setNodeInfo(nodeId).m_type = clData.nodeData[i].nodeType;
  }
}

Cmvmi::~Cmvmi()
{
}


void Cmvmi::execNDB_TAMPER(Signal* signal) 
{
  jamEntry();
  SET_ERROR_INSERT_VALUE(signal->theData[0]);
  if(ERROR_INSERTED(9999)){
    CRASH_INSERTION(9999);
  }
}//execNDB_TAMPER()

void Cmvmi::execSET_LOGLEVELORD(Signal* signal) 
{
  SetLogLevelOrd * const llOrd = (SetLogLevelOrd *)&signal->theData[0];
  LogLevel::EventCategory category;
  Uint32 level;
  jamEntry();

  for(unsigned int i = 0; i<llOrd->noOfEntries; i++){
    category = (LogLevel::EventCategory)llOrd->theCategories[i];
    level = llOrd->theLevels[i];

    clogLevel.setLogLevel(category, level);
  }
}//execSET_LOGLEVELORD()

void Cmvmi::execEVENT_REP(Signal* signal) 
{
  //-----------------------------------------------------------------------
  // This message is sent to report any types of events in NDB.
  // Based on the log level they will be either ignored or
  // reported. Currently they are printed, but they will be
  // transferred to the management server for further distribution
  // to the graphical management interface.
  //-----------------------------------------------------------------------
  EventReport * const eventReport = (EventReport *)&signal->theData[0]; 
  EventReport::EventType eventType = eventReport->getEventType();

  jamEntry();
  
  /**
   * If entry is not found
   */
  Uint32 threshold = 16;
  LogLevel::EventCategory eventCategory = (LogLevel::EventCategory)0;
  
  for(unsigned int i = 0; i< EventLogger::matrixSize; i++){
    if(EventLogger::matrix[i].eventType == eventType){
      eventCategory = EventLogger::matrix[i].eventCategory;
      threshold     = EventLogger::matrix[i].threshold;
      break;
    }
  }
  
  if(threshold > 15){
    // No entry found in matrix (or event that should never be printed)
    return;
  }
  
  SubscriberPtr ptr;
  for(subscribers.first(ptr); ptr.i != RNIL; subscribers.next(ptr)){
    if(ptr.p->logLevel.getLogLevel(eventCategory) < threshold){
      continue;
    }
    
    sendSignal(ptr.p->blockRef, GSN_EVENT_REP, signal, signal->length(), JBB);
  }
  
  if(clogLevel.getLogLevel(eventCategory) < threshold){
    return;
  }

  // Print the event info
  eventLogger.log(eventReport->getEventType(), signal->theData);

}//execEVENT_REP()

void
Cmvmi::execEVENT_SUBSCRIBE_REQ(Signal * signal){
  EventSubscribeReq * subReq = (EventSubscribeReq *)&signal->theData[0];
  SubscriberPtr ptr;

  jamEntry();

  /**
   * Search for subcription
   */
  for(subscribers.first(ptr); ptr.i != RNIL; subscribers.next(ptr)){
    if(ptr.p->blockRef == subReq->blockRef)
      break;
  }
  
  if(ptr.i == RNIL){
    /**
     * Create a new one
     */
    if(subscribers.seize(ptr) == false){
      sendSignal(subReq->blockRef, GSN_EVENT_SUBSCRIBE_REF, signal, 1, JBB);
      return;
    }
    /**
     * If it's a new subscription, clear the loglevel
     *
     * Clear only if noOfEntries is 0, this is needed beacuse we set
     * the default loglevels for the MGMT nodes during the inital connect phase.
     * See reportConnected().
     */
    if (subReq->noOfEntries == 0){
      ptr.p->logLevel.clear();
    }

    ptr.p->blockRef = subReq->blockRef;    
  }
  
  if(subReq->noOfEntries == 0){
    /**
     * Cancel subscription
     */
    subscribers.release(ptr.i);
  } else {
    /**
     * Update subscription
     */
    LogLevel::EventCategory category;
    Uint32 level = 0;
    for(Uint32 i = 0; i<subReq->noOfEntries; i++){
      category = (LogLevel::EventCategory)subReq->theCategories[i];
      level = subReq->theLevels[i];
      ptr.p->logLevel.setLogLevel(category,
                                  level);
    }
  }
  
  signal->theData[0] = ptr.i;
  sendSignal(ptr.p->blockRef, GSN_EVENT_SUBSCRIBE_CONF, signal, 1, JBB);
}

void
Cmvmi::cancelSubscription(NodeId nodeId){
  
  SubscriberPtr ptr;
  subscribers.first(ptr);
  
  while(ptr.i != RNIL){
    Uint32 i = ptr.i;
    BlockReference blockRef = ptr.p->blockRef;
    
    subscribers.next(ptr);
    
    if(refToNode(blockRef) == nodeId){
      subscribers.release(i);
    }
  }
}

void Cmvmi::sendSTTORRY(Signal* signal)
{
  if( theStartPhase == 1 ) {
    const ClusterConfiguration::ClusterData & clusterConf = 
      theConfig.clusterConfigurationData() ;
    const int myNodeId = globalData.ownId;
    int MyNodeFound = 0;
    
    jam();

    CmInit * const cmInit = (CmInit *)&signal->theData[0];
    
    cmInit->heartbeatDbDb            = clusterConf.ispValues[0][2]; 
    cmInit->heartbeatDbApi           = clusterConf.ispValues[0][3];
    cmInit->arbitTimeout             = clusterConf.ispValues[0][5];
    
    NodeBitmask::clear(cmInit->allNdbNodes);
    for(unsigned int i = 0; i < clusterConf.SizeAltData.noOfNodes; i++ ) {
      jam();
      if (clusterConf.nodeData[i].nodeType == NodeInfo::DB){
        jam();
        const NodeId nodeId = clusterConf.nodeData[i].nodeId;
        if (nodeId == myNodeId) {
          jam();
          MyNodeFound = 1;
        }//if
        NodeBitmask::set(cmInit->allNdbNodes, nodeId);
      }//if
    }//for
    
    if (MyNodeFound == 0) {
      ERROR_SET(fatal, ERR_NODE_NOT_IN_CONFIG, "", "");
    }//if
    
    sendSignal(QMGR_REF, GSN_CM_INIT, signal, CmInit::SignalLength, JBB);

    // these do not fit into CM_INIT
    ArbitSignalData* const sd = (ArbitSignalData*)&signal->theData[0];
    for (unsigned rank = 1; rank <= 2; rank++) {
      sd->sender = myNodeId;
      sd->code = rank;
      sd->node = 0;
      sd->ticket.clear();
      sd->mask.clear();
      for (int i = 0; i < MAX_NODES; i++) {
        if (clusterConf.nodeData[i].arbitRank == rank)
          sd->mask.set(clusterConf.nodeData[i].nodeId);
      }
      sendSignal(QMGR_REF, GSN_ARBIT_CFG, signal,
        ArbitSignalData::SignalLength, JBB);
    }
  } else {
    jam();
    signal->theData[0] = theSignalKey;
    signal->theData[3] = 1;
    signal->theData[4] = 3;
    signal->theData[5] = 255;
    sendSignal(NDBCNTR_REF, GSN_STTORRY, signal,6, JBB);
  }
}//Cmvmi::sendSTTORRY


// Received a restart signal.
// Answer it like any other block
// PR0  : StartCase
// DR0  : StartPhase
// DR1  : ?
// DR2  : ?
// DR3  : ?
// DR4  : ?
// DR5  : SignalKey

void Cmvmi::execSTTOR_Local(Signal* signal)
{
  theStartPhase  = signal->theData[1];
  theSignalKey   = signal->theData[6];

  const ClusterConfiguration::ClusterData & clusterConf = 
    theConfig.clusterConfigurationData();
  jamEntry();
  if (theStartPhase == 1 && clusterConf.SizeAltData.exist == true){
    jam();
    signalCount = 0;
    execSIZEALT_ACK(signal);
    return;
  } else if (theStartPhase == 3) {
    jam();
    globalData.activateSendPacked = 1;
    sendSTTORRY(signal);
  } else {
    jam();
    sendSTTORRY(signal);
  }
}

void Cmvmi::execSIZEALT_ACK(Signal* signal)
{  
  const ClusterConfiguration::ClusterData & clusterConf = 
    theConfig.clusterConfigurationData();
  jamEntry();
  
  if (signalCount < NDB_SIZEALT_OFF){
    jam();
    BlockNumber blockNo = clusterConf.SizeAltData.blockNo[signalCount];
    signal->theData[0] = CMVMI_REF;
    
    /**
     * This send SizeAlt(s) to blocks
     * Definition of data content can be found in SignalData/XXXSizeAltReq.H
     */
    const unsigned int noOfWords = 20;
    for(unsigned int i = 1; i<noOfWords; i++){
      signal->theData[i] = clusterConf.SizeAltData.varSize[signalCount][i].nrr;
    }
    
    signalCount++;
    sendSignal(numberToRef(blockNo, 0), GSN_SIZEALT_REP, signal,21, JBB);
  } else {
    jam();
    sendSTTORRY(signal);
  }
}

void Cmvmi::execCM_INFOREQ(Signal* signal)
{
  int id = signal->theData[1];
  const BlockReference userRef = signal->theData[0];
  const ClusterConfiguration::ClusterData & clusterConf = 
        theConfig.clusterConfigurationData();
  const int myNodeId = globalData.ownId;
  
  jamEntry();
  signal->theData[0] =  id;
  
  for(unsigned int i= 0; i< clusterConf.SizeAltData.noOfNodes; i++ ) {
    jam();
    if (clusterConf.nodeData[i].nodeType == NodeInfo::DB){
      NodeId nodeId = clusterConf.nodeData[i].nodeId;
      if (nodeId != myNodeId) {
        jam();
        globalTransporterRegistry.setPerformState(nodeId, PerformConnect);
      }
    }
  }
  
  sendSignal(userRef, GSN_CM_INFOCONF, signal, 1, JBB);
}

void Cmvmi::execCM_RUN(Signal* signal)
{
  jamEntry();
  if (signal->theData[0] == 0) {
    jam();
    signal->theData[0] = theSignalKey;
    signal->theData[3] = 1;
    signal->theData[4] = 3;
    signal->theData[5] = 255;
    sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 6, JBB);
  } else {
    globalData.theStartLevel = NodeState::SL_STARTED;
    
    // Connect to all application nodes. 
    // Enable communication with all NDB blocks.
    
    const ClusterConfiguration::ClusterData & clusterConf = 
      theConfig.clusterConfigurationData();
    jam();
    for(unsigned int i= 0; i< clusterConf.SizeAltData.noOfNodes; i++ ) {
      NodeId nodeId = clusterConf.nodeData[i].nodeId;
      jam();
      if (clusterConf.nodeData[i].nodeType != NodeInfo::DB &&
          clusterConf.nodeData[i].nodeType != NodeInfo::MGM){
        
        jam();
        globalTransporterRegistry.setPerformState(nodeId, PerformConnect);
        globalTransporterRegistry.setIOState(nodeId, HaltIO);
        //-----------------------------------------------------
        // Report that the connection to the node is opened
        //-----------------------------------------------------
        signal->theData[0] = EventReport::CommunicationOpened;
        signal->theData[1] = clusterConf.nodeData[i].nodeId;
        sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
        //-----------------------------------------------------
      }
    }
  }
}

void Cmvmi::execCMVMI_CFGREQ(Signal* signal)
{
  const BlockReference userRef = signal->theData[0];
  const ClusterConfiguration::ClusterData & clusterConf = 
    theConfig.clusterConfigurationData();
  
  int theStart_phase = signal->theData[1];
  
  jamEntry();

  CmvmiCfgConf * const cfgConf = (CmvmiCfgConf *)&signal->theData[0];
  
  cfgConf->startPhase = theStart_phase;
  for(unsigned int i = 0; i<CmvmiCfgConf::NO_OF_WORDS; i++)
    cfgConf->theData[i] = clusterConf.ispValues[theStart_phase][i];
  
  sendSignal(userRef, GSN_CMVMI_CFGCONF, signal, CmvmiCfgConf::LENGTH,JBB );
}

void Cmvmi::execCLOSE_COMREQ(Signal* signal)
{
  // Close communication with the node and halt input/output from 
  // other blocks than QMGR
  
  CloseComReqConf * const closeCom = (CloseComReqConf *)&signal->theData[0];

  const BlockReference userRef = closeCom->xxxBlockRef;
  Uint32 failNo = closeCom->failNo;
//  Uint32 noOfNodes = closeCom->noOfNodes;
  
  jamEntry();
  for (unsigned i = 0; i < MAX_NODES; i++){
    if(NodeBitmask::get(closeCom->theNodes, i)){
    
      jam();

      //-----------------------------------------------------
      // Report that the connection to the node is closed
      //-----------------------------------------------------
      signal->theData[0] = EventReport::CommunicationClosed;
      signal->theData[1] = i;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
      
      globalTransporterRegistry.setIOState(i, HaltIO);
      globalTransporterRegistry.setPerformState(i, PerformDisconnect);

      /**
       * Cancel possible event subscription
       */
      cancelSubscription(i);
    }
  }
  if (failNo != 0) {
    jam();
    signal->theData[0] = userRef;
    signal->theData[1] = failNo;
    sendSignal(QMGR_REF, GSN_CLOSE_COMCONF, signal, 19, JBA);
  }
}

void Cmvmi::execOPEN_COMREQ(Signal* signal)
{
  // Connect to the specifed NDB node, only QMGR allowed communication 
  // so far with the node

  const BlockReference userRef = signal->theData[0];
  Uint32 tStartingNode = signal->theData[1];

  jamEntry();
  if (userRef != 0) {
    jam(); 
    signal->theData[0] = signal->theData[1];
    sendSignal(userRef, GSN_OPEN_COMCONF, signal, 2,JBA);
  }
  globalTransporterRegistry.setPerformState(tStartingNode, PerformConnect);
  //-----------------------------------------------------
  // Report that the connection to the node is opened
  //-----------------------------------------------------
  signal->theData[0] = EventReport::CommunicationOpened;
  signal->theData[1] = tStartingNode;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  //-----------------------------------------------------
}

void Cmvmi::execENABLE_COMORD(Signal* signal)
{
  // Enable communication with all our NDB blocks to this node
  
  Uint32 tStartingNode = signal->theData[0];
  globalTransporterRegistry.setIOState(tStartingNode, NoHalt);
  setNodeInfo(tStartingNode).m_connected = true;
    //-----------------------------------------------------
  // Report that the version of the node
  //-----------------------------------------------------
  signal->theData[0] = EventReport::ConnectedApiVersion;
  signal->theData[1] = tStartingNode;
  signal->theData[2] = getNodeInfo(tStartingNode).m_version;

  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  //-----------------------------------------------------
  
  jamEntry();
}

void Cmvmi::execDISCONNECT_REP(Signal *signal)
{
  const DisconnectRep * const rep = (DisconnectRep *)&signal->theData[0];
  const Uint32 hostId = rep->nodeId;
  const Uint32 errNo  = rep->err;
  
  jamEntry();

  setNodeInfo(hostId).m_connected = false;
  setNodeInfo(hostId).m_connectCount++;
  const NodeInfo::NodeType type = getNodeInfo(hostId).getType();
  ndbrequire(type != NodeInfo::INVALID);
  
  if (globalTransporterRegistry.performState(hostId) != PerformDisconnect) {
    jam();

    // -------------------------------------------------------------------
    // We do not report the disconnection when disconnection is already ongoing.
    // This reporting should be looked into but this secures that we avoid
    // crashes due to too quick re-reporting of disconnection.
    // -------------------------------------------------------------------
    if(type == NodeInfo::DB || globalData.theStartLevel == NodeState::SL_STARTED){
      jam();
      DisconnectRep * const rep = (DisconnectRep *)&signal->theData[0];
      rep->nodeId = hostId;
      rep->err = errNo;
      sendSignal(QMGR_REF, GSN_DISCONNECT_REP, signal, 
		 DisconnectRep::SignalLength, JBA);
      globalTransporterRegistry.setPerformState(hostId, PerformDisconnect);
    } else if(globalData.theStartLevel == NodeState::SL_CMVMI ||
	      globalData.theStartLevel == NodeState::SL_STARTING) {
      /**
       * Someone disconnected during cmvmi period
       */
      if(type == NodeInfo::MGM){
	jam();
	globalTransporterRegistry.setPerformState(hostId, PerformConnect);
      } else {
	globalTransporterRegistry.setPerformState(hostId, PerformDisconnect);
      }
    }
  }

  signal->theData[0] = EventReport::Disconnected;
  signal->theData[1] = hostId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
}
 
void Cmvmi::execCONNECT_REP(Signal *signal){
  const Uint32 hostId = signal->theData[0];
  
  jamEntry();
  
  const NodeInfo::NodeType type = (NodeInfo::NodeType)getNodeInfo(hostId).m_type;
  ndbrequire(type != NodeInfo::INVALID);
  globalData.m_nodeInfo[hostId].m_version = 0;
  globalData.m_nodeInfo[hostId].m_signalVersion = 0;
  
  if(type == NodeInfo::DB || globalData.theStartLevel >= NodeState::SL_STARTED){
    jam();
    
    /**
     * Inform QMGR that client has connected
     */

    signal->theData[0] = hostId;
    sendSignal(QMGR_REF, GSN_CONNECT_REP, signal, 1, JBA);
  } else if(globalData.theStartLevel == NodeState::SL_CMVMI ||
            globalData.theStartLevel == NodeState::SL_STARTING) {
    jam();
    /**
     * Someone connected before start was finished
     */
    if(type == NodeInfo::MGM){
      jam();
    } else {
      /**
       * Dont allow api nodes to connect
       */
      globalTransporterRegistry.setPerformState(hostId, PerformDisconnect);
    }
  }
  
  /* Automatically subscribe events for MGM nodes.
   */
  if(type == NodeInfo::MGM){
    jam();
    globalTransporterRegistry.setIOState(hostId, NoHalt);
    
    EventSubscribeReq* dst = (EventSubscribeReq *)&signal->theData[0];
    
    for (Uint32 i = 0; i < EventLogger::defEventLogMatrixSize; i++) {
      dst->theCategories[i] = EventLogger::defEventLogMatrix[i].eventCategory;
      dst->theLevels[i]     = EventLogger::defEventLogMatrix[i].threshold;
    }

    dst->noOfEntries = EventLogger::defEventLogMatrixSize;
    /* The BlockNumber is hardcoded as 1 in MgmtSrvr */
    dst->blockRef = numberToRef(MIN_API_BLOCK_NO, hostId); 
    
    execEVENT_SUBSCRIBE_REQ(signal);
    
  }

  //------------------------------------------
  // Also report this event to the Event handler
  //------------------------------------------
  signal->theData[0] = EventReport::Connected;
  signal->theData[1] = hostId;
  signal->header.theLength = 2;
  
  execEVENT_REP(signal);
}

#ifdef VM_TRACE
void
modifySignalLogger(bool allBlocks, BlockNumber bno, 
                   TestOrd::Command cmd, 
                   TestOrd::SignalLoggerSpecification spec){
  SignalLoggerManager::LogMode logMode;

  /**
   * Mapping between SignalLoggerManager::LogMode and 
   *                 TestOrd::SignalLoggerSpecification
   */
  switch(spec){
  case TestOrd::InputSignals:
    logMode = SignalLoggerManager::LogIn;
    break;
  case TestOrd::OutputSignals:
    logMode = SignalLoggerManager::LogOut;
    break;
  case TestOrd::InputOutputSignals:
    logMode = SignalLoggerManager::LogInOut;
    break;
  default:
    return;
    break;
  }
  
  switch(cmd){
  case TestOrd::On:
    globalSignalLoggers.logOn(allBlocks, bno, logMode);
    break;
  case TestOrd::Off:
    globalSignalLoggers.logOff(allBlocks, bno, logMode);
    break;
  case TestOrd::Toggle:
    globalSignalLoggers.logToggle(allBlocks, bno, logMode);
    break;
  case TestOrd::KeepUnchanged:
    // Do nothing
    break;
  }
  globalSignalLoggers.flushSignalLog();
}
#endif

void
Cmvmi::execTEST_ORD(Signal * signal){
  jamEntry();
  
#ifdef VM_TRACE
  TestOrd * const testOrd = (TestOrd *)&signal->theData[0];

  TestOrd::Command cmd;

  {
    /**
     * Process Trace command
     */
    TestOrd::TraceSpecification traceSpec;

    testOrd->getTraceCommand(cmd, traceSpec);
    unsigned long traceVal = traceSpec;
    unsigned long currentTraceVal = globalSignalLoggers.getTrace();
    switch(cmd){
    case TestOrd::On:
      currentTraceVal |= traceVal;
      break;
    case TestOrd::Off:
      currentTraceVal &= (~traceVal);
      break;
    case TestOrd::Toggle:
      currentTraceVal ^= traceVal;
      break;
    case TestOrd::KeepUnchanged:
      // Do nothing
      break;
    }
    globalSignalLoggers.setTrace(currentTraceVal);
  }
  
  {
    /**
     * Process Log command
     */
    TestOrd::SignalLoggerSpecification logSpec;
    BlockNumber bno;
    unsigned int loggers = testOrd->getNoOfSignalLoggerCommands();
    
    if(loggers == (unsigned)~0){ // Apply command to all blocks
      testOrd->getSignalLoggerCommand(0, bno, cmd, logSpec);
      modifySignalLogger(true, bno, cmd, logSpec);
    } else {
      for(unsigned int i = 0; i<loggers; i++){
        testOrd->getSignalLoggerCommand(i, bno, cmd, logSpec);
        modifySignalLogger(false, bno, cmd, logSpec);
      }
    }
  }

  {
    /**
     * Process test command
     */
    testOrd->getTestCommand(cmd);
    switch(cmd){
    case TestOrd::On:{
      SET_GLOBAL_TEST_ON;
    }
    break;
    case TestOrd::Off:{
      SET_GLOBAL_TEST_OFF;
    }
    break;
    case TestOrd::Toggle:{
      TOGGLE_GLOBAL_TEST_FLAG;
    }
    break;
    case TestOrd::KeepUnchanged:
      // Do nothing
      break;
    }
  }

#endif
}

void Cmvmi::execSTATISTICS_REQ(Signal* signal) 
{
  // TODO Note ! This is only a test implementation...

  static int stat1 = 0;
  jamEntry();

  //ndbout << "data 1: " << signal->theData[1];

  int x = signal->theData[0];
  stat1++;
  signal->theData[0] = stat1;
  sendSignal(x, GSN_STATISTICS_CONF, signal, 7, JBB);

}//execSTATISTICS_REQ()



void Cmvmi::execSTOP_ORD(Signal* signal) 
{
  jamEntry();
  globalData.theRestartFlag = perform_stop;
}//execSTOP_ORD()

void
Cmvmi::execSTART_ORD(Signal* signal) {

  StartOrd * const startOrd = (StartOrd *)&signal->theData[0];
  jamEntry();
  
  Uint32 tmp = startOrd->restartInfo;
  if(StopReq::getPerformRestart(tmp)){
    jam();
    /**
     *
     */
    NdbRestartType type = NRT_Default;
    if(StopReq::getNoStart(tmp) && StopReq::getInitialStart(tmp))
      type = NRT_NoStart_InitialStart;
    if(StopReq::getNoStart(tmp) && !StopReq::getInitialStart(tmp))
      type = NRT_NoStart_Restart;
    if(!StopReq::getNoStart(tmp) && StopReq::getInitialStart(tmp))
      type = NRT_DoStart_InitialStart;
    if(!StopReq::getNoStart(tmp)&&!StopReq::getInitialStart(tmp))
      type = NRT_DoStart_Restart;
    NdbShutdown(NST_Restart, type);
  }

  if(globalData.theRestartFlag == system_started){
    jam()
    /**
     * START_ORD received when already started(ignored)
     */
    //ndbout << "START_ORD received when already started(ignored)" << endl;
    return;
  }
  
  if(globalData.theRestartFlag == perform_stop){
    jam()
    /**
     * START_ORD received when stopping(ignored)
     */
    //ndbout << "START_ORD received when stopping(ignored)" << endl;
    return;
  }
  
  if(globalData.theStartLevel == NodeState::SL_NOTHING){
    jam();
    globalData.theStartLevel = NodeState::SL_CMVMI;
    /**
     * Open connections to management servers
     */
    
    const ClusterConfiguration::ClusterData & clusterConf = 
      theConfig.clusterConfigurationData() ;
    
    for(unsigned int i= 0; i < clusterConf.SizeAltData.noOfNodes; i++ ){
      NodeId nodeId = clusterConf.nodeData[i].nodeId;
      
      if (clusterConf.nodeData[i].nodeType == NodeInfo::MGM){ 
        
        if(globalTransporterRegistry.performState(nodeId) != PerformIO){
          globalTransporterRegistry.setPerformState(nodeId, PerformConnect);
          globalTransporterRegistry.setIOState(nodeId, NoHalt);
        }
      }
    }
    return ;
  }
  
  if(globalData.theStartLevel == NodeState::SL_CMVMI){
    jam();
    globalData.theStartLevel  = NodeState::SL_STARTING;
    globalData.theRestartFlag = system_started;
    /**
     * StartLevel 1
     *
     * Do Restart
     */

    globalScheduler.clear();
    globalTimeQueue.clear();
    
    // Disconnect all nodes as part of the system restart. 
    // We need to ensure that we are starting up
    // without any connected nodes.   
    const ClusterConfiguration::ClusterData & clusterConf = 
      theConfig.clusterConfigurationData() ;
    const int myNodeId = globalData.ownId;
    
    for(unsigned int i= 0; i < clusterConf.SizeAltData.noOfNodes; i++ ){
      NodeId nodeId = clusterConf.nodeData[i].nodeId;
      if (myNodeId != nodeId && 
          clusterConf.nodeData[i].nodeType != NodeInfo::MGM){
        
        globalTransporterRegistry.setPerformState(nodeId, PerformDisconnect);
        globalTransporterRegistry.setIOState(nodeId, HaltIO);
      }
    }
    
    /**
     * Start running startphases
     */
    sendSignal(NDBCNTR_REF, GSN_START_ORD, signal, 1, JBA);  
    return;
  }
}//execSTART_ORD()

void Cmvmi::execTAMPER_ORD(Signal* signal) 
{
  jamEntry();
  // TODO We should maybe introduce a CONF and REF signal
  // to be able to indicate if we really introduced an error.
#ifdef ERROR_INSERT
  TamperOrd* const tamperOrd = (TamperOrd*)&signal->theData[0];
  
  signal->theData[1] = tamperOrd->errorNo;
  signal->theData[0] = 5;
  sendSignal(DBDIH_REF, GSN_DIHNDBTAMPER, signal, 3,JBB);
#endif

}//execTAMPER_ORD()



void Cmvmi::execSET_VAR_REQ(Signal* signal) 
{

  SetVarReq* const setVarReq = (SetVarReq*)&signal->theData[0];
  ConfigParamId var = setVarReq->variable();
  jamEntry();
  switch (var) {
    
    // NDBCNTR_REF
    
    // DBTC
  case TransactionDeadlockDetectionTimeout:
  case TransactionInactiveTime:
  case NoOfConcurrentProcessesHandleTakeover:
    sendSignal(DBTC_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;
    
    // DBDIH
  case TimeBetweenLocalCheckpoints:
  case TimeBetweenGlobalCheckpoints:
    sendSignal(DBDIH_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;

    // DBLQH
  case NoOfConcurrentCheckpointsDuringRestart:
  case NoOfConcurrentCheckpointsAfterRestart:
    sendSignal(DBLQH_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;

    // DBACC
  case NoOfDiskPagesToDiskDuringRestartACC:
  case NoOfDiskPagesToDiskAfterRestartACC:
    sendSignal(DBACC_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;

    // DBTUP
  case NoOfDiskPagesToDiskDuringRestartTUP:
  case NoOfDiskPagesToDiskAfterRestartTUP:
    sendSignal(DBTUP_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;

    // DBDICT

    // NDBCNTR
  case TimeToWaitAlive:

    // QMGR
  case HeartbeatIntervalDbDb: // TODO ev till Ndbcnt ocks�
  case HeartbeatIntervalDbApi:
  case ArbitTimeout:
    sendSignal(QMGR_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;

    // NDBFS

    // CMVMI
  case MaxNoOfSavedMessages:
  case LockPagesInMainMemory:
  case TimeBetweenWatchDogCheck:
  case StopOnError:
    handleSET_VAR_REQ(signal);
    break;


    // Not possible to update (this could of course be handled by each block
    // instead but I havn't investigated where they belong)
  case Id:
  case ExecuteOnComputer:
  case ShmKey:
  case MaxNoOfConcurrentOperations:
  case MaxNoOfConcurrentTransactions:
  case MemorySpaceIndexes:
  case MemorySpaceTuples:
  case MemoryDiskPages:
  case NoOfFreeDiskClusters:
  case NoOfDiskClusters:
  case NoOfFragmentLogFiles:
  case NoOfDiskClustersPerDiskFile:
  case NoOfDiskFiles:
  case MaxNoOfSavedEvents:
  default:

    int mgmtSrvr = setVarReq->mgmtSrvrBlockRef();
    sendSignal(mgmtSrvr, GSN_SET_VAR_REF, signal, 0, JBB);
  } // switch


}//execSET_VAR_REQ()


void Cmvmi::execSET_VAR_CONF(Signal* signal) 
{
  int mgmtSrvr = signal->theData[0];
  sendSignal(mgmtSrvr, GSN_SET_VAR_CONF, signal, 0, JBB);

}//execSET_VAR_CONF()


void Cmvmi::execSET_VAR_REF(Signal* signal) 
{
  int mgmtSrvr = signal->theData[0];
  sendSignal(mgmtSrvr, GSN_SET_VAR_REF, signal, 0, JBB);

}//execSET_VAR_REF()


void Cmvmi::handleSET_VAR_REQ(Signal* signal) {

  SetVarReq* const setVarReq = (SetVarReq*)&signal->theData[0];
  ConfigParamId var = setVarReq->variable();
  int val = setVarReq->value();

  switch (var) {
  case MaxNoOfSavedMessages:
    theConfig.maxNoOfErrorLogs(val);
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;
    
  case LockPagesInMainMemory:
    int result;
    if (val == 0) {
      result = NdbMem_MemUnlockAll();
    }
    else {
      result = NdbMem_MemLockAll();
    }
    if (result == 0) {
      sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    }
    else {
      sendSignal(CMVMI_REF, GSN_SET_VAR_REF, signal, 1, JBB);
    }
    break;

  case TimeBetweenWatchDogCheck:
    theConfig.timeBetweenWatchDogCheck(val);
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  case StopOnError:
    theConfig.stopOnError(val);
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;
    
  default:
    sendSignal(CMVMI_REF, GSN_SET_VAR_REF, signal, 1, JBB);
    return;
  } // switch

}

#ifdef VM_TRACE
class RefSignalTest {
public:
  enum ErrorCode {
    OK = 0,
    NF_FakeErrorREF = 7
  };
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};
#endif

void
Cmvmi::execDUMP_STATE_ORD(Signal* signal)
{

  sendSignal(QMGR_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(NDBCNTR_REF, GSN_DUMP_STATE_ORD, signal, signal->length(), JBB);
  sendSignal(DBTC_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(DBDIH_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(DBDICT_REF, GSN_DUMP_STATE_ORD,  signal, signal->length(), JBB);
  sendSignal(DBLQH_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(DBTUP_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(DBACC_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(NDBFS_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(BACKUP_REF, GSN_DUMP_STATE_ORD,  signal, signal->length(), JBB);
  sendSignal(DBUTIL_REF, GSN_DUMP_STATE_ORD,  signal, signal->length(), JBB);
  sendSignal(SUMA_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(GREP_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(TRIX_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(DBTUX_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  
  /**
   *
   * Here I can dump CMVMI state if needed
   */
  if(signal->theData[0] == 13){
    infoEvent("Cmvmi: signalCount = %d", signalCount);
  }

  DumpStateOrd * const & dumpState = (DumpStateOrd *)&signal->theData[0];
  if (dumpState->args[0] == DumpStateOrd::CmvmiDumpConnections){
    const ClusterConfiguration::ClusterData & clusterConf = 
      theConfig.clusterConfigurationData() ;
    
    for(unsigned int i= 0; i < clusterConf.SizeAltData.noOfNodes; i++ ){
      NodeId nodeId = clusterConf.nodeData[i].nodeId;

      const char* nodeTypeStr = "";
      switch(clusterConf.nodeData[i].nodeType){
      case NodeInfo::DB:
	nodeTypeStr = "DB";
	break;
      case NodeInfo::API:
	nodeTypeStr = "API";
	break;
      case NodeInfo::MGM:
	nodeTypeStr = "MGM";
	break;
      case NodeInfo::REP:
	nodeTypeStr = "REP";
	break;
      default:
	nodeTypeStr = "<UNKNOWN>";
      }

      const char* actionStr = "";
      switch (globalTransporterRegistry.performState(nodeId)){
      case PerformNothing:
        actionStr = "does nothing";
        break;
      case PerformIO:
        actionStr = "is connected";
        break;
      case PerformConnect:
        actionStr = "is trying to connect";
        break;
      case PerformDisconnect:
        actionStr = "is trying to disconnect";
        break;
      case RemoveTransporter:
        actionStr = "will be removed";
        break;
      }

      infoEvent("Connection to %d (%s) %s", 
                nodeId, 
                nodeTypeStr,
                actionStr);
    }
  }

  if (dumpState->args[0] == DumpStateOrd::CmvmiDumpLongSignalMemory){
    infoEvent("Cmvmi: g_sectionSegmentPool size: %d free: %d",
	      g_sectionSegmentPool.getSize(),
	      g_sectionSegmentPool.getNoOfFree());
  }

  if (dumpState->args[0] == DumpStateOrd::CmvmiSetRestartOnErrorInsert){
    if(signal->getLength() == 1)
      theConfig.setRestartOnErrorInsert((int)NRT_NoStart_Restart);
    else
      theConfig.setRestartOnErrorInsert(signal->theData[1]);
  }

  if (dumpState->args[0] == DumpStateOrd::CmvmiTestLongSigWithDelay) {
    Uint32 loopCount = dumpState->args[1];
    const unsigned len0 = 11;
    const unsigned len1 = 123;
    Uint32 sec0[len0];
    Uint32 sec1[len1];
    for (unsigned i = 0; i < len0; i++)
      sec0[i] = i;
    for (unsigned i = 0; i < len1; i++)
      sec1[i] = 16 * i;
    Uint32* sig = signal->getDataPtrSend();
    sig[0] = reference();
    sig[1] = 20; // test type
    sig[2] = 0;
    sig[3] = 0;
    sig[4] = loopCount;
    sig[5] = len0;
    sig[6] = len1;
    sig[7] = 0;
    LinearSectionPtr ptr[3];
    ptr[0].p = sec0;
    ptr[0].sz = len0;
    ptr[1].p = sec1;
    ptr[1].sz = len1;
    sendSignal(reference(), GSN_TESTSIG, signal, 8, JBB, ptr, 2);
  }

#ifdef VM_TRACE
#if 0
  {
    SafeCounterManager mgr(* this); mgr.setSize(1);
    SafeCounterHandle handle;

    {
      SafeCounter tmp(mgr, handle);
      tmp.init<RefSignalTest>(CMVMI, GSN_TESTSIG, /* senderData */ 13);
      tmp.setWaitingFor(3);
      ndbrequire(!tmp.done());
      ndbout_c("Allocted");
    }
    ndbrequire(!handle.done());
    {
      SafeCounter tmp(mgr, handle);
      tmp.clearWaitingFor(3);
      ndbrequire(tmp.done());
      ndbout_c("Deallocted");
    }
    ndbrequire(handle.done());
  }
#endif
#endif
}//Cmvmi::execDUMP_STATE_ORD()


BLOCK_FUNCTIONS(Cmvmi);

static Uint32 g_print;
static LinearSectionPtr g_test[3];

void
Cmvmi::execTESTSIG(Signal* signal){
  /**
   * Test of SafeCounter
   */
  jamEntry();

  if(!assembleFragments(signal)){
    jam();
    return;
  }

  Uint32 ref = signal->theData[0];
  Uint32 testType = signal->theData[1];
  Uint32 fragmentLength = signal->theData[2];
  g_print = signal->theData[3];
//  Uint32 returnCount = signal->theData[4];
  Uint32 * secSizes = &signal->theData[5];
  
  if(g_print){
    SignalLoggerManager::printSignalHeader(stdout, 
					   signal->header,
					   0,
					   getOwnNodeId(),
					   true);
    ndbout_c("-- Fixed section --");    
    for(Uint32 i = 0; i<signal->length(); i++){
      fprintf(stdout, "H'0x%.8x ", signal->theData[i]);
      if(((i + 1) % 6) == 0)
	fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");
    
    for(Uint32 i = 0; i<signal->header.m_noOfSections; i++){
      SegmentedSectionPtr ptr;
      ndbout_c("-- Section %d --", i);
      signal->getSection(ptr, i);
      ndbrequire(ptr.p != 0);
      print(ptr, stdout);
      ndbrequire(ptr.sz == secSizes[i]);
    }
  }

  /**
   * Validate length:s
   */
  for(Uint32 i = 0; i<signal->header.m_noOfSections; i++){
    SegmentedSectionPtr ptr;
    signal->getSection(ptr, i);
    ndbrequire(ptr.p != 0);
    ndbrequire(ptr.sz == secSizes[i]);
  }

  /**
   * Testing send with delay.
   */
  if (testType == 20) {
    if (signal->theData[4] == 0) {
      releaseSections(signal);
      return;
    }
    signal->theData[4]--;
    sendSignalWithDelay(reference(), GSN_TESTSIG, signal, 100, 8);
    return;
  }
  
  NodeReceiverGroup rg; rg.m_block = CMVMI;
  const ClusterConfiguration::ClusterData & clusterConf = 
    theConfig.clusterConfigurationData() ;
  for(unsigned int i = 0; i < clusterConf.SizeAltData.noOfNodes; i++ ){
    NodeId nodeId = clusterConf.nodeData[i].nodeId;
    if (clusterConf.nodeData[i].nodeType == NodeInfo::DB){ 
      rg.m_nodes.set(nodeId);
    }
  }

  if(signal->getSendersBlockRef() == ref){
    /**
     * Signal from API (not via NodeReceiverGroup)
     */
    if((testType % 2) == 1){
      signal->theData[4] = 1;
    } else {
      signal->theData[1] --;
      signal->theData[4] = rg.m_nodes.count();
    }
  } 
  
  switch(testType){
  case 1:
    sendSignal(ref, GSN_TESTSIG,  signal, signal->length(), JBB);      
    break;
  case 2:
    sendSignal(rg, GSN_TESTSIG,  signal, signal->length(), JBB);
    break;
  case 3:
  case 4:{
    LinearSectionPtr ptr[3];
    const Uint32 secs = signal->getNoOfSections();
    for(Uint32 i = 0; i<secs; i++){
      SegmentedSectionPtr sptr;
      signal->getSection(sptr, i);
      ptr[i].sz = sptr.sz;
      ptr[i].p = new Uint32[sptr.sz];
      copy(ptr[i].p, sptr);
    }
    
    if(testType == 3){
      sendSignal(ref, GSN_TESTSIG,  signal, signal->length(), JBB, ptr, secs); 
    } else {
      sendSignal(rg, GSN_TESTSIG,  signal, signal->length(), JBB, ptr, secs); 
    }
    for(Uint32 i = 0; i<secs; i++){
      delete[] ptr[i].p;
    }
    break;
  }
  case 5:
  case 6:{
    
    NodeReceiverGroup tmp;
    if(testType == 5){
      tmp  = ref;
    } else {
      tmp = rg;
    }
    
    FragmentSendInfo fragSend;
    sendFirstFragment(fragSend,
		      tmp,
		      GSN_TESTSIG,
		      signal,
		      signal->length(),
		      JBB,
		      fragmentLength);
    int count = 1;
    while(fragSend.m_status != FragmentSendInfo::SendComplete){
      count++;
      if(g_print)
	ndbout_c("Sending fragment %d", count);
      sendNextSegmentedFragment(signal, fragSend);
    }
    break;
  }
  case 7:
  case 8:{
    LinearSectionPtr ptr[3];
    const Uint32 secs = signal->getNoOfSections();
    for(Uint32 i = 0; i<secs; i++){
      SegmentedSectionPtr sptr;
      signal->getSection(sptr, i);
      ptr[i].sz = sptr.sz;
      ptr[i].p = new Uint32[sptr.sz];
      copy(ptr[i].p, sptr);
    }

    NodeReceiverGroup tmp;
    if(testType == 7){
      tmp  = ref;
    } else {
      tmp = rg;
    }

    FragmentSendInfo fragSend;
    sendFirstFragment(fragSend,
		      tmp,
		      GSN_TESTSIG,
		      signal,
		      signal->length(),
		      JBB,
		      ptr,
		      secs,
		      fragmentLength);
    
    int count = 1;
    while(fragSend.m_status != FragmentSendInfo::SendComplete){
      count++;
      if(g_print)
	ndbout_c("Sending fragment %d", count);
      sendNextLinearFragment(signal, fragSend);
    }
    
    for(Uint32 i = 0; i<secs; i++){
      delete[] ptr[i].p;
    }
    break;
  }
  case 9:
  case 10:{

    Callback m_callBack;
    m_callBack.m_callbackFunction = 
      safe_cast(&Cmvmi::sendFragmentedComplete);
    
    if(testType == 9){
      m_callBack.m_callbackData = 9;
      sendFragmentedSignal(ref,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   m_callBack,
			   fragmentLength);
    } else {
      m_callBack.m_callbackData = 10;
      sendFragmentedSignal(rg,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   m_callBack,
			   fragmentLength);
    }
    break;
  }
  case 11:
  case 12:{

    const Uint32 secs = signal->getNoOfSections();
    memset(g_test, 0, sizeof(g_test));
    for(Uint32 i = 0; i<secs; i++){
      SegmentedSectionPtr sptr;
      signal->getSection(sptr, i);
      g_test[i].sz = sptr.sz;
      g_test[i].p = new Uint32[sptr.sz];
      copy(g_test[i].p, sptr);
    }
    
    
    Callback m_callBack;
    m_callBack.m_callbackFunction = 
      safe_cast(&Cmvmi::sendFragmentedComplete);
    
    if(testType == 11){
      m_callBack.m_callbackData = 11;
      sendFragmentedSignal(ref,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   g_test, secs,
			   m_callBack,
			   fragmentLength);
    } else {
      m_callBack.m_callbackData = 12;
      sendFragmentedSignal(rg,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   g_test, secs,
			   m_callBack,
			   fragmentLength);
    }
    break;
  }
  default:
    ndbrequire(false);
  }
  return;
}

void
Cmvmi::sendFragmentedComplete(Signal* signal, Uint32 data, Uint32 returnCode){
  if(g_print)
    ndbout_c("sendFragmentedComplete: %d", data);
  if(data == 11 || data == 12){
    for(Uint32 i = 0; i<3; i++){
      if(g_test[i].p != 0)
	delete[] g_test[i].p;
    }
  }
}
