#ifndef _BACKUP_AUX_H
#define _BACKUP_AUX_H

#include <backup/api_types.h>

namespace backup {

/**
  Local version of LEX_STRING structure.

  Defines various constructors for convenience.
 */
struct LEX_STRING: public ::LEX_STRING
{
  LEX_STRING()
  {
    str= NULL;
    length= 0;
  }

  LEX_STRING(const ::LEX_STRING &s)
  {
    str= s.str;
    length= s.length;
  }

  LEX_STRING(const char *s)
  {
    str= const_cast<char*>(s);
    length= strlen(s);
  }

  LEX_STRING(const String &s)
  {
    str= const_cast<char*>(s.ptr());
    length= s.length();
  }
};


TABLE_LIST *build_table_list(const Table_list&,thr_lock_type);


/// Check if a database exists.

inline
bool db_exists(const Db_ref &db)
{
  //  This is how database existence is checked inside mysql_change_db().
  return ! ::check_db_dir_existence(db.name().ptr());
}

inline
bool change_db(THD *thd, const Db_ref &db)
{
  LEX_STRING db_name= db.name();

  return 0 == ::mysql_change_db(thd,&db_name,TRUE);
}

/*
  Free the memory for the table list.
*/
inline int free_table_list(TABLE_LIST *all_tables)
{
  if (all_tables)
  {
    TABLE_LIST *tbl= all_tables;
    TABLE_LIST *prev= tbl;
    while (tbl != NULL)
    {
      prev= tbl;
      tbl= tbl->next_global;
      my_free(prev, MYF(0));
    }
  }
  return 0;
}

} // backup namespace

#endif
