#ifndef _BACKUP_AUX_H
#define _BACKUP_AUX_H

#include <backup/api_types.h>
#include <backup_stream.h>

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

  LEX_STRING(byte *begin, byte *end)
  {
    str= (char*)begin;
    if( begin && end > begin)
      length= end - begin;
    else
      length= 0;
  }
};

class String: public ::String
{
 public:

  String(const ::String &s): ::String(s)
  {}

  String(const ::LEX_STRING &s):
    ::String(s.str,s.length,&::my_charset_bin) // FIXME: charset info
  {}

  String(byte *begin, byte *end):
    ::String((char*)begin,end-begin,&::my_charset_bin) // FIXME: charset info
  {
    if (!begin)
     set((char*)NULL,0,NULL);
  }

  String(const char *s):
    ::String(s,&::my_charset_bin)
  {}

  String(): ::String()
  {}
};


/*

  Dynamic_array<Foo> array;

  new (array.get_entry(7)) Foo(....);

  if (foo = array[7])
  {
    foo->...
  }

 TODO: Look at similar class in sql_array.h
*/

template<class X>
class Dynamic_array
{
   ::DYNAMIC_ARRAY m_array;

 public:

   Dynamic_array(uint init_size, uint alloc_increment)
   {
     my_init_dynamic_array(&m_array,1+sizeof(X),init_size,alloc_increment);
     clear_free_space();
   }

   ~Dynamic_array()
   {
     for (uint pos=0; pos < m_array.elements; ++pos)
     {
       X *ptr= const_cast<X*>((*this)[pos]);
       if (ptr)
         ptr->~X();
     }

     delete_dynamic(&m_array);
   }

   X* operator[](uint pos) const
   {
     if (pos >= m_array.elements)
       return NULL;

     uchar *ptr= dynamic_array_ptr(&m_array,pos);

     return *ptr == 0xFF ? (X*)(ptr+1) : NULL;
   }

   X* get_entry(uint pos)
   {
     uchar *entry;

     while (pos > m_array.max_element)
     {
       entry= alloc_dynamic(&m_array);
       if (!entry)
        break;
     }

     clear_free_space();

     if (pos > m_array.max_element)
      return NULL;

     if (pos >= m_array.elements)
       m_array.elements= pos+1;

     entry= dynamic_array_ptr(&m_array,pos);
     X *ptr= (X*)(entry+1);

     if (*entry == 0xFF)
       ptr->~X();

     *entry= 0xFF;
     return new (ptr) X();
   }

   uint size() const
   { return m_array.elements; }

 private:

   void clear_free_space()
   {
     uchar *start= dynamic_array_ptr(&m_array,m_array.elements);
     uchar *end= dynamic_array_ptr(&m_array,m_array.max_element);
     if (end > start)
       bzero(start, end - start);
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

// These functions are implemented in kernel.cc

int silent_exec_query(THD*, ::String&);
void save_current_time(bstream_time_t &buf);

} // backup namespace

#endif
