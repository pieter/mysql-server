/* Copyright (C) 2000-2003 MySQL AB

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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

/*
  This file defines the NDB Cluster handler: the interface between MySQL and
  NDB Cluster
*/


#ifdef __GNUC__
#pragma implementation                          // gcc: Class implementation
#endif

#include "mysql_priv.h"

#ifdef HAVE_NDBCLUSTER_DB
#include <my_dir.h>
#include "ha_ndbcluster.h"
#include <ndbapi/NdbApi.hpp>
#include <ndbapi/NdbScanFilter.hpp>

#define USE_DISCOVER_ON_STARTUP
//#define USE_NDB_POOL

// Default value for parallelism
static const int parallelism= 240;

// Default value for max number of transactions
// createable against NDB from this handler
static const int max_transactions = 256;

#define NDB_HIDDEN_PRIMARY_KEY_LENGTH 8


#define ERR_PRINT(err) \
  DBUG_PRINT("error", ("Error: %d  message: %s", err.code, err.message))

#define ERR_RETURN(err)		         \
{				         \
  ERR_PRINT(err);		         \
  DBUG_RETURN(ndb_to_mysql_error(&err)); \
}

// Typedefs for long names
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table  NDBTAB;
typedef NdbDictionary::Index  NDBINDEX;
typedef NdbDictionary::Dictionary  NDBDICT;

bool ndbcluster_inited= false;

static Ndb* g_ndb= NULL;

// Handler synchronization
pthread_mutex_t ndbcluster_mutex;

// Table lock handling
static HASH ndbcluster_open_tables;

static byte *ndbcluster_get_key(NDB_SHARE *share,uint *length,
                                my_bool not_used __attribute__((unused)));
static NDB_SHARE *get_share(const char *table_name);
static void free_share(NDB_SHARE *share);

static int packfrm(const void *data, uint len, const void **pack_data, uint *pack_len);
static int unpackfrm(const void **data, uint *len,
		     const void* pack_data);

/*
  Error handling functions
*/

struct err_code_mapping
{
  int ndb_err;
  int my_err;
};

static const err_code_mapping err_map[]= 
{
  { 626, HA_ERR_KEY_NOT_FOUND },
  { 630, HA_ERR_FOUND_DUPP_KEY },
  { 893, HA_ERR_FOUND_DUPP_UNIQUE },
  { 721, HA_ERR_TABLE_EXIST },
  { 4244, HA_ERR_TABLE_EXIST },
  { 241, HA_ERR_OLD_METADATA },

  { 266, HA_ERR_LOCK_WAIT_TIMEOUT },
  { 274, HA_ERR_LOCK_WAIT_TIMEOUT },
  { 296, HA_ERR_LOCK_WAIT_TIMEOUT },
  { 297, HA_ERR_LOCK_WAIT_TIMEOUT },
  { 237, HA_ERR_LOCK_WAIT_TIMEOUT },

  { 623, HA_ERR_RECORD_FILE_FULL },
  { 624, HA_ERR_RECORD_FILE_FULL },
  { 625, HA_ERR_RECORD_FILE_FULL },
  { 826, HA_ERR_RECORD_FILE_FULL },
  { 827, HA_ERR_RECORD_FILE_FULL },
  { 832, HA_ERR_RECORD_FILE_FULL },

  { -1, -1 }
};


static int ndb_to_mysql_error(const NdbError *err)
{
  uint i;
  for (i=0 ; err_map[i].ndb_err != err->code ; i++)
  {
    if (err_map[i].my_err == -1)
      return err->code;
  }
  return err_map[i].my_err;
}


/*
  Take care of the error that occured in NDB
  
  RETURN
    0	No error
    #   The mapped error code
*/

int ha_ndbcluster::ndb_err(NdbConnection *trans)
{
  const NdbError err= trans->getNdbError();
  if (!err.code)
    return 0;			// Don't log things to DBUG log if no error
  DBUG_ENTER("ndb_err");
  
  ERR_PRINT(err);
  switch (err.classification) {
  case NdbError::SchemaError:
  {
    NDBDICT *dict= m_ndb->getDictionary();
    DBUG_PRINT("info", ("invalidateTable %s", m_tabname));
    dict->invalidateTable(m_tabname);
    break;
  }
  default:
    break;
  }
  DBUG_RETURN(ndb_to_mysql_error(&err));
}


/*
  Override the default get_error_message in order to add the 
  error message of NDB 
 */

bool ha_ndbcluster::get_error_message(int error, 
				      String *buf)
{
  DBUG_ENTER("ha_ndbcluster::get_error_message");
  DBUG_PRINT("enter", ("error: %d", error));

  if (!m_ndb)
    DBUG_RETURN(false);

  const NdbError err= m_ndb->getNdbError(error);
  bool temporary= err.status==NdbError::TemporaryError;
  buf->set(err.message, strlen(err.message), &my_charset_bin);
  DBUG_PRINT("exit", ("message: %s, temporary: %d", buf->ptr(), temporary));
  DBUG_RETURN(temporary);
}


/*
  Instruct NDB to set the value of the hidden primary key
*/

bool ha_ndbcluster::set_hidden_key(NdbOperation *ndb_op,
				   uint fieldnr, const byte *field_ptr)
{
  DBUG_ENTER("set_hidden_key");
  DBUG_RETURN(ndb_op->equal(fieldnr, (char*)field_ptr,
			    NDB_HIDDEN_PRIMARY_KEY_LENGTH) != 0);
}


/*
  Instruct NDB to set the value of one primary key attribute
*/

int ha_ndbcluster::set_ndb_key(NdbOperation *ndb_op, Field *field,
                               uint fieldnr, const byte *field_ptr)
{
  uint32 pack_len= field->pack_length();
  DBUG_ENTER("set_ndb_key");
  DBUG_PRINT("enter", ("%d: %s, ndb_type: %u, len=%d", 
                       fieldnr, field->field_name, field->type(),
                       pack_len));
  DBUG_DUMP("key", (char*)field_ptr, pack_len);
  
  switch (field->type()) {
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:  
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_FLOAT: 
  case MYSQL_TYPE_DOUBLE:
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_STRING:
    // Common implementation for most field types
    DBUG_RETURN(ndb_op->equal(fieldnr, (char*) field_ptr, pack_len) != 0);
    
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_NULL:   
  case MYSQL_TYPE_GEOMETRY:
  default:
    // Unhandled field types
    DBUG_PRINT("error", ("Field type %d not supported", field->type()));
    DBUG_RETURN(2);
  }
  DBUG_RETURN(3);
}


/*
 Instruct NDB to set the value of one attribute
*/

int ha_ndbcluster::set_ndb_value(NdbOperation *ndb_op, Field *field, 
                                 uint fieldnr)
{
  const byte* field_ptr= field->ptr;
  uint32 pack_len=  field->pack_length();
  DBUG_ENTER("set_ndb_value");
  DBUG_PRINT("enter", ("%d: %s, type: %u, len=%d, is_null=%s", 
                       fieldnr, field->field_name, field->type(), 
                       pack_len, field->is_null()?"Y":"N"));
  DBUG_DUMP("value", (char*) field_ptr, pack_len);
  
  if (field->is_null())
  {
    // Set value to NULL
    DBUG_RETURN((ndb_op->setValue(fieldnr, (char*)NULL, pack_len) != 0));
  }    
   
  switch (field->type()) {
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:  
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_FLOAT: 
  case MYSQL_TYPE_DOUBLE:
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_STRING:
    // Common implementation for most field types
    DBUG_RETURN(ndb_op->setValue(fieldnr, (char*)field_ptr, pack_len) != 0);
    
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_NULL:   
  case MYSQL_TYPE_GEOMETRY:
  default:
    // Unhandled field types
    DBUG_PRINT("error", ("Field type %d not supported", field->type()));
    DBUG_RETURN(2);
  }
  DBUG_RETURN(3);
}


/*
  Instruct NDB to fetch one field
  - data is read directly into buffer provided by field_ptr
    if it's NULL, data is read into memory provided by NDBAPI
*/

int ha_ndbcluster::get_ndb_value(NdbOperation *op, 
				 uint field_no, byte *field_ptr)
{
  DBUG_ENTER("get_ndb_value");
  DBUG_PRINT("enter", ("field_no: %d", field_no));
  m_value[field_no]= op->getValue(field_no, field_ptr);
  DBUG_RETURN(m_value == NULL);
}


/*
  Get metadata for this table from NDB 

  IMPLEMENTATION
    - save the NdbDictionary::Table for easy access
    - check that frm-file on disk is equal to frm-file
      of table accessed in NDB
    - build a list of the indexes for the table
*/

int ha_ndbcluster::get_metadata(const char *path)
{
  NDBDICT *dict= m_ndb->getDictionary();
  const NDBTAB *tab;
  const void *data, *pack_data;
  const char **key_name;
  uint ndb_columns, mysql_columns, length, pack_length;
  int error;
  DBUG_ENTER("get_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s, path: %s", m_tabname, path));

  if (!(tab= dict->getTable(m_tabname)))
    ERR_RETURN(dict->getNdbError());
  DBUG_PRINT("info", ("Table schema version: %d", tab->getObjectVersion()));
  
  /*
    This is the place to check that the table we got from NDB
    is equal to the one on local disk
  */
  ndb_columns=   (uint) tab->getNoOfColumns();
  mysql_columns= table->fields;
  if (table->primary_key == MAX_KEY)
    ndb_columns--;
  if (ndb_columns != mysql_columns)
  {
    DBUG_PRINT("error",
               ("Wrong number of columns, ndb: %d mysql: %d", 
                ndb_columns, mysql_columns));
    DBUG_RETURN(HA_ERR_OLD_METADATA);
  }
  
  /*
    Compare FrmData in NDB with frm file from disk.
  */
  error= 0;
  if (readfrm(path, &data, &length) ||
      packfrm(data, length, &pack_data, &pack_length))
  {
    my_free((char*)data, MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*)pack_data, MYF(MY_ALLOW_ZERO_PTR));
    DBUG_RETURN(1);
  }
    
  if ((pack_length != tab->getFrmLength()) || 
      (memcmp(pack_data, tab->getFrmData(), pack_length)))
  {
    DBUG_PRINT("error", 
	       ("metadata, pack_length: %d getFrmLength: %d memcmp: %d", 
		pack_length, tab->getFrmLength(),
		memcmp(pack_data, tab->getFrmData(), pack_length)));      
    DBUG_DUMP("pack_data", (char*)pack_data, pack_length);
    DBUG_DUMP("frm", (char*)tab->getFrmData(), tab->getFrmLength());
    error= HA_ERR_OLD_METADATA;
  }
  my_free((char*)data, MYF(0));
  my_free((char*)pack_data, MYF(0));
  if (error)
    DBUG_RETURN(error);

  // All checks OK, lets use the table
  m_table= (void*)tab;

  DBUG_RETURN(build_index_list());  
}

int ha_ndbcluster::build_index_list()
{
  char *name;
  const char *index_name;
  static const char* unique_suffix= "$unique";
  uint i, name_len;
  DBUG_ENTER("build_index_list");
  
  // Save information about all known indexes
  for (uint i= 0; i < table->keys; i++)
  {
    NDB_INDEX_TYPE idx_type= get_index_type_from_table(i);
    m_indextype[i]= idx_type;
    
    if (idx_type == UNIQUE_ORDERED_INDEX || idx_type == UNIQUE_INDEX)
    {
      index_name= get_index_name(i);
      name_len= strlen(index_name)+strlen(unique_suffix)+1;
      // Create name for unique index by appending "$unique";     
      if (!(name= my_malloc(name_len, MYF(MY_WME))))
	DBUG_RETURN(2);
      strxnmov(name, name_len, index_name, unique_suffix, NullS);
      m_unique_index_name[i]= name;      
      DBUG_PRINT("info", ("Created unique index name: %s for index %d",
			  name, i));
    }
  }
  DBUG_RETURN(0);
}



/*
  Decode the type of an index from information 
  provided in table object
*/
NDB_INDEX_TYPE ha_ndbcluster::get_index_type_from_table(uint inx) const
{
  bool is_hash_index=  (table->key_info[inx].algorithm == HA_KEY_ALG_HASH);
  if (inx == table->primary_key)
    return is_hash_index ? PRIMARY_KEY_INDEX : PRIMARY_KEY_ORDERED_INDEX;
  else
    return ((table->key_info[inx].flags & HA_NOSAME) ? 
	    (is_hash_index ? UNIQUE_INDEX : UNIQUE_ORDERED_INDEX) :
	    ORDERED_INDEX);
} 


void ha_ndbcluster::release_metadata()
{
  uint i;

  DBUG_ENTER("release_metadata");
  DBUG_PRINT("enter", ("m_tabname: %s", m_tabname));

  m_table= NULL;

  // Release index list 
  for (i= 0; i < MAX_KEY; i++)
  {
    if (m_unique_index_name[i])
      my_free((char*)m_unique_index_name[i], MYF(0));
    m_unique_index_name[i]= NULL;
  }

  DBUG_VOID_RETURN;
}

NdbCursorOperation::LockMode get_ndb_lock_type(enum thr_lock_type type)
{
  return (type == TL_WRITE_ALLOW_WRITE) ? 
    NdbCursorOperation::LM_Exclusive : NdbCursorOperation::LM_Read;
}

static const ulong index_type_flags[]=
{
  /* UNDEFINED_INDEX */
  0,                         

  /* PRIMARY_KEY_INDEX */
  HA_ONLY_WHOLE_INDEX, 

  /* PRIMARY_KEY_ORDERED_INDEX */
  /* 
     Enable HA_KEY_READ_ONLY when "sorted" indexes are supported, 
     thus ORDERD BY clauses can be optimized by reading directly 
     through the index.
  */
  // HA_KEY_READ_ONLY | 
  HA_READ_NEXT |              
  HA_READ_RANGE,

  /* UNIQUE_INDEX */
  HA_ONLY_WHOLE_INDEX,

  /* UNIQUE_ORDERED_INDEX */
  HA_READ_NEXT |              
  HA_READ_RANGE,

  /* ORDERED_INDEX */
  HA_READ_NEXT |              
  HA_READ_RANGE,
};

static const int index_flags_size= sizeof(index_type_flags)/sizeof(ulong);

inline const char* ha_ndbcluster::get_index_name(uint idx_no) const
{
  return table->keynames.type_names[idx_no];
}

inline const char* ha_ndbcluster::get_unique_index_name(uint idx_no) const
{
  return m_unique_index_name[idx_no];
}

inline NDB_INDEX_TYPE ha_ndbcluster::get_index_type(uint idx_no) const
{
  DBUG_ASSERT(idx_no < MAX_KEY);
  return m_indextype[idx_no];
}


/*
  Get the flags for an index

  RETURN
    flags depending on the type of the index.
*/

inline ulong ha_ndbcluster::index_flags(uint idx_no, uint part) const 
{ 
  DBUG_ENTER("index_flags");
  DBUG_PRINT("info", ("idx_no: %d", idx_no));
  DBUG_ASSERT(get_index_type_from_table(idx_no) < index_flags_size);
  DBUG_RETURN(index_type_flags[get_index_type_from_table(idx_no)]);
}


int ha_ndbcluster::set_primary_key(NdbOperation *op, const byte *key)
{
  KEY* key_info= table->key_info + table->primary_key;
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;
  DBUG_ENTER("set_primary_key");

  for (; key_part != end; key_part++) 
  {
    Field* field= key_part->field;
    if (set_ndb_key(op, field, 
		    key_part->fieldnr-1, key))
      ERR_RETURN(op->getNdbError());
    key += key_part->length;
  }
  DBUG_RETURN(0);
}


int ha_ndbcluster::set_primary_key_from_old_data(NdbOperation *op, const byte *old_data)
{
  KEY* key_info= table->key_info + table->primary_key;
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;
  DBUG_ENTER("set_primary_key_from_old_data");

  for (; key_part != end; key_part++) 
  {
    Field* field= key_part->field;
    if (set_ndb_key(op, field, 
		    key_part->fieldnr-1, old_data+key_part->offset))
      ERR_RETURN(op->getNdbError());
  }
  DBUG_RETURN(0);
}


int ha_ndbcluster::set_primary_key(NdbOperation *op)
{
  DBUG_ENTER("set_primary_key");
  KEY* key_info= table->key_info + table->primary_key;
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;

  for (; key_part != end; key_part++) 
  {
    Field* field= key_part->field;
    if (set_ndb_key(op, field, 
                    key_part->fieldnr-1, field->ptr))
      ERR_RETURN(op->getNdbError());
  }
  DBUG_RETURN(0);
}


/*
  Read one record from NDB using primary key
*/

int ha_ndbcluster::pk_read(const byte *key, uint key_len, byte *buf)
{
  uint no_fields= table->fields, i;
  NdbConnection *trans= m_active_trans;
  NdbOperation *op;
  THD *thd= current_thd;
  DBUG_ENTER("pk_read");
  DBUG_PRINT("enter", ("key_len: %u", key_len));
  DBUG_DUMP("key", (char*)key, key_len);

  if (!(op= trans->getNdbOperation(m_tabname)) || op->readTuple() != 0)
    goto err;

  if (table->primary_key == MAX_KEY) 
  {
    // This table has no primary key, use "hidden" primary key
    DBUG_PRINT("info", ("Using hidden key"));
    DBUG_DUMP("key", (char*)key, 8);    
    if (set_hidden_key(op, no_fields, key))
      goto err;
    // Read key at the same time, for future reference
    if (get_ndb_value(op, no_fields, NULL))
      goto err;
  } 
  else 
  {
    int res;
    if ((res= set_primary_key(op, key)))
      return res;
  }
  
  // Read non-key field(s)
  for (i= 0; i < no_fields; i++) 
  {
    Field *field= table->field[i];
    if (thd->query_id == field->query_id) 
    {
      if (get_ndb_value(op, i, field->ptr))
        goto err;
    }
    else
    {
      // Attribute was not to be read
      m_value[i]= NULL;
    }
  }
  
  if (trans->execute(NoCommit, IgnoreError) != 0) 
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
  }

  // The value have now been fetched from NDB  
  unpack_record(buf);
  table->status= 0;     
  DBUG_RETURN(0);

 err:
  ERR_RETURN(trans->getNdbError());
}


/*
  Read one record from NDB using unique secondary index
*/

int ha_ndbcluster::unique_index_read(const byte *key,
				     uint key_len, byte *buf)
{
  NdbConnection *trans= m_active_trans;
  NdbIndexOperation *op;
  THD *thd= current_thd;
  byte *key_ptr;
  KEY* key_info;
  KEY_PART_INFO *key_part, *end;
  uint i;
  DBUG_ENTER("unique_index_read");
  DBUG_PRINT("enter", ("key_len: %u, index: %u", key_len, active_index));
  DBUG_DUMP("key", (char*)key, key_len);
  DBUG_PRINT("enter", ("name: %s", get_unique_index_name(active_index)));
  
  if (!(op= trans->getNdbIndexOperation(get_unique_index_name(active_index), 
					m_tabname)) ||
      op->readTuple() != 0)
    ERR_RETURN(trans->getNdbError());
  
  // Set secondary index key(s)
  key_ptr= (byte *) key;
  key_info= table->key_info + active_index;
  DBUG_ASSERT(key_info->key_length == key_len);
  end= (key_part= key_info->key_part) + key_info->key_parts;

  for (i= 0; key_part != end; key_part++, i++) 
  {
    if (set_ndb_key(op, key_part->field, i, key_ptr))
      ERR_RETURN(trans->getNdbError());
    key_ptr+= key_part->length;
  }

  // Get non-index attribute(s)
  for (i= 0; i < table->fields; i++) 
  {
    Field *field= table->field[i];
    if ((thd->query_id == field->query_id) ||
        (field->flags & PRI_KEY_FLAG))
    {
      if (get_ndb_value(op, i, field->ptr))
        ERR_RETURN(op->getNdbError());
    }
    else
    {
      // Attribute was not to be read
      m_value[i]= NULL;
    }
  }

  if (trans->execute(NoCommit, IgnoreError) != 0) 
  {
    table->status= STATUS_NOT_FOUND;
    DBUG_RETURN(ndb_err(trans));
  }
  // The value have now been fetched from NDB
  unpack_record(buf);
  table->status= 0;
  DBUG_RETURN(0);
}

/*
  Get the next record of a started scan. Try to fetch
  it locally from NdbApi cached records if possible, 
  otherwise ask NDB for more.

  NOTE
  If this is a update/delete make sure to not contact 
  NDB before any pending ops have been sent to NDB.

*/

inline int ha_ndbcluster::next_result(byte *buf)
{  
  int check;
  NdbConnection *trans= m_active_trans;
  NdbResultSet *cursor= m_active_cursor; 
  DBUG_ENTER("next_result");

  if (!cursor)
    DBUG_RETURN(HA_ERR_END_OF_FILE);
    
  /* 
     If this an update or delete, call nextResult with false
     to process any records already cached in NdbApi
  */
  bool contact_ndb = m_lock.type != TL_WRITE_ALLOW_WRITE;
  do {
    DBUG_PRINT("info", ("Call nextResult, contact_ndb: %d", contact_ndb));
    check= cursor->nextResult(contact_ndb);
    if (check == 0)
    {
      // One more record found
      DBUG_PRINT("info", ("One more record found"));    
      unpack_record(buf);
      table->status= 0;
      DBUG_RETURN(0);
    } 
    else if (check == 1 || check == 2)
    {
      // 1: No more records
      // 2: No more cached records

      /*
	Before fetching more rows and releasing lock(s),
	all pending update or delete operations should 
	be sent to NDB
      */
      DBUG_PRINT("info", ("ops_pending: %d", ops_pending));    
      if (ops_pending && (trans->execute(NoCommit) != 0))	
	DBUG_RETURN(ndb_err(trans));
      ops_pending= 0;
      
      contact_ndb= (check == 2);
    }
  } while (check == 2);
    
  table->status= STATUS_NOT_FOUND;
  if (check == -1)
    DBUG_RETURN(ndb_err(trans));

  // No more records
  DBUG_PRINT("info", ("No more records"));
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


/*
  Set bounds for a ordered index scan, use key_range
*/

int ha_ndbcluster::set_bounds(NdbOperation *op,
			      const key_range *key,
			      int bound)
{
  uint i, tot_len;
  byte *key_ptr;
  KEY* key_info= table->key_info + active_index;
  KEY_PART_INFO* key_part= key_info->key_part;
  KEY_PART_INFO* end= key_part+key_info->key_parts;

  DBUG_ENTER("set_bounds");
  DBUG_PRINT("enter", ("bound: %d", bound));
  DBUG_PRINT("enter", ("key_parts: %d", key_info->key_parts));
  DBUG_PRINT("enter", ("key->length: %d", key->length));
  DBUG_PRINT("enter", ("key->flag: %d", key->flag));

  // Set bounds using key data
  tot_len= 0;
  key_ptr= (byte *) key->key;    
  for (; key_part != end; key_part++)
  {
    Field* field= key_part->field;
    uint32 field_len=  field->pack_length();
    tot_len+= field_len;

    const char* bounds[]= {"LE", "LT", "GE", "GT", "EQ"};
    DBUG_ASSERT(bound >= 0 && bound <= 4);    
    DBUG_PRINT("info", ("Set Bound%s on %s", 
			bounds[bound],
			field->field_name));
    DBUG_DUMP("key", (char*)key_ptr, field_len);

    if (op->setBound(field->field_name,
		     bound, 
		     key_ptr,
		     field_len) != 0)
      ERR_RETURN(op->getNdbError());
    
    key_ptr+= field_len;
    
    if (tot_len >= key->length)
      break;

    /*
      Only one bound which is not EQ can be set
      so if this bound was not EQ, bail out and make 
      a best effort attempt
    */
    if (bound != NdbOperation::BoundEQ)
      break;
  }

  DBUG_RETURN(0);
}


/*
  Start ordered index scan in NDB
*/

int ha_ndbcluster::ordered_index_scan(const key_range *start_key,
				      const key_range *end_key,
				      bool sorted, byte* buf)
{  
  NdbConnection *trans= m_active_trans;
  NdbResultSet *cursor;
  NdbScanOperation *op;
  const char *index_name;

  DBUG_ENTER("ordered_index_scan");
  DBUG_PRINT("enter", ("index: %u, sorted: %d", active_index, sorted));  
  DBUG_PRINT("enter", ("Starting new ordered scan on %s", m_tabname));
  
  index_name= get_index_name(active_index);
  if (!(op= trans->getNdbScanOperation(index_name, m_tabname)))
    ERR_RETURN(trans->getNdbError());
  if (!(cursor= op->readTuples(parallelism, get_ndb_lock_type(m_lock.type))))
    ERR_RETURN(trans->getNdbError());
  m_active_cursor= cursor;

  if (start_key && 
      set_bounds(op, start_key, 
		 (start_key->flag == HA_READ_KEY_EXACT) ? 
		 NdbOperation::BoundEQ :
		 (start_key->flag == HA_READ_AFTER_KEY) ? 
		 NdbOperation::BoundLT : 
		 NdbOperation::BoundLE))
    DBUG_RETURN(1);

  if (end_key)
  {
    if (start_key && start_key->flag == HA_READ_KEY_EXACT)
    {
      DBUG_PRINT("info", ("start_key is HA_READ_KEY_EXACT ignoring end_key"));
    }
    else if (set_bounds(op, end_key, 
			(end_key->flag == HA_READ_AFTER_KEY) ? 
			NdbOperation::BoundGE : 
			NdbOperation::BoundGT))
      DBUG_RETURN(1);    
  }
  DBUG_RETURN(define_read_attrs(buf, op));
} 


/*
  Start a filtered scan in NDB.

  NOTE
  This function is here as an example of how to start a
  filtered scan. It should be possible to replace full_table_scan 
  with this function and make a best effort attempt 
  at filtering out the irrelevant data by converting the "items" 
  into interpreted instructions.
  This would speed up table scans where there is a limiting WHERE clause
  that doesn't match any index in the table.

 */

int ha_ndbcluster::filtered_scan(const byte *key, uint key_len, 
				 byte *buf,
				 enum ha_rkey_function find_flag)
{  
  NdbConnection *trans= m_active_trans;
  NdbResultSet *cursor;
  NdbScanOperation *op;

  DBUG_ENTER("filtered_scan");
  DBUG_PRINT("enter", ("key_len: %u, index: %u", 
                       key_len, active_index));
  DBUG_DUMP("key", (char*)key, key_len);  
  DBUG_PRINT("info", ("Starting a new filtered scan on %s",
		      m_tabname));

  if (!(op= trans->getNdbScanOperation(m_tabname)))
    ERR_RETURN(trans->getNdbError());
  if (!(cursor= op->readTuples(parallelism, get_ndb_lock_type(m_lock.type))))
    ERR_RETURN(trans->getNdbError());
  m_active_cursor= cursor;

  {
    // Start scan filter
    NdbScanFilter sf(op);
    sf.begin();
      
    // Set filter using the supplied key data
    byte *key_ptr= (byte *) key;    
    uint tot_len= 0;
    KEY* key_info= table->key_info + active_index;
    for (uint k= 0; k < key_info->key_parts; k++) 
    {
      KEY_PART_INFO* key_part= key_info->key_part+k;
      Field* field= key_part->field;
      uint ndb_fieldnr= key_part->fieldnr-1;
      DBUG_PRINT("key_part", ("fieldnr: %d", ndb_fieldnr));
      //      const NDBCOL *col= tab->getColumn(ndb_fieldnr);
      uint32 field_len=  field->pack_length();
      DBUG_DUMP("key", (char*)key, field_len);
	
      DBUG_PRINT("info", ("Column %s, type: %d, len: %d", 
			  field->field_name, field->real_type(), field_len));
	
      // Define scan filter
      if (field->real_type() == MYSQL_TYPE_STRING)
	sf.eq(ndb_fieldnr, key_ptr, field_len);
      else 
      {
	if (field_len == 8)
	  sf.eq(ndb_fieldnr, (Uint64)*key_ptr);
	else if (field_len <= 4)
	  sf.eq(ndb_fieldnr, (Uint32)*key_ptr);
	else 
	  DBUG_RETURN(1);
      }
	
      key_ptr += field_len;
      tot_len += field_len;
	
      if (tot_len >= key_len)
	break;
    }
    // End scan filter
    sf.end();
  }

  DBUG_RETURN(define_read_attrs(buf, op));
} 


/*
  Start full table scan in NDB
 */

int ha_ndbcluster::full_table_scan(byte *buf)
{
  uint i;
  NdbResultSet *cursor;
  NdbScanOperation *op;
  NdbConnection *trans= m_active_trans;

  DBUG_ENTER("full_table_scan");  
  DBUG_PRINT("enter", ("Starting new scan on %s", m_tabname));

  if (!(op=trans->getNdbScanOperation(m_tabname)))
    ERR_RETURN(trans->getNdbError());  
  if (!(cursor= op->readTuples(parallelism, get_ndb_lock_type(m_lock.type))))
    ERR_RETURN(trans->getNdbError());
  m_active_cursor= cursor;
  DBUG_RETURN(define_read_attrs(buf, op));
}


inline 
int ha_ndbcluster::define_read_attrs(byte* buf, NdbOperation* op)
{
  uint i;
  THD *thd= current_thd;
  NdbConnection *trans= m_active_trans;

  DBUG_ENTER("define_read_attrs");  

  // Define attributes to read
  for (i= 0; i < table->fields; i++) 
  {
    Field *field= table->field[i];
    if ((thd->query_id == field->query_id) ||
	(field->flags & PRI_KEY_FLAG) || 
	retrieve_all_fields)
    {      
      if (get_ndb_value(op, i, field->ptr))
	ERR_RETURN(op->getNdbError());
    } 
    else 
    {
      m_value[i]= NULL;
    }
  }
    
  if (table->primary_key == MAX_KEY) 
  {
    DBUG_PRINT("info", ("Getting hidden key"));
    // Scanning table with no primary key
    int hidden_no= table->fields;      
#ifndef DBUG_OFF
    const NDBTAB *tab= (NDBTAB *) m_table;    
    if (!tab->getColumn(hidden_no))
      DBUG_RETURN(1);
#endif
    if (get_ndb_value(op, hidden_no, NULL))
      ERR_RETURN(op->getNdbError());
  }

  if (trans->execute(NoCommit) != 0)
    DBUG_RETURN(ndb_err(trans));
  DBUG_PRINT("exit", ("Scan started successfully"));
  DBUG_RETURN(next_result(buf));
} 


/*
  Insert one record into NDB
*/

int ha_ndbcluster::write_row(byte *record)
{
  uint i;
  NdbConnection *trans= m_active_trans;
  NdbOperation *op;
  int res;
  DBUG_ENTER("write_row");
  
  statistic_increment(ha_write_count,&LOCK_status);
  if (table->timestamp_default_now)
    update_timestamp(record+table->timestamp_default_now-1);
  if (table->next_number_field && record == table->record[0])
    update_auto_increment();

  if (!(op= trans->getNdbOperation(m_tabname)))
    ERR_RETURN(trans->getNdbError());

  res= (m_use_write) ? op->writeTuple() :op->insertTuple(); 
  if (res != 0)
    ERR_RETURN(trans->getNdbError());  
 
  if (table->primary_key == MAX_KEY) 
  {
    // Table has hidden primary key
    Uint64 auto_value= m_ndb->getAutoIncrementValue(m_tabname);
    if (set_hidden_key(op, table->fields, (const byte*)&auto_value))
      ERR_RETURN(op->getNdbError());
  } 
  else 
  {
    int res;
    if ((res= set_primary_key(op)))
      return res;
  }

  // Set non-key attribute(s)
  for (i= 0; i < table->fields; i++) 
  {
    Field *field= table->field[i];
    if (!(field->flags & PRI_KEY_FLAG) &&
	set_ndb_value(op, field, i))
      ERR_RETURN(op->getNdbError());
  }

  /*
    Execute write operation
    NOTE When doing inserts with many values in 
    each INSERT statement it should not be necessary
    to NoCommit the transaction between each row.
    Find out how this is detected!
  */
  rows_inserted++;
  if ((rows_inserted == rows_to_insert) || 
      ((rows_inserted % bulk_insert_rows) == 0))
  {
    // Send rows to NDB
    DBUG_PRINT("info", ("Sending inserts to NDB, "\
			"rows_inserted:%d, bulk_insert_rows: %d", 
			rows_inserted, bulk_insert_rows)); 
    if (trans->execute(NoCommit) != 0)
      DBUG_RETURN(ndb_err(trans));
  }
  DBUG_RETURN(0);
}


/* Compare if a key in a row has changed */

int ha_ndbcluster::key_cmp(uint keynr, const byte * old_row,
			   const byte * new_row)
{
  KEY_PART_INFO *key_part=table->key_info[keynr].key_part;
  KEY_PART_INFO *end=key_part+table->key_info[keynr].key_parts;

  for (; key_part != end ; key_part++)
  {
    if (key_part->null_bit)
    {
      if ((old_row[key_part->null_offset] & key_part->null_bit) !=
	  (new_row[key_part->null_offset] & key_part->null_bit))
	return 1;
    }
    if (key_part->key_part_flag & (HA_BLOB_PART | HA_VAR_LENGTH))
    {

      if (key_part->field->cmp_binary((char*) (old_row + key_part->offset),
				      (char*) (new_row + key_part->offset),
				      (ulong) key_part->length))
	return 1;
    }
    else
    {
      if (memcmp(old_row+key_part->offset, new_row+key_part->offset,
		 key_part->length))
	return 1;
    }
  }
  return 0;
}

/*
  Update one record in NDB using primary key
*/

int ha_ndbcluster::update_row(const byte *old_data, byte *new_data)
{
  THD *thd= current_thd;
  NdbConnection *trans= m_active_trans;
  NdbResultSet* cursor= m_active_cursor;
  NdbOperation *op;
  uint i;
  DBUG_ENTER("update_row");
  
  statistic_increment(ha_update_count,&LOCK_status);
  if (table->timestamp_on_update_now)
    update_timestamp(new_data+table->timestamp_on_update_now-1);
  
  /* Check for update of primary key and return error */  
  if ((table->primary_key != MAX_KEY) &&
      (key_cmp(table->primary_key, old_data, new_data)))
    DBUG_RETURN(HA_ERR_UNSUPPORTED);
  
  if (cursor)
  {
    /*
      We are scanning records and want to update the record
      that was just found, call updateTuple on the cursor 
      to take over the lock to a new update operation
      And thus setting the primary key of the record from 
      the active record in cursor
    */
    DBUG_PRINT("info", ("Calling updateTuple on cursor"));
    if (!(op= cursor->updateTuple()))
      ERR_RETURN(trans->getNdbError());
    ops_pending++;
  }
  else
  {  
    if (!(op= trans->getNdbOperation(m_tabname)) ||
	op->updateTuple() != 0)
      ERR_RETURN(trans->getNdbError());  
    
    if (table->primary_key == MAX_KEY) 
    {
      // This table has no primary key, use "hidden" primary key
      DBUG_PRINT("info", ("Using hidden key"));
      
      // Require that the PK for this record has previously been 
      // read into m_value
      uint no_fields= table->fields;
      NdbRecAttr* rec= m_value[no_fields];
      DBUG_ASSERT(rec);
      DBUG_DUMP("key", (char*)rec->aRef(), NDB_HIDDEN_PRIMARY_KEY_LENGTH);
      
      if (set_hidden_key(op, no_fields, rec->aRef()))
	ERR_RETURN(op->getNdbError());
    } 
    else 
    {
      int res;
      if ((res= set_primary_key_from_old_data(op, old_data)))
	DBUG_RETURN(res);
    }
  }

  // Set non-key attribute(s)
  for (i= 0; i < table->fields; i++) 
  {
    Field *field= table->field[i];
    if ((thd->query_id == field->query_id) &&
        (!(field->flags & PRI_KEY_FLAG)) &&
	set_ndb_value(op, field, i))
      ERR_RETURN(op->getNdbError());
  }

  // Execute update operation
  if (!cursor && trans->execute(NoCommit) != 0)
    DBUG_RETURN(ndb_err(trans));
  
  DBUG_RETURN(0);
}


/*
  Delete one record from NDB, using primary key 
*/

int ha_ndbcluster::delete_row(const byte *record)
{
  NdbConnection *trans= m_active_trans;
  NdbResultSet* cursor= m_active_cursor;
  NdbOperation *op;
  DBUG_ENTER("delete_row");

  statistic_increment(ha_delete_count,&LOCK_status);

  if (cursor)
  {
    /*
      We are scanning records and want to update the record
      that was just found, call deleteTuple on the cursor 
      to take over the lock to a new update operation
      And thus setting the primary key of the record from 
      the active record in cursor
    */
    DBUG_PRINT("info", ("Calling deleteTuple on cursor"));
    if (cursor->deleteTuple() != 0)
      ERR_RETURN(trans->getNdbError());     
    ops_pending++;

    // If deleting from cursor, NoCommit will be handled in next_result
    DBUG_RETURN(0);
  }
  else
  {
    
    if (!(op=trans->getNdbOperation(m_tabname)) || 
	op->deleteTuple() != 0)
      ERR_RETURN(trans->getNdbError());
    
    if (table->primary_key == MAX_KEY) 
    {
      // This table has no primary key, use "hidden" primary key
      DBUG_PRINT("info", ("Using hidden key"));
      uint no_fields= table->fields;
      NdbRecAttr* rec= m_value[no_fields];
      DBUG_ASSERT(rec != NULL);
      
      if (set_hidden_key(op, no_fields, rec->aRef()))
	ERR_RETURN(op->getNdbError());
    } 
    else 
    {
      int res;
      if ((res= set_primary_key(op)))
	return res;  
    }
  }
  
  // Execute delete operation
  if (trans->execute(NoCommit) != 0)
    DBUG_RETURN(ndb_err(trans));
  DBUG_RETURN(0);
}
  
/*
  Unpack a record read from NDB 

  SYNOPSIS
    unpack_record()
    buf			Buffer to store read row

  NOTE
    The data for each row is read directly into the
    destination buffer. This function is primarily 
    called in order to check if any fields should be 
    set to null.
*/

void ha_ndbcluster::unpack_record(byte* buf)
{
  uint row_offset= (uint) (buf - table->record[0]);
  Field **field, **end;
  NdbRecAttr **value= m_value;
  DBUG_ENTER("unpack_record");
  
  // Set null flag(s)
  bzero(buf, table->null_bytes);
  for (field= table->field, end= field+table->fields;
       field < end;
       field++, value++)
  {
    if (*value && (*value)->isNULL())
     (*field)->set_null(row_offset);
  }

#ifndef DBUG_OFF
  // Read and print all values that was fetched
  if (table->primary_key == MAX_KEY)
  {
    // Table with hidden primary key
    int hidden_no= table->fields;
    const NDBTAB *tab= (NDBTAB *) m_table;
    const NDBCOL *hidden_col= tab->getColumn(hidden_no);
    NdbRecAttr* rec= m_value[hidden_no];
    DBUG_ASSERT(rec);
    DBUG_PRINT("hidden", ("%d: %s \"%llu\"", hidden_no, 
                          hidden_col->getName(), rec->u_64_value()));
  } 
  print_results();
#endif
  DBUG_VOID_RETURN;
}

 
/*
  Utility function to print/dump the fetched field
 */

void ha_ndbcluster::print_results()
{
  const NDBTAB *tab= (NDBTAB*) m_table;
  DBUG_ENTER("print_results");

#ifndef DBUG_OFF
  if (!_db_on_)
    DBUG_VOID_RETURN;
  
  for (uint f=0; f<table->fields;f++)
  {
    Field *field;
    const NDBCOL *col;
    NdbRecAttr *value;

    if (!(value= m_value[f]))
    {
      fprintf(DBUG_FILE, "Field %d was not read\n", f);
      continue;
    }
    field= table->field[f];
    DBUG_DUMP("field->ptr", (char*)field->ptr, field->pack_length());
    col= tab->getColumn(f);
    fprintf(DBUG_FILE, "%d: %s\t", f, col->getName());
    
    if (value->isNULL())
    {
      fprintf(DBUG_FILE, "NULL\n");
      continue;
    }

    switch (col->getType()) {
    case NdbDictionary::Column::Blob:
    case NdbDictionary::Column::Clob:
    case NdbDictionary::Column::Undefined:
      fprintf(DBUG_FILE, "Unknown type: %d", col->getType());
      break;
    case NdbDictionary::Column::Tinyint: {
      char value= *field->ptr;
      fprintf(DBUG_FILE, "Tinyint\t%d", value);
      break;
    }
    case NdbDictionary::Column::Tinyunsigned: {
      unsigned char value= *field->ptr;
      fprintf(DBUG_FILE, "Tinyunsigned\t%u", value);
      break;
    }
    case NdbDictionary::Column::Smallint: {
      short value= *field->ptr;
      fprintf(DBUG_FILE, "Smallint\t%d", value);
      break;
    }
    case NdbDictionary::Column::Smallunsigned: {
      unsigned short value= *field->ptr;
      fprintf(DBUG_FILE, "Smallunsigned\t%u", value);
      break;
    }
    case NdbDictionary::Column::Mediumint: {
      byte value[3];
      memcpy(value, field->ptr, 3);
      fprintf(DBUG_FILE, "Mediumint\t%d,%d,%d", value[0], value[1], value[2]);
      break;
    }
    case NdbDictionary::Column::Mediumunsigned: {
      byte value[3];
      memcpy(value, field->ptr, 3);
      fprintf(DBUG_FILE, "Mediumunsigned\t%u,%u,%u", value[0], value[1], value[2]);
      break;
    }
    case NdbDictionary::Column::Int: {
      fprintf(DBUG_FILE, "Int\t%lld", field->val_int());
      break;
    }
    case NdbDictionary::Column::Unsigned: {
      Uint32 value= (Uint32) *field->ptr;
      fprintf(DBUG_FILE, "Unsigned\t%u", value);
      break;
    }
    case NdbDictionary::Column::Bigint: {
      Int64 value= (Int64) *field->ptr;
      fprintf(DBUG_FILE, "Bigint\t%lld", value);
      break;
    }
    case NdbDictionary::Column::Bigunsigned: {
      Uint64 value= (Uint64) *field->ptr;
      fprintf(DBUG_FILE, "Bigunsigned\t%llu", value);
      break;
    }
    case NdbDictionary::Column::Float: {
      float value= (float) *field->ptr;
      fprintf(DBUG_FILE, "Float\t%f", value);
      break;
    }
    case NdbDictionary::Column::Double: {
      double value= (double) *field->ptr;
      fprintf(DBUG_FILE, "Double\t%f", value);
      break;
    }
    case NdbDictionary::Column::Decimal: {
      char *value= field->ptr;

      fprintf(DBUG_FILE, "Decimal\t'%-*s'", field->pack_length(), value);
      break;
    }
    case NdbDictionary::Column::Char:{
      char buf[field->pack_length()+1];
      char *value= (char *) field->ptr;
      snprintf(buf, field->pack_length(), "%s", value);
      fprintf(DBUG_FILE, "Char\t'%s'", buf);
      break;
    }
    case NdbDictionary::Column::Varchar:
    case NdbDictionary::Column::Binary:
    case NdbDictionary::Column::Varbinary: {
      char *value= (char *) field->ptr;
      fprintf(DBUG_FILE, "'%s'", value);
      break;
    }
    case NdbDictionary::Column::Datetime: {
      Uint64 value= (Uint64) *field->ptr;
      fprintf(DBUG_FILE, "Datetime\t%llu", value);
      break;
    }
    case NdbDictionary::Column::Timespec: {
      Uint64 value= (Uint64) *field->ptr;
      fprintf(DBUG_FILE, "Timespec\t%llu", value);
      break;
    }
    }
    fprintf(DBUG_FILE, "\n");
    
  }
#endif
  DBUG_VOID_RETURN;
}


int ha_ndbcluster::index_init(uint index)
{
  DBUG_ENTER("index_init");
  DBUG_PRINT("enter", ("index: %u", index));
  DBUG_RETURN(handler::index_init(index));
}


int ha_ndbcluster::index_end()
{
  DBUG_ENTER("index_end");
  DBUG_RETURN(close_scan());
}


int ha_ndbcluster::index_read(byte *buf,
			  const byte *key, uint key_len, 
			  enum ha_rkey_function find_flag)
{
  DBUG_ENTER("index_read");
  DBUG_PRINT("enter", ("active_index: %u, key_len: %u, find_flag: %d", 
                       active_index, key_len, find_flag));

  key_range start_key;
  start_key.key=    key;
  start_key.length= key_len;
  start_key.flag=   find_flag;
  DBUG_RETURN(read_range_first(&start_key, NULL, false, true));
}


int ha_ndbcluster::index_read_idx(byte *buf, uint index_no, 
			      const byte *key, uint key_len, 
			      enum ha_rkey_function find_flag)
{
  statistic_increment(ha_read_key_count,&LOCK_status);
  DBUG_ENTER("index_read_idx");
  DBUG_PRINT("enter", ("index_no: %u, key_len: %u", index_no, key_len));  
  index_init(index_no);  
  DBUG_RETURN(index_read(buf, key, key_len, find_flag));
}


int ha_ndbcluster::index_next(byte *buf)
{
  DBUG_ENTER("index_next");

  int error = 1;
  statistic_increment(ha_read_next_count,&LOCK_status);
  DBUG_RETURN(next_result(buf));
}


int ha_ndbcluster::index_prev(byte *buf)
{
  DBUG_ENTER("index_prev");
  statistic_increment(ha_read_prev_count,&LOCK_status);
  DBUG_RETURN(1);
}


int ha_ndbcluster::index_first(byte *buf)
{
  DBUG_ENTER("index_first");
  statistic_increment(ha_read_first_count,&LOCK_status);
  DBUG_RETURN(1);
}


int ha_ndbcluster::index_last(byte *buf)
{
  DBUG_ENTER("index_last");
  statistic_increment(ha_read_last_count,&LOCK_status);
  DBUG_RETURN(1);
}


int ha_ndbcluster::read_range_first(const key_range *start_key,
				    const key_range *end_key,
				    bool eq_range, bool sorted)
{
  KEY* key_info;
  int error= 1; 
  byte* buf= table->record[0];
  DBUG_ENTER("ha_ndbcluster::read_range_first");
  DBUG_PRINT("info", ("eq_range: %d, sorted: %d", eq_range, sorted));

  if (m_active_cursor)
    close_scan();

  switch (get_index_type(active_index)){
  case PRIMARY_KEY_ORDERED_INDEX:
  case PRIMARY_KEY_INDEX:
    key_info= table->key_info + active_index;
    if (start_key && 
	start_key->length == key_info->key_length &&
	start_key->flag == HA_READ_KEY_EXACT)
    {
      error= pk_read(start_key->key, start_key->length, buf);      
      DBUG_RETURN(error == HA_ERR_KEY_NOT_FOUND ? HA_ERR_END_OF_FILE : error);
    }
    break;
  case UNIQUE_ORDERED_INDEX:
  case UNIQUE_INDEX:
    key_info= table->key_info + active_index;
    if (start_key && 
	start_key->length == key_info->key_length &&
	start_key->flag == HA_READ_KEY_EXACT)
    {
      error= unique_index_read(start_key->key, start_key->length, buf);
      DBUG_RETURN(error == HA_ERR_KEY_NOT_FOUND ? HA_ERR_END_OF_FILE : error);
    }
    break;
  default:
    break;
  }


  // Start the ordered index scan and fetch the first row
  error= ordered_index_scan(start_key, end_key, sorted,
			    buf);

  DBUG_RETURN(error);
}


int ha_ndbcluster::read_range_next()
{
  DBUG_ENTER("ha_ndbcluster::read_range_next");
  DBUG_RETURN(next_result(table->record[0]));
}


int ha_ndbcluster::rnd_init(bool scan)
{
  NdbResultSet *cursor= m_active_cursor;
  DBUG_ENTER("rnd_init");
  DBUG_PRINT("enter", ("scan: %d", scan));
  // Check that cursor is not defined
  if (cursor)
    DBUG_RETURN(1);
  index_init(table->primary_key);
  DBUG_RETURN(0);
}

int ha_ndbcluster::close_scan()
{
  NdbResultSet *cursor= m_active_cursor;
  DBUG_ENTER("close_scan");

  if (!cursor)
    DBUG_RETURN(1);

  cursor->close();
  m_active_cursor= NULL;
  DBUG_RETURN(0);
}

int ha_ndbcluster::rnd_end()
{
  DBUG_ENTER("rnd_end");
  DBUG_RETURN(close_scan());
}


int ha_ndbcluster::rnd_next(byte *buf)
{
  DBUG_ENTER("rnd_next");
  statistic_increment(ha_read_rnd_next_count, &LOCK_status);

  if (!m_active_cursor)
    DBUG_RETURN(full_table_scan(buf));
  DBUG_RETURN(next_result(buf));
}


/*
  An "interesting" record has been found and it's pk 
  retrieved by calling position
  Now it's time to read the record from db once 
  again
*/

int ha_ndbcluster::rnd_pos(byte *buf, byte *pos)
{
  DBUG_ENTER("rnd_pos");
  statistic_increment(ha_read_rnd_count,&LOCK_status);
  // The primary key for the record is stored in pos
  // Perform a pk_read using primary key "index"
  DBUG_RETURN(pk_read(pos, ref_length, buf));  
}


/*
  Store the primary key of this record in ref 
  variable, so that the row can be retrieved again later
  using "reference" in rnd_pos
*/

void ha_ndbcluster::position(const byte *record)
{
  KEY *key_info;
  KEY_PART_INFO *key_part;
  KEY_PART_INFO *end;
  byte *buff;
  DBUG_ENTER("position");

  if (table->primary_key != MAX_KEY) 
  {
    key_info= table->key_info + table->primary_key;
    key_part= key_info->key_part;
    end= key_part + key_info->key_parts;
    buff= ref;
    
    for (; key_part != end; key_part++) 
    {
      if (key_part->null_bit) {
        /* Store 0 if the key part is a NULL part */      
        if (record[key_part->null_offset]
            & key_part->null_bit) {
          *buff++= 1;
          continue;
        }      
        *buff++= 0;
      }
      memcpy(buff, record + key_part->offset, key_part->length);
      buff += key_part->length;
    }
  } 
  else 
  {
    // No primary key, get hidden key
    DBUG_PRINT("info", ("Getting hidden key"));
    int hidden_no= table->fields;
    NdbRecAttr* rec= m_value[hidden_no];
    const NDBTAB *tab= (NDBTAB *) m_table;  
    const NDBCOL *hidden_col= tab->getColumn(hidden_no);
    DBUG_ASSERT(hidden_col->getPrimaryKey() && 
                hidden_col->getAutoIncrement() &&
                rec != NULL && 
                ref_length == NDB_HIDDEN_PRIMARY_KEY_LENGTH);
    memcpy(ref, (const void*)rec->aRef(), ref_length);
  }
  
  DBUG_DUMP("ref", (char*)ref, ref_length);
  DBUG_VOID_RETURN;
}


void ha_ndbcluster::info(uint flag)
{
  DBUG_ENTER("info");
  DBUG_PRINT("enter", ("flag: %d", flag));
  
  if (flag & HA_STATUS_POS)
    DBUG_PRINT("info", ("HA_STATUS_POS"));
  if (flag & HA_STATUS_NO_LOCK)
    DBUG_PRINT("info", ("HA_STATUS_NO_LOCK"));
  if (flag & HA_STATUS_TIME)
    DBUG_PRINT("info", ("HA_STATUS_TIME"));
  if (flag & HA_STATUS_CONST)
    DBUG_PRINT("info", ("HA_STATUS_CONST"));
  if (flag & HA_STATUS_VARIABLE)
    DBUG_PRINT("info", ("HA_STATUS_VARIABLE"));
  if (flag & HA_STATUS_ERRKEY)
    DBUG_PRINT("info", ("HA_STATUS_ERRKEY"));
  if (flag & HA_STATUS_AUTO)
    DBUG_PRINT("info", ("HA_STATUS_AUTO"));
  DBUG_VOID_RETURN;
}


int ha_ndbcluster::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("extra");
  switch (operation) {
  case HA_EXTRA_NORMAL:              /* Optimize for space (def) */
    DBUG_PRINT("info", ("HA_EXTRA_NORMAL"));
    break;
  case HA_EXTRA_QUICK:                 /* Optimize for speed */
    DBUG_PRINT("info", ("HA_EXTRA_QUICK"));
    break;
  case HA_EXTRA_RESET:                 /* Reset database to after open */
    DBUG_PRINT("info", ("HA_EXTRA_RESET"));
    break;
  case HA_EXTRA_CACHE:                 /* Cash record in HA_rrnd() */
    DBUG_PRINT("info", ("HA_EXTRA_CACHE"));
    break;
  case HA_EXTRA_NO_CACHE:              /* End cacheing of records (def) */
    DBUG_PRINT("info", ("HA_EXTRA_NO_CACHE"));
    break;
  case HA_EXTRA_NO_READCHECK:          /* No readcheck on update */
    DBUG_PRINT("info", ("HA_EXTRA_NO_READCHECK"));
    break;
  case HA_EXTRA_READCHECK:             /* Use readcheck (def) */
    DBUG_PRINT("info", ("HA_EXTRA_READCHECK"));
    break;
  case HA_EXTRA_KEYREAD:               /* Read only key to database */
    DBUG_PRINT("info", ("HA_EXTRA_KEYREAD"));
    break;
  case HA_EXTRA_NO_KEYREAD:            /* Normal read of records (def) */
    DBUG_PRINT("info", ("HA_EXTRA_NO_KEYREAD"));
    break;
  case HA_EXTRA_NO_USER_CHANGE:        /* No user is allowed to write */
    DBUG_PRINT("info", ("HA_EXTRA_NO_USER_CHANGE"));
    break;
  case HA_EXTRA_KEY_CACHE:
    DBUG_PRINT("info", ("HA_EXTRA_KEY_CACHE"));
    break;
  case HA_EXTRA_NO_KEY_CACHE:
    DBUG_PRINT("info", ("HA_EXTRA_NO_KEY_CACHE"));
    break;
  case HA_EXTRA_WAIT_LOCK:            /* Wait until file is avalably (def) */
    DBUG_PRINT("info", ("HA_EXTRA_WAIT_LOCK"));
    break;
  case HA_EXTRA_NO_WAIT_LOCK:         /* If file is locked, return quickly */
    DBUG_PRINT("info", ("HA_EXTRA_NO_WAIT_LOCK"));
    break;
  case HA_EXTRA_WRITE_CACHE:           /* Use write cache in ha_write() */
    DBUG_PRINT("info", ("HA_EXTRA_WRITE_CACHE"));
    break;
  case HA_EXTRA_FLUSH_CACHE:           /* flush write_record_cache */
    DBUG_PRINT("info", ("HA_EXTRA_FLUSH_CACHE"));
    break;
  case HA_EXTRA_NO_KEYS:               /* Remove all update of keys */
    DBUG_PRINT("info", ("HA_EXTRA_NO_KEYS"));
    break;
  case HA_EXTRA_KEYREAD_CHANGE_POS:         /* Keyread, but change pos */
    DBUG_PRINT("info", ("HA_EXTRA_KEYREAD_CHANGE_POS")); /* xxxxchk -r must be used */
    break;                                  
  case HA_EXTRA_REMEMBER_POS:          /* Remember pos for next/prev */
    DBUG_PRINT("info", ("HA_EXTRA_REMEMBER_POS"));
    break;
  case HA_EXTRA_RESTORE_POS:
    DBUG_PRINT("info", ("HA_EXTRA_RESTORE_POS"));
    break;
  case HA_EXTRA_REINIT_CACHE:          /* init cache from current record */
    DBUG_PRINT("info", ("HA_EXTRA_REINIT_CACHE"));
    break;
  case HA_EXTRA_FORCE_REOPEN:          /* Datafile have changed on disk */
    DBUG_PRINT("info", ("HA_EXTRA_FORCE_REOPEN"));
    break;
  case HA_EXTRA_FLUSH:                 /* Flush tables to disk */
    DBUG_PRINT("info", ("HA_EXTRA_FLUSH"));
    break;
  case HA_EXTRA_NO_ROWS:               /* Don't write rows */
    DBUG_PRINT("info", ("HA_EXTRA_NO_ROWS"));
    break;
  case HA_EXTRA_RESET_STATE:           /* Reset positions */
    DBUG_PRINT("info", ("HA_EXTRA_RESET_STATE"));
    break;
  case HA_EXTRA_IGNORE_DUP_KEY:       /* Dup keys don't rollback everything*/
    DBUG_PRINT("info", ("HA_EXTRA_IGNORE_DUP_KEY"));

    DBUG_PRINT("info", ("Turning ON use of write instead of insert"));
    m_use_write= TRUE;
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    DBUG_PRINT("info", ("HA_EXTRA_NO_IGNORE_DUP_KEY"));
    DBUG_PRINT("info", ("Turning OFF use of write instead of insert"));
    m_use_write= false;
    break;
  case HA_EXTRA_RETRIEVE_ALL_COLS:    /* Retrieve all columns, not just those
					 where field->query_id is the same as
					 the current query id */
    DBUG_PRINT("info", ("HA_EXTRA_RETRIEVE_ALL_COLS"));
    retrieve_all_fields= TRUE;
    break;
  case HA_EXTRA_PREPARE_FOR_DELETE:
    DBUG_PRINT("info", ("HA_EXTRA_PREPARE_FOR_DELETE"));
    break;
  case HA_EXTRA_PREPARE_FOR_UPDATE:     /* Remove read cache if problems */
    DBUG_PRINT("info", ("HA_EXTRA_PREPARE_FOR_UPDATE"));
    break;
  case HA_EXTRA_PRELOAD_BUFFER_SIZE: 
    DBUG_PRINT("info", ("HA_EXTRA_PRELOAD_BUFFER_SIZE"));
    break;
  case HA_EXTRA_RETRIEVE_PRIMARY_KEY: 
    DBUG_PRINT("info", ("HA_EXTRA_RETRIEVE_PRIMARY_KEY"));
    break;
  case HA_EXTRA_CHANGE_KEY_TO_UNIQUE: 
    DBUG_PRINT("info", ("HA_EXTRA_CHANGE_KEY_TO_UNIQUE"));
    break;
  case HA_EXTRA_CHANGE_KEY_TO_DUP: 
    DBUG_PRINT("info", ("HA_EXTRA_CHANGE_KEY_TO_DUP"));
    break;

  }
  
  DBUG_RETURN(0);
}

/* 
   Start of an insert, remember number of rows to be inserted, it will
   be used in write_row and get_autoincrement to send an optimal number
   of rows in each roundtrip to the server

   SYNOPSIS
   rows     number of rows to insert, 0 if unknown

*/

void ha_ndbcluster::start_bulk_insert(ha_rows rows)
{
  int bytes, batch;
  const NDBTAB *tab= (NDBTAB *) m_table;    

  DBUG_ENTER("start_bulk_insert");
  DBUG_PRINT("enter", ("rows: %d", rows));
  
  rows_inserted= 0;
  rows_to_insert= rows; 

  /* 
    Calculate how many rows that should be inserted
    per roundtrip to NDB. This is done in order to minimize the 
    number of roundtrips as much as possible. However performance will 
    degrade if too many bytes are inserted, thus it's limited by this 
    calculation.   
  */
  const int bytesperbatch = 8192;
  bytes= 12 + tab->getRowSizeInBytes() + 4 * tab->getNoOfColumns();
  batch= bytesperbatch/bytes;
  batch= batch == 0 ? 1 : batch;
  DBUG_PRINT("info", ("batch: %d, bytes: %d", batch, bytes));
  bulk_insert_rows= batch;

  DBUG_VOID_RETURN;
}

/*
  End of an insert
 */
int ha_ndbcluster::end_bulk_insert()
{
  DBUG_ENTER("end_bulk_insert");
  DBUG_RETURN(0);
}


int ha_ndbcluster::extra_opt(enum ha_extra_function operation, ulong cache_size)
{
  DBUG_ENTER("extra_opt");
  DBUG_PRINT("enter", ("cache_size: %d", cache_size));
  DBUG_RETURN(extra(operation));
}


int ha_ndbcluster::reset()
{
  DBUG_ENTER("reset");
  // Reset what?
  DBUG_RETURN(1);
}


const char **ha_ndbcluster::bas_ext() const
{ static const char *ext[1] = { NullS }; return ext; }


/*
  How many seeks it will take to read through the table
  This is to be comparable to the number returned by records_in_range so
  that we can decide if we should scan the table or use keys.
*/

double ha_ndbcluster::scan_time()
{
  return rows2double(records*1000);
}


THR_LOCK_DATA **ha_ndbcluster::store_lock(THD *thd,
                                          THR_LOCK_DATA **to,
                                          enum thr_lock_type lock_type)
{
  DBUG_ENTER("store_lock");
  
  if (lock_type != TL_IGNORE && m_lock.type == TL_UNLOCK) 
  {
    
    /* If we are not doing a LOCK TABLE, then allow multiple
       writers */
    
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !thd->in_lock_tables)      
      lock_type= TL_WRITE_ALLOW_WRITE;
    
    /* In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
       MySQL would use the lock TL_READ_NO_INSERT on t2, and that
       would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
       to t2. Convert the lock to a normal read lock to allow
       concurrent inserts to t2. */
    
    if (lock_type == TL_READ_NO_INSERT && !thd->in_lock_tables)
      lock_type= TL_READ;
    
    m_lock.type=lock_type;
  }
  *to++= &m_lock;

  DBUG_PRINT("exit", ("lock_type: %d", lock_type));
  
  DBUG_RETURN(to);
}

#ifndef DBUG_OFF
#define PRINT_OPTION_FLAGS(t) { \
      if (t->options & OPTION_NOT_AUTOCOMMIT) \
        DBUG_PRINT("thd->options", ("OPTION_NOT_AUTOCOMMIT")); \
      if (t->options & OPTION_BEGIN) \
        DBUG_PRINT("thd->options", ("OPTION_BEGIN")); \
      if (t->options & OPTION_TABLE_LOCK) \
        DBUG_PRINT("thd->options", ("OPTION_TABLE_LOCK")); \
}
#else
#define PRINT_OPTION_FLAGS(t)
#endif


/*
  As MySQL will execute an external lock for every new table it uses
  we can use this to start the transactions.
  If we are in auto_commit mode we just need to start a transaction
  for the statement, this will be stored in transaction.stmt.
  If not, we have to start a master transaction if there doesn't exist
  one from before, this will be stored in transaction.all
 
  When a table lock is held one transaction will be started which holds
  the table lock and for each statement a hupp transaction will be started  
 */

int ha_ndbcluster::external_lock(THD *thd, int lock_type)
{
  int error=0;
  NdbConnection* trans= NULL;

  DBUG_ENTER("external_lock");
  DBUG_PRINT("enter", ("transaction.ndb_lock_count: %d", 
                       thd->transaction.ndb_lock_count));

  /*
    Check that this handler instance has a connection
    set up to the Ndb object of thd
   */
  if (check_ndb_connection())
    DBUG_RETURN(1);
 
  if (lock_type != F_UNLCK)
  {
    DBUG_PRINT("info", ("lock_type != F_UNLCK"));
    if (!thd->transaction.ndb_lock_count++)
    {
      PRINT_OPTION_FLAGS(thd);

      if (!(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN | OPTION_TABLE_LOCK))) 
      {
        // Autocommit transaction
        DBUG_ASSERT(!thd->transaction.stmt.ndb_tid);
        DBUG_PRINT("trans",("Starting transaction stmt"));      

        trans= m_ndb->startTransaction();
        if (trans == NULL)
	{     
          thd->transaction.ndb_lock_count--;    // We didn't get the lock
          ERR_RETURN(m_ndb->getNdbError());
        }
        thd->transaction.stmt.ndb_tid= trans;
      } 
      else 
      { 
        if (!thd->transaction.all.ndb_tid)
	{
          // Not autocommit transaction
          // A "master" transaction ha not been started yet
          DBUG_PRINT("trans",("starting transaction, all"));
          
          trans= m_ndb->startTransaction();
          if (trans == NULL)
	  {   
            thd->transaction.ndb_lock_count--;  // We didn't get the lock
            ERR_RETURN(m_ndb->getNdbError());
          }       

          /*
            If this is the start of a LOCK TABLE, a table look 
            should be taken on the table in NDB
           
            Check if it should be read or write lock
           */
          if (thd->options & (OPTION_TABLE_LOCK))
	  {
            //lockThisTable();
            DBUG_PRINT("info", ("Locking the table..." ));
          }

          thd->transaction.all.ndb_tid= trans; 
        }
      }
    }
    /*
      This is the place to make sure this handler instance
      has a started transaction.
     
      The transaction is started by the first handler on which 
      MySQL Server calls external lock
     
      Other handlers in the same stmt or transaction should use 
      the same NDB transaction. This is done by setting up the m_active_trans
      pointer to point to the NDB transaction. 
     */

    m_active_trans= thd->transaction.all.ndb_tid ? 
      (NdbConnection*)thd->transaction.all.ndb_tid:
      (NdbConnection*)thd->transaction.stmt.ndb_tid;
    DBUG_ASSERT(m_active_trans);

    // Start of transaction
    retrieve_all_fields= FALSE;
    ops_pending= 0;    
  } 
  else 
  {
    DBUG_PRINT("info", ("lock_type == F_UNLCK"));
    if (!--thd->transaction.ndb_lock_count)
    {
      DBUG_PRINT("trans", ("Last external_lock"));
      PRINT_OPTION_FLAGS(thd);

      if (thd->transaction.stmt.ndb_tid)
      {
        /*
          Unlock is done without a transaction commit / rollback.
          This happens if the thread didn't update any rows
          We must in this case close the transaction to release resources
        */
        DBUG_PRINT("trans",("ending non-updating transaction"));
        m_ndb->closeTransaction(m_active_trans);
        thd->transaction.stmt.ndb_tid= 0;
      }
    }
    m_active_trans= NULL;
  }
  DBUG_RETURN(error);
}

/*
  When using LOCK TABLE's external_lock is only called when the actual
  TABLE LOCK is done.
  Under LOCK TABLES, each used tables will force a call to start_stmt.
*/

int ha_ndbcluster::start_stmt(THD *thd)
{
  int error=0;
  DBUG_ENTER("start_stmt");
  PRINT_OPTION_FLAGS(thd);

  NdbConnection *trans= (NdbConnection*)thd->transaction.stmt.ndb_tid;
  if (!trans){
    DBUG_PRINT("trans",("Starting transaction stmt"));  
    
    NdbConnection *tablock_trans= 
      (NdbConnection*)thd->transaction.all.ndb_tid;
    DBUG_PRINT("info", ("tablock_trans: %x", tablock_trans));
    DBUG_ASSERT(tablock_trans);    trans= m_ndb->hupp(tablock_trans);
    if (trans == NULL)
      ERR_RETURN(m_ndb->getNdbError());
    thd->transaction.stmt.ndb_tid= trans;
  }
  m_active_trans= trans;

  // Start of statement
  retrieve_all_fields= FALSE;
  ops_pending= 0;    
  
  DBUG_RETURN(error);
}


/*
  Commit a transaction started in NDB 
 */

int ndbcluster_commit(THD *thd, void *ndb_transaction)
{
  int res= 0;
  Ndb *ndb= (Ndb*)thd->transaction.ndb;
  NdbConnection *trans= (NdbConnection*)ndb_transaction;

  DBUG_ENTER("ndbcluster_commit");
  DBUG_PRINT("transaction",("%s",
                            trans == thd->transaction.stmt.ndb_tid ? 
                            "stmt" : "all"));
  DBUG_ASSERT(ndb && trans);

  if (trans->execute(Commit) != 0)
  {
    const NdbError err= trans->getNdbError();
    ERR_PRINT(err);     
    res= ndb_to_mysql_error(&err);
  }
  ndb->closeTransaction(trans);    
  DBUG_RETURN(res);
}


/*
  Rollback a transaction started in NDB
 */

int ndbcluster_rollback(THD *thd, void *ndb_transaction)
{
  int res= 0;
  Ndb *ndb= (Ndb*)thd->transaction.ndb;
  NdbConnection *trans= (NdbConnection*)ndb_transaction;

  DBUG_ENTER("ndbcluster_rollback");
  DBUG_PRINT("transaction",("%s",
                            trans == thd->transaction.stmt.ndb_tid ? 
                            "stmt" : "all"));
  DBUG_ASSERT(ndb && trans);

  if (trans->execute(Rollback) != 0)
  {
    const NdbError err= trans->getNdbError();
    ERR_PRINT(err);     
    res= ndb_to_mysql_error(&err);
  }
  ndb->closeTransaction(trans);
  DBUG_RETURN(0);
}


/*
  Map MySQL type to the corresponding NDB type
 */

inline NdbDictionary::Column::Type 
mysql_to_ndb_type(enum enum_field_types mysql_type, bool unsigned_flg)
{
  switch(mysql_type) {
  case MYSQL_TYPE_DECIMAL:    
    return NdbDictionary::Column::Char; 
  case MYSQL_TYPE_TINY:        
    return (unsigned_flg) ?
      NdbDictionary::Column::Tinyunsigned :  
      NdbDictionary::Column::Tinyint; 
  case MYSQL_TYPE_SHORT:
    return (unsigned_flg) ?
      NdbDictionary::Column::Smallunsigned :    
      NdbDictionary::Column::Smallint;  
  case MYSQL_TYPE_LONG:
    return (unsigned_flg) ?
      NdbDictionary::Column::Unsigned : 
      NdbDictionary::Column::Int;
  case MYSQL_TYPE_TIMESTAMP:
    return NdbDictionary::Column::Unsigned; 
  case MYSQL_TYPE_LONGLONG:
    return (unsigned_flg) ?
      NdbDictionary::Column::Bigunsigned :
      NdbDictionary::Column::Bigint;
  case MYSQL_TYPE_INT24:       
    return (unsigned_flg) ?
      NdbDictionary::Column::Mediumunsigned :
      NdbDictionary::Column::Mediumint; 
    break;
  case MYSQL_TYPE_FLOAT:
    return NdbDictionary::Column::Float; 
  case MYSQL_TYPE_DOUBLE:
    return NdbDictionary::Column::Double;   
  case MYSQL_TYPE_DATETIME :    
    return NdbDictionary::Column::Datetime; 
  case MYSQL_TYPE_DATE :
  case MYSQL_TYPE_NEWDATE :
  case MYSQL_TYPE_TIME :        
  case MYSQL_TYPE_YEAR :        
    // Missing NDB data types, mapped to char
    return NdbDictionary::Column::Char; 
  case MYSQL_TYPE_ENUM :
    return NdbDictionary::Column::Char; 
  case MYSQL_TYPE_SET :         
    return NdbDictionary::Column::Char; 
  case MYSQL_TYPE_TINY_BLOB :   
  case MYSQL_TYPE_MEDIUM_BLOB :   
  case MYSQL_TYPE_LONG_BLOB :  
  case MYSQL_TYPE_BLOB :    
    return NdbDictionary::Column::Blob; 
  case MYSQL_TYPE_VAR_STRING :  
    return NdbDictionary::Column::Varchar; 
  case MYSQL_TYPE_STRING :      
    return NdbDictionary::Column::Char; 
  case MYSQL_TYPE_NULL :        
  case MYSQL_TYPE_GEOMETRY :    
    return NdbDictionary::Column::Undefined; 
  }
  return NdbDictionary::Column::Undefined;
}


/*
  Create a table in NDB Cluster
 */

int ha_ndbcluster::create(const char *name, 
			  TABLE *form, 
			  HA_CREATE_INFO *info)
{
  NDBTAB tab;
  NdbDictionary::Column::Type ndb_type;
  NDBCOL col;
  uint pack_length, length, i;
  const void *data, *pack_data;
  const char **key_names= form->keynames.type_names;
  char name2[FN_HEADLEN];
   
  DBUG_ENTER("create");
  DBUG_PRINT("enter", ("name: %s", name));
  fn_format(name2, name, "", "",2);       // Remove the .frm extension
  set_dbname(name2);
  set_tabname(name2);  

  DBUG_PRINT("table", ("name: %s", m_tabname));  
  tab.setName(m_tabname);
  tab.setLogging(!(info->options & HA_LEX_CREATE_TMP_TABLE));    
   
  // Save frm data for this table
  if (readfrm(name, &data, &length))
    DBUG_RETURN(1);
  if (packfrm(data, length, &pack_data, &pack_length))
    DBUG_RETURN(2);
  
  DBUG_PRINT("info", ("setFrm data=%x, len=%d", pack_data, pack_length));
  tab.setFrm(pack_data, pack_length);      
  my_free((char*)data, MYF(0));
  my_free((char*)pack_data, MYF(0));
  
  for (i= 0; i < form->fields; i++) 
  {
    Field *field= form->field[i];
    ndb_type= mysql_to_ndb_type(field->real_type(),
				field->flags & UNSIGNED_FLAG);
    DBUG_PRINT("info", ("name: %s, type: %u, pack_length: %d", 
                        field->field_name, field->real_type(),
			field->pack_length()));
    col.setName(field->field_name);
    col.setType(ndb_type);
    if ((ndb_type == NdbDictionary::Column::Char) ||
        (ndb_type == NdbDictionary::Column::Varchar))
      col.setLength(field->pack_length());
    else 
      col.setLength(1);
    col.setNullable(field->maybe_null());
    col.setPrimaryKey(field->flags & PRI_KEY_FLAG);
    if (field->flags & AUTO_INCREMENT_FLAG) 
    {
      col.setAutoIncrement(TRUE);
      ulonglong value= info->auto_increment_value ?
	info->auto_increment_value -1 : (ulonglong) 0;
      DBUG_PRINT("info", ("Autoincrement key, initial: %d", value));
      col.setAutoIncrementInitialValue(value);
    }
    else
      col.setAutoIncrement(false);
    
    tab.addColumn(col);
  }
  
  // No primary key, create shadow key as 64 bit, auto increment  
  if (form->primary_key == MAX_KEY) 
  {
    DBUG_PRINT("info", ("Generating shadow key"));
    col.setName("$PK");
    col.setType(NdbDictionary::Column::Bigunsigned);
    col.setLength(1);
    col.setNullable(false);
    col.setPrimaryKey(TRUE);
    col.setAutoIncrement(TRUE);
    tab.addColumn(col);
  }
  
  my_errno= 0;
  if (check_ndb_connection())
  {
    my_errno= HA_ERR_NO_CONNECTION;
    DBUG_RETURN(my_errno);
  }
  
  // Create the table in NDB     
  NDBDICT *dict= m_ndb->getDictionary();
  if (dict->createTable(tab)) 
  {
    const NdbError err= dict->getNdbError();
    ERR_PRINT(err);
    my_errno= ndb_to_mysql_error(&err);
    DBUG_RETURN(my_errno);
  }
  DBUG_PRINT("info", ("Table %s/%s created successfully", 
                      m_dbname, m_tabname));
  
  if ((my_errno= build_index_list()))
    DBUG_RETURN(my_errno);
  
  // Create secondary indexes
  KEY* key_info= form->key_info;
  const char** key_name= key_names;
  for (i= 0; i < form->keys; i++, key_info++, key_name++) 
  {
    int error= 0;
    DBUG_PRINT("info", ("Index %u: %s", i, *key_name));

    switch (get_index_type_from_table(i)){

    case PRIMARY_KEY_INDEX:
      // Do nothing, already created
      break;
    case PRIMARY_KEY_ORDERED_INDEX:
      error= create_ordered_index(*key_name, key_info);
      break;
    case UNIQUE_ORDERED_INDEX:
      if (!(error= create_ordered_index(*key_name, key_info)))
	error= create_unique_index(get_unique_index_name(i), key_info);
      break;
    case UNIQUE_INDEX:
	error= create_unique_index(get_unique_index_name(i), key_info);
      break;
    case ORDERED_INDEX:
      error= create_ordered_index(*key_name, key_info);
      break;
    default:
      DBUG_ASSERT(false);
      break;
    }

    if (error)
    {
      DBUG_PRINT("error", ("Failed to create index %u", i));
      drop_table();
      my_errno= error;
      break;
    }
  }
  
  DBUG_RETURN(my_errno);
}


int ha_ndbcluster::create_ordered_index(const char *name, 
					KEY *key_info)
{
  DBUG_ENTER("create_ordered_index");
  DBUG_RETURN(create_index(name, key_info, false));
}

int ha_ndbcluster::create_unique_index(const char *name, 
				       KEY *key_info)
{

  DBUG_ENTER("create_unique_index");
  DBUG_RETURN(create_index(name, key_info, true));
}


/*
  Create an index in NDB Cluster
 */

int ha_ndbcluster::create_index(const char *name, 
				KEY *key_info,
				bool unique)
{
  NdbDictionary::Dictionary *dict= m_ndb->getDictionary();
  KEY_PART_INFO *key_part= key_info->key_part;
  KEY_PART_INFO *end= key_part + key_info->key_parts;
  
  DBUG_ENTER("create_index");
  DBUG_PRINT("enter", ("name: %s ", name));

  NdbDictionary::Index ndb_index(name);
  if (unique)
    ndb_index.setType(NdbDictionary::Index::UniqueHashIndex);
  else 
  {
    ndb_index.setType(NdbDictionary::Index::OrderedIndex);
    // TODO Only temporary ordered indexes supported
    ndb_index.setLogging(false); 
  }
  ndb_index.setTable(m_tabname);

  for (; key_part != end; key_part++) 
  {
    Field *field= key_part->field;
    DBUG_PRINT("info", ("attr: %s", field->field_name));
    ndb_index.addColumnName(field->field_name);
  }
  
  if (dict->createIndex(ndb_index))
    ERR_RETURN(dict->getNdbError());

  // Success
  DBUG_PRINT("info", ("Created index %s", name));
  DBUG_RETURN(0);  
}


/*
  Rename a table in NDB Cluster
*/

int ha_ndbcluster::rename_table(const char *from, const char *to)
{
  char new_tabname[FN_HEADLEN];

  DBUG_ENTER("ha_ndbcluster::rename_table");
  set_dbname(from);
  set_tabname(from);
  set_tabname(to, new_tabname);

  if (check_ndb_connection()) {
    my_errno= HA_ERR_NO_CONNECTION;
    DBUG_RETURN(my_errno);
  }

  int result= alter_table_name(m_tabname, new_tabname);
  if (result == 0)
    set_tabname(to);
  
  DBUG_RETURN(result);
}


/*
  Rename a table in NDB Cluster using alter table
 */

int ha_ndbcluster::alter_table_name(const char *from, const char *to)
{
  NDBDICT *dict= m_ndb->getDictionary();
  const NDBTAB *orig_tab;
  DBUG_ENTER("alter_table_name_table");
  DBUG_PRINT("enter", ("Renaming %s to %s", from, to));

  if (!(orig_tab= dict->getTable(from)))
    ERR_RETURN(dict->getNdbError());
      
  NdbDictionary::Table copy_tab= dict->getTableForAlteration(from);
  copy_tab.setName(to);
  if (dict->alterTable(copy_tab) != 0)
    ERR_RETURN(dict->getNdbError());

  m_table= NULL;
                                                                             
  DBUG_RETURN(0);
}


/*
  Delete a table from NDB Cluster
 */

int ha_ndbcluster::delete_table(const char *name)
{
  DBUG_ENTER("delete_table");
  DBUG_PRINT("enter", ("name: %s", name));
  set_dbname(name);
  set_tabname(name);
  
  if (check_ndb_connection())
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
  DBUG_RETURN(drop_table());
}


/*
  Drop a table in NDB Cluster
 */

int ha_ndbcluster::drop_table()
{
  NdbDictionary::Dictionary *dict= m_ndb->getDictionary();

  DBUG_ENTER("drop_table");
  DBUG_PRINT("enter", ("Deleting %s", m_tabname));
  
  if (dict->dropTable(m_tabname)) 
  {
    const NdbError err= dict->getNdbError();
    if (err.code == 709)
      ; // 709: No such table existed
    else 
      ERR_RETURN(dict->getNdbError());
  }  
  release_metadata();
  DBUG_RETURN(0);
}


/*
  Drop a database in NDB Cluster
 */

int ndbcluster_drop_database(const char *path)
{
  DBUG_ENTER("ndbcluster_drop_database");
  // TODO drop all tables for this database
  DBUG_RETURN(1);
}


longlong ha_ndbcluster::get_auto_increment()
{  
  int cache_size = rows_to_insert ? rows_to_insert : 32;
  Uint64 auto_value= 
    m_ndb->getAutoIncrementValue(m_tabname, cache_size);
  return (longlong)auto_value;
}


/*
  Constructor for the NDB Cluster table handler 
 */

ha_ndbcluster::ha_ndbcluster(TABLE *table_arg):
  handler(table_arg),
  m_active_trans(NULL),
  m_active_cursor(NULL),
  m_ndb(NULL),
  m_table(NULL),
  m_table_flags(HA_REC_NOT_IN_SEQ |
                HA_NOT_EXACT_COUNT |
                HA_NO_PREFIX_CHAR_KEYS | 
                HA_NO_BLOBS),
  m_use_write(false),
  retrieve_all_fields(FALSE),
  rows_to_insert(0),
  rows_inserted(0),
  bulk_insert_rows(1024),
  ops_pending(0)
{ 
  int i;
  
  DBUG_ENTER("ha_ndbcluster");

  m_tabname[0]= '\0';
  m_dbname[0]= '\0';

  // TODO Adjust number of records and other parameters for proper 
  // selection of scan/pk access
  records= 100;
  block_size= 1024;

  for (i= 0; i < MAX_KEY; i++)
  {
    m_indextype[i]= UNDEFINED_INDEX;
    m_unique_index_name[i]= NULL;      
  }

  DBUG_VOID_RETURN;
}


/*
  Destructor for NDB Cluster table handler
 */

ha_ndbcluster::~ha_ndbcluster() 
{
  DBUG_ENTER("~ha_ndbcluster");

  release_metadata();

  // Check for open cursor/transaction
  DBUG_ASSERT(m_active_cursor == NULL);
  DBUG_ASSERT(m_active_trans == NULL);

  DBUG_VOID_RETURN;
}


/*
  Open a table for further use
  - fetch metadata for this table from NDB
  - check that table exists
*/

int ha_ndbcluster::open(const char *name, int mode, uint test_if_locked)
{
  KEY *key;
  DBUG_ENTER("open");
  DBUG_PRINT("enter", ("name: %s mode: %d test_if_locked: %d",
                       name, mode, test_if_locked));
  
  // Setup ref_length to make room for the whole 
  // primary key to be written in the ref variable
  
  if (table->primary_key != MAX_KEY) 
  {
    key= table->key_info+table->primary_key;
    ref_length= key->key_length;
    DBUG_PRINT("info", (" ref_length: %d", ref_length));
  }
  // Init table lock structure 
  if (!(m_share=get_share(name)))
    DBUG_RETURN(1);
  thr_lock_data_init(&m_share->lock,&m_lock,(void*) 0);
  
  set_dbname(name);
  set_tabname(name);
  
  if (check_ndb_connection())
    DBUG_RETURN(HA_ERR_NO_CONNECTION);

  DBUG_RETURN(get_metadata(name));
}


/*
  Close the table
  - release resources setup by open()
 */

int ha_ndbcluster::close(void)
{
  DBUG_ENTER("close");  
  free_share(m_share);
  release_metadata();
  m_ndb= NULL;
  DBUG_RETURN(0);
}


Ndb* ha_ndbcluster::seize_ndb()
{
  Ndb* ndb;
  DBUG_ENTER("seize_ndb");

#ifdef USE_NDB_POOL
  // Seize from pool
  ndb= Ndb::seize();
#else
  ndb= new Ndb("");  
#endif
  if (ndb->init(max_transactions) != 0)
  {
    ERR_PRINT(ndb->getNdbError());
    /*
      TODO 
      Alt.1 If init fails because to many allocated Ndb 
      wait on condition for a Ndb object to be released.
      Alt.2 Seize/release from pool, wait until next release 
    */
    delete ndb;
    ndb= NULL;
  }
  DBUG_RETURN(ndb);
}


void ha_ndbcluster::release_ndb(Ndb* ndb)
{
  DBUG_ENTER("release_ndb");
#ifdef USE_NDB_POOL
  // Release to  pool
  Ndb::release(ndb);
#else
  delete ndb;
#endif
  DBUG_VOID_RETURN;
}


/*
  If this thread already has a Ndb object allocated
  in current THD, reuse it. Otherwise
  seize a Ndb object, assign it to current THD and use it.
 
  Having a Ndb object also means that a connection to 
  NDB cluster has been opened. The connection is 
  checked.
 
*/

int ha_ndbcluster::check_ndb_connection()
{
  THD* thd= current_thd;
  Ndb* ndb;
  DBUG_ENTER("check_ndb_connection");
  
  if (!thd->transaction.ndb)
  {
    ndb= seize_ndb();
    if (!ndb)
      DBUG_RETURN(2);
    thd->transaction.ndb= ndb;
  }
  m_ndb= (Ndb*)thd->transaction.ndb;
  m_ndb->setDatabaseName(m_dbname);
  DBUG_RETURN(0);
}

void ndbcluster_close_connection(THD *thd)
{
  Ndb* ndb;
  DBUG_ENTER("ndbcluster_close_connection");
  ndb= (Ndb*)thd->transaction.ndb;
  ha_ndbcluster::release_ndb(ndb);
  thd->transaction.ndb= NULL;
  DBUG_VOID_RETURN;
}


/*
  Try to discover one table from NDB
 */

int ndbcluster_discover(const char *dbname, const char *name,
			const void** frmblob, uint* frmlen)
{
  uint len;
  const void* data;
  const NDBTAB* tab;
  DBUG_ENTER("ndbcluster_discover");
  DBUG_PRINT("enter", ("db: %s, name: %s", dbname, name)); 

  Ndb ndb(dbname);
  if ((ndb.init() != 0) && (ndb.waitUntilReady() != 0))
    ERR_RETURN(ndb.getNdbError());
  
  if (!(tab= ndb.getDictionary()->getTable(name)))
  {
    DBUG_PRINT("info", ("Table %s not found", name));
    DBUG_RETURN(1);
  }
  
  DBUG_PRINT("info", ("Found table %s", tab->getName()));
  
  len= tab->getFrmLength();  
  if (len == 0 || tab->getFrmData() == NULL)
  {
    DBUG_PRINT("No frm data found",
               ("Table is probably created via NdbApi")); 
    DBUG_RETURN(2);
  }
  
  if (unpackfrm(&data, &len, tab->getFrmData()))
    DBUG_RETURN(3);

  *frmlen= len;
  *frmblob= data;
  
  DBUG_RETURN(0);
}


#ifdef USE_DISCOVER_ON_STARTUP
/*
  Dicover tables from NDB Cluster
  - fetch a list of tables from NDB 
  - store the frm file for each table on disk 
   - if the table has an attached frm file
   - if the database of the table exists
*/

int ndb_discover_tables()
{
  uint i;
  NdbDictionary::Dictionary::List list;
  NdbDictionary::Dictionary* dict;
  char  path[FN_REFLEN];
  DBUG_ENTER("ndb_discover_tables");
  
  /* List tables in NDB Cluster kernel    */  
  dict= g_ndb->getDictionary();
  if (dict->listObjects(list, 
			NdbDictionary::Object::UserTable) != 0)
    ERR_RETURN(g_ndb->getNdbError());
  
  for (i= 0 ; i < list.count ; i++)
  {
    NdbDictionary::Dictionary::List::Element& t= list.elements[i];

    DBUG_PRINT("discover", ("%d: %s/%s", t.id, t.database, t.name));     
    if (create_table_from_handler(t.database, t.name, true))
      DBUG_PRINT("info", ("Could not discover %s/%s", t.database, t.name));
  }
  DBUG_RETURN(0);  
}
#endif


/*
  Initialise all gloal variables before creating 
  a NDB Cluster table handler
 */

bool ndbcluster_init()
{
  DBUG_ENTER("ndbcluster_init");
  // Create a Ndb object to open the connection  to NDB
  g_ndb= new Ndb("sys");
  if (g_ndb->init() != 0)
  {
    ERR_PRINT (g_ndb->getNdbError());
    DBUG_RETURN(TRUE);
  }
  if (g_ndb->waitUntilReady() != 0)
  {
    ERR_PRINT (g_ndb->getNdbError());
    DBUG_RETURN(TRUE);   
  }
  (void) hash_init(&ndbcluster_open_tables,system_charset_info,32,0,0,
                   (hash_get_key) ndbcluster_get_key,0,0);
  pthread_mutex_init(&ndbcluster_mutex,MY_MUTEX_INIT_FAST);
  ndbcluster_inited= 1;
#ifdef USE_DISCOVER_ON_STARTUP
  if (ndb_discover_tables() != 0)
    DBUG_RETURN(TRUE);    
#endif
  DBUG_RETURN(false);
}


/*
  End use of the NDB Cluster table handler
  - free all global variables allocated by 
    ndcluster_init()
*/

bool ndbcluster_end()
{
  DBUG_ENTER("ndbcluster_end");
  delete g_ndb;
  g_ndb= NULL;
  if (!ndbcluster_inited)
    DBUG_RETURN(0);
  hash_free(&ndbcluster_open_tables);
#ifdef USE_NDB_POOL
  ndb_pool_release();
#endif
  pthread_mutex_destroy(&ndbcluster_mutex);
  ndbcluster_inited= 0;
  DBUG_RETURN(0);
}

void ndbcluster_print_error(int error)
{
  DBUG_ENTER("ndbcluster_print_error");
  TABLE tab;
  tab.table_name = NULL;
  ha_ndbcluster error_handler(&tab);
  error_handler.print_error(error, MYF(0));
}

/*
  Set m_tabname from full pathname to table file 
 */

void ha_ndbcluster::set_tabname(const char *path_name)
{
  char *end, *ptr;
  
  /* Scan name from the end */
  end= strend(path_name)-1;
  ptr= end;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len= end - ptr;
  memcpy(m_tabname, ptr + 1, end - ptr);
  m_tabname[name_len]= '\0';
#ifdef __WIN__
  /* Put to lower case */
  ptr= m_tabname;
  
  while (*ptr != '\0') {
    *ptr = tolower(*ptr);
    ptr++;
  }
#endif
}

/**
 * Set a given location from full pathname to table file
 *
 */
void
ha_ndbcluster::set_tabname(const char *path_name, char * tabname)
{
  char *end, *ptr;
  
  /* Scan name from the end */
  end = strend(path_name)-1;
  ptr = end;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len = end - ptr;
  memcpy(tabname, ptr + 1, end - ptr);
  tabname[name_len] = '\0';
#ifdef __WIN__
  /* Put to lower case */
  ptr = tabname;
  
  while (*ptr != '\0') {
    *ptr= tolower(*ptr);
    ptr++;
  }
#endif
}


/*
  Set m_dbname from full pathname to table file
 
 */

void ha_ndbcluster::set_dbname(const char *path_name)
{
  char *end, *ptr;
  
  /* Scan name from the end */
  ptr= strend(path_name)-1;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  ptr--;
  end= ptr;
  while (ptr >= path_name && *ptr != '\\' && *ptr != '/') {
    ptr--;
  }
  uint name_len= end - ptr;
  memcpy(m_dbname, ptr + 1, name_len);
  m_dbname[name_len]= '\0';
#ifdef __WIN__
  /* Put to lower case */
  
  ptr= m_dbname;
  
  while (*ptr != '\0') {
    *ptr= tolower(*ptr);
    ptr++;
  }
#endif
}


ha_rows 
ha_ndbcluster::records_in_range(uint inx, key_range *min_key,
                                key_range *max_key)
{
  KEY *key_info= table->key_info + inx;
  uint key_length= key_info->key_length;
  NDB_INDEX_TYPE idx_type= get_index_type(inx);  

  DBUG_ENTER("records_in_range");
  DBUG_PRINT("enter", ("inx: %u", inx));

  // Prevent partial read of hash indexes by returning HA_POS_ERROR
  if ((idx_type == UNIQUE_INDEX || idx_type == PRIMARY_KEY_INDEX) &&
      ((min_key && min_key->length < key_length) ||
       (max_key && max_key->length < key_length)))
    DBUG_RETURN(HA_POS_ERROR);
  
  // Read from hash index with full key
  // This is a "const" table which returns only one record!      
  if ((idx_type != ORDERED_INDEX) &&
      ((min_key && min_key->length == key_length) || 
       (max_key && max_key->length == key_length)))
    DBUG_RETURN(1);
  
  DBUG_RETURN(10); /* Good guess when you don't know anything */
}


/*
  Handling the shared NDB_SHARE structure that is needed to 
  provide table locking.
  It's also used for sharing data with other NDB handlers
  in the same MySQL Server. There is currently not much
  data we want to or can share.
 */

static byte* ndbcluster_get_key(NDB_SHARE *share,uint *length,
				my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (byte*) share->table_name;
}

static NDB_SHARE* get_share(const char *table_name)
{
  NDB_SHARE *share;
  pthread_mutex_lock(&ndbcluster_mutex);
  uint length=(uint) strlen(table_name);
  if (!(share=(NDB_SHARE*) hash_search(&ndbcluster_open_tables,
                                       (byte*) table_name,
                                       length)))
  {
    if ((share=(NDB_SHARE *) my_malloc(sizeof(*share)+length+1,
                                       MYF(MY_WME | MY_ZEROFILL))))
    {
      share->table_name_length=length;
      share->table_name=(char*) (share+1);
      strmov(share->table_name,table_name);
      if (my_hash_insert(&ndbcluster_open_tables, (byte*) share))
      {
        pthread_mutex_unlock(&ndbcluster_mutex);
        my_free((gptr) share,0);
        return 0;
      }
      thr_lock_init(&share->lock);
      pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);
    }
  }
  share->use_count++;
  pthread_mutex_unlock(&ndbcluster_mutex);
  return share;
}


static void free_share(NDB_SHARE *share)
{
  pthread_mutex_lock(&ndbcluster_mutex);
  if (!--share->use_count)
  {
    hash_delete(&ndbcluster_open_tables, (byte*) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&ndbcluster_mutex);
}



/*
  Internal representation of the frm blob
   
*/

struct frm_blob_struct 
{
  struct frm_blob_header 
  {
    uint ver;      // Version of header
    uint orglen;   // Original length of compressed data
    uint complen;  // Compressed length of data, 0=uncompressed
  } head;
  char data[1];  
};



static int packfrm(const void *data, uint len, 
		   const void **pack_data, uint *pack_len)
{
  int error;
  ulong org_len, comp_len;
  uint blob_len;
  frm_blob_struct* blob;
  DBUG_ENTER("packfrm");
  DBUG_PRINT("enter", ("data: %x, len: %d", data, len));
  
  error= 1;
  org_len = len;
  if (my_compress((byte*)data, &org_len, &comp_len))
    goto err;
  
  DBUG_PRINT("info", ("org_len: %d, comp_len: %d", org_len, comp_len));
  DBUG_DUMP("compressed", (char*)data, org_len);
  
  error= 2;
  blob_len= sizeof(frm_blob_struct::frm_blob_header)+org_len;
  if (!(blob= (frm_blob_struct*) my_malloc(blob_len,MYF(MY_WME))))
    goto err;
  
  // Store compressed blob in machine independent format
  int4store((char*)(&blob->head.ver), 1);
  int4store((char*)(&blob->head.orglen), comp_len);
  int4store((char*)(&blob->head.complen), org_len);
  
  // Copy frm data into blob, already in machine independent format
  memcpy(blob->data, data, org_len);  
  
  *pack_data = blob;
  *pack_len = blob_len;
  error = 0;
  
  DBUG_PRINT("exit", ("pack_data: %x, pack_len: %d", *pack_data, *pack_len));
err:
  DBUG_RETURN(error);
  
}


static int unpackfrm(const void **unpack_data, uint *unpack_len,
		    const void *pack_data)
{
   const frm_blob_struct *blob = (frm_blob_struct*)pack_data;
   byte *data;
   ulong complen, orglen, ver;
   DBUG_ENTER("unpackfrm");
   DBUG_PRINT("enter", ("pack_data: %x", pack_data));

   complen=	uint4korr((char*)&blob->head.complen);
   orglen=	uint4korr((char*)&blob->head.orglen);
   ver=		uint4korr((char*)&blob->head.ver);
 
   DBUG_PRINT("blob",("ver: %d complen: %d orglen: %d",
 		     ver,complen,orglen));
   DBUG_DUMP("blob->data", (char*) blob->data, complen);
 
   if (ver != 1)
     DBUG_RETURN(1);
   if (!(data = my_malloc(max(orglen, complen), MYF(MY_WME))))
     DBUG_RETURN(2);
   memcpy(data, blob->data, complen);
 
   if (my_uncompress(data, &complen, &orglen))
   {
     my_free((char*)data, MYF(0));
     DBUG_RETURN(3);
   }

   *unpack_data = data;
   *unpack_len = complen;

   DBUG_PRINT("exit", ("frmdata: %x, len: %d", *unpack_data, *unpack_len));

   DBUG_RETURN(0);
}
#endif /* HAVE_NDBCLUSTER_DB */
