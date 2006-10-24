/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "mysql_connection.h"

#include <m_string.h>
#include <m_string.h>
#include <my_global.h>
#include <mysql_com.h>
#include <mysql.h>
#include <my_sys.h>
#include <violite.h>

#include "command.h"
#include "log.h"
#include "messages.h"
#include "mysqld_error.h"
#include "mysql_manager_error.h"
#include "parse.h"
#include "priv.h"
#include "protocol.h"
#include "thread_registry.h"
#include "user_map.h"


Mysql_connection_thread_args::Mysql_connection_thread_args(
                             struct st_vio *vio_arg,
                             Thread_registry &thread_registry_arg,
                             const User_map &user_map_arg,
                             ulong connection_id_arg,
                             Instance_map &instance_map_arg) :
    vio(vio_arg)
    ,thread_registry(thread_registry_arg)
    ,user_map(user_map_arg)
    ,connection_id(connection_id_arg)
    ,instance_map(instance_map_arg)
  {}

/*
  MySQL connection - handle one connection with mysql command line client
  See also comments in mysqlmanager.cc to picture general Instance Manager
  architecture.
  We use conventional technique to work with classes without exceptions:
  class acquires all vital resource in init(); Thus if init() succeed,
  a user must call cleanup(). All other methods are valid only between
  init() and cleanup().
*/

class Mysql_connection_thread: public Mysql_connection_thread_args
{
public:
  Mysql_connection_thread(const Mysql_connection_thread_args &args);

  int init();
  void cleanup();

  void run();

  ~Mysql_connection_thread();
private:
  Thread_info thread_info;
  NET net;
  struct rand_struct rand_st;
  char scramble[SCRAMBLE_LENGTH + 1];
  uint status;
  ulong client_capabilities;
private:
  /* Names are conventionally the same as in mysqld */
  int check_connection();
  int do_command();
  int dispatch_command(enum enum_server_command command,
                       const char *text, uint len);
};


Mysql_connection_thread::Mysql_connection_thread(
                                   const Mysql_connection_thread_args &args) :
  Mysql_connection_thread_args(args.vio,
                               args.thread_registry,
                               args.user_map,
                               args.connection_id,
                               args.instance_map)
  ,thread_info(pthread_self(), TRUE)
{
  thread_registry.register_thread(&thread_info);
}


/*
  NET subsystem requieres its user to provide my_net_local_init extern
  C function (exactly as declared below). my_net_local_init is called by
  my_net_init and is supposed to set NET controlling variables.
  See also priv.h for variables description.
*/

C_MODE_START

void my_net_local_init(NET *net)
{
  net->max_packet= net_buffer_length;
  net->read_timeout= net_read_timeout;
  net->write_timeout= net_write_timeout;
  net->retry_count= net_retry_count;
  net->max_packet_size= max_allowed_packet;
}

C_MODE_END


/*
  Every resource, which we can fail to acquire, is allocated in init().
  This function is complementary to cleanup().
*/

int Mysql_connection_thread::init()
{
  /* Allocate buffers for network I/O */
  if (my_net_init(&net, vio))
    return 1;
  net.return_status= &status;
  /* Initialize random number generator */
  {
    ulong seed1= (ulong) &rand_st + rand();
    ulong seed2= (ulong) rand() + time(0);
    randominit(&rand_st, seed1, seed2);
  }
  /* Fill scramble - server's random message used for handshake */
  create_random_string(scramble, SCRAMBLE_LENGTH, &rand_st);
  /* We don't support transactions, every query is atomic */
  status= SERVER_STATUS_AUTOCOMMIT;
  return 0;
}


void Mysql_connection_thread::cleanup()
{
  net_end(&net);
}


Mysql_connection_thread::~Mysql_connection_thread()
{
  /* vio_delete closes the socket if necessary */
  vio_delete(vio);
  thread_registry.unregister_thread(&thread_info);
}


void Mysql_connection_thread::run()
{
  log_info("accepted mysql connection %d", (int) connection_id);

  my_thread_init();

  if (check_connection())
  {
    my_thread_end();
    return;
  }

  log_info("connection %d is checked successfully", (int) connection_id);

  vio_keepalive(vio, TRUE);

  while (!net.error && net.vio && !thread_registry.is_shutdown())
  {
    if (do_command())
      break;
  }

  my_thread_end();
}


int Mysql_connection_thread::check_connection()
{
  ulong pkt_len=0;                              // to hold client reply length

  /* buffer for the first packet */             /* packet contains: */
  char buff[MAX_VERSION_LENGTH + 1 +            // server version, 0-ended
            4 +                                 // connection id
            SCRAMBLE_LENGTH + 2 +               // scramble (in 2 pieces)
            18];                                // server variables: flags,
                                                // charset number, status,
  char *pos= buff;
  ulong server_flags;

  memcpy(pos, mysqlmanager_version.str, mysqlmanager_version.length + 1);
  pos+= mysqlmanager_version.length + 1;

  int4store((uchar*) pos, connection_id);
  pos+= 4;

  /*
    Old clients does not understand long scrambles, but can ignore packet
    tail: that's why first part of the scramble is placed here, and second
    part at the end of packet (even though we don't support old clients,
    we must follow standard packet format.)
  */
  memcpy(pos, scramble, SCRAMBLE_LENGTH_323);
  pos+= SCRAMBLE_LENGTH_323;
  *pos++= '\0';

  server_flags= CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 |
                CLIENT_SECURE_CONNECTION;

  /*
    18-bytes long section for various flags/variables

    Every flag occupies a bit in first half of ulong; int2store will
    gracefully pick up all flags.
  */
  int2store(pos, server_flags);
  pos+= 2;
  *pos++= (char) default_charset_info->number;  // global mysys variable
  int2store(pos, status);                       // connection status
  pos+= 2;
  bzero(pos, 13);                               // not used now
  pos+= 13;

  /* second part of the scramble, null-terminated */
  memcpy(pos, scramble + SCRAMBLE_LENGTH_323,
         SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323 + 1);
  pos+= SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323 + 1;

  /* write connection message and read reply */
  enum { MIN_HANDSHAKE_SIZE= 2 };
  if (net_write_command(&net, protocol_version, "", 0, buff, pos - buff) ||
     (pkt_len= my_net_read(&net)) == packet_error ||
      pkt_len < MIN_HANDSHAKE_SIZE)
  {
    net_send_error(&net, ER_HANDSHAKE_ERROR);
    return 1;
  }

  client_capabilities= uint2korr(net.read_pos);
  if (!(client_capabilities & CLIENT_PROTOCOL_41))
  {
    net_send_error_323(&net, ER_NOT_SUPPORTED_AUTH_MODE);
    return 1;
  }
  client_capabilities|= ((ulong) uint2korr(net.read_pos + 2)) << 16;

  pos= (char*) net.read_pos + 32;

  /* At least one byte for username and one byte for password */
  if (pos >= (char*) net.read_pos + pkt_len + 2)
  {
    /*TODO add user and password handling in error messages*/
    net_send_error(&net, ER_HANDSHAKE_ERROR);
    return 1;
  }

  const char *user= pos;
  const char *password= strend(user)+1;
  ulong password_len= *password++;
  LEX_STRING user_name= { (char *) user, password - user - 2 };

  if (password_len != SCRAMBLE_LENGTH)
  {
    net_send_error(&net, ER_ACCESS_DENIED_ERROR);
    return 1;
  }
  if (user_map.authenticate(&user_name, password, scramble))
  {
    net_send_error(&net, ER_ACCESS_DENIED_ERROR);
    return 1;
  }
  net_send_ok(&net, connection_id, NULL);
  return 0;
}


int Mysql_connection_thread::do_command()
{
  char *packet;
  ulong packet_length;

  /* We start to count packets from 0 for each new command */
  net.pkt_nr= 0;

  if ((packet_length=my_net_read(&net)) == packet_error)
  {
    /* Check if we can continue without closing the connection */
    if (net.error != 3) // what is 3 - find out
      return 1;
    if (thread_registry.is_shutdown())
      return 1;
    net_send_error(&net, net.last_errno);
    net.error= 0;
    return 0;
  }
  else
  {
    if (thread_registry.is_shutdown())
      return 1;
    packet= (char*) net.read_pos;
    enum enum_server_command command= (enum enum_server_command)
                                      (uchar) *packet;
    log_info("connection %d: packet_length=%d, command=%d",
             (int) connection_id, (int) packet_length, (int) command);
    return dispatch_command(command, packet + 1, packet_length - 1);
  }
}

int Mysql_connection_thread::dispatch_command(enum enum_server_command command,
                                              const char *packet, uint len)
{
  switch (command) {
  case COM_QUIT:                                // client exit
    log_info("query for connection %d received quit command",
             (int) connection_id);
    return 1;
  case COM_PING:
    log_info("query for connection %d received ping command",
             (int) connection_id);
    net_send_ok(&net, connection_id, NULL);
    break;
  case COM_QUERY:
  {
    log_info("query for connection %d : ----\n%s\n-------------------------",
	     (int) connection_id,
             (const char *) packet);
    if (Command *command= parse_command(&instance_map, packet))
    {
      int res= 0;
      log_info("query for connection %d successfully parsed",
               (int) connection_id);
      res= command->execute(&net, connection_id);
      delete command;
      if (!res)
        log_info("query for connection %d executed ok",
                 (int) connection_id);
      else
      {
        log_info("query for connection %d executed err=%d",
                 (int) connection_id, (int) res);
        net_send_error(&net, res);
        return 0;
      }
    }
    else
    {
      net_send_error(&net,ER_OUT_OF_RESOURCES);
      return 0;
    }
    break;
  }
  default:
    log_info("query for connection %d received unknown command",
             (int) connection_id);
    net_send_error(&net, ER_UNKNOWN_COM_ERROR);
    break;
  }
  return 0;
}


pthread_handler_t mysql_connection(void *arg)
{
  Mysql_connection_thread_args *args= (Mysql_connection_thread_args *) arg;
  Mysql_connection_thread mysql_connection_thread(*args);
  delete args;
  if (mysql_connection_thread.init())
    log_info("mysql_connection(): error initializing thread");
  else
  {
    mysql_connection_thread.run();
    mysql_connection_thread.cleanup();
  }
  return 0;
}

/*
  vim: fdm=marker
*/
