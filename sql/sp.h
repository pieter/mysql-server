/* -*- C++ -*- */
/* Copyright (C) 2002 MySQL AB

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

#ifndef _SP_H_
#define _SP_H_

// Return codes from sp_create_*, sp_drop_*, and sp_show_*:
#define SP_OK                 0
#define SP_KEY_NOT_FOUND     -1
#define SP_OPEN_TABLE_FAILED -2
#define SP_WRITE_ROW_FAILED  -3
#define SP_DELETE_ROW_FAILED -4
#define SP_GET_FIELD_FAILED  -5
#define SP_PARSE_ERROR       -6
#define SP_INTERNAL_ERROR    -7
#define SP_NO_DB_ERROR       -8
#define SP_BAD_IDENTIFIER    -9
#define SP_BODY_TOO_LONG    -10

/* Drop all routines in database 'db' */
int
sp_drop_db_routines(THD *thd, char *db);

sp_head *
sp_find_procedure(THD *thd, sp_name *name, bool cache_only = 0);

int
sp_exists_routine(THD *thd, TABLE_LIST *procs, bool any, bool no_error);

int
sp_create_procedure(THD *thd, sp_head *sp);

int
sp_drop_procedure(THD *thd, sp_name *name);


int
sp_update_procedure(THD *thd, sp_name *name, st_sp_chistics *chistics);

int
sp_show_create_procedure(THD *thd, sp_name *name);

int
sp_show_status_procedure(THD *thd, const char *wild);

sp_head *
sp_find_function(THD *thd, sp_name *name, bool cache_only = 0);

int
sp_create_function(THD *thd, sp_head *sp);

int
sp_drop_function(THD *thd, sp_name *name);

int
sp_update_function(THD *thd, sp_name *name, st_sp_chistics *chistics);

int
sp_show_create_function(THD *thd, sp_name *name);

int
sp_show_status_function(THD *thd, const char *wild);


/*
  Procedures for pre-caching of stored routines and building table list
  for prelocking.
*/
void sp_get_prelocking_info(THD *thd, bool *need_prelocking, 
                            bool *first_no_prelocking);
void sp_add_used_routine(LEX *lex, Query_arena *arena,
                         sp_name *rt, char rt_type);
void sp_update_sp_used_routines(HASH *dst, HASH *src);
bool sp_cache_routines_and_add_tables(THD *thd, LEX *lex, 
                                      bool first_no_prelock);
void sp_cache_routines_and_add_tables_for_view(THD *thd, LEX *lex,
                                               LEX *aux_lex);
void sp_cache_routines_and_add_tables_for_triggers(THD *thd, LEX *lex,
                                         Table_triggers_list *triggers);

extern "C" byte* sp_sroutine_key(const byte *ptr, uint *plen, my_bool first);

/*
  Routines which allow open/lock and close mysql.proc table even when
  we already have some tables open and locked.
*/
TABLE *open_proc_table_for_read(THD *thd, Open_tables_state *backup);
void close_proc_table(THD *thd, Open_tables_state *backup);

//
// Utilities...
//

// Do a "use newdb". The current db is stored at olddb.
// If newdb is the same as the current one, nothing is changed.
// dbchangedp is set to true if the db was actually changed.
int
sp_use_new_db(THD *thd, char *newdb, char *olddb, uint olddbmax,
	      bool no_access_check, bool *dbchangedp);

#endif /* _SP_H_ */
