/* Copyright (C) 2005 MySQL AB

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

#ifdef __GNUC__
#pragma interface				/* gcc class implementation */
#endif

/* Flags for partition handlers */
#define HA_CAN_PARTITION       (1 << 0) /* Partition support */
#define HA_CAN_UPDATE_PARTITION_KEY (1 << 1)
#define HA_CAN_PARTITION_UNIQUE (1 << 2)
#define HA_USE_AUTO_PARTITION (1 << 3)

/*
  HA_PARTITION_FUNCTION_SUPPORTED indicates that the function is
  supported at all.
  HA_FAST_CHANGE_PARTITION means that optimised variants of the changes
  exists but they are not necessarily done online.

  HA_ONLINE_DOUBLE_WRITE means that the handler supports writing to both
  the new partition and to the old partitions when updating through the
  old partitioning schema while performing a change of the partitioning.
  This means that we can support updating of the table while performing
  the copy phase of the change. For no lock at all also a double write
  from new to old must exist and this is not required when this flag is
  set.
  This is actually removed even before it was introduced the first time.
  The new idea is that handlers will handle the lock level already in
  store_lock for ALTER TABLE partitions.

  HA_PARTITION_ONE_PHASE is a flag that can be set by handlers that take
  care of changing the partitions online and in one phase. Thus all phases
  needed to handle the change are implemented inside the storage engine.
  The storage engine must also support auto-discovery since the frm file
  is changed as part of the change and this change must be controlled by
  the storage engine. A typical engine to support this is NDB (through
  WL #2498).
*/
#define HA_PARTITION_FUNCTION_SUPPORTED         (1L << 12)
#define HA_FAST_CHANGE_PARTITION                (1L << 13)
#define HA_PARTITION_ONE_PHASE                  (1L << 14)

/*typedef struct {
  ulonglong data_file_length;
  ulonglong max_data_file_length;
  ulonglong index_file_length;
  ulonglong delete_length;
  ha_rows records;
  ulong mean_rec_length;
  time_t create_time;
  time_t check_time;
  time_t update_time;
  ulonglong check_sum;
} PARTITION_INFO;
*/
typedef struct {
  longlong list_value;
  uint32 partition_id;
} LIST_PART_ENTRY;

typedef struct {
  uint32 start_part;
  uint32 end_part;
} part_id_range;

struct st_partition_iter;
#define NOT_A_PARTITION_ID ((uint32)-1)

bool is_partition_in_list(char *part_name, List<char> list_part_names);
char *are_partitions_in_table(partition_info *new_part_info,
                              partition_info *old_part_info);
bool check_reorganise_list(partition_info *new_part_info,
                           partition_info *old_part_info,
                           List<char> list_part_names);
handler *get_ha_partition(partition_info *part_info);
int get_parts_for_update(const byte *old_data, byte *new_data,
                         const byte *rec0, partition_info *part_info,
                         uint32 *old_part_id, uint32 *new_part_id,
                         longlong *func_value);
int get_part_for_delete(const byte *buf, const byte *rec0,
                        partition_info *part_info, uint32 *part_id);
void prune_partition_set(const TABLE *table, part_id_range *part_spec);
bool check_partition_info(partition_info *part_info,handlerton **eng_type,
                          handler *file, ulonglong max_rows);
bool fix_partition_func(THD *thd, const char *name, TABLE *table,
                        bool create_table_ind);
char *generate_partition_syntax(partition_info *part_info,
                                uint *buf_length, bool use_sql_alloc,
                                bool write_all);
bool partition_key_modified(TABLE *table, List<Item> &fields);
void get_partition_set(const TABLE *table, byte *buf, const uint index,
                       const key_range *key_spec,
                       part_id_range *part_spec);
void get_full_part_id_from_key(const TABLE *table, byte *buf,
                               KEY *key_info,
                               const key_range *key_spec,
                               part_id_range *part_spec);
bool mysql_unpack_partition(THD *thd, const uchar *part_buf,
                            uint part_info_len,
                            uchar *part_state, uint part_state_len,
                            TABLE *table, bool is_create_table_ind,
                            handlerton *default_db_type);
void make_used_partitions_str(partition_info *part_info, String *parts_str);
uint32 get_list_array_idx_for_endpoint(partition_info *part_info,
                                       bool left_endpoint,
                                       bool include_endpoint);
uint32 get_partition_id_range_for_endpoint(partition_info *part_info,
                                           bool left_endpoint,
                                           bool include_endpoint);

/*
  A "Get next" function for partition iterator.
  SYNOPSIS
    partition_iter_func()
      part_iter  Partition iterator, you call only "iter.get_next(&iter)"

  RETURN 
    NOT_A_PARTITION_ID if there are no more partitions.
    [sub]partition_id  of the next partition
*/

typedef uint32 (*partition_iter_func)(st_partition_iter* part_iter);


/*
  Partition set iterator. Used to enumerate a set of [sub]partitions
  obtained in partition interval analysis (see get_partitions_in_range_iter).

  For the user, the only meaningful field is get_next, which may be used as
  follows:
             part_iterator.get_next(&part_iterator);
  
  Initialization is done by any of the following calls:
    - get_partitions_in_range_iter-type function call
    - init_single_partition_iterator()
    - init_all_partitions_iterator()
  Cleanup is not needed.
*/

typedef struct st_partition_iter
{
  partition_iter_func get_next;
  
  struct st_part_num_range
  {
    uint32 start;
    uint32 end;
  };

  struct st_field_value_range
  {
    longlong start;
    longlong end;
  };

  union
  {
    struct st_part_num_range     part_nums;
    struct st_field_value_range  field_vals;
  };
  partition_info *part_info;
} PARTITION_ITERATOR;


/*
  Get an iterator for set of partitions that match given field-space interval

  SYNOPSIS
    get_partitions_in_range_iter()
      part_info   Partitioning info
      is_subpart  
      min_val     Left edge,  field value in opt_range_key format.
      max_val     Right edge, field value in opt_range_key format. 
      flags       Some combination of NEAR_MIN, NEAR_MAX, NO_MIN_RANGE,
                  NO_MAX_RANGE.
      part_iter   Iterator structure to be initialized

  DESCRIPTION
    Functions with this signature are used to perform "Partitioning Interval
    Analysis". This analysis is applicable for any type of [sub]partitioning 
    by some function of a single fieldX. The idea is as follows:
    Given an interval "const1 <=? fieldX <=? const2", find a set of partitions
    that may contain records with value of fieldX within the given interval.

    The min_val, max_val and flags parameters specify the interval.
    The set of partitions is returned by initializing an iterator in *part_iter

  NOTES
    There are currently two functions of this type:
     - get_part_iter_for_interval_via_walking
     - get_part_iter_for_interval_via_mapping

  RETURN 
    0 - No matching partitions, iterator not initialized
    1 - Some partitions would match, iterator intialized for traversing them
   -1 - All partitions would match, iterator not initialized
*/

typedef int (*get_partitions_in_range_iter)(partition_info *part_info,
                                            bool is_subpart,
                                            char *min_val, char *max_val,
                                            uint flags,
                                            PARTITION_ITERATOR *part_iter);

#include "partition_info.h"

