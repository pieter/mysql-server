/* Copyright (C) 2000 MySQL AB

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

#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <mysql_version.h>

#include <stdio.h>

static void _print_array(uint8 *data, uint size)
{
  uint i;
  for (i = 0; i < size; ++i)
  {
    if (i == 0 || i % 16 == size % 16) printf("  ");
    printf(" %02x", data[i]);
    if ((i+1) % 16 == size % 16) printf("\n");
  }
}

static void _print_csinfo(CHARSET_INFO *cs)
{
  printf("%s #%d\n", cs->name, cs->number);
  printf("ctype:\n"); _print_array(cs->ctype, 257);
  printf("to_lower:\n"); _print_array(cs->to_lower, 256);
  printf("to_upper:\n"); _print_array(cs->to_upper, 256);
  printf("sort_order:\n"); _print_array(cs->sort_order, 256);
  printf("collate:    %3s (%d, %p, %p, %p)\n",
         cs->strxfrm_multiply ? "yes" : "no",
         cs->strxfrm_multiply,
         cs->strnncoll,
         cs->strnxfrm,
         cs->like_range);
  printf("multi-byte: %3s (%d, %p, %p, %p)\n",
         cs->mbmaxlen ? "yes" : "no",
         cs->mbmaxlen,
         cs->ismbchar,
         cs->ismbhead,
         cs->mbcharlen);
}


int main(int argc, char **argv) {
  const char *the_set = MYSQL_CHARSET;
  char *cs_list;
  int argcnt = 1;

  my_init();

  if (argc > argcnt && argv[argcnt][0] == '-' && argv[argcnt][1] == '#')
    DBUG_PUSH(argv[argcnt++]+2);

  if (argc > argcnt)
    the_set = argv[argcnt++];

  if (argc > argcnt)
    charsets_dir = argv[argcnt++];

  if (set_default_charset_by_name(the_set, MYF(MY_WME)))
    return 1;

  puts("CHARSET INFO:");
  _print_csinfo(default_charset_info);
  fflush(stdout);

  cs_list = list_charsets(MYF(MY_CS_COMPILED | MY_CS_CONFIG));
  printf("LIST OF CHARSETS (compiled + *.conf):\n%s\n", cs_list);
  my_free(cs_list,MYF(0));

  cs_list = list_charsets(MYF(MY_CS_INDEX | MY_CS_LOADED));
  printf("LIST OF CHARSETS (index + loaded):\n%s\n", cs_list);
  my_free(cs_list,MYF(0));

  return 0;
}
