#ifndef BACKUP_STREAM_V1_
#define BACKUP_STREAM_V1_

/** 
  @file

  @brief
  Backup stream library API for version 1 of stream format.

  This file declares functions and data types used to read and write backup
  stream using version 1 of backup stream format.
*/ 

/*********************************************************************
 * 
 *   BASIC TYPES
 * 
 *********************************************************************/
  
typedef unsigned char bstream_byte;
 
/**
  Describes continuous area of memory.
  
  The @c begin member points at the first byte in the area, @c at one byte
  after the last byte of the area. Thus, blob @c b contains exactly 
  <code>b.end - b.begin</code> bytes and is empty if 
  <code>b.begin == b.end</code>. A null blob is a blob @c b with 
  <code>b.begin == NULL</code>.
*/ 
struct st_blob
{
  bstream_byte *begin; /**< first byte of the blob */
  bstream_byte *end;   /**< one byte after the last byte of the blob */
};

typedef struct st_blob bstream_blob;

/**
  Stores time point with one second accuracy.
  
  This structure is similar to the POSIX <code>struct tm</code>. We define it
  explicitly to make this header self-contained.
*/ 
struct st_bstream_time
{
  unsigned short int  sec;   /**< seconds [0,61] */
  unsigned short int  min;   /**< minutes [0,59] */
  unsigned short int  hour;  /**< hour [0,23] */
  unsigned short int  mday;  /**< day of month [1,31] */
  unsigned short int  mon;   /**< month of year [0,11] */
  unsigned int        year;  /**< years since 1900 */
};

typedef struct st_bstream_time bstream_time_t;

/**
  Describes position of an event in MySQL server's binary log.
  
  The event is identified by the name of the file in which it is stored and
  the position within that file.
*/ 
struct st_bstream_binlog_pos
{
  char              *file;  /**< binlog file storing the event */
  unsigned long int pos;    /**< position (offset) within the file */ 
};

/* struct st_backup_stream is defined below */
typedef struct st_backup_stream backup_stream;

/** Codes returned by backup stream functions */
enum enum_bstream_ret_codes { 
  BSTREAM_OK=0,  /**< Success */ 
  BSTREAM_EOC,   /**< End of chunk detected */
  BSTREAM_EOS,   /**< End of stream detected */
  BSTREAM_ERROR  /**< Error */
};

/*********************************************************************
 * 
 *   TYPES FOR IMAGE HEADER
 * 
 *********************************************************************/

/**
  Describes version of MySQL server.
  
  For example, if server has version "5.2.32-online-backup" then:
  major   = 5
  minor   = 2
  release = 32
  extra   = "5.2.32-online-backup"
*/ 
struct st_server_version
{
  unsigned short int  major;
  unsigned short int  minor;
  unsigned short int  release;
  bstream_blob        extra;
};

/**
  Describes version of storage engine.
*/ 
struct st_bstream_engine_info
{
  bstream_blob        name;   /**< name of the storage engine */
  unsigned short int  major;  /**< version number (major) */
  unsigned short int  minor;  /**< version number (minor) */
};

/** Types of table data snapshots. */
enum enum_bstream_snapshot_type { 
  BI_NATIVE,  /**< created by native backup driver of a storage engine */ 
  BI_DEFAULT, /**< created by built-in blocking backup driver */
  BI_CS       /**< created by built-in driver using consistent read transaction */
};

/** Describes table data snapshot. */
struct st_bstream_snapshot_info
{
  unsigned int            version;      /**< snapshot format version */
  enum enum_bstream_snapshot_type type; /**< type of the snapshot */
  unsigned int            options;      /**< snapshot options (not used currently) */
  unsigned long int       table_count;  /**< number of tables in the snapshot */
  /**
    In case of native snapshot, information about storage engine 
    which created it 
  */
  struct st_bstream_engine_info  engine; 
};

/** 
  Extension of st_bstream_snapshot_info describing snapshot created by a native
  backup driver.
*/ 
struct st_native_snapshot_info
{
  struct st_bstream_snapshot_info  base;   /**< standard snapshot data */   
};

/** Information about backup image. */ 
struct st_bstream_image_header
{
  unsigned int      version;    /**< image's format version number */
  /** version of the server which created the image */ 
  struct st_server_version  server_version; 
  unsigned int      flags;      /**< image options */
  bstream_time_t    start_time; /**< time when backup operation started */
  bstream_time_t    end_time;   /**< time when backup operation completed */
  bstream_time_t    vp_time;    /**< time of the image's validity point */ 
  
  /*
    If server which created backup image had binary log enabled, the image 
    contains information about the position inside the log corresponding to
    the validity point time.
   */ 
   
  /** position of the last binlog entry at the VP time */ 
  struct st_bstream_binlog_pos  binlog_pos;
  /** start of the last binlog event group at the VP time */ 
  struct st_bstream_binlog_pos  binlog_group;
  
  /** number of table data snapshots in the image */
  unsigned short int        snap_count;
  /** descriptions of table data snapshots */
  struct st_bstream_snapshot_info snapshot[256];
};

/* Flags */

/**
  Position of the summary block.
  
  If this flag is set, the block containing summary info (available at the end
  of backup process) is stored in the image's preamble. Otherwise, this block
  is appended at the end of the image.
 */ 
#define BSTREAM_FLAG_INLINE_SUMMARY (1U<<0)

/**
  Byte order of the server which created backup image.
  
  If set, informs that backup image was created on big-endian server. This might
  be useful to detect problems, if backup engines are not endian-agnostic.
*/ 
#define BSTREAM_FLAG_BIG_ENDIAN     (1U<<1)

/**
  Informs if image stores binlog position.
  
  If this flag is not set, the @c binlog_pos and @c binlog_group entries in
  the image should be ignored.
 */ 
#define BSTREAM_FLAG_BINLOG         (1U<<2)



/*********************************************************************
 * 
 *   TYPES FOR DESCRIBING BACKUP ITEMS
 * 
 *********************************************************************/

/** 
  Types of items stored in a backup image.
  
  @note Not all of these types are supported currently.
*/
enum enum_bstream_item_type {
   BSTREAM_IT_CHARSET,
   BSTREAM_IT_USER,
   BSTREAM_IT_PRIVILEGE,
   BSTREAM_IT_DB,
   BSTREAM_IT_TABLE,
   BSTREAM_IT_VIEW,
   BSTREAM_IT_LAST
};

/**
  Common data about backup image item.
*/ 
struct st_bstream_item_info
{
  enum enum_bstream_item_type type;  /**< type of the item */
  bstream_blob name;     /**< name of the item */
  unsigned long int pos; /**< position of the item in image's catalogue */
};

/**
  Describes database item.
  
  Currently no data specific to database items is used.
*/ 
struct st_bstream_db_info
{
  struct st_bstream_item_info  base;
};


/**
  Describes item which sits inside a database.
*/ 
struct st_bstream_dbitem_info
{
  struct st_bstream_item_info  base; /**< data common to all items */
  struct st_bstream_db_info    *db;  /**< database to which this item belongs */
};

/**
  Describes a table item.
  
  Table is a per-database item. Additionally we store information about the
  snapshot which contains its data.
*/ 
struct st_bstream_table_info
{
  struct st_bstream_dbitem_info  base;  /**< data common to all per-db items */
  unsigned short int  snap_no;  /**< snapshot where table's data is stored */
};

/**
  Describes item which sits inside a table.
 */ 
struct st_bstream_titem_info
{
  struct st_bstream_item_info  base;   /**< data common to all items */
  struct st_bstream_table_info *table; /**< table to which this item belongs */
};

/*
  The following constants denote additional backup item categories. Items of 
  different types can fall into one of these categories. Thus the categories 
  are something different than item types and therefore are not listed  
  inside enum_bstream_item_type but defined separately.  
*/ 

#define BSTREAM_IT_GLOBAL    BSTREAM_IT_LAST
#define BSTREAM_IT_PERDB     (BSTREAM_IT_LAST+1)
#define BSTREAM_IT_PERTABLE  (BSTREAM_IT_LAST+2)


/*************************************************************************
 * 
 *   STRUCTURE FOR WRITING/READING TABLE DATA
 * 
 *************************************************************************/

/**
  Describe chunk of data from backup driver or for restore driver.
*/ 
struct st_bstream_data_chunk
{
  unsigned long int  table_no;  /**< table to which this data belongs */
  bstream_blob       data;      /**< the data */
  unsigned short int flags;     /**< flags to be saved together with the chunk */
  unsigned short int snap_no;   /**< which snapshot this chunk belongs to */
};

/** Indicates that given chunk is the last chunk of data for a given table */
#define BSTREAM_FLAG_LAST_CHUNK  1


/*************************************************************************
 * 
 *   FUNCTIONS FOR WRITING BACKUP IMAGE
 * 
 *************************************************************************/

int bstream_wr_preamble(backup_stream*, struct st_bstream_image_header*);
int bstream_wr_data_chunk(backup_stream*, 
                          struct st_bstream_data_chunk*);
int bstream_wr_summary(backup_stream *s, struct st_bstream_image_header *hdr);

/*********************************************************************
 * 
 *   FUNCTIONS FOR READING BACKUP IMAGE
 * 
 *********************************************************************/

int bstream_rd_preamble(backup_stream*, struct st_bstream_image_header*);
int bstream_rd_data_chunk(backup_stream*, 
                          struct st_bstream_data_chunk*);
int bstream_rd_summary(backup_stream *s, struct st_bstream_image_header *hdr);

/* parts of preamble */

int bstream_rd_header(backup_stream*, struct st_bstream_image_header*);
int bstream_rd_catalogue(backup_stream*, struct st_bstream_image_header*);
int bstream_rd_meta_data(backup_stream *, struct st_bstream_image_header*);

/* basic types */

int bstream_rd_time(backup_stream*, bstream_time_t*); 
int bstream_rd_string(backup_stream*, bstream_blob*);
int bstream_rd_num(backup_stream*, unsigned long int*);
int bstream_rd_int4(backup_stream*, unsigned long int*);
int bstream_rd_int2(backup_stream*, unsigned int*);
int bstream_rd_byte(backup_stream*, unsigned short int*); 

int bstream_next_chunk(backup_stream*);


/*********************************************************************
 * 
 *   DEFINITION OF BACKUP STREAM STRUCTURE
 * 
 *********************************************************************/

/**
  Structure defining base I/O operations.
  
  Abstract stream is a way of defining the final destination/source of
  the bytes being written to/read from a backup stream. This is done by
  storing pointers to functions performing basic I/O operations inside
  this structure.
 */
struct st_abstract_stream
{
  int (*write)(void*, bstream_blob*, bstream_blob);
  int (*read)(void*, bstream_blob*, bstream_blob);
  int (*forward)(void*, unsigned long int*);
};

/** 
  Structure describing backup stream`s input or output buffer. 
  @see stream_v1_carrier.c  
*/
struct st_bstream_buffer
{
  bstream_byte  *begin;
  bstream_byte  *pos;
  bstream_byte  *header;
  bstream_byte  *end;
};

/** 
  Structure describing state of a backup stream. 
*/
struct st_backup_stream
{
  struct st_abstract_stream stream;
  size_t  block_size;
  enum { CLOSED, READING, WRITING, EOS, ERROR } state;
  struct st_bstream_buffer buf;  
  int  reading_last_fragment;
  bstream_blob mem;
  bstream_blob data_buf;
};

int bstream_open_wr(backup_stream*, unsigned long int);
int bstream_open_rd(backup_stream*, unsigned long int);
int bstream_close(backup_stream*);

#endif /*BACKUP_V1_*/
