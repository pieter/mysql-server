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


/* This struct includes all reserved words and functions */

#ifndef _lex_symbol_h
#define _lex_symbol_h

struct st_sym_group;

typedef struct st_symbol {
  const char *name;
  uint	tok;
  uint length;
  struct st_sym_group *group;
} SYMBOL;

typedef struct st_lex_symbol
{
  SYMBOL *symbol;
  char   *str;
  uint   length;
} LEX_SYMBOL;

typedef struct st_sym_group {
  const char *name;
  const char *needed_define;
} SYM_GROUP;

extern SYM_GROUP sym_group_common;
extern SYM_GROUP sym_group_geom;
extern SYM_GROUP sym_group_rtree;

#endif /* _lex_symbol_h */
