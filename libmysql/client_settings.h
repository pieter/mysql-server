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

extern uint		mysql_port;
extern my_string	mysql_unix_port;

#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG |	  \
                             CLIENT_LOCAL_FILES | CLIENT_TRANSACTIONS |   \
			     CLIENT_PROTOCOL_41 | CLIENT_SECURE_CONNECTION)

sig_handler pipe_sig_handler(int sig __attribute__((unused)));
my_bool stmt_close(MYSQL_STMT *stmt, my_bool skip_list);
void read_user_name(char *name);
my_bool send_file_to_server(MYSQL *mysql, const char *filename);

/*
  Let the user specify that we don't want SIGPIPE;  This doesn't however work
  with threaded applications as we can have multiple read in progress.
*/

#if !defined(__WIN__) && defined(SIGPIPE) && !defined(THREAD)
#define init_sigpipe_variables  sig_return old_signal_handler=(sig_return) 0;
#define set_sigpipe(mysql)     if ((mysql)->client_flag & CLIENT_IGNORE_SIGPIPE) old_signal_handler=signal(SIGPIPE,pipe_sig_handler)
#define reset_sigpipe(mysql) if ((mysql)->client_flag & CLIENT_IGNORE_SIGPIPE) signal(SIGPIPE,old_signal_handler);
#else
#define init_sigpipe_variables
#define set_sigpipe(mysql)
#define reset_sigpipe(mysql)
#endif

void mysql_read_default_options(struct st_mysql_options *options,
				const char *filename,const char *group);
MYSQL * STDCALL 
cli_mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		       const char *passwd, const char *db,
		       uint port, const char *unix_socket,ulong client_flag);

void STDCALL cli_mysql_close(MYSQL *mysql);

MYSQL_FIELD * STDCALL cli_list_fields(MYSQL *mysql);
my_bool STDCALL cli_read_prepare_result(MYSQL *mysql, MYSQL_STMT *stmt);
MYSQL_DATA *cli_read_rows(MYSQL *mysql,MYSQL_FIELD *mysql_fields,
			  uint fields);
int STDCALL cli_stmt_execute(MYSQL_STMT *stmt);
MYSQL_DATA *cli_read_binary_rows(MYSQL_STMT *stmt);
int STDCALL cli_unbuffered_fetch(MYSQL *mysql, char **row);
const char * STDCALL cli_read_statistic(MYSQL *mysql);
