/* Copyright (C) 2004-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_PRIV_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_PRIV_H

#include <sys/types.h>
#ifdef __WIN__
#include "portability.h"
#else
#include <unistd.h>
#endif
#include "my_pthread.h"

/* IM-wide platform-independent defines */
#define SERVER_DEFAULT_PORT MYSQL_PORT
#define DEFAULT_MONITORING_INTERVAL 20
#define DEFAULT_PORT 2273
/* three-week timeout should be enough */
#define LONG_TIMEOUT ((ulong) 3600L*24L*21L)

/* the pid of the manager process (of the signal thread on the LinuxThreads) */
extern pid_t manager_pid;

#ifndef __WIN__
/*
  This flag is set if mysqlmanager has detected that it is running on the
  system using LinuxThreads
*/
extern bool linuxthreads;
#endif

extern const char mysqlmanager_version[];
extern const int mysqlmanager_version_length;

/* MySQL client-server protocol version: substituted from configure */
extern const unsigned char protocol_version;

/*
  These variables are used in MySQL subsystem to work with mysql clients
  To be moved to a config file/options one day.
*/


/* Buffer length for TCP/IP and socket communication */
extern unsigned long net_buffer_length;


/* Maximum allowed incoming/ougoung packet length */
extern unsigned long max_allowed_packet;


/*
  Number of seconds to wait for more data from a connection before aborting
  the read
*/
extern unsigned long net_read_timeout;


/*
  Number of seconds to wait for a block to be written to a connection
  before aborting the write.
*/
extern unsigned long net_write_timeout;


/*
  If a read on a communication port is interrupted, retry this many times
  before giving up.
*/
extern unsigned long net_retry_count;

extern unsigned int test_flags;
extern unsigned long bytes_sent, bytes_received;
extern unsigned long mysqld_net_retry_count;
extern unsigned long open_files_limit;


int set_stacksize_n_create_thread(pthread_t  *thread, pthread_attr_t *attr,
                                  void *(*start_routine)(void *), void *arg);

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_PRIV_H
