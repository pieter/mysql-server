/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#define MANAGER_CLIENT_VERSION "1.0"

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql.h>
#include <mysql_version.h>
#include <m_ctype.h>
#ifdef OS2
#include <config-os2.h>
#else
#include <my_config.h>
#endif
#include <my_dir.h>
#include <hash.h>
#include <mysqld_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <violite.h>

#ifndef MYSQL_MANAGER_PORT
#define MYSQL_MANAGER_PORT 23546
#endif

static void die(const char* fmt, ...);

const char* user="root",*host="localhost";
char* pass=0;
int quiet=0;
uint port=MYSQL_MANAGER_PORT;
static const char *load_default_groups[]= { "mysqlmanagerc",0 };
char** default_argv;
MYSQL_MANAGER *manager;
FILE* fp, *fp_out;

struct option long_options[] =
{
  {"host",required_argument,0,'h'},
  {"user",required_argument,0,'u'},
  {"password",optional_argument,0,'p',},
  {"port",required_argument,0,'P'},
  {"help",no_argument,0,'?'},
  {"version",no_argument,0,'V'},
  {"quiet",no_argument,0,'q'},
  {0,0,0,0}
};

static void die(const char* fmt, ...)
{
  va_list args;
  DBUG_ENTER("die");
  va_start(args, fmt);
  if (fmt)
  {
    fprintf(stderr, "%s: ", my_progname);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  va_end(args);
  exit(1);
}

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,
	 MANAGER_CLIENT_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

void usage()
{
  print_version();
  printf("MySQL AB, by Sasha, Matt & Monty\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Runs a test against the mysql server and compares output with a results file.\n\n");
  printf("Usage: %s [OPTIONS] < command_file\n", my_progname);
  printf("\n\
  -?, --help               Display this help and exit.\n");
  printf("\
  -h, --host=...           Connect to host.\n\
  -u, --user=...           User for login.\n\
  -p[password], --password[=...]\n\
                           Password to use when connecting to server.\n\
  -P, --port=...           Port number to use for connection.\n\
  -q, --quiet, --silent    Suppress all normal output.\n\
  -V, --version            Output version information and exit.\n\
  --no-defaults            Don't read default options from any options file.\n\n");
}
int parse_args(int argc, char **argv)
{
  int c, option_index = 0;
  my_bool tty_password=0;

  load_defaults("my",load_default_groups,&argc,&argv);
  default_argv= argv;

  while ((c = getopt_long(argc, argv, "h:p::u:P:?Vq",
			 long_options, &option_index)) != EOF)
    {
      switch (c)
      {
      case 'h':
	host=optarg;
	break;
      case 'u':
	user=optarg;
	break;
      case 'p':
	if (optarg)
	{
	  my_free(pass,MYF(MY_ALLOW_ZERO_PTR));
	  pass=my_strdup(optarg,MYF(MY_FAE));
	  while (*optarg) *optarg++= 'x';		/* Destroy argument */
	}
	else
	  tty_password=1;
	break;
      case 'P':
	port=atoi(optarg);
	break;
      case 'q':
	quiet=1;
	break;
      case 'V':
	print_version();
	exit(0);
      case '?':
	usage();
	exit(1);				/* Unknown option */
      default:
	usage();
	exit(1);
      }
    }
 return 0;
}
int main(int argc, char** argv)
{
  MY_INIT(argv[0]);
  fp=stdin;
  fp_out=stdout;
  parse_args(argc,argv);
  if (!(manager=mysql_manager_init(0)))
    die("Failed in mysql_manager_init()");
  if (!mysql_manager_connect(manager,host,user,pass,port))
    die("Could not connect to MySQL manager: %s(%d)",manager->last_error,
	manager->last_errno);
  for (;!feof(fp);)
  {
    char buf[1024];
    if (!fgets(buf,sizeof(buf),fp))
      break;
    if (mysql_manager_command(manager,buf,strlen(buf)))
      die("Error in command: %s(%d)",manager->last_error,manager->last_errno);
    while (!manager->eof)
    {
      if (mysql_manager_fetch_line(manager,buf,sizeof(buf)))
	die("Error fetching result line: %s(%d)", manager->last_error,
	    manager->last_errno);
      fprintf(fp_out,"%s\n",buf);
    }
  }
  mysql_manager_close(manager);
  return 0;
}


