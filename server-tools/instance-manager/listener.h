/* Copyright (C) 2003-2006 MySQL AB

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

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H

#include <my_global.h>
#include <my_pthread.h>

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif


pthread_handler_t listener(void *arg);

class Thread_registry;
struct Options;
class User_map;
class Instance_map;

struct Listener_thread_args
{
  Thread_registry &thread_registry;
  const Options &options;
  const User_map &user_map;
  Instance_map &instance_map;

  Listener_thread_args(Thread_registry &thread_registry_arg,
                       const Options &options_arg,
                       const User_map &user_map_arg,
                       Instance_map &instance_map_arg) :
    thread_registry(thread_registry_arg)
    ,options(options_arg)
    ,user_map(user_map_arg)
    ,instance_map(instance_map_arg)
  {}
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H
