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

/*
  PARTITION_SHARE is a structure that will be shared amoung all open handlers
  The partition implements the minimum of what you will probably need.
*/

typedef struct st_partition_share
{
  char *table_name;
  uint table_name_length, use_count;
  pthread_mutex_t mutex;
  THR_LOCK lock;
} PARTITION_SHARE;


#define PARTITION_BYTES_IN_POS 2
class ha_partition :public handler
{
private:
  enum partition_index_scan_type
  {
    partition_index_read= 0,
    partition_index_first= 1,
    partition_index_last= 2,
    partition_no_index_scan= 3
  };
  /* Data for the partition handler */
  char *m_file_buffer;                  // Buffer with names
  char *m_name_buffer_ptr;		// Pointer to first partition name
  handlerton **m_engine_array;          // Array of types of the handlers
  handler **m_file;                     // Array of references to handler inst.
  partition_info *m_part_info;          // local reference to partition
  byte *m_start_key_ref;                // Reference of start key in current
                                        // index scan info
  Field **m_part_field_array;           // Part field array locally to save acc
  byte *m_ordered_rec_buffer;           // Row and key buffer for ord. idx scan
  KEY *m_curr_key_info;                 // Current index
  byte *m_rec0;                         // table->record[0]
  QUEUE queue;                          // Prio queue used by sorted read
  /*
    Since the partition handler is a handler on top of other handlers, it
    is necessary to keep information about what the underlying handler
    characteristics is. It is not possible to keep any handler instances
    for this since the MySQL Server sometimes allocating the handler object
    without freeing them.
  */
  u_long m_table_flags;
  u_long m_low_byte_first;

  uint m_tot_parts;                      // Total number of partitions;
  uint m_no_locks;                        // For engines like ha_blackhole, which needs no locks
  uint m_last_part;                      // Last file that we update,write
  int m_lock_type;                       // Remembers type of last
                                         // external_lock
  part_id_range m_part_spec;             // Which parts to scan
  uint m_scan_value;                     // Value passed in rnd_init
                                         // call
  uint m_ref_length;                     // Length of position in this
                                         // handler object
  key_range m_start_key;                 // index read key range
  enum partition_index_scan_type m_index_scan_type;// What type of index
                                                   // scan
  uint m_top_entry;                      // Which partition is to
                                         // deliver next result
  uint m_rec_length;                     // Local copy of record length

  bool m_ordered;                        // Ordered/Unordered index scan
  bool m_has_transactions;               // Can we support transactions
  bool m_pkey_is_clustered;              // Is primary key clustered
  bool m_create_handler;                 // Handler used to create table
  bool m_is_sub_partitioned;             // Is subpartitioned
  bool m_ordered_scan_ongoing;
  bool m_use_bit_array;

  /*
    We keep track if all underlying handlers are MyISAM since MyISAM has a
    great number of extra flags not needed by other handlers.
  */
  bool m_myisam;                         // Are all underlying handlers
                                         // MyISAM
  /*
    We keep track of InnoDB handlers below since it requires proper setting
    of query_id in fields at index_init and index_read calls.
  */
  bool m_innodb;                        // Are all underlying handlers
                                        // InnoDB
  /*
    When calling extra(HA_EXTRA_CACHE) we do not pass this to the underlying
    handlers immediately. Instead we cache it and call the underlying
    immediately before starting the scan on the partition. This is to
    prevent allocating a READ CACHE for each partition in parallel when
    performing a full table scan on MyISAM partitioned table.
    This state is cleared by extra(HA_EXTRA_NO_CACHE).
  */
  bool m_extra_cache;
  uint m_extra_cache_size;

  void init_handler_variables();
  /*
    Variables for lock structures.
  */
  THR_LOCK_DATA lock;                   /* MySQL lock */
  PARTITION_SHARE *share;               /* Shared lock info */

public:
  virtual void set_part_info(partition_info *part_info)
  {
     m_part_info= part_info;
     m_is_sub_partitioned= is_sub_partitioned(part_info);
  }
  /*
    -------------------------------------------------------------------------
    MODULE create/delete handler object
    -------------------------------------------------------------------------
    Object create/delete methode. The normal called when a table object
    exists. There is also a method to create the handler object with only
    partition information. This is used from mysql_create_table when the
    table is to be created and the engine type is deduced to be the
    partition handler.
    -------------------------------------------------------------------------
  */
    ha_partition(TABLE_SHARE * table);
    ha_partition(partition_info * part_info);
   ~ha_partition();
  /*
    A partition handler has no characteristics in itself. It only inherits
    those from the underlying handlers. Here we set-up those constants to
    enable later calls of the methods to retrieve constants from the under-
    lying handlers. Returns false if not successful.
  */
  int ha_initialise();

  /*
    -------------------------------------------------------------------------
    MODULE meta data changes
    -------------------------------------------------------------------------
    Meta data routines to CREATE, DROP, RENAME table and often used at
    ALTER TABLE (update_create_info used from ALTER TABLE and SHOW ..).

    update_table_comment is used in SHOW TABLE commands to provide a
    chance for the handler to add any interesting comments to the table
    comments not provided by the users comment.

    create_handler_files is called before opening a new handler object
    with openfrm to call create. It is used to create any local handler
    object needed in opening the object in openfrm
    -------------------------------------------------------------------------
  */
  virtual int delete_table(const char *from);
  virtual int rename_table(const char *from, const char *to);
  virtual int create(const char *name, TABLE * form,
		     HA_CREATE_INFO * create_info);
  virtual int create_handler_files(const char *name);
  virtual void update_create_info(HA_CREATE_INFO * create_info);
  virtual char *update_table_comment(const char *comment);
  virtual int drop_partitions(const char *path);
private:
  /*
    delete_table, rename_table and create uses very similar logic which
    is packed into this routine.
  */
    uint del_ren_cre_table(const char *from,
			   const char *to= NULL,
			   TABLE * table_arg= NULL,
			   HA_CREATE_INFO * create_info= NULL);
  /*
    One method to create the table_name.par file containing the names of the
    underlying partitions, their engine and the number of partitions.
    And one method to read it in.
  */
  bool create_handler_file(const char *name);
  bool get_from_handler_file(const char *name);
  bool new_handlers_from_part_info();
  bool create_handlers();
  void clear_handler_file();
  void set_up_table_before_create(TABLE * table_arg, HA_CREATE_INFO * info,
				  uint part_id);
  partition_element *find_partition_element(uint part_id);
public:

  /*
    -------------------------------------------------------------------------
    MODULE open/close object
    -------------------------------------------------------------------------
    Open and close handler object to ensure all underlying files and
    objects allocated and deallocated for query handling is handled
    properly.
    -------------------------------------------------------------------------

    A handler object is opened as part of its initialisation and before
    being used for normal queries (not before meta-data changes always.
    If the object was opened it will also be closed before being deleted.
  */
  virtual int open(const char *name, int mode, uint test_if_locked);
  virtual int close(void);

  /*
    -------------------------------------------------------------------------
    MODULE start/end statement
    -------------------------------------------------------------------------
    This module contains methods that are used to understand start/end of
    statements, transaction boundaries, and aid for proper concurrency
    control.
    The partition handler need not implement abort and commit since this
    will be handled by any underlying handlers implementing transactions.
    There is only one call to each handler type involved per transaction
    and these go directly to the handlers supporting transactions
    currently InnoDB, BDB and NDB).
    -------------------------------------------------------------------------
  */
  virtual THR_LOCK_DATA **store_lock(THD * thd, THR_LOCK_DATA ** to,
				     enum thr_lock_type lock_type);
  virtual int external_lock(THD * thd, int lock_type);
  /*
    When table is locked a statement is started by calling start_stmt
    instead of external_lock
  */
  virtual int start_stmt(THD * thd, thr_lock_type lock_type);
  /*
    Lock count is number of locked underlying handlers (I assume)
  */
  virtual uint lock_count(void) const;
  /*
    Call to unlock rows not to be updated in transaction
  */
  virtual void unlock_row();

  /*
    -------------------------------------------------------------------------
    MODULE change record
    -------------------------------------------------------------------------
    This part of the handler interface is used to change the records
    after INSERT, DELETE, UPDATE, REPLACE method calls but also other
    special meta-data operations as ALTER TABLE, LOAD DATA, TRUNCATE.
    -------------------------------------------------------------------------

    These methods are used for insert (write_row), update (update_row)
    and delete (delete_row). All methods to change data always work on
    one row at a time. update_row and delete_row also contains the old
    row.
    delete_all_rows will delete all rows in the table in one call as a
    special optimisation for DELETE from table;

    Bulk inserts are supported if all underlying handlers support it.
    start_bulk_insert and end_bulk_insert is called before and after a
    number of calls to write_row.
    Not yet though.
  */
  virtual int write_row(byte * buf);
  virtual int update_row(const byte * old_data, byte * new_data);
  virtual int delete_row(const byte * buf);
  virtual int delete_all_rows(void);
  virtual void start_bulk_insert(ha_rows rows);
  virtual int end_bulk_insert();

  /*
    -------------------------------------------------------------------------
    MODULE full table scan
    -------------------------------------------------------------------------
    This module is used for the most basic access method for any table
    handler. This is to fetch all data through a full table scan. No
    indexes are needed to implement this part.
    It contains one method to start the scan (rnd_init) that can also be
    called multiple times (typical in a nested loop join). Then proceeding
    to the next record (rnd_next) and closing the scan (rnd_end).
    To remember a record for later access there is a method (position)
    and there is a method used to retrieve the record based on the stored
    position.
    The position can be a file position, a primary key, a ROWID dependent
    on the handler below.
    -------------------------------------------------------------------------
  */
  /*
    unlike index_init(), rnd_init() can be called two times
    without rnd_end() in between (it only makes sense if scan=1).
    then the second call should prepare for the new table scan
    (e.g if rnd_init allocates the cursor, second call should
    position it to the start of the table, no need to deallocate
    and allocate it again
  */
  virtual int rnd_init(bool scan);
  virtual int rnd_end();
  virtual int rnd_next(byte * buf);
  virtual int rnd_pos(byte * buf, byte * pos);
  virtual void position(const byte * record);

  /*
    -------------------------------------------------------------------------
    MODULE index scan
    -------------------------------------------------------------------------
    This part of the handler interface is used to perform access through
    indexes. The interface is defined as a scan interface but the handler
    can also use key lookup if the index is a unique index or a primary
    key index.
    Index scans are mostly useful for SELECT queries but are an important
    part also of UPDATE, DELETE, REPLACE and CREATE TABLE table AS SELECT
    and so forth.
    Naturally an index is needed for an index scan and indexes can either
    be ordered, hash based. Some ordered indexes can return data in order
    but not necessarily all of them.
    There are many flags that define the behavior of indexes in the
    various handlers. These methods are found in the optimizer module.
    -------------------------------------------------------------------------

    index_read is called to start a scan of an index. The find_flag defines
    the semantics of the scan. These flags are defined in
    include/my_base.h
    index_read_idx is the same but also initializes index before calling doing
    the same thing as index_read. Thus it is similar to index_init followed
    by index_read. This is also how we implement it.

    index_read/index_read_idx does also return the first row. Thus for
    key lookups, the index_read will be the only call to the handler in
    the index scan.

    index_init initializes an index before using it and index_end does
    any end processing needed.
  */
  virtual int index_read(byte * buf, const byte * key,
			 uint key_len, enum ha_rkey_function find_flag);
  virtual int index_read_idx(byte * buf, uint idx, const byte * key,
			     uint key_len, enum ha_rkey_function find_flag);
  virtual int index_init(uint idx, bool sorted);
  virtual int index_end();

  /*
    These methods are used to jump to next or previous entry in the index
    scan. There are also methods to jump to first and last entry.
  */
  virtual int index_next(byte * buf);
  virtual int index_prev(byte * buf);
  virtual int index_first(byte * buf);
  virtual int index_last(byte * buf);
  virtual int index_next_same(byte * buf, const byte * key, uint keylen);
  virtual int index_read_last(byte * buf, const byte * key, uint keylen);

  /*
    read_first_row is virtual method but is only implemented by
    handler.cc, no storage engine has implemented it so neither
    will the partition handler.
    
    virtual int read_first_row(byte *buf, uint primary_key);
  */

  /*
    We don't implement multi read range yet, will do later.
    virtual int read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
    KEY_MULTI_RANGE *ranges, uint range_count,
    bool sorted, HANDLER_BUFFER *buffer);
    virtual int read_multi_range_next(KEY_MULTI_RANGE **found_range_p);
  */


  virtual int read_range_first(const key_range * start_key,
			       const key_range * end_key,
			       bool eq_range, bool sorted);
  virtual int read_range_next();

private:
  int common_index_read(byte * buf, const byte * key,
			uint key_len, enum ha_rkey_function find_flag);
  int common_first_last(byte * buf);
  int partition_scan_set_up(byte * buf, bool idx_read_flag);
  int handle_unordered_next(byte * buf, bool next_same);
  int handle_unordered_scan_next_partition(byte * buf);
  byte *queue_buf(uint part_id)
    {
      return (m_ordered_rec_buffer +
              (part_id * (m_rec_length + PARTITION_BYTES_IN_POS)));
    }
  byte *rec_buf(uint part_id)
    {
      return (queue_buf(part_id) +
              PARTITION_BYTES_IN_POS);
    }
  int handle_ordered_index_scan(byte * buf);
  int handle_ordered_next(byte * buf, bool next_same);
  int handle_ordered_prev(byte * buf);
  void return_top_record(byte * buf);
  void include_partition_fields_in_used_fields();
public:
  /*
    -------------------------------------------------------------------------
    MODULE information calls
    -------------------------------------------------------------------------
    This calls are used to inform the handler of specifics of the ongoing
    scans and other actions. Most of these are used for optimisation
    purposes.
    -------------------------------------------------------------------------
  */
  virtual void info(uint);
  virtual int extra(enum ha_extra_function operation);
  virtual int extra_opt(enum ha_extra_function operation, ulong cachesize);
  virtual int reset(void);

private:
  static const uint NO_CURRENT_PART_ID= 0xFFFFFFFF;
  int loop_extra(enum ha_extra_function operation);
  void late_extra_cache(uint partition_id);
  void late_extra_no_cache(uint partition_id);
  void prepare_extra_cache(uint cachesize);
public:

  /*
    -------------------------------------------------------------------------
    MODULE optimiser support
    -------------------------------------------------------------------------
    -------------------------------------------------------------------------
  */

  /*
    NOTE !!!!!!
     -------------------------------------------------------------------------
     -------------------------------------------------------------------------
     One important part of the public handler interface that is not depicted in
     the methods is the attribute records

     which is defined in the base class. This is looked upon directly and is
     set by calling info(HA_STATUS_INFO) ?
     -------------------------------------------------------------------------
  */

  /*
    keys_to_use_for_scanning can probably be implemented as the
    intersection of all underlying handlers if mixed handlers are used.
    This method is used to derive whether an index can be used for
    index-only scanning when performing an ORDER BY query.
    Only called from one place in sql_select.cc
  */
  virtual const key_map *keys_to_use_for_scanning();

  /*
    Called in test_quick_select to determine if indexes should be used.
  */
  virtual double scan_time();

  /*
    The next method will never be called if you do not implement indexes.
  */
  virtual double read_time(uint index, uint ranges, ha_rows rows);
  /*
    For the given range how many records are estimated to be in this range.
    Used by optimiser to calculate cost of using a particular index.
  */
  virtual ha_rows records_in_range(uint inx, key_range * min_key,
				   key_range * max_key);

  /*
    Upper bound of number records returned in scan is sum of all
    underlying handlers.
  */
  virtual ha_rows estimate_rows_upper_bound();

  /*
    table_cache_type is implemented by the underlying handler but all
    underlying handlers must have the same implementation for it to work.
  */
  virtual uint8 table_cache_type();

  /*
    -------------------------------------------------------------------------
    MODULE print messages
    -------------------------------------------------------------------------
    This module contains various methods that returns text messages for
    table types, index type and error messages.
    -------------------------------------------------------------------------
  */
  /*
    The name of the index type that will be used for display
    Here we must ensure that all handlers use the same index type
    for each index created.
  */
  virtual const char *index_type(uint inx);

  /* The name of the table type that will be used for display purposes */
  virtual const char *table_type() const
  { return "PARTITION"; }

  /*
     Handler specific error messages
  */
  virtual void print_error(int error, myf errflag);
  virtual bool get_error_message(int error, String * buf);
  /*
   -------------------------------------------------------------------------
    MODULE handler characteristics
    -------------------------------------------------------------------------
    This module contains a number of methods defining limitations and
    characteristics of the handler. The partition handler will calculate
    this characteristics based on underlying handler characteristics.
    -------------------------------------------------------------------------

    This is a list of flags that says what the storage engine
    implements. The current table flags are documented in handler.h
    The partition handler will support whatever the underlying handlers
    support except when specifically mentioned below about exceptions
    to this rule.

    HA_READ_RND_SAME:
    Not currently used. (Means that the handler supports the rnd_same() call)
    (MyISAM, HEAP)

    HA_TABLE_SCAN_ON_INDEX:
    Used to avoid scanning full tables on an index. If this flag is set then
    the handler always has a primary key (hidden if not defined) and this
    index is used for scanning rather than a full table scan in all
    situations.
    (InnoDB, BDB, Federated)

    HA_REC_NOT_IN_SEQ:
    This flag is set for handlers that cannot guarantee that the rows are
    returned accroding to incremental positions (0, 1, 2, 3...).
    This also means that rnd_next() should return HA_ERR_RECORD_DELETED
    if it finds a deleted row.
    (MyISAM (not fixed length row), BDB, HEAP, NDB, InooDB)

    HA_CAN_GEOMETRY:
    Can the storage engine handle spatial data.
    Used to check that no spatial attributes are declared unless
    the storage engine is capable of handling it.
    (MyISAM)

    HA_FAST_KEY_READ:
    Setting this flag indicates that the handler is equally fast in
    finding a row by key as by position.
    This flag is used in a very special situation in conjunction with
    filesort's. For further explanation see intro to init_read_record.
    (BDB, HEAP, InnoDB)

    HA_NULL_IN_KEY:
    Is NULL values allowed in indexes.
    If this is not allowed then it is not possible to use an index on a
    NULLable field.
    (BDB, HEAP, MyISAM, NDB, InnoDB)

    HA_DUPP_POS:
    Tells that we can the position for the conflicting duplicate key
    record is stored in table->file->dupp_ref. (insert uses rnd_pos() on
    this to find the duplicated row)
    (MyISAM)

    HA_CAN_INDEX_BLOBS:
    Is the storage engine capable of defining an index of a prefix on
    a BLOB attribute.
    (BDB, Federated, MyISAM, InnoDB)

    HA_AUTO_PART_KEY:
    Auto increment fields can be part of a multi-part key. For second part
    auto-increment keys, the auto_incrementing is done in handler.cc
    (BDB, Federated, MyISAM, NDB)

    HA_REQUIRE_PRIMARY_KEY:
    Can't define a table without primary key (and cannot handle a table
    with hidden primary key)
    (No handler has this limitation currently)

    HA_NOT_EXACT_COUNT:
    Does the counter of records after the info call specify an exact
    value or not. If it doesn't this flag is set.
    Only MyISAM and HEAP uses exact count.
    (MyISAM, HEAP, BDB, InnoDB, NDB, Federated)

    HA_CAN_INSERT_DELAYED:
    Can the storage engine support delayed inserts.
    To start with the partition handler will not support delayed inserts.
    Further investigation needed.
    (HEAP, MyISAM)

    HA_PRIMARY_KEY_IN_READ_INDEX:
    This parameter is set when the handler will also return the primary key
    when doing read-only-key on another index.

    HA_NOT_DELETE_WITH_CACHE:
    Seems to be an old MyISAM feature that is no longer used. No handler
    has it defined but it is checked in init_read_record.
    Further investigation needed.
    (No handler defines it)

    HA_NO_PREFIX_CHAR_KEYS:
    Indexes on prefixes of character fields is not allowed.
    (NDB)

    HA_CAN_FULLTEXT:
    Does the storage engine support fulltext indexes
    The partition handler will start by not supporting fulltext indexes.
    (MyISAM)

    HA_CAN_SQL_HANDLER:
    Can the HANDLER interface in the MySQL API be used towards this
    storage engine.
    (MyISAM, InnoDB)

    HA_NO_AUTO_INCREMENT:
    Set if the storage engine does not support auto increment fields.
    (Currently not set by any handler)

    HA_HAS_CHECKSUM:
    Special MyISAM feature. Has special SQL support in CREATE TABLE.
    No special handling needed by partition handler.
    (MyISAM)

    HA_FILE_BASED:
    Should file names always be in lower case (used by engines
    that map table names to file names.
    Since partition handler has a local file this flag is set.
    (BDB, Federated, MyISAM)

    HA_CAN_BIT_FIELD:
    Is the storage engine capable of handling bit fields?
    (MyISAM, NDB)

    HA_NEED_READ_RANGE_BUFFER:
    Is Read Multi-Range supported => need multi read range buffer
    This parameter specifies whether a buffer for read multi range
    is needed by the handler. Whether the handler supports this
    feature or not is dependent of whether the handler implements
    read_multi_range* calls or not. The only handler currently
    supporting this feature is NDB so the partition handler need
    not handle this call. There are methods in handler.cc that will
    transfer those calls into index_read and other calls in the
    index scan module.
    (NDB)
  */
  virtual ulong alter_table_flags(void) const
  {
    //return HA_ONLINE_ADD_EMPTY_PARTITION + HA_ONLINE_DROP_PARTITION;
    return HA_ONLINE_DROP_PARTITION;
  }
  virtual ulong table_flags() const
  { return m_table_flags; }
  /*
    HA_CAN_PARTITION:
    Used by storage engines that can handle partitioning without this
    partition handler
    (Partition, NDB)

    HA_CAN_UPDATE_PARTITION_KEY:
    Set if the handler can update fields that are part of the partition
    function.

    HA_CAN_PARTITION_UNIQUE:
    Set if the handler can handle unique indexes where the fields of the
    unique key are not part of the fields of the partition function. Thus
    a unique key can be set on all fields.
  */
  virtual ulong partition_flags() const
  { return HA_CAN_PARTITION; }

  /*
    This is a bitmap of flags that says how the storage engine
    implements indexes. The current index flags are documented in
    handler.h. If you do not implement indexes, just return zero
    here.

    part is the key part to check. First key part is 0
    If all_parts it's set, MySQL want to know the flags for the combined
    index up to and including 'part'.

    HA_READ_NEXT:
    Does the index support read next, this is assumed in the server
    code and never checked so all indexes must support this.
    Note that the handler can be used even if it doesn't have any index.
    (BDB, HEAP, MyISAM, Federated, NDB, InnoDB)

    HA_READ_PREV:
    Can the index be used to scan backwards.
    (BDB, HEAP, MyISAM, NDB, InnoDB)

    HA_READ_ORDER:
    Can the index deliver its record in index order. Typically true for
    all ordered indexes and not true for hash indexes.
    In first step this is not true for partition handler until a merge
    sort has been implemented in partition handler.
    Used to set keymap part_of_sortkey
    This keymap is only used to find indexes usable for resolving an ORDER BY
    in the query. Thus in most cases index_read will work just fine without
    order in result production. When this flag is set it is however safe to
    order all output started by index_read since most engines do this. With
    read_multi_range calls there is a specific flag setting order or not
    order so in those cases ordering of index output can be avoided.
    (BDB, InnoDB, HEAP, MyISAM, NDB)

    HA_READ_RANGE:
    Specify whether index can handle ranges, typically true for all
    ordered indexes and not true for hash indexes.
    Used by optimiser to check if ranges (as key >= 5) can be optimised
    by index.
    (BDB, InnoDB, NDB, MyISAM, HEAP)

    HA_ONLY_WHOLE_INDEX:
    Can't use part key searches. This is typically true for hash indexes
    and typically not true for ordered indexes.
    (Federated, NDB, HEAP)

    HA_KEYREAD_ONLY:
    Does the storage engine support index-only scans on this index.
    Enables use of HA_EXTRA_KEYREAD and HA_EXTRA_NO_KEYREAD
    Used to set key_map keys_for_keyread and to check in optimiser for
    index-only scans.  When doing a read under HA_EXTRA_KEYREAD the handler
    only have to fill in the columns the key covers. If
    HA_PRIMARY_KEY_IN_READ_INDEX is set then also the PRIMARY KEY columns
    must be updated in the row.
    (BDB, InnoDB, MyISAM)
  */
  virtual ulong index_flags(uint inx, uint part, bool all_parts) const
  {
    return m_file[0]->index_flags(inx, part, all_parts);
  }

  /*
     extensions of table handler files
  */
  virtual const char **bas_ext() const;
  /*
    unireg.cc will call the following to make sure that the storage engine
    can handle the data it is about to send.

    The maximum supported values is the minimum of all handlers in the table
  */
  uint min_of_the_max_uint(uint (handler::*operator_func)(void) const) const; 
  virtual uint max_supported_record_length() const;
  virtual uint max_supported_keys() const;
  virtual uint max_supported_key_parts() const;
  virtual uint max_supported_key_length() const;
  virtual uint max_supported_key_part_length() const;

  /*
    All handlers in a partitioned table must have the same low_byte_first
  */
  virtual bool low_byte_first() const
  { return m_low_byte_first; }

  /*
    The extra record buffer length is the maximum needed by all handlers.
    The minimum record length is the maximum of all involved handlers.
  */
  virtual uint extra_rec_buf_length() const;
  virtual uint min_record_length(uint options) const;

  /*
    Transactions on the table is supported if all handlers below support
    transactions.
  */
  virtual bool has_transactions()
  { return m_has_transactions; }

  /*
    Primary key is clustered can only be true if all underlying handlers have
    this feature.
  */
  virtual bool primary_key_is_clustered()
  { return m_pkey_is_clustered; }

  /*
    -------------------------------------------------------------------------
    MODULE compare records
    -------------------------------------------------------------------------
    cmp_ref checks if two references are the same. For most handlers this is
    a simple memcmp of the reference. However some handlers use primary key
    as reference and this can be the same even if memcmp says they are
    different. This is due to character sets and end spaces and so forth.
    For the partition handler the reference is first two bytes providing the
    partition identity of the referred record and then the reference of the
    underlying handler.
    Thus cmp_ref for the partition handler always returns FALSE for records
    not in the same partition and uses cmp_ref on the underlying handler
    to check whether the rest of the reference part is also the same.
    -------------------------------------------------------------------------
  */
  virtual int cmp_ref(const byte * ref1, const byte * ref2);
  /*
    -------------------------------------------------------------------------
    MODULE auto increment
    -------------------------------------------------------------------------
    This module is used to handle the support of auto increments.

    This variable in the handler is used as part of the handler interface
    It is maintained by the parent handler object and should not be
    touched by child handler objects (see handler.cc for its use).

    auto_increment_column_changed
     -------------------------------------------------------------------------
  */
  virtual void restore_auto_increment();
  virtual ulonglong get_auto_increment();

  /*
     -------------------------------------------------------------------------
     MODULE initialise handler for HANDLER call
     -------------------------------------------------------------------------
     This method is a special InnoDB method called before a HANDLER query.
     -------------------------------------------------------------------------
  */
  virtual void init_table_handle_for_HANDLER();

  /*
    The remainder of this file defines the handler methods not implemented
    by the partition handler
  */

  /*
    -------------------------------------------------------------------------
    MODULE foreign key support
    -------------------------------------------------------------------------
    The following methods are used to implement foreign keys as supported by
    InnoDB. Implement this ??
    get_foreign_key_create_info is used by SHOW CREATE TABLE to get a textual
    description of how the CREATE TABLE part to define FOREIGN KEY's is done.
    free_foreign_key_create_info is used to free the memory area that provided
    this description.
    -------------------------------------------------------------------------

    virtual char* get_foreign_key_create_info()
    virtual void free_foreign_key_create_info(char* str)

    virtual int get_foreign_key_list(THD *thd,
    List<FOREIGN_KEY_INFO> *f_key_list)
    virtual uint referenced_by_foreign_key()
  */

  /*
    -------------------------------------------------------------------------
    MODULE fulltext index
    -------------------------------------------------------------------------
    Fulltext stuff not yet.
    -------------------------------------------------------------------------
    virtual int ft_init() { return HA_ERR_WRONG_COMMAND; }
    virtual FT_INFO *ft_init_ext(uint flags,uint inx,const byte *key,
    uint keylen)
    { return NULL; }
    virtual int ft_read(byte *buf) { return HA_ERR_WRONG_COMMAND; }
  */

  /*
     -------------------------------------------------------------------------
     MODULE restart full table scan at position (MyISAM)
     -------------------------------------------------------------------------
     The following method is only used by MyISAM when used as
     temporary tables in a join.
     virtual int restart_rnd_next(byte *buf, byte *pos);
  */

  /*
    -------------------------------------------------------------------------
    MODULE on-line ALTER TABLE
    -------------------------------------------------------------------------
    These methods are in the handler interface but never used (yet)
    They are to be used by on-line alter table add/drop index:
    -------------------------------------------------------------------------
    virtual ulong index_ddl_flags(KEY *wanted_index) const
    virtual int add_index(TABLE *table_arg,KEY *key_info,uint num_of_keys);
    virtual int drop_index(TABLE *table_arg,uint *key_num,uint num_of_keys);
  */

  /*
    -------------------------------------------------------------------------
    MODULE tablespace support
    -------------------------------------------------------------------------
    Admin of table spaces is not applicable to the partition handler (InnoDB)
    This means that the following method is not implemented:
    -------------------------------------------------------------------------
    virtual int discard_or_import_tablespace(my_bool discard)
  */

  /*
    -------------------------------------------------------------------------
    MODULE admin MyISAM
    -------------------------------------------------------------------------
    Admin commands not supported currently (almost purely MyISAM routines)
    This means that the following methods are not implemented:
    -------------------------------------------------------------------------

    virtual int check(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int backup(TD* thd, HA_CHECK_OPT *check_opt);
    virtual int restore(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int repair(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int optimize(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int analyze(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int assign_to_keycache(THD* thd, HA_CHECK_OPT *check_opt);
    virtual int preload_keys(THD *thd, HA_CHECK_OPT *check_opt);
    virtual bool check_and_repair(THD *thd);
    virtual int dump(THD* thd, int fd = -1);
    virtual int net_read_dump(NET* net);
    virtual uint checksum() const;
    virtual bool is_crashed() const;
    virtual bool auto_repair() const;

    -------------------------------------------------------------------------
    MODULE enable/disable indexes
    -------------------------------------------------------------------------
    Enable/Disable Indexes are not supported currently (Heap, MyISAM)
    This means that the following methods are not implemented:
    -------------------------------------------------------------------------
    virtual int disable_indexes(uint mode);
    virtual int enable_indexes(uint mode);
    virtual int indexes_are_disabled(void);
  */

  /*
    -------------------------------------------------------------------------
    MODULE append_create_info
    -------------------------------------------------------------------------
    append_create_info is only used by MyISAM MERGE tables and the partition
    handler will not support this handler as underlying handler.
    Implement this??
    -------------------------------------------------------------------------
    virtual void append_create_info(String *packet)
  */
};
