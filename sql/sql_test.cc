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


/* Write some debug info */


#include "mysql_priv.h"
#include "sql_select.h"
#include <hash.h>
#include <thr_alarm.h>

/* Intern key cache variables */
extern "C" pthread_mutex_t THR_LOCK_keycache;

static const char *lock_descriptions[] =
{
  "No lock",
  "Low priority read lock",
  "Shared Read lock",
  "High priority read lock",
  "Read lock  without concurrent inserts",
  "Write lock that allows other writers",
  "Write lock, but allow reading",
  "Concurrent insert lock",
  "Lock Used by delayed insert",
  "Low priority write lock",
  "High priority write lock",
  "Highest priority write lock"
};
extern HASH open_cache;


#ifndef DBUG_OFF

void
print_where(COND *cond,const char *info)
{
  if (cond)
  {
    char buff[256];
    String str(buff,(uint32) sizeof(buff), system_charset_info);
    str.length(0);
    cond->print(&str);
    str.append('\0');
    DBUG_LOCK_FILE;
    (void) fprintf(DBUG_FILE,"\nWHERE:(%s) ",info);
    (void) fputs(str.ptr(),DBUG_FILE);
    (void) fputc('\n',DBUG_FILE);
    DBUG_UNLOCK_FILE;
  }
}
	/* This is for debugging purposes */

extern TABLE *unused_tables;

void print_cached_tables(void)
{
  uint idx,count,unused;
  TABLE *start_link,*lnk;

  VOID(pthread_mutex_lock(&LOCK_open));
  puts("DB             Table                            Version  Thread  L.thread  Open  Lock");

  for (idx=unused=0 ; idx < open_cache.records ; idx++)
  {
    TABLE *entry=(TABLE*) hash_element(&open_cache,idx);
    printf("%-14.14s %-32s%6ld%8ld%10ld%6d  %s\n",
	   entry->table_cache_key,entry->real_name,entry->version,
	   entry->in_use ? entry->in_use->thread_id : 0L,
	   entry->in_use ? entry->in_use->dbug_thread_id : 0L,
	   entry->db_stat ? 1 : 0, entry->in_use ? lock_descriptions[(int)entry->reginfo.lock_type] : "Not in use");
    if (!entry->in_use)
      unused++;
  }
  count=0;
  if ((start_link=lnk=unused_tables))
  {
    do
    {
      if (lnk != lnk->next->prev || lnk != lnk->prev->next)
      {
	printf("unused_links isn't linked properly\n");
	return;
      }
    } while (count++ < open_cache.records && (lnk=lnk->next) != start_link);
    if (lnk != start_link)
    {
      printf("Unused_links aren't connected\n");
    }
  }
  if (count != unused)
    printf("Unused_links (%d) doesn't match open_cache: %d\n", count,unused);
  printf("\nCurrent refresh version: %ld\n",refresh_version);
  if (hash_check(&open_cache))
    printf("Error: File hash table is corrupted\n");
  fflush(stdout);
  VOID(pthread_mutex_unlock(&LOCK_open));
  return;
}


void TEST_filesort(SORT_FIELD *sortorder,uint s_length)
{
  char buff[256],buff2[256];
  String str(buff,sizeof(buff),system_charset_info);
  String out(buff2,sizeof(buff2),system_charset_info);
  const char *sep;
  DBUG_ENTER("TEST_filesort");

  out.length(0);
  for (sep=""; s_length-- ; sortorder++, sep=" ")
  {
    out.append(sep);
    if (sortorder->reverse)
      out.append('-');
    if (sortorder->field)
    {
      if (sortorder->field->table_name)
      {
	out.append(sortorder->field->table_name);
	out.append('.');
      }
      out.append(sortorder->field->field_name ? sortorder->field->field_name:
		 "tmp_table_column");
    }
    else
    {
      str.length(0);
      sortorder->item->print(&str);
      out.append(str);
    }
  }
  out.append('\0');				// Purify doesn't like c_ptr()
  DBUG_LOCK_FILE;
  VOID(fputs("\nInfo about FILESORT\n",DBUG_FILE));
  fprintf(DBUG_FILE,"Sortorder: %s\n",out.ptr());
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}


void
TEST_join(JOIN *join)
{
  uint i,ref;
  DBUG_ENTER("TEST_join");

  DBUG_LOCK_FILE;
  VOID(fputs("\nInfo about JOIN\n",DBUG_FILE));
  for (i=0 ; i < join->tables ; i++)
  {
    JOIN_TAB *tab=join->join_tab+i;
    TABLE *form=tab->table;
    fprintf(DBUG_FILE,"%-16.16s  type: %-7s  q_keys: %4d  refs: %d  key: %d  len: %d\n",
	    form->table_name,
	    join_type_str[tab->type],
	    tab->keys,
	    tab->ref.key_parts,
	    tab->ref.key,
	    tab->ref.key_length);
    if (tab->select)
    {
      if (tab->use_quick == 2)
	fprintf(DBUG_FILE,
		"                  quick select checked for each record (keys: %d)\n",
		(int) tab->select->quick_keys);
      else if (tab->select->quick)
      {
        int quick_type= tab->select->quick->get_type();
        if ((quick_type == QUICK_SELECT_I::QS_TYPE_RANGE) || 
            (quick_type == QUICK_SELECT_I::QS_TYPE_RANGE_DESC))
        {
	  fprintf(DBUG_FILE,
                "                  quick select used on key %s, length: %d\n",
		form->key_info[tab->select->quick->index].name,
		tab->select->quick->max_used_key_length);
        }
        else if (quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE)
        {
          QUICK_INDEX_MERGE_SELECT *quick_imerge= 
              (QUICK_INDEX_MERGE_SELECT*)tab->select->quick;
          QUICK_RANGE_SELECT *quick;
          fprintf(DBUG_FILE,
                  "                  index_merge quick select used\n");
                    
          List_iterator_fast<QUICK_RANGE_SELECT> it(quick_imerge->quick_selects);
          while ((quick = it++))
          {
  	    fprintf(DBUG_FILE,
                "                  range quick select: key %s, length: %d\n",
		form->key_info[quick->index].name,
		quick->max_used_key_length);
          }
        }
        else
        {
          fprintf(DBUG_FILE,
                  "                  quick select of unknown nature used\n");
        }
      }
      else
	VOID(fputs("                  select used\n",DBUG_FILE));
    }
    if (tab->ref.key_parts)
    {
      VOID(fputs("                  refs: ",DBUG_FILE));
      for (ref=0 ; ref < tab->ref.key_parts ; ref++)
      {
	Item *item=tab->ref.items[ref];
	fprintf(DBUG_FILE,"%s  ", item->full_name());
      }
      VOID(fputc('\n',DBUG_FILE));
    }
  }
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}

#endif

typedef struct st_debug_lock
{
  ulong thread_id;
  char table_name[FN_REFLEN];
  bool waiting;
  const char *lock_text;
  enum thr_lock_type type;
} TABLE_LOCK_INFO;

static int dl_compare(TABLE_LOCK_INFO *a,TABLE_LOCK_INFO *b)
{
  if (a->thread_id > b->thread_id)
    return 1;
  if (a->thread_id < b->thread_id)
    return -1;
  if (a->waiting == b->waiting)
    return 0;
  else if (a->waiting)
    return -1;
  return 1;
}

static void push_locks_into_array(DYNAMIC_ARRAY *ar, THR_LOCK_DATA *data, bool wait, const char *text)
{
  if (data)
  {
    TABLE *table=(TABLE *)data->debug_print_param;
    if (table && table->tmp_table == NO_TMP_TABLE)
    {
      TABLE_LOCK_INFO table_lock_info;
      table_lock_info.thread_id=table->in_use->thread_id;
      memcpy(table_lock_info.table_name, table->table_cache_key, table->key_length);
      table_lock_info.table_name[strlen(table_lock_info.table_name)]='.';
      table_lock_info.waiting=wait;
      table_lock_info.lock_text=text;
      table_lock_info.type=table->reginfo.lock_type; // obtainable also from THR_LOCK_DATA
      VOID(push_dynamic(ar,(gptr) &table_lock_info));
    }
  }
}
/* 
   Regarding MERGE tables:

For now, the best option is to use the common TABLE *pointer for all
cases;  The drawback is that for MERGE tables we will see many locks
for the merge tables even if some of them are for individual tables.

The way to solve this is to add to 'THR_LOCK' structure a pointer to
the filename and use this when printing the data.
(We can for now ignore this and just print the same name for all merge
table parts;  Please add the above as a comment to the display_lock
function so that we can easily add this if we ever need this.

*/

static void display_table_locks (void) 
{
  LIST *list;
  DYNAMIC_ARRAY saved_table_locks;

  VOID(my_init_dynamic_array(&saved_table_locks,sizeof(TABLE_LOCK_INFO),open_cache.records + 20,50));
  VOID(pthread_mutex_lock(&THR_LOCK_lock));
  for (list=thr_lock_thread_list ; list ; list=rest(list))
  {
    THR_LOCK *lock=(THR_LOCK*) list->data;

    VOID(pthread_mutex_lock(&lock->mutex));
    push_locks_into_array(&saved_table_locks, lock->write.data, false, "Locked - write");
    push_locks_into_array(&saved_table_locks, lock->write_wait.data, true, "Waiting - write");
    push_locks_into_array(&saved_table_locks, lock->read.data, false, "Locked - read");
    push_locks_into_array(&saved_table_locks, lock->read_wait.data, true, "Waiting - read");
    VOID(pthread_mutex_unlock(&lock->mutex));
  }
  VOID(pthread_mutex_unlock(&THR_LOCK_lock));
  if (!saved_table_locks.elements) goto end;
  
  qsort((gptr) dynamic_element(&saved_table_locks,0,TABLE_LOCK_INFO *),saved_table_locks.elements,sizeof(TABLE_LOCK_INFO),(qsort_cmp) dl_compare);
  freeze_size(&saved_table_locks);

  puts("\nThread database.table_name          Locked/Waiting        Lock_type\n");
  
  unsigned int i;
  for (i=0 ; i < saved_table_locks.elements ; i++)
  {
    TABLE_LOCK_INFO *dl_ptr=dynamic_element(&saved_table_locks,i,TABLE_LOCK_INFO*);
    printf("%-8ld%-28.28s%-22s%s\n",
	   dl_ptr->thread_id,dl_ptr->table_name,dl_ptr->lock_text,lock_descriptions[(int)dl_ptr->type]);
  }
  puts("\n\n");
end:
  delete_dynamic(&saved_table_locks);
}


void mysql_print_status(THD *thd)
{
  char current_dir[FN_REFLEN];
  printf("\nStatus information:\n\n");
  my_getwd(current_dir, sizeof(current_dir),MYF(0));
  printf("Current dir: %s\n", current_dir);
  if (thd)
    thd->proc_info="locks";
  thr_print_locks();				// Write some debug info
#ifndef DBUG_OFF
  if (thd)
    thd->proc_info="table cache";
  print_cached_tables();
#endif
  /* Print key cache status */
  if (thd)
    thd->proc_info="key cache";
  pthread_mutex_lock(&THR_LOCK_keycache);
  printf("key_cache status:\n\
blocks used:%10lu\n\
not flushed:%10lu\n\
w_requests: %10lu\n\
writes:     %10lu\n\
r_requests: %10lu\n\
reads:      %10lu\n",
	 my_blocks_used,my_blocks_changed,my_cache_w_requests,
	 my_cache_write,my_cache_r_requests,my_cache_read);
  pthread_mutex_unlock(&THR_LOCK_keycache);

  if (thd)
    thd->proc_info="status";
  pthread_mutex_lock(&LOCK_status);
  printf("\nhandler status:\n\
read_key:   %10lu\n\
read_next:  %10lu\n\
read_rnd    %10lu\n\
read_first: %10lu\n\
write:      %10lu\n\
delete      %10lu\n\
update:     %10lu\n",
	 ha_read_key_count, ha_read_next_count,
	 ha_read_rnd_count, ha_read_first_count,
	 ha_write_count, ha_delete_count, ha_update_count);
  pthread_mutex_unlock(&LOCK_status);
  printf("\nTable status:\n\
Opened tables: %10lu\n\
Open tables:   %10lu\n\
Open files:    %10lu\n\
Open streams:  %10lu\n",
	 opened_tables,
	 (ulong) cached_tables(),
	 (ulong) my_file_opened,
	 (ulong) my_stream_opened);

  ALARM_INFO alarm_info;
#ifndef DONT_USE_THR_ALARM
  thr_alarm_info(&alarm_info);
  printf("\nAlarm status:\n\
Active alarms:   %u\n\
Max used alarms: %u\n\
Next alarm time: %lu\n",
	 alarm_info.active_alarms,
	 alarm_info.max_used_alarms,
	 alarm_info.next_alarm_time);
#endif
  display_table_locks();
  fflush(stdout);
  if (thd)
    thd->proc_info="malloc";
  my_checkmalloc();
  TERMINATE(stdout);				// Write malloc information
  if (thd)
    thd->proc_info=0;
}
