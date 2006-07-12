#ifndef _EVENT_QUEUE_H_
#define _EVENT_QUEUE_H_
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

class Event_basic;
class Event_db_repository;
class Event_job_data;
class Event_queue_element;

class THD;
class Event_scheduler;

class Event_queue
{
public:
  Event_queue();

  void
  init_mutexes();
  
  void
  deinit_mutexes();
  
  bool
  init_queue(Event_db_repository *db_repo, Event_scheduler *sched);
  
  void
  deinit_queue();

  /* Methods for queue management follow */

  int
  create_event(THD *thd, LEX_STRING dbname, LEX_STRING name);

  int
  update_event(THD *thd, LEX_STRING dbname, LEX_STRING name,
               LEX_STRING *new_schema, LEX_STRING *new_name);

  void
  drop_event(THD *thd, LEX_STRING dbname, LEX_STRING name);

  void
  drop_schema_events(THD *thd, LEX_STRING schema);

  static bool
  check_system_tables(THD *thd);

  void
  recalculate_activation_times(THD *thd);

  bool
  get_top_for_execution_if_time(THD *thd, time_t now, Event_job_data **job_data,
                                struct timespec *abstime);
  bool
  dump_internal_status(THD *thd);

protected:
  Event_queue_element *
  find_n_remove_event(LEX_STRING db, LEX_STRING name);

  int
  load_events_from_db(THD *thd);

  void
  drop_matching_events(THD *thd, LEX_STRING pattern,
                       bool (*)(LEX_STRING, Event_basic *));

  void
  empty_queue();

  /* LOCK_event_queue is the mutex which protects the access to the queue. */
  pthread_mutex_t LOCK_event_queue;

  Event_db_repository *db_repository;

  uint mutex_last_locked_at_line;
  uint mutex_last_unlocked_at_line;
  uint mutex_last_attempted_lock_at_line;
  const char* mutex_last_locked_in_func;
  const char* mutex_last_unlocked_in_func;
  const char* mutex_last_attempted_lock_in_func;
  bool mutex_queue_data_locked;
  bool mutex_queue_data_attempting_lock;

  /* helper functions for working with mutexes & conditionals */
  void
  lock_data(const char *func, uint line);

  void
  unlock_data(const char *func, uint line);

  void
  notify_observers();

  void
  dbug_dump_queue(time_t now);

  Event_scheduler *scheduler;

  /* The sorted queue with the Event_job_data objects */
  QUEUE queue;
};

#endif /* _EVENT_QUEUE_H_ */
