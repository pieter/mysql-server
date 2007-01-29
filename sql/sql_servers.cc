/* Copyright (C) 2000-2003 MySQL AB

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


/*
  The servers are saved in the system table "servers"
*/

#include "mysql_priv.h"
#include "hash_filo.h"
#include <m_ctype.h>
#include <stdarg.h>
#include "sp_head.h"
#include "sp.h"

static my_bool servers_load(THD *thd, TABLE_LIST *tables);
HASH servers_cache;
pthread_mutex_t servers_cache_mutex;                // To init the hash
uint servers_cache_initialised=FALSE;
/* Version of server table. incremented by servers_load */
static uint servers_version=0;
static MEM_ROOT mem;
static rw_lock_t THR_LOCK_servers;
static bool initialized=0;

static byte *servers_cache_get_key(FOREIGN_SERVER *server, uint *length,
			       my_bool not_used __attribute__((unused)))
{
  DBUG_ENTER("servers_cache_get_key");
  DBUG_PRINT("info", ("server_name_length %d server_name %s",
                      server->server_name_length,
                      server->server_name));

  *length= (uint) server->server_name_length;
  DBUG_RETURN((byte*) server->server_name);
}

/*
  Initialize structures responsible for servers used in federated
  server scheme information for them from the server
  table in the 'mysql' database.

  SYNOPSIS
    servers_init()
      dont_read_server_table  TRUE if we want to skip loading data from
                            server table and disable privilege checking.

  NOTES
    This function is mostly responsible for preparatory steps, main work
    on initialization and grants loading is done in servers_reload().

  RETURN VALUES
    0	ok
    1	Could not initialize servers
*/

my_bool servers_init(bool dont_read_servers_table)
{
  THD  *thd;
  my_bool return_val= 0;
  DBUG_ENTER("servers_init");

  /* init the mutex */
  if (pthread_mutex_init(&servers_cache_mutex, MY_MUTEX_INIT_FAST))
    DBUG_RETURN(1);

  if (my_rwlock_init(&THR_LOCK_servers, NULL))
    DBUG_RETURN(1);

  /* initialise our servers cache */
  if (hash_init(&servers_cache, system_charset_info, 32, 0, 0,
                (hash_get_key) servers_cache_get_key, 0, 0))
  {
    return_val= 1; /* we failed, out of memory? */
    goto end;
  }

  /* Initialize the mem root for data */
  init_alloc_root(&mem, ACL_ALLOC_BLOCK_SIZE, 0);

  /*
    at this point, the cache is initialised, let it be known
  */
  servers_cache_initialised= TRUE;

  if (dont_read_servers_table)
    goto end;

  /*
    To be able to run this from boot, we allocate a temporary THD
  */
  if (!(thd=new THD))
    DBUG_RETURN(1);
  thd->thread_stack= (char*) &thd;
  thd->store_globals();
  /*
    It is safe to call servers_reload() since servers_* arrays and hashes which
    will be freed there are global static objects and thus are initialized
    by zeros at startup.
  */
  return_val= servers_reload(thd);
  delete thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD,  0);

end:
  DBUG_RETURN(return_val);
}

/*
  Initialize server structures

  SYNOPSIS
    servers_load()
      thd     Current thread
      tables  List containing open "mysql.servers"

  RETURN VALUES
    FALSE  Success
    TRUE   Error
*/

static my_bool servers_load(THD *thd, TABLE_LIST *tables)
{
  TABLE *table;
  READ_RECORD read_record_info;
  my_bool return_val= TRUE;
  DBUG_ENTER("servers_load");

  if (!servers_cache_initialised)
    DBUG_RETURN(0);

  /* need to figure out how to utilise this variable */
  servers_version++; /* servers updated */

  /* first, send all cached rows to sleep with the fishes, oblivion!
     I expect this crappy comment replaced */
  free_root(&mem, MYF(MY_MARK_BLOCKS_FREE));
  my_hash_reset(&servers_cache);

  init_read_record(&read_record_info,thd,table=tables[0].table,NULL,1,0);
  while (!(read_record_info.read_record(&read_record_info)))
  {
    /* return_val is already TRUE, so no need to set */
    if ((get_server_from_table_to_cache(table)))
      goto end;
  }

  return_val=0;

end:
  end_read_record(&read_record_info);
  DBUG_RETURN(return_val);
}


/*
  Forget current servers cache and read new servers 
  from the conneciton table.

  SYNOPSIS
    servers_reload()
      thd  Current thread

  NOTE
    All tables of calling thread which were open and locked by LOCK TABLES
    statement will be unlocked and closed.
    This function is also used for initialization of structures responsible
    for user/db-level privilege checking.

  RETURN VALUE
    FALSE  Success
    TRUE   Failure
*/

my_bool servers_reload(THD *thd)
{
  TABLE_LIST tables[1];
  my_bool return_val= 1;
  DBUG_ENTER("servers_reload");

  if (thd->locked_tables)
  {					// Can't have locked tables here
    thd->lock=thd->locked_tables;
    thd->locked_tables=0;
    close_thread_tables(thd);
  }

  /*
    To avoid deadlocks we should obtain table locks before
    obtaining servers_cache->lock mutex.
  */
  bzero((char*) tables, sizeof(tables));
  tables[0].alias= tables[0].table_name= (char*) "servers";
  tables[0].db= (char*) "mysql";
  tables[0].lock_type= TL_READ;

  if (simple_open_n_lock_tables(thd, tables))
  {
    sql_print_error("Fatal error: Can't open and lock privilege tables: %s",
		    thd->net.last_error);
    goto end;
  }

  DBUG_PRINT("info", ("locking servers_cache"));
  VOID(pthread_mutex_lock(&servers_cache_mutex));

  //old_servers_cache= servers_cache;
  //old_mem=mem;

  if ((return_val= servers_load(thd, tables)))
  {					// Error. Revert to old list
    /* blast, for now, we have no servers, discuss later way to preserve */

    DBUG_PRINT("error",("Reverting to old privileges"));
    servers_free();
  }

  DBUG_PRINT("info", ("unlocking servers_cache"));
  VOID(pthread_mutex_unlock(&servers_cache_mutex));

end:
  close_thread_tables(thd);
  DBUG_RETURN(return_val);
}

/*
  Initialize structures responsible for servers used in federated
  server scheme information for them from the server
  table in the 'mysql' database.

  SYNOPSIS
    get_server_from_table_to_cache()
      TABLE *table         open table pointer


  NOTES
    This function takes a TABLE pointer (pointing to an opened
    table). With this open table, a FOREIGN_SERVER struct pointer
    is allocated into root memory, then each member of the FOREIGN_SERVER
    struct is populated. A char pointer takes the return value of get_field
    for each column we're interested in obtaining, and if that pointer
    isn't 0x0, the FOREIGN_SERVER member is set to that value, otherwise,
    is set to the value of an empty string, since get_field would set it to
    0x0 if the column's value is empty, even if the default value for that
    column is NOT NULL.

  RETURN VALUES
    0	ok
    1	could not insert server struct into global servers cache
*/

my_bool get_server_from_table_to_cache(TABLE *table)
{
  /* alloc a server struct */
  char *ptr;
  char *blank= (char*)"";
  FOREIGN_SERVER *server= (FOREIGN_SERVER *)alloc_root(&mem,
                                                       sizeof(FOREIGN_SERVER));
  DBUG_ENTER("get_server_from_table_to_cache");
  table->use_all_columns();

  /* get each field into the server struct ptr */
  server->server_name= get_field(&mem, table->field[0]);
  server->server_name_length= strlen(server->server_name);
  ptr= get_field(&mem, table->field[1]);
  server->host= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[2]);
  server->db= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[3]);
  server->username= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[4]);
  server->password= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[5]);
  server->sport= ptr ? ptr : blank;

  server->port= server->sport ? atoi(server->sport) : 0;

  ptr= get_field(&mem, table->field[6]);
  server->socket= ptr && strlen(ptr) ? ptr : NULL;
  ptr= get_field(&mem, table->field[7]);
  server->scheme= ptr ? ptr : blank;
  ptr= get_field(&mem, table->field[8]);
  server->owner= ptr ? ptr : blank;
  DBUG_PRINT("info", ("server->server_name %s", server->server_name));
  DBUG_PRINT("info", ("server->host %s", server->host));
  DBUG_PRINT("info", ("server->db %s", server->db));
  DBUG_PRINT("info", ("server->username %s", server->username));
  DBUG_PRINT("info", ("server->password %s", server->password));
  DBUG_PRINT("info", ("server->socket %s", server->socket));
  if (my_hash_insert(&servers_cache, (byte*) server))
  {
    DBUG_PRINT("info", ("had a problem inserting server %s at %lx",
                        server->server_name, (long unsigned int) server));
    // error handling needed here
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}

/*
  SYNOPSIS
    server_exists_in_table()
      THD   *thd - thread pointer
      LEX_SERVER_OPTIONS *server_options - pointer to Lex->server_options

  NOTES
    This function takes a LEX_SERVER_OPTIONS struct, which is very much the
    same type of structure as a FOREIGN_SERVER, it contains the values parsed
    in any one of the [CREATE|DELETE|DROP] SERVER statements. Using the
    member "server_name", index_read_idx either founds the record and returns
    1, or doesn't find the record, and returns 0

  RETURN VALUES
    0   record not found
    1	record found
*/

my_bool server_exists_in_table(THD *thd, LEX_SERVER_OPTIONS *server_options)
{
  byte server_key[MAX_KEY_LENGTH];
  int result= 1;
  int error= 0;
  TABLE_LIST tables;
  TABLE *table;

  DBUG_ENTER("server_exists");

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.alias= tables.table_name= (char*) "servers";

  table->use_all_columns();

  /* need to open before acquiring THR_LOCK_plugin or it will deadlock */
  if (! (table= open_ltable(thd, &tables, TL_WRITE)))
    DBUG_RETURN(TRUE);

  rw_wrlock(&THR_LOCK_servers);
  VOID(pthread_mutex_lock(&servers_cache_mutex));

  /* set the field that's the PK to the value we're looking for */
  table->field[0]->store(server_options->server_name,
                         server_options->server_name_length,
                         system_charset_info);

  if ((error= table->file->index_read_idx(table->record[0], 0,
                                   (byte *)table->field[0]->ptr, ~(ulonglong)0,
                                   HA_READ_KEY_EXACT)))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    {
      table->file->print_error(error, MYF(0));
      result= -1;
    }
    result= 0;
    DBUG_PRINT("info",("record for server '%s' not found!",
                       server_options->server_name));
  }

  VOID(pthread_mutex_unlock(&servers_cache_mutex));
  rw_unlock(&THR_LOCK_servers);
  DBUG_RETURN(result);
}

/*
  SYNOPSIS
    insert_server()
      THD   *thd     - thread pointer
      FOREIGN_SERVER *server - pointer to prepared FOREIGN_SERVER struct

  NOTES
    This function takes a server object that is has all members properly
    prepared, ready to be inserted both into the mysql.servers table and
    the servers cache.

  RETURN VALUES
    0  - no error
    other - error code
*/

int insert_server(THD *thd, FOREIGN_SERVER *server)
{
  byte server_key[MAX_KEY_LENGTH];
  int error= 0;
  TABLE_LIST tables;
  TABLE *table;

  DBUG_ENTER("insert_server");

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.alias= tables.table_name= (char*) "servers";

  /* need to open before acquiring THR_LOCK_plugin or it will deadlock */
  if (! (table= open_ltable(thd, &tables, TL_WRITE)))
    DBUG_RETURN(TRUE);

  /* lock mutex to make sure no changes happen */
  VOID(pthread_mutex_lock(&servers_cache_mutex));

  /* lock table */
  rw_wrlock(&THR_LOCK_servers);

  /* insert the server into the table */
  if ((error= insert_server_record(table, server)))
    goto end;

  /* insert the server into the cache */
  if ((error= insert_server_record_into_cache(server)))
    goto end;

end:
  /* unlock the table */
  rw_unlock(&THR_LOCK_servers);
  VOID(pthread_mutex_unlock(&servers_cache_mutex));
  DBUG_RETURN(error);
}

/*
  SYNOPSIS
    int insert_server_record_into_cache()
      FOREIGN_SERVER *server

  NOTES
    This function takes a FOREIGN_SERVER pointer to an allocated (root mem)
    and inserts it into the global servers cache

  RETURN VALUE
    0   - no error
    >0  - error code

*/

int insert_server_record_into_cache(FOREIGN_SERVER *server)
{
  int error=0;
  DBUG_ENTER("insert_server_record_into_cache");
  /*
    We succeded in insertion of the server to the table, now insert
    the server to the cache
  */
  DBUG_PRINT("info", ("inserting server %s at %lx, length %d",
                        server->server_name, (long unsigned int) server,
                        server->server_name_length));
  if (my_hash_insert(&servers_cache, (byte*) server))
  {
    DBUG_PRINT("info", ("had a problem inserting server %s at %lx",
                        server->server_name, (long unsigned int) server));
    // error handling needed here
    error= 1;
  }
  DBUG_RETURN(error);
}

/*
  SYNOPSIS
    store_server_fields()
      TABLE *table
      FOREIGN_SERVER *server

  NOTES
    This function takes an opened table object, and a pointer to an 
    allocated FOREIGN_SERVER struct, and then stores each member of
    the FOREIGN_SERVER to the appropriate fields in the table, in 
    advance of insertion into the mysql.servers table

  RETURN VALUE
    VOID

*/

void store_server_fields(TABLE *table, FOREIGN_SERVER *server)
{

  table->use_all_columns();
  /*
    "server" has already been prepped by prepare_server_struct_for_<>
    so, all we need to do is check if the value is set (> -1 for port)

    If this happens to be an update, only the server members that 
    have changed will be set. If an insert, then all will be set,
    even if with empty strings
  */
  if (server->host)
    table->field[1]->store(server->host,
                           (uint) strlen(server->host), system_charset_info);
  if (server->db)
    table->field[2]->store(server->db,
                           (uint) strlen(server->db), system_charset_info);
  if (server->username)
    table->field[3]->store(server->username,
                           (uint) strlen(server->username), system_charset_info);
  if (server->password)
    table->field[4]->store(server->password,
                           (uint) strlen(server->password), system_charset_info);
  if (server->port > -1)
    table->field[5]->store(server->port);

  if (server->socket)
    table->field[6]->store(server->socket,
                           (uint) strlen(server->socket), system_charset_info);
  if (server->scheme)
    table->field[7]->store(server->scheme,
                           (uint) strlen(server->scheme), system_charset_info);
  if (server->owner)
    table->field[8]->store(server->owner,
                           (uint) strlen(server->owner), system_charset_info);
}

/*
  SYNOPSIS
    insert_server_record()
      TABLE *table
      FOREIGN_SERVER *server

  NOTES
    This function takes the arguments of an open table object and a pointer
    to an allocated FOREIGN_SERVER struct. It stores the server_name into
    the first field of the table (the primary key, server_name column). With
    this, index_read_idx is called, if the record is found, an error is set
    to ER_FOREIGN_SERVER_EXISTS (the server with that server name exists in the
    table), if not, then store_server_fields stores all fields of the
    FOREIGN_SERVER to the table, then ha_write_row is inserted. If an error
    is encountered in either index_read_idx or ha_write_row, then that error
    is returned

  RETURN VALUE
    0 - no errors
    >0 - error code

  */

int insert_server_record(TABLE *table, FOREIGN_SERVER *server)
{
  int error;
  DBUG_ENTER("insert_server_record");
  table->use_all_columns();

  /* set the field that's the PK to the value we're looking for */
  table->field[0]->store(server->server_name,
                         server->server_name_length,
                         system_charset_info);

  /* read index until record is that specified in server_name */
  if ((error= table->file->index_read_idx(table->record[0], 0,
                                   (byte *)table->field[0]->ptr, ~(longlong)0,
                                   HA_READ_KEY_EXACT)))
  {
    /* if not found, err */
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    {
      table->file->print_error(error, MYF(0));
      error= 1;
    }
    /* store each field to be inserted */
    store_server_fields(table, server);

    DBUG_PRINT("info",("record for server '%s' not found!",
                       server->server_name));
    /* write/insert the new server */
    if ((error=table->file->ha_write_row(table->record[0])))
    {
      table->file->print_error(error, MYF(0));
    }
    else
      error= 0;
  }
  else
    error= ER_FOREIGN_SERVER_EXISTS;
  DBUG_RETURN(error);
}

/*
  SYNOPSIS
    drop_server()
      THD *thd
      LEX_SERVER_OPTIONS *server_options

  NOTES
    This function takes as its arguments a THD object pointer and a pointer
    to a LEX_SERVER_OPTIONS struct from the parser. The member 'server_name'
    of this LEX_SERVER_OPTIONS struct contains the value of the server to be
    deleted. The mysql.servers table is opened via open_ltable, a table object
    returned, the servers cache mutex locked, then delete_server_record is
    called with this table object and LEX_SERVER_OPTIONS server_name and
    server_name_length passed, containing the name of the server to be
    dropped/deleted, then delete_server_record_in_cache is called to delete
    the server from the servers cache.

  RETURN VALUE
    0 - no error
    > 0 - error code
*/

int drop_server(THD *thd, LEX_SERVER_OPTIONS *server_options)
{
  byte server_key[MAX_KEY_LENGTH];
  int error= 0;
  TABLE_LIST tables;
  TABLE *table;

  DBUG_ENTER("drop_server");
  DBUG_PRINT("info", ("server name server->server_name %s",
                      server_options->server_name));

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*) "mysql";
  tables.alias= tables.table_name= (char*) "servers";

  /* need to open before acquiring THR_LOCK_plugin or it will deadlock */
  if (! (table= open_ltable(thd, &tables, TL_WRITE)))
    DBUG_RETURN(TRUE);

  rw_wrlock(&THR_LOCK_servers);
  VOID(pthread_mutex_lock(&servers_cache_mutex));


  if ((error= delete_server_record(table,
                                   server_options->server_name,
                                   server_options->server_name_length)))
    goto end;


  if ((error= delete_server_record_in_cache(server_options)))
    goto end;

end:
  VOID(pthread_mutex_unlock(&servers_cache_mutex));
  rw_unlock(&THR_LOCK_servers);
  DBUG_RETURN(error);
}
/*

  SYNOPSIS
    delete_server_record_in_cache()
      LEX_SERVER_OPTIONS *server_options

  NOTES
    This function's  argument is a LEX_SERVER_OPTIONS struct pointer. This
    function uses the "server_name" and "server_name_length" members of the
    lex->server_options to search for the server in the servers_cache. Upon
    returned the server (pointer to a FOREIGN_SERVER struct), it then deletes
    that server from the servers_cache hash.

  RETURN VALUE
    0 - no error

*/

int delete_server_record_in_cache(LEX_SERVER_OPTIONS *server_options)
{

  int error= 0;
  FOREIGN_SERVER *server;
  DBUG_ENTER("delete_server_record_in_cache");

  DBUG_PRINT("info",("trying to obtain server name %s length %d",
                     server_options->server_name,
                     server_options->server_name_length));


  if (!(server= (FOREIGN_SERVER *) hash_search(&servers_cache,
                                     (byte*) server_options->server_name,
                                     server_options->server_name_length)))
  {
    DBUG_PRINT("info", ("server_name %s length %d not found!",
                        server_options->server_name,
                        server_options->server_name_length));
    // what should be done if not found in the cache?
  }
  /*
    We succeded in deletion of the server to the table, now delete
    the server from the cache
  */
  DBUG_PRINT("info",("deleting server %s length %d",
                     server->server_name,
                     server->server_name_length));

  if (server)
    VOID(hash_delete(&servers_cache, (byte*) server));

  servers_version++; /* servers updated */

  DBUG_RETURN(error);
}

/*

  SYNOPSIS
    update_server()
      THD *thd
      FOREIGN_SERVER *existing
      FOREIGN_SERVER *altered

  NOTES
    This function takes as arguments a THD object pointer, and two pointers,
    one pointing to the existing FOREIGN_SERVER struct "existing" (which is
    the current record as it is) and another pointer pointing to the
    FOREIGN_SERVER struct with the members containing the modified/altered
    values that need to be updated in both the mysql.servers table and the 
    servers_cache. It opens a table, passes the table and the altered
    FOREIGN_SERVER pointer, which will be used to update the mysql.servers 
    table for the particular server via the call to update_server_record,
    and in the servers_cache via update_server_record_in_cache. 

  RETURN VALUE
    0 - no error
    >0 - error code

*/

int update_server(THD *thd, FOREIGN_SERVER *existing, FOREIGN_SERVER *altered)
{
  int error= 0;
  TABLE *table;
  TABLE_LIST tables;
  DBUG_ENTER("update_server");

  bzero((char*) &tables, sizeof(tables));
  tables.db= (char*)"mysql";
  tables.alias= tables.table_name= (char*)"servers";

  if (!(table= open_ltable(thd, &tables, TL_WRITE)))
    DBUG_RETURN(1);

  rw_wrlock(&THR_LOCK_servers);
  if ((error= update_server_record(table, altered)))
    goto end;

  update_server_record_in_cache(existing, altered);

end:
  rw_unlock(&THR_LOCK_servers);
  DBUG_RETURN(error);
}

/*

  SYNOPSIS
    update_server_record_in_cache()
      FOREIGN_SERVER *existing
      FOREIGN_SERVER *altered

  NOTES
    This function takes as an argument the FOREIGN_SERVER structi pointer
    for the existing server and the FOREIGN_SERVER struct populated with only 
    the members which have been updated. It then "merges" the "altered" struct
    members to the existing server, the existing server then represents an
    updated server. Then, the existing record is deleted from the servers_cache
    HASH, then the updated record inserted, in essence replacing the old
    record.

  RETURN VALUE
    0 - no error
    1 - error

*/

int update_server_record_in_cache(FOREIGN_SERVER *existing,
                                  FOREIGN_SERVER *altered)
{
  int error= 0;
  DBUG_ENTER("update_server_record_in_cache");

  /*
    update the members that haven't been change in the altered server struct
    with the values of the existing server struct
  */
  merge_server_struct(existing, altered);

  /*
    delete the existing server struct from the server cache
  */
  VOID(hash_delete(&servers_cache, (byte*)existing));

  /*
    Insert the altered server struct into the server cache
  */
  if (my_hash_insert(&servers_cache, (byte*)altered))
  {
    DBUG_PRINT("info", ("had a problem inserting server %s at %lx",
                        altered->server_name, (long unsigned int) altered));
    error= 1;
  }

  servers_version++; /* servers updated */
  DBUG_RETURN(error);
}

/*

  SYNOPSIS
    merge_server_struct()
      FOREIGN_SERVER *from
      FOREIGN_SERVER *to

  NOTES
    This function takes as its arguments two pointers each to an allocated
    FOREIGN_SERVER struct. The first FOREIGN_SERVER struct represents the struct
    that we will obtain values from (hence the name "from"), the second
    FOREIGN_SERVER struct represents which FOREIGN_SERVER struct we will be
    "copying" any members that have a value to (hence the name "to")

  RETURN VALUE
    VOID

*/

void merge_server_struct(FOREIGN_SERVER *from, FOREIGN_SERVER *to)
{
  DBUG_ENTER("merge_server_struct");
  if (!to->host)
    to->host= strdup_root(&mem, from->host);
  if (!to->db)
    to->db= strdup_root(&mem, from->db);
  if (!to->username)
    to->username= strdup_root(&mem, from->username);
  if (!to->password)
    to->password= strdup_root(&mem, from->password);
  if (to->port == -1)
    to->port= from->port;
  if (!to->socket)
    to->socket= strdup_root(&mem, from->socket);
  if (!to->scheme)
    to->scheme= strdup_root(&mem, from->scheme);
  if (!to->owner)
    to->owner= strdup_root(&mem, from->owner);

  DBUG_VOID_RETURN;
}

/*

  SYNOPSIS
    update_server_record()
      TABLE *table
      FOREIGN_SERVER *server

  NOTES
    This function takes as its arguments an open TABLE pointer, and a pointer
    to an allocated FOREIGN_SERVER structure representing an updated record
    which needs to be inserted. The primary key, server_name is stored to field
    0, then index_read_idx is called to read the index to that record, the
    record then being ready to be updated, if found. If not found an error is
    set and error message printed. If the record is found, store_record is
    called, then store_server_fields stores each field from the the members of
    the updated FOREIGN_SERVER struct.

  RETURN VALUE
    0 - no error

*/

int update_server_record(TABLE *table, FOREIGN_SERVER *server)
{
  int error=0;
  DBUG_ENTER("update_server_record");
  table->use_all_columns();
  /* set the field that's the PK to the value we're looking for */
  table->field[0]->store(server->server_name,
                         server->server_name_length,
                         system_charset_info);

  if ((error= table->file->index_read_idx(table->record[0], 0,
                                   (byte *)table->field[0]->ptr, ~(longlong)0,
                                   HA_READ_KEY_EXACT)))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    {
      table->file->print_error(error, MYF(0));
      error= 1;
    }
    DBUG_PRINT("info",("server not found!"));
    error= ER_FOREIGN_SERVER_DOESNT_EXIST;
  }
  else
  {
    /* ok, so we can update since the record exists in the table */
    store_record(table,record[1]);
    store_server_fields(table, server);
    if ((error=table->file->ha_update_row(table->record[1],table->record[0])))
    {
      DBUG_PRINT("info",("problems with ha_update_row %d", error));
      goto end;
    }
  }

end:
  DBUG_RETURN(error);
}

/*

  SYNOPSIS
    delete_server_record()
      TABLE *table
      char *server_name
      int server_name_length

  NOTES

  RETURN VALUE
    0 - no error

*/

int delete_server_record(TABLE *table,
                         char *server_name,
                         int server_name_length)
{
  int error= 0;
  DBUG_ENTER("delete_server_record");
  table->use_all_columns();

  /* set the field that's the PK to the value we're looking for */
  table->field[0]->store(server_name, server_name_length, system_charset_info);

  if ((error= table->file->index_read_idx(table->record[0], 0,
                                   (byte *)table->field[0]->ptr, ~(ulonglong)0,
                                   HA_READ_KEY_EXACT)))
  {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    {
      table->file->print_error(error, MYF(0));
      error= 1;
    }
    DBUG_PRINT("info",("server not found!"));
    error= ER_FOREIGN_SERVER_DOESNT_EXIST;
  }
  else
  {
    if ((error= table->file->ha_delete_row(table->record[0])))
      table->file->print_error(error, MYF(0));
  }

  DBUG_RETURN(error);
}

/*

  SYNOPSIS
    create_server()
        THD *thd
        LEX_SERVER_OPTIONS *server_options

  NOTES

  RETURN VALUE
    0 - no error

*/

int create_server(THD *thd, LEX_SERVER_OPTIONS *server_options)
{
  int error;
  FOREIGN_SERVER *server;

  DBUG_ENTER("create_server");
  DBUG_PRINT("info", ("server_options->server_name %s",
                      server_options->server_name));

  server= (FOREIGN_SERVER *)alloc_root(&mem,
                                       sizeof(FOREIGN_SERVER));

  if ((error= prepare_server_struct_for_insert(server_options, server)))
    goto end;

  if ((error= insert_server(thd, server)))
    goto end;

  DBUG_PRINT("info", ("error returned %d", error));

end:
  DBUG_RETURN(error);
}

/*

  SYNOPSIS
    alter_server()
      THD *thd
      LEX_SERVER_OPTIONS *server_options

  NOTES

  RETURN VALUE
    0 - no error

*/

int alter_server(THD *thd, LEX_SERVER_OPTIONS *server_options)
{
  int error= 0;
  FOREIGN_SERVER *altered, *existing;
  DBUG_ENTER("alter_server");
  DBUG_PRINT("info", ("server_options->server_name %s",
                      server_options->server_name));

  altered= (FOREIGN_SERVER *)alloc_root(&mem,
                                        sizeof(FOREIGN_SERVER));

  VOID(pthread_mutex_lock(&servers_cache_mutex));

  if (!(existing= (FOREIGN_SERVER *) hash_search(&servers_cache,
                                                 (byte*) server_options->server_name,
                                               server_options->server_name_length)))
  {
    error= ER_FOREIGN_SERVER_DOESNT_EXIST;
    goto end;
  }

  if ((error= prepare_server_struct_for_update(server_options, existing, altered)))
    goto end;

  if ((error= update_server(thd, existing, altered)))
    goto end;

end:
  DBUG_PRINT("info", ("error returned %d", error));
  VOID(pthread_mutex_unlock(&servers_cache_mutex));
  DBUG_RETURN(error);
}

/*

  SYNOPSIS
    prepare_server_struct_for_insert()
      LEX_SERVER_OPTIONS *server_options
      FOREIGN_SERVER *server

  NOTES

  RETURN VALUE
    0 - no error

*/

int prepare_server_struct_for_insert(LEX_SERVER_OPTIONS *server_options,
                                     FOREIGN_SERVER *server)
{
  int error;
  char *unset_ptr= (char*)"";
  DBUG_ENTER("prepare_server_struct");

  error= 0;

  /* these two MUST be set */
  server->server_name= strdup_root(&mem, server_options->server_name);
  server->server_name_length= server_options->server_name_length;

  server->host= server_options->host ?
    strdup_root(&mem, server_options->host) : unset_ptr;

  server->db= server_options->db ?
    strdup_root(&mem, server_options->db) : unset_ptr;

  server->username= server_options->username ?
    strdup_root(&mem, server_options->username) : unset_ptr;

  server->password= server_options->password ?
    strdup_root(&mem, server_options->password) : unset_ptr;

  /* set to 0 if not specified */
  server->port= server_options->port > -1 ?
    server_options->port : 0;

  server->socket= server_options->socket ?
    strdup_root(&mem, server_options->socket) : unset_ptr;

  server->scheme= server_options->scheme ?
    strdup_root(&mem, server_options->scheme) : unset_ptr;

  server->owner= server_options->owner ?
    strdup_root(&mem, server_options->owner) : unset_ptr;

  DBUG_RETURN(error);
}

/*

  SYNOPSIS
    prepare_server_struct_for_update()
      LEX_SERVER_OPTIONS *server_options

  NOTES

  RETURN VALUE
    0 - no error

*/

int prepare_server_struct_for_update(LEX_SERVER_OPTIONS *server_options,
                                     FOREIGN_SERVER *existing,
                                     FOREIGN_SERVER *altered)
{
  int error;
  DBUG_ENTER("prepare_server_struct_for_update");
  error= 0;

  altered->server_name= strdup_root(&mem, server_options->server_name);
  altered->server_name_length= server_options->server_name_length;
  DBUG_PRINT("info", ("existing name %s altered name %s",
                      existing->server_name, altered->server_name));

  /*
    The logic here is this: is this value set AND is it different
    than the existing value?
  */
  altered->host=
    (server_options->host && (strcmp(server_options->host, existing->host))) ?
     strdup_root(&mem, server_options->host) : 0;

  altered->db=
      (server_options->db && (strcmp(server_options->db, existing->db))) ?
        strdup_root(&mem, server_options->db) : 0;

  altered->username=
      (server_options->username &&
      (strcmp(server_options->username, existing->username))) ?
        strdup_root(&mem, server_options->username) : 0;

  altered->password=
      (server_options->password &&
      (strcmp(server_options->password, existing->password))) ?
        strdup_root(&mem, server_options->password) : 0;

  /*
    port is initialised to -1, so if unset, it will be -1
  */
  altered->port= (server_options->port > -1 &&
                 server_options->port != existing->port) ?
    server_options->port : -1;

  altered->socket=
    (server_options->socket &&
    (strcmp(server_options->socket, existing->socket))) ?
      strdup_root(&mem, server_options->socket) : 0;

  altered->scheme=
    (server_options->scheme &&
    (strcmp(server_options->scheme, existing->scheme))) ?
      strdup_root(&mem, server_options->scheme) : 0;

  altered->owner=
    (server_options->owner &&
    (strcmp(server_options->owner, existing->owner))) ?
      strdup_root(&mem, server_options->owner) : 0;

  DBUG_RETURN(error);
}

/*

  SYNOPSIS
    servers_free()
      bool end

  NOTES

  RETURN VALUE
    void

*/

void servers_free(bool end)
{
  DBUG_ENTER("servers_free");
  if (!servers_cache_initialised)
    DBUG_VOID_RETURN;
  VOID(pthread_mutex_destroy(&servers_cache_mutex));
  servers_cache_initialised=0;
  free_root(&mem,MYF(0));
  hash_free(&servers_cache);
  DBUG_VOID_RETURN;
}



/*

  SYNOPSIS
    get_server_by_name()
      const char *server_name

  NOTES

  RETURN VALUE
   FOREIGN_SERVER *

*/

FOREIGN_SERVER *get_server_by_name(const char *server_name)
{
  ulong error_num=0;
  uint i, server_name_length;
  FOREIGN_SERVER *server= 0;
  DBUG_ENTER("get_server_by_name");
  DBUG_PRINT("info", ("server_name %s", server_name));

  server_name_length= strlen(server_name);

  if (! server_name || !strlen(server_name))
  {
    DBUG_PRINT("info", ("server_name not defined!"));
    error_num= 1;
    DBUG_RETURN((FOREIGN_SERVER *)NULL);
  }

  DBUG_PRINT("info", ("locking servers_cache"));
  VOID(pthread_mutex_lock(&servers_cache_mutex));
  if (!(server= (FOREIGN_SERVER *) hash_search(&servers_cache,
                                               (byte*) server_name,
                                               server_name_length)))
  {
    DBUG_PRINT("info", ("server_name %s length %d not found!",
                        server_name, server_name_length));
    server= (FOREIGN_SERVER *) NULL;
  }
  DBUG_PRINT("info", ("unlocking servers_cache"));
  VOID(pthread_mutex_unlock(&servers_cache_mutex));
  DBUG_RETURN(server);

}
