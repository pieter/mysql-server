/* Copyright (C) 2000-2004 MySQL AB

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

/*
  Set MYSQL_SERVER_SUFFIX
  The following code is quite ugly as there is no portable way to set a
  string to the value of a macro
*/

#if defined(MYSQL_SERVER_SUFFIX_NT)
#undef MYSQL_SERVER_SUFFIX
#define MYSQL_SERVER_SUFFIX "-nt"
#elif defined(MYSQL_SERVER_SUFFIX_MAX)
#undef MYSQL_SERVER_SUFFIX
#define MYSQL_SERVER_SUFFIX "-max"
#elif defined(MYSQL_SERVER_SUFFIX_NT_MAX)
#undef MYSQL_SERVER_SUFFIX
#define MYSQL_SERVER_SUFFIX "-nt-max"
#elif defined(MYSQL_SERVER_SUFFIX_PRO)
#undef MYSQL_SERVER_SUFFIX
#define MYSQL_SERVER_SUFFIX "-pro"
#elif defined(MYSQL_SERVER_SUFFIX_PRO_NT)
#undef MYSQL_SERVER_SUFFIX
#define MYSQL_SERVER_SUFFIX "-pro-nt"
#endif
