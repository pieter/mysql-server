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
#include <ndb_opts.h>

#include "MgmtSrvr.hpp"
#include "EventLogger.hpp"
#include <Config.hpp>
#include "InitConfigFileParser.hpp"
#include <SocketServer.hpp>
#include "Services.hpp"
#include <version.h>
#include <kernel_types.h>
#include <Properties.hpp>
#include <NdbOut.hpp>
#include <NdbMain.h>
#include <NdbDaemon.h>
#include <NdbConfig.h>
#include <NdbHost.h>
#include <ndb_version.h>
#include <ConfigRetriever.hpp>
#include <mgmapi_config_parameters.h>

#include <NdbAutoPtr.hpp>

#if defined NDB_OSE || defined NDB_SOFTOSE
#include <efs.h>
#else
#include <ndb_mgmclient.hpp>
#endif

#undef DEBUG
#define DEBUG(x) ndbout << x << endl;

const char progname[] = "mgmtsrvr";

// copied from mysql.cc to get readline
extern "C" {
#if defined( __WIN__) || defined(OS2)
#include <conio.h>
#elif !defined(__NETWARE__)
#include <readline/readline.h>
extern "C" int add_history(const char *command); /* From readline directory */
#define HAVE_READLINE
#endif
}

static int 
read_and_execute(Ndb_mgmclient* com, const char * prompt, int _try_reconnect) 
{
  static char *line_read = (char *)NULL;

  /* If the buffer has already been allocated, return the memory
     to the free pool. */
  if (line_read)
  {
    free (line_read);
    line_read = (char *)NULL;
  }
#ifdef HAVE_READLINE
  /* Get a line from the user. */
  line_read = readline (prompt);    
  /* If the line has any text in it, save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);
#else
  static char linebuffer[254];
  fputs(prompt, stdout);
  linebuffer[sizeof(linebuffer)-1]=0;
  line_read = fgets(linebuffer, sizeof(linebuffer)-1, stdin);
  if (line_read == linebuffer) {
    char *q=linebuffer;
    while (*q > 31) q++;
    *q=0;
    line_read= strdup(linebuffer);
  }
#endif
  return com->execute(line_read,_try_reconnect);
}

/**
 * @struct  MgmGlobals
 * @brief   Global Variables used in the management server
 *****************************************************************************/
struct MgmGlobals {
  MgmGlobals();
  ~MgmGlobals();
  
  /** Command line arguments  */
  int daemon;   // NOT bool, bool need not be int
  int non_interactive;
  int interactive;
  const char * config_filename;
  
  /** Stuff found in environment or in local config  */
  NodeId localNodeId;
  bool use_specific_ip;
  char * interface_name;
  int port;
  
  /** The Mgmt Server */
  MgmtSrvr * mgmObject;
  
  /** The Socket Server */
  SocketServer * socketServer;
};

int g_no_nodeid_checks= 0;
static MgmGlobals glob;

/******************************************************************************
 * Function prototypes
 ******************************************************************************/
/**
 * Global variables
 */
bool g_StopServer;
extern EventLogger g_eventLogger;

extern int global_mgmt_server_check;

enum ndb_mgmd_options {
  OPT_INTERACTIVE = NDB_STD_OPTIONS_LAST,
  OPT_NO_NODEID_CHECKS,
  OPT_NO_DAEMON
};
NDB_STD_OPTS_VARS;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_mgmd"),
  { "config-file", 'f', "Specify cluster configuration file",
    (gptr*) &glob.config_filename, (gptr*) &glob.config_filename, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "daemon", 'd', "Run ndb_mgmd in daemon mode (default)",
    (gptr*) &glob.daemon, (gptr*) &glob.daemon, 0,
    GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0 },
  { "interactive", OPT_INTERACTIVE,
    "Run interactive. Not supported but provided for testing purposes",
    (gptr*) &glob.interactive, (gptr*) &glob.interactive, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "no-nodeid-checks", OPT_NO_NODEID_CHECKS,
    "Do not provide any node id checks", 
    (gptr*) &g_no_nodeid_checks, (gptr*) &g_no_nodeid_checks, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "nodaemon", OPT_NO_DAEMON,
    "Don't run as daemon, but don't read from stdin",
    (gptr*) &glob.non_interactive, (gptr*) &glob.non_interactive, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  printf("Usage: %s [OPTIONS]\n", my_progname);
}
static void usage()
{
  short_usage_sub();
  ndb_std_print_version();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  ndb_std_get_one_option(optid, opt, argument ? argument :
			 "d:t:O,/tmp/ndb_mgmd.trace");
  return 0;
}

/*
 *  MAIN 
 */
int main(int argc, char** argv)
{
  NDB_INIT(argv[0]);

  /**
   * OSE specific. Enable shared ownership of file system resources. 
   * This is needed in order to use the cluster log since the events 
   * from the cluster is written from the 'ndb_receive'(NDBAPI) thread/process.
   */
#if defined NDB_OSE || defined NDB_SOFTOSE
  efs_segment_share();
#endif

  global_mgmt_server_check = 1;

  const char *load_default_groups[]= { "mysql_cluster","ndb_mgmd",0 };
  load_defaults("my",load_default_groups,&argc,&argv);

  int ho_error;
  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (glob.interactive ||
      glob.non_interactive) {
    glob.daemon= 0;
  }

  glob.socketServer = new SocketServer();

  MgmApiService * mapi = new MgmApiService();

  glob.mgmObject = new MgmtSrvr(glob.socketServer,
				glob.config_filename,
				opt_connect_str);

  if (glob.mgmObject->init())
    goto error_end;

  my_setwd(NdbConfig_get_path(0), MYF(0));

  glob.localNodeId= glob.mgmObject->getOwnNodeId();
  if (glob.localNodeId == 0) {
    goto error_end;
  }

  glob.port= glob.mgmObject->getPort();

  if (glob.port == 0)
    goto error_end;

  glob.interface_name = 0;
  glob.use_specific_ip = false;

  if(!glob.use_specific_ip){
    int count= 5; // no of retries for tryBind
    while(!glob.socketServer->tryBind(glob.port, glob.interface_name)){
      if (--count > 0) {
	NdbSleep_MilliSleep(1000);
	continue;
      }
      ndbout_c("Unable to setup port: %s:%d!\n"
	       "Please check if the port is already used,\n"
	       "(perhaps a ndb_mgmd is already running),\n"
	       "and if you are executing on the correct computer", 
	       (glob.interface_name ? glob.interface_name : "*"), glob.port);
      goto error_end;
    }
    free(glob.interface_name);
    glob.interface_name = 0;
  }

  if(!glob.socketServer->setup(mapi, glob.port, glob.interface_name)){
    ndbout_c("Unable to setup management port: %d!\n"
	     "Please check if the port is already used,\n"
	     "(perhaps a ndb_mgmd is already running),\n"
	     "and if you are executing on the correct computer", 
	     glob.port);
    delete mapi;
    goto error_end;
  }
  
  if(!glob.mgmObject->check_start()){
    ndbout_c("Unable to check start management server.");
    ndbout_c("Probably caused by illegal initial configuration file.");
    goto error_end;
  }

  if (glob.daemon) {
    // Become a daemon
    char *lockfile= NdbConfig_PidFileName(glob.localNodeId);
    char *logfile=  NdbConfig_StdoutFileName(glob.localNodeId);
    NdbAutoPtr<char> tmp_aptr1(lockfile), tmp_aptr2(logfile);

    if (NdbDaemon_Make(lockfile, logfile, 0) == -1) {
      ndbout << "Cannot become daemon: " << NdbDaemon_ErrorText << endl;
      return 1;
    }
  }

#ifndef NDB_WIN32
  signal(SIGPIPE, SIG_IGN);
#endif
  {
    BaseString error_string;
    if(!glob.mgmObject->start(error_string)){
      ndbout_c("Unable to start management server.");
      ndbout_c("Probably caused by illegal initial configuration file.");
      ndbout_c(error_string.c_str());
      goto error_end;
    }
  }

  //glob.mgmObject->saveConfig();
  mapi->setMgm(glob.mgmObject);

  char msg[256];
  BaseString::snprintf(msg, sizeof(msg),
	   "NDB Cluster Management Server. %s", NDB_VERSION_STRING);
  ndbout_c(msg);
  g_eventLogger.info(msg);

  BaseString::snprintf(msg, 256, "Id: %d, Command port: %d",
	   glob.localNodeId, glob.port);
  ndbout_c(msg);
  g_eventLogger.info(msg);
  
  g_StopServer = false;
  glob.socketServer->startServer();

#if ! defined NDB_OSE && ! defined NDB_SOFTOSE
  if(glob.interactive) {
    BaseString con_str;
    if(glob.interface_name)
      con_str.appfmt("host=%s:%d", glob.interface_name, glob.port);
    else 
      con_str.appfmt("localhost:%d", glob.port);
    Ndb_mgmclient com(con_str.c_str(), 1);
    while(g_StopServer != true && read_and_execute(&com, "ndb_mgm> ", 1));
  } else 
#endif
  {
    while(g_StopServer != true)
      NdbSleep_MilliSleep(500);
  }
  
  g_eventLogger.info("Shutting down server...");
  glob.socketServer->stopServer();
  glob.socketServer->stopSessions();
  g_eventLogger.info("Shutdown complete");
  return 0;
 error_end:
  return 1;
}

MgmGlobals::MgmGlobals(){
  // Default values
  port = 0;
  config_filename = NULL;
  interface_name = 0;
  daemon = 1;
  non_interactive = 0;
  interactive = 0;
  socketServer = 0;
  mgmObject = 0;
}

MgmGlobals::~MgmGlobals(){
  if (socketServer)
    delete socketServer;
  if (mgmObject)
    delete mgmObject;
  if (interface_name)
    free(interface_name);
}
