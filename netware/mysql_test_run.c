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
#include <dirent.h>
#include <string.h>
#include <screen.h>
#include <nks/vm.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "my_config.h"
#include "my_manage.h"

/******************************************************************************

  macros
  
******************************************************************************/

#define HEADER  "TEST                                           ELAPSED      RESULT      \n"
#define DASH    "------------------------------------------------------------------------\n"

#define NW_TEST_SUFFIX    ".nw-test"
#define NW_RESULT_SUFFIX  ".nw-result"
#define TEST_SUFFIX		    ".test"
#define RESULT_SUFFIX	    ".result"
#define REJECT_SUFFIX	    ".reject"
#define OUT_SUFFIX		    ".out"
#define ERR_SUFFIX		    ".err"

#define TEST_PASS		"[ pass ]"
#define TEST_SKIP		"[ skip ]"
#define TEST_FAIL		"[ fail ]"
#define TEST_BAD		"[ bad  ]"

/******************************************************************************

  global variables
  
******************************************************************************/

char base_dir[PATH_MAX]   = "sys:/mysql";
char db[PATH_MAX]         = "test";
char user[PATH_MAX]       = "root";
char password[PATH_MAX]   = "";

int master_port           = 9306;
int slave_port            = 9307;

// comma delimited list of tests to skip or empty string
char skip_test[PATH_MAX]  = "";

char bin_dir[PATH_MAX];
char mysql_test_dir[PATH_MAX];
char test_dir[PATH_MAX];
char mysql_tmp_dir[PATH_MAX];
char result_dir[PATH_MAX];
char master_dir[PATH_MAX];
char slave_dir[PATH_MAX];
char lang_dir[PATH_MAX];
char char_dir[PATH_MAX];

char mysqladmin_file[PATH_MAX];
char mysqld_file[PATH_MAX];
char mysqltest_file[PATH_MAX];
char master_pid[PATH_MAX];
char slave_pid[PATH_MAX];

char master_opt[PATH_MAX] = "";
char slave_opt[PATH_MAX]  = "";

char slave_master_info[PATH_MAX]  = "";

char master_init_script[PATH_MAX]  = "";
char slave_init_script[PATH_MAX]  = "";

int total_skip    = 0;
int total_pass    = 0;
int total_fail    = 0;
int total_test    = 0;

double total_time = 0;

int master_running  = FALSE;
int slave_running   = FALSE;
int skip_slave      = TRUE;
int single_test     = TRUE;

int restarts  = 0;

FILE *log_fd  = NULL;

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
void vlog(char *, va_list);
void log(char *, ...);
void log_info(char *, ...);
void log_error(char *, ...);
void log_errno(char *, ...);
void die(char *);

/******************************************************************************

  report_stats()
  
  Report the gathered statistics.

******************************************************************************/
void report_stats()
{
  if (total_fail == 0)
  {
    log("\nAll %d test(s) were successful.\n", total_test);
  }
  else
  {
    double percent = ((double)total_pass / total_test) * 100;
    
    log("\nFailed %u/%u test(s), %.02f%% successful.\n",
      total_fail, total_test, percent);
		log("\nThe .out and .err files in %s may give you some\n", result_dir);
		log("hint of what when wrong.\n");
		log("\nIf you want to report this error, please first read the documentation\n");
		log("at: http://www.mysql.com/doc/M/y/MySQL_test_suite.html\n");
  }

  log("\n%.02f total minutes elapsed in the test cases\n\n", total_time / 60);
}

/******************************************************************************

  install_db()
  
  Install the a database.

******************************************************************************/
void install_db(char *datadir)
{
  arg_list_t al;
  int err, i;
  char input[PATH_MAX];
  char output[PATH_MAX];
  char error[PATH_MAX];
  
  // input file
  snprintf(input, PATH_MAX, "%s/bin/init_db.sql", base_dir);
  snprintf(output, PATH_MAX, "%s/install.out", datadir);
  snprintf(error, PATH_MAX, "%s/install.err", datadir);
  
  // args
  init_args(&al);
  add_arg(&al, mysqld_file);
  add_arg(&al, "--bootstrap");
  add_arg(&al, "--skip-grant-tables");
  add_arg(&al, "--basedir=%s", base_dir);
  add_arg(&al, "--datadir=%s", datadir);
  add_arg(&al, "--skip-innodb");
  add_arg(&al, "--skip-bdb");
  
  // spawn
  if ((err = spawn(mysqld_file, &al, TRUE, input, output, error)) != 0)
  {
    die("Unable to create database.");
  }
  
  // free args
  free_args(&al);
}

/******************************************************************************

  mysql_install_db()
  
  Install the test databases.

******************************************************************************/
void mysql_install_db()
{
  char temp[PATH_MAX];
  
  // var directory
  snprintf(temp, PATH_MAX, "%s/var", mysql_test_dir);
  
  // clean up old direcotry
  del_tree(temp);
  
  // create var directory
  mkdir(temp, S_IRWXU);
  
  // create subdirectories
  snprintf(temp, PATH_MAX, "%s/var/run", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/tmp", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/master-data", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/master-data/mysql", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/master-data/test", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/slave-data", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/slave-data/mysql", mysql_test_dir);
  mkdir(temp, S_IRWXU);
  snprintf(temp, PATH_MAX, "%s/var/slave-data/test", mysql_test_dir);
  mkdir(temp, S_IRWXU);

  // install databases
  install_db(master_dir);
  install_db(slave_dir);
}

/******************************************************************************

  start_master()
  
  Start the master server.

******************************************************************************/
void start_master()
{
  arg_list_t al;
  int err, i;
  char master_out[PATH_MAX];
  char master_err[PATH_MAX];
  char temp[PATH_MAX];
  
  // remove old berkeley db log files that can confuse the server
  removef("%s/log.*", master_dir);
  
  // remove stale binary logs
  removef("%s/*-bin.*", master_dir);

  // remove stale binary logs
  removef("%s/*.index", master_dir);

  // remove master.info file
  removef("%s/master.info", master_dir);

  // remove relay files
  removef("%s/var/log/*relay*", mysql_test_dir);

  // remove relay-log.info file
  removef("%s/relay-log.info", master_dir);
  
  // init script
  if (master_init_script[0] != NULL)
  {
    // run_init_script(master_init_script);
    
    // TODO: use the scripts
    if (strindex(master_init_script, "repair_part2-master.sh") != NULL)
    {
      FILE *fp;
      
      // create an empty index file
      snprintf(temp, PATH_MAX, "%s/test/t1.MYI", master_dir);
      fp = fopen(temp, "wb+");
      
      fputs("1", fp);

      fclose(fp);
    }
  }

  // redirection files
  snprintf(master_out, PATH_MAX, "%s/var/run/master%u.out",
           mysql_test_dir, restarts);
  snprintf(master_err, PATH_MAX, "%s/var/run/master%u.err",
           mysql_test_dir, restarts);
  
  // args
  init_args(&al);
  add_arg(&al, "%s", mysqld_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--log-bin=master-bin");
  add_arg(&al, "--server-id=1");
  add_arg(&al, "--basedir=%s", base_dir);
  add_arg(&al, "--port=%u", master_port);
  add_arg(&al, "--local-infile");
  add_arg(&al, "--core");
  add_arg(&al, "--datadir=%s", master_dir);
  add_arg(&al, "--pid-file=%s", master_pid);
  add_arg(&al, "--character-sets-dir=%s", char_dir);
  add_arg(&al, "--tmpdir=%s", mysql_tmp_dir);
  add_arg(&al, "--language=%s", lang_dir);
  
  // $MASTER_40_ARGS
  add_arg(&al, "--rpl-recovery-rank=1");
  add_arg(&al, "--init-rpl-role=master");
  
  // $SMALL_SERVER
  add_arg(&al, "-O");
  add_arg(&al, "key_buffer_size=1M");
  add_arg(&al, "-O");
  add_arg(&al, "sort_buffer=256K");
  add_arg(&al, "-O");
  add_arg(&al, "max_heap_table_size=1M");

  // $EXTRA_MASTER_OPT
  if (master_opt[0] != NULL)
  {
    char *p;

    p = (char *)strtok(master_opt, " \t");

    while(p)
    {
      add_arg(&al, "%s", p);
      
      p = (char *)strtok(NULL, " \t");
    }
  }
  
  // remove the pid file if it exists
  remove(master_pid);

  // spawn
  if ((err = spawn(mysqld_file, &al, FALSE, NULL, master_out, master_err)) == 0)
  {
    sleep_until_file_exists(master_pid);
    
	if ((err = wait_for_server_start(bin_dir, user, password, master_port)) == 0)
    {
      master_running = TRUE;
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
  
  // free_args
  free_args(&al);
}

/******************************************************************************

  start_slave()
  
  Start the slave server.

******************************************************************************/
void start_slave()
{
  arg_list_t al;
  int err, i;
  char slave_out[PATH_MAX];
  char slave_err[PATH_MAX];
  char temp[PATH_MAX];
  
  // skip?
  if (skip_slave) return;

  // remove stale binary logs
  removef("%s/*-bin.*", slave_dir);

  // remove stale binary logs
  removef("%s/*.index", slave_dir);

  // remove master.info file
  removef("%s/master.info", slave_dir);

  // remove relay files
  removef("%s/var/log/*relay*", mysql_test_dir);

  // remove relay-log.info file
  removef("%s/relay-log.info", slave_dir);

  // init script
  if (slave_init_script[0] != NULL)
  {
    // run_init_script(slave_init_script);
    
    // TODO: use the scripts
    if (strindex(slave_init_script, "rpl000016-slave.sh") != NULL)
    {
      // create empty master.info file
      snprintf(temp, PATH_MAX, "%s/master.info", slave_dir);
      close(open(temp, O_WRONLY | O_CREAT));
    }
    else if (strindex(slave_init_script, "rpl000017-slave.sh") != NULL)
    {
      FILE *fp;
      
      // create a master.info file
      snprintf(temp, PATH_MAX, "%s/master.info", slave_dir);
      fp = fopen(temp, "wb+");
      
      fputs("master-bin.001\n", fp);
      fputs("4\n", fp);
      fputs("127.0.0.1\n", fp);
      fputs("replicate\n", fp);
      fputs("aaaaaaaaaaaaaaabthispartofthepasswordisnotused\n", fp);
      fputs("9306\n", fp);
      fputs("1\n", fp);
      fputs("0\n", fp);

      fclose(fp);
    }
    else if (strindex(slave_init_script, "rpl_rotate_logs-slave.sh") != NULL)
    {
      // create empty master.info file
      snprintf(temp, PATH_MAX, "%s/master.info", slave_dir);
      close(open(temp, O_WRONLY | O_CREAT));
    }
  }

  // redirection files
  snprintf(slave_out, PATH_MAX, "%s/var/run/slave%u.out",
           mysql_test_dir, restarts);
  snprintf(slave_err, PATH_MAX, "%s/var/run/slave%u.err",
           mysql_test_dir, restarts);
  
  // args
  init_args(&al);
  add_arg(&al, "%s", mysqld_file);
  add_arg(&al, "--no-defaults");
  add_arg(&al, "--log-bin=slave-bin");
  add_arg(&al, "--relay_log=slave-relay-bin");
  add_arg(&al, "--basedir=%s", base_dir);
  add_arg(&al, "--port=%u", slave_port);
  add_arg(&al, "--datadir=%s", slave_dir);
  add_arg(&al, "--pid-file=%s", slave_pid);
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

  // slave master info
  if (slave_master_info[0] != NULL)
  {
    char *p;

    p = (char *)strtok(slave_master_info, " \t");

    while(p)
    {
      add_arg(&al, "%s", p);
      
      p = (char *)strtok(NULL, " \t");
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
  
  // small server
  add_arg(&al, "-O");
  add_arg(&al, "key_buffer_size=1M");
  add_arg(&al, "-O");
  add_arg(&al, "sort_buffer=256K");
  add_arg(&al, "-O");
  add_arg(&al, "max_heap_table_size=1M");

  // opt args
  if (slave_opt[0] != NULL)
  {
    char *p;

    p = (char *)strtok(slave_opt, " \t");

    while(p)
    {
      add_arg(&al, "%s", p);
      
      p = (char *)strtok(NULL, " \t");
    }
  }
  
  // remove the pid file if it exists
  remove(slave_pid);

  // spawn
  if ((err = spawn(mysqld_file, &al, FALSE, NULL, slave_out, slave_err)) == 0)
  {
    sleep_until_file_exists(slave_pid);
    
    if ((err = wait_for_server_start(bin_dir, user, password, slave_port)) == 0)
    {
      slave_running = TRUE;
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
  
  // free args
  free_args(&al);
}

/******************************************************************************

  mysql_start()
  
  Start the mysql servers.

******************************************************************************/
void mysql_start()
{
  start_master();

  start_slave();
  
  // activate the test screen
  ActivateScreen(getscreenhandle());
}

/******************************************************************************

  stop_slave()
  
  Stop the slave server.

******************************************************************************/
void stop_slave()
{
  int err;
  
  // running?
  if (!slave_running) return;
  
  // stop
  if ((err = stop_server(bin_dir, user, password, slave_port, slave_pid)) == 0)
  {
    slave_running = FALSE;
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
  
  // running?
  if (!master_running) return;
  
  if ((err = stop_server(bin_dir, user, password, master_port, master_pid)) == 0)
  {
    master_running = FALSE;
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
  
  // activate the test screen
  ActivateScreen(getscreenhandle());
}

/******************************************************************************

  mysql_restart()
  
  Restart the mysql servers.

******************************************************************************/
void mysql_restart()
{
  log_info("Restarting the MySQL server(s): %u", ++restarts);

  mysql_stop();

  mysql_start();
}

/******************************************************************************

  read_option()
  
  Read the option file.

******************************************************************************/
int read_option(char *opt_file, char *opt)
{
  int fd, err;
  int result;
  char *p;
  char buf[PATH_MAX];
  
  // copy current option
  strncpy(buf, opt, PATH_MAX);
  
  // open options file
  fd = open(opt_file, O_RDONLY);
  
  err = read(fd, opt, PATH_MAX);
  
  close(fd);
  
  if (err > 0)
  {
    // terminate string
    if ((p = strchr(opt, '\n')) != NULL)
    {
      *p = NULL;
      
      // check for a '\r'
      if ((p = strchr(opt, '\r')) != NULL)
      {
        *p = NULL;
      }
    }
    else
    {
      opt[err] = NULL;
    }

    // check for $MYSQL_TEST_DIR
    if ((p = strstr(opt, "$MYSQL_TEST_DIR")) != NULL)
    {
      char temp[PATH_MAX];
      
      *p = NULL;
      
      strcpy(temp, p + strlen("$MYSQL_TEST_DIR"));
      
      strcat(opt, mysql_test_dir);
      
      strcat(opt, temp);
    }
  }
  else
  {
    // clear option
    *opt = NULL;
  }
  
  // compare current option with previous
  return strcmp(opt, buf);
}

/******************************************************************************

  run_test()
  
  Run the given test case.

******************************************************************************/
void run_test(char *test)
{
  char temp[PATH_MAX];
  char *rstr;
  double elapsed = 0;
  int skip = FALSE;
  int restart = FALSE;
  int flag = FALSE;
  struct stat info;
  
  // single test?
  if (!single_test)
  {
    // skip tests in the skip list
    snprintf(temp, PATH_MAX, " %s ", test);
    skip = (strindex(skip_test, temp) != NULL);
  }
    
  // skip test?
  if (!skip)
  {
    char test_file[PATH_MAX];
    char master_opt_file[PATH_MAX];
    char slave_opt_file[PATH_MAX];
    char slave_master_info_file[PATH_MAX];
    char result_file[PATH_MAX];
    char reject_file[PATH_MAX];
    char out_file[PATH_MAX];
    char err_file[PATH_MAX];
    int err;
    arg_list_t al;
    NXTime_t start, stop;
    
    // skip slave?
    flag = skip_slave;
    skip_slave = (strncmp(test, "rpl", 3) != 0);
    if (flag != skip_slave) restart = TRUE;
    
    // create files
    snprintf(master_opt_file, PATH_MAX, "%s/%s-master.opt", test_dir, test);
    snprintf(slave_opt_file, PATH_MAX, "%s/%s-slave.opt", test_dir, test);
    snprintf(slave_master_info_file, PATH_MAX, "%s/%s.slave-mi", test_dir, test);
    snprintf(reject_file, PATH_MAX, "%s/%s%s", result_dir, test, REJECT_SUFFIX);
    snprintf(out_file, PATH_MAX, "%s/%s%s", result_dir, test, OUT_SUFFIX);
    snprintf(err_file, PATH_MAX, "%s/%s%s", result_dir, test, ERR_SUFFIX);
    
    // netware specific files
    snprintf(test_file, PATH_MAX, "%s/%s%s", test_dir, test, NW_TEST_SUFFIX);
    if (stat(test_file, &info))
    {
      snprintf(test_file, PATH_MAX, "%s/%s%s", test_dir, test, TEST_SUFFIX);
    }
    
    snprintf(result_file, PATH_MAX, "%s/%s%s", result_dir, test, NW_RESULT_SUFFIX);
    if (stat(result_file, &info))
    {
      snprintf(result_file, PATH_MAX, "%s/%s%s", result_dir, test, RESULT_SUFFIX);
    }
    
    // init scripts
    snprintf(master_init_script, PATH_MAX, "%s/%s-master.sh", test_dir, test);
    if (stat(master_init_script, &info))
      master_init_script[0] = NULL;
    else
      restart = TRUE;
    
    snprintf(slave_init_script, PATH_MAX, "%s/%s-slave.sh", test_dir, test);
    if (stat(slave_init_script, &info))
      slave_init_script[0] = NULL;
    else
      restart = TRUE;

    // read options
    if (read_option(master_opt_file, master_opt)) restart = TRUE;
    if (read_option(slave_opt_file, slave_opt)) restart = TRUE;
    if (read_option(slave_master_info_file, slave_master_info)) restart = TRUE;
    
    // cleanup previous run
    remove(reject_file);
    remove(out_file);
    remove(err_file);
    
    // start or restart?
    if (!master_running) mysql_start();
      else if (restart) mysql_restart();
    
    // let the system stabalize
    sleep(1);

    // show test
    log("%-46s ", test);
    
    // args
    init_args(&al);
    add_arg(&al, "%s", mysqltest_file);
    add_arg(&al, "--no-defaults");
    add_arg(&al, "--port=%u", master_port);
    add_arg(&al, "--database=%s", db);
    add_arg(&al, "--user=%s", user);
    add_arg(&al, "--password=%s", password);
    add_arg(&al, "--silent");
    add_arg(&al, "--basedir=%s/", mysql_test_dir);
    add_arg(&al, "--host=127.0.0.1");
    add_arg(&al, "-v");
    add_arg(&al, "-R");
    add_arg(&al, "%s", result_file);
    
    // start timer
    NXGetTime(NX_SINCE_BOOT, NX_USECONDS, &start);
    
    // spawn
    err = spawn(mysqltest_file, &al, TRUE, test_file, out_file, err_file);
    
    // stop timer
    NXGetTime(NX_SINCE_BOOT, NX_USECONDS, &stop);
    
    // calculate
    elapsed = ((double)(stop - start)) / NX_USECONDS;
    total_time += elapsed;
    
    // free args
    free_args(&al);
    
    if (err == 0)
    {
      // pass
      rstr = TEST_PASS;
      ++total_pass;
      
      // increment total
      ++total_test;
    }
    else if (err == 2)
    {
      // skip
      rstr = TEST_SKIP;
      ++total_skip;
    }
    else if (err == 1)
    {
      // fail
      rstr = TEST_FAIL;
      ++total_fail;
      
      // increment total
      ++total_test;
    }
    else
    {
      rstr = TEST_BAD;
    }
  }
  else // early skips
  {
    // show test
    log("%-46s ", test);
    
    // skip
    rstr = TEST_SKIP;
    ++total_skip;
  }
  
  // result
  log("%10.06f   %-14s\n", elapsed, rstr);
}

/******************************************************************************

  vlog()
  
  Log the message.

******************************************************************************/
void vlog(char *format, va_list ap)
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
void log(char *format, ...)
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
void log_info(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);

  log("-- INFO : ");
  vlog(format, ap);
  log("\n");

  va_end(ap);
}

/******************************************************************************

  log_error()
  
  Log the given error.

******************************************************************************/
void log_error(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);

  log("-- ERROR: ");
  vlog(format, ap);
  log("\n");

  va_end(ap);
}

/******************************************************************************

  log_errno()
  
  Log the given error and errno.

******************************************************************************/
void log_errno(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);

  log("-- ERROR: (%003u) ", errno);
  vlog(format, ap);
  log("\n");

  va_end(ap);
}

/******************************************************************************

  die()
  
  Exit the application.

******************************************************************************/
void die(char *msg)
{
  log_error(msg);

  pressanykey();

  exit(-1);
}

/******************************************************************************

  setup()
  
  Setup the mysql test enviornment.

******************************************************************************/
void setup(char *file)
{
  char temp[PATH_MAX];
  char *p;
  
  // set the timezone for the timestamp test
  setenv("TZ", "GMT-3", TRUE);

  // find base dir
  strcpy(temp, strlwr(file));
  while((p = strchr(temp, '\\')) != NULL) *p = '/';
  
  if ((p = strindex(temp, "/mysql-test/")) != NULL)
  {
    *p = NULL;
    strcpy(base_dir, temp);
  }
  
  // setup paths
  snprintf(bin_dir, PATH_MAX, "%s/bin", base_dir);
  snprintf(mysql_test_dir, PATH_MAX, "%s/mysql-test", base_dir);
  snprintf(test_dir, PATH_MAX, "%s/t", mysql_test_dir);
  snprintf(mysql_tmp_dir, PATH_MAX, "%s/var/tmp", mysql_test_dir);
  snprintf(result_dir, PATH_MAX, "%s/r", mysql_test_dir);
  snprintf(master_dir, PATH_MAX, "%s/var/master-data", mysql_test_dir);
  snprintf(slave_dir, PATH_MAX, "%s/var/slave-data", mysql_test_dir);
  snprintf(lang_dir, PATH_MAX, "%s/share/english", base_dir);
  snprintf(char_dir, PATH_MAX, "%s/share/charsets", base_dir);
  
  // setup files
  snprintf(mysqld_file, PATH_MAX, "%s/mysqld", bin_dir);
  snprintf(mysqltest_file, PATH_MAX, "%s/mysqltest", bin_dir);
  snprintf(mysqladmin_file, PATH_MAX, "%s/mysqladmin", bin_dir);
  snprintf(master_pid, PATH_MAX, "%s/var/run/master.pid", mysql_test_dir);
  snprintf(slave_pid, PATH_MAX, "%s/var/run/slave.pid", mysql_test_dir);

  // create log file
  snprintf(temp, PATH_MAX, "%s/mysql-test-run.log", mysql_test_dir);
  if ((log_fd = fopen(temp, "w+")) == NULL)
  {
    log_errno("Unable to create log file.");
  }
  
  // prepare skip test list
  while((p = strchr(skip_test, ',')) != NULL) *p = ' ';
  strcpy(temp, strlwr(skip_test));
  snprintf(skip_test, PATH_MAX, " %s ", temp);
  
  // enviornment
  setenv("MYSQL_TEST_DIR", mysql_test_dir, 1);
}

/******************************************************************************

  main()
  
******************************************************************************/
int main(int argc, char **argv)
{
  // setup
  setup(argv[0]);
  
  // header
  log("MySQL Server %s, for %s (%s)\n\n", VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  
  log("Initializing Tests...\n");
  
  // install test databases
  mysql_install_db();
  
  log("Starting Tests...\n");
  
  log("\n");
  log(HEADER);
  log(DASH);

  if (argc > 1)
  {
    int i;

    // single test
    single_test = TRUE;    

    for (i = 1; i < argc; i++)
    {
      // run given test
      run_test(argv[i]);
    }
  }
  else
  {
    // run all tests
    DIR *dir = opendir(test_dir);
    DIR *entry;
    char test[NAME_MAX];
    char *p;
    
    // single test
    single_test = FALSE;    

    if (dir == NULL)
    {
      die("Unable to open tests directory.");
    }
    
    while((entry = readdir(dir)) != NULL)
    {
      if (!S_ISDIR(entry->d_type))
      {
        strcpy(test, strlwr(entry->d_name));
        
        // find the test suffix
        if ((p = strindex(test, TEST_SUFFIX)) != NULL)
        {
          // null terminate at the suffix
          *p = '\0';

          // run test
          run_test(test);
        }
      }
    }
    
    closedir(dir);
  }
  
  log(DASH);
  log("\n");
  
  log("Ending Tests...\n");

  // stop server
  mysql_stop();

  // report stats
  report_stats();
    
  // close log
  if (log_fd) fclose(log_fd);
  
  // keep results up
  pressanykey();

  return 0;
}

