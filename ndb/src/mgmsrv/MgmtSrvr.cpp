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
#include <my_pthread.h>

#include "MgmtSrvr.hpp"
#include "MgmtErrorReporter.hpp"
#include <ConfigRetriever.hpp>

#include <NdbOut.hpp>
#include <NdbApiSignal.hpp>
#include <kernel_types.h>
#include <RefConvert.hpp>
#include <BlockNumbers.h>
#include <GlobalSignalNumbers.h>
#include <signaldata/TestOrd.hpp>
#include <signaldata/TamperOrd.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/ApiVersion.hpp>
#include <signaldata/ResumeReq.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <signaldata/EventSubscribeReq.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/BackupSignalData.hpp>
#include <signaldata/GrepImpl.hpp>
#include <signaldata/ManagementServer.hpp>
#include <NdbSleep.h>
#include <EventLogger.hpp>
#include <DebuggerNames.hpp>
#include <ndb_version.h>

#include <SocketServer.hpp>
#include <NdbConfig.h>

#include <NdbAutoPtr.hpp>

#include <ndberror.h>

#include <mgmapi.h>
#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters.h>
#include <m_string.h>
#include <Transporter.hpp>

//#define MGM_SRV_DEBUG
#ifdef MGM_SRV_DEBUG
#define DEBUG(x) do ndbout << x << endl; while(0)
#else
#define DEBUG(x)
#endif

extern int global_flag_send_heartbeat_now;
extern int g_no_nodeid_checks;

void *
MgmtSrvr::logLevelThread_C(void* m)
{
  MgmtSrvr *mgm = (MgmtSrvr*)m;
  mgm->logLevelThreadRun();
  return 0;
}

void *
MgmtSrvr::signalRecvThread_C(void *m) 
{
  MgmtSrvr *mgm = (MgmtSrvr*)m;
  mgm->signalRecvThreadRun();
  return 0;
}

class SigMatch 
{
public:
  int gsn;
  void (MgmtSrvr::* function)(NdbApiSignal *signal);

  SigMatch() { gsn = 0; function = NULL; };

  SigMatch(int _gsn,
	   void (MgmtSrvr::* _function)(NdbApiSignal *signal)) {
    gsn = _gsn;
    function = _function;
  };
  
  bool check(NdbApiSignal *signal) {
    if(signal->readSignalNumber() == gsn)
      return true;
    return false;
  };
  
};

void
MgmtSrvr::signalRecvThreadRun() 
{
  Vector<SigMatch> siglist;
  siglist.push_back(SigMatch(GSN_MGM_LOCK_CONFIG_REQ,
			     &MgmtSrvr::handle_MGM_LOCK_CONFIG_REQ));
  siglist.push_back(SigMatch(GSN_MGM_UNLOCK_CONFIG_REQ,
			     &MgmtSrvr::handle_MGM_UNLOCK_CONFIG_REQ));
  
  while(!_isStopThread) {
    SigMatch *handler = NULL;
    NdbApiSignal *signal = NULL;
    if(m_signalRecvQueue.waitFor(siglist, &handler, &signal, DEFAULT_TIMEOUT)) {
      if(handler->function != 0)
	(this->*handler->function)(signal);
    }
  }
}

extern EventLogger g_eventLogger;

static NdbOut&
operator<<(NdbOut& out, const LogLevel & ll)
{
  out << "[LogLevel: ";
  for(size_t i = 0; i<LogLevel::LOGLEVEL_CATEGORIES; i++)
    out << ll.getLogLevel((LogLevel::EventCategory)i) << " ";
  out << "]";
  return out;
}

void
MgmtSrvr::logLevelThreadRun() 
{
  while (!_isStopThread) {
    /**
     * Handle started nodes
     */
    EventSubscribeReq req;
    req = m_event_listner[0].m_logLevel;
    req.blockRef = _ownReference;

    SetLogLevelOrd ord;
    
    m_started_nodes.lock();
    while(m_started_nodes.size() > 0){
      Uint32 node = m_started_nodes[0];
      m_started_nodes.erase(0, false);
      m_started_nodes.unlock();

      setEventReportingLevelImpl(node, req);
      
      ord = m_nodeLogLevel[node];
      setNodeLogLevelImpl(node, ord);
      
      m_started_nodes.lock();
    }				 
    m_started_nodes.unlock();
    
    m_log_level_requests.lock();
    while(m_log_level_requests.size() > 0){
      req = m_log_level_requests[0];
      m_log_level_requests.erase(0, false);
      m_log_level_requests.unlock();
      
      LogLevel tmp;
      tmp = req;
      
      if(req.blockRef == 0){
	req.blockRef = _ownReference;
	setEventReportingLevelImpl(0, req);
      } else {
	ord = req;
	setNodeLogLevelImpl(req.blockRef, ord);
      }
      m_log_level_requests.lock();
    }      
    m_log_level_requests.unlock();
    NdbSleep_MilliSleep(_logLevelThreadSleep);  
  }
}

void
MgmtSrvr::startEventLog() 
{
  NdbMutex_Lock(m_configMutex);

  g_eventLogger.setCategory("MgmSrvr");

  ndb_mgm_configuration_iterator 
    iter(* _config->m_configValues, CFG_SECTION_NODE);

  if(iter.find(CFG_NODE_ID, _ownNodeId) != 0){
    NdbMutex_Unlock(m_configMutex);
    return;
  }
  
  const char * tmp;
  BaseString logdest;
  char *clusterLog= NdbConfig_ClusterLogFileName(_ownNodeId);
  NdbAutoPtr<char> tmp_aptr(clusterLog);

  if(iter.get(CFG_LOG_DESTINATION, &tmp) == 0){
    logdest.assign(tmp);
  }
  NdbMutex_Unlock(m_configMutex);
  
  if(logdest.length() == 0 || logdest == "") {
    logdest.assfmt("FILE:filename=%s,maxsize=1000000,maxfiles=6", 
		   clusterLog);
  }
  if(!g_eventLogger.addHandler(logdest)) {
    ndbout << "Warning: could not add log destination \""
	   << logdest.c_str() << "\"" << endl;
  }
}

void 
MgmtSrvr::stopEventLog() 
{
  // Nothing yet
}

class ErrorItem 
{
public:
  int _errorCode;
  const char * _errorText;
};

bool
MgmtSrvr::setEventLogFilter(int severity, int enable)
{
  Logger::LoggerLevel level = (Logger::LoggerLevel)severity;
  if (enable > 0) {
    g_eventLogger.enable(level);
  } else if (enable == 0) {
    g_eventLogger.disable(level);
  } else if (g_eventLogger.isEnable(level)) {
    g_eventLogger.disable(level);
  } else {
    g_eventLogger.enable(level);
  }
  return g_eventLogger.isEnable(level);
}

bool 
MgmtSrvr::isEventLogFilterEnabled(int severity) 
{
  return g_eventLogger.isEnable((Logger::LoggerLevel)severity);
}

static ErrorItem errorTable[] = 
{
  {MgmtSrvr::NO_CONTACT_WITH_PROCESS, "No contact with the process (dead ?)."},
  {MgmtSrvr::PROCESS_NOT_CONFIGURED, "The process is not configured."},
  {MgmtSrvr::WRONG_PROCESS_TYPE, 
   "The process has wrong type. Expected a DB process."},
  {MgmtSrvr::COULD_NOT_ALLOCATE_MEMORY, "Could not allocate memory."},
  {MgmtSrvr::SEND_OR_RECEIVE_FAILED, "Send to process or receive failed."},
  {MgmtSrvr::INVALID_LEVEL, "Invalid level. Should be between 1 and 30."},
  {MgmtSrvr::INVALID_ERROR_NUMBER, "Invalid error number. Should be >= 0."},
  {MgmtSrvr::INVALID_TRACE_NUMBER, "Invalid trace number."},
  {MgmtSrvr::NOT_IMPLEMENTED, "Not implemented."},
  {MgmtSrvr::INVALID_BLOCK_NAME, "Invalid block name"},

  {MgmtSrvr::CONFIG_PARAM_NOT_EXIST, 
   "The configuration parameter does not exist for the process type."},
  {MgmtSrvr::CONFIG_PARAM_NOT_UPDATEABLE, 
   "The configuration parameter is not possible to update."},
  {MgmtSrvr::VALUE_WRONG_FORMAT_INT_EXPECTED, 
   "Incorrect value. Expected integer."},
  {MgmtSrvr::VALUE_TOO_LOW, "Value is too low."},
  {MgmtSrvr::VALUE_TOO_HIGH, "Value is too high."},
  {MgmtSrvr::VALUE_WRONG_FORMAT_BOOL_EXPECTED, 
   "Incorrect value. Expected TRUE or FALSE."},

  {MgmtSrvr::CONFIG_FILE_OPEN_WRITE_ERROR, 
   "Could not open configuration file for writing."},
  {MgmtSrvr::CONFIG_FILE_OPEN_READ_ERROR, 
   "Could not open configuration file for reading."},
  {MgmtSrvr::CONFIG_FILE_WRITE_ERROR, 
   "Write error when writing configuration file."},
  {MgmtSrvr::CONFIG_FILE_READ_ERROR, 
   "Read error when reading configuration file."},
  {MgmtSrvr::CONFIG_FILE_CLOSE_ERROR, "Could not close configuration file."},

  {MgmtSrvr::CONFIG_CHANGE_REFUSED_BY_RECEIVER, 
   "The change was refused by the receiving process."},
  {MgmtSrvr::COULD_NOT_SYNC_CONFIG_CHANGE_AGAINST_PHYSICAL_MEDIUM, 
   "The change could not be synced against physical medium."},
  {MgmtSrvr::CONFIG_FILE_CHECKSUM_ERROR, 
   "The config file is corrupt. Checksum error."},
  {MgmtSrvr::NOT_POSSIBLE_TO_SEND_CONFIG_UPDATE_TO_PROCESS_TYPE, 
   "It is not possible to send an update of a configuration variable "
   "to this kind of process."},
  {5026, "Node shutdown in progress" },
  {5027, "System shutdown in progress" },
  {5028, "Node shutdown would cause system crash" },
  {5029, "Only one shutdown at a time is possible via mgm server" },
  {5060, "Operation not allowed in single user mode." }, 
  {5061, "DB is not in single user mode." },
  {5062, "The specified node is not an API node." },
  {5063, 
   "Cannot enter single user mode. DB nodes in inconsistent startlevel."},
  {MgmtSrvr::NO_CONTACT_WITH_DB_NODES, "No contact with database nodes" }
};

int MgmtSrvr::translateStopRef(Uint32 errCode)
{
  switch(errCode){
  case StopRef::NodeShutdownInProgress:
    return 5026;
    break;
  case StopRef::SystemShutdownInProgress:
    return 5027;
    break;
  case StopRef::NodeShutdownWouldCauseSystemCrash:
    return 5028;
    break;
  }
  return 4999;
}

static int noOfErrorCodes = sizeof(errorTable) / sizeof(ErrorItem);

int 
MgmtSrvr::getNodeCount(enum ndb_mgm_node_type type) const 
{
  int count = 0;
  NodeId nodeId = 0;

  while (getNextNodeId(&nodeId, type)) {
    count++;
  }
  return count;
}

int 
MgmtSrvr::getPort() const
{
  if(NdbMutex_Lock(m_configMutex))
    return 0;

  ndb_mgm_configuration_iterator 
    iter(* _config->m_configValues, CFG_SECTION_NODE);

  if(iter.find(CFG_NODE_ID, getOwnNodeId()) != 0){
    ndbout << "Could not retrieve configuration for Node " 
	   << getOwnNodeId() << " in config file." << endl 
	   << "Have you set correct NodeId for this node?" << endl;
    NdbMutex_Unlock(m_configMutex);
    return 0;
  }

  unsigned type;
  if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0 ||
     type != NODE_TYPE_MGM){
    ndbout << "Local node id " << getOwnNodeId()
	   << " is not defined as management server" << endl
	   << "Have you set correct NodeId for this node?" << endl;
    NdbMutex_Unlock(m_configMutex);
    return 0;
  }
  
  Uint32 port = 0;
  if(iter.get(CFG_MGM_PORT, &port) != 0){
    ndbout << "Could not find PortNumber in the configuration file." << endl;
    NdbMutex_Unlock(m_configMutex);
    return 0;
  }

  NdbMutex_Unlock(m_configMutex);

  return port;
}

/* Constructor */
int MgmtSrvr::init()
{
  if ( _ownNodeId > 0)
    return 0;
  return -1;
}

MgmtSrvr::MgmtSrvr(SocketServer *socket_server,
		   const char *config_filename,
		   const char *connect_string) :
  _blockNumber(1), // Hard coded block number since it makes it easy to send
                   // signals to other management servers.
  m_socket_server(socket_server),
  _ownReference(0),
  theSignalIdleList(NULL),
  theWaitState(WAIT_SUBSCRIBE_CONF),
  m_event_listner(this)
{
    
  DBUG_ENTER("MgmtSrvr::MgmtSrvr");

  _ownNodeId= 0;

  _config     = NULL;

  _isStopThread        = false;
  _logLevelThread      = NULL;
  _logLevelThreadSleep = 500;
  m_signalRecvThread   = NULL;

  theFacade = 0;

  m_newConfig = NULL;
  if (config_filename)
    m_configFilename.assign(config_filename);
  else
    m_configFilename.assign("config.ini");

  m_nextConfigGenerationNumber = 0;

  m_config_retriever= new ConfigRetriever(connect_string,
					  NDB_VERSION, NDB_MGM_NODE_TYPE_MGM);
  // if connect_string explicitly given or
  // no config filename is given then
  // first try to allocate nodeid from another management server
  if ((connect_string || config_filename == NULL) &&
      (m_config_retriever->do_connect(0,0,0) == 0))
  {
    int tmp_nodeid= 0;
    tmp_nodeid= m_config_retriever->allocNodeId(0 /*retry*/,0 /*delay*/);
    if (tmp_nodeid == 0)
    {
      ndbout_c(m_config_retriever->getErrorString());
      exit(-1);
    }
    // read config from other managent server
    _config= fetchConfig();
    if (_config == 0)
    {
      ndbout << m_config_retriever->getErrorString() << endl;
      exit(-1);
    }
    _ownNodeId= tmp_nodeid;
  }

  if (_ownNodeId == 0)
  {
    // read config locally
    _config= readConfig();
    if (_config == 0) {
      ndbout << "Unable to read config file" << endl;
      exit(-1);
    }
  }

  theMgmtWaitForResponseCondPtr = NdbCondition_Create();

  m_configMutex = NdbMutex_Create();

  /**
   * Fill the nodeTypes array
   */
  for(Uint32 i = 0; i<MAX_NODES; i++) {
    nodeTypes[i] = (enum ndb_mgm_node_type)-1;
    m_connect_address[i].s_addr= 0;
  }

  {
    ndb_mgm_configuration_iterator
      iter(* _config->m_configValues, CFG_SECTION_NODE);

    for(iter.first(); iter.valid(); iter.next()){
      unsigned type, id;
      if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0)
	continue;
      
      if(iter.get(CFG_NODE_ID, &id) != 0)
	continue;
      
      MGM_REQUIRE(id < MAX_NODES);
      
      switch(type){
      case NODE_TYPE_DB:
	nodeTypes[id] = NDB_MGM_NODE_TYPE_NDB;
	break;
      case NODE_TYPE_API:
	nodeTypes[id] = NDB_MGM_NODE_TYPE_API;
	break;
      case NODE_TYPE_MGM:
	nodeTypes[id] = NDB_MGM_NODE_TYPE_MGM;
	break;
      case NODE_TYPE_REP:
	nodeTypes[id] = NDB_MGM_NODE_TYPE_REP;
	break;
      case NODE_TYPE_EXT_REP:
      default:
	break;
      }
    }
  }

  _props = NULL;
  BaseString error_string;

  if ((m_node_id_mutex = NdbMutex_Create()) == 0)
  {
    ndbout << "mutex creation failed line = " << __LINE__ << endl;
    exit(-1);
  }

  if (_ownNodeId == 0) // we did not get node id from other server
  {
    NodeId tmp= m_config_retriever->get_configuration_nodeid();

    if (!alloc_node_id(&tmp, NDB_MGM_NODE_TYPE_MGM,
		       0, 0, error_string)){
      ndbout << "Unable to obtain requested nodeid: "
	     << error_string.c_str() << endl;
      exit(-1);
    }
    _ownNodeId = tmp;
  }

  {
    DBUG_PRINT("info", ("verifyConfig"));
    if (!m_config_retriever->verifyConfig(_config->m_configValues,
					  _ownNodeId))
    {
      ndbout << m_config_retriever->getErrorString() << endl;
      exit(-1);
    }
  }

  // Setup clusterlog as client[0] in m_event_listner
  {
    Ndb_mgmd_event_service::Event_listener se;
    se.m_socket = NDB_INVALID_SOCKET;
    for(size_t t = 0; t<LogLevel::LOGLEVEL_CATEGORIES; t++){
      se.m_logLevel.setLogLevel((LogLevel::EventCategory)t, 7);
    }
    se.m_logLevel.setLogLevel(LogLevel::llError, 15);
    se.m_logLevel.setLogLevel(LogLevel::llConnection, 8);
    se.m_logLevel.setLogLevel(LogLevel::llBackup, 15);
    m_event_listner.m_clients.push_back(se);
    m_event_listner.m_logLevel = se.m_logLevel;
  }
  
  DBUG_VOID_RETURN;
}


//****************************************************************************
//****************************************************************************
bool 
MgmtSrvr::check_start() 
{
  if (_config == 0) {
    DEBUG("MgmtSrvr.cpp: _config is NULL.");
    return false;
  }

  return true;
}

bool 
MgmtSrvr::start(BaseString &error_string)
{
  if (_props == NULL) {
    if (!check_start()) {
      error_string.append("MgmtSrvr.cpp: check_start() failed.");
      return false;
    }
  }
  theFacade= TransporterFacade::theFacadeInstance
    = new TransporterFacade();
  
  if(theFacade == 0) {
    DEBUG("MgmtSrvr.cpp: theFacade is NULL.");
    error_string.append("MgmtSrvr.cpp: theFacade is NULL.");
    return false;
  }  
  if ( theFacade->start_instance
       (_ownNodeId, (ndb_mgm_configuration*)_config->m_configValues) < 0) {
    DEBUG("MgmtSrvr.cpp: TransporterFacade::start_instance < 0.");
    return false;
  }

  MGM_REQUIRE(_blockNumber == 1);

  // Register ourself at TransporterFacade to be able to receive signals
  // and to be notified when a database process has died.
  _blockNumber = theFacade->open(this,
				 signalReceivedNotification,
				 nodeStatusNotification);
  
  if(_blockNumber == -1){
    DEBUG("MgmtSrvr.cpp: _blockNumber is -1.");
    error_string.append("MgmtSrvr.cpp: _blockNumber is -1.");
    theFacade->stop_instance();
    theFacade = 0;
    return false;
  }

  TransporterRegistry *reg = theFacade->get_registry();
  for(unsigned int i=0;i<reg->m_transporter_interface.size();i++) {
    BaseString msg;
    DBUG_PRINT("info",("Setting dynamic port %d->%d : %d",
		       reg->get_localNodeId(),
		       reg->m_transporter_interface[i].m_remote_nodeId,
		       reg->m_transporter_interface[i].m_s_service_port
		       )
	       );
    int res = setConnectionDbParameter((int)reg->get_localNodeId(),
				       (int)reg->m_transporter_interface[i]
				            .m_remote_nodeId,
				       (int)CFG_CONNECTION_SERVER_PORT,
				       reg->m_transporter_interface[i]
				            .m_s_service_port,
					 msg);
    DBUG_PRINT("info",("Set result: %d: %s",res,msg.c_str()));
  }

  
  _ownReference = numberToRef(_blockNumber, _ownNodeId);
  
  startEventLog();
  // Set the initial confirmation count for subscribe requests confirm
  // from NDB nodes in the cluster.
  //
  // Loglevel thread
  _logLevelThread = NdbThread_Create(logLevelThread_C,
				     (void**)this,
				     32768,
				     "MgmtSrvr_Loglevel",
				     NDB_THREAD_PRIO_LOW);

  m_signalRecvThread = NdbThread_Create(signalRecvThread_C,
					(void **)this,
					32768,
					"MgmtSrvr_Service",
					NDB_THREAD_PRIO_LOW);

  return true;
}


//****************************************************************************
//****************************************************************************
MgmtSrvr::~MgmtSrvr() 
{
  while (theSignalIdleList != NULL) {
    freeSignal();
  }

  if(theFacade != 0){
    theFacade->stop_instance();
    theFacade = 0;
  }

  stopEventLog();

  NdbMutex_Destroy(m_node_id_mutex);
  NdbCondition_Destroy(theMgmtWaitForResponseCondPtr);
  NdbMutex_Destroy(m_configMutex);

  if(m_newConfig != NULL)
    free(m_newConfig);

  if(_config != NULL)
    delete _config;

  // End set log level thread
  void* res = 0;
  _isStopThread = true;

  if (_logLevelThread != NULL) {
    NdbThread_WaitFor(_logLevelThread, &res);
    NdbThread_Destroy(&_logLevelThread);
  }

  if (m_signalRecvThread != NULL) {
    NdbThread_WaitFor(m_signalRecvThread, &res);
    NdbThread_Destroy(&m_signalRecvThread);
  }
  if (m_config_retriever)
    delete m_config_retriever;
}

//****************************************************************************
//****************************************************************************

int MgmtSrvr::okToSendTo(NodeId processId, bool unCond) 
{
  if(processId == 0)
    return 0;

  if (getNodeType(processId) != NDB_MGM_NODE_TYPE_NDB)
    return WRONG_PROCESS_TYPE;
  
  // Check if we have contact with it
  if(unCond){
    if(theFacade->theClusterMgr->getNodeInfo(processId).connected)
      return 0;
    return NO_CONTACT_WITH_PROCESS;
  }
  if (theFacade->get_node_alive(processId) == 0) {
    return NO_CONTACT_WITH_PROCESS;
  } else {
    return 0;
  }
}

/*****************************************************************************
 * Starting and stopping database nodes
 ****************************************************************************/

int 
MgmtSrvr::start(int processId)
{
  int result;
  
  result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }
  
  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  StartOrd* const startOrd = CAST_PTR(StartOrd, signal->getDataPtrSend());
  signal->set(TestOrd::TraceAPI, CMVMI, GSN_START_ORD, StartOrd::SignalLength);
  
  startOrd->restartInfo = 0;
  
  result = sendSignal(processId, NO_WAIT, signal, true);
  if (result == -1) {
    return SEND_OR_RECEIVE_FAILED;
  }
  
  return 0;
}

/**
 * Restart one database node
 */
int
MgmtSrvr::restartNode(int processId, bool nostart, 
		      bool initalStart, bool abort,
		      StopCallback callback, void * anyData) 
{
  int result;

  if(m_stopRec.singleUserMode)
    return 5060;

  if(m_stopRec.inUse){
    return 5029;
  }
  
  result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }
  
  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  StopReq* const stopReq = CAST_PTR(StopReq, signal->getDataPtrSend());
  signal->set(TestOrd::TraceAPI, NDBCNTR, GSN_STOP_REQ, StopReq::SignalLength);
  
  stopReq->requestInfo = 0;
  StopReq::setSystemStop(stopReq->requestInfo, false);
  StopReq::setPerformRestart(stopReq->requestInfo, true);
  StopReq::setNoStart(stopReq->requestInfo, nostart);
  StopReq::setInitialStart(stopReq->requestInfo, initalStart);
  StopReq::setStopAbort(stopReq->requestInfo, abort);
  stopReq->singleuser = 0;
  stopReq->apiTimeout = 5000;
  stopReq->transactionTimeout = 1000;
  stopReq->readOperationTimeout = 1000;
  stopReq->operationTimeout = 1000;
  stopReq->senderData = 12;
  stopReq->senderRef = _ownReference;

  m_stopRec.singleUserMode = false;
  m_stopRec.sentCount = 1;
  m_stopRec.reply = 0;
  m_stopRec.nodeId = processId;
  m_stopRec.anyData = anyData;
  m_stopRec.callback = callback;
  m_stopRec.inUse = true;
  
  if(callback == NULL){
    Uint32 timeOut = 0;
    timeOut += stopReq->apiTimeout;
    timeOut += stopReq->transactionTimeout;
    timeOut += stopReq->readOperationTimeout;
    timeOut += stopReq->operationTimeout;
    timeOut *= 3;
    result = sendRecSignal(processId, WAIT_STOP, signal, true, timeOut);
  } else {
    result = sendSignal(processId, NO_WAIT, signal, true);
  }
  
  if (result == -1) {
    m_stopRec.inUse = false;
    return SEND_OR_RECEIVE_FAILED;
  }

  if(callback == 0){
    m_stopRec.inUse = false;
    return m_stopRec.reply;
  } else {
    return 0;
  }
}

/**
 * Restart all database nodes
 */
int
MgmtSrvr::restart(bool nostart, bool initalStart, bool abort,
		  int * stopCount, StopCallback callback, void * anyData) 
{
  if(m_stopRec.singleUserMode)
    return 5060;  

  if(m_stopRec.inUse){
    return 5029;
  }
  
  m_stopRec.singleUserMode = false;
  m_stopRec.sentCount = 0;
  m_stopRec.reply = 0;
  m_stopRec.nodeId = 0;
  m_stopRec.anyData = anyData;
  m_stopRec.callback = callback;
  m_stopRec.inUse = true;
  
  /**
   * Restart all database nodes into idle ("no-started") state
   */
  Uint32 timeOut = 0;
  NodeId nodeId = 0;
  NodeBitmask nodes;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)){
    if(okToSendTo(nodeId, true) == 0){

      NdbApiSignal* signal = getSignal();
      if (signal == NULL) {
	return COULD_NOT_ALLOCATE_MEMORY;
      }
      
      StopReq* const stopReq = CAST_PTR(StopReq, signal->getDataPtrSend());
      signal->set(TestOrd::TraceAPI, NDBCNTR, GSN_STOP_REQ, 
		  StopReq::SignalLength);
      
      stopReq->requestInfo = 0;
      stopReq->singleuser = 0;
      StopReq::setSystemStop(stopReq->requestInfo, true);
      StopReq::setPerformRestart(stopReq->requestInfo, true);
      if (callback == 0) {
	// Start node in idle ("no-started") state
	StopReq::setNoStart(stopReq->requestInfo, 1); 
      } else {
	StopReq::setNoStart(stopReq->requestInfo, nostart); 
      }
      StopReq::setInitialStart(stopReq->requestInfo, initalStart);
      StopReq::setStopAbort(stopReq->requestInfo, abort);
      
      stopReq->apiTimeout = 5000;
      stopReq->transactionTimeout = 1000;
      stopReq->readOperationTimeout = 1000;
      stopReq->operationTimeout = 1000;
      stopReq->senderData = 12;
      stopReq->senderRef = _ownReference;

      timeOut += stopReq->apiTimeout;
      timeOut += stopReq->transactionTimeout;
      timeOut += stopReq->readOperationTimeout;
      timeOut += stopReq->operationTimeout;
      timeOut *= 3;
      
      m_stopRec.sentCount++;
      int res;
      if(callback == 0){
	res = sendSignal(nodeId, WAIT_STOP, signal, true);
      } else {
	res = sendSignal(nodeId, NO_WAIT, signal, true);
      }
      
      if(res != -1){
	nodes.set(nodeId);
      }
    }
  }
  
  if(stopCount != 0){
    * stopCount = m_stopRec.sentCount;
  }

  if(m_stopRec.sentCount == 0){
    m_stopRec.inUse = false;
    return 0;
  }
  
  if(callback != 0){
    return 0;
  }
  
  theFacade->lock_mutex();
  int waitTime = timeOut/m_stopRec.sentCount;
  if (receiveOptimisedResponse(waitTime) != 0) {
    m_stopRec.inUse = false;
    return -1;
  }
  
  /**
   * Here all nodes were correctly stopped,
   * so we wait for all nodes to be contactable
   */
  nodeId = 0;
  NDB_TICKS maxTime = NdbTick_CurrentMillisecond() + waitTime;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB) && nodes.get(nodeId)) {
    enum ndb_mgm_node_status s;
    s = NDB_MGM_NODE_STATUS_NO_CONTACT;
    while (s != NDB_MGM_NODE_STATUS_NOT_STARTED && waitTime > 0) {
      Uint32 startPhase = 0, version = 0, dynamicId = 0, nodeGroup = 0;
      Uint32 connectCount = 0;
      bool system;
      status(nodeId, &s, &version, &startPhase, 
	     &system, &dynamicId, &nodeGroup, &connectCount);
      NdbSleep_MilliSleep(100);  
      waitTime = (maxTime - NdbTick_CurrentMillisecond());
    }
  }
  
  if(nostart){
    m_stopRec.inUse = false;
    return 0;
  }
  
  /**
   * Now we start all database nodes (i.e. we make them non-idle)
   * We ignore the result we get from the start command.
   */
  nodeId = 0;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB) && nodes.get(nodeId)) {
    int result;
    result = start(nodeId);
    DEBUG("Starting node " << nodeId << " with result " << result);
    /**
     * Errors from this call are deliberately ignored.
     * Maybe the user only wanted to restart a subset of the nodes.
     * It is also easy for the user to check which nodes have 
     * started and which nodes have not.
     *
     * if (result != 0) {
     *   m_stopRec.inUse = false;
     *   return result;
     * }
     */
  }
  
  m_stopRec.inUse = false;
  return 0;
}

/*****************************************************************************
 * Version handling
 *****************************************************************************/

int 
MgmtSrvr::versionNode(int processId, bool abort, 
		      VersionCallback callback, void * anyData)
{
  int version;

  if(m_versionRec.inUse)
    return OPERATION_IN_PROGRESS;

  m_versionRec.callback = callback;
  m_versionRec.inUse = true ;

  if (getOwnNodeId() == processId)
  {
    version= NDB_VERSION;
  }
  else if (getNodeType(processId) == NDB_MGM_NODE_TYPE_NDB)
  {
    ClusterMgr::Node node= theFacade->theClusterMgr->getNodeInfo(processId);
    if(node.connected)
      version= node.m_info.m_version;
    else
      version= 0;
  }
  else if (getNodeType(processId) == NDB_MGM_NODE_TYPE_API ||
	   getNodeType(processId) == NDB_MGM_NODE_TYPE_MGM)
  {
    return sendVersionReq(processId);
  }
  else
    version= 0;

  if(m_versionRec.callback != 0)
    m_versionRec.callback(processId, version, this,0);
  m_versionRec.inUse = false ;

  m_versionRec.version[processId]= version;

  return 0;
}

int 
MgmtSrvr::sendVersionReq(int processId) 
{
  Uint32 ndbnode=0;
  int result;
  for(Uint32 i = 0; i<MAX_NODES; i++) {
    if (getNodeType(i) == NDB_MGM_NODE_TYPE_NDB) {
      if(okToSendTo(i, true) == 0) 
	{
	  ndbnode = i;
	  break;
	}
    }
  }
  
  if (ndbnode == 0) {
    m_versionRec.inUse = false;
    if(m_versionRec.callback != 0)
      m_versionRec.callback(processId, 0, this,0);
    return NO_CONTACT_WITH_CLUSTER;
  }
  
  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    m_versionRec.inUse = false;
    if(m_versionRec.callback != 0)
      m_versionRec.callback(processId, 0, this,0);
    return COULD_NOT_ALLOCATE_MEMORY;
  }
  ApiVersionReq* req = CAST_PTR(ApiVersionReq, signal->getDataPtrSend());
  req->senderRef = _ownReference;
  req->nodeId = processId;
  
  signal->set(TestOrd::TraceAPI, QMGR, GSN_API_VERSION_REQ, 
	      ApiVersionReq::SignalLength);
  
  
  //  if(m_versionRec.callback == 0){
  Uint32 timeOut = 0;
  timeOut = 10000;
  result = sendRecSignal(ndbnode, WAIT_VERSION, signal, true, timeOut);
  //} else {
  //result = sendSignal(processId, NO_WAIT, signal, true);
  // }
  
  if (result == -1) {
    m_versionRec.inUse = false;
    if(m_versionRec.callback != 0)
      m_versionRec.callback(processId, 0, this,0);
    m_versionRec.version[processId] = 0;
    return SEND_OR_RECEIVE_FAILED;
  }
  
  m_versionRec.inUse = false;
  return 0;
}

int
MgmtSrvr::version(int * stopCount, bool abort, 
		  VersionCallback callback, void * anyData) 
{
  ClusterMgr::Node node;
  int version;

  if(m_versionRec.inUse)
    return 1;

  m_versionRec.callback = callback;
  m_versionRec.inUse = true ;
  Uint32 i; 
  for(i = 0; i<MAX_NODES; i++) {
    if (getNodeType(i) == NDB_MGM_NODE_TYPE_MGM) {
      m_versionRec.callback(i, NDB_VERSION, this,0);
    }
  }
  for(i = 0; i<MAX_NODES; i++) {
    if (getNodeType(i) == NDB_MGM_NODE_TYPE_NDB) {
      node = theFacade->theClusterMgr->getNodeInfo(i);
      version = node.m_info.m_version;
      if(theFacade->theClusterMgr->getNodeInfo(i).connected)
	m_versionRec.callback(i, version, this,0);
      else
	m_versionRec.callback(i, 0, this,0);
      
    }
  }
  for(i = 0; i<MAX_NODES; i++) {
    if (getNodeType(i) == NDB_MGM_NODE_TYPE_API) {
      return sendVersionReq(i);   
    }
  }
  
  return 0;
}

int 
MgmtSrvr::stopNode(int processId, bool abort, StopCallback callback, 
		   void * anyData) 
		   
{
  if(m_stopRec.singleUserMode)
    return 5060;

  if(m_stopRec.inUse)
    return 5029;

  int result;

  result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }
  
  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  StopReq* const stopReq = CAST_PTR(StopReq, signal->getDataPtrSend());
  signal->set(TestOrd::TraceAPI, NDBCNTR, GSN_STOP_REQ, StopReq::SignalLength);
  
  stopReq->requestInfo = 0;
  stopReq->singleuser = 0;
  StopReq::setPerformRestart(stopReq->requestInfo, false);
  StopReq::setSystemStop(stopReq->requestInfo, false);
  StopReq::setStopAbort(stopReq->requestInfo, abort);

  stopReq->apiTimeout = 5000;
  stopReq->transactionTimeout = 1000;
  stopReq->readOperationTimeout = 1000;
  stopReq->operationTimeout = 1000;
  stopReq->senderData = 12;
  stopReq->senderRef = _ownReference;

  m_stopRec.sentCount = 1;
  m_stopRec.reply = 0;
  m_stopRec.nodeId = processId;
  m_stopRec.anyData = anyData;
  m_stopRec.callback = callback;
  m_stopRec.inUse = true;

  if(callback == NULL){
    Uint32 timeOut = 0;
    timeOut += stopReq->apiTimeout;
    timeOut += stopReq->transactionTimeout;
    timeOut += stopReq->readOperationTimeout;
    timeOut += stopReq->operationTimeout;
    timeOut *= 3;
    result = sendRecSignal(processId, WAIT_STOP, signal, true, timeOut);
  } else {
    result = sendSignal(processId, NO_WAIT, signal, true);
  }

  if (result == -1) {
    m_stopRec.inUse = false;
    return SEND_OR_RECEIVE_FAILED;
  }

  if(callback == 0){
    m_stopRec.inUse = false;
    return m_stopRec.reply;
  } else {
    return 0;
  }
}

int
MgmtSrvr::stop(int * stopCount, bool abort, StopCallback callback, 
	       void * anyData) 
{
  if(m_stopRec.singleUserMode)
    return 5060;

  if(m_stopRec.inUse){
    return 5029;
  }
  
  m_stopRec.singleUserMode = false;
  m_stopRec.sentCount = 0;
  m_stopRec.reply = 0;
  m_stopRec.nodeId = 0;
  m_stopRec.anyData = anyData;
  m_stopRec.callback = callback;
  m_stopRec.inUse = true;
  
  NodeId nodeId = 0;
  Uint32 timeOut = 0;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)){
    if(okToSendTo(nodeId, true) == 0){
  
      NdbApiSignal* signal = getSignal();
      if (signal == NULL) {
	return COULD_NOT_ALLOCATE_MEMORY;
      }
      
      StopReq* const stopReq = CAST_PTR(StopReq, signal->getDataPtrSend());
      signal->set(TestOrd::TraceAPI, NDBCNTR, GSN_STOP_REQ, 
		  StopReq::SignalLength);
      
      stopReq->requestInfo = 0;
      stopReq->singleuser = 0;
      StopReq::setSystemStop(stopReq->requestInfo, true);
      StopReq::setPerformRestart(stopReq->requestInfo, false);
      StopReq::setStopAbort(stopReq->requestInfo, abort);
      
      stopReq->apiTimeout = 5000;
      stopReq->transactionTimeout = 1000;
      stopReq->readOperationTimeout = 1000;
      stopReq->operationTimeout = 1000;
      stopReq->senderData = 12;
      stopReq->senderRef = _ownReference;

      timeOut += stopReq->apiTimeout;
      timeOut += stopReq->transactionTimeout;
      timeOut += stopReq->readOperationTimeout;
      timeOut += stopReq->operationTimeout;
      timeOut *= 3;
      
      m_stopRec.sentCount++;
      if(callback == 0)
	sendSignal(nodeId, WAIT_STOP, signal, true);
      else
	sendSignal(nodeId, NO_WAIT, signal, true);
    }
  }

  if(stopCount != 0)
    * stopCount = m_stopRec.sentCount;
  
  if(m_stopRec.sentCount > 0){
    if(callback == 0){
      theFacade->lock_mutex();
      receiveOptimisedResponse(timeOut / m_stopRec.sentCount);
    } else {
      return 0;
    }
  }
  
  m_stopRec.inUse = false;
  return m_stopRec.reply;
}

/*****************************************************************************
 * Single user mode
 ****************************************************************************/

int
MgmtSrvr::enterSingleUser(int * stopCount, Uint32 singleUserNodeId, 
			  EnterSingleCallback callback, void * anyData) 
{
  if(m_stopRec.singleUserMode) {
    return 5060;
  }

  if (getNodeType(singleUserNodeId) != NDB_MGM_NODE_TYPE_API) {
    return 5062;
  }  
  ClusterMgr::Node node;

  for(Uint32 i = 0; i<MAX_NODES; i++) {
    if (getNodeType(i) == NDB_MGM_NODE_TYPE_NDB) {
      node = theFacade->theClusterMgr->getNodeInfo(i);
      if((node.m_state.startLevel != NodeState::SL_STARTED) && 
	 (node.m_state.startLevel != NodeState::SL_NOTHING)) {
	return 5063;
      }
    }
  }
      
  if(m_stopRec.inUse){
    return 5029;
  }

  if(singleUserNodeId == 0) 
    return 1;
  m_stopRec.singleUserMode =  true;
  m_stopRec.sentCount = 0;
  m_stopRec.reply = 0;
  m_stopRec.nodeId = 0;
  m_stopRec.anyData = anyData;
  m_stopRec.callback = callback;
  m_stopRec.inUse = true;
  
  NodeId nodeId = 0;
  Uint32 timeOut = 0;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)){
    if(okToSendTo(nodeId, true) == 0){
  
      NdbApiSignal* signal = getSignal();
      if (signal == NULL) {
	return COULD_NOT_ALLOCATE_MEMORY;
      }
      
      StopReq* const stopReq = CAST_PTR(StopReq, signal->getDataPtrSend());
      signal->set(TestOrd::TraceAPI, NDBCNTR, GSN_STOP_REQ, 
		  StopReq::SignalLength);
      
      stopReq->requestInfo = 0;
      stopReq->singleuser = 1;
      stopReq->singleUserApi = singleUserNodeId;
      StopReq::setSystemStop(stopReq->requestInfo, false);
      StopReq::setPerformRestart(stopReq->requestInfo, false);
      StopReq::setStopAbort(stopReq->requestInfo, false);

      stopReq->apiTimeout = 5000;
      stopReq->transactionTimeout = 1000;
      stopReq->readOperationTimeout = 1000;
      stopReq->operationTimeout = 1000;
      stopReq->senderData = 12;
      stopReq->senderRef = _ownReference;
      timeOut += stopReq->apiTimeout;
      timeOut += stopReq->transactionTimeout;
      timeOut += stopReq->readOperationTimeout;
      timeOut += stopReq->operationTimeout;
      timeOut *= 3;
      
      m_stopRec.sentCount++;
      if(callback == 0)
	sendSignal(nodeId, WAIT_STOP, signal, true);
      else
	sendSignal(nodeId, NO_WAIT, signal, true);
    }
  }

  if(stopCount != 0)
    * stopCount = m_stopRec.sentCount;

  if(callback == 0){
    m_stopRec.inUse = false;
    return 0;
    //    return m_stopRec.reply;
  } else {
    return 0;
  }  

  m_stopRec.inUse = false;
  return m_stopRec.reply;
}

int
MgmtSrvr::exitSingleUser(int * stopCount, bool abort, 
			 ExitSingleCallback callback, void * anyData) 
{
  m_stopRec.sentCount = 0;
  m_stopRec.reply = 0;
  m_stopRec.nodeId = 0;
  m_stopRec.anyData = anyData;
  m_stopRec.callback = callback;
  m_stopRec.inUse = true;
  
  NodeId nodeId = 0;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)){
    if(okToSendTo(nodeId, true) == 0){
  
      NdbApiSignal* signal = getSignal();
      if (signal == NULL) {
	return COULD_NOT_ALLOCATE_MEMORY;
      }
      
      ResumeReq* const resumeReq = 
	CAST_PTR(ResumeReq, signal->getDataPtrSend());
      signal->set(TestOrd::TraceAPI, NDBCNTR, GSN_RESUME_REQ, 
		  StopReq::SignalLength);
      resumeReq->senderData = 12;
      resumeReq->senderRef = _ownReference;

      m_stopRec.sentCount++;
      if(callback == 0)
	sendSignal(nodeId, WAIT_STOP, signal, true);
      else
	sendSignal(nodeId, NO_WAIT, signal, true);
    }
  }

  m_stopRec.singleUserMode = false;

  if(stopCount != 0)
    * stopCount = m_stopRec.sentCount;


  if(callback == 0){
    m_stopRec.inUse = false;
    return m_stopRec.reply;
  } else {
    return 0;
  }  

  m_stopRec.inUse = false;
  return m_stopRec.reply;
}


/*****************************************************************************
 * Status
 ****************************************************************************/

#include <ClusterMgr.hpp>

int 
MgmtSrvr::status(int processId, 
                 ndb_mgm_node_status * _status, 
		 Uint32 * version,
		 Uint32 * _phase, 
		 bool * _system,
		 Uint32 * dynamic,
		 Uint32 * nodegroup,
		 Uint32 * connectCount)
{
  if (getNodeType(processId) == NDB_MGM_NODE_TYPE_API ||
      getNodeType(processId) == NDB_MGM_NODE_TYPE_MGM) {
    if(versionNode(processId, false,0,0) ==0)
      * version = m_versionRec.version[processId];
    else
      * version = 0;
  }

  const ClusterMgr::Node node = 
    theFacade->theClusterMgr->getNodeInfo(processId);

  if(!node.connected){
    * _status = NDB_MGM_NODE_STATUS_NO_CONTACT;
    return 0;
  }
  
  if (getNodeType(processId) == NDB_MGM_NODE_TYPE_NDB) {
    * version = node.m_info.m_version;
  }

  * dynamic = node.m_state.dynamicId;
  * nodegroup = node.m_state.nodeGroup;
  * connectCount = node.m_info.m_connectCount;
  
  switch(node.m_state.startLevel){
  case NodeState::SL_CMVMI:
    * _status = NDB_MGM_NODE_STATUS_NOT_STARTED;
    * _phase = 0;
    return 0;
    break;
  case NodeState::SL_STARTING:
    * _status     = NDB_MGM_NODE_STATUS_STARTING;
    * _phase = node.m_state.starting.startPhase;
    return 0;
    break;
  case NodeState::SL_STARTED:
    * _status = NDB_MGM_NODE_STATUS_STARTED;
    * _phase = 0;
    return 0;
    break;
  case NodeState::SL_STOPPING_1:
    * _status = NDB_MGM_NODE_STATUS_SHUTTING_DOWN;
    * _phase = 1;
    * _system = node.m_state.stopping.systemShutdown != 0;
    return 0;
    break;
  case NodeState::SL_STOPPING_2:
    * _status = NDB_MGM_NODE_STATUS_SHUTTING_DOWN;
    * _phase = 2;
    * _system = node.m_state.stopping.systemShutdown != 0;
    return 0;
    break;
  case NodeState::SL_STOPPING_3:
    * _status = NDB_MGM_NODE_STATUS_SHUTTING_DOWN;
    * _phase = 3;
    * _system = node.m_state.stopping.systemShutdown != 0;
    return 0;
    break;
  case NodeState::SL_STOPPING_4:
    * _status = NDB_MGM_NODE_STATUS_SHUTTING_DOWN;
    * _phase = 4;
    * _system = node.m_state.stopping.systemShutdown != 0;
    return 0;
    break;
  case NodeState::SL_SINGLEUSER:
    * _status = NDB_MGM_NODE_STATUS_SINGLEUSER;
    * _phase  = 0;
    return 0;
    break;
  default:
    * _status = NDB_MGM_NODE_STATUS_UNKNOWN;
    * _phase = 0;
    return 0;
  }
  
  return -1;
}

int 
MgmtSrvr::setEventReportingLevelImpl(int processId, 
				     const EventSubscribeReq& ll)
{
    
  int result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }

  NdbApiSignal signal(_ownReference);

  EventSubscribeReq * dst = 
    CAST_PTR(EventSubscribeReq, signal.getDataPtrSend());
  
  * dst = ll;
  
  signal.set(TestOrd::TraceAPI, CMVMI, GSN_EVENT_SUBSCRIBE_REQ,
	     EventSubscribeReq::SignalLength);
  
  theFacade->lock_mutex();
  send(&signal, processId, NODE_TYPE_DB);
  theFacade->unlock_mutex();
  
  return 0;
}

//****************************************************************************
//****************************************************************************
int 
MgmtSrvr::setNodeLogLevelImpl(int processId, const SetLogLevelOrd & ll)
{
  int result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }

  NdbApiSignal signal(_ownReference);
  
  SetLogLevelOrd * dst = CAST_PTR(SetLogLevelOrd, signal.getDataPtrSend());
  
  * dst = ll;
  
  signal.set(TestOrd::TraceAPI, CMVMI, GSN_SET_LOGLEVELORD,
	     SetLogLevelOrd::SignalLength);
  
  theFacade->lock_mutex();
  theFacade->sendSignalUnCond(&signal, processId);
  theFacade->unlock_mutex();

  return 0;
}

int
MgmtSrvr::send(NdbApiSignal* signal, Uint32 node, Uint32 node_type){
  Uint32 max = (node == 0) ? MAX_NODES : node + 1;
  
  for(; node < max; node++){
    while(nodeTypes[node] != (int)node_type && node < max) node++;
    if(nodeTypes[node] != (int)node_type)
      break;
    theFacade->sendSignalUnCond(signal, node);
  }
  return 0;
}

//****************************************************************************
//****************************************************************************

int 
MgmtSrvr::insertError(int processId, int errorNo) 
{
  if (errorNo < 0) {
    return INVALID_ERROR_NUMBER;
  }

  int result;
  result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }
  
  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  TamperOrd* const tamperOrd = CAST_PTR(TamperOrd, signal->getDataPtrSend());
  tamperOrd->errorNo = errorNo;
  signal->set(TestOrd::TraceAPI, CMVMI, GSN_TAMPER_ORD, 
              TamperOrd::SignalLength);

  result = sendSignal(processId, NO_WAIT, signal, true);
  if (result == -1) {
    return SEND_OR_RECEIVE_FAILED;
  }

  return 0;
}



//****************************************************************************
//****************************************************************************

int 
MgmtSrvr::setTraceNo(int processId, int traceNo) 
{
  if (traceNo < 0) {
    return INVALID_TRACE_NUMBER;
  }

  int result;
  result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }

  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  TestOrd* const testOrd = CAST_PTR(TestOrd, signal->getDataPtrSend());
  testOrd->clear();
  
  // Assume TRACE command causes toggling. Not really defined... ? TODO
  testOrd->setTraceCommand(TestOrd::Toggle, 
			   (TestOrd::TraceSpecification)traceNo);
  signal->set(TestOrd::TraceAPI, CMVMI, GSN_TEST_ORD, TestOrd::SignalLength);

  result = sendSignal(processId, NO_WAIT, signal, true);
  if (result == -1) {
    return SEND_OR_RECEIVE_FAILED;
  }

  return 0;
}

//****************************************************************************
//****************************************************************************

int 
MgmtSrvr::getBlockNumber(const BaseString &blockName) 
{
  short bno = getBlockNo(blockName.c_str());
  if(bno != 0)
    return bno;
  return -1;
}

//****************************************************************************
//****************************************************************************

int 
MgmtSrvr::setSignalLoggingMode(int processId, LogMode mode, 
			       const Vector<BaseString>& blocks)
{
  int result;
  result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }

  // Convert from MgmtSrvr format...

  TestOrd::Command command;
  if (mode == Off) {
    command = TestOrd::Off;
  }
  else {
    command = TestOrd::On;
  }

  TestOrd::SignalLoggerSpecification logSpec;
  switch (mode) {
  case In:
    logSpec = TestOrd::InputSignals;
    break;
  case Out:
    logSpec = TestOrd::OutputSignals;
    break;
  case InOut:
    logSpec = TestOrd::InputOutputSignals;
    break;
  case Off:
    // In MgmtSrvr interface it's just possible to switch off all logging, both
    // "in" and "out" (this should probably be changed).
    logSpec = TestOrd::InputOutputSignals;
    break;
  default:
    ndbout_c("Unexpected value %d, MgmtSrvr::setSignalLoggingMode, line %d",
	     (unsigned)mode, __LINE__);
    assert(false);
    return -1;
  }

  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  TestOrd* const testOrd = CAST_PTR(TestOrd, signal->getDataPtrSend());
  testOrd->clear();
  
  if (blocks.size() == 0 || blocks[0] == "ALL") {
    // Logg command for all blocks
    testOrd->addSignalLoggerCommand(command, logSpec);
  } else {
    for(unsigned i = 0; i < blocks.size(); i++){
      int blockNumber = getBlockNumber(blocks[i]);
      if (blockNumber == -1) {
        releaseSignal(signal);
        return INVALID_BLOCK_NAME;
      }
      testOrd->addSignalLoggerCommand(blockNumber, command, logSpec);
    } // for
  } // else
  
  signal->set(TestOrd::TraceAPI, CMVMI, GSN_TEST_ORD, TestOrd::SignalLength);
  result = sendSignal(processId, NO_WAIT, signal, true);

  if (result == -1) {
    return SEND_OR_RECEIVE_FAILED;
  }
  return 0;
}


/*****************************************************************************
 * Signal tracing
 *****************************************************************************/
int MgmtSrvr::startSignalTracing(int processId)
{
  int result;
  result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }

  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  
  TestOrd* const testOrd = CAST_PTR(TestOrd, signal->getDataPtrSend());
  testOrd->clear();
  testOrd->setTestCommand(TestOrd::On);

  signal->set(TestOrd::TraceAPI, CMVMI, GSN_TEST_ORD, TestOrd::SignalLength);
  result = sendSignal(processId, NO_WAIT, signal, true);
  if (result == -1) {
    return SEND_OR_RECEIVE_FAILED;
  }

  return 0;
}

int 
MgmtSrvr::stopSignalTracing(int processId) 
{
  int result;
  result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }

  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  TestOrd* const testOrd = CAST_PTR(TestOrd, signal->getDataPtrSend());
  testOrd->clear();
  testOrd->setTestCommand(TestOrd::Off);

  signal->set(TestOrd::TraceAPI, CMVMI, GSN_TEST_ORD, TestOrd::SignalLength);
  result = sendSignal(processId, NO_WAIT, signal, true);
  if (result == -1) {
    return SEND_OR_RECEIVE_FAILED;
  }

  return 0;
}


/*****************************************************************************
 * Dump state
 *****************************************************************************/

int
MgmtSrvr::dumpState(int processId, const char* args)
{
  // Convert the space separeted args 
  // string to an int array
  Uint32 args_array[25];
  Uint32 numArgs = 0;

  char buf[10];  
  int b  = 0;
  memset(buf, 0, 10);
  for (size_t i = 0; i <= strlen(args); i++){
    if (args[i] == ' ' || args[i] == 0){
      args_array[numArgs] = atoi(buf);
      numArgs++;
      memset(buf, 0, 10);
      b = 0;
    } else {
      buf[b] = args[i];
      b++;
    }    
  }
  
  return dumpState(processId, args_array, numArgs);
}

int
MgmtSrvr::dumpState(int processId, const Uint32 args[], Uint32 no)
{
  int result;
  
  result = okToSendTo(processId, true);
  if (result != 0) {
    return result;
  }
  
  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  const Uint32 len = no > 25 ? 25 : no;
  
  DumpStateOrd * const dumpOrd = 
    CAST_PTR(DumpStateOrd, signal->getDataPtrSend());
  signal->set(TestOrd::TraceAPI, CMVMI, GSN_DUMP_STATE_ORD, len);
  for(Uint32 i = 0; i<25; i++){
    if (i < len)
      dumpOrd->args[i] = args[i];
    else
      dumpOrd->args[i] = 0;
  }
  
  result = sendSignal(processId, NO_WAIT, signal, true);
  if (result == -1) {
    return SEND_OR_RECEIVE_FAILED;
  }
  
  return 0;
}


//****************************************************************************
//****************************************************************************

const char* MgmtSrvr::getErrorText(int errorCode, char *buf, int buf_sz)
{

  for (int i = 0; i < noOfErrorCodes; ++i) {
    if (errorCode == errorTable[i]._errorCode) {
      BaseString::snprintf(buf, buf_sz, errorTable[i]._errorText);
      buf[buf_sz-1]= 0;
      return buf;
    }
  }

  ndb_error_string(errorCode, buf, buf_sz);
  buf[buf_sz-1]= 0;

  return buf;
}

void 
MgmtSrvr::handleReceivedSignal(NdbApiSignal* signal)
{
  // The way of handling a received signal is taken from the Ndb class.
  int gsn = signal->readSignalNumber();

  switch (gsn) {
  case GSN_API_VERSION_CONF: {
    if (theWaitState == WAIT_VERSION) {
      const ApiVersionConf * const conf = 
	CAST_CONSTPTR(ApiVersionConf, signal->getDataPtr());
      if(m_versionRec.callback != 0)
	m_versionRec.callback(conf->nodeId, conf->version, this, 0);
      else {
	m_versionRec.version[conf->nodeId]=conf->version;
      }
    } else return;
    theWaitState = NO_WAIT;
  }
    break;
    
  case GSN_EVENT_SUBSCRIBE_CONF:
    break;

  case GSN_EVENT_REP:
    eventReport(refToNode(signal->theSendersBlockRef), signal->getDataPtr());
    break;

  case GSN_STOP_REF:{
    const StopRef * const ref = CAST_CONSTPTR(StopRef, signal->getDataPtr());
    const NodeId nodeId = refToNode(signal->theSendersBlockRef);
    handleStopReply(nodeId, ref->errorCode);
    return;
  }
    break;
    
  case GSN_BACKUP_CONF:{
    const BackupConf * const conf = 
      CAST_CONSTPTR(BackupConf, signal->getDataPtr());
    BackupEvent event;
    event.Event = BackupEvent::BackupStarted;
    event.Started.BackupId = conf->backupId;
    event.Nodes = conf->nodes;
#ifdef VM_TRACE
    ndbout_c("Backup master is %d", refToNode(signal->theSendersBlockRef));
#endif
    backupCallback(event);
  }
    break;

  case GSN_BACKUP_REF:{
    const BackupRef * const ref = 
      CAST_CONSTPTR(BackupRef, signal->getDataPtr());
    Uint32 errCode = ref->errorCode;
    if(ref->errorCode == BackupRef::IAmNotMaster){
      const Uint32 aNodeId = refToNode(ref->masterRef);
#ifdef VM_TRACE
      ndbout_c("I'm not master resending to %d", aNodeId);
#endif
      NdbApiSignal aSignal(_ownReference);
      BackupReq* req = CAST_PTR(BackupReq, aSignal.getDataPtrSend());
      aSignal.set(TestOrd::TraceAPI, BACKUP, GSN_BACKUP_REQ, 
		  BackupReq::SignalLength);
      req->senderData = 19;
      req->backupDataLen = 0;
      
      int i = theFacade->sendSignalUnCond(&aSignal, aNodeId);
      if(i == 0){
	return;
      }
      errCode = 5030;
    }
    BackupEvent event;
    event.Event = BackupEvent::BackupFailedToStart;
    event.FailedToStart.ErrorCode = errCode;
    backupCallback(event);
    break;
  }

  case GSN_BACKUP_ABORT_REP:{
    const BackupAbortRep * const rep = 
      CAST_CONSTPTR(BackupAbortRep, signal->getDataPtr());
    BackupEvent event;
    event.Event = BackupEvent::BackupAborted;
    event.Aborted.Reason = rep->reason;
    event.Aborted.BackupId = rep->backupId;
    backupCallback(event);
  }
  break;

  case GSN_BACKUP_COMPLETE_REP:{
    const BackupCompleteRep * const rep = 
      CAST_CONSTPTR(BackupCompleteRep, signal->getDataPtr());
    BackupEvent event;
    event.Event = BackupEvent::BackupCompleted;
    event.Completed.BackupId = rep->backupId;

    event.Completed.NoOfBytes = rep->noOfBytes;
    event.Completed.NoOfLogBytes = rep->noOfLogBytes;
    event.Completed.NoOfRecords = rep->noOfRecords;
    event.Completed.NoOfLogRecords = rep->noOfLogRecords;
    event.Completed.stopGCP = rep->stopGCP;
    event.Completed.startGCP = rep->startGCP;
    event.Nodes = rep->nodes;

    backupCallback(event);
  }
    break;

  case GSN_MGM_LOCK_CONFIG_REP:
  case GSN_MGM_LOCK_CONFIG_REQ:
  case GSN_MGM_UNLOCK_CONFIG_REP:
  case GSN_MGM_UNLOCK_CONFIG_REQ: {
    m_signalRecvQueue.receive(new NdbApiSignal(*signal));
    break;
  }

  default:
    g_eventLogger.error("Unknown signal received. SignalNumber: "
			"%i from (%d, %x)",
			gsn,
			refToNode(signal->theSendersBlockRef),
			refToBlock(signal->theSendersBlockRef));
  }
  
  if (theWaitState == NO_WAIT) {
    NdbCondition_Signal(theMgmtWaitForResponseCondPtr);
  }
}

/**
 * A database node was either stopped or there was some error
 */
void
MgmtSrvr::handleStopReply(NodeId nodeId, Uint32 errCode)
{
  /**
   * If we are in single user mode and get a stop reply from a
   * DB node, then we have had a node crash.
   * If all DB nodes are gone, and we are still in single user mode,
   * the set m_stopRec.singleUserMode = false; 
   */
  if(m_stopRec.singleUserMode) {
    ClusterMgr::Node node;
    bool failure = true;
    for(Uint32 i = 0; i<MAX_NODES; i++) {
      if (getNodeType(i) == NDB_MGM_NODE_TYPE_NDB) {
	node = theFacade->theClusterMgr->getNodeInfo(i);
	if((node.m_state.startLevel == NodeState::SL_NOTHING))
	  failure = true;
	else
	  failure = false;
      }
    }
    if(failure) {
      m_stopRec.singleUserMode = false;
    }
  }
  if(m_stopRec.inUse == false)
    return;

  if(!(m_stopRec.nodeId == 0 || m_stopRec.nodeId == nodeId))
    goto error;
  
  if(m_stopRec.sentCount <= 0)
    goto error;
  
  if(!(theWaitState == WAIT_STOP || m_stopRec.callback != 0))
    goto error;
  
  if(errCode != 0)
    m_stopRec.reply = translateStopRef(errCode);
 
  m_stopRec.sentCount --;
  if(m_stopRec.sentCount == 0){
    if(theWaitState == WAIT_STOP){
      theWaitState = NO_WAIT;
      NdbCondition_Signal(theMgmtWaitForResponseCondPtr);	
      return;
    }
    if(m_stopRec.callback != 0){
      m_stopRec.inUse = false;
      StopCallback callback = m_stopRec.callback;
      m_stopRec.callback = NULL;
      (* callback)(m_stopRec.nodeId,
		   m_stopRec.anyData,
		   m_stopRec.reply);
      return;
    }
  }
  return;
  
 error:
  if(errCode != 0){
    g_eventLogger.error("Unexpected signal received. SignalNumber: %i from %d",
			GSN_STOP_REF, nodeId);
  }
}

void
MgmtSrvr::handleStatus(NodeId nodeId, bool alive)
{
  DBUG_ENTER("MgmtSrvr::handleStatus");
  Uint32 theData[25];
  theData[1] = nodeId;
  if (alive) {
    m_started_nodes.push_back(nodeId);
    theData[0] = NDB_LE_Connected;
  } else {
    handleStopReply(nodeId, 0);
    theData[0] = NDB_LE_Disconnected;
  }
  eventReport(_ownNodeId, theData);
  DBUG_VOID_RETURN;
}

//****************************************************************************
//****************************************************************************

void 
MgmtSrvr::signalReceivedNotification(void* mgmtSrvr, 
                                     NdbApiSignal* signal,
				     LinearSectionPtr ptr[3]) 
{
  ((MgmtSrvr*)mgmtSrvr)->handleReceivedSignal(signal);
}


//****************************************************************************
//****************************************************************************
void 
MgmtSrvr::nodeStatusNotification(void* mgmSrv, Uint32 nodeId, 
				 bool alive, bool nfComplete)
{
  DBUG_ENTER("MgmtSrvr::nodeStatusNotification");
  DBUG_PRINT("enter",("nodeid= %d, alive= %d, nfComplete= %d", nodeId, alive, nfComplete));
  if(!(!alive && nfComplete))
    ((MgmtSrvr*)mgmSrv)->handleStatus(nodeId, alive);
  DBUG_VOID_RETURN;
}

enum ndb_mgm_node_type 
MgmtSrvr::getNodeType(NodeId nodeId) const 
{
  if(nodeId >= MAX_NODES)
    return (enum ndb_mgm_node_type)-1;
  
  return nodeTypes[nodeId];
}

void
MgmtSrvr::get_connected_nodes(NodeBitmask &connected_nodes) const
{
  if (theFacade && theFacade->theClusterMgr) 
  {
    for(Uint32 i = 0; i < MAX_NODES; i++)
    {
      if (getNodeType(i) == NDB_MGM_NODE_TYPE_NDB)
      {
	const ClusterMgr::Node &node= theFacade->theClusterMgr->getNodeInfo(i);
	if (theFacade->get_registry()->get_transporter(i)->isConnected())
	{
	  connected_nodes.bitOR(node.m_state.m_connected_nodes);
	  connected_nodes.set(i);
	}
      }
    }
  }
}

bool
MgmtSrvr::alloc_node_id(NodeId * nodeId, 
			enum ndb_mgm_node_type type,
			struct sockaddr *client_addr, 
			SOCKET_SIZE_TYPE *client_addr_len,
			BaseString &error_string)
{
  DBUG_ENTER("MgmtSrvr::alloc_node_id");
  DBUG_PRINT("enter", ("nodeid=%d, type=%d, client_addr=%d",
		       *nodeId, type, client_addr));
  if (g_no_nodeid_checks) {
    if (*nodeId == 0) {
      error_string.appfmt("no-nodeid-checks set in management server.\n"
			  "node id must be set explicitly in connectstring");
      DBUG_RETURN(false);
    }
    DBUG_RETURN(true);
  }
  Guard g(m_node_id_mutex);
  int no_mgm= 0;
  NodeBitmask connected_nodes(m_reserved_nodes);
  get_connected_nodes(connected_nodes);
  {
    for(Uint32 i = 0; i < MAX_NODES; i++)
      if (getNodeType(i) == NDB_MGM_NODE_TYPE_MGM)
	no_mgm++;
  }
  bool found_matching_id= false;
  bool found_matching_type= false;
  bool found_free_node= false;
  unsigned id_found= 0;
  const char *config_hostname= 0;
  struct in_addr config_addr= {0};
  int r_config_addr= -1;
  unsigned type_c= 0;

  if(NdbMutex_Lock(m_configMutex))
  {
    error_string.appfmt("unable to lock configuration mutex");
    return false;
  }
  ndb_mgm_configuration_iterator
    iter(* _config->m_configValues, CFG_SECTION_NODE);
  for(iter.first(); iter.valid(); iter.next()) {
    unsigned tmp= 0;
    if(iter.get(CFG_NODE_ID, &tmp)) abort();
    if (*nodeId && *nodeId != tmp)
      continue;
    found_matching_id= true;
    if(iter.get(CFG_TYPE_OF_SECTION, &type_c)) abort();
    if(type_c != (unsigned)type)
      continue;
    found_matching_type= true;
    if (connected_nodes.get(tmp))
      continue;
    found_free_node= true;
    if(iter.get(CFG_NODE_HOST, &config_hostname)) abort();
    if (config_hostname && config_hostname[0] == 0)
      config_hostname= 0;
    else if (client_addr) {
      // check hostname compatability
      const void *tmp_in= &(((sockaddr_in*)client_addr)->sin_addr);
      if((r_config_addr= Ndb_getInAddr(&config_addr, config_hostname)) != 0
	 || memcmp(&config_addr, tmp_in, sizeof(config_addr)) != 0) {
	struct in_addr tmp_addr;
	if(Ndb_getInAddr(&tmp_addr, "localhost") != 0
	   || memcmp(&tmp_addr, tmp_in, sizeof(config_addr)) != 0) {
	  // not localhost
#if 0
	  ndbout << "MgmtSrvr::getFreeNodeId compare failed for \""
		 << config_hostname
		 << "\" id=" << tmp << endl;
#endif
	  continue;
	}
	// connecting through localhost
	// check if config_hostname is local
	if (!SocketServer::tryBind(0,config_hostname)) {
	  continue;
	}
      }
    } else { // client_addr == 0
      if (!SocketServer::tryBind(0,config_hostname)) {
	continue;
      }
    }
    if (*nodeId != 0 ||
	type != NDB_MGM_NODE_TYPE_MGM ||
	no_mgm == 1) { // any match is ok

      if (config_hostname == 0 &&
	  *nodeId == 0 &&
	  type != NDB_MGM_NODE_TYPE_MGM)
      {
	if (!id_found) // only set if not set earlier
	  id_found= tmp;
	continue; /* continue looking for a nodeid with specified
		   * hostname
		   */
      }
      assert(id_found == 0);
      id_found= tmp;
      break;
    }
    if (id_found) { // mgmt server may only have one match
      error_string.appfmt("Ambiguous node id's %d and %d.\n"
			  "Suggest specifying node id in connectstring,\n"
			  "or specifying unique host names in config file.",
			  id_found, tmp);
      NdbMutex_Unlock(m_configMutex);
      DBUG_RETURN(false);
    }
    if (config_hostname == 0) {
      error_string.appfmt("Ambiguity for node id %d.\n"
			  "Suggest specifying node id in connectstring,\n"
			  "or specifying unique host names in config file,\n"
			  "or specifying just one mgmt server in config file.",
			  tmp);
      DBUG_RETURN(false);
    }
    id_found= tmp; // mgmt server matched, check for more matches
  }
  NdbMutex_Unlock(m_configMutex);

  if (id_found)
  {
    *nodeId= id_found;
    DBUG_PRINT("info", ("allocating node id %d",*nodeId));
    {
      int r= 0;
      if (client_addr)
	m_connect_address[id_found]=
	  ((struct sockaddr_in *)client_addr)->sin_addr;
      else if (config_hostname)
	r= Ndb_getInAddr(&(m_connect_address[id_found]), config_hostname);
      else {
	char name[256];
	r= gethostname(name, sizeof(name));
	if (r == 0) {
	  name[sizeof(name)-1]= 0;
	  r= Ndb_getInAddr(&(m_connect_address[id_found]), name);
	}
      }
      if (r)
	m_connect_address[id_found].s_addr= 0;
    }
    m_reserved_nodes.set(id_found);
    char tmp_str[128];
    m_reserved_nodes.getText(tmp_str);
    g_eventLogger.info("Mgmt server state: nodeid %d reserved for ip %s, m_reserved_nodes %s.",
		       id_found, get_connect_address(id_found), tmp_str);
    DBUG_RETURN(true);
  }

  if (found_matching_type && !found_free_node) {
    // we have a temporary error which might be due to that 
    // we have got the latest connect status from db-nodes.  Force update.
    global_flag_send_heartbeat_now= 1;
  }

  BaseString type_string, type_c_string;
  {
    const char *alias, *str;
    alias= ndb_mgm_get_node_type_alias_string(type, &str);
    type_string.assfmt("%s(%s)", alias, str);
    alias= ndb_mgm_get_node_type_alias_string((enum ndb_mgm_node_type)type_c,
					      &str);
    type_c_string.assfmt("%s(%s)", alias, str);
  }

  if (*nodeId == 0) {
    if (found_matching_id)
      if (found_matching_type)
	if (found_free_node)
	  error_string.appfmt("Connection done from wrong host ip %s.",
			      (client_addr)?
			        inet_ntoa(((struct sockaddr_in *)
					 (client_addr))->sin_addr):"");
	else
	  error_string.appfmt("No free node id found for %s.",
			      type_string.c_str());
      else
	error_string.appfmt("No %s node defined in config file.",
			    type_string.c_str());
    else
      error_string.append("No nodes defined in config file.");
  } else {
    if (found_matching_id)
      if (found_matching_type)
	if (found_free_node) {
	  // have to split these into two since inet_ntoa overwrites itself
	  error_string.appfmt("Connection with id %d done from wrong host ip %s,",
			      *nodeId, inet_ntoa(((struct sockaddr_in *)
						  (client_addr))->sin_addr));
	  error_string.appfmt(" expected %s(%s).", config_hostname,
			      r_config_addr ?
			      "lookup failed" : inet_ntoa(config_addr));
	} else
	  error_string.appfmt("Id %d already allocated by another node.",
			      *nodeId);
      else
	error_string.appfmt("Id %d configured as %s, connect attempted as %s.",
			    *nodeId, type_c_string.c_str(),
			    type_string.c_str());
    else
      error_string.appfmt("No node defined with id=%d in config file.",
			  *nodeId);
  }

  g_eventLogger.warning("Allocate nodeid (%d) failed. Connection from ip %s. "
			"Returned error string \"%s\"",
			*nodeId,
			client_addr != 0 ? inet_ntoa(((struct sockaddr_in *)(client_addr))->sin_addr) : "<none>",
			error_string.c_str());

  NodeBitmask connected_nodes2;
  get_connected_nodes(connected_nodes2);
  {
    BaseString tmp_connected, tmp_not_connected;
    for(Uint32 i = 0; i < MAX_NODES; i++)
    {
      if (connected_nodes2.get(i))
      {
	if (!m_reserved_nodes.get(i))
	  tmp_connected.appfmt(" %d", i);
      }
      else if (m_reserved_nodes.get(i))
      {
	tmp_not_connected.appfmt(" %d", i);
      }
    }
    if (tmp_connected.length() > 0)
      g_eventLogger.info("Mgmt server state: node id's %s connected but not reserved", 
			 tmp_connected.c_str());
    if (tmp_not_connected.length() > 0)
      g_eventLogger.info("Mgmt server state: node id's %s not connected but reserved",
			 tmp_not_connected.c_str());
  }
  DBUG_RETURN(false);
}

bool
MgmtSrvr::getNextNodeId(NodeId * nodeId, enum ndb_mgm_node_type type) const 
{
  NodeId tmp = * nodeId;

  tmp++;
  while(nodeTypes[tmp] != type && tmp < MAX_NODES)
    tmp++;
  
  if(tmp == MAX_NODES){
    return false;
  }

  * nodeId = tmp;
  return true;
}

#include "Services.hpp"

void
MgmtSrvr::eventReport(NodeId nodeId, const Uint32 * theData)
{
  const EventReport * const eventReport = (EventReport *)&theData[0];
  
  Ndb_logevent_type type = eventReport->getEventType();
  // Log event
  g_eventLogger.log(type, theData, nodeId, 
		    &m_event_listner[0].m_logLevel);  
  m_event_listner.log(type, theData, nodeId);
}

/***************************************************************************
 * Backup
 ***************************************************************************/
int
MgmtSrvr::startBackup(Uint32& backupId, int waitCompleted)
{
  bool next;
  NodeId nodeId = 0;
  while((next = getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) == true &&
	theFacade->get_node_alive(nodeId) == false);
  
  if(!next) return NO_CONTACT_WITH_DB_NODES;

  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  BackupReq* req = CAST_PTR(BackupReq, signal->getDataPtrSend());
  signal->set(TestOrd::TraceAPI, BACKUP, GSN_BACKUP_REQ, 
	      BackupReq::SignalLength);
  
  req->senderData = 19;
  req->backupDataLen = 0;

  int result;
  if (waitCompleted == 2) {
    result = sendRecSignal(nodeId, WAIT_BACKUP_COMPLETED,
			   signal, true, 30*60*1000 /*30 secs*/);
  }
  else if (waitCompleted == 1) {
    result = sendRecSignal(nodeId, WAIT_BACKUP_STARTED,
			   signal, true, 5*60*1000 /*5 mins*/);
  }
  else {
    result = sendRecSignal(nodeId, NO_WAIT, signal, true);
  }
  if (result == -1) {
    return SEND_OR_RECEIVE_FAILED;
  }

  if (waitCompleted){
    switch(m_lastBackupEvent.Event){
    case BackupEvent::BackupCompleted:
      backupId = m_lastBackupEvent.Completed.BackupId;
      break;
    case BackupEvent::BackupStarted:
      backupId = m_lastBackupEvent.Started.BackupId;
      break;
    case BackupEvent::BackupFailedToStart:
      return m_lastBackupEvent.FailedToStart.ErrorCode;
    case BackupEvent::BackupAborted:
      return m_lastBackupEvent.Aborted.ErrorCode;
    default:
      return -1;
      break;
    }
  } else {
    switch(m_lastBackupEvent.Event){
    case BackupEvent::BackupCompleted:
      backupId = m_lastBackupEvent.Completed.BackupId;
      break;
    case BackupEvent::BackupStarted:
      backupId = m_lastBackupEvent.Started.BackupId;
      break;
    case BackupEvent::BackupFailedToStart:
      return m_lastBackupEvent.FailedToStart.ErrorCode;
    case BackupEvent::BackupAborted:
      return m_lastBackupEvent.Aborted.ErrorCode;
    default:
      return -1;
      break;
    }
  }
  
  return 0;
}

int 
MgmtSrvr::abortBackup(Uint32 backupId)
{
  bool next;
  NodeId nodeId = 0;
  while((next = getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) == true &&
	theFacade->get_node_alive(nodeId) == false);
  
  if(!next){
    return NO_CONTACT_WITH_DB_NODES;
  }
  
  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  AbortBackupOrd* ord = CAST_PTR(AbortBackupOrd, signal->getDataPtrSend());
  signal->set(TestOrd::TraceAPI, BACKUP, GSN_ABORT_BACKUP_ORD, 
	      AbortBackupOrd::SignalLength);
  
  ord->requestType = AbortBackupOrd::ClientAbort;
  ord->senderData = 19;
  ord->backupId = backupId;
  
  int result = sendSignal(nodeId, NO_WAIT, signal, true);
  if (result == -1) {
    return SEND_OR_RECEIVE_FAILED;
  }
  
  return 0;
}

void
MgmtSrvr::backupCallback(BackupEvent & event)
{
  DBUG_ENTER("MgmtSrvr::backupCallback");
  m_lastBackupEvent = event;
  switch(event.Event){
  case BackupEvent::BackupFailedToStart:
    DBUG_PRINT("info",("BackupEvent::BackupFailedToStart"));
    theWaitState = NO_WAIT;
    break;
  case BackupEvent::BackupAborted:
    DBUG_PRINT("info",("BackupEvent::BackupAborted"));
    theWaitState = NO_WAIT;
    break;
  case BackupEvent::BackupCompleted:
    DBUG_PRINT("info",("BackupEvent::BackupCompleted"));
    theWaitState = NO_WAIT;
    break;
  case BackupEvent::BackupStarted:
    if(theWaitState == WAIT_BACKUP_STARTED)
    {
      DBUG_PRINT("info",("BackupEvent::BackupStarted NO_WAIT"));
      theWaitState = NO_WAIT;
    } else {
      DBUG_PRINT("info",("BackupEvent::BackupStarted"));
    }
  }
  DBUG_VOID_RETURN;
}


/*****************************************************************************
 * Global Replication
 *****************************************************************************/

int
MgmtSrvr::repCommand(Uint32* repReqId, Uint32 request, bool waitCompleted)
{
  bool    next;
  NodeId  nodeId = 0;
  
  while((next = getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) == true &&
	theFacade->get_node_alive(nodeId) == false);
  
  if(!next){
    return NO_CONTACT_WITH_DB_NODES;
  }

  NdbApiSignal* signal = getSignal();
  if (signal == NULL) {
    return COULD_NOT_ALLOCATE_MEMORY;
  }

  GrepReq* req = CAST_PTR(GrepReq, signal->getDataPtrSend());
  signal->set(TestOrd::TraceAPI, GREP, GSN_GREP_REQ, GrepReq::SignalLength);
  req->senderRef = _ownReference;
  req->request   = request;

  int result;
  if (waitCompleted)
    result = sendRecSignal(nodeId, NO_WAIT, signal, true);
  else
    result = sendRecSignal(nodeId, NO_WAIT, signal, true);
  if (result == -1) {
    return SEND_OR_RECEIVE_FAILED;
  }

  /**
   * @todo
   * Maybe add that we should receive a confirmation that the 
   * request was received ok.
   * Then we should give the user the correct repReqId.
   */

  *repReqId = 4711;

  return 0;
}


/*****************************************************************************
 * Area 51 ???
 *****************************************************************************/

MgmtSrvr::Area51
MgmtSrvr::getStuff()
{
  Area51 ret;
  ret.theFacade = theFacade;
  ret.theRegistry = theFacade->theTransporterRegistry;
  return ret;
}

NodeId
MgmtSrvr::getPrimaryNode() const {
#if 0
  Uint32 tmp;
  const Properties *prop = NULL;

  getConfig()->get("SYSTEM", &prop);
  if(prop == NULL)
    return 0;

  prop->get("PrimaryMGMNode", &tmp);
  
  return tmp;
#else
  return 0;
#endif
}


MgmtSrvr::Allocated_resources::Allocated_resources(MgmtSrvr &m)
  : m_mgmsrv(m)
{
}

MgmtSrvr::Allocated_resources::~Allocated_resources()
{
  Guard g(m_mgmsrv.m_node_id_mutex);
  if (!m_reserved_nodes.isclear()) {
    m_mgmsrv.m_reserved_nodes.bitANDC(m_reserved_nodes); 
    // node has been reserved, force update signal to ndb nodes
    global_flag_send_heartbeat_now= 1;

    char tmp_str[128];
    m_mgmsrv.m_reserved_nodes.getText(tmp_str);
    g_eventLogger.info("Mgmt server state: nodeid %d freed, m_reserved_nodes %s.",
		       get_nodeid(), tmp_str);
  }
}

void
MgmtSrvr::Allocated_resources::reserve_node(NodeId id)
{
  m_reserved_nodes.set(id);
}

NodeId
MgmtSrvr::Allocated_resources::get_nodeid() const
{
  for(Uint32 i = 0; i < MAX_NODES; i++)
  {
    if (m_reserved_nodes.get(i))
      return i;
  }
  return 0;
}

int
MgmtSrvr::setDbParameter(int node, int param, const char * value,
			 BaseString& msg){

  if(NdbMutex_Lock(m_configMutex))
    return -1;

  /**
   * Check parameter
   */
  ndb_mgm_configuration_iterator
    iter(* _config->m_configValues, CFG_SECTION_NODE);
  if(iter.first() != 0){
    msg.assign("Unable to find node section (iter.first())");
    NdbMutex_Unlock(m_configMutex);
    return -1;
  }
  
  Uint32 type = NODE_TYPE_DB + 1;
  if(node != 0){
    if(iter.find(CFG_NODE_ID, node) != 0){
      msg.assign("Unable to find node (iter.find())");
      NdbMutex_Unlock(m_configMutex);
      return -1;
    }
    if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0){
      msg.assign("Unable to get node type(iter.get(CFG_TYPE_OF_SECTION))");
      NdbMutex_Unlock(m_configMutex);
      return -1;
    }
  } else {
    do {
      if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0){
	msg.assign("Unable to get node type(iter.get(CFG_TYPE_OF_SECTION))");
	NdbMutex_Unlock(m_configMutex);
	return -1;
      }
      if(type == NODE_TYPE_DB)
	break;
    } while(iter.next() == 0);
  }
  
  if(type != NODE_TYPE_DB){
    msg.assfmt("Invalid node type or no such node (%d %d)", 
	       type, NODE_TYPE_DB);
    NdbMutex_Unlock(m_configMutex);
    return -1;
  }

  int p_type;
  unsigned val_32;
  Uint64 val_64;
  const char * val_char;
  do {
    p_type = 0;
    if(iter.get(param, &val_32) == 0){
      val_32 = atoi(value);
      break;
    }
    
    p_type++;
    if(iter.get(param, &val_64) == 0){
      val_64 = strtoll(value, 0, 10);
      break;
    }
    p_type++;
    if(iter.get(param, &val_char) == 0){
      val_char = value;
      break;
    }
    msg.assign("Could not get parameter");
    NdbMutex_Unlock(m_configMutex);
    return -1;
  } while(0);
  
  bool res = false;
  do {
    int ret = iter.get(CFG_TYPE_OF_SECTION, &type);
    assert(ret == 0);
    
    if(type != NODE_TYPE_DB)
      continue;
    
    Uint32 node;
    ret = iter.get(CFG_NODE_ID, &node);
    assert(ret == 0);
    
    ConfigValues::Iterator i2(_config->m_configValues->m_config, 
			      iter.m_config);
    switch(p_type){
    case 0:
      res = i2.set(param, val_32);
      ndbout_c("Updating node %d param: %d to %d",  node, param, val_32);
      break;
    case 1:
      res = i2.set(param, val_64);
      ndbout_c("Updating node %d param: %d to %Ld",  node, param, val_32);
      break;
    case 2:
      res = i2.set(param, val_char);
      ndbout_c("Updating node %d param: %d to %s",  node, param, val_char);
      break;
    default:
      abort();
    }
    assert(res);
  } while(node == 0 && iter.next() == 0);

  msg.assign("Success");
  NdbMutex_Unlock(m_configMutex);
  return 0;
}

int
MgmtSrvr::setConnectionDbParameter(int node1, 
				   int node2,
				   int param,
				   int value,
				   BaseString& msg){
  Uint32 current_value,new_value;

  DBUG_ENTER("MgmtSrvr::setConnectionDbParameter");

  if(NdbMutex_Lock(m_configMutex))
  {
    DBUG_RETURN(-1);
  }

  ndb_mgm_configuration_iterator 
    iter(* _config->m_configValues, CFG_SECTION_CONNECTION);

  if(iter.first() != 0){
    msg.assign("Unable to find connection section (iter.first())");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-1);
  }

  for(;iter.valid();iter.next()) {
    Uint32 n1,n2;
    iter.get(CFG_CONNECTION_NODE_1, &n1);
    iter.get(CFG_CONNECTION_NODE_2, &n2);
    if((n1 == (unsigned)node1 && n2 == (unsigned)node2)
       || (n1 == (unsigned)node2 && n2 == (unsigned)node1))
      break;
  }
  if(!iter.valid()) {
    msg.assign("Unable to find connection between nodes");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-2);
  }
  
  if(iter.get(param, &current_value) != 0) {
    msg.assign("Unable to get current value of parameter");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-3);
  }

  ConfigValues::Iterator i2(_config->m_configValues->m_config, 
			    iter.m_config);

  if(i2.set(param, (unsigned)value) == false) {
    msg.assign("Unable to set new value of parameter");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-4);
  }
  
  if(iter.get(param, &new_value) != 0) {
    msg.assign("Unable to get parameter after setting it.");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-5);
  }

  msg.assfmt("%u -> %u",current_value,new_value);
  NdbMutex_Unlock(m_configMutex);
  DBUG_RETURN(1);
}


int
MgmtSrvr::getConnectionDbParameter(int node1, 
				   int node2,
				   int param,
				   int *value,
				   BaseString& msg){
  DBUG_ENTER("MgmtSrvr::getConnectionDbParameter");

  if(NdbMutex_Lock(m_configMutex))
  {
    DBUG_RETURN(-1);
  }

  ndb_mgm_configuration_iterator
    iter(* _config->m_configValues, CFG_SECTION_CONNECTION);

  if(iter.first() != 0){
    msg.assign("Unable to find connection section (iter.first())");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-1);
  }

  for(;iter.valid();iter.next()) {
    Uint32 n1=0,n2=0;
    iter.get(CFG_CONNECTION_NODE_1, &n1);
    iter.get(CFG_CONNECTION_NODE_2, &n2);
    if((n1 == (unsigned)node1 && n2 == (unsigned)node2)
       || (n1 == (unsigned)node2 && n2 == (unsigned)node1))
      break;
  }
  if(!iter.valid()) {
    msg.assign("Unable to find connection between nodes");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-1);
  }
  
  if(iter.get(param, (Uint32*)value) != 0) {
    msg.assign("Unable to get current value of parameter");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-1);
  }

  msg.assfmt("%d",*value);
  NdbMutex_Unlock(m_configMutex);
  DBUG_RETURN(1);
}

void MgmtSrvr::transporter_connect(NDB_SOCKET_TYPE sockfd)
{
  if (theFacade->get_registry()->connect_server(sockfd))
  {
    /**
     * Force an update_connections() so that the
     * ClusterMgr and TransporterFacade is up to date
     * with the new connection.
     * Important for correct node id reservation handling
     */
    NdbMutex_Lock(theFacade->theMutexPtr);
    theFacade->get_registry()->update_connections();
    NdbMutex_Unlock(theFacade->theMutexPtr);
  }
}

int MgmtSrvr::set_connect_string(const char *str)
{
  return ndb_mgm_set_connectstring(m_config_retriever->get_mgmHandle(),str);
}


template class Vector<SigMatch>;
#if __SUNPRO_CC != 0x560
template bool SignalQueue::waitFor<SigMatch>(Vector<SigMatch>&, SigMatch**, NdbApiSignal**, unsigned);
#endif

template class MutexVector<unsigned short>;
template class MutexVector<Ndb_mgmd_event_service::Event_listener>;
template class MutexVector<EventSubscribeReq>;
