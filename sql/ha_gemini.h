/* Copyright (C) 2000 MySQL AB & NuSphere Corporation
   
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
#pragma interface			/* gcc class implementation */
#endif

#include "gem_global.h"
#include "dstd.h"
#include "dsmpub.h"

/* class for the the gemini handler */

enum enum_key_string_options{KEY_CREATE,KEY_DELETE,KEY_CHECK};
typedef struct st_gemini_share {
  ha_rows  *rec_per_key;
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length,use_count;
} GEM_SHARE;

typedef struct gemBlobDesc
{
  dsmBlobId_t  blobId;
  dsmBuffer_t *pBlob;
} gemBlobDesc_t;

class ha_gemini: public handler
{
  /* define file as an int for now until we have a real file struct */
  int file;
  uint int_option_flag;
  int tableNumber;
  dsmIndex_t  *pindexNumbers;  // dsm object numbers for the indexes on this table 
  dsmRecid_t lastRowid;
  uint  last_dup_key;
  bool fixed_length_row, key_read, using_ignore;
  byte *rec_buff;
  dsmKey_t *pbracketBase;
  dsmKey_t *pbracketLimit;
  dsmKey_t *pfoundKey;
  dsmMask_t    tableStatus;   // Crashed/repair status
  gemBlobDesc_t *pBlobDescs;

  int index_open(char *tableName);
  int  pack_row(byte **prow, int *ppackedLength, const byte *record, 
                bool update);
  int  unpack_row(char *record, char *prow);
  int findRow(THD *thd, dsmMask_t findMode, byte *buf);
  int fetch_row(void *gemini_context, const byte *buf);
  int handleIndexEntries(const byte * record, dsmRecid_t recid,
                                  enum_key_string_options option);

  int handleIndexEntry(const byte * record, dsmRecid_t recid,
				enum_key_string_options option,uint keynr);

  int createKeyString(const byte * record, KEY *pkeyinfo,
                               unsigned char *pkeyBuf, int bufSize,
                               int  *pkeyStringLen, short geminiIndexNumber,
                               bool *thereIsAnull);
  int fullCheck(THD *thd,byte *buf);

  int pack_key( uint keynr, dsmKey_t  *pkey, 
                           const byte *key_ptr, uint key_length);

  void unpack_key(char *record, dsmKey_t *key, uint index);

  int key_cmp(uint keynr, const byte * old_row,
                         const byte * new_row, bool updateStats);

  int saveKeyStats(THD *thd);
  void get_index_stats(THD *thd);

  short cursorId;  /* cursorId of active index cursor if any   */
  dsmMask_t  lockMode;  /* Shared or exclusive      */

  /* FIXFIX Don't know why we need this because I don't know what
     store_lock method does but we core dump without this  */
  THR_LOCK_DATA lock;
  GEM_SHARE     *share;

 public:
  ha_gemini(TABLE *table): handler(table), file(0),
    int_option_flag(HA_READ_NEXT | HA_READ_PREV |
		    HA_REC_NOT_IN_SEQ |
		    HA_KEYPOS_TO_RNDPOS | HA_READ_ORDER | HA_LASTKEY_ORDER |
		    HA_LONGLONG_KEYS | HA_NULL_KEY | HA_HAVE_KEY_READ_ONLY |
		    HA_BLOB_KEY | 
		    HA_NO_TEMP_TABLES | HA_NO_FULLTEXT_KEY |
		    /*HA_NOT_EXACT_COUNT | */ 
		    /*HA_KEY_READ_WRONG_STR |*/ HA_DROP_BEFORE_CREATE),
                    pbracketBase(0),pbracketLimit(0),pfoundKey(0),
                    cursorId(0)
  {
  }
  ~ha_gemini() {}
  const char *table_type() const { return "Gemini"; }
  const char **bas_ext() const;
  ulong option_flag() const { return int_option_flag; }
  uint max_record_length() const { return MAXRECSZ; }
  uint max_keys()          const { return MAX_KEY-1; }
  uint max_key_parts()     const { return MAX_REF_PARTS; }
  uint max_key_length()    const { return MAXKEYSZ / 2; }
  bool fast_key_read()	   { return 1;}
  bool has_transactions()  { return 1;}

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  double scan_time();
  int write_row(byte * buf);
  int update_row(const byte * old_data, byte * new_data);
  int delete_row(const byte * buf);
  int index_init(uint index);
  int index_end();
  int index_read(byte * buf, const byte * key,
		 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(byte * buf, uint index, const byte * key,
		     uint key_len, enum ha_rkey_function find_flag);
  int index_next(byte * buf);
  int index_next_same(byte * buf, const byte *key, uint keylen);
  int index_prev(byte * buf);
  int index_first(byte * buf);
  int index_last(byte * buf);
  int rnd_init(bool scan=1);
  int rnd_end();
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  void position(const byte *record);
  void info(uint);
  int extra(enum ha_extra_function operation);
  int reset(void);
  int analyze(THD* thd, HA_CHECK_OPT* check_opt);
  int check(THD* thd, HA_CHECK_OPT* check_opt);
  int repair(THD* thd,  HA_CHECK_OPT* check_opt);
  int restore(THD* thd, HA_CHECK_OPT* check_opt);
  int backup(THD* thd, HA_CHECK_OPT* check_opt);
  int optimize(THD* thd, HA_CHECK_OPT* check_opt);
  int external_lock(THD *thd, int lock_type);
  virtual longlong get_auto_increment();
  void position(byte *record);
  ha_rows records_in_range(int inx,
			   const byte *start_key,uint start_key_len,
			   enum ha_rkey_function start_search_flag,
			   const byte *end_key,uint end_key_len,
			   enum ha_rkey_function end_search_flag);
  void update_create_info(HA_CREATE_INFO *create_info);
  int create(const char *name, register TABLE *form,
	     HA_CREATE_INFO *create_info);
  int delete_table(const char *name);
  int rename_table(const char* from, const char* to);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);
};

#define GEMOPT_FLUSH_LOG          0x00000001
#define GEMOPT_UNBUFFERED_IO      0x00000002

#define GEMINI_RECOVERY_FULL      0x00000001
#define GEMINI_RECOVERY_NONE      0x00000002
#define GEMINI_RECOVERY_FORCE     0x00000004

#define GEM_OPTID_SPIN_RETRIES      1

extern bool gemini_skip;
extern SHOW_COMP_OPTION have_gemini;
extern long gemini_options;
extern long gemini_buffer_cache;
extern long gemini_io_threads;
extern long gemini_log_cluster_size;
extern long gemini_locktablesize;
extern long gemini_lock_wait_timeout;
extern long gemini_spin_retries;
extern long gemini_connection_limit;
extern char *gemini_basedir;
extern TYPELIB gemini_recovery_typelib;
extern ulong gemini_recovery_options;

bool gemini_init(void);
bool gemini_end(void);
bool gemini_flush_logs(void);
int gemini_commit(THD *thd);
int gemini_rollback(THD *thd);
int gemini_recovery_logging(THD *thd, bool on);
void gemini_disconnect(THD *thd);
int gemini_rollback_to_savepoint(THD *thd);
int gemini_parse_table_name(const char *fullname, char *dbname, char *tabname);
int gemini_is_vst(const char *pname);
int gemini_set_option_long(int optid, long optval);

const int gemini_blocksize = BLKSIZE;
const int gemini_recbits = DEFAULT_RECBITS;

extern "C" void uttrace(void);
