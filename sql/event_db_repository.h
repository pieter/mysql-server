#ifndef _EVENT_DB_REPOSITORY_H_
#define _EVENT_DB_REPOSITORY_H_
/* Copyright (C) 2004-2006 MySQL AB

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

enum enum_events_table_field
{
  ET_FIELD_DB = 0,
  ET_FIELD_NAME,
  ET_FIELD_BODY,
  ET_FIELD_DEFINER,
  ET_FIELD_EXECUTE_AT,
  ET_FIELD_INTERVAL_EXPR,
  ET_FIELD_TRANSIENT_INTERVAL,
  ET_FIELD_CREATED,
  ET_FIELD_MODIFIED,
  ET_FIELD_LAST_EXECUTED,
  ET_FIELD_STARTS,
  ET_FIELD_ENDS,
  ET_FIELD_STATUS,
  ET_FIELD_ON_COMPLETION,
  ET_FIELD_SQL_MODE,
  ET_FIELD_COMMENT,
  ET_FIELD_COUNT /* a cool trick to count the number of fields :) */
};


int
evex_db_find_event_by_name(THD *thd, const LEX_STRING dbname,
                           const LEX_STRING ev_name,
                           TABLE *table);

int
events_table_index_read_for_db(THD *thd, TABLE *schema_table,
                               TABLE *event_table);

int
events_table_scan_all(THD *thd, TABLE *schema_table, TABLE *event_table);

int
fill_schema_events(THD *thd, TABLE_LIST *tables, COND * /* cond */);


class Event_queue_element;

class Event_db_repository
{
public:
  Event_db_repository(){}
  ~Event_db_repository(){}

  int
  init_repository();

  void
  deinit_repository();

  int
  create_event(THD *thd, Event_timed *et, my_bool create_if_not,
              uint *rows_affected);

  int
  update_event(THD *thd, Event_timed *et, sp_name *new_name);

  int 
  drop_event(THD *thd, LEX_STRING db, LEX_STRING name, bool drop_if_exists,
             uint *rows_affected);

  int
  drop_schema_events(THD *thd, LEX_STRING schema);

  int
  drop_user_events(THD *thd, LEX_STRING definer);

  int
  find_event(THD *thd, sp_name *name, Event_timed **ett, TABLE *tbl,
             MEM_ROOT *root);

  int
  load_named_event(THD *thd, Event_timed *etn, Event_timed **etn_new);

  int
  find_event_by_name(THD *thd, LEX_STRING db, LEX_STRING name, TABLE *table);

  int
  open_event_table(THD *thd, enum thr_lock_type lock_type, TABLE **table);

  int
  fill_schema_events(THD *thd, TABLE_LIST *tables, char *db);

private:
  int
  drop_events_by_field(THD *thd, enum enum_events_table_field field,
                       LEX_STRING field_value);
  int
  index_read_for_db_for_i_s(THD *thd, TABLE *schema_table, TABLE *event_table,
                            char *db);

  int
  table_scan_all_for_i_s(THD *thd, TABLE *schema_table, TABLE *event_table);

  MEM_ROOT repo_root;

  /* Prevent use of these */
  Event_db_repository(const Event_db_repository &);
  void operator=(Event_db_repository &);
};
 
#endif /* _EVENT_DB_REPOSITORY_H_ */
