/*
  Copyright (c) 2002, 2003 Novell, Inc. All Rights Reserved. 

  This program is free software; you can redistribute it and/or modify 
  it under the terms of the GNU General Public License as published by 
  the Free Software Foundation; either version 2 of the License, or 
  (at your option) any later version. 

  This program is distributed in the hope that it will be useful, 
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
  GNU General Public License for more details. 

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/ 

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#ifndef __WIN__
#include <dirent.h>
#endif
#include <string.h>
#ifdef __NETWARE__
#include <screen.h>
#include <nks/vm.h>
#endif
#include <ctype.h>
#include <sys/stat.h>
#ifndef __WIN__
#include <unistd.h>
#endif
#include <fcntl.h>
#ifdef __NETWARE__
#include <sys/mode.h>
#endif
#ifdef __WIN__
#include <windows.h>
#include <shlwapi.h>
#include <direct.h>
#endif

#include "my_manage.h"

/******************************************************************************

  macros

******************************************************************************/

#define HEADER  "TEST                                           RESULT  \n"
#define DASH    "-------------------------------------------------------\n"

#define NW_TEST_SUFFIX   ".nw-test"
#define NW_RESULT_SUFFIX ".nw-result"
#define TEST_SUFFIX      ".test"
#define RESULT_SUFFIX     ".result"
#define REJECT_SUFFIX    ".reject"
#define OUT_SUFFIX       ".out"
#define ERR_SUFFIX       ".err"

const char *TEST_PASS=   "[ pass ]";
const char *TEST_SKIP=   "[ skip ]";
const char *TEST_FAIL=   "[ fail ]";
const char *TEST_BAD=    "[ bad  ]";
const char *TEST_IGNORE= "[ignore]";

/******************************************************************************

  global variables

******************************************************************************/

#ifdef __NETWARE__
static char base_dir[FN_REFLEN]= "sys:/mysql";
#else
static char base_dir[FN_REFLEN]= "..";
#endif
static char db[FN_LEN]=       "test";
static char user[FN_LEN]=     "root";
static char password[FN_LEN]= "";

int master_port= 9306;
int slave_port=  9307;

#if !defined(__NETWARE__) && !defined(__WIN__)
static char master_socket[FN_REFLEN]= "./var/tmp/master.sock";
static char slave_socket[FN_REFLEN]=  "./var/tmp/slave.sock";
#endif

#define MAX_COUNT_TESTES 1024

#ifdef __WIN__
#  define sting_compare_func _stricmp
#else
#  ifdef HAVE_STRCASECMP
#    define sting_compare_func strcasecmp
#  else
#    define sting_compare_func strcmp
#  endif
#endif

/* comma delimited list of tests to skip or empty string */
#ifndef __WIN__
static char skip_test[FN_REFLEN]= " lowercase_table3 , system_mysql_db_fix ";
#else
/*
  The most ignore testes contain the calls of system command

  lowercase_table3 is disabled by Gerg
  system_mysql_db_fix  is disabled by Gerg
  sp contains a command system
  rpl_EE_error contains a command system
  rpl_loaddatalocal contains a command system
  ndb_autodiscover contains a command system
  rpl_rotate_logs contains a command system
  repair contains a command system
  rpl_trunc_binlog contains a command system
  mysqldump contains a command system
  rpl000001 makes non-exit loop...temporary skiped
*/
static char skip_test[FN_REFLEN]=
" lowercase_table3 ,"
" system_mysql_db_fix ,"
" sp ,"
" rpl_EE_error ,"
" rpl_loaddatalocal ,"
" ndb_autodiscover ,"
" rpl_rotate_logs ,"
" repair ,"
" rpl_trunc_binlog ,"
" mysqldump ,"
" rpl000001 ,"

" derived ,"
" group_by ,"
" select ,"
" rpl000015 ,"
" subselect ";
#endif
static char ignore_test[FN_REFLEN]= "";

static char bin_dir[FN_REFLEN];
static char mysql_test_dir[FN_REFLEN];
static char test_dir[FN_REFLEN];
static char mysql_tmp_dir[FN_REFLEN];
static char result_dir[FN_REFLEN];
static char master_dir[FN_REFLEN];
static char slave_dir[FN_REFLEN];
static char slave1_dir[FN_REFLEN];
static char slave2_dir[FN_REFLEN];
static char lang_dir[FN_REFLEN];
static char char_dir[FN_REFLEN];

static char mysqladmin_file[FN_REFLEN];
static char mysqld_file[FN_REFLEN];
static char mysqltest_file[FN_REFLEN];
#ifndef __WIN__
static char master_pid[FN_REFLEN];
static char slave_pid[FN_REFLEN];
static char sh_file[FN_REFLEN]= "/bin/sh";
#else
static HANDLE master_pid;
static HANDLE slave_pid;
#endif

static char master_opt[FN_REFLEN]= "";
static char slave_opt[FN_REFLEN]= "";

static char slave_master_info[FN_REFLEN]= "";

static char master_init_script[FN_REFLEN]= "";
static char slave_init_script[FN_REFLEN]= "";

/* OpenSSL */
static char ca_cert[FN_REFLEN];
static char server_cert[FN_REFLEN];
static char server_key[FN_REFLEN];
static char client_cert[FN_REFLEN];
static char client_key[FN_REFLEN];

int total_skip= 0;
int total_pass= 0;
int total_fail= 0;
int total_test= 0;

int total_ignore= 0;

int use_openssl=    FALSE;
int master_running= FALSE;
int slave_running=  FALSE;
int skip_slave=     TRUE;
int single_test=    TRUE;

int restarts= 0;

FILE *log_fd= NULL;

static char argument[FN_REFLEN];

/******************************************************************************

  functions

******************************************************************************/

/******************************************************************************

  prototypes

******************************************************************************/

void report_stats();
void install_db(char *);
void mysql_install_db();
void start_master();
void start_slave();
void mysql_start();
void stop_slave();
void stop_master();
void mysql_stop();
void mysql_restart();
int read_option(char *, char *);
void run_test(char *);
void setup(char *);
void vlog(const char *, va_list);
void mlog(const char *, ...);
void log_info(const char *, ...);
void log_error(const char *, ...);
void log_errno(const char *, ...);
void die(const char *);
char *str_tok(char* dest, char *string, const char *delim);
#ifndef __WIN__
void run_init_script(const char *script_name);
#endif
/******************************************************************************

  report_stats()

  Report the gathered statistics.

******************************************************************************/

void report_stats()
{
  if (total_fail == 0)
  {
    mlog("\nAll %d test(s) were successful.\n", total_test);
  }
  else
  {
    double percent= ((double)total_pass / total_test) * 100;

    mlog("\nFailed %u/%u test(s), %.02f%% successful.\n",
         total_fail, total_test, percent);
    mlog("\nThe .out and .err files in %s may give you some\n", result_dir);
    mlog("hint of what when wrong.\n");
    mlog("\nIf you want to report this error, please first read "
         "the documentation\n");
    mlog("at: http://www.mysql.com/doc/M/y/MySQL_test_suite.html\n");
  }
}

/******************************************************************************

  install_db()

  Install the a database.

******************************************************************************/

void install_db(char *datadir)
{
  arg_list_t al;
  int err;
  char input[FN_REFLEN];
  char output[FN_REFLEN];
  char error[FN_REFLEN];

  /* input file */
#ifdef __NETWARE__
  snprintf(input, FN_REFLEN, "%s/bin/init_db.sql", base_dir);
#else
  snprintf(input, FN_REFLEN, "%s/mysql-test/init_db.sql", base_dir);
#endif
  snprintf(output, FN_REFLEN, "%s/install.out", datadir);
  snprintf(error, FN_REFLEN, "%s/install.err", datadir);

  if (create_system_files(datadir,input, TRUE))
    die("Unable to create init_db.sql.");
  /* args */
  init_args(&al);
  add_arg(&al, mysqld_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--bootstrap");
  add_arg(&al, "--skip-grant-tables");
  add_arg(&al, "--basedir=%s", base_dir);
  add_arg(&al, "--datadir=%s", datadir);
  add_arg(&al, "--skip-innodb");
  add_arg(&al, "--skip-ndbcluster");
  add_arg(&al, "--skip-bdb");
#ifndef __NETWARE__
  add_arg(&al, "--character-sets-dir=%s", char_dir);
  add_arg(&al, "--language=%s", lang_dir);
#endif
// added 
  add_arg(&al, "--default-character-set=latin1");
  add_arg(&al, "--innodb_data_file_path=ibdata1:50M");

  /* spawn */
  if ((err= spawn(mysqld_file, &al, TRUE, input, output, error, NULL)) != 0)
  {
    die("Unable to create database.");
  }

  /* free args */
  free_args(&al);
}

/******************************************************************************

  mysql_install_db()

  Install the test databases.

******************************************************************************/

void mysql_install_db()
{
  char temp[FN_REFLEN];

  /* var directory */
  snprintf(temp, FN_REFLEN, "%s/var", mysql_test_dir);

  /* create var directory */
#ifndef __WIN__
  mkdir(temp, S_IRWXU);
  /* create subdirectories */
  mlog("Creating test-suite folders...\n");
  snprintf(temp, FN_REFLEN, "%s/var/run", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, FN_REFLEN, "%s/var/tmp", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, FN_REFLEN, "%s/var/master-data", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, FN_REFLEN, "%s/var/master-data/mysql", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, FN_REFLEN, "%s/var/master-data/test", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  
  snprintf(temp, FN_REFLEN, "%s/var/slave-data", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, FN_REFLEN, "%s/var/slave-data/mysql", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, FN_REFLEN, "%s/var/slave-data/test", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  
  snprintf(temp, FN_REFLEN, "%s/var/slave1-data", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, FN_REFLEN, "%s/var/slave1-data/mysql", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, FN_REFLEN, "%s/var/slave1-data/test", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  
  snprintf(temp, FN_REFLEN, "%s/var/slave2-data", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, FN_REFLEN, "%s/var/slave2-data/mysql", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, FN_REFLEN, "%s/var/slave2-data/test", mysql_test_dir);
  mkdir(temp, S_IRWXU);
#else
  mkdir(temp);
  /* create subdirectories */
  mlog("Creating test-suite folders...\n");
  snprintf(temp, FN_REFLEN, "%s/var/run", mysql_test_dir);
  mkdir(temp);
  snprintf(temp, FN_REFLEN, "%s/var/tmp", mysql_test_dir);
  mkdir(temp);
  snprintf(temp, FN_REFLEN, "%s/var/master-data", mysql_test_dir);
  mkdir(temp);
  snprintf(temp, FN_REFLEN, "%s/var/master-data/mysql", mysql_test_dir);
  mkdir(temp);
  snprintf(temp, FN_REFLEN, "%s/var/master-data/test", mysql_test_dir);
  mkdir(temp);
  snprintf(temp, FN_REFLEN, "%s/var/slave-data", mysql_test_dir);
  mkdir(temp);
  snprintf(temp, FN_REFLEN, "%s/var/slave-data/mysql", mysql_test_dir);
  mkdir(temp);
  snprintf(temp, FN_REFLEN, "%s/var/slave-data/test", mysql_test_dir);
  mkdir(temp);
#endif

  /* install databases */
  mlog("Creating test databases for master... \n");
  install_db(master_dir);
  mlog("Creating test databases for slave... \n");
  install_db(slave_dir);
  install_db(slave1_dir);
  install_db(slave2_dir);
}

/******************************************************************************

  start_master()

  Start the master server.

******************************************************************************/

void start_master()
{
  arg_list_t al;
  int err;
  char master_out[FN_REFLEN];
  char master_err[FN_REFLEN];
  char temp2[FN_REFLEN];

  /* remove old berkeley db log files that can confuse the server */
  removef("%s/log.*", master_dir);

  /* remove stale binary logs */
  removef("%s/var/log/*-bin.*", mysql_test_dir);

  /* remove stale binary logs */
  removef("%s/var/log/*.index", mysql_test_dir);

  /* remove master.info file */
  removef("%s/master.info", master_dir);

  /* remove relay files */
  removef("%s/var/log/*relay*", mysql_test_dir);

  /* remove relay-log.info file */
  removef("%s/relay-log.info", master_dir);

  /* init script */
  if (master_init_script[0] != 0)
  {
#ifdef __NETWARE__
    /* TODO: use the scripts */
    if (strinstr(master_init_script, "repair_part2-master.sh") != 0)
    {
      FILE *fp;

      /* create an empty index file */
      snprintf(temp, FN_REFLEN, "%s/test/t1.MYI", master_dir);
      fp= fopen(temp, "wb+");

      fputs("1", fp);

      fclose(fp);
    }
#elif !defined(__WIN__)
    run_init_script(master_init_script);
#endif
  }

  /* redirection files */
  snprintf(master_out, FN_REFLEN, "%s/var/run/master%u.out",
           mysql_test_dir, restarts);
  snprintf(master_err, FN_REFLEN, "%s/var/run/master%u.err",
           mysql_test_dir, restarts);
#ifndef __WIN__
  snprintf(temp2,FN_REFLEN,"%s/var",mysql_test_dir);
  mkdir(temp2,S_IRWXU);
  snprintf(temp2,FN_REFLEN,"%s/var/log",mysql_test_dir);
  mkdir(temp2,S_IRWXU);
#else
  snprintf(temp2,FN_REFLEN,"%s/var",mysql_test_dir);
  mkdir(temp2);
  snprintf(temp2,FN_REFLEN,"%s/var/log",mysql_test_dir);
  mkdir(temp2);
#endif
  /* args */
  init_args(&al);
  add_arg(&al, "%s", mysqld_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--log-bin=%s/var/log/master-bin",mysql_test_dir);
  add_arg(&al, "--server-id=1");
  add_arg(&al, "--basedir=%s", base_dir);
  add_arg(&al, "--port=%u", master_port);
#if !defined(__NETWARE__) && !defined(__WIN__)
  add_arg(&al, "--socket=%s",master_socket);
#endif
  add_arg(&al, "--local-infile");
  add_arg(&al, "--core");
  add_arg(&al, "--datadir=%s", master_dir);
#ifndef __WIN__
  add_arg(&al, "--pid-file=%s", master_pid);
#endif
  add_arg(&al, "--character-sets-dir=%s", char_dir);
  add_arg(&al, "--tmpdir=%s", mysql_tmp_dir);
  add_arg(&al, "--language=%s", lang_dir);
 
  add_arg(&al, "--rpl-recovery-rank=1");
  add_arg(&al, "--init-rpl-role=master");
  add_arg(&al, "--default-character-set=latin1");
//  add_arg(&al, "--innodb_data_file_path=ibdata1:50M");
#ifdef DEBUG /* only for debug builds */
  add_arg(&al, "--debug");
#endif

  if (use_openssl)
  {
    add_arg(&al, "--ssl-ca=%s", ca_cert);
    add_arg(&al, "--ssl-cert=%s", server_cert);
    add_arg(&al, "--ssl-key=%s", server_key);
  }

  /* $MASTER_40_ARGS */
  add_arg(&al, "--rpl-recovery-rank=1");
  add_arg(&al, "--init-rpl-role=master");

  /* $SMALL_SERVER */
  add_arg(&al, "-O");
  add_arg(&al, "key_buffer_size=1M");
  add_arg(&al, "-O");
  add_arg(&al, "sort_buffer=256K");
  add_arg(&al, "-O");
  add_arg(&al, "max_heap_table_size=1M");

  /* $EXTRA_MASTER_OPT */
  if (master_opt[0] != 0)
  {
    char *p;

    p= (char *)str_tok(argument, master_opt, " \t");
    if (!strstr(master_opt, "timezone"))
    {
      while (p)
      {
        add_arg(&al, "%s", p);
        p= (char *)str_tok(argument, NULL, " \t");
      }
    }
  }

  /* remove the pid file if it exists */
#ifndef __WIN__
  remove(master_pid);
#endif

  /* spawn */
#ifdef __WIN__
  if ((err= spawn(mysqld_file, &al, FALSE, NULL,
                  master_out, master_err, &master_pid)) == 0)
#else
  if ((err= spawn(mysqld_file, &al, FALSE, NULL,
                  master_out, master_err, master_pid)) == 0)
#endif
  {
    sleep_until_file_exists(master_pid);

    if ((err= wait_for_server_start(bin_dir, mysqladmin_file, user, password,
                                    master_port, mysql_tmp_dir)) == 0)
    {
      master_running= TRUE;
    }
    else
    {
      log_error("The master server went down early.");
    }
  }
  else
  {
    log_error("Unable to start master server.");
  }

  /* free_args */
  free_args(&al);
}

/******************************************************************************

  start_slave()

  Start the slave server.

******************************************************************************/

void start_slave()
{
  arg_list_t al;
  int err;
  char slave_out[FN_REFLEN];
  char slave_err[FN_REFLEN];

  /* skip? */
  if (skip_slave) return;

  /* remove stale binary logs */
  removef("%s/*-bin.*", slave_dir);

  /* remove stale binary logs */
  removef("%s/*.index", slave_dir);

  /* remove master.info file */
  removef("%s/master.info", slave_dir);

  /* remove relay files */
  removef("%s/var/log/*relay*", mysql_test_dir);

  /* remove relay-log.info file */
  removef("%s/relay-log.info", slave_dir);

  /* init script */
  if (slave_init_script[0] != 0)
  {
#ifdef __NETWARE__
    /* TODO: use the scripts */
    if (strinstr(slave_init_script, "rpl000016-slave.sh") != 0)
    {
      /* create empty master.info file */
      snprintf(temp, FN_REFLEN, "%s/master.info", slave_dir);
      close(open(temp, O_WRONLY | O_CREAT,S_IRWXU|S_IRWXG|S_IRWXO));
    }
    else if (strinstr(slave_init_script, "rpl000017-slave.sh") != 0)
    {
      FILE *fp;

      /* create a master.info file */
      snprintf(temp, FN_REFLEN, "%s/master.info", slave_dir);
      fp= fopen(temp, "wb+");

      fputs("master-bin.000001\n", fp);
      fputs("4\n", fp);
      fputs("127.0.0.1\n", fp);
      fputs("replicate\n", fp);
      fputs("aaaaaaaaaaaaaaab\n", fp);
      fputs("9306\n", fp);
      fputs("1\n", fp);
      fputs("0\n", fp);

      fclose(fp);
    }
    else if (strinstr(slave_init_script, "rpl_rotate_logs-slave.sh") != 0)
    {
      /* create empty master.info file */
      snprintf(temp, FN_REFLEN, "%s/master.info", slave_dir);
      close(open(temp, O_WRONLY | O_CREAT,S_IRWXU|S_IRWXG|S_IRWXO));
    }
#elif !defined(__WIN__)
    run_init_script(slave_init_script);
#endif
  }

  /* redirection files */
  snprintf(slave_out, FN_REFLEN, "%s/var/run/slave%u.out",
           mysql_test_dir, restarts);
  snprintf(slave_err, FN_REFLEN, "%s/var/run/slave%u.err",
           mysql_test_dir, restarts);

  /* args */
  init_args(&al);
  add_arg(&al, "%s", mysqld_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--log-bin=slave-bin");
  add_arg(&al, "--relay_log=slave-relay-bin");
  add_arg(&al, "--basedir=%s", base_dir);
#if !defined(__NETWARE__) && !defined(__WIN__)
  add_arg(&al, "--socket=%s",slave_socket);
#endif
  add_arg(&al, "--port=%u", slave_port);
  add_arg(&al, "--datadir=%s", slave_dir);
#ifndef __WIN__
  add_arg(&al, "--pid-file=%s", slave_pid);
#endif
  add_arg(&al, "--character-sets-dir=%s", char_dir);
  add_arg(&al, "--core");
  add_arg(&al, "--tmpdir=%s", mysql_tmp_dir);
  add_arg(&al, "--language=%s", lang_dir);

  add_arg(&al, "--exit-info=256");
  add_arg(&al, "--log-slave-updates");
  add_arg(&al, "--init-rpl-role=slave");
  add_arg(&al, "--skip-innodb");
  add_arg(&al, "--skip-slave-start");
  add_arg(&al, "--slave-load-tmpdir=../../var/tmp");

  add_arg(&al, "--report-user=%s", user);
  add_arg(&al, "--report-host=127.0.0.1");
  add_arg(&al, "--report-port=%u", slave_port);

  add_arg(&al, "--master-retry-count=10");
  add_arg(&al, "-O");
  add_arg(&al, "slave_net_timeout=10");
  add_arg(&al, "--log-slave-updates");
  add_arg(&al, "--log=%s/var/log/slave.log", mysql_test_dir);
  add_arg(&al, "--default-character-set=latin1");
  add_arg(&al, "--skip-ndbcluster");
  
#ifdef DEBUG /* only for debug builds */
  add_arg(&al, "--debug");
#endif      
	           
  if (use_openssl)
  {
    add_arg(&al, "--ssl-ca=%s", ca_cert);
    add_arg(&al, "--ssl-cert=%s", server_cert);
    add_arg(&al, "--ssl-key=%s", server_key);
  }

  /* slave master info */
  if (slave_master_info[0] != 0)
  {
    char *p;

    p= (char *)str_tok(argument, slave_master_info, " \t");

    while (p)
    {
      add_arg(&al, "%s", p);
      p= (char *)str_tok(argument, NULL, " \t");
    }
  }
  else
  {
    add_arg(&al, "--master-user=%s", user);
    add_arg(&al, "--master-password=%s", password);
    add_arg(&al, "--master-host=127.0.0.1");
    add_arg(&al, "--master-port=%u", master_port);
    add_arg(&al, "--master-connect-retry=1");
    add_arg(&al, "--server-id=2");
    add_arg(&al, "--rpl-recovery-rank=2");
  }

  /* small server */
  add_arg(&al, "-O");
  add_arg(&al, "key_buffer_size=1M");
  add_arg(&al, "-O");
  add_arg(&al, "sort_buffer=256K");
  add_arg(&al, "-O");
  add_arg(&al, "max_heap_table_size=1M");


  /* opt args */
  if (slave_opt[0] != 0)
  {
    char *p;

    p= (char *)str_tok(argument, slave_opt, " \t");

    while (p)
    {
      add_arg(&al, "%s", p);
      p= (char *)str_tok(argument, NULL, " \t");
    } 
  }

  /* remove the pid file if it exists */
#ifndef __WIN__
  remove(slave_pid);
#endif
  /* spawn */
#ifdef __WIN__
  if ((err= spawn(mysqld_file, &al, FALSE, NULL,
                  slave_out, slave_err, &slave_pid)) == 0)
#else
  if ((err= spawn(mysqld_file, &al, FALSE, NULL,
                  slave_out, slave_err, slave_pid)) == 0)
#endif
  {
    sleep_until_file_exists(slave_pid);

    if ((err= wait_for_server_start(bin_dir, mysqladmin_file, user, password,
                                    slave_port, mysql_tmp_dir)) == 0)
    {
      slave_running= TRUE;
    }
    else
    {
      log_error("The slave server went down early.");
    }
  }
  else
  {
    log_error("Unable to start slave server.");
  }

  /* free args */
  free_args(&al);
}

/******************************************************************************

  mysql_start()

  Start the mysql servers.

******************************************************************************/

void mysql_start()
{


  printf("loading master...\r");
  start_master();

  printf("loading slave...\r");
  start_slave();

  /* activate the test screen */
#ifdef __NETWARE__
  ActivateScreen(getscreenhandle());
#endif
}

/******************************************************************************

  stop_slave()

  Stop the slave server.

******************************************************************************/

void stop_slave()
{
  int err;

  /* running? */
  if (!slave_running) return;

  /* stop */
  if ((err= stop_server(bin_dir, mysqladmin_file, user, password,
                        slave_port, slave_pid, mysql_tmp_dir)) == 0)
  {
    slave_running= FALSE;
  }
  else
  {
    log_error("Unable to stop slave server.");
  }
}

/******************************************************************************

  stop_master()

  Stop the master server.

******************************************************************************/

void stop_master()
{
  int err;

  /* running? */
  if (!master_running) return;

  if ((err= stop_server(bin_dir, mysqladmin_file, user, password,
                        master_port, master_pid, mysql_tmp_dir)) == 0)
  {
    master_running= FALSE;
  }
  else
  {
    log_error("Unable to stop master server.");
  }
}

/******************************************************************************

  mysql_stop()

  Stop the mysql servers.

******************************************************************************/

void mysql_stop()
{

  stop_master();

  stop_slave();

  /* activate the test screen */
#ifdef __NETWARE__
  ActivateScreen(getscreenhandle());
#endif
}

/******************************************************************************

  mysql_restart()

  Restart the mysql servers.

******************************************************************************/

void mysql_restart()
{
/*  log_info("Restarting the MySQL server(s): %u", ++restarts); */

  mysql_stop();

  mlog(DASH);
  sleep(1);

  mysql_start();
}

/******************************************************************************

  read_option()

  Read the option file.

******************************************************************************/

int read_option(char *opt_file, char *opt)
{
  int fd, err;
  char *p;
  char buf[FN_REFLEN];

  /* copy current option */
  strncpy(buf, opt, FN_REFLEN);

  /* open options file */
  fd= open(opt_file, O_RDONLY);
  err= read(fd, opt, FN_REFLEN);
  close(fd);

  if (err > 0)
  {
    /* terminate string */
    if ((p= strchr(opt, '\n')) != NULL)
    {
      *p= 0;

      /* check for a '\r' */
      if ((p= strchr(opt, '\r')) != NULL)
      {
        *p= 0;
      }
    }
    else
    {
      opt[err]= 0;
    }

    /* check for $MYSQL_TEST_DIR */
    if ((p= strstr(opt, "$MYSQL_TEST_DIR")) != NULL)
    {
      char temp[FN_REFLEN];

      *p= 0;

      strcpy(temp, p + strlen("$MYSQL_TEST_DIR"));
      strcat(opt, mysql_test_dir);
      strcat(opt, temp);
    }
    /* Check for double backslash and replace it with single bakslash */
    if ((p= strstr(opt, "\\\\")) != NULL)
    {
      /* bmove is guranteed to work byte by byte */
      bmove(p, p+1, strlen(p)+1);
    }
  }
  else
  {
    /* clear option */
    *opt= 0;
  }

  /* compare current option with previous */
  return strcmp(opt, buf);
}

/******************************************************************************

  run_test()

  Run the given test case.

******************************************************************************/

void run_test(char *test)
{
  char temp[FN_REFLEN];
  const char *rstr;
  int skip= FALSE, ignore=FALSE;
  int restart= FALSE;
  int flag= FALSE;
  struct stat info;

  /* skip tests in the skip list */
  snprintf(temp, FN_REFLEN, " %s ", test);
  skip= (strinstr(skip_test, temp) != 0);
  if (skip == FALSE)
    ignore= (strinstr(ignore_test, temp) != 0);

  snprintf(master_init_script, FN_REFLEN, "%s/%s-master.sh", test_dir, test);
  snprintf(slave_init_script, FN_REFLEN, "%s/%s-slave.sh", test_dir, test);
#ifdef __WIN__
  if (! stat(master_init_script, &info))
      skip= TRUE;
  if (!stat(slave_init_script, &info))
      skip= TRUE;
#endif
  if (ignore)
  {
    /* show test */
    mlog("%-46s ", test);

    /* ignore */
    rstr= TEST_IGNORE;
    ++total_ignore;
  }
  else if (!skip)     /* skip test? */
  {
    char test_file[FN_REFLEN];
    char master_opt_file[FN_REFLEN];
    char slave_opt_file[FN_REFLEN];
    char slave_master_info_file[FN_REFLEN];
    char result_file[FN_REFLEN];
    char reject_file[FN_REFLEN];
    char out_file[FN_REFLEN];
    char err_file[FN_REFLEN];
    int err;
    arg_list_t al;
    /* skip slave? */
    flag= skip_slave;
    skip_slave= (strncmp(test, "rpl", 3) != 0);
    if (flag != skip_slave) restart= TRUE;

    /* create files */
    snprintf(master_opt_file, FN_REFLEN, "%s/%s-master.opt", test_dir, test);
    snprintf(slave_opt_file, FN_REFLEN, "%s/%s-slave.opt", test_dir, test);
    snprintf(slave_master_info_file, FN_REFLEN, "%s/%s.slave-mi",
             test_dir, test);
    snprintf(reject_file, FN_REFLEN, "%s/%s%s",
             result_dir, test, REJECT_SUFFIX);
    snprintf(out_file, FN_REFLEN, "%s/%s%s", result_dir, test, OUT_SUFFIX);
    snprintf(err_file, FN_REFLEN, "%s/%s%s", result_dir, test, ERR_SUFFIX);

    /* netware specific files */
    snprintf(test_file, FN_REFLEN, "%s/%s%s", test_dir, test, NW_TEST_SUFFIX);
    if (stat(test_file, &info))
    {
      snprintf(test_file, FN_REFLEN, "%s/%s%s", test_dir, test, TEST_SUFFIX);
      if (access(test_file,0))
      {
        printf("Invalid test name %s, %s file not found\n",test,test_file);
        return;
      }
    }

    snprintf(result_file, FN_REFLEN, "%s/%s%s",
             result_dir, test, NW_RESULT_SUFFIX);
    if (stat(result_file, &info))
    {
      snprintf(result_file, FN_REFLEN, "%s/%s%s",
               result_dir, test, RESULT_SUFFIX);
    }

    /* init scripts */
    if (stat(master_init_script, &info))
      master_init_script[0]= 0;
    else
      restart= TRUE;

    if (stat(slave_init_script, &info))
      slave_init_script[0]= 0;
    else
      restart= TRUE;

    /* read options */
    if (read_option(master_opt_file, master_opt)) restart= TRUE;
    if (read_option(slave_opt_file, slave_opt)) restart= TRUE;
    if (read_option(slave_master_info_file, slave_master_info)) restart= TRUE;

    /* cleanup previous run */
    remove(reject_file);
    remove(out_file);
    remove(err_file);

    /* start or restart? */
    if (!master_running) mysql_start();
      else if (restart) mysql_restart();

    /* show test */
    mlog("%-46s ", test);

    /* args */
    init_args(&al);
    add_arg(&al, "%s", mysqltest_file);
    add_arg(&al, "--no-defaults");
    add_arg(&al, "--port=%u", master_port);
#if !defined(__NETWARE__) && !defined(__WIN__)
    add_arg(&al, "--socket=%s", master_socket);
    add_arg(&al, "--tmpdir=%s", mysql_tmp_dir);
#endif
    add_arg(&al, "--database=%s", db);
    add_arg(&al, "--user=%s", user);
    add_arg(&al, "--password=%s", password);
    add_arg(&al, "--silent");
    add_arg(&al, "--basedir=%s/", mysql_test_dir);
    add_arg(&al, "--host=127.0.0.1");
    add_arg(&al, "--skip-safemalloc");
    add_arg(&al, "-v");
    add_arg(&al, "-R");
    add_arg(&al, "%s", result_file);
    
    
    if (use_openssl)
    {
      add_arg(&al, "--ssl-ca=%s", ca_cert);
      add_arg(&al, "--ssl-cert=%s", client_cert);
      add_arg(&al, "--ssl-key=%s", client_key);
    }

    /* spawn */
    err= spawn(mysqltest_file, &al, TRUE, test_file, out_file, err_file, NULL);
    /* free args */
    free_args(&al);

    remove_empty_file(out_file);
    remove_empty_file(err_file);

    if (err == 0)
    {
      /* pass */
      rstr= TEST_PASS;
      ++total_pass;

      /* increment total */
      ++total_test;
    }
    else if (err == 2)
    {
      /* skip */
      rstr= TEST_SKIP;
      ++total_skip;
    }
    else if (err == 1)
    {
      /* fail */
      rstr= TEST_FAIL;
      ++total_fail;

      /* increment total */
      ++total_test;
    }
    else
    {
      rstr= TEST_BAD;
    }
  }
  else /* early skips */
  {
    /* show test */
    mlog("%-46s ", test);

    /* skip */
    rstr= TEST_SKIP;
    ++total_skip;
  }

  /* result */
  mlog("%-14s\n", rstr);
}

/******************************************************************************

  vlog()

  Log the message.

******************************************************************************/

void vlog(const char *format, va_list ap)
{
  vfprintf(stdout, format, ap);
  fflush(stdout);

  if (log_fd)
  {
    vfprintf(log_fd, format, ap);
    fflush(log_fd);
  }
}

/******************************************************************************

  log()

  Log the message.

******************************************************************************/

void mlog(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);

  vlog(format, ap);

  va_end(ap);
}

/******************************************************************************

  log_info()

  Log the given information.

******************************************************************************/

void log_info(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);

  mlog("-- INFO : ");
  vlog(format, ap);
  mlog("\n");

  va_end(ap);
}

/******************************************************************************

  log_error()

  Log the given error.

******************************************************************************/

void log_error(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);

  mlog("-- ERROR: ");
  vlog(format, ap);
  mlog("\n");

  va_end(ap);
}

/******************************************************************************

  log_errno()

  Log the given error and errno.

******************************************************************************/

void log_errno(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);

  mlog("-- ERROR: (%003u) ", errno);
  vlog(format, ap);
  mlog("\n");

  va_end(ap);
}

/******************************************************************************

  die()

  Exit the application.

******************************************************************************/

void die(const char *msg)
{
  log_error(msg);
#ifdef __NETWARE__
  pressanykey();
#endif
  exit(-1);
}

/******************************************************************************

  setup()

  Setup the mysql test enviornment.

******************************************************************************/

void setup(char *file __attribute__((unused)))
{
  char temp[FN_REFLEN];
#if defined(__WIN__) || defined(__NETWARE__)  
  char file_path[FN_REFLEN*2];
#endif  
  char *p;
  int position;

  /* set the timezone for the timestamp test */
#ifdef __WIN__
  _putenv( "TZ=GMT-3" );
#else
  putenv((char *)"TZ=GMT-3");
#endif
  /* find base dir */
#ifdef __NETWARE__
  strcpy(temp, strlwr(file));
  while ((p= strchr(temp, '\\')) != NULL) *p= '/';
#else
  getcwd(temp, FN_REFLEN);
  position= strlen(temp);
  temp[position]= '/';
  temp[position+1]= 0;
#ifdef __WIN__
  while ((p= strchr(temp, '\\')) != NULL) *p= '/';
#endif
#endif

  if ((position= strinstr(temp, "/mysql-test/")) != 0)
  {
    p= temp + position - 1;
    *p= 0;
    strcpy(base_dir, temp);
  }

  log_info("Currect directory: %s",base_dir);

#ifdef __NETWARE__
  /* setup paths */
  snprintf(bin_dir, FN_REFLEN, "%s/bin", base_dir);
  snprintf(mysql_test_dir, FN_REFLEN, "%s/mysql-test", base_dir);
  snprintf(test_dir, FN_REFLEN, "%s/t", mysql_test_dir);
  snprintf(mysql_tmp_dir, FN_REFLEN, "%s/var/tmp", mysql_test_dir);
  snprintf(result_dir, FN_REFLEN, "%s/r", mysql_test_dir);
  snprintf(master_dir, FN_REFLEN, "%s/var/master-data", mysql_test_dir);
  snprintf(slave_dir, FN_REFLEN, "%s/var/slave-data", mysql_test_dir);
  snprintf(lang_dir, FN_REFLEN, "%s/share/english", base_dir);
  snprintf(char_dir, FN_REFLEN, "%s/share/charsets", base_dir);

#ifdef HAVE_OPENSSL
  use_openssl= TRUE;
#endif /* HAVE_OPENSSL */

  /* OpenSSL paths */
  snprintf(ca_cert, FN_REFLEN, "%s/SSL/cacert.pem", base_dir);
  snprintf(server_cert, FN_REFLEN, "%s/SSL/server-cert.pem", base_dir);
  snprintf(server_key, FN_REFLEN, "%s/SSL/server-key.pem", base_dir);
  snprintf(client_cert, FN_REFLEN, "%s/SSL/client-cert.pem", base_dir);
  snprintf(client_key, FN_REFLEN, "%s/SSL/client-key.pem", base_dir);

  /* setup files */
  snprintf(mysqld_file, FN_REFLEN, "%s/mysqld", bin_dir);
  snprintf(mysqltest_file, FN_REFLEN, "%s/mysqltest", bin_dir);
  snprintf(mysqladmin_file, FN_REFLEN, "%s/mysqladmin", bin_dir);
  snprintf(master_pid, FN_REFLEN, "%s/var/run/master.pid", mysql_test_dir);
  snprintf(slave_pid, FN_REFLEN, "%s/var/run/slave.pid", mysql_test_dir);
#elif __WIN__
  /* setup paths */
#ifdef _DEBUG
  snprintf(bin_dir, FN_REFLEN, "%s/client_debug", base_dir);
#else
  snprintf(bin_dir, FN_REFLEN, "%s/client_release", base_dir);
#endif
  snprintf(mysql_test_dir, FN_REFLEN, "%s/mysql-test", base_dir);
  snprintf(test_dir, FN_REFLEN, "%s/t", mysql_test_dir);
  snprintf(mysql_tmp_dir, FN_REFLEN, "%s/var/tmp", mysql_test_dir);
  snprintf(result_dir, FN_REFLEN, "%s/r", mysql_test_dir);
  snprintf(master_dir, FN_REFLEN, "%s/var/master-data", mysql_test_dir);
  snprintf(slave_dir, FN_REFLEN, "%s/var/slave-data", mysql_test_dir);
  snprintf(lang_dir, FN_REFLEN, "%s/share/english", base_dir);
  snprintf(char_dir, FN_REFLEN, "%s/share/charsets", base_dir);

#ifdef HAVE_OPENSSL
  use_openssl= TRUE;
#endif /* HAVE_OPENSSL */

  /* OpenSSL paths */
  snprintf(ca_cert, FN_REFLEN, "%s/SSL/cacert.pem", base_dir);
  snprintf(server_cert, FN_REFLEN, "%s/SSL/server-cert.pem", base_dir);
  snprintf(server_key, FN_REFLEN, "%s/SSL/server-key.pem", base_dir);
  snprintf(client_cert, FN_REFLEN, "%s/SSL/client-cert.pem", base_dir);
  snprintf(client_key, FN_REFLEN, "%s/SSL/client-key.pem", base_dir);

  /* setup files */
#ifdef _DEBUG 
  snprintf(mysqld_file, FN_REFLEN, "%s/mysqld-debug.exe", bin_dir);
#else
  snprintf(mysqld_file, FN_REFLEN, "%s/mysqld.exe", bin_dir);
#endif
  snprintf(mysqltest_file, FN_REFLEN, "%s/mysqltest.exe", bin_dir);
  snprintf(mysqladmin_file, FN_REFLEN, "%s/mysqladmin.exe", bin_dir);
#else
  /* setup paths */
  snprintf(bin_dir, FN_REFLEN, "%s/client", base_dir);
  snprintf(mysql_test_dir, FN_REFLEN, "%s/mysql-test", base_dir);
  snprintf(test_dir, FN_REFLEN, "%s/t", mysql_test_dir);
  snprintf(mysql_tmp_dir, FN_REFLEN, "%s/var/tmp", mysql_test_dir);
  snprintf(result_dir, FN_REFLEN, "%s/r", mysql_test_dir);
  snprintf(master_dir, FN_REFLEN, "%s/var/master-data", mysql_test_dir);
  snprintf(slave_dir, FN_REFLEN, "%s/var/slave-data", mysql_test_dir);
  snprintf(slave1_dir, FN_REFLEN, "%s/var/slave1-data", mysql_test_dir);
  snprintf(slave2_dir, FN_REFLEN, "%s/var/slave2-data", mysql_test_dir);
  snprintf(lang_dir, FN_REFLEN, "%s/sql/share/english", base_dir);
  snprintf(char_dir, FN_REFLEN, "%s/sql/share/charsets", base_dir);

#ifdef HAVE_OPENSSL
  use_openssl= TRUE;
#endif /* HAVE_OPENSSL */

  /* OpenSSL paths */
  snprintf(ca_cert, FN_REFLEN, "%s/SSL/cacert.pem", base_dir);
  snprintf(server_cert, FN_REFLEN, "%s/SSL/server-cert.pem", base_dir);
  snprintf(server_key, FN_REFLEN, "%s/SSL/server-key.pem", base_dir);
  snprintf(client_cert, FN_REFLEN, "%s/SSL/client-cert.pem", base_dir);
  snprintf(client_key, FN_REFLEN, "%s/SSL/client-key.pem", base_dir);

  /* setup files */
  snprintf(mysqld_file, FN_REFLEN, "%s/sql/mysqld", base_dir);
  snprintf(mysqltest_file, FN_REFLEN, "%s/mysqltest", bin_dir);
  snprintf(mysqladmin_file, FN_REFLEN, "%s/mysqladmin", bin_dir);
  snprintf(master_pid, FN_REFLEN, "%s/var/run/master.pid", mysql_test_dir);
  snprintf(slave_pid, FN_REFLEN, "%s/var/run/slave.pid", mysql_test_dir);

  snprintf(master_socket,FN_REFLEN, "%s/var/tmp/master.sock", mysql_test_dir);
  snprintf(slave_socket,FN_REFLEN, "%s/var/tmp/slave.sock", mysql_test_dir);

#endif
  /* create log file */
  snprintf(temp, FN_REFLEN, "%s/mysql-test-run.log", mysql_test_dir);
  if ((log_fd= fopen(temp, "w+")) == NULL)
  {
    log_errno("Unable to create log file.");
  }

  /* prepare skip test list */
  while ((p= strchr(skip_test, ',')) != NULL) *p= ' ';
  strcpy(temp, strlwr(skip_test));
  snprintf(skip_test, FN_REFLEN, " %s ", temp);

  /* environment */
#ifdef __NETWARE__
  setenv("MYSQL_TEST_DIR", mysql_test_dir, 1);
  snprintf(file_path, FN_REFLEN*2,
           "%s/client/mysqldump --no-defaults -u root --port=%u",
           bin_dir, master_port);
  setenv("MYSQL_DUMP", file_path, 1);
  snprintf(file_path, FN_REFLEN*2,
           "%s/client/mysqlbinlog --no-defaults --local-load=%s",
           bin_dir, mysql_tmp_dir);
  setenv("MYSQL_BINLOG", file_path, 1);
#elif __WIN__
  snprintf(file_path,FN_REFLEN,"MYSQL_TEST_DIR=%s",mysql_test_dir);
  _putenv(file_path);
  snprintf(file_path, FN_REFLEN*2,
           "MYSQL_DUMP=%s/mysqldump.exe --no-defaults -uroot --port=%u",
           bin_dir, master_port);
  _putenv(file_path);
  snprintf(file_path, FN_REFLEN*2,
          "MYSQL_BINLOG=%s/mysqlbinlog.exe --no-defaults --local-load=%s",
           bin_dir, mysql_tmp_dir);
  _putenv(file_path);
    
  snprintf(file_path, FN_REFLEN*2,
          "TESTS_BINDIR=%s/tests", base_dir);
  _putenv(file_path);

  snprintf(file_path, FN_REFLEN*2,
           "CHARSETSDIR=%s/sql/share/charsets", base_dir);
  _putenv(file_path);

  snprintf(file_path, FN_REFLEN*2,
           "MYSQL=%s/mysql --port=%u ", 
           bin_dir, master_port);
  _putenv(file_path);
    
  snprintf(file_path, FN_REFLEN*2,
           "MYSQL_FIX_SYSTEM_TABLES=%s/scripts/mysql_fix_privilege_tables --no-defaults "
           "--host=localhost --port=%u "
           "--basedir=%s --bindir=%s --verbose",
           base_dir,master_port, base_dir, bin_dir);
  _putenv(file_path);
     
  snprintf(file_path, FN_REFLEN*2,
           "NDB_TOOLS_DIR=%s/ndb/tools", base_dir);
  _putenv(file_path);
    
  snprintf(file_path, FN_REFLEN*2,
           "CLIENT_BINDIR=%s", bin_dir);
  _putenv(file_path);

  snprintf(file_path, FN_REFLEN*2,
             "MYSQL_CLIENT_TEST=%s/tests/mysql_client_test --no-defaults --testcase "
	     "--user=root --port=%u --silent", 
	     base_dir, master_port);
  _putenv(file_path);

#else
  {
    static char env_MYSQL_TEST_DIR[FN_REFLEN*2];
    static char env_MYSQL_DUMP[FN_REFLEN*2];
    static char env_MYSQL_BINLOG[FN_REFLEN*2];
    static char env_MASTER_MYSOCK[FN_REFLEN*2];
    static char env_TESTS_BINDIR[FN_REFLEN*2];
    static char env_CHARSETSDIR[FN_REFLEN*2];
    static char env_MYSQL[FN_REFLEN*2];
    static char env_MYSQL_FIX_SYSTEM_TABLES[FN_REFLEN*2];
    static char env_CLIENT_BINDIR[FN_REFLEN*2];
    static char env_MYSQL_CLIENT_TEST[FN_REFLEN*2];
    static char env_NDB_TOOLS_DIR[FN_REFLEN*2];
    static char env_NDB_MGM[FN_REFLEN*2];
    static char env_NDB_BACKUP_DIR[FN_REFLEN*2];
    static char env_NDB_TOOLS_OUTPUT[FN_REFLEN*2];
    
    snprintf(env_MYSQL_TEST_DIR,FN_REFLEN*2,
             "MYSQL_TEST_DIR=%s",mysql_test_dir);
    putenv(env_MYSQL_TEST_DIR);
    
    snprintf(env_MYSQL_DUMP, FN_REFLEN*2,"MYSQL_DUMP=%s/mysqldump --no-defaults "
             "-uroot --port=%u --socket=%s ", 
             bin_dir, master_port, master_socket);    
    putenv(env_MYSQL_DUMP);
    
    snprintf(env_MYSQL_BINLOG, FN_REFLEN*2,
             "MYSQL_BINLOG=%s/mysqlbinlog --no-defaults --local-load=%s -uroot ",
             bin_dir, mysql_tmp_dir);
    putenv(env_MYSQL_BINLOG);
    
    snprintf(env_MASTER_MYSOCK, FN_REFLEN*2,
             "MASTER_MYSOCK=%s", master_socket);
    putenv(env_MASTER_MYSOCK);

    snprintf(env_TESTS_BINDIR, FN_REFLEN*2,
             "TESTS_BINDIR=%s/tests", base_dir);
    putenv(env_TESTS_BINDIR);

    snprintf(env_CHARSETSDIR, FN_REFLEN*2,
             "CHARSETSDIR=%s/sql/share/charsets", base_dir);
    putenv(env_CHARSETSDIR);

    snprintf(env_MYSQL, FN_REFLEN*2,
             "MYSQL=%s/mysql --port=%u --socket=%s -uroot ", 
	     bin_dir, master_port, master_socket);
    putenv(env_MYSQL);
    
    snprintf(env_MYSQL_FIX_SYSTEM_TABLES, FN_REFLEN*2,
             "MYSQL_FIX_SYSTEM_TABLES=%s/scripts/mysql_fix_privilege_tables --no-defaults "
	     "--host=localhost --port=%u --socket=%s "
	     "--basedir=%s --bindir=%s --verbose -uroot ",
	     base_dir,master_port, master_socket, base_dir, bin_dir);
    putenv(env_MYSQL_FIX_SYSTEM_TABLES);
     
    
    snprintf(env_CLIENT_BINDIR, FN_REFLEN*2,
             "CLIENT_BINDIR=%s", bin_dir);
    putenv(env_CLIENT_BINDIR);

    snprintf(env_MYSQL_CLIENT_TEST, FN_REFLEN*2,
             "MYSQL_CLIENT_TEST=%s/tests/mysql_client_test --no-defaults --testcase "
	     "--user=root --socket=%s --port=%u --silent", 
	     base_dir, master_socket, master_port);
    putenv(env_MYSQL_CLIENT_TEST);
    
    // NDB
    
    snprintf(env_NDB_TOOLS_DIR, FN_REFLEN*2,
             "NDB_TOOLS_DIR=%s/ndb/tools", base_dir);
    putenv(env_NDB_TOOLS_DIR);
  
    snprintf(env_NDB_MGM, FN_REFLEN*2,
             "NDB_MGM=%s/ndb/src/mgmclient/ndb_mgm", base_dir);
    putenv(env_NDB_MGM);
  
    //NDBCLUSTER_PORT=9350
    snprintf(env_NDB_BACKUP_DIR, FN_REFLEN*2,
             "NDB_BACKUP_DIR=%s/var/ndbcluster-%i", mysql_test_dir, 9350);
    putenv(env_NDB_BACKUP_DIR);
  
    snprintf(env_NDB_TOOLS_OUTPUT, FN_REFLEN*2,
             "NDB_TOOLS_OUTPUT=%s/var/log/ndb_tools.log", mysql_test_dir);
    putenv(env_NDB_TOOLS_OUTPUT);

    putenv((char *)"NDB_STATUS_OK=1");
  
// NDB_MGM="$BASEDIR/ndb/src/mgmclient/ndb_mgm"
// NDB_BACKUP_DIR=$MYSQL_TEST_DIR/var/ndbcluster-$NDBCLUSTER_PORT
// NDB_TOOLS_OUTPUT=$MYSQL_TEST_DIR/var/log/ndb_tools.log
  }
  
#endif

#ifndef __WIN__
  putenv((char *)"MASTER_MYPORT=9306");
  putenv((char *)"SLAVE_MYPORT=9307");
  putenv((char *)"MYSQL_TCP_PORT=3306");

#else
  _putenv("MASTER_MYPORT=9306");
  _putenv("SLAVE_MYPORT=9307");
  _putenv("MYSQL_TCP_PORT=3306");
#endif

}

/*
  Compare names of testes for right order
*/
int compare( const void *arg1, const void *arg2 )
{
  return sting_compare_func( * ( char** ) arg1, * ( char** ) arg2 );
}



/******************************************************************************

  main()

******************************************************************************/

int main(int argc, char **argv)
{
  int is_ignore_list= 0;
  char **names= 0;
  char **testes= 0;
  int name_index;
  int index;
  char var_dir[FN_REFLEN];
  /* setup */
  setup(argv[0]);
  
  /* delete all file in var */
  snprintf(var_dir,FN_REFLEN,"%s/var",mysql_test_dir);
  del_tree(var_dir);

  /*
    The --ignore option is comma saperated list of test cases to skip and
    should be very first command line option to the test suite.

    The usage is now:
    mysql_test_run --ignore=test1,test2 test3 test4
    where test1 and test2 are test cases to ignore
    and test3 and test4 are test cases to run.
  */
  if (argc >= 2 && !strnicmp(argv[1], "--ignore=", sizeof("--ignore=")-1))
  {
    char *temp, *token;
    temp= strdup(strchr(argv[1],'=') + 1);
    for (token=str_tok(argument, temp, ","); token != NULL; 
         token=str_tok(argument, NULL, ","))
    {
      if (strlen(ignore_test) + strlen(token) + 2 <= FN_REFLEN-1)
        sprintf(ignore_test+strlen(ignore_test), " %s ", token);
      else
      {
        free(temp);
        die("ignore list too long.");
      }
    }
    free(temp);
    is_ignore_list= 1;
  }
  /* header */
#ifndef __WIN__
  mlog("MySQL Server %s, for %s (%s)\n\n", VERSION, SYSTEM_TYPE, MACHINE_TYPE);
#else
  mlog("MySQL Server ---, for %s (%s)\n\n", SYSTEM_TYPE, MACHINE_TYPE);
#endif

  mlog("Initializing Tests...\n");

  /* install test databases */
  mysql_install_db();
  
  mlog("Starting Tests...\n");

  mlog("\n");
  mlog(HEADER);
  mlog(DASH);

  if ( argc > 1 + is_ignore_list )
  {
    int i;

    /* single test */
    single_test= TRUE;

    for (i= 1 + is_ignore_list; i < argc; i++)
    {
      /* run given test */
      run_test(argv[i]);
    }
  }
  else
  {
    /* run all tests */
    testes= malloc(MAX_COUNT_TESTES*sizeof(void*));
    if (!testes)
      die("can not allcate memory for sorting");
    names= testes;
    name_index= 0;
#ifndef __WIN__
    struct dirent *entry;
    DIR *parent;
    char test[FN_LEN];
    int position;

    /* FIXME are we sure the list is sorted if using readdir()? */
    if ((parent= opendir(test_dir)) == NULL)    /* Not thread safe */
      die("Unable to open tests directory.");
    else
    {
      while ((entry= readdir(parent)) != NULL)  /* Not thread safe */
      {
        strcpy(test, strlwr(entry->d_name));
        /* find the test suffix */
        if ((position= strinstr(test, TEST_SUFFIX)) != 0)
        {
	  if (name_index < MAX_COUNT_TESTES)
	  {
            /* null terminate at the suffix */
            *(test + position - 1)= '\0';
            /* insert test */
            *names= malloc(FN_REFLEN);
            strcpy(*names,test);
            names++;
            name_index++;
          }
	  else
	    die("can not sort files, array is overloaded");
        }
      }
      closedir(parent);
    }
#else
    {
      struct _finddata_t dir;
      int* handle;
      char test[FN_LEN];
      char mask[FN_REFLEN];
      char *p;
      int position;

      /* single test */
      single_test= FALSE;

      snprintf(mask,FN_REFLEN,"%s/*.test",test_dir);

      if ((handle=_findfirst(mask,&dir)) == -1L)
      {
        die("Unable to open tests directory.");
      }


      do
      {
        if (!(dir.attrib & _A_SUBDIR))
        {
          strcpy(test, strlwr(dir.name));

          /* find the test suffix */
          if ((position= strinstr(test, TEST_SUFFIX)) != 0)
          {
            if (name_index < MAX_COUNT_TESTES)
	    {
              /* null terminate at the suffix */
              *(test + position - 1)= '\0';
              /* insert test */
              *names= malloc(FN_REFLEN);
              strcpy(*names,test);
              names++;
              name_index++;
	    }
	    else
	      die("can not sort files, array is overloaded");
          }
        }
      }while (_findnext(handle,&dir) == 0);

      _findclose(handle);
    }
#endif
    qsort( (void *)testes, name_index, sizeof( char * ), compare );

    for (index= 0; index < name_index; index++)
    {
      run_test(testes[index]);
      free(testes[index]);
    }

    free(testes);
  }

  /* stop server */
  mysql_stop();

  mlog(DASH);
  mlog("\n");

  mlog("Ending Tests...\n");

  /* report stats */
  report_stats();

  /* close log */
  if (log_fd) fclose(log_fd);

  /* keep results up */
#ifdef __NETWARE__
  pressanykey();
#endif
  return 0;
}


/*
 Synopsis:
  This function breaks the string into a sequence of tokens. The difference
  between this function and strtok is that it respects the quoted string i.e.
  it skips  any delimiter character within the quoted part of the string.
  It return tokens by eliminating quote character. It modifies the input string
  passed. It will work with whitespace delimeter but may not work properly with
  other delimeter. If the delimeter will contain any quote character, then
  function will not tokenize and will return null string.
  e.g. if input string is
     --init-slave="set global max_connections=500" --skip-external-locking
  then the output will two string i.e.
     --init-slave=set global max_connections=500
     --skip-external-locking

Arguments:
  string:  input string
  delim:   set of delimiter character
Output:
  return the null terminated token of NULL.
*/
char *str_tok(char* dest, char *string, const char *delim)
{
  char *token;            
  char *ptr_end_token= NULL;
  char *ptr_quote= NULL;
  char *ptr_token= NULL;
  int count_quotes= 0;

  *dest = '\0';
  if (strchr(delim,'\'') || strchr(delim,'\"'))
    return NULL;

  token= (char*)strtok(string, delim);
  if (token) 
  {
    /* double quote is found */
    if (strchr(token,'\"'))
    {
      do
      {
        if (count_quotes & 1)
        {
	  if (*dest == '\0')
            sprintf(dest,"%s", ptr_token);
	  else
            sprintf(dest,"%s %s", dest, ptr_token);
          ptr_token= (char*)strtok(NULL, delim);
          if (!ptr_token)
            break;
        }
        else
        {
          ptr_token= token;
        }
        if (ptr_quote = strchr(ptr_token,'\"'))
        {
          ptr_end_token= ptr_token + strlen(ptr_token);
          do
          {
#ifndef __WIN__
            bmove(ptr_quote, ptr_quote+1, ptr_end_token - ptr_quote);
#endif
            count_quotes++;
          } while (ptr_quote != NULL && (ptr_quote = strchr(ptr_quote+1,'\"')));
        }
      /* there are unpair quotes we have to search next quote*/
      } while (count_quotes & 1);
      if (ptr_token != NULL)
      {
        if (*dest == '\0')
          sprintf(dest,"%s", ptr_token);
	else
          sprintf(dest,"%s %s",dest,ptr_token);
      }
    }
    else
    {
      sprintf(dest,"%s",token);
    }
  }
  return token ? dest : NULL;
}

#ifndef __WIN__
/*
 Synopsis:
  This function run scripts files on Linux and Netware

Arguments:
  script_name:  name of script file

Output:
  nothing
*/

void run_init_script(const char *script_name)
{
  arg_list_t al;
  int err;

  /* args */
  init_args(&al);
  add_arg(&al, sh_file);
  add_arg(&al, script_name);

  /* spawn */
  if ((err= spawn(sh_file, &al, TRUE, NULL, NULL, NULL, NULL)) != 0)
  {
    die("Unable to run script.");
  }

  /* free args */
  free_args(&al);
}
#endif
