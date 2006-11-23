/* Copyright (C) 2000 MySQL AB

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

#include "client_priv.h"
#include <my_dir.h>
#include <my_list.h>
#include <sslopt-vars.h>

#define UPGRADE_DEFAULTS_NAME "mysql_upgrade_defaults"
#define MYSQL_UPGRADE_INFO_NAME "mysql_upgrade_info"
#define MYSQL_FIX_PRIV_TABLES_NAME "mysql_fix_privilege_tables.sql"

#define MY_PARENT       (1 << 0)
#define MY_ISDIR        (1 << 1)
#define MY_SEARCH_SELF  (1 << 2)

#ifdef __WIN__
const char *mysqlcheck_name= "mysqlcheck.exe";
const char *mysql_name= "mysql.exe";
const char *mysqld_name= "mysqld.exe";
#define EXTRA_CLIENT_PATHS "client/release", "client/debug"
#else
const char *mysqlcheck_name= "mysqlcheck";
const char *mysql_name= "mysql";
const char *mysqld_name= "mysqld";
#define EXTRA_CLIENT_PATHS "client"
#endif /*__WIN__*/

extern TYPELIB sql_protocol_typelib;

static my_bool opt_force= 0, opt_verbose= 0, opt_compress= 0;
static char *user= (char*) "root", *basedir= 0, *datadir= 0, *opt_password= 0;
static char *current_host= 0;
static char *opt_default_charset= 0, *opt_charsets_dir= 0;
#ifdef HAVE_SMEM
static char *shared_memory_base_name= 0;
#endif
static char *opt_protocol= 0;
static my_string opt_mysql_port= 0, opt_mysql_unix_port= 0;
#ifndef DBUG_OFF
static char *default_dbug_option= (char*) "d:t:O,/tmp/mysql_upgrade.trace";
#endif
static my_bool info_flag= 0, tty_password= 0;

static struct my_option my_long_options[]=
{
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 'b', "Specifies the directory where MySQL is installed",
   (gptr*) &basedir,
   (gptr*) &basedir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'd', "Specifies the data directory", (gptr*) &datadir,
   (gptr*) &datadir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit",
   0, 0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log", (gptr *) & default_dbug_option,
   (gptr *) & default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"debug-info", 'T', "Print some debug info at exit.", (gptr *) & info_flag,
   (gptr *) & info_flag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", (gptr*) &opt_default_charset,
   (gptr*) &opt_default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"force", 'f', "Force execution of mysqlcheck even if mysql_upgrade " 
   "has already been executed for the current version of MySQL.",
   (gptr*) &opt_force, (gptr*) &opt_force, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.", (gptr*) &opt_charsets_dir,
   (gptr*) &opt_charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", OPT_COMPRESS, "Use compression in server/client protocol.",
   (gptr*) &opt_compress, (gptr*) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"host",'h', "Connect to host.", (gptr*) &current_host,
   (gptr*) &current_host, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given"
   " it's solicited on the tty.", 0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, 
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection.", (gptr*) &opt_mysql_port,
   (gptr*) &opt_mysql_port, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL,
   "The protocol of connection (tcp,socket,pipe,memory).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", (gptr*) &shared_memory_base_name, 
   (gptr*) &shared_memory_base_name, 0, 
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"socket", 'S', "Socket file to use for connection.",
   (gptr*) &opt_mysql_unix_port, (gptr*) &opt_mysql_unix_port, 0, 
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login if not current user.", (gptr*) &user,
   (gptr*) &user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
  {"verbose", 'v', "Display more output about the process", 
   (gptr*) &opt_verbose, (gptr*) &opt_verbose, 0, 
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static const char *load_default_groups[]=
{
  "mysql_upgrade", 0
};

#include <help_end.h>

static LIST *extra_defaults= NULL;

typedef struct _extra_default
{
  int id;
  const char *name;
  int n_len;
  const char *value;
  int v_len;
} extra_default_t;

static inline 
void set_extra_default(int id, const struct my_option *opt)
{
  switch (id) {
  case 'b': case 'd':   /* these are ours */
  case 'f':             /* --force is ours */
  case 'u':             /* --user passed on cmdline */
  case 'T':             /* --debug-info is not accepted by mysqlcheck */
    /* so, do nothing */
    break;
  default:
    {
      LIST *l;
      extra_default_t *d;

      /* 
        Remove any earlier duplicates: they can
        refer to invalid memory addresses (stale pointers) 
      */
      l= extra_defaults; 
      while (l)
      {
        LIST *n= l->next;

        d= l->data;
        if (d->id == id) 
        {
          extra_defaults= list_delete(extra_defaults, l);
          my_free((gptr)l, MYF(0));
          my_free((gptr)d, MYF(0));
        }
        l= n;
      }

      d= (extra_default_t *)my_malloc(sizeof(extra_default_t), 
                           MYF(MY_FAE|MY_ZEROFILL));
      d->id= id;
      d->name= opt->name;
      d->n_len= strlen(opt->name);
      if (opt->arg_type != NO_ARG)
        switch (opt->var_type & GET_TYPE_MASK) {
        case GET_BOOL:
          if (*((int *)opt->value))
          {
            d->value= "true";
            d->v_len= 4;
          }
          break;
        case GET_STR: 
        case GET_STR_ALLOC:
          d->value= *opt->value;
          d->v_len= strlen(d->value); 
          break;
        default:
          my_printf_error(0, "Error: internal error at %s:%d", MYF(0), 
                          __FILE__, __LINE__);
          exit(1);
        }
      list_push(extra_defaults, d);
    }
  }
}


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__ ((unused)),
               char *argument)
{
  switch (optid) {
  case '?':
    puts
      ("MySQL utility to upgrade database to the current server version");
    puts("");
    my_print_help(my_long_options);
    exit(0);
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    break;
  case 'p':
    tty_password= 1;
    if (argument)
    {
      char *start= argument;
      my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
      opt_password= my_strdup(argument, MYF(MY_FAE));
      while (*argument)
        *argument++= 'x';                       /* Destroy argument */
      if (*start)
        start[1]= 0;                            /* Cut length of argument */
      tty_password= 0;
    }
    break;
#ifdef __WIN__
  case 'W':
    my_free(opt_protocol, MYF(MY_ALLOW_ZERO_PTR));
    opt_protocol= my_strdup("pipe", MYF(MY_FAE));
    break;
#endif
  case OPT_MYSQL_PROTOCOL:
    if (find_type(argument, &sql_protocol_typelib, 0) > 0)
    {
      my_free(opt_protocol, MYF(MY_ALLOW_ZERO_PTR));
      opt_protocol= my_strdup(argument, MYF(MY_FAE));
    }
    else 
    {
      fprintf(stderr, "Unknown option to protocol: %s\n", argument);
      exit(1); 
    }
    break;
#include <sslopt-case.h>
  default:;
  }
  set_extra_default(opt->id, opt);
  return 0;
}


static int create_check_file(const char *path)
{
  int ret;
  File fd;
  
  fd= my_open(path, O_CREAT | O_WRONLY, MYF(MY_FAE | MY_WME));
  if (fd < 0) {
    ret= 1;
    goto error;
  }
  ret= my_write(fd, MYSQL_SERVER_VERSION, 
                  sizeof(MYSQL_SERVER_VERSION) - 1,
                  MYF(MY_WME | MY_FNABP));
  ret|= my_close(fd, MYF(MY_FAE | MY_WME));
error:
  return ret;
}


static int create_defaults_file(const char *path, const char *forced_path)
{
  int ret;
  uint cnt;
  File forced_file, defaults_file;
  
  DYNAMIC_STRING buf;
  extra_default_t *d;

  my_delete(path, MYF(0));
  
  defaults_file= my_open(path, O_BINARY | O_CREAT | O_WRONLY | O_EXCL,
                         MYF(MY_FAE | MY_WME));
  if (defaults_file < 0)
  {
    ret= 1;
    goto out;
  }

  if (init_dynamic_string(&buf, NULL, my_getpagesize(), FN_REFLEN))
  {
    ret= 1;
    goto error;
  }

  if (forced_path)
  {
    forced_file= my_open(forced_path, O_RDONLY, MYF(MY_FAE | MY_WME));
    if (forced_file < 0)
    {
      ret= 1;
      goto error;
    }
    do
    {
      cnt= my_read(forced_file, buf.str, buf.max_length, MYF(MY_WME));
      if ((cnt == MY_FILE_ERROR)
           || my_write(defaults_file, buf.str, cnt, MYF(MY_FNABP | MY_WME)))
      {
        ret= 1;
        my_close(forced_file, MYF(0));
        goto error;
      }
    } while (cnt == buf.max_length);
    my_close(forced_file, MYF(0));
  }
  
  dynstr_set(&buf, "\n[client]");
  while (extra_defaults) 
  {
    int len;
        
    d= extra_defaults->data;
    len= d->n_len + d->v_len + 1; 
    if (buf.length + len >= buf.max_length)     /* to avoid realloc() */
    {
      if (my_write(defaults_file, buf.str, buf.length, MYF(MY_FNABP | MY_WME)))
      {
        ret= 1;
        goto error;
      }
      dynstr_set(&buf, NULL);
    }
    if (dynstr_append_mem(&buf, "\n", 1)
       || dynstr_append_mem(&buf, d->name, d->n_len)
       || (d->v_len && (dynstr_append_mem(&buf, "=", 1)
       || dynstr_append_mem(&buf, d->value, d->v_len))))
    {
      ret= 1;
      goto error;
    }
    my_delete((gptr)d, MYF(0));
    list_pop(extra_defaults);                   /* pop off the head */
  }
  if (my_write(defaults_file, buf.str, buf.length, MYF(MY_FNABP | MY_WME)))
  {
    ret= 1;
    goto error;
  }
  /* everything's all right */
  ret= 0;
error:
  dynstr_free(&buf);
  
  if (defaults_file >= 0)
    ret|= my_close(defaults_file, MYF(MY_WME));
  
  if (ret)
    my_delete(path, MYF(0));
  
out:
  return ret;
}


/* Compare filenames */
static int comp_names(struct fileinfo *a, struct fileinfo *b)
{
  return (strcmp(a->name,b->name));
}


static int 
find_file(const char *name, const char *root, uint flags, char *result, size_t len, ...)
{
  int ret;
  va_list va;
  FILEINFO key= { (char*)name, NULL };
  const char *subdir;
  char *cp;
  
  DBUG_ASSERT(root != NULL);

  cp= strmake(result, root, len);
  if (cp[-1] != FN_LIBCHAR) 
    *cp++= FN_LIBCHAR; 
  
  ret= 1;
  va_start(va, len);
  subdir= (!(flags & MY_SEARCH_SELF)) ? va_arg(va, char *) : "";
  while (subdir)
  {
    MY_DIR *dir;
    FILEINFO *match;
    char *cp1;
    
    cp1= strnmov(cp, subdir, len - (cp - result) - 1);
    
    dir= my_dir(result, (flags & MY_ISDIR) ? MY_WANT_STAT : MYF(0));  
    if (dir) 
    { 
      match= bsearch(&key, dir->dir_entry, dir->number_off_files, 
                      sizeof(FILEINFO), (qsort_cmp)comp_names);
      if (match)
      {
        ret= (flags & MY_ISDIR) ? !MY_S_ISDIR(match->mystat->st_mode) : 0;
        if (!ret)
        { 
          if (cp1[-1] != FN_LIBCHAR)
            *cp1++= FN_LIBCHAR;
          
          if (!(flags & MY_PARENT)) 
            strnmov(cp1, name, len - (cp1 - result));
          else
            *cp1= '\0';
          
          my_dirend(dir);
          break;
        }
      }
      my_dirend(dir);
    }
    subdir= va_arg(va, char *);
  }
  va_end(va);
  return ret;
}


int main(int argc, char **argv)
{
  int ret;
  
  char *forced_defaults_file;
  char *forced_extra_defaults;
  char *defaults_group_suffix;
  const char *script_line;
  char *upgrade_defaults_path; 
  char *defaults_to_use= NULL;
  int upgrade_defaults_created= 0;
  
  char path[FN_REFLEN];
  DYNAMIC_STRING cmdline;

  MY_INIT(argv[0]);
#ifdef __NETWARE__
  setscreenmode(SCR_AUTOCLOSE_ON_EXIT);
#endif

  /* Check if we are forced to use specific defaults */
  get_defaults_options(argc, argv,
                       &forced_defaults_file, &forced_extra_defaults,
                       &defaults_group_suffix);
  
  load_defaults("my", load_default_groups, &argc, &argv);

  if (handle_options(&argc, &argv, my_long_options, get_one_option)) 
  {
    ret= 1;
    goto error;
  }
  if (tty_password)
    opt_password= get_tty_password(NullS);

  if (init_dynamic_string(&cmdline, NULL, 2 * FN_REFLEN + 128, FN_REFLEN))
  {
    ret= 1;
    goto error;
  }
  if (!basedir)
  {
    my_getwd(path, sizeof(path), MYF(0));
    basedir= my_strdup(path, MYF(0));
    if (find_file("errmsg.sys", basedir, MYF(0), path, sizeof(path),
                              "share/mysql/english", NullS)
       || find_file(mysqld_name, basedir, MYF(0), path, sizeof(path),
                              "bin", "libexec", NullS))
    {
      my_free((gptr)basedir, MYF(0));
      basedir= (char *)DEFAULT_MYSQL_HOME;
    }
  }

  if (!datadir)
  {
    if (!find_file("mysql", basedir, MYF(MY_ISDIR|MY_PARENT), 
                            path, sizeof(path),
                            "data", "var", NullS))
      datadir= my_strdup(path, MYF(0));
    else
      datadir= (char *)DATADIR;
  }
  if (find_file("user.frm", datadir, MYF(0), path, sizeof(path), 
                          "mysql", NullS))
  {
    ret= 1;
    fprintf(stderr,
            "Can't find data directory. Please restart with"
            " --datadir=path-to-writable-data-dir");
    goto error;
  }

  /* 
     Create the modified defaults file to be used by mysqlcheck 
     and mysql tools                                            
   */
  fn_format(path, UPGRADE_DEFAULTS_NAME, datadir, "", MYF(0));
  upgrade_defaults_path= my_strdup(path, MYF(0));
  
  if (extra_defaults) 
  {
    ret= create_defaults_file(upgrade_defaults_path, forced_extra_defaults);
    if (ret)
      goto error;
    
    defaults_to_use= upgrade_defaults_path;
    upgrade_defaults_created= 1;
  } 
  else
    defaults_to_use= forced_extra_defaults;

  if (!find_file(MYSQL_UPGRADE_INFO_NAME, datadir, MY_SEARCH_SELF, 
                          path, sizeof(path), NULL, NullS)
     && !opt_force)
  {
    char buf[sizeof(MYSQL_SERVER_VERSION)];
    int fd, cnt;
   
    fd= my_open(path, O_RDONLY, MYF(0));
    cnt= my_read(fd, buf, sizeof(buf) - 1, MYF(0));
    my_close(fd, MYF(0));
    buf[cnt]= 0;
    if (!strcmp(buf, MYSQL_SERVER_VERSION))
    {
      if (opt_verbose)
        puts("mysql_upgrade has already been done for this version");
      goto fix_priv_tables;
    }
  }
  
  if (find_file(mysqlcheck_name, basedir, MYF(0), path, sizeof(path),
                "bin", EXTRA_CLIENT_PATHS, NullS))
  {
     ret= 1;
     fprintf(stderr,
             "Can't find program '%s'\n"
             "Please restart with --basedir=mysql-install-directory",
             mysqlcheck_name);
     goto error;
  }
  else
    dynstr_set(&cmdline, path);

  if (defaults_to_use)
  {
    dynstr_append(&cmdline, " --defaults-extra-file=");
    dynstr_append(&cmdline, defaults_to_use);
  }
  
  dynstr_append(&cmdline, " --check-upgrade --all-databases"
                 " --auto-repair --user=");
  dynstr_append(&cmdline, user);

  if (opt_verbose)
    printf("Running %s\n", cmdline.str);

  ret= system(cmdline.str);
  if (ret)
  {
    fprintf(stderr, "Error executing '%s'\n", cmdline.str);
    goto error;
  }

  fn_format(path, MYSQL_UPGRADE_INFO_NAME, datadir, "", MYF(0));
  ret= create_check_file(path);
  if (ret)
    goto error;

fix_priv_tables:
  if (find_file(mysql_name, basedir, MYF(0), path, sizeof(path), 
                "bin", EXTRA_CLIENT_PATHS, NullS))
  {
    ret= 1;
    fprintf(stderr,
           "Could not find MySQL command-line client (mysql).\n"
           "Please use --basedir to specify the directory"
           " where MySQL is installed.");
    goto error;
  }
  else
    dynstr_set(&cmdline, path);

  if (find_file(MYSQL_FIX_PRIV_TABLES_NAME, basedir, MYF(0), 
                          path, sizeof(path), 
                          "support_files", "share", "share/mysql", "scripts",
                          NullS)
     && find_file(MYSQL_FIX_PRIV_TABLES_NAME, "/usr/local/mysql", MYF(0),
                          path, sizeof(path),
                          "share/mysql", NullS))
  {
    ret= 1;
    fprintf(stderr,
           "Could not find file " MYSQL_FIX_PRIV_TABLES_NAME "\n"
           "Please use --basedir to specify the directory"
           " where MySQL is installed");
    goto error;
  }
  else
    script_line= my_strdup(path, MYF(0));

  if (defaults_to_use)
  {
    dynstr_append(&cmdline, " --defaults-extra-file=");
    dynstr_append(&cmdline, defaults_to_use);
  }
  dynstr_append(&cmdline, " --force --no-auto-rehash --batch --user=");
  dynstr_append(&cmdline, user);
  dynstr_append(&cmdline, " mysql < ");
  dynstr_append(&cmdline, script_line);

  if (opt_verbose)
    printf("Running %s\n", cmdline.str);

  ret= system(cmdline.str);
  if (ret)
    fprintf(stderr, "Error executing '%s'\n", cmdline.str);

error:
  dynstr_free(&cmdline);

  if (upgrade_defaults_created)
    my_delete(upgrade_defaults_path, MYF(0));
  
  my_end(info_flag ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  return ret;
}

