/* Copyright (C) 2003 MySQL AB

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

#include <sys/types.h>
#include <sys/stat.h>
#include <my_dir.h>


class Transparent_file
{
  File filedes;
  byte *buff;  /* in-memory window to the file or mmaped area */
  /* current window sizes */
  off_t lower_bound;
  off_t upper_bound;
  uint buff_size;

public:

  Transparent_file();
  ~Transparent_file();

  void init_buff(File filedes_arg);
  byte *ptr();
  off_t start();
  off_t end();
  char get_value (off_t offset);
  off_t read_next();
};
