/* Copyright (C) 2000 MySQL AB

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


/*
** change the following to the output of password('our password')
** split into 2 parts of 8 characters each.
** This is done to make it impossible to search after a text string in the
** mysql binary.
*/

#include "mysql_priv.h"

#ifdef HAVE_CRYPTED_FRM

/* password('test') */
ulong password_seed[2]={0x378b243e, 0x220ca493};

SQL_CRYPT *get_crypt_for_frm(void)
{
  return new SQL_CRYPT(password_seed);
}

#endif
