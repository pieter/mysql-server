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
#include <my_sys.h>

//#define HAVE_GLOBAL_REPLICATION

#include <Vector.hpp>
#ifdef  HAVE_GLOBAL_REPLICATION
#include "../rep/repapi/repapi.h"
#endif

#include <mgmapi.h>

class MgmtSrvr;

/** 
 *  @class CommandInterpreter
 *  @brief Reads command line in management client
 *
 *  This class has one public method which reads a command line 
 *  from a stream. It then interpret that commmand line and calls a suitable 
 *  method in the MgmtSrvr class which executes the command.
 *
 *  For command syntax, see the HELP command.
 */ 
class CommandInterpreter {
public:
  /**
   *   Constructor
   *   @param mgmtSrvr: Management server to use when executing commands
   */
  CommandInterpreter(const char *, int verbose);
  ~CommandInterpreter();
  
  /**
   *   Reads one line from the stream, parse the line to find 
   *   a command and then calls a suitable method which executes 
   *   the command.
   *
   *   @return true until quit/bye/exit has been typed
   */
  int execute(const char *_line, int _try_reconnect=-1, int *error= 0);

private:
  void printError();
  int execute_impl(const char *_line);

  /**
   *   Analyse the command line, after the first token.
   *
   *   @param  processId:           DB process id to send command to or -1 if
   *                                command will be sent to all DB processes.
   *   @param  allAfterFirstToken:  What the client gave after the 
   *                                first token on the command line
   */
  void analyseAfterFirstToken(int processId, char* allAfterFirstTokenCstr);

  /**
   *   Parse the block specification part of the LOG* commands,
   *   things after LOG*: [BLOCK = {ALL|<blockName>+}]
   *
   *   @param  allAfterLog: What the client gave after the second token 
   *                        (LOG*) on the command line
   *   @param  blocks, OUT: ALL or name of all the blocks
   *   @return: true if correct syntax, otherwise false
   */
  bool parseBlockSpecification(const char* allAfterLog, 
			       Vector<const char*>& blocks);
  
  /**
   *   A bunch of execute functions: Executes one of the commands
   *
   *   @param  processId:   DB process id to send command to
   *   @param  parameters:  What the client gave after the command name 
   *                        on the command line.
   *   For example if complete input from user is: "1 LOGLEVEL 22" then the
   *   parameters argument is the string with everything after LOGLEVEL, in
   *   this case "22". Each function is responsible to check the parameters
   *   argument.
   */
  void executeHelp(char* parameters);
  void executeShow(char* parameters);
  void executeConnect(char* parameters);
  void executePurge(char* parameters);
  int  executeShutdown(char* parameters);
  void executeRun(char* parameters);
  void executeInfo(char* parameters);
  void executeClusterLog(char* parameters);

public:
  void executeStop(int processId, const char* parameters, bool all);
  void executeEnterSingleUser(char* parameters);
  void executeExitSingleUser(char* parameters);
  void executeStart(int processId, const char* parameters, bool all);
  void executeRestart(int processId, const char* parameters, bool all);
  void executeLogLevel(int processId, const char* parameters, bool all);
  void executeError(int processId, const char* parameters, bool all);
  void executeLog(int processId, const char* parameters, bool all);
  void executeLogIn(int processId, const char* parameters, bool all);
  void executeLogOut(int processId, const char* parameters, bool all);
  void executeLogOff(int processId, const char* parameters, bool all);
  void executeTestOn(int processId, const char* parameters, bool all);
  void executeTestOff(int processId, const char* parameters, bool all);
  void executeSet(int processId, const char* parameters, bool all);
  void executeGetStat(int processId, const char* parameters, bool all);
  void executeStatus(int processId, const char* parameters, bool all);
  void executeEventReporting(int processId, const char* parameters, bool all);
  void executeDumpState(int processId, const char* parameters, bool all);
  int executeStartBackup(char * parameters);
  void executeAbortBackup(char * parameters);

  void executeRep(char* parameters);

  void executeCpc(char * parameters);

public:
  bool connect();
  bool disconnect();

  /**
   * A execute function definition
   */
public:
  typedef void (CommandInterpreter::* ExecuteFunction)(int processId, 
						       const char * param, 
						       bool all);
  
  struct CommandFunctionPair {
    const char * command;
    ExecuteFunction executeFunction;
  };
private:
  /**
   * 
   */
  void executeForAll(const char * cmd, 
		     ExecuteFunction fun,
		     const char * param);

  NdbMgmHandle m_mgmsrv;
  NdbMgmHandle m_mgmsrv2;
  bool m_connected;
  int m_verbose;
  int try_reconnect;
  int m_error;
#ifdef HAVE_GLOBAL_REPLICATION  
  NdbRepHandle m_repserver;
  const char *rep_host;
  bool rep_connected;
#endif
  struct NdbThread* m_event_thread;
};


/*
 * Facade object for CommandInterpreter
 */

#include "ndb_mgmclient.hpp"
#include "ndb_mgmclient.h"

Ndb_mgmclient::Ndb_mgmclient(const char *host,int verbose)
{
  m_cmd= new CommandInterpreter(host,verbose);
}
Ndb_mgmclient::~Ndb_mgmclient()
{
  delete m_cmd;
}
int Ndb_mgmclient::execute(const char *_line, int _try_reconnect, int *error)
{
  return m_cmd->execute(_line,_try_reconnect,error);
}
int
Ndb_mgmclient::disconnect()
{
  return m_cmd->disconnect();
}

extern "C" {
  Ndb_mgmclient_handle ndb_mgmclient_handle_create(const char *connect_string)
  {
    return (Ndb_mgmclient_handle) new Ndb_mgmclient(connect_string);
  }
  int ndb_mgmclient_execute(Ndb_mgmclient_handle h, int argc, char** argv)
  {
    return ((Ndb_mgmclient*)h)->execute(argc, argv, 1);
  }
  int ndb_mgmclient_handle_destroy(Ndb_mgmclient_handle h)
  {
    delete (Ndb_mgmclient*)h;
    return 0;
  }
}
/*
 * The CommandInterpreter
 */

#include <mgmapi.h>
#include <mgmapi_debug.h>
#include <version.h>
#include <NdbAutoPtr.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbMem.h>
#include <EventLogger.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <signaldata/GrepImpl.hpp>
#ifdef HAVE_GLOBAL_REPLICATION

#endif // HAVE_GLOBAL_REPLICATION
#include "MgmtErrorReporter.hpp"
#include <Parser.hpp>
#include <SocketServer.hpp>
#include <util/InputStream.hpp>
#include <util/OutputStream.hpp>

int Ndb_mgmclient::execute(int argc, char** argv, int _try_reconnect, int *error)
{
  if (argc <= 0)
    return 0;
  BaseString _line(argv[0]);
  for (int i= 1; i < argc; i++)
  {
    _line.appfmt(" %s", argv[i]);
  }
  return m_cmd->execute(_line.c_str(),_try_reconnect, error);
}

/*****************************************************************************
 * HELP
 *****************************************************************************/
static const char* helpText =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help\n"
"---------------------------------------------------------------------------\n"
"HELP                                   Print help text\n"
"HELP SHOW                              Help for SHOW command\n"
#ifdef HAVE_GLOBAL_REPLICATION
"HELP REPLICATION                       Help for global replication\n"
#endif // HAVE_GLOBAL_REPLICATION
#ifdef VM_TRACE // DEBUG ONLY
"HELP DEBUG                             Help for debug compiled version\n"
#endif
"SHOW                                   Print information about cluster\n"
#if 0
"SHOW CONFIG                            Print configuration\n"
"SHOW PARAMETERS                        Print configuration parameters\n"
#endif
"START BACKUP [NOWAIT | WAIT STARTED | WAIT COMPLETED]\n"
"                                       Start backup (default WAIT COMPLETED)\n"
"ABORT BACKUP <backup id>               Abort backup\n"
"SHUTDOWN                               Shutdown all processes in cluster\n"
"CLUSTERLOG ON [<severity>] ...         Enable Cluster logging\n"
"CLUSTERLOG OFF [<severity>] ...        Disable Cluster logging\n"
"CLUSTERLOG TOGGLE [<severity>] ...     Toggle severity filter on/off\n"
"CLUSTERLOG INFO                        Print cluster log information\n"
"<id> START                             Start DB node (started with -n)\n"
"<id> RESTART [-n] [-i]                 Restart DB node\n"
"<id> STOP                              Stop DB node\n"
"ENTER SINGLE USER MODE <api-node>      Enter single user mode\n"
"EXIT SINGLE USER MODE                  Exit single user mode\n"
"<id> STATUS                            Print status\n"
"<id> CLUSTERLOG {<category>=<level>}+  Set log level for cluster log\n"
#ifdef HAVE_GLOBAL_REPLICATION
"REP CONNECT <host:port>                Connect to REP server on host:port\n"
#endif
"PURGE STALE SESSIONS                   Reset reserved nodeid's in the mgmt server\n"
"CONNECT [<connectstring>]              Connect to management server (reconnect if already connected)\n"
"QUIT                                   Quit management client\n"
;

static const char* helpTextShow =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for SHOW command\n"
"---------------------------------------------------------------------------\n"
"SHOW prints NDB Cluster information\n\n"
"SHOW               Print information about cluster\n" 
#if 0
"SHOW CONFIG        Print configuration (in initial config file format)\n" 
"SHOW PARAMETERS    Print information about configuration parameters\n\n"
#endif
;

#ifdef HAVE_GLOBAL_REPLICATION
static const char* helpTextRep =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for Global Replication\n"
"---------------------------------------------------------------------------\n"
"Commands should be executed on the standby NDB Cluster\n"
"These features are in an experimental release state.\n"
"\n"
"Simple Commands:\n"
"REP START              Start Global Replication\n" 
"REP START REQUESTOR    Start Global Replication Requestor\n" 
"REP STATUS             Show Global Replication status\n" 
"REP STOP               Stop Global Replication\n"
"REP STOP REQUESTOR     Stop Global Replication Requestor\n"
"\n" 
"Advanced Commands:\n"
"REP START <protocol>   Starts protocol\n"
"REP STOP <protocol>    Stops protocol\n"
"<protocol> = TRANSFER | APPLY | DELETE\n"
"\n"
#ifdef VM_TRACE // DEBUG ONLY
"Debugging commands:\n"
"REP DELETE             Removes epochs stored in primary and standy systems\n"
"REP DROP <tableid>     Drop a table in SS identified by table id\n"
"REP SLOWSTOP           Stop Replication (Tries to synchonize with primary)\n" 
"REP FASTSTOP           Stop Replication (Stops in consistent state)\n" 
"<component> = SUBSCRIPTION\n"
"              METALOG | METASCAN | DATALOG | DATASCAN\n"
"              REQUESTOR | TRANSFER | APPLY | DELETE\n"
#endif
;
#endif // HAVE_GLOBAL_REPLICATION

#ifdef VM_TRACE // DEBUG ONLY
static const char* helpTextDebug =
"---------------------------------------------------------------------------\n"
" NDB Cluster -- Management Client -- Help for Debugging (Internal use only)\n"
"---------------------------------------------------------------------------\n"
"SHOW PROPERTIES                       Print config properties object\n"
"<id> LOGLEVEL {<category>=<level>}+   Set log level\n"
#ifdef ERROR_INSERT
"<id> ERROR <errorNo>                  Inject error into NDB node\n"
#endif
"<id> LOG [BLOCK = {ALL|<block>+}]     Set logging on in & out signals\n"
"<id> LOGIN [BLOCK = {ALL|<block>+}]   Set logging on in signals\n"
"<id> LOGOUT [BLOCK = {ALL|<block>+}]  Set logging on out signals\n"
"<id> LOGOFF [BLOCK = {ALL|<block>+}]  Unset signal logging\n"
"<id> TESTON                           Start signal logging\n"
"<id> TESTOFF                          Stop signal logging\n"
"<id> SET <configParamName> <value>    Update configuration variable\n"
"<id> DUMP <arg>                       Dump system state to cluster.log\n"
"<id> GETSTAT                          Print statistics\n"
"\n"
"<id>       = ALL | Any database node id\n"
;
#endif

static bool
convert(const char* s, int& val) {
  
  if (s == NULL)
    return false;

  if (strlen(s) == 0)
    return false;

  errno = 0;
  char* p;
  long v = strtol(s, &p, 10);
  if (errno != 0)
    return false;

  if (p != &s[strlen(s)])
    return false;
  
  val = v;
  return true;
}

/*
 * Constructor
 */
CommandInterpreter::CommandInterpreter(const char *_host,int verbose) 
  : m_verbose(verbose)
{
  m_mgmsrv = ndb_mgm_create_handle();
  if(m_mgmsrv == NULL) {
    ndbout_c("Cannot create handle to management server.");
    exit(-1);
  }
  m_mgmsrv2 = ndb_mgm_create_handle();
  if(m_mgmsrv2 == NULL) {
    ndbout_c("Cannot create 2:nd handle to management server.");
    exit(-1);
  }
  if (ndb_mgm_set_connectstring(m_mgmsrv, _host))
  {
    printError();
    exit(-1);
  }

  m_connected= false;
  m_event_thread= 0;
  try_reconnect = 0;
#ifdef HAVE_GLOBAL_REPLICATION
  rep_host = NULL;
  m_repserver = NULL;
  rep_connected = false;
#endif
}

/*
 * Destructor
 */
CommandInterpreter::~CommandInterpreter() 
{
  disconnect();
  ndb_mgm_destroy_handle(&m_mgmsrv);
  ndb_mgm_destroy_handle(&m_mgmsrv2);
}

static bool 
emptyString(const char* s) 
{
  if (s == NULL) {
    return true;
  }

  for (unsigned int i = 0; i < strlen(s); ++i) {
    if (! isspace(s[i])) {
      return false;
    }
  }

  return true;
}

void
CommandInterpreter::printError() 
{
  if (ndb_mgm_check_connection(m_mgmsrv))
  {
    m_connected= false;
    disconnect();
  }
  ndbout_c("* %5d: %s", 
	   ndb_mgm_get_latest_error(m_mgmsrv),
	   ndb_mgm_get_latest_error_msg(m_mgmsrv));
  ndbout_c("*        %s", ndb_mgm_get_latest_error_desc(m_mgmsrv));
}

//*****************************************************************************
//*****************************************************************************

static int do_event_thread;
static void*
event_thread_run(void* m)
{
  NdbMgmHandle handle= *(NdbMgmHandle*)m;

  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP, 0 };
  int fd = ndb_mgm_listen_event(handle, filter);
  if (fd > 0)
  {
    do_event_thread= 1;
    char *tmp= 0;
    char buf[1024];
    SocketInputStream in(fd,10);
    do {
      if (tmp == 0) NdbSleep_MilliSleep(10);
      if((tmp = in.gets(buf, 1024)))
	ndbout << tmp;
    } while(do_event_thread);
  }
  else
  {
    do_event_thread= -1;
  }

  return NULL;
}

bool
CommandInterpreter::connect() 
{
  DBUG_ENTER("CommandInterpreter::connect");
  if(!m_connected)
  {
    if(!ndb_mgm_connect(m_mgmsrv, try_reconnect-1, 5, 1))
    {
      const char *host= ndb_mgm_get_connected_host(m_mgmsrv);
      unsigned port= ndb_mgm_get_connected_port(m_mgmsrv);
      BaseString constr;
      constr.assfmt("%s:%d",host,port);
      if(!ndb_mgm_set_connectstring(m_mgmsrv2, constr.c_str()) &&
	 !ndb_mgm_connect(m_mgmsrv2, try_reconnect-1, 5, 1))
      {
	DBUG_PRINT("info",("2:ndb connected to Management Server ok at: %s:%d",
			   host, port));
	assert(m_event_thread == 0);
	assert(do_event_thread == 0);
	do_event_thread= 0;
	m_event_thread = NdbThread_Create(event_thread_run,
					  (void**)&m_mgmsrv2,
					  32768,
					  "CommandInterpreted_event_thread",
					  NDB_THREAD_PRIO_LOW);
	if (m_event_thread != 0)
	{
	  DBUG_PRINT("info",("Thread created ok, waiting for started..."));
	  int iter= 1000; // try for 30 seconds
	  while(do_event_thread == 0 &&
		iter-- > 0)
	    NdbSleep_MilliSleep(30);
	}
	if (m_event_thread == 0 ||
	    do_event_thread == 0 ||
	    do_event_thread == -1)
	{
	  DBUG_PRINT("warning",("thread not started"));
	  printf("Warning, event thread startup failed, degraded printouts as result\n");
	  do_event_thread= 0;
	}
      }
      else
      {
	DBUG_PRINT("warning",
		   ("Could not do 2:nd connect to mgmtserver for event listening"));
	DBUG_PRINT("info", ("code: %d, msg: %s",
		    ndb_mgm_get_latest_error(m_mgmsrv2),
		    ndb_mgm_get_latest_error_msg(m_mgmsrv2)));
	printf("Warning, event connect failed, degraded printouts as result\n");
	printf("code: %d, msg: %s\n",
	       ndb_mgm_get_latest_error(m_mgmsrv2),
	       ndb_mgm_get_latest_error_msg(m_mgmsrv2));
      }
      m_connected= true;
      DBUG_PRINT("info",("Connected to Management Server at: %s:%d", host, port));
      if (m_verbose)
      {
	printf("Connected to Management Server at: %s:%d\n",
	       host, port);
      }
    }
  }
  DBUG_RETURN(m_connected);
}

bool 
CommandInterpreter::disconnect() 
{
  if (m_event_thread) {
    void *res;
    do_event_thread= 0;
    NdbThread_WaitFor(m_event_thread, &res);
    NdbThread_Destroy(&m_event_thread);
    m_event_thread= 0;
    ndb_mgm_disconnect(m_mgmsrv2);
  }
  if (m_connected)
  {
    if (ndb_mgm_disconnect(m_mgmsrv) == -1) {
      ndbout_c("Could not disconnect from management server");
      printError();
    }
    m_connected= false;
  }
  return true;
}

//*****************************************************************************
//*****************************************************************************

int 
CommandInterpreter::execute(const char *_line, int _try_reconnect,
			    int *error) 
{
  if (_try_reconnect >= 0)
    try_reconnect=_try_reconnect;
  int result= execute_impl(_line);
  if (error)
    *error= m_error;
  return result;
}

static void
invalid_command(const char *cmd)
{
  ndbout << "Invalid command: " << cmd << endl;
  ndbout << "Type HELP for help." << endl << endl;
}

int 
CommandInterpreter::execute_impl(const char *_line) 
{
  DBUG_ENTER("CommandInterpreter::execute_impl");
  DBUG_PRINT("enter",("line=\"%s\"",_line));
  m_error= 0;

  char * line;
  if(_line == NULL) {
    DBUG_RETURN(false);
  }
  line = my_strdup(_line,MYF(MY_WME));
  My_auto_ptr<char> ptr(line);

  int do_continue;
  do {
    do_continue= 0;
    BaseString::trim(line," \t");
    if (line[0] == 0 ||
	line[0] == '#')
    {
      DBUG_RETURN(true);
    }
    // for mysql client compatability remove trailing ';'
    {
      unsigned last= strlen(line)-1;
      if (line[last] == ';')
      {
	line[last]= 0;
	do_continue= 1;
      }
    }
  } while (do_continue);
  // if there is anything in the line proceed
  char* firstToken = strtok(line, " ");
  char* allAfterFirstToken = strtok(NULL, "");
  
  if (strcasecmp(firstToken, "HELP") == 0 ||
      strcasecmp(firstToken, "?") == 0) {
    executeHelp(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "CONNECT") == 0) {
    executeConnect(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "SLEEP") == 0) {
    if (allAfterFirstToken)
      sleep(atoi(allAfterFirstToken));
    DBUG_RETURN(true);
  }
  else if((strcasecmp(firstToken, "QUIT") == 0 ||
	  strcasecmp(firstToken, "EXIT") == 0 ||
	  strcasecmp(firstToken, "BYE") == 0) && 
	  allAfterFirstToken == NULL){
    DBUG_RETURN(false);
  }

  if (!connect())
    DBUG_RETURN(true);

  if (strcasecmp(firstToken, "SHOW") == 0) {
    executeShow(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "SHUTDOWN") == 0) {
    m_error= executeShutdown(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "CLUSTERLOG") == 0){
    executeClusterLog(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if(strcasecmp(firstToken, "START") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncasecmp(allAfterFirstToken, "BACKUP", sizeof("BACKUP") - 1) == 0){
    m_error= executeStartBackup(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if(strcasecmp(firstToken, "ABORT") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncasecmp(allAfterFirstToken, "BACKUP", sizeof("BACKUP") - 1) == 0){
    executeAbortBackup(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "PURGE") == 0) {
    executePurge(allAfterFirstToken);
    DBUG_RETURN(true);
  } 
#ifdef HAVE_GLOBAL_REPLICATION
  else if(strcasecmp(firstToken, "REPLICATION") == 0 ||
	  strcasecmp(firstToken, "REP") == 0) {
    executeRep(allAfterFirstToken);
    DBUG_RETURN(true);
  }
#endif // HAVE_GLOBAL_REPLICATION
  else if(strcasecmp(firstToken, "ENTER") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncasecmp(allAfterFirstToken, "SINGLE USER MODE ", 
		  sizeof("SINGLE USER MODE") - 1) == 0){
    executeEnterSingleUser(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if(strcasecmp(firstToken, "EXIT") == 0 &&
	  allAfterFirstToken != NULL &&
	  strncasecmp(allAfterFirstToken, "SINGLE USER MODE ", 
		  sizeof("SINGLE USER MODE") - 1) == 0){
    executeExitSingleUser(allAfterFirstToken);
    DBUG_RETURN(true);
  }
  else if (strcasecmp(firstToken, "ALL") == 0) {
    analyseAfterFirstToken(-1, allAfterFirstToken);
  } else {
    /**
     * First token should be a digit, node ID
     */
    int nodeId;

    if (! convert(firstToken, nodeId)) {
      invalid_command(_line);
      DBUG_RETURN(true);
    }

    if (nodeId <= 0) {
      ndbout << "Invalid node ID: " << firstToken << "." << endl;
      DBUG_RETURN(true);
    }
    
    analyseAfterFirstToken(nodeId, allAfterFirstToken);
    
  }
  DBUG_RETURN(true);
}


/**
 * List of commands used as second command argument
 */
static const CommandInterpreter::CommandFunctionPair commands[] = {
  { "START", &CommandInterpreter::executeStart }
  ,{ "RESTART", &CommandInterpreter::executeRestart }
  ,{ "STOP", &CommandInterpreter::executeStop }
  ,{ "STATUS", &CommandInterpreter::executeStatus }
  ,{ "LOGLEVEL", &CommandInterpreter::executeLogLevel }
  ,{ "CLUSTERLOG", &CommandInterpreter::executeEventReporting }
#ifdef ERROR_INSERT
  ,{ "ERROR", &CommandInterpreter::executeError }
#endif
  ,{ "LOG", &CommandInterpreter::executeLog }
  ,{ "LOGIN", &CommandInterpreter::executeLogIn }
  ,{ "LOGOUT", &CommandInterpreter::executeLogOut }
  ,{ "LOGOFF", &CommandInterpreter::executeLogOff }
  ,{ "TESTON", &CommandInterpreter::executeTestOn }
  ,{ "TESTOFF", &CommandInterpreter::executeTestOff }
  ,{ "SET", &CommandInterpreter::executeSet }
  ,{ "GETSTAT", &CommandInterpreter::executeGetStat }
  ,{ "DUMP", &CommandInterpreter::executeDumpState }
};


//*****************************************************************************
//*****************************************************************************
void
CommandInterpreter::analyseAfterFirstToken(int processId,
					   char* allAfterFirstToken) {
  
  if (emptyString(allAfterFirstToken)) {
    ndbout << "Expected a command after "
	   << ((processId == -1) ? "ALL." : "node ID.") << endl;
    return;
  }
  
  char* secondToken = strtok(allAfterFirstToken, " ");
  char* allAfterSecondToken = strtok(NULL, "\0");

  const int tmpSize = sizeof(commands)/sizeof(CommandFunctionPair);
  ExecuteFunction fun = 0;
  const char * command = 0;
  for(int i = 0; i<tmpSize; i++){
    if(strcasecmp(secondToken, commands[i].command) == 0){
      fun = commands[i].executeFunction;
      command = commands[i].command;
      break;
    }
  }
  
  if(fun == 0){
    invalid_command(secondToken);
    return;
  }
  
  if(processId == -1){
    executeForAll(command, fun, allAfterSecondToken);
  } else {
    (this->*fun)(processId, allAfterSecondToken, false);
  }
  ndbout << endl;
}

/**
 * Get next nodeid larger than the give node_id. node_id will be
 * set to the next node_id in the list. node_id should be set
 * to 0 (zero) on the first call.
 *
 * @param handle the NDB management handle
 * @param node_id last node_id retreived, 0 at first call
 * @param type type of node to look for
 * @return 1 if a node was found, 0 if no more node exist
 */
static 
int 
get_next_nodeid(struct ndb_mgm_cluster_state *cl,
		int *node_id,
		enum ndb_mgm_node_type type)
{
  int i;
  
  if(cl == NULL)
    return 0;
  
  i=0;
  while((i < cl->no_of_nodes)) {
    if((*node_id < cl->node_states[i].node_id) &&
       (cl->node_states[i].node_type == type)) {
      
      if(i >= cl->no_of_nodes)
	return 0;
      
      *node_id = cl->node_states[i].node_id;
      return 1;
    }
    i++;
  }
  
  return 0;
}

void
CommandInterpreter::executeForAll(const char * cmd, ExecuteFunction fun, 
				  const char * allAfterSecondToken)
{
  int nodeId = 0;
  if(strcasecmp(cmd, "STOP") == 0) {
    ndbout_c("Executing STOP on all nodes.");
    (this->*fun)(nodeId, allAfterSecondToken, true);
  } else if(strcasecmp(cmd, "RESTART") == 0) {
    ndbout_c("Executing RESTART on all nodes.");
    ndbout_c("Starting shutdown. This may take a while. Please wait...");
    (this->*fun)(nodeId, allAfterSecondToken, true);
    ndbout_c("Trying to start all nodes of system.");
    ndbout_c("Use ALL STATUS to see the system start-up phases.");
  } else {
    struct ndb_mgm_cluster_state *cl= ndb_mgm_get_status(m_mgmsrv);
    if(cl == 0){
      ndbout_c("Unable get status from management server");
      printError();
      return;
    }
    NdbAutoPtr<char> ap1((char*)cl);
    while(get_next_nodeid(cl, &nodeId, NDB_MGM_NODE_TYPE_NDB))
      (this->*fun)(nodeId, allAfterSecondToken, true);
  }
}

//*****************************************************************************
//*****************************************************************************
bool 
CommandInterpreter::parseBlockSpecification(const char* allAfterLog,
					    Vector<const char*>& blocks) 
{
  // Parse: [BLOCK = {ALL|<blockName>+}]

  if (emptyString(allAfterLog)) {
    return true;
  }

  // Copy allAfterLog since strtok will modify it  
  char* newAllAfterLog = my_strdup(allAfterLog,MYF(MY_WME));
  My_auto_ptr<char> ap1(newAllAfterLog);
  char* firstTokenAfterLog = strtok(newAllAfterLog, " ");
  for (unsigned int i = 0; i < strlen(firstTokenAfterLog); ++i) {
    firstTokenAfterLog[i] = toupper(firstTokenAfterLog[i]);
  }
  
  if (strcasecmp(firstTokenAfterLog, "BLOCK") != 0) {
    ndbout << "Unexpected value: " << firstTokenAfterLog 
	   << ". Expected BLOCK." << endl;
    return false;
  }

  char* allAfterFirstToken = strtok(NULL, "\0");
  if (emptyString(allAfterFirstToken)) {
    ndbout << "Expected =." << endl;
    return false;
  }

  char* secondTokenAfterLog = strtok(allAfterFirstToken, " ");
  if (strcasecmp(secondTokenAfterLog, "=") != 0) {
    ndbout << "Unexpected value: " << secondTokenAfterLog 
	   << ". Expected =." << endl;
    return false;
  }

  char* blockName = strtok(NULL, " ");
  bool all = false;
  if (blockName != NULL && (strcasecmp(blockName, "ALL") == 0)) {
    all = true;
  }
  while (blockName != NULL) {
    blocks.push_back(strdup(blockName));
    blockName = strtok(NULL, " ");
  }

  if (blocks.size() == 0) {
    ndbout << "No block specified." << endl;
    return false;
  }
  if (blocks.size() > 1 && all) {
    // More than "ALL" specified
    ndbout << "Nothing expected after ALL." << endl;
    return false;
  }
  
  return true;
}



/*****************************************************************************
 * HELP
 *****************************************************************************/
void 
CommandInterpreter::executeHelp(char* parameters)
{
  if (emptyString(parameters)) {
    ndbout << helpText;

    ndbout << endl 
	   << "<severity> = " 
	   << "ALERT | CRITICAL | ERROR | WARNING | INFO | DEBUG"
	   << endl;

    ndbout << "<category> = ";
    for(int i = CFG_MIN_LOGLEVEL; i <= CFG_MAX_LOGLEVEL; i++){
      const char *str= ndb_mgm_get_event_category_string((ndb_mgm_event_category)i);
      if (str) {
	if (i != CFG_MIN_LOGLEVEL)
	  ndbout << " | ";
	ndbout << str;
      }
    }
    ndbout << endl;

    ndbout << "<level>    = " << "0 - 15" << endl;
    ndbout << "<id>       = " << "ALL | Any database node id" << endl;
    ndbout << endl;
  } else if (strcasecmp(parameters, "SHOW") == 0) {
    ndbout << helpTextShow;
#ifdef HAVE_GLOBAL_REPLICATION
  } else if (strcasecmp(parameters, "REPLICATION") == 0 ||
	     strcasecmp(parameters, "REP") == 0) {
    ndbout << helpTextRep;
#endif // HAVE_GLOBAL_REPLICATION
#ifdef VM_TRACE // DEBUG ONLY
  } else if (strcasecmp(parameters, "DEBUG") == 0) {
    ndbout << helpTextDebug;
#endif
  } else {
    invalid_command(parameters);
  }
}


/*****************************************************************************
 * SHUTDOWN
 *****************************************************************************/

int
CommandInterpreter::executeShutdown(char* parameters) 
{ 
  ndb_mgm_cluster_state *state = ndb_mgm_get_status(m_mgmsrv);
  if(state == NULL) {
    ndbout_c("Could not get status");
    printError();
    return 1;
  }
  NdbAutoPtr<char> ap1((char*)state);

  int result = 0;
  result = ndb_mgm_stop(m_mgmsrv, 0, 0);
  if (result < 0) {
    ndbout << "Shutdown off NDB Cluster storage node(s) failed." << endl;
    printError();
    return result;
  }

  ndbout << result << " NDB Cluster storage node(s) have shutdown." << endl;

  int mgm_id= 0;
  for(int i=0; i < state->no_of_nodes; i++) {
    if(state->node_states[i].node_type == NDB_MGM_NODE_TYPE_MGM &&
       state->node_states[i].version != 0){
      if (mgm_id == 0)
	mgm_id= state->node_states[i].node_id;
      else {
	ndbout << "Unable to locate management server, "
	       << "shutdown manually with <id> STOP"
	       << endl;
	return 1;
      }
    }
  }

  result = ndb_mgm_stop(m_mgmsrv, 1, &mgm_id);
  if (result <= 0) {
    ndbout << "Shutdown of NDB Cluster management server failed." << endl;
    printError();
    if (result == 0)
      return 1;
    return result;
  }

  m_connected= false;
  disconnect();
  ndbout << "NDB Cluster management server shutdown." << endl;
  return 0;
}

/*****************************************************************************
 * SHOW
 *****************************************************************************/


static
const char *status_string(ndb_mgm_node_status status)
{
  switch(status){
  case NDB_MGM_NODE_STATUS_NO_CONTACT:
    return "not connected";
  case NDB_MGM_NODE_STATUS_NOT_STARTED:
    return "not started";
  case NDB_MGM_NODE_STATUS_STARTING:
    return "starting";
  case NDB_MGM_NODE_STATUS_STARTED:
    return "started";
  case NDB_MGM_NODE_STATUS_SHUTTING_DOWN:
    return "shutting down";
  case NDB_MGM_NODE_STATUS_RESTARTING:
    return "restarting";
  case NDB_MGM_NODE_STATUS_SINGLEUSER:
    return "single user mode";
  default:
    return "unknown state";
  }
}

static void
print_nodes(ndb_mgm_cluster_state *state, ndb_mgm_configuration_iterator *it,
	    const char *proc_name, int no_proc, ndb_mgm_node_type type,
	    int master_id)
{ 
  int i;
  ndbout << "[" << proc_name
	 << "(" << ndb_mgm_get_node_type_string(type) << ")]\t"
	 << no_proc << " node(s)" << endl;
  for(i=0; i < state->no_of_nodes; i++) {
    struct ndb_mgm_node_state *node_state= &(state->node_states[i]);
    if(node_state->node_type == type) {
      int node_id= node_state->node_id;
      ndbout << "id=" << node_id;
      if(node_state->version != 0) {
	const char *hostname= node_state->connect_address;
	if (hostname == 0
	    || strlen(hostname) == 0
	    || strcasecmp(hostname,"0.0.0.0") == 0)
	  ndbout << " ";
	else
	  ndbout << "\t@" << hostname;
	ndbout << "  (Version: "
	       << getMajor(node_state->version) << "."
	       << getMinor(node_state->version) << "."
	       << getBuild(node_state->version);
	if (type == NDB_MGM_NODE_TYPE_NDB) {
	  if (node_state->node_status != NDB_MGM_NODE_STATUS_STARTED) {
	    ndbout << ", " << status_string(node_state->node_status);
	  }
	  if (node_state->node_group >= 0) {
	    ndbout << ", Nodegroup: " << node_state->node_group;
	    if (node_state->dynamic_id == master_id)
	      ndbout << ", Master";
	  }
	}
	ndbout << ")" << endl;
      } else {
	if(ndb_mgm_find(it, CFG_NODE_ID, node_id) != 0){
	  ndbout_c("Unable to find node with id: %d", node_id);
	  return;
	}
	const char *config_hostname= 0;
	ndb_mgm_get_string_parameter(it, CFG_NODE_HOST, &config_hostname);
	if (config_hostname == 0 || config_hostname[0] == 0)
	  config_hostname= "any host";
	ndbout << " (not connected, accepting connect from "
	       << config_hostname << ")" << endl;
      }
    }
  }
  ndbout << endl;
}

void
CommandInterpreter::executePurge(char* parameters) 
{ 
  int command_ok= 0;
  do {
    if (emptyString(parameters))
      break;
    char* firstToken = strtok(parameters, " ");
    char* nextToken = strtok(NULL, " \0");
    if (strcasecmp(firstToken,"STALE") == 0 &&
	nextToken &&
	strcasecmp(nextToken, "SESSIONS") == 0) {
      command_ok= 1;
      break;
    }
  } while(0);

  if (!command_ok) {
    ndbout_c("Unexpected command, expected: PURGE STALE SESSIONS");
    return;
  }

  int i;
  char *str;
  
  if (ndb_mgm_purge_stale_sessions(m_mgmsrv, &str)) {
    ndbout_c("Command failed");
    return;
  }
  if (str) {
    ndbout_c("Purged sessions with node id's: %s", str);
    free(str);
  }
  else
  {
    ndbout_c("No sessions purged");
  }
}

void
CommandInterpreter::executeShow(char* parameters) 
{ 
  int i;
  if (emptyString(parameters)) {
    ndb_mgm_cluster_state *state = ndb_mgm_get_status(m_mgmsrv);
    if(state == NULL) {
      ndbout_c("Could not get status");
      printError();
      return;
    }
    NdbAutoPtr<char> ap1((char*)state);

    ndb_mgm_configuration * conf = ndb_mgm_get_configuration(m_mgmsrv,0);
    if(conf == 0){
      ndbout_c("Could not get configuration");
      printError();
      return;
    }

    ndb_mgm_configuration_iterator * it;
    it = ndb_mgm_create_configuration_iterator((struct ndb_mgm_configuration *)conf, CFG_SECTION_NODE);

    if(it == 0){
      ndbout_c("Unable to create config iterator");
      return;
    }
    NdbAutoPtr<ndb_mgm_configuration_iterator> ptr(it);

    int
      master_id= 0,
      ndb_nodes= 0,
      api_nodes= 0,
      mgm_nodes= 0;

    for(i=0; i < state->no_of_nodes; i++) {
      if(state->node_states[i].node_type == NDB_MGM_NODE_TYPE_NDB &&
	 state->node_states[i].version != 0){
	master_id= state->node_states[i].dynamic_id;
	break;
      }
    }
    
    for(i=0; i < state->no_of_nodes; i++) {
      switch(state->node_states[i].node_type) {
      case NDB_MGM_NODE_TYPE_API:
	api_nodes++;
	break;
      case NDB_MGM_NODE_TYPE_NDB:
	if (state->node_states[i].dynamic_id < master_id)
	  master_id= state->node_states[i].dynamic_id;
	ndb_nodes++;
	break;
      case NDB_MGM_NODE_TYPE_MGM:
	mgm_nodes++;
	break;
      case NDB_MGM_NODE_TYPE_UNKNOWN:
        ndbout << "Error: Unknown Node Type" << endl;
        return;
      case NDB_MGM_NODE_TYPE_REP:
	abort();
      }
    }

    ndbout << "Cluster Configuration" << endl
	   << "---------------------" << endl;
    print_nodes(state, it, "ndbd",     ndb_nodes, NDB_MGM_NODE_TYPE_NDB, master_id);
    print_nodes(state, it, "ndb_mgmd", mgm_nodes, NDB_MGM_NODE_TYPE_MGM, 0);
    print_nodes(state, it, "mysqld",   api_nodes, NDB_MGM_NODE_TYPE_API, 0);
    //    ndbout << helpTextShow;
    return;
  } else if (strcasecmp(parameters, "PROPERTIES") == 0 ||
	     strcasecmp(parameters, "PROP") == 0) {
    ndbout << "SHOW PROPERTIES is not yet implemented." << endl;
    //  ndbout << "_mgmtSrvr.getConfig()->print();" << endl; /* XXX */
  } else if (strcasecmp(parameters, "CONFIGURATION") == 0 ||
	     strcasecmp(parameters, "CONFIG") == 0){
    ndbout << "SHOW CONFIGURATION is not yet implemented." << endl;
    //nbout << "_mgmtSrvr.getConfig()->printConfigFile();" << endl; /* XXX */
  } else if (strcasecmp(parameters, "PARAMETERS") == 0 ||
	     strcasecmp(parameters, "PARAMS") == 0 ||
	     strcasecmp(parameters, "PARAM") == 0) {
    ndbout << "SHOW PARAMETERS is not yet implemented." << endl;
    //    ndbout << "_mgmtSrvr.getConfig()->getConfigInfo()->print();" 
    //           << endl; /* XXX */
  } else {
    ndbout << "Invalid argument." << endl;
  }
}

void
CommandInterpreter::executeConnect(char* parameters) 
{
  disconnect();
  if (!emptyString(parameters)) {
    if (ndb_mgm_set_connectstring(m_mgmsrv,
				  BaseString(parameters).trim().c_str()))
    {
      printError();
      return;
    }
  }
  connect();
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeClusterLog(char* parameters) 
{
  DBUG_ENTER("CommandInterpreter::executeClusterLog");
  int i;
  if (emptyString(parameters))
  {
    ndbout << "Missing argument." << endl;
    DBUG_VOID_RETURN;
  }

  enum ndb_mgm_event_severity severity = NDB_MGM_EVENT_SEVERITY_ALL;
    
  char * tmpString = my_strdup(parameters,MYF(MY_WME));
  My_auto_ptr<char> ap1(tmpString);
  char * tmpPtr = 0;
  char * item = strtok_r(tmpString, " ", &tmpPtr);
  int enable;

  const unsigned int *enabled= ndb_mgm_get_logfilter(m_mgmsrv);
  if(enabled == NULL) {
    ndbout << "Couldn't get status" << endl;
    printError();
    DBUG_VOID_RETURN;
  }

  /********************
   * CLUSTERLOG INFO
   ********************/
  if (strcasecmp(item, "INFO") == 0) {
    DBUG_PRINT("info",("INFO"));
    if(enabled[0] == 0)
    {
      ndbout << "Cluster logging is disabled." << endl;
      DBUG_VOID_RETURN;
    }
#if 0 
    for(i = 0; i<7;i++)
      printf("enabled[%d] = %d\n", i, enabled[i]);
#endif
    ndbout << "Severities enabled: ";
    for(i = 1; i < (int)NDB_MGM_EVENT_SEVERITY_ALL; i++) {
      const char *str= ndb_mgm_get_event_severity_string((ndb_mgm_event_severity)i);
      if (str == 0)
      {
	DBUG_ASSERT(false);
	continue;
      }
      if(enabled[i])
	ndbout << BaseString(str).ndb_toupper() << " ";
    }
    ndbout << endl;
    DBUG_VOID_RETURN;

  } 
  else if (strcasecmp(item, "FILTER") == 0 ||
	   strcasecmp(item, "TOGGLE") == 0)
  {
    DBUG_PRINT("info",("TOGGLE"));
    enable= -1;
  } 
  else if (strcasecmp(item, "OFF") == 0) 
  {
    DBUG_PRINT("info",("OFF"));
    enable= 0;
  } else if (strcasecmp(item, "ON") == 0) {
    DBUG_PRINT("info",("ON"));
    enable= 1;
  } else {
    ndbout << "Invalid argument." << endl;
    DBUG_VOID_RETURN;
  }

  int res_enable;
  item = strtok_r(NULL, " ", &tmpPtr);
  if (item == NULL) {
    res_enable=
      ndb_mgm_set_clusterlog_severity_filter(m_mgmsrv,
					     NDB_MGM_EVENT_SEVERITY_ON,
					     enable, NULL);
    if (res_enable < 0)
    {
      ndbout << "Couldn't set filter" << endl;
      printError();
      DBUG_VOID_RETURN;
    }
    ndbout << "Cluster logging is " << (res_enable ? "enabled.":"disabled") << endl;
    DBUG_VOID_RETURN;
  }

  do {
    severity= NDB_MGM_ILLEGAL_EVENT_SEVERITY;
    if (strcasecmp(item, "ALL") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_ALL;	
    } else if (strcasecmp(item, "ALERT") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_ALERT;
    } else if (strcasecmp(item, "CRITICAL") == 0) { 
      severity = NDB_MGM_EVENT_SEVERITY_CRITICAL;
    } else if (strcasecmp(item, "ERROR") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_ERROR;
    } else if (strcasecmp(item, "WARNING") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_WARNING;
    } else if (strcasecmp(item, "INFO") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_INFO;
    } else if (strcasecmp(item, "DEBUG") == 0) {
      severity = NDB_MGM_EVENT_SEVERITY_DEBUG;
    } else if (strcasecmp(item, "OFF") == 0 ||
	       strcasecmp(item, "ON") == 0) {
      if (enable < 0) // only makes sense with toggle
	severity = NDB_MGM_EVENT_SEVERITY_ON;
    }
    if (severity == NDB_MGM_ILLEGAL_EVENT_SEVERITY) {
      ndbout << "Invalid severity level: " << item << endl;
      DBUG_VOID_RETURN;
    }

    res_enable= ndb_mgm_set_clusterlog_severity_filter(m_mgmsrv, severity,
						       enable, NULL);
    if (res_enable < 0)
    {
      ndbout << "Couldn't set filter" << endl;
      printError();
      DBUG_VOID_RETURN;
    }
    ndbout << BaseString(item).ndb_toupper().c_str() << " " << (res_enable ? "enabled":"disabled") << endl;

    item = strtok_r(NULL, " ", &tmpPtr);	
  } while(item != NULL);

  DBUG_VOID_RETURN;
} 

//*****************************************************************************
//*****************************************************************************

void
CommandInterpreter::executeStop(int processId, const char *, bool all) 
{
  int result = 0;
  if(all) {
    result = ndb_mgm_stop(m_mgmsrv, 0, 0);
  } else {
    result = ndb_mgm_stop(m_mgmsrv, 1, &processId);
  }
  if (result < 0) {
    ndbout << "Shutdown failed." << endl;
    printError();
  } else
    {
      if(all)
	ndbout << "NDB Cluster has shutdown." << endl;
      else
	ndbout << "Node " << processId << " has shutdown." << endl;
    }
}

void
CommandInterpreter::executeEnterSingleUser(char* parameters) 
{
  strtok(parameters, " ");
  struct ndb_mgm_reply reply;
  char* id = strtok(NULL, " ");
  id = strtok(NULL, " ");
  id = strtok(NULL, "\0");
  int nodeId = -1;
  if(id == 0 || sscanf(id, "%d", &nodeId) != 1){
    ndbout_c("Invalid arguments: expected <NodeId>");
    ndbout_c("Use SHOW to see what API nodes are configured");
    return;
  }
  int result = ndb_mgm_enter_single_user(m_mgmsrv, nodeId, &reply);
  
  if (result != 0) {
    ndbout_c("Entering single user mode for node %d failed", nodeId);
    printError();
  } else {
    ndbout_c("Entering single user mode");
    ndbout_c("Access will be granted for API node %d only.", nodeId);
    ndbout_c("Use ALL STATUS to see when single user mode has been entered.");
  }
}

void 
CommandInterpreter::executeExitSingleUser(char* parameters) 
{
  int result = ndb_mgm_exit_single_user(m_mgmsrv, 0);
  if (result != 0) {
    ndbout_c("Exiting single user mode failed.");
    printError();
  } else {
    ndbout_c("Exiting single user mode in progress.");
    ndbout_c("Use ALL STATUS to see when single user mode has been exited.");
  }
}

void
CommandInterpreter::executeStart(int processId, const char* parameters,
				 bool all) 
{
  int result;
  if(all) {
    result = ndb_mgm_start(m_mgmsrv, 0, 0);
  } else {
    result = ndb_mgm_start(m_mgmsrv, 1, &processId);
  }

  if (result <= 0) {
    ndbout << "Start failed." << endl;
    printError();
  } else
    {
      if(all)
	ndbout_c("NDB Cluster is being started.");
      else
	ndbout_c("Database node %d is being started.", processId);
    }
}

void
CommandInterpreter::executeRestart(int processId, const char* parameters,
				   bool all) 
{
  int result;
  int nostart = 0;
  int initialstart = 0;
  int abort = 0;

  if(parameters != 0 && strlen(parameters) != 0){
    char * tmpString = my_strdup(parameters,MYF(MY_WME));
    My_auto_ptr<char> ap1(tmpString);
    char * tmpPtr = 0;
    char * item = strtok_r(tmpString, " ", &tmpPtr);
    while(item != NULL){
      if(strcasecmp(item, "-N") == 0)
	nostart = 1;
      if(strcasecmp(item, "-I") == 0)
	initialstart = 1;
      if(strcasecmp(item, "-A") == 0)
	abort = 1;
      item = strtok_r(NULL, " ", &tmpPtr);
    }
  }

  if(all) {
    result = ndb_mgm_restart2(m_mgmsrv, 0, NULL, initialstart, nostart, abort);
  } else {
    int v[1];
    v[0] = processId;
    result = ndb_mgm_restart2(m_mgmsrv, 1, v, initialstart, nostart, abort);
  }
  
  if (result <= 0) {
    ndbout.println("Restart failed.", result);
    printError();
  } else
    {
      if(all)
	ndbout << "NDB Cluster is being restarted." << endl;
      else
	ndbout_c("Node %d is being restarted.", processId);
    }
}

void
CommandInterpreter::executeDumpState(int processId, const char* parameters,
				     bool all) 
{
  if(emptyString(parameters)){
    ndbout << "Expected argument" << endl;
    return;
  }

  Uint32 no = 0;
  int pars[25];
  
  char * tmpString = my_strdup(parameters,MYF(MY_WME));
  My_auto_ptr<char> ap1(tmpString);
  char * tmpPtr = 0;
  char * item = strtok_r(tmpString, " ", &tmpPtr);
  while(item != NULL){
    if (0x0 <= strtoll(item, NULL, 0) && strtoll(item, NULL, 0) <= 0xffffffff){
      pars[no] = strtoll(item, NULL, 0); 
    } else {
      ndbout << "Illegal value in argument to signal." << endl
	     << "(Value must be between 0 and 0xffffffff.)" 
	     << endl;
      return;
    }
    no++;
    item = strtok_r(NULL, " ", &tmpPtr);
  }
  ndbout << "Sending dump signal with data:" << endl;
  for (Uint32 i=0; i<no; i++) {
    ndbout.setHexFormat(1) << pars[i] << " ";
    if (!(i+1 & 0x3)) ndbout << endl;
  }
  
  struct ndb_mgm_reply reply;
  ndb_mgm_dump_state(m_mgmsrv, processId, pars, no, &reply);
}

void 
CommandInterpreter::executeStatus(int processId, 
				  const char* parameters, bool all) 
{
  if (! emptyString(parameters)) {
    ndbout_c("No parameters expected to this command.");
    return;
  }

  ndb_mgm_node_status status;
  Uint32 startPhase, version;
  bool system;
  
  struct ndb_mgm_cluster_state *cl;
  cl = ndb_mgm_get_status(m_mgmsrv);
  if(cl == NULL) {
    ndbout_c("Cannot get status of node %d.", processId);
    printError();
    return;
  }
  NdbAutoPtr<char> ap1((char*)cl);

  int i = 0;
  while((i < cl->no_of_nodes) && cl->node_states[i].node_id != processId)
    i++;
  if(cl->node_states[i].node_id != processId) {
    ndbout << processId << ": Node not found" << endl;
    return;
  }
  status = cl->node_states[i].node_status;
  startPhase = cl->node_states[i].start_phase;
  version = cl->node_states[i].version;

  ndbout << "Node " << processId << ": " << status_string(status);
  switch(status){
  case NDB_MGM_NODE_STATUS_STARTING:
    ndbout << " (Phase " << startPhase << ")";
    break;
  case NDB_MGM_NODE_STATUS_SHUTTING_DOWN:
    ndbout << " (Phase " << startPhase << ")";
    break;
  default:
    break;
  }
  if(status != NDB_MGM_NODE_STATUS_NO_CONTACT)
    ndbout_c(" (Version %d.%d.%d)", 
	     getMajor(version) ,
	     getMinor(version),
	     getBuild(version));
  else
    ndbout << endl;
}


//*****************************************************************************
//*****************************************************************************

void 
CommandInterpreter::executeLogLevel(int processId, const char* parameters, 
				    bool all) 
{
  (void) all;
  if (emptyString(parameters)) {
    ndbout << "Expected argument" << endl;
    return;
  } 
  BaseString tmp(parameters);
  Vector<BaseString> spec;
  tmp.split(spec, "=");
  if(spec.size() != 2){
    ndbout << "Invalid loglevel specification: " << parameters << endl;
    return;
  }

  spec[0].trim().ndb_toupper();
  int category = ndb_mgm_match_event_category(spec[0].c_str());
  if(category == NDB_MGM_ILLEGAL_EVENT_CATEGORY){
    category = atoi(spec[0].c_str());
    if(category < NDB_MGM_MIN_EVENT_CATEGORY ||
       category > NDB_MGM_MAX_EVENT_CATEGORY){
      ndbout << "Unknown category: \"" << spec[0].c_str() << "\"" << endl;
      return;
    }
  }
  
  int level = atoi(spec[1].c_str());
  if(level < 0 || level > 15){
    ndbout << "Invalid level: " << spec[1].c_str() << endl;
    return;
  }
  
  ndbout << "Executing LOGLEVEL on node " << processId << flush;

  struct ndb_mgm_reply reply;
  int result;
  result = ndb_mgm_set_loglevel_node(m_mgmsrv, 
				     processId,
				     (ndb_mgm_event_category)category,
				     level, 
				     &reply);
  
  if (result < 0) {
    ndbout_c(" failed.");
    printError();
  } else {
    ndbout_c(" OK!");
  }  
  
}

//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeError(int processId, 
				      const char* parameters, bool /* all */) 
{
  if (emptyString(parameters)) {
    ndbout << "Missing error number." << endl;
    return;
  }

  // Copy parameters since strtok will modify it
  char* newpar = my_strdup(parameters,MYF(MY_WME)); 
  My_auto_ptr<char> ap1(newpar);
  char* firstParameter = strtok(newpar, " ");

  int errorNo;
  if (! convert(firstParameter, errorNo)) {
    ndbout << "Expected an integer." << endl;
    return;
  }

  char* allAfterFirstParameter = strtok(NULL, "\0");
  if (! emptyString(allAfterFirstParameter)) {
    ndbout << "Nothing expected after error number." << endl;
    return;
  }

  ndb_mgm_insert_error(m_mgmsrv, processId, errorNo, NULL);
}

//*****************************************************************************
//*****************************************************************************

void 
CommandInterpreter::executeLog(int processId,
			       const char* parameters, bool all) 
{
  struct ndb_mgm_reply reply;
  Vector<const char *> blocks;
  if (! parseBlockSpecification(parameters, blocks)) {
    return;
  }
  int len=1;
  Uint32 i;
  for(i=0; i<blocks.size(); i++) {
    len += strlen(blocks[i]) + 1;
  }
  char * blockNames = (char*)my_malloc(len,MYF(MY_WME));
  My_auto_ptr<char> ap1(blockNames);
  
  blockNames[0] = 0;
  for(i=0; i<blocks.size(); i++) {
    strcat(blockNames, blocks[i]);
    strcat(blockNames, "|");
  }
  
  int result = ndb_mgm_log_signals(m_mgmsrv,
				   processId, 
				   NDB_MGM_SIGNAL_LOG_MODE_INOUT, 
				   blockNames,
				   &reply);
  if (result != 0) {
    ndbout_c("Execute LOG on node %d failed.", processId);
    printError();
  }
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeLogIn(int /* processId */,
				 const char* parameters, bool /* all */) 
{
  ndbout << "Command LOGIN not implemented." << endl;
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeLogOut(int /*processId*/, 
				  const char* parameters, bool /*all*/) 
{
  ndbout << "Command LOGOUT not implemented." << endl;
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeLogOff(int /*processId*/,
				  const char* parameters, bool /*all*/) 
{
  ndbout << "Command LOGOFF not implemented." << endl;
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeTestOn(int processId,
				  const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }
  struct ndb_mgm_reply reply;
  int result = ndb_mgm_start_signallog(m_mgmsrv, processId, &reply);
  if (result != 0) {
    ndbout_c("Execute TESTON failed.");
    printError();
  }
}

//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeTestOff(int processId,
				   const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }
  struct ndb_mgm_reply reply;
  int result = ndb_mgm_stop_signallog(m_mgmsrv, processId, &reply);
  if (result != 0) {
    ndbout_c("Execute TESTOFF failed.");
    printError();
  }
}


//*****************************************************************************
//*****************************************************************************
void 
CommandInterpreter::executeSet(int /*processId*/, 
			       const char* parameters, bool /*all*/) 
{
  if (emptyString(parameters)) {
    ndbout << "Missing parameter name." << endl;
    return;
  }
#if 0
  // Copy parameters since strtok will modify it
  char* newpar = my_strdup(parameters,MYF(MY_WME));
  My_auto_ptr<char> ap1(newpar);
  char* configParameterName = strtok(newpar, " ");

  char* allAfterParameterName = strtok(NULL, "\0");
  if (emptyString(allAfterParameterName)) {
    ndbout << "Missing parameter value." << endl;
    return;
  }

  char* value = strtok(allAfterParameterName, " ");

  char* allAfterValue = strtok(NULL, "\0");
  if (! emptyString(allAfterValue)) {
    ndbout << "Nothing expected after parameter value." << endl;
    return;
  }

  bool configBackupFileUpdated;
  bool configPrimaryFileUpdated;
  
  // TODO The handling of the primary and backup config files should be 
  // analysed further.
  // How it should be handled if only the backup is possible to write.

  int result = _mgmtSrvr.updateConfigParam(processId, configParameterName, 
					   value, configBackupFileUpdated, 
					   configPrimaryFileUpdated);
  if (result == 0) {
    if (configBackupFileUpdated && configPrimaryFileUpdated) {
      ndbout << "The configuration is updated." << endl;
    }
    else if (configBackupFileUpdated && !configPrimaryFileUpdated) {
      ndbout << "The configuration is updated but it was only possible " 
	     << "to update the backup configuration file, not the primary." 
	     << endl;
    }
    else {
      assert(false);
    }
  }
  else {
    ndbout << get_error_text(result) << endl;
    if (configBackupFileUpdated && configPrimaryFileUpdated) {
      ndbout << "The configuration files are however updated and "
	     << "the value will be used next time the process is restarted." 
	     << endl;
    }
    else if (configBackupFileUpdated && !configPrimaryFileUpdated) {
      ndbout << "It was only possible to update the backup "
	     << "configuration file, not the primary." << endl;
    }
    else if (!configBackupFileUpdated && !configPrimaryFileUpdated) {
      ndbout << "The configuration files are not updated." << endl;
    }
    else {
      // The primary is not tried to write if the write of backup file fails
      abort();
    }
  }
#endif
}

//*****************************************************************************
//*****************************************************************************
void CommandInterpreter::executeGetStat(int /*processId*/,
					const char* parameters, bool /*all*/) 
{
  if (! emptyString(parameters)) {
    ndbout << "No parameters expected to this command." << endl;
    return;
  }

#if 0
  MgmtSrvr::Statistics statistics;
  int result = _mgmtSrvr.getStatistics(processId, statistics);
  if (result != 0) {
    ndbout << get_error_text(result) << endl;
    return;
  }
#endif
  // Print statistic...
  /*
  ndbout << "Number of GETSTAT commands: " 
  << statistics._test1 << endl;
  */
}

//*****************************************************************************
//*****************************************************************************
				 
void 
CommandInterpreter::executeEventReporting(int processId,
					  const char* parameters, 
					  bool all) 
{
  if (emptyString(parameters)) {
    ndbout << "Expected argument" << endl;
    return;
  }
  BaseString tmp(parameters);
  Vector<BaseString> spec;
  tmp.split(spec, "=");
  if(spec.size() != 2){
    ndbout << "Invalid loglevel specification: " << parameters << endl;
    return;
  }

  spec[0].trim().ndb_toupper();
  int category = ndb_mgm_match_event_category(spec[0].c_str());
  if(category == NDB_MGM_ILLEGAL_EVENT_CATEGORY){
    if(!convert(spec[0].c_str(), category) ||
       category < NDB_MGM_MIN_EVENT_CATEGORY ||
       category > NDB_MGM_MAX_EVENT_CATEGORY){
      ndbout << "Unknown category: \"" << spec[0].c_str() << "\"" << endl;
      return;
    }
  }

  int level;
  if (!convert(spec[1].c_str(),level))
  {
    ndbout << "Invalid level: " << spec[1].c_str() << endl;
    return;
  }

  ndbout << "Executing CLUSTERLOG on node " << processId << flush;

  struct ndb_mgm_reply reply;
  int result;
  result = ndb_mgm_set_loglevel_clusterlog(m_mgmsrv, 
					   processId,
					   (ndb_mgm_event_category)category,
					   level, 
					   &reply);
  
  if (result != 0) {
    ndbout_c(" failed."); 
    printError();
  } else {
    ndbout_c(" OK!"); 
  }  
}

/*****************************************************************************
 * Backup
 *****************************************************************************/
int
CommandInterpreter::executeStartBackup(char* parameters)
{
  struct ndb_mgm_reply reply;
  unsigned int backupId;
#if 0
  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP, 0 };
  int fd = ndb_mgm_listen_event(m_mgmsrv, filter);
  if (fd < 0)
  {
    ndbout << "Initializing start of backup failed" << endl;
    printError();
    return fd;
  }
#endif
  Vector<BaseString> args;
  {
    BaseString(parameters).split(args);
    for (unsigned i= 0; i < args.size(); i++)
      if (args[i].length() == 0)
	args.erase(i--);
      else
	args[i].ndb_toupper();
  }
  int sz= args.size();

  int result;
  if (sz == 2 &&
      args[1] == "NOWAIT")
  {
    result = ndb_mgm_start_backup(m_mgmsrv, 0, &backupId, &reply);
  }
  else if (sz == 1 ||
	   (sz == 3 &&
	    args[1] == "WAIT" &&
	    args[2] == "COMPLETED"))
  {
    ndbout_c("Waiting for completed, this may take several minutes");
    result = ndb_mgm_start_backup(m_mgmsrv, 2, &backupId, &reply);
  }
  else if (sz == 3 &&
	   args[1] == "WAIT" &&
	   args[2] == "STARTED")
  {
    ndbout_c("Waiting for started, this may take several minutes");
    result = ndb_mgm_start_backup(m_mgmsrv, 1, &backupId, &reply);
  }
  else
  {
    invalid_command(parameters);
    return -1;
  }

  if (result != 0) {
    ndbout << "Start of backup failed" << endl;
    printError();
#if 0
    close(fd);
#endif
    return result;
  }
#if 0
  ndbout_c("Waiting for completed, this may take several minutes");
  char *tmp;
  char buf[1024];
  {
    SocketInputStream in(fd);
    int count = 0;
    do {
      tmp = in.gets(buf, 1024);
      if(tmp)
      {
	ndbout << tmp;
	unsigned int id;
	if(sscanf(tmp, "%*[^:]: Backup %d ", &id) == 1 && id == backupId){
	  count++;
	}
      }
    } while(count < 2);
  }

  SocketInputStream in(fd, 10);
  do {
    tmp = in.gets(buf, 1024);
    if(tmp && tmp[0] != 0)
    {
      ndbout << tmp;
    }
  } while(tmp && tmp[0] != 0);

  close(fd);
#endif  
  return 0;
}

void
CommandInterpreter::executeAbortBackup(char* parameters) 
{
  int bid = -1;
  struct ndb_mgm_reply reply;
  if (emptyString(parameters))
    goto executeAbortBackupError1;

  {
    strtok(parameters, " ");
    char* id = strtok(NULL, "\0");
    if(id == 0 || sscanf(id, "%d", &bid) != 1)
      goto executeAbortBackupError1;
  }
  {
    int result= ndb_mgm_abort_backup(m_mgmsrv, bid, &reply);
    if (result != 0) {
      ndbout << "Abort of backup " << bid << " failed" << endl;
      printError();
    } else {
      ndbout << "Abort of backup " << bid << " ordered" << endl;
    }
  }
  return;
 executeAbortBackupError1:
  ndbout << "Invalid arguments: expected <BackupId>" << endl;
  return;
}

#ifdef HAVE_GLOBAL_REPLICATION
/*****************************************************************************
 * Global Replication
 *
 * For information about the different commands, see
 * GrepReq::Request in file signaldata/grepImpl.cpp.
 *
 * Below are commands as of 2003-07-05 (may change!):
 * START = 0,            ///< Start Global Replication (all phases)
 * START_METALOG = 1,    ///< Start Global Replication (all phases)
 * START_METASCAN = 2,   ///< Start Global Replication (all phases)
 * START_DATALOG = 3,    ///< Start Global Replication (all phases)
 * START_DATASCAN = 4,   ///< Start Global Replication (all phases)
 * START_REQUESTOR = 5,  ///< Start Global Replication (all phases)
 * ABORT = 6,            ///< Immediate stop (removes subscription)
 * SLOW_STOP = 7,        ///< Stop after finishing applying current GCI epoch
 * FAST_STOP = 8,        ///< Stop after finishing applying all PS GCI epochs
 * START_TRANSFER = 9,   ///< Start SS-PS transfer
 * STOP_TRANSFER = 10,   ///< Stop SS-PS transfer
 * START_APPLY = 11,     ///< Start applying GCI epochs in SS
 * STOP_APPLY = 12,      ///< Stop applying GCI epochs in SS
 * STATUS = 13,           ///< Status
 * START_SUBSCR = 14,
 * REMOVE_BUFFERS = 15,
 * DROP_TABLE = 16

 *****************************************************************************/

void
CommandInterpreter::executeRep(char* parameters) 
{
  if (emptyString(parameters)) {
    ndbout << helpTextRep;
    return;
  }

  char * line = my_strdup(parameters,MYF(MY_WME));
  My_auto_ptr<char> ap1((char*)line);
  char * firstToken = strtok(line, " ");
  
  struct ndb_rep_reply  reply;
  unsigned int          repId;


  if (!strcasecmp(firstToken, "CONNECT")) {
    char * host = strtok(NULL, "\0");
    for (unsigned int i = 0; i < strlen(host); ++i) {
      host[i] = tolower(host[i]);
    }
    
    if(host == NULL)
    {
      ndbout_c("host:port must be specified.");
      return;
    }
    
    if(rep_connected) {
      if(m_repserver != NULL) {
	ndb_rep_disconnect(m_repserver);
	rep_connected = false;
      }       
    }
          
    if(m_repserver == NULL)
      m_repserver = ndb_rep_create_handle();
    if(ndb_rep_connect(m_repserver, host) < 0)
      ndbout_c("Failed to connect to %s", host); 
    else
      rep_connected=true;
    return;
    
    if(!rep_connected) {
      ndbout_c("Not connected to REP server");
    }
  }
    
  /********
   * START 
   ********/
  if (!strcasecmp(firstToken, "START")) {
    
    unsigned int          req;
    char *startType = strtok(NULL, "\0");
    
    if (startType == NULL) {                
      req = GrepReq::START;
    } else if (!strcasecmp(startType, "SUBSCRIPTION")) {  
      req = GrepReq::START_SUBSCR;
    } else if (!strcasecmp(startType, "METALOG")) { 
      req = GrepReq::START_METALOG;
    } else if (!strcasecmp(startType, "METASCAN")) {
      req = GrepReq::START_METASCAN;
    } else if (!strcasecmp(startType, "DATALOG")) {
      req = GrepReq::START_DATALOG;
    } else if (!strcasecmp(startType, "DATASCAN")) {
      req = GrepReq::START_DATASCAN;
    } else if (!strcasecmp(startType, "REQUESTOR")) {
      req = GrepReq::START_REQUESTOR;
    } else if (!strcasecmp(startType, "TRANSFER")) {
      req = GrepReq::START_TRANSFER;
    } else if (!strcasecmp(startType, "APPLY")) {
      req = GrepReq::START_APPLY;
    } else if (!strcasecmp(startType, "DELETE")) {
      req = GrepReq::START_DELETE;
    } else {
      ndbout_c("Illegal argument to command 'REPLICATION START'");
      return;
    }

    int result = ndb_rep_command(m_repserver, req, &repId, &reply);
    
    if (result != 0) {
      ndbout << "Start of Global Replication failed" << endl;
    } else {
      ndbout << "Start of Global Replication ordered" << endl;
    }
    return;
  }

  /********
   * STOP
   ********/
  if (!strcasecmp(firstToken, "STOP")) {    
    unsigned int          req;
    char *startType = strtok(NULL, " ");
    unsigned int epoch = 0;
    
    if (startType == NULL) {                 
      /**
       * Stop immediately
       */
      req = GrepReq::STOP;
    } else if (!strcasecmp(startType, "EPOCH")) {  
      char *strEpoch = strtok(NULL, "\0");
      if(strEpoch == NULL) {
	ndbout_c("Epoch expected!");
	return;
      }
      req = GrepReq::STOP;
      epoch=atoi(strEpoch);      
    } else if (!strcasecmp(startType, "SUBSCRIPTION")) {  
      req = GrepReq::STOP_SUBSCR;
    } else if (!strcasecmp(startType, "METALOG")) { 
      req = GrepReq::STOP_METALOG;
    } else if (!strcasecmp(startType, "METASCAN")) {
      req = GrepReq::STOP_METASCAN;
    } else if (!strcasecmp(startType, "DATALOG")) {
      req = GrepReq::STOP_DATALOG;
    } else if (!strcasecmp(startType, "DATASCAN")) {
      req = GrepReq::STOP_DATASCAN;
    } else if (!strcasecmp(startType, "REQUESTOR")) {
      req = GrepReq::STOP_REQUESTOR;
    } else if (!strcasecmp(startType, "TRANSFER")) {
      req = GrepReq::STOP_TRANSFER;
    } else if (!strcasecmp(startType, "APPLY")) {
      req = GrepReq::STOP_APPLY;
    } else if (!strcasecmp(startType, "DELETE")) {
      req = GrepReq::STOP_DELETE;
    } else {
      ndbout_c("Illegal argument to command 'REPLICATION STOP'");
      return;
    }
    int result = ndb_rep_command(m_repserver, req, &repId, &reply, epoch);
    
    if (result != 0) {
      ndbout << "Stop command failed" << endl;
    } else {
      ndbout << "Stop ordered" << endl;
    }
    return;
  }

  /*********
   * STATUS
   *********/
  if (!strcasecmp(firstToken, "STATUS")) {
    struct rep_state repstate;
    int result = 
      ndb_rep_get_status(m_repserver, &repId, &reply, &repstate);
    
    if (result != 0) {
      ndbout << "Status request of Global Replication failed" << endl;
    } else {
      ndbout << "Status request of Global Replication ordered" << endl;
      ndbout << "See printout at one of the DB nodes" << endl;
      ndbout << "(Better status report is under development.)" << endl;
      ndbout << " SubscriptionId " << repstate.subid 
	     << " SubscriptionKey " << repstate.subkey << endl;
    }
    return;
  }

  /*********
   * QUERY (see repapi.h for querable counters)
   *********/
  if (!strcasecmp(firstToken, "QUERY")) {
    char *query = strtok(NULL, "\0");
    int queryCounter=-1;
    if(query != NULL) {
      queryCounter = atoi(query);
    }
    struct rep_state repstate;
    unsigned repId = 0;
    int result = ndb_rep_query(m_repserver, (QueryCounter)queryCounter,
			       &repId, &reply, &repstate);
    
    if (result != 0) {
      ndbout << "Query repserver failed" << endl;
    } else {
      ndbout << "Query repserver sucessful" << endl;
      ndbout_c("repstate : QueryCounter %d, f=%d l=%d"
	       " nodegroups %d" , 
	       repstate.queryCounter,
	       repstate.first[0], repstate.last[0],
	       repstate.no_of_nodegroups );
    }
    return;
  }
}
#endif // HAVE_GLOBAL_REPLICATION

template class Vector<char const*>;
