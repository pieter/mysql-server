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

/* This file is included by all internal myisam files */

#define ISAM_LIBRARY
#include "myisam.h"			/* Structs & some defines */
#include "myisampack.h"			/* packing of keys */
#ifdef THREAD
#include <my_pthread.h>
#include <thr_lock.h>
#else
#include <my_no_pthread.h>
#endif

#if defined(my_write) && !defined(MAP_TO_USE_RAID)
#undef my_write				/* undef map from my_nosys; We need test-if-disk full */
#endif

typedef struct st_mi_status_info
{
  ha_rows records;			/* Rows in table */
  ha_rows del;				/* Removed rows */
  my_off_t empty;			/* lost space in datafile */
  my_off_t key_empty;			/* lost space in indexfile */
  my_off_t key_file_length;
  my_off_t data_file_length;
} MI_STATUS_INFO;  

typedef struct st_mi_state_info
{
  struct {				/* Fileheader */
    uchar file_version[4];
    uchar options[2];
    uchar header_length[2];
    uchar state_info_length[2];
    uchar base_info_length[2];
    uchar base_pos[2];
    uchar key_parts[2];			/* Key parts */
    uchar unique_key_parts[2];		/* Key parts + unique parts */
    uchar keys;				/* number of keys in file */
    uchar uniques;			/* number of UNIQUE definitions */
    uchar language;			/* Language for indexes */
    uchar max_block_size;		/* max keyblock size */
    uchar not_used[2];			/* To align to 8 */
  } header;

  MI_STATUS_INFO state;
  ha_rows split;			/* number of split blocks */
  my_off_t dellink;			/* Link to next removed block */
  ulonglong auto_increment;
  ulong process;			/* process that updated table last */
  ulong unique;				/* Unique number for this process */
  ulong status;
  my_off_t *key_root;			/* Start of key trees */
  my_off_t *key_del;			/* delete links for trees */

  ulong sec_index_changed;		/* Updated when new sec_index */
  ulong sec_index_used;			/* which extra index are in use */
  ulonglong key_map;			/* Which keys are in use */
  ha_checksum checksum;
  ulong version;			/* timestamp of create */
  time_t create_time;			/* Time when created database */
  time_t recover_time;			/* Time for last recover */
  time_t check_time;			/* Time for last check */
  uint	sortkey;			/* sorted by this key  (not used) */
  uint open_count;
  bool changed;				/* Changed since isamchk */
  my_off_t rec_per_key_rows;		/* Rows when calculating rec_per_key */
  ulong *rec_per_key_part;

  /* the following isn't saved on disk */
  uint state_diff_length;		/* Should be 0 */
  uint	state_length;			/* Length of state header in file */
  ulong *key_info;
} MI_STATE_INFO;

#define MI_STATE_INFO_SIZE	(24+14*8+7*4+2*2+8)
#define MI_STATE_KEY_SIZE 	8
#define MI_STATE_KEYBLOCK_SIZE  8
#define MI_STATE_KEYSEG_SIZE	4
#define MI_STATE_EXTRA_SIZE ((MI_MAX_KEY+MI_MAX_KEY_BLOCK_SIZE)*MI_STATE_KEY_SIZE + MI_MAX_KEY*MI_MAX_KEY_SEG*MI_STATE_KEYSEG_SIZE)
#define MI_KEYDEF_SIZE		(2+ 5*2)
#define MI_UNIQUEDEF_SIZE	(2+1+1)
#define MI_KEYSEG_SIZE		(6+ 2*2 + 4*2)
#define MI_COLUMNDEF_SIZE	(2*3+1)
#define MI_BASE_INFO_SIZE	(5*8 + 8*4 + 4 + 4*2 + 16)
#define MI_INDEX_BLOCK_MARGIN	16	/* Safety margin for .MYI tables */

typedef struct st_mi_base_info
{
  my_off_t keystart;			/* Start of keys */
  my_off_t max_data_file_length;
  my_off_t max_key_file_length;
  my_off_t margin_key_file_length;
  ha_rows records,reloc;		/* Create information */
  ulong mean_row_length;		/* Create information */
  ulong reclength;			/* length of unpacked record */
  ulong pack_reclength;			/* Length of full packed rec. */
  ulong min_pack_length;
  ulong max_pack_length;		/* Max possibly length of packed rec.*/
  ulong min_block_length;
  ulong fields,				/* fields in table */
       pack_fields;			/* packed fields in table */
  uint rec_reflength;			/* = 2-8 */
  uint key_reflength;			/* = 2-8 */
  uint keys;				/* same as in state.header */
  uint auto_key;			/* Which key-1 is a auto key */
  uint blobs;				/* Number of blobs */
  uint pack_bits;			/* Length of packed bits */
  uint max_key_block_length;		/* Max block length */
  uint max_key_length;			/* Max key length */
  /* Extra allocation when using dynamic record format */
  uint extra_alloc_bytes;
  uint extra_alloc_procent;
  /* Info about raid */
  uint raid_type,raid_chunks;
  ulong raid_chunksize;
  /* The following are from the header */
  uint key_parts,all_key_parts;
} MI_BASE_INFO;


	/* Structs used intern in database */

typedef struct st_mi_blob		/* Info of record */
{
  ulong offset;				/* Offset to blob in record */
  uint pack_length;			/* Type of packed length */
  ulong length;				/* Calc:ed for each record */
} MI_BLOB;


typedef struct st_mi_isam_pack {
  ulong header_length;
  uint ref_length;
} MI_PACK;


typedef struct st_mi_isam_share {	/* Shared between opens */
  MI_STATE_INFO state;
  MI_BASE_INFO base;
  MI_KEYDEF  *keyinfo;			/* Key definitions */
  MI_UNIQUEDEF *uniqueinfo;		/* unique definitions */
  MI_KEYSEG *keyparts;			/* key part info */
  MI_COLUMNDEF *rec;			/* Pointer to field information */
  MI_PACK    pack;			/* Data about packed records */
  MI_BLOB    *blobs;			/* Pointer to blobs */
  char	*filename;			/* Name of indexfile */
  byte *file_map;			/* mem-map of file if possible */
  ulong this_process;			/* processid */
  ulong last_process;			/* For table-change-check */
  ulong last_version;			/* Version on start */
  ulong options;			/* Options used */
  uint	rec_reflength;			/* rec_reflength in use now */
  int	kfile;				/* Shared keyfile */
  int	data_file;			/* Shared data file */
  int	mode;				/* mode of file on open */
  uint	reopen;				/* How many times reopened */
  uint	w_locks,r_locks;		/* Number of read/write locks */
  uint	blocksize;			/* blocksize of keyfile */
  ulong min_pack_length;		/* Theese are used by packed data */
  ulong max_pack_length;
  ulong state_diff_length;
  my_bool  changed,			/* If changed since lock */
    global_changed,			/* If changed since open */
    not_flushed,
    temporary,delay_key_write,
    concurrent_insert;
  myf write_flag;
  int	rnd;				/* rnd-counter */
  MI_DECODE_TREE *decode_trees;
  uint16 *decode_tables;
  enum data_file_type data_file_type;
  int (*read_record)(struct st_myisam_info*, my_off_t, byte*);
  int (*write_record)(struct st_myisam_info*, const byte*);
  int (*update_record)(struct st_myisam_info*, my_off_t, const byte*);
  int (*delete_record)(struct st_myisam_info*);
  int (*read_rnd)(struct st_myisam_info*, byte*, my_off_t, my_bool);
  int (*compare_record)(struct st_myisam_info*, const byte *);
  ha_checksum (*calc_checksum)(struct st_myisam_info*, const byte *);
  int (*compare_unique)(struct st_myisam_info*, MI_UNIQUEDEF *,
			const byte *record, my_off_t pos);
#ifdef THREAD
  THR_LOCK lock;
  pthread_mutex_t intern_lock;		/* Locking for use with _locking */
  rw_lock_t *key_root_lock;
#endif
} MYISAM_SHARE;


typedef uint mi_bit_type;

typedef struct st_mi_bit_buff {		/* Used for packing of record */
  mi_bit_type current_byte;
  uint bits;
  uchar *pos,*end,*blob_pos;
  uint error;
} MI_BIT_BUFF;


struct st_myisam_info {
  MYISAM_SHARE *s;			/* Shared between open:s */
  MI_STATUS_INFO *state,save_state;
  MI_BLOB     *blobs;			/* Pointer to blobs */
  int dfile;				/* The datafile */
  MI_BIT_BUFF   bit_buff;
  uint	opt_flag;			/* Optim. for space/speed */
  uint update;				/* If file changed since open */
  char *filename;			/* parameter to open filename */
  ulong this_unique;			/* uniq filenumber or thread */
  ulong last_unique;			/* last unique number */
  ulong this_loop;			/* counter for this open */
  ulong last_loop;			/* last used counter */
  my_off_t lastpos,			/* Last record position */
	nextpos;			/* Position to next record */
  my_off_t save_lastpos;
  my_off_t pos;				/* Intern variable */
  ha_checksum checksum;
  ulong packed_length,blob_length;	/* Length of found, packed record */
  uint	alloced_rec_buff_length;	/* Max recordlength malloced */
  uchar *buff,				/* Temp area for key */
	*lastkey,*lastkey2;		/* Last used search key */
  byte	*rec_buff,			/* Tempbuff for recordpack */
	*rec_alloc;			/* Malloced area for record */
  uchar *int_keypos,			/* Save position for next/previous */
    *int_maxpos;			/*  -""-  */
  uint32 int_keytree_version;		/*  -""-  */
  uint   int_nod_flag;			/*  -""-  */
  my_off_t last_keypage;		/* Last key page read */
  my_off_t last_search_keypage;		/* Last keypage when searching */
  my_off_t dupp_key_pos;
  int	lastinx;			/* Last used index */
  uint	lastkey_length;			/* Length of key in lastkey */
  uint	last_rkey_length;		/* Last length in mi_rkey() */
  uint  save_lastkey_length;
  int	errkey;				/* Got last error on this key */
  int   lock_type;			/* How database was locked */
  int   tmp_lock_type;			/* When locked by readinfo */
  uint	data_changed;			/* Somebody has changed data */
  uint	save_update;			/* When using KEY_READ */
  int	save_lastinx;
  my_bool was_locked;			/* Was locked in panic */
  my_bool quick_mode;
  my_bool page_changed;		/* If info->buff can't be used for rnext */
  my_bool buff_used;		/* If info->buff has to be reread for rnext */
  myf lock_wait;			/* is 0 or MY_DONT_WAIT */
  int (*read_record)(struct st_myisam_info*, my_off_t, byte*);
  LIST	open_list;
  IO_CACHE rec_cache;			/* When cacheing records */
#ifdef THREAD
  THR_LOCK_DATA lock;
#endif
};


	/* Some defines used by isam-funktions */

#define USE_WHOLE_KEY	MI_MAX_KEY_BUFF*2 /* Use whole key in _mi_search() */
#define F_EXTRA_LCK	-1

	/* bits in opt_flag */
#define MEMMAP_USED	32
#define REMEMBER_OLD_POS 64

#define WRITEINFO_UPDATE_KEYFILE	1
#define WRITEINFO_NO_UNLOCK		2

#define mi_getint(x)	((uint) mi_uint2korr(x) & 32767)
#define mi_putint(x,y,nod) { uint16 boh=(nod ? (uint16) 32768 : 0) + (uint16) (y);\
			  mi_int2store(x,boh); }
#define mi_test_if_nod(x) (x[0] & 128 ? info->s->base.key_reflength : 0)
#define mi_mark_crashed(x) (x)->s->state.changed|=2
#define mi_mark_crashed_on_repair(x) (x)->s->state.changed|=4+2
#define mi_is_crashed(x) ((x)->s->state.changed & 2)
#define mi_is_crashed_on_repair(x) ((x)->s->state.changed & 4)

/* Functions to store length of space packed keys, VARCHAR or BLOB keys */

#define store_key_length_inc(key,length) \
{ if ((length) < 255) \
  { *(key)++=(length); } \
  else \
  { *(key)=255; mi_int2store((key)+1,(length)); (key)+=3; } \
}

#define store_key_length(key,length) \
{ if ((length) < 255) \
  { *(key)=(length); } \
  else \
  { *(key)=255; mi_int2store((key)+1,(length)); } \
}

#define get_key_length(length,key) \
{ if ((uchar) *(key) != 255) \
    length= (uint) (uchar) *((key)++); \
  else \
  { length=mi_uint2korr((key)+1); (key)+=3; } \
}

#define get_key_full_length(length,key) \
{ if ((uchar) *(key) != 255) \
    length= ((uint) (uchar) *((key)++))+1; \
  else \
  { length=mi_uint2korr((key)+1)+3; (key)+=3; } \
}

#define get_key_pack_length(length,length_pack,key) \
{ if ((uchar) *(key) != 255) \
  { length= (uint) (uchar) *((key)++); length_pack=1; }\
  else \
  { length=mi_uint2korr((key)+1); (key)+=3; length_pack=3; } \
}

#define get_pack_length(length) ((length) >= 255 ? 3 : 1)

#define MI_MIN_BLOCK_LENGTH	20	/* Because of delete-link */
#define MI_EXTEND_BLOCK_LENGTH	20	/* Don't use to small record-blocks */
#define MI_SPLIT_LENGTH	((MI_EXTEND_BLOCK_LENGTH+4)*2)
#define MI_MAX_DYN_BLOCK_HEADER	20	/* Max prefix of record-block */
#define MI_BLOCK_INFO_HEADER_LENGTH 20
#define MI_DYN_DELETE_BLOCK_HEADER 20	/* length of delete-block-header */
#define MI_DYN_MAX_BLOCK_LENGTH	((1L << 24)-4L)
#define MI_DYN_MAX_ROW_LENGTH	(MI_DYN_MAX_BLOCK_LENGTH - MI_SPLIT_LENGTH)
#define MI_DYN_ALIGN_SIZE	4	/* Align blocks on this */
#define MI_MAX_DYN_HEADER_BYTE	12	/* max header byte for dynamic rows */

#define MEMMAP_EXTRA_MARGIN	7	/* Write this as a suffix for file */

#define PACK_TYPE_SELECTED	1	/* Bits in field->pack_type */
#define PACK_TYPE_SPACE_FIELDS	2
#define PACK_TYPE_ZERO_FILL	4
#define MI_FOUND_WRONG_KEY 32738	/* Impossible value from _mi_key_cmp */

#define MI_KEY_BLOCK_LENGTH	1024	/* Min key block length */
#define MI_MAX_KEY_BLOCK_LENGTH	8192
#define MI_MAX_KEY_BLOCK_SIZE	(MI_MAX_KEY_BLOCK_LENGTH/MI_KEY_BLOCK_LENGTH)
#define MI_BLOCK_SIZE(key_length,data_pointer,key_pointer) ((((key_length+data_pointer+key_pointer)*4+key_pointer+2)/MI_KEY_BLOCK_LENGTH+1)*MI_KEY_BLOCK_LENGTH)
#define MI_MAX_KEYPTR_SIZE	5	/* For calculating block lengths */

/* The UNIQUE check is done with a hashed long key */

#define MI_UNIQUE_HASH_TYPE	HA_KEYTYPE_ULONG_INT
#define mi_unique_store(A,B)    mi_int4store((A),(B))

#ifdef THREAD
extern pthread_mutex_t THR_LOCK_myisam;
#endif
#if !defined(THREAD) || defined(DONT_USE_RW_LOCKS)
#define rw_wrlock(A) {}
#define rw_rdlock(A) {}
#define rw_unlock(A) {}
#endif

	/* Some extern variables */

extern LIST *myisam_open_list;
extern uchar NEAR myisam_file_magic[],NEAR myisam_pack_file_magic[];
extern uint NEAR myisam_read_vec[],NEAR myisam_readnext_vec[];
extern uint myisam_quick_table_bits;
extern File myisam_log_file;

	/* This is used by _mi_calc_xxx_key_length och _mi_store_key */

typedef struct st_mi_s_param
{
  uint	ref_length,key_length,
	n_ref_length,
	n_length,
	totlength,
	part_of_prev_key,prev_length,pack_marker;
  uchar *key, *prev_key,*next_key_pos;
  bool	store_not_null;
} MI_KEY_PARAM;

	/* Prototypes for intern functions */

extern int _mi_read_dynamic_record(MI_INFO *info,my_off_t filepos,byte *buf);
extern int _mi_write_dynamic_record(MI_INFO*, const byte*);
extern int _mi_update_dynamic_record(MI_INFO*, my_off_t, const byte*);
extern int _mi_delete_dynamic_record(MI_INFO *info);
extern int _mi_cmp_dynamic_record(MI_INFO *info,const byte *record);
extern int _mi_read_rnd_dynamic_record(MI_INFO *, byte *,my_off_t, my_bool);
extern int _mi_write_blob_record(MI_INFO*, const byte*);
extern int _mi_update_blob_record(MI_INFO*, my_off_t, const byte*);
extern int _mi_read_static_record(MI_INFO *info, my_off_t filepos,byte *buf);
extern int _mi_write_static_record(MI_INFO*, const byte*);
extern int _mi_update_static_record(MI_INFO*, my_off_t, const byte*);
extern int _mi_delete_static_record(MI_INFO *info);
extern int _mi_cmp_static_record(MI_INFO *info,const byte *record);
extern int _mi_read_rnd_static_record(MI_INFO*, byte *,my_off_t, my_bool);
extern int _mi_ck_write(MI_INFO *info,uint keynr,uchar *key,uint length);
extern int _mi_enlarge_root(MI_INFO *info,uint keynr,uchar *key);
extern int _mi_insert(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *key,
		      uchar *anc_buff,uchar *key_pos,uchar *key_buff,
		      uchar *father_buff, uchar *father_keypos,
		      my_off_t father_page, my_bool insert_last);
extern int _mi_split_page(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *key,
			  uchar *buff,uchar *key_buff, my_bool insert_last);
extern uchar *_mi_find_half_pos(uint nod_flag,MI_KEYDEF *keyinfo,uchar *page,
				uchar *key,uint *return_key_length,
				uchar **after_key);
extern int _mi_calc_static_key_length(MI_KEYDEF *keyinfo,uint nod_flag,
				      uchar *key_pos, uchar *org_key,
				      uchar *key_buff,
				      uchar *key, MI_KEY_PARAM *s_temp);
extern int _mi_calc_var_key_length(MI_KEYDEF *keyinfo,uint nod_flag,
				   uchar *key_pos, uchar *org_key,
				   uchar *key_buff,
				   uchar *key, MI_KEY_PARAM *s_temp);
extern int _mi_calc_var_pack_key_length(MI_KEYDEF *keyinfo,uint nod_flag,
					uchar *key_pos, uchar *org_key,
					uchar *prev_key,
					uchar *key, MI_KEY_PARAM *s_temp);
extern int _mi_calc_bin_pack_key_length(MI_KEYDEF *keyinfo,uint nod_flag,
					uchar *key_pos,uchar *org_key,
					uchar *prev_key,
					uchar *key, MI_KEY_PARAM *s_temp);
void _mi_store_static_key(MI_KEYDEF *keyinfo,  uchar *key_pos,
			   MI_KEY_PARAM *s_temp);
void _mi_store_var_pack_key(MI_KEYDEF *keyinfo,  uchar *key_pos,
			     MI_KEY_PARAM *s_temp);
#ifdef NOT_USED
void _mi_store_pack_key(MI_KEYDEF *keyinfo,  uchar *key_pos,
			 MI_KEY_PARAM *s_temp);
#endif
void _mi_store_bin_pack_key(MI_KEYDEF *keyinfo,  uchar *key_pos,
			    MI_KEY_PARAM *s_temp);

extern int _mi_ck_delete(MI_INFO *info,uint keynr,uchar *key,uint key_length);
extern int _mi_readinfo(MI_INFO *info,int lock_flag,int check_keybuffer);
extern int _mi_writeinfo(MI_INFO *info,uint options);
extern int _mi_test_if_changed(MI_INFO *info);
extern int _mi_mark_file_changed(MI_INFO *info);
extern int _mi_decrement_open_count(MI_INFO *info);
extern int _mi_check_index(MI_INFO *info,int inx);
extern int _mi_search(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *key,uint key_len,
		      uint nextflag,my_off_t pos);
extern int _mi_bin_search(struct st_myisam_info *info,MI_KEYDEF *keyinfo,
			  uchar *page,uchar *key,uint key_len,uint comp_flag,
			  uchar * *ret_pos,uchar *buff, my_bool *was_last_key);
extern int _mi_seq_search(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *page,
			  uchar *key,uint key_len,uint comp_flag,
			  uchar **ret_pos,uchar *buff, my_bool *was_last_key);
extern int _mi_compare_text(CHARSET_INFO *, uchar *, uint, uchar *, uint ,
			    my_bool);
extern my_off_t _mi_kpos(uint nod_flag,uchar *after_key);
extern void _mi_kpointer(MI_INFO *info,uchar *buff,my_off_t pos);
extern my_off_t _mi_dpos(MI_INFO *info, uint nod_flag,uchar *after_key);
extern my_off_t _mi_rec_pos(MYISAM_SHARE *info, uchar *ptr);
extern void _mi_dpointer(MI_INFO *info, uchar *buff,my_off_t pos);
extern int _mi_key_cmp(MI_KEYSEG *keyseg, uchar *a,uchar *b,
		       uint key_length,uint nextflag,uint *diff_length);
extern uint _mi_get_static_key(MI_KEYDEF *keyinfo,uint nod_flag,uchar * *page,
			       uchar *key);
extern uint _mi_get_pack_key(MI_KEYDEF *keyinfo,uint nod_flag,uchar * *page,
			     uchar *key);
extern uint _mi_get_binary_pack_key(MI_KEYDEF *keyinfo, uint nod_flag,
				    uchar **page_pos, uchar *key);
extern uchar *_mi_get_last_key(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *keypos,
			       uchar *lastkey,uchar *endpos,
			       uint *return_key_length);
extern uchar *_mi_get_key(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page,
			  uchar *key, uchar *keypos, uint *return_key_length);
extern uint _mi_keylength(MI_KEYDEF *keyinfo,uchar *key);
extern uchar *_mi_move_key(MI_KEYDEF *keyinfo,uchar *to,uchar *from);
extern int _mi_search_next(MI_INFO *info,MI_KEYDEF *keyinfo,uchar *key,
			   uint key_length,uint nextflag,my_off_t pos);
extern int _mi_search_first(MI_INFO *info,MI_KEYDEF *keyinfo,my_off_t pos);
extern int _mi_search_last(MI_INFO *info,MI_KEYDEF *keyinfo,my_off_t pos);
extern uchar *_mi_fetch_keypage(MI_INFO *info,MI_KEYDEF *keyinfo,my_off_t page,
				uchar *buff,int return_buffer);
extern int _mi_write_keypage(MI_INFO *info,MI_KEYDEF *keyinfo,my_off_t page,
			     uchar *buff);
extern int _mi_dispose(MI_INFO *info,MI_KEYDEF *keyinfo,my_off_t pos);
extern my_off_t _mi_new(MI_INFO *info,MI_KEYDEF *keyinfo);
extern uint _mi_make_key(MI_INFO *info,uint keynr,uchar *key,
			 const byte *record,my_off_t filepos);
extern uint _mi_pack_key(MI_INFO *info,uint keynr,uchar *key,uchar *old,
			 uint key_length);
extern int _mi_read_key_record(MI_INFO *info,my_off_t filepos,byte *buf);
extern int _mi_read_cache(IO_CACHE *info,byte *buff,my_off_t pos,
			  uint length,int re_read_if_possibly);
extern void update_auto_increment(MI_INFO *info,const byte *record);
extern byte *mi_fix_rec_buff_for_blob(MI_INFO *info,ulong blob_length);
extern ulong _mi_rec_unpack(MI_INFO *info,byte *to,byte *from,
			    ulong reclength);
extern my_bool _mi_rec_check(MI_INFO *info,const char *from);
extern int _mi_write_part_record(MI_INFO *info,my_off_t filepos,ulong length,
				 my_off_t next_filepos,byte **record,
				 ulong *reclength,int *flag);
extern void _mi_print_key(FILE *stream,MI_KEYSEG *keyseg,const uchar *key,
			  uint length);
extern my_bool _mi_read_pack_info(MI_INFO *info,pbool fix_keys);
extern int _mi_read_pack_record(MI_INFO *info,my_off_t filepos,byte *buf);
extern int _mi_read_rnd_pack_record(MI_INFO*, byte *,my_off_t, my_bool);
extern int _mi_pack_rec_unpack(MI_INFO *info,byte *to,byte *from,
			       ulong reclength);
extern ulonglong mi_safe_mul(ulonglong a,ulonglong b);

struct st_sort_info;


typedef struct st_mi_block_info {	/* Parameter to _mi_get_block_info */
  uchar header[MI_BLOCK_INFO_HEADER_LENGTH];
  ulong rec_len;
  ulong data_len;
  ulong block_len;
  ulong blob_len;
  my_off_t filepos;
  my_off_t next_filepos;
  my_off_t prev_filepos;
  uint second_read;
  uint offset;
} MI_BLOCK_INFO;

	/* bits in return from _mi_get_block_info */

#define BLOCK_FIRST	1
#define BLOCK_LAST	2
#define BLOCK_DELETED	4
#define BLOCK_ERROR	8	/* Wrong data */
#define BLOCK_SYNC_ERROR 16	/* Right data at wrong place */
#define BLOCK_FATAL_ERROR 32	/* hardware-error */

#define NEAD_MEM	((uint) 10*4*(IO_SIZE+32)+32) /* Nead for recursion */
#define MAXERR			20
#define BUFFERS_WHEN_SORTING	16		/* Alloc for sort-key-tree */
#define WRITE_COUNT		MY_HOW_OFTEN_TO_WRITE
#define INDEX_TMP_EXT		".TMM"
#define DATA_TMP_EXT		".TMD"

#define UPDATE_TIME		1
#define UPDATE_STAT		2
#define UPDATE_SORT		4
#define UPDATE_AUTO_INC		8
#define UPDATE_OPEN_COUNT	16

#define USE_BUFFER_INIT		(((1024L*512L-MALLOC_OVERHEAD)/IO_SIZE)*IO_SIZE)
#define READ_BUFFER_INIT	(1024L*256L-MALLOC_OVERHEAD)
#define SORT_BUFFER_INIT	(2048L*1024L-MALLOC_OVERHEAD)
#define MIN_SORT_BUFFER		(4096-MALLOC_OVERHEAD)

enum myisam_log_commands {
  MI_LOG_OPEN,MI_LOG_WRITE,MI_LOG_UPDATE,MI_LOG_DELETE,MI_LOG_CLOSE,MI_LOG_EXTRA,MI_LOG_LOCK,MI_LOG_DELETE_ALL
};

#define myisam_log(a,b,c,d) if (myisam_log_file >= 0) _myisam_log(a,b,c,d)
#define myisam_log_command(a,b,c,d,e) if (myisam_log_file >= 0) _myisam_log_command(a,b,c,d,e)
#define myisam_log_record(a,b,c,d,e) if (myisam_log_file >= 0) _myisam_log_record(a,b,c,d,e)

extern uint _mi_get_block_info(MI_BLOCK_INFO *,File, my_off_t);
extern uint _mi_rec_pack(MI_INFO *info,byte *to,const byte *from);
extern uint _mi_pack_get_block_info(MI_INFO *mysql, MI_BLOCK_INFO *, File,
				    my_off_t, char *rec_buf);
extern void _my_store_blob_length(byte *pos,uint pack_length,uint length);
extern void _myisam_log(enum myisam_log_commands command,MI_INFO *info,
		       const byte *buffert,uint length);
extern void _myisam_log_command(enum myisam_log_commands command,
			       MI_INFO *info, const byte *buffert,
			       uint length, int result);
extern void _myisam_log_record(enum myisam_log_commands command,MI_INFO *info,
			      const byte *record,my_off_t filepos,
			      int result);
extern my_bool _mi_memmap_file(MI_INFO *info);
extern void _mi_unmap_file(MI_INFO *info);
extern uint save_pack_length(byte *block_buff,ulong length);

uint mi_state_info_write(File file, MI_STATE_INFO *state, uint pWrite);
char *mi_state_info_read(char *ptr, MI_STATE_INFO *state);
uint mi_state_info_read_dsk(File file, MI_STATE_INFO *state, my_bool pRead);
uint mi_base_info_write(File file, MI_BASE_INFO *base);
char *my_n_base_info_read(char *ptr, MI_BASE_INFO *base);
int mi_keyseg_write(File file, const MI_KEYSEG *keyseg);
char *mi_keyseg_read(char *ptr, MI_KEYSEG *keyseg);
uint mi_keydef_write(File file, MI_KEYDEF *keydef);
char *mi_keydef_read(char *ptr, MI_KEYDEF *keydef);
uint mi_uniquedef_write(File file, MI_UNIQUEDEF *keydef);
char *mi_uniquedef_read(char *ptr, MI_UNIQUEDEF *keydef);
uint mi_recinfo_write(File file, MI_COLUMNDEF *recinfo);
char *mi_recinfo_read(char *ptr, MI_COLUMNDEF *recinfo);
ulong _my_calc_total_blob_length(MI_INFO *info, const byte *record);
ha_checksum mi_checksum(MI_INFO *info, const byte *buf);
ha_checksum mi_static_checksum(MI_INFO *info, const byte *buf);
my_bool mi_check_unique(MI_INFO *info, MI_UNIQUEDEF *def, byte *record,
		     ha_checksum unique_hash, my_off_t pos);
ha_checksum mi_unique_hash(MI_UNIQUEDEF *def, const byte *buf);
int _mi_cmp_static_unique(MI_INFO *info, MI_UNIQUEDEF *def,
			   const byte *record, my_off_t pos);
int _mi_cmp_dynamic_unique(MI_INFO *info, MI_UNIQUEDEF *def,
			   const byte *record, my_off_t pos);
int mi_unique_comp(MI_UNIQUEDEF *def, const byte *a, const byte *b,
		   my_bool null_are_equal);
void mi_get_status(void* param);
void mi_update_status(void* param);
void mi_copy_status(void* to,void *from);
my_bool mi_check_status(void* param);
void mi_dectivate_non_unique_index(MI_INFO *info, ha_rows rows);

/* Functions needed by mi_check */
#ifdef	__cplusplus
extern "C" {
#endif
void mi_check_print_error _VARARGS((MI_CHECK *param, const char *fmt,...));
void mi_check_print_warning _VARARGS((MI_CHECK *param, const char *fmt,...));
void mi_check_print_info _VARARGS((MI_CHECK *param, const char *fmt,...));

#ifdef __cplusplus
}
#endif

