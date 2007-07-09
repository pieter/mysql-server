/* Copyright (C) 2000-2006 MySQL AB

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


/* Write some debug info */


#include "mysql_priv.h"
#include "sql_select.h"
#include <hash.h>
#include <thr_alarm.h>
#if defined(HAVE_MALLINFO) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#elif defined(HAVE_MALLINFO) && defined(HAVE_SYS_MALLOC_H)
#include <sys/malloc.h>
#endif

#include "events.h"

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


void print_cached_tables(void)
{
  uint idx,count,unused;
  TABLE *start_link,*lnk;

  /* purecov: begin tested */
  VOID(pthread_mutex_lock(&LOCK_open));
  puts("DB             Table                            Version  Thread  Open  Lock");

  for (idx=unused=0 ; idx < open_cache.records ; idx++)
  {
    TABLE *entry=(TABLE*) hash_element(&open_cache,idx);
    printf("%-14.14s %-32s%6ld%8ld%6d  %s\n",
           entry->s->db.str, entry->s->table_name.str, entry->s->version,
	   entry->in_use ? entry->in_use->thread_id : 0L,
	   entry->db_stat ? 1 : 0,
           entry->in_use ? lock_descriptions[(int)entry->reginfo.lock_type] : "Not in use");
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
  /* purecov: end */
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
	out.append(*sortorder->field->table_name);
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
    char key_map_buff[128];
    fprintf(DBUG_FILE,"%-16.16s  type: %-7s  q_keys: %s  refs: %d  key: %d  len: %d\n",
	    form->alias,
	    join_type_str[tab->type],
	    tab->keys.print(key_map_buff),
	    tab->ref.key_parts,
	    tab->ref.key,
	    tab->ref.key_length);
    if (tab->select)
    {
      char buf[MAX_KEY/8+1];
      if (tab->use_quick == 2)
	fprintf(DBUG_FILE,
		"                  quick select checked for each record (keys: %s)\n",
		tab->select->quick_keys.print(buf));
      else if (tab->select->quick)
      {
	fprintf(DBUG_FILE, "                  quick select used:\n");
        tab->select->quick->dbug_dump(18, FALSE);
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

#define FT_KEYPART   (MAX_REF_PARTS+10)

void print_keyuse(KEYUSE *keyuse)
{
  char buff[256];
  char buf2[64]; 
  const char *fieldname;
  String str(buff,(uint32) sizeof(buff), system_charset_info);
  str.length(0);
  keyuse->val->print(&str);
  str.append('\0');
  if (keyuse->keypart == FT_KEYPART)
    fieldname= "FT_KEYPART";
  else
    fieldname= keyuse->table->key_info[keyuse->key].key_part[keyuse->keypart].field->field_name;
  longlong2str(keyuse->used_tables, buf2, 16); 
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE, "KEYUSE: %s.%s=%s  optimize= %d used_tables=%s "
          "ref_table_rows= %lu keypart_map= %0lx\n",
          keyuse->table->alias, fieldname, str.ptr(),
          keyuse->optimize, buf2, (ulong)keyuse->ref_table_rows, 
          keyuse->keypart_map);
  DBUG_UNLOCK_FILE;
  //key_part_map keypart_map; --?? there can be several? 
}


void print_keyuse_array(DYNAMIC_ARRAY *keyuse_array)
{
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE, "KEYUSE array (%d elements)\n", keyuse_array->elements);
  DBUG_UNLOCK_FILE;
  for(uint i=0; i < keyuse_array->elements; i++)
    print_keyuse((KEYUSE*)dynamic_array_ptr(keyuse_array, i));
}


/* 
  Print the current state during query optimization.

  SYNOPSIS
    print_plan()
    join         pointer to the structure providing all context info for
                 the query
    read_time    the cost of the best partial plan
    record_count estimate for the number of records returned by the best
                 partial plan
    idx          length of the partial QEP in 'join->positions';
                 also an index in the array 'join->best_ref';
    info         comment string to appear above the printout

  DESCRIPTION
    This function prints to the log file DBUG_FILE the members of 'join' that
    are used during query optimization (join->positions, join->best_positions,
    and join->best_ref) and few other related variables (read_time,
    record_count).
    Useful to trace query optimizer functions.

  RETURN
    None
*/

void
print_plan(JOIN* join, uint idx, double record_count, double read_time,
           double current_read_time, const char *info)
{
  uint i;
  POSITION pos;
  JOIN_TAB *join_table;
  JOIN_TAB **plan_nodes;
  TABLE*   table;

  if (info == 0)
    info= "";

  DBUG_LOCK_FILE;
  if (join->best_read == DBL_MAX)
  {
    fprintf(DBUG_FILE,
            "%s; idx: %u  best: DBL_MAX  atime: %g  itime: %g  count: %g\n",
            info, idx, current_read_time, read_time, record_count);
  }
  else
  {
    fprintf(DBUG_FILE,
            "%s; idx :%u  best: %g  accumulated: %g  increment: %g  count: %g\n",
            info, idx, join->best_read, current_read_time, read_time,
            record_count);
  }

  /* Print the tables in JOIN->positions */
  fputs("     POSITIONS: ", DBUG_FILE);
  for (i= 0; i < idx ; i++)
  {
    pos = join->positions[i];
    table= pos.table->table;
    if (table)
      fputs(table->s->table_name.str, DBUG_FILE);
    fputc(' ', DBUG_FILE);
  }
  fputc('\n', DBUG_FILE);

  /*
    Print the tables in JOIN->best_positions only if at least one complete plan
    has been found. An indicator for this is the value of 'join->best_read'.
  */
  if (join->best_read < DBL_MAX)
  {
    fputs("BEST_POSITIONS: ", DBUG_FILE);
    for (i= 0; i < idx ; i++)
    {
      pos= join->best_positions[i];
      table= pos.table->table;
      if (table)
        fputs(table->s->table_name.str, DBUG_FILE);
      fputc(' ', DBUG_FILE);
    }
  }
  fputc('\n', DBUG_FILE);

  /* Print the tables in JOIN->best_ref */
  fputs("      BEST_REF: ", DBUG_FILE);
  for (plan_nodes= join->best_ref ; *plan_nodes ; plan_nodes++)
  {
    join_table= (*plan_nodes);
    fputs(join_table->table->s->table_name.str, DBUG_FILE);
    fprintf(DBUG_FILE, "(%lu,%lu,%lu)",
            (ulong) join_table->found_records,
            (ulong) join_table->records,
            (ulong) join_table->read_time);
    fputc(' ', DBUG_FILE);
  }
  fputc('\n', DBUG_FILE);

  DBUG_UNLOCK_FILE;
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


static void push_locks_into_array(DYNAMIC_ARRAY *ar, THR_LOCK_DATA *data,
				  bool wait, const char *text)
{
  if (data)
  {
    TABLE *table=(TABLE *)data->debug_print_param;
    if (table && table->s->tmp_table == NO_TMP_TABLE)
    {
      TABLE_LOCK_INFO table_lock_info;
      table_lock_info.thread_id= table->in_use->thread_id;
      memcpy(table_lock_info.table_name, table->s->table_cache_key.str,
	     table->s->table_cache_key.length);
      table_lock_info.table_name[strlen(table_lock_info.table_name)]='.';
      table_lock_info.waiting=wait;
      table_lock_info.lock_text=text;
      // lock_type is also obtainable from THR_LOCK_DATA
      table_lock_info.type=table->reginfo.lock_type;
      VOID(push_dynamic(ar,(uchar*) &table_lock_info));
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

static void display_table_locks(void) 
{
  LIST *list;
  DYNAMIC_ARRAY saved_table_locks;

  VOID(my_init_dynamic_array(&saved_table_locks,sizeof(TABLE_LOCK_INFO),open_cache.records + 20,50));
  VOID(pthread_mutex_lock(&THR_LOCK_lock));
  for (list= thr_lock_thread_list; list; list= list_rest(list))
  {
    THR_LOCK *lock=(THR_LOCK*) list->data;

    VOID(pthread_mutex_lock(&lock->mutex));
    push_locks_into_array(&saved_table_locks, lock->write.data, FALSE,
			  "Locked - write");
    push_locks_into_array(&saved_table_locks, lock->write_wait.data, TRUE,
			  "Waiting - write");
    push_locks_into_array(&saved_table_locks, lock->read.data, FALSE,
			  "Locked - read");
    push_locks_into_array(&saved_table_locks, lock->read_wait.data, TRUE,
			  "Waiting - read");
    VOID(pthread_mutex_unlock(&lock->mutex));
  }
  VOID(pthread_mutex_unlock(&THR_LOCK_lock));
  if (!saved_table_locks.elements) goto end;
  
  qsort((uchar*) dynamic_element(&saved_table_locks,0,TABLE_LOCK_INFO *),saved_table_locks.elements,sizeof(TABLE_LOCK_INFO),(qsort_cmp) dl_compare);
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


static int print_key_cache_status(const char *name, KEY_CACHE *key_cache)
{
  char llbuff1[22];
  char llbuff2[22];
  char llbuff3[22];
  char llbuff4[22];

  if (!key_cache->key_cache_inited)
  {
    printf("%s: Not in use\n", name);
  }
  else
  {
    printf("%s\n\
Buffer_size:    %10lu\n\
Block_size:     %10lu\n\
Division_limit: %10lu\n\
Age_limit:      %10lu\n\
blocks used:    %10lu\n\
not flushed:    %10lu\n\
w_requests:     %10s\n\
writes:         %10s\n\
r_requests:     %10s\n\
reads:          %10s\n\n",
	   name,
	   (ulong) key_cache->param_buff_size, key_cache->param_block_size,
	   key_cache->param_division_limit, key_cache->param_age_threshold,
	   key_cache->blocks_used,key_cache->global_blocks_changed,
	   llstr(key_cache->global_cache_w_requests,llbuff1),
           llstr(key_cache->global_cache_write,llbuff2),
	   llstr(key_cache->global_cache_r_requests,llbuff3),
           llstr(key_cache->global_cache_read,llbuff4));
  }
  return 0;
}


void mysql_print_status()
{
  char current_dir[FN_REFLEN];
  STATUS_VAR tmp;

  calc_sum_of_all_status(&tmp);
  printf("\nStatus information:\n\n");
  VOID(my_getwd(current_dir, sizeof(current_dir),MYF(0)));
  printf("Current dir: %s\n", current_dir);
  printf("Running threads: %d  Stack size: %ld\n", thread_count,
	 (long) thread_stack);
  thr_print_locks();				// Write some debug info
#ifndef DBUG_OFF
  print_cached_tables();
#endif
  /* Print key cache status */
  puts("\nKey caches:");
  process_key_caches(print_key_cache_status);
  pthread_mutex_lock(&LOCK_status);
  printf("\nhandler status:\n\
read_key:   %10lu\n\
read_next:  %10lu\n\
read_rnd    %10lu\n\
read_first: %10lu\n\
write:      %10lu\n\
delete      %10lu\n\
update:     %10lu\n",
	 tmp.ha_read_key_count,
	 tmp.ha_read_next_count,
	 tmp.ha_read_rnd_count,
	 tmp.ha_read_first_count,
	 tmp.ha_write_count,
	 tmp.ha_delete_count,
	 tmp.ha_update_count);
  pthread_mutex_unlock(&LOCK_status);
  printf("\nTable status:\n\
Opened tables: %10lu\n\
Open tables:   %10lu\n\
Open files:    %10lu\n\
Open streams:  %10lu\n",
	 tmp.opened_tables,
	 (ulong) cached_open_tables(),
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
  my_checkmalloc();
  fprintf(stdout,"\nBegin safemalloc memory dump:\n"); // tag needed for test suite
  TERMINATE(stdout);				// Write malloc information
  fprintf(stdout,"\nEnd safemalloc memory dump.\n");  

#ifdef HAVE_MALLINFO
  struct mallinfo info= mallinfo();
  printf("\nMemory status:\n\
Non-mmapped space allocated from system: %d\n\
Number of free chunks:			 %d\n\
Number of fastbin blocks:		 %d\n\
Number of mmapped regions:		 %d\n\
Space in mmapped regions:		 %d\n\
Maximum total allocated space:		 %d\n\
Space available in freed fastbin blocks: %d\n\
Total allocated space:			 %d\n\
Total free space:			 %d\n\
Top-most, releasable space:		 %d\n\
Estimated memory (with thread stack):    %ld\n",
	 (int) info.arena	,
	 (int) info.ordblks,
	 (int) info.smblks,
	 (int) info.hblks,
	 (int) info.hblkhd,
	 (int) info.usmblks,
	 (int) info.fsmblks,
	 (int) info.uordblks,
	 (int) info.fordblks,
	 (int) info.keepcost,
	 (long) (thread_count * thread_stack + info.hblkhd + info.arena));
#endif

  Events::dump_internal_status();
  puts("");
}


#ifndef DBUG_OFF
#ifdef EXTRA_DEBUG_DUMP_TABLE_LISTS


/*
  A fixed-size FIFO pointer queue that also doesn't allow one to put an
  element that has previously been put into it. 
  
  There is a hard-coded limit of the total number of queue put operations.
  The implementation is trivial and is intended for use in debug dumps only.
*/

template <class T> class Unique_fifo_queue
{
public:
  /* Add an element to the queue */
  void push_back(T *tbl)
  {
    if (!tbl)
      return;
    // check if we've already scheduled and/or dumped the element
    for (int i= 0; i < last; i++)
    {
      if (elems[i] == tbl)
        return;
    }
    elems[last++]=  tbl;
  }

  bool pop_first(T **elem)
  {
    if (first < last)
    {
      *elem= elems[first++];
      return TRUE;
    }
    return FALSE;
  }

  void reset()
  {
    first= last= 0;
  }
  enum { MAX_ELEMS=1000};
  T *elems[MAX_ELEMS];
  int first; // First undumped table
  int last;  // Last undumped element
};

class Dbug_table_list_dumper
{
  FILE *out;
  Unique_fifo_queue<TABLE_LIST> tables_fifo;
  Unique_fifo_queue<List<TABLE_LIST> > tbl_lists;
public:
  void dump_one_struct(TABLE_LIST *tbl);

  int dump_graph(st_select_lex *select_lex, TABLE_LIST *first_leaf);
};


void dump_TABLE_LIST_graph(SELECT_LEX *select_lex, TABLE_LIST* tl)
{
  Dbug_table_list_dumper dumper;
  dumper.dump_graph(select_lex, tl);
}


/* 
  - Dump one TABLE_LIST objects and its outgoing edges
  - Schedule that other objects seen along the edges are dumped too.
*/

void Dbug_table_list_dumper::dump_one_struct(TABLE_LIST *tbl)
{
  fprintf(out, "\"%p\" [\n", tbl);
  fprintf(out, "  label = \"%p|", tbl);
  fprintf(out, "alias=%s|", tbl->alias? tbl->alias : "NULL");
  fprintf(out, "<next_leaf>next_leaf=%p|", tbl->next_leaf);
  fprintf(out, "<next_local>next_local=%p|", tbl->next_local);
  fprintf(out, "<next_global>next_global=%p|", tbl->next_global);
  fprintf(out, "<embedding>embedding=%p", tbl->embedding);

  if (tbl->nested_join)
    fprintf(out, "|<nested_j>nested_j=%p", tbl->nested_join);
  if (tbl->join_list)
    fprintf(out, "|<join_list>join_list=%p", tbl->join_list);
  if (tbl->on_expr)
    fprintf(out, "|<on_expr>on_expr=%p", tbl->on_expr);
  fprintf(out, "\"\n");
  fprintf(out, "  shape = \"record\"\n];\n\n");
 
  if (tbl->next_leaf)
  {
    fprintf(out, "\n\"%p\":next_leaf -> \"%p\"[ color = \"#000000\" ];\n",  
            tbl, tbl->next_leaf);
    tables_fifo.push_back(tbl->next_leaf);
  }
  if (tbl->next_local)
  {
    fprintf(out, "\n\"%p\":next_local -> \"%p\"[ color = \"#404040\" ];\n",  
            tbl, tbl->next_local);
    tables_fifo.push_back(tbl->next_local);
  }
  if (tbl->next_global)
  {
    fprintf(out, "\n\"%p\":next_global -> \"%p\"[ color = \"#808080\" ];\n",  
            tbl, tbl->next_global);
    tables_fifo.push_back(tbl->next_global);
  }

  if (tbl->embedding)
  {
    fprintf(out, "\n\"%p\":embedding -> \"%p\"[ color = \"#FF0000\" ];\n",  
            tbl, tbl->embedding);
    tables_fifo.push_back(tbl->embedding);
  }

  if (tbl->join_list)
  {
    fprintf(out, "\n\"%p\":join_list -> \"%p\"[ color = \"#0000FF\" ];\n",  
            tbl, tbl->join_list);
    tbl_lists.push_back(tbl->join_list);
  }
}


int Dbug_table_list_dumper::dump_graph(st_select_lex *select_lex, 
                                       TABLE_LIST *first_leaf)
{
  DBUG_ENTER("Dbug_table_list_dumper::dump_graph");
  char filename[500];
  int no = 0;
  do
  {
    sprintf(filename, "tlist_tree%.3d.g", no);
    if ((out= fopen(filename, "rt")))
    {
      /* File exists, try next name */
      fclose(out);
    }
    no++;
  } while (out);
 
  /* Ok, found an unoccupied name, create the file */
  if (!(out= fopen(filename, "wt")))
  {
    DBUG_PRINT("tree_dump", ("Failed to create output file"));
    DBUG_RETURN(1);
  }
 
  DBUG_PRINT("tree_dump", ("dumping tree to %s", filename));
     
  fputs("digraph g {\n", out);
  fputs("graph [", out);
  fputs("  rankdir = \"LR\"", out);
  fputs("];", out);
   
  TABLE_LIST *tbl;
  tables_fifo.reset();
  dump_one_struct(first_leaf);   
  while (tables_fifo.pop_first(&tbl))
  {
    dump_one_struct(tbl);
  }

  List<TABLE_LIST> *plist;
  tbl_lists.push_back(&select_lex->top_join_list);
  while (tbl_lists.pop_first(&plist))
  {
    fprintf(out, "\"%p\" [\n", plist);
    fprintf(out, "  bgcolor = \"\"");
    fprintf(out, "  label = \"L %p\"", plist);
    fprintf(out, "  shape = \"record\"\n];\n\n");
  }

  fprintf(out, " { rank = same; ");
  for (TABLE_LIST *tl=first_leaf; tl; tl= tl->next_leaf)
    fprintf(out, " \"%p\"; ", tl);
  fprintf(out, "};\n");
  fputs("}", out);
  fclose(out);
 
  char filename2[500];
  filename[strlen(filename) - 1]= 0;
  filename[strlen(filename) - 1]= 0;
  sprintf(filename2, "%s.query", filename);
  
  if ((out= fopen(filename2, "wt")))
  {
    fprintf(out, "%s", current_thd->query);
    fclose(out);
  }
  DBUG_RETURN(0);
}

#endif

#endif 

