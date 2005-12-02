/* Copyright (C) 2004 MySQL AB

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


#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class interface */
#endif

#if !defined(TESTTIME) && !defined(TZINFO2SQL)

/*
  This class represents abstract time zone and provides 
  basic interface for TIME <-> my_time_t conversion.
  Actual time zones which are specified by DB, or via offset 
  or use system functions are its descendants.
*/
class Time_zone: public Sql_alloc 
{
public:
  /*
    Converts local time in broken down TIME representation to 
    my_time_t (UTC seconds since Epoch) represenation.
    Returns 0 in case of error. Sets in_dst_time_gap to true if date provided
    falls into spring time-gap (or lefts it untouched otherwise).
  */
  virtual my_time_t TIME_to_gmt_sec(const TIME *t, 
                                    my_bool *in_dst_time_gap) const = 0;
  /*
    Converts time in my_time_t representation to local time in
    broken down TIME representation.
  */
  virtual void   gmt_sec_to_TIME(TIME *tmp, my_time_t t) const = 0;
  /*
    Because of constness of String returned by get_name() time zone name 
    have to be already zeroended to be able to use String::ptr() instead
    of c_ptr().
  */
  virtual const String * get_name() const = 0;

  /* 
    We need this only for surpressing warnings, objects of this type are
    allocated on MEM_ROOT and should not require destruction.
  */
  virtual ~Time_zone() {};
};

extern Time_zone * my_tz_UTC;
extern Time_zone * my_tz_SYSTEM;
extern TABLE_LIST * my_tz_get_table_list(THD *thd, TABLE_LIST ***global_next_ptr);
extern Time_zone * my_tz_find(const String *name, TABLE_LIST *tz_tables);
extern Time_zone * my_tz_find_with_opening_tz_tables(THD *thd, const String *name);
extern my_bool     my_tz_init(THD *org_thd, const char *default_tzname, my_bool bootstrap);
extern void        my_tz_free();
my_time_t          sec_since_epoch_TIME(TIME *t);

extern TABLE_LIST fake_time_zone_tables_list;

/*
  Check if we have pointer to the begining of list of implicitly used time
  zone tables, set SELECT_ACL for them and fast-forward to its end.

  SYNOPSIS
    my_tz_check_n_skip_implicit_tables()
      table     - (in/out) pointer to element of table list to check
      tz_tables - list of implicitly used time zone tables received
                  from my_tz_get_table_list() function.

  NOTE
    This function relies on my_tz_get_table_list() implementation.

  RETURN VALUE
    TRUE  - if table points to the beggining of tz_tables list
    FALSE - otherwise.
*/
inline bool my_tz_check_n_skip_implicit_tables(TABLE_LIST **table,
                                               TABLE_LIST *tz_tables)
{
  if (*table == tz_tables)
  {
    for (int i= 0; i < 4; i++)
      (*table)[i].grant.privilege= SELECT_ACL;
    (*table)+= 3;
    return TRUE;
  }
  return FALSE;
}

#endif /* !defined(TESTTIME) && !defined(TZINFO2SQL) */
