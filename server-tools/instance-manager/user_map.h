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

#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_USER_MAP_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_USER_MAP_H

#include <my_global.h>

#include <my_sys.h>
#include <hash.h>

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

/*
  User_map -- all users and passwords
*/

class User_map
{
public:
  User_map() {}
  ~User_map();

  int init();
  int load(const char *password_file_name);
  int authenticate(const char *user_name, uint length,
                   const char *scrambled_password,
                   const char *scramble) const;
private:
  HASH hash;
};

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_USER_MAP_H
