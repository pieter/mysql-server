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


#include <thr_alarm.h>

#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG |	  \
                             CLIENT_SECURE_CONNECTION | CLIENT_TRANSACTIONS | \
			     CLIENT_PROTOCOL_41 | CLIENT_SECURE_CONNECTION)

#define init_sigpipe_variables
#define set_sigpipe(mysql)
#define reset_sigpipe(mysql)
#define read_user_name(A) {}
#define mysql_rpl_query_type(A,B) MYSQL_RPL_ADMIN
#define mysql_master_send_query(A, B, C) 1
#define mysql_slave_send_query(A, B, C) 1
#define mysql_rpl_probe(mysql) 0
#undef HAVE_SMEM
#undef _CUSTOMCONFIG_

#define cli_list_fields NULL
#define cli_read_prepare_result NULL
#define cli_stmt_execute NULL
#define cli_read_binary_rows NULL

