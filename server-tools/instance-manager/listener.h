#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_LISTENER_H
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

#ifdef __GNUC__
#pragma interface
#endif

#include <my_global.h>
#include <my_pthread.h>

C_MODE_START

pthread_handler_decl(listener, arg);

C_MODE_END

class Thread_repository;

struct Listener_thread_args
{
  Thread_repository &thread_repository;
  const char *socket_file_name;

  Listener_thread_args(Thread_repository &thread_repository_arg,
                       const char *socket_file_name_arg) :
    thread_repository(thread_repository_arg),
    socket_file_name(socket_file_name_arg) {}
};

#endif
