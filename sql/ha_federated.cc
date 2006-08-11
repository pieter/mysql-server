/* Copyright (C) 2004 MySQL AB

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

/*

  MySQL Federated Storage Engine

  ha_federated.cc - MySQL Federated Storage Engine
  Patrick Galbraith and Brian Aker, 2004

  This is a handler which uses a foreign database as the data file, as
  opposed to a handler like MyISAM, which uses .MYD files locally.

  How this handler works
  ----------------------------------
  Normal database files are local and as such: You create a table called
  'users', a file such as 'users.MYD' is created. A handler reads, inserts,
  deletes, updates data in this file. The data is stored in particular format,
  so to read, that data has to be parsed into fields, to write, fields have to
  be stored in this format to write to this data file.

  With MySQL Federated storage engine, there will be no local files
  for each table's data (such as .MYD). A foreign database will store
  the data that would normally be in this file. This will necessitate
  the use of MySQL client API to read, delete, update, insert this
  data. The data will have to be retrieve via an SQL call "SELECT *
  FROM users". Then, to read this data, it will have to be retrieved
  via mysql_fetch_row one row at a time, then converted from the
  column in this select into the format that the handler expects.

  The create table will simply create the .frm file, and within the
  "CREATE TABLE" SQL, there SHALL be any of the following :

  comment=scheme://username:password@hostname:port/database/tablename
  comment=scheme://username@hostname/database/tablename
  comment=scheme://username:password@hostname/database/tablename
  comment=scheme://username:password@hostname/database/tablename

  An example would be:

  comment=mysql://username:password@hostname:port/database/tablename

  ***IMPORTANT***

  This is a first release, conceptual release
  Only 'mysql://' is supported at this release.


  This comment connection string is necessary for the handler to be
  able to connect to the foreign server.


  The basic flow is this:

  SQL calls issues locally ->
  mysql handler API (data in handler format) ->
  mysql client API (data converted to SQL calls) ->
  foreign database -> mysql client API ->
  convert result sets (if any) to handler format ->
  handler API -> results or rows affected to local

  What this handler does and doesn't support
  ------------------------------------------
  * Tables MUST be created on the foreign server prior to any action on those
    tables via the handler, first version. IMPORTANT: IF you MUST use the
    federated storage engine type on the REMOTE end, MAKE SURE [ :) ] That
    the table you connect to IS NOT a table pointing BACK to your ORIGNAL
    table! You know  and have heard the screaching of audio feedback? You
    know putting two mirror in front of each other how the reflection
    continues for eternity? Well, need I say more?!
  * There will not be support for transactions.
  * There is no way for the handler to know if the foreign database or table
    has changed. The reason for this is that this database has to work like a
    data file that would never be written to by anything other than the
    database. The integrity of the data in the local table could be breached
    if there was any change to the foreign database.
  * Support for SELECT, INSERT, UPDATE , DELETE, indexes.
  * No ALTER TABLE, DROP TABLE or any other Data Definition Language calls.
  * Prepared statements will not be used in the first implementation, it
    remains to to be seen whether the limited subset of the client API for the
    server supports this.
  * This uses SELECT, INSERT, UPDATE, DELETE and not HANDLER for its
    implementation.
  * This will not work with the query cache.

   Method calls

   A two column table, with one record:

   (SELECT)

   "SELECT * FROM foo"
    ha_federated::info
    ha_federated::scan_time:
    ha_federated::rnd_init: share->select_query SELECT * FROM foo
    ha_federated::extra

    <for every row of data retrieved>
    ha_federated::rnd_next
    ha_federated::convert_row_to_internal_format
    ha_federated::rnd_next
    </for every row of data retrieved>

    ha_federated::rnd_end
    ha_federated::extra
    ha_federated::reset

    (INSERT)

    "INSERT INTO foo (id, ts) VALUES (2, now());"

    ha_federated::write_row

    ha_federated::reset

    (UPDATE)

    "UPDATE foo SET ts = now() WHERE id = 1;"

    ha_federated::index_init
    ha_federated::index_read
    ha_federated::index_read_idx
    ha_federated::rnd_next
    ha_federated::convert_row_to_internal_format
    ha_federated::update_row

    ha_federated::extra
    ha_federated::extra
    ha_federated::extra
    ha_federated::external_lock
    ha_federated::reset


    How do I use this handler?
    --------------------------
    First of all, you need to build this storage engine:

      ./configure --with-federated-storage-engine
      make

    Next, to use this handler, it's very simple. You must
    have two databases running, either both on the same host, or
    on different hosts.

    One the server that will be connecting to the foreign
    host (client), you create your table as such:

    CREATE TABLE test_table (
      id     int(20) NOT NULL auto_increment,
      name   varchar(32) NOT NULL default '',
      other  int(20) NOT NULL default '0',
      PRIMARY KEY  (id),
      KEY name (name),
      KEY other_key (other))
       ENGINE="FEDERATED"
       DEFAULT CHARSET=latin1
       COMMENT='root@127.0.0.1:9306/federated/test_federated';

   Notice the "COMMENT" and "ENGINE" field? This is where you
   respectively set the engine type, "FEDERATED" and foreign
   host information, this being the database your 'client' database
   will connect to and use as the "data file". Obviously, the foreign
   database is running on port 9306, so you want to start up your other
   database so that it is indeed on port 9306, and your federated
   database on a port other than that. In my setup, I use port 5554
   for federated, and port 5555 for the foreign database.

   Then, on the foreign database:

   CREATE TABLE test_table (
     id     int(20) NOT NULL auto_increment,
     name   varchar(32) NOT NULL default '',
     other  int(20) NOT NULL default '0',
     PRIMARY KEY  (id),
     KEY name (name),
     KEY other_key (other))
     ENGINE="<NAME>" <-- whatever you want, or not specify
     DEFAULT CHARSET=latin1 ;

    This table is exactly the same (and must be exactly the same),
    except that it is not using the federated handler and does
    not need the URL.


    How to see the handler in action
    --------------------------------

    When developing this handler, I compiled the federated database with
    debugging:

    ./configure --with-federated-storage-engine
    --prefix=/home/mysql/mysql-build/federated/ --with-debug

    Once compiled, I did a 'make install' (not for the purpose of installing
    the binary, but to install all the files the binary expects to see in the
    diretory I specified in the build with --prefix,
    "/home/mysql/mysql-build/federated".

    Then, I started the foreign server:

    /usr/local/mysql/bin/mysqld_safe
    --user=mysql --log=/tmp/mysqld.5555.log -P 5555

    Then, I went back to the directory containing the newly compiled mysqld,
    <builddir>/sql/, started up gdb:

    gdb ./mysqld

    Then, withn the (gdb) prompt:
    (gdb) run --gdb --port=5554 --socket=/tmp/mysqld.5554 --skip-innodb --debug

    Next, I open several windows for each:

    1. Tail the debug trace: tail -f /tmp/mysqld.trace|grep ha_fed
    2. Tail the SQL calls to the foreign database: tail -f /tmp/mysqld.5555.log
    3. A window with a client open to the federated server on port 5554
    4. A window with a client open to the federated server on port 5555

    I would create a table on the client to the foreign server on port
    5555, and then to the federated server on port 5554. At this point,
    I would run whatever queries I wanted to on the federated server,
    just always remembering that whatever changes I wanted to make on
    the table, or if I created new tables, that I would have to do that
    on the foreign server.

    Another thing to look for is 'show variables' to show you that you have
    support for federated handler support:

    show variables like '%federat%'

    and:

    show storage engines;

    Both should display the federated storage handler.


    Testing
    -------

    There is a test for MySQL Federated Storage Handler in ./mysql-test/t,
    federatedd.test It starts both a slave and master database using
    the same setup that the replication tests use, with the exception that
    it turns off replication, and sets replication to ignore the test tables.
    After ensuring that you actually do have support for the federated storage
    handler, numerous queries/inserts/updates/deletes are run, many derived
    from the MyISAM tests, plus som other tests which were meant to reveal
    any issues that would be most likely to affect this handler. All tests
    should work! ;)

    To run these tests, go into ./mysql-test (based in the directory you
    built the server in)

    ./mysql-test-run federatedd

    To run the test, or if you want to run the test and have debug info:

    ./mysql-test-run --debug federated

    This will run the test in debug mode, and you can view the trace and
    log files in the ./mysql-test/var/log directory

    ls -l mysql-test/var/log/
    -rw-r--r--  1 patg  patg        17  4 Dec 12:27 current_test
    -rw-r--r--  1 patg  patg       692  4 Dec 12:52 manager.log
    -rw-rw----  1 patg  patg     21246  4 Dec 12:51 master-bin.000001
    -rw-rw----  1 patg  patg        68  4 Dec 12:28 master-bin.index
    -rw-r--r--  1 patg  patg      1620  4 Dec 12:51 master.err
    -rw-rw----  1 patg  patg     23179  4 Dec 12:51 master.log
    -rw-rw----  1 patg  patg  16696550  4 Dec 12:51 master.trace
    -rw-r--r--  1 patg  patg         0  4 Dec 12:28 mysqltest-time
    -rw-r--r--  1 patg  patg   2024051  4 Dec 12:51 mysqltest.trace
    -rw-rw----  1 patg  patg     94992  4 Dec 12:51 slave-bin.000001
    -rw-rw----  1 patg  patg        67  4 Dec 12:28 slave-bin.index
    -rw-rw----  1 patg  patg       249  4 Dec 12:52 slave-relay-bin.000003
    -rw-rw----  1 patg  patg        73  4 Dec 12:28 slave-relay-bin.index
    -rw-r--r--  1 patg  patg      1349  4 Dec 12:51 slave.err
    -rw-rw----  1 patg  patg     96206  4 Dec 12:52 slave.log
    -rw-rw----  1 patg  patg  15706355  4 Dec 12:51 slave.trace
    -rw-r--r--  1 patg  patg         0  4 Dec 12:51 warnings

    Of course, again, you can tail the trace log:

    tail -f mysql-test/var/log/master.trace |grep ha_fed

    As well as the slave query log:

    tail -f mysql-test/var/log/slave.log

    Files that comprise the test suit
    ---------------------------------
    mysql-test/t/federated.test
    mysql-test/r/federated.result
    mysql-test/r/have_federated_db.require
    mysql-test/include/have_federated_db.inc


    Other tidbits
    -------------

    These were the files that were modified or created for this
    Federated handler to work:

    ./configure.in
    ./sql/Makefile.am
    ./config/ac_macros/ha_federated.m4
    ./sql/handler.cc
    ./sql/mysqld.cc
    ./sql/set_var.cc
    ./sql/field.h
    ./sql/sql_string.h
    ./mysql-test/mysql-test-run(.sh)
    ./mysql-test/t/federated.test
    ./mysql-test/r/federated.result
    ./mysql-test/r/have_federated_db.require
    ./mysql-test/include/have_federated_db.inc
    ./sql/ha_federated.cc
    ./sql/ha_federated.h

*/


#include "mysql_priv.h"
#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation                          // gcc: Class implementation
#endif

#ifdef WITH_FEDERATED_STORAGE_ENGINE
#include "ha_federated.h"

#include "m_string.h"

#include <mysql/plugin.h>

/* Variables for federated share methods */
static HASH federated_open_tables;              // To track open tables
pthread_mutex_t federated_mutex;                // To init the hash
static int federated_init= FALSE;               // Checking the state of hash

/* Variables used when chopping off trailing characters */
static const uint sizeof_trailing_comma= sizeof(", ") - 1;
static const uint sizeof_trailing_closeparen= sizeof(") ") - 1;
static const uint sizeof_trailing_and= sizeof(" AND ") - 1;
static const uint sizeof_trailing_where= sizeof(" WHERE ") - 1;

/* Static declaration for handerton */
static handler *federated_create_handler(TABLE_SHARE *table,
                                         MEM_ROOT *mem_root);
static int federated_commit(THD *thd, bool all);
static int federated_rollback(THD *thd, bool all);

/* Federated storage engine handlerton */

handlerton federated_hton;

static handler *federated_create_handler(TABLE_SHARE *table,
                                         MEM_ROOT *mem_root)
{
  return new (mem_root) ha_federated(table);
}


/* Function we use in the creation of our hash to get key */

static byte *federated_get_key(FEDERATED_SHARE *share, uint *length,
                               my_bool not_used __attribute__ ((unused)))
{
  *length= share->connect_string_length;
  return (byte*) share->scheme;
}

/*
  Initialize the federated handler.

  SYNOPSIS
    federated_db_init()
    void

  RETURN
    FALSE       OK
    TRUE        Error
*/

int federated_db_init()
{
  DBUG_ENTER("federated_db_init");

  federated_hton.state= SHOW_OPTION_YES;
  federated_hton.db_type= DB_TYPE_FEDERATED_DB;
  federated_hton.commit= federated_commit;
  federated_hton.rollback= federated_rollback;
  federated_hton.create= federated_create_handler;
  federated_hton.panic= federated_db_end;
  federated_hton.flags= HTON_ALTER_NOT_SUPPORTED;

  if (pthread_mutex_init(&federated_mutex, MY_MUTEX_INIT_FAST))
    goto error;
  if (!hash_init(&federated_open_tables, &my_charset_bin, 32, 0, 0,
                    (hash_get_key) federated_get_key, 0, 0))
  {
    federated_init= TRUE;
    DBUG_RETURN(FALSE);
  }

  VOID(pthread_mutex_destroy(&federated_mutex));
error:
  have_federated_db= SHOW_OPTION_DISABLED;	// If we couldn't use handler
  DBUG_RETURN(TRUE);
}


/*
  Release the federated handler.

  SYNOPSIS
    federated_db_end()

  RETURN
    FALSE       OK
*/

int federated_db_end(ha_panic_function type)
{
  if (federated_init)
  {
    hash_free(&federated_open_tables);
    VOID(pthread_mutex_destroy(&federated_mutex));
  }
  federated_init= 0;
  return 0;
}


/*
 Check (in create) whether the tables exists, and that it can be connected to

  SYNOPSIS
    check_foreign_data_source()
      share               pointer to FEDERATED share
      table_create_flag   tells us that ::create is the caller, 
                          therefore, return CANT_CREATE_FEDERATED_TABLE

  DESCRIPTION
    This method first checks that the connection information that parse url
    has populated into the share will be sufficient to connect to the foreign
    table, and if so, does the foreign table exist.
*/

static int check_foreign_data_source(FEDERATED_SHARE *share,
                                     bool table_create_flag)
{
  char escaped_table_name[NAME_LEN*2];
  char query_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char error_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  uint error_code;
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  MYSQL *mysql;
  DBUG_ENTER("ha_federated::check_foreign_data_source");

  /* Zero the length, otherwise the string will have misc chars */
  query.length(0);

  /* error out if we can't alloc memory for mysql_init(NULL) (per Georg) */
  if (!(mysql= mysql_init(NULL)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  /* check if we can connect */
  if (!mysql_real_connect(mysql,
                          share->hostname,
                          share->username,
                          share->password,
                          share->database,
                          share->port,
                          share->socket, 0))
  {
    /*
      we want the correct error message, but it to return
      ER_CANT_CREATE_FEDERATED_TABLE if called by ::create
    */
    error_code= (table_create_flag ?
                 ER_CANT_CREATE_FEDERATED_TABLE :
                 ER_CONNECT_TO_FOREIGN_DATA_SOURCE);

    my_sprintf(error_buffer,
               (error_buffer,
                "database: '%s'  username: '%s'  hostname: '%s'",
                share->database, share->username, share->hostname));

    my_error(ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), error_buffer);
    goto error;
  }
  else
  {
    int escaped_table_name_length= 0;
    /*
      Since we do not support transactions at this version, we can let the 
      client API silently reconnect. For future versions, we will need more 
      logic to deal with transactions
    */
    mysql->reconnect= 1;
    /*
      Note: I am not using INORMATION_SCHEMA because this needs to work with 
      versions prior to 5.0
      
      if we can connect, then make sure the table exists 

      the query will be: SELECT * FROM `tablename` WHERE 1=0
    */
    query.append(STRING_WITH_LEN("SELECT * FROM `"));
    escaped_table_name_length=
      escape_string_for_mysql(&my_charset_bin, (char*)escaped_table_name,
                            sizeof(escaped_table_name),
                            share->table_name,
                            share->table_name_length);
    query.append(escaped_table_name, escaped_table_name_length);
    query.append(STRING_WITH_LEN("` WHERE 1=0"));

    if (mysql_real_query(mysql, query.ptr(), query.length()))
    {
      error_code= table_create_flag ?
        ER_CANT_CREATE_FEDERATED_TABLE : ER_FOREIGN_DATA_SOURCE_DOESNT_EXIST;
      my_sprintf(error_buffer, (error_buffer, "error: %d  '%s'",
                                mysql_errno(mysql), mysql_error(mysql)));

      my_error(error_code, MYF(0), error_buffer);
      goto error;
    }
  }
  error_code=0;

error:
    mysql_close(mysql);
    DBUG_RETURN(error_code);
}


static int parse_url_error(FEDERATED_SHARE *share, TABLE *table, int error_num)
{
  char buf[FEDERATED_QUERY_BUFFER_SIZE];
  int buf_len;
  DBUG_ENTER("ha_federated parse_url_error");

  if (share->scheme)
  {
    DBUG_PRINT("info",
               ("error: parse_url. Returning error code %d \
                freeing share->scheme %lx", error_num, share->scheme));
    my_free((gptr) share->scheme, MYF(0));
    share->scheme= 0;
  }
  buf_len= min(table->s->connect_string.length,
               FEDERATED_QUERY_BUFFER_SIZE-1);
  strmake(buf, table->s->connect_string.str, buf_len);
  my_error(error_num, MYF(0), buf);
  DBUG_RETURN(error_num);
}

/*
  Parse connection info from table->s->connect_string

  SYNOPSIS
    parse_url()
    share               pointer to FEDERATED share
    table               pointer to current TABLE class
    table_create_flag   determines what error to throw

  DESCRIPTION
    Populates the share with information about the connection
    to the foreign database that will serve as the data source.
    This string must be specified (currently) in the "comment" field,
    listed in the CREATE TABLE statement.

    This string MUST be in the format of any of these:

    scheme://username:password@hostname:port/database/table
    scheme://username@hostname/database/table
    scheme://username@hostname:port/database/table
    scheme://username:password@hostname/database/table

  An Example:

  mysql://joe:joespass@192.168.1.111:9308/federated/testtable

  ***IMPORTANT***
  Currently, only "mysql://" is supported.

  'password' and 'port' are both optional.

  RETURN VALUE
    0           success
    error_num   particular error code 

*/

static int parse_url(FEDERATED_SHARE *share, TABLE *table,
                     uint table_create_flag)
{
  uint error_num= (table_create_flag ?
                   ER_FOREIGN_DATA_STRING_INVALID_CANT_CREATE :
                   ER_FOREIGN_DATA_STRING_INVALID);
  DBUG_ENTER("ha_federated::parse_url");

  share->port= 0;
  share->socket= 0;
  DBUG_PRINT("info", ("Length: %d", table->s->connect_string.length));
  DBUG_PRINT("info", ("String: '%.*s'", table->s->connect_string.length, 
                      table->s->connect_string.str));
  share->scheme= my_strndup(table->s->connect_string.str,
                            table->s->connect_string.length,
                            MYF(0));

  share->connect_string_length= table->s->connect_string.length;
  DBUG_PRINT("info",("parse_url alloced share->scheme %lx", share->scheme));

  /*
    remove addition of null terminator and store length
    for each string  in share
  */
  if (!(share->username= strstr(share->scheme, "://")))
    goto error;
  share->scheme[share->username - share->scheme]= '\0';

  if (strcmp(share->scheme, "mysql") != 0)
    goto error;

  share->username+= 3;

  if (!(share->hostname= strchr(share->username, '@')))
    goto error;
    
  share->username[share->hostname - share->username]= '\0';
  share->hostname++;

  if ((share->password= strchr(share->username, ':')))
  {
    share->username[share->password - share->username]= '\0';
    share->password++;
    share->username= share->username;
    /* make sure there isn't an extra / or @ */
    if ((strchr(share->password, '/') || strchr(share->hostname, '@')))
      goto error;
    /*
      Found that if the string is:
      user:@hostname:port/database/table
      Then password is a null string, so set to NULL
    */
    if ((share->password[0] == '\0'))
      share->password= NULL;
  }
  else
    share->username= share->username;

  /* make sure there isn't an extra / or @ */
  if ((strchr(share->username, '/')) || (strchr(share->hostname, '@')))
    goto error;

  if (!(share->database= strchr(share->hostname, '/')))
    goto error;
  share->hostname[share->database - share->hostname]= '\0';
  share->database++;

  if ((share->sport= strchr(share->hostname, ':')))
  {
    share->hostname[share->sport - share->hostname]= '\0';
    share->sport++;
    if (share->sport[0] == '\0')
      share->sport= NULL;
    else
      share->port= atoi(share->sport);
  }

  if (!(share->table_name= strchr(share->database, '/')))
    goto error;
  share->database[share->table_name - share->database]= '\0';
  share->table_name++;

  share->table_name_length= strlen(share->table_name);
 
  /* make sure there's not an extra / */
  if ((strchr(share->table_name, '/')))
    goto error;

  if (share->hostname[0] == '\0')
    share->hostname= NULL;

  if (!share->port)
  {
    if (strcmp(share->hostname, my_localhost) == 0)
      share->socket= my_strdup(MYSQL_UNIX_ADDR, MYF(0));
    else
      share->port= MYSQL_PORT;
  }

  DBUG_PRINT("info",
             ("scheme: %s  username: %s  password: %s \
               hostname: %s  port: %d  database: %s  tablename: %s",
              share->scheme, share->username, share->password,
              share->hostname, share->port, share->database,
              share->table_name));

  DBUG_RETURN(0);

error:
  DBUG_RETURN(parse_url_error(share, table, error_num));
}


/*****************************************************************************
** FEDERATED tables
*****************************************************************************/

ha_federated::ha_federated(TABLE_SHARE *table_arg)
  :handler(&federated_hton, table_arg),
  mysql(0), stored_result(0)
{
  trx_next= 0;
}


/*
  Convert MySQL result set row to handler internal format

  SYNOPSIS
    convert_row_to_internal_format()
      record    Byte pointer to record
      row       MySQL result set row from fetchrow()
      result	Result set to use

  DESCRIPTION
    This method simply iterates through a row returned via fetchrow with
    values from a successful SELECT , and then stores each column's value
    in the field object via the field object pointer (pointing to the table's
    array of field object pointers). This is how the handler needs the data
    to be stored to then return results back to the user

  RETURN VALUE
    0   After fields have had field values stored from record
*/

uint ha_federated::convert_row_to_internal_format(byte *record,
                                                  MYSQL_ROW row,
                                                  MYSQL_RES *result)
{
  ulong *lengths;
  Field **field;
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);
  DBUG_ENTER("ha_federated::convert_row_to_internal_format");

  lengths= mysql_fetch_lengths(result);

  for (field= table->field; *field; field++, row++, lengths++)
  {
    /*
      index variable to move us through the row at the
      same iterative step as the field
    */
    my_ptrdiff_t old_ptr;
    old_ptr= (my_ptrdiff_t) (record - table->record[0]);
    (*field)->move_field_offset(old_ptr);
    if (!*row)
      (*field)->set_null();
    else
    {
      if (bitmap_is_set(table->read_set, (*field)->field_index))
      {
        (*field)->set_notnull();
        (*field)->store(*row, *lengths, &my_charset_bin);
      }
    }
    (*field)->move_field_offset(-old_ptr);
  }
  dbug_tmp_restore_column_map(table->write_set, old_map);
  DBUG_RETURN(0);
}

static bool emit_key_part_name(String *to, KEY_PART_INFO *part)
{
  DBUG_ENTER("emit_key_part_name");
  if (to->append(STRING_WITH_LEN("`")) ||
      to->append(part->field->field_name) ||
      to->append(STRING_WITH_LEN("`")))
    DBUG_RETURN(1);                           // Out of memory
  DBUG_RETURN(0);
}

static bool emit_key_part_element(String *to, KEY_PART_INFO *part,
                                  bool needs_quotes, bool is_like,
                                  const byte *ptr, uint len)
{
  Field *field= part->field;
  DBUG_ENTER("emit_key_part_element");

  if (needs_quotes && to->append(STRING_WITH_LEN("'")))
    DBUG_RETURN(1);

  if (part->type == HA_KEYTYPE_BIT)
  {
    char buff[STRING_BUFFER_USUAL_SIZE], *buf= buff;

    *buf++= '0';
    *buf++= 'x';
    buf= octet2hex(buf, (char*) ptr, len);
    if (to->append((char*) buff, (uint)(buf - buff)))
      DBUG_RETURN(1);
  }
  else if (part->key_part_flag & HA_BLOB_PART)
  {
    String blob;
    uint blob_length= uint2korr(ptr);
    blob.set_quick((char*) ptr+HA_KEY_BLOB_LENGTH,
                   blob_length, &my_charset_bin);
    if (append_escaped(to, &blob))
      DBUG_RETURN(1);
  }
  else if (part->key_part_flag & HA_VAR_LENGTH_PART)
  {
    String varchar;
    uint var_length= uint2korr(ptr);
    varchar.set_quick((char*) ptr+HA_KEY_BLOB_LENGTH,
                      var_length, &my_charset_bin);
    if (append_escaped(to, &varchar))
      DBUG_RETURN(1);
  }
  else
  {
    char strbuff[MAX_FIELD_WIDTH];
    String str(strbuff, sizeof(strbuff), part->field->charset()), *res;

    res= field->val_str(&str, (char *)ptr);

    if (field->result_type() == STRING_RESULT)
    {
      if (append_escaped(to, res))
        DBUG_RETURN(1);
    }
    else if (to->append(res->ptr(), res->length()))
      DBUG_RETURN(1);
  }

  if (is_like && to->append(STRING_WITH_LEN("%")))
    DBUG_RETURN(1);

  if (needs_quotes && to->append(STRING_WITH_LEN("'")))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}

/*
  Create a WHERE clause based off of values in keys
  Note: This code was inspired by key_copy from key.cc

  SYNOPSIS
    create_where_from_key ()
      to          String object to store WHERE clause
      key_info    KEY struct pointer
      key         byte pointer containing key
      key_length  length of key
      range_type  0 - no range, 1 - min range, 2 - max range
                  (see enum range_operation)

  DESCRIPTION
    Using iteration through all the keys via a KEY_PART_INFO pointer,
    This method 'extracts' the value of each key in the byte pointer
    *key, and for each key found, constructs an appropriate WHERE clause

  RETURN VALUE
    0   After all keys have been accounted for to create the WHERE clause
    1   No keys found

    Range flags Table per Timour:

   -----------------
   - start_key:
     * ">"  -> HA_READ_AFTER_KEY
     * ">=" -> HA_READ_KEY_OR_NEXT
     * "="  -> HA_READ_KEY_EXACT

   - end_key:
     * "<"  -> HA_READ_BEFORE_KEY
     * "<=" -> HA_READ_AFTER_KEY

   records_in_range:
   -----------------
   - start_key:
     * ">"  -> HA_READ_AFTER_KEY
     * ">=" -> HA_READ_KEY_EXACT
     * "="  -> HA_READ_KEY_EXACT

   - end_key:
     * "<"  -> HA_READ_BEFORE_KEY
     * "<=" -> HA_READ_AFTER_KEY
     * "="  -> HA_READ_AFTER_KEY

0 HA_READ_KEY_EXACT,              Find first record else error
1 HA_READ_KEY_OR_NEXT,            Record or next record
2 HA_READ_KEY_OR_PREV,            Record or previous
3 HA_READ_AFTER_KEY,              Find next rec. after key-record
4 HA_READ_BEFORE_KEY,             Find next rec. before key-record
5 HA_READ_PREFIX,                 Key which as same prefix
6 HA_READ_PREFIX_LAST,            Last key with the same prefix
7 HA_READ_PREFIX_LAST_OR_PREV,    Last or prev key with the same prefix

Flags that I've found:

id, primary key, varchar

id = 'ccccc'
records_in_range: start_key 0 end_key 3
read_range_first: start_key 0 end_key NULL

id > 'ccccc'
records_in_range: start_key 3 end_key NULL
read_range_first: start_key 3 end_key NULL

id < 'ccccc'
records_in_range: start_key NULL end_key 4
read_range_first: start_key NULL end_key 4

id <= 'ccccc'
records_in_range: start_key NULL end_key 3
read_range_first: start_key NULL end_key 3

id >= 'ccccc'
records_in_range: start_key 0 end_key NULL
read_range_first: start_key 1 end_key NULL

id like 'cc%cc'
records_in_range: start_key 0 end_key 3
read_range_first: start_key 1 end_key 3

id > 'aaaaa' and id < 'ccccc'
records_in_range: start_key 3 end_key 4
read_range_first: start_key 3 end_key 4

id >= 'aaaaa' and id < 'ccccc';
records_in_range: start_key 0 end_key 4
read_range_first: start_key 1 end_key 4

id >= 'aaaaa' and id <= 'ccccc';
records_in_range: start_key 0 end_key 3
read_range_first: start_key 1 end_key 3

id > 'aaaaa' and id <= 'ccccc';
records_in_range: start_key 3 end_key 3
read_range_first: start_key 3 end_key 3

numeric keys:

id = 4
index_read_idx: start_key 0 end_key NULL 

id > 4
records_in_range: start_key 3 end_key NULL
read_range_first: start_key 3 end_key NULL

id >= 4
records_in_range: start_key 0 end_key NULL
read_range_first: start_key 1 end_key NULL

id < 4
records_in_range: start_key NULL end_key 4
read_range_first: start_key NULL end_key 4

id <= 4
records_in_range: start_key NULL end_key 3
read_range_first: start_key NULL end_key 3

id like 4
full table scan, select * from

id > 2 and id < 8
records_in_range: start_key 3 end_key 4
read_range_first: start_key 3 end_key 4

id >= 2 and id < 8
records_in_range: start_key 0 end_key 4
read_range_first: start_key 1 end_key 4

id >= 2 and id <= 8
records_in_range: start_key 0 end_key 3
read_range_first: start_key 1 end_key 3

id > 2 and id <= 8
records_in_range: start_key 3 end_key 3
read_range_first: start_key 3 end_key 3

multi keys (id int, name varchar, other varchar)

id = 1;
records_in_range: start_key 0 end_key 3
read_range_first: start_key 0 end_key NULL

id > 4;
id > 2 and name = '333'; remote: id > 2
id > 2 and name > '333'; remote: id > 2
id > 2 and name > '333' and other < 'ddd'; remote: id > 2 no results
id > 2 and name >= '333' and other < 'ddd'; remote: id > 2 1 result
id >= 4 and name = 'eric was here' and other > 'eeee';
records_in_range: start_key 3 end_key NULL
read_range_first: start_key 3 end_key NULL

id >= 4;
id >= 2 and name = '333' and other < 'ddd';
remote: `id`  >= 2 AND `name`  >= '333';
records_in_range: start_key 0 end_key NULL
read_range_first: start_key 1 end_key NULL

id < 4;
id < 3 and name = '222' and other <= 'ccc'; remote: id < 3
records_in_range: start_key NULL end_key 4
read_range_first: start_key NULL end_key 4

id <= 4;
records_in_range: start_key NULL end_key 3
read_range_first: start_key NULL end_key 3

id like 4;
full table scan

id  > 2 and id < 4;
records_in_range: start_key 3 end_key 4
read_range_first: start_key 3 end_key 4

id >= 2 and id < 4;
records_in_range: start_key 0 end_key 4
read_range_first: start_key 1 end_key 4

id >= 2 and id <= 4;
records_in_range: start_key 0 end_key 3
read_range_first: start_key 1 end_key 3

id > 2 and id <= 4;
id = 6 and name = 'eric was here' and other > 'eeee';
remote: (`id`  > 6 AND `name`  > 'eric was here' AND `other`  > 'eeee')
AND (`id`  <= 6) AND ( AND `name`  <= 'eric was here')
no results
records_in_range: start_key 3 end_key 3
read_range_first: start_key 3 end_key 3

Summary:

* If the start key flag is 0 the max key flag shouldn't even be set, 
  and if it is, the query produced would be invalid.
* Multipart keys, even if containing some or all numeric columns,
  are treated the same as non-numeric keys

  If the query is " = " (quotes or not):
  - records in range start key flag HA_READ_KEY_EXACT,
    end key flag HA_READ_AFTER_KEY (incorrect)
  - any other: start key flag HA_READ_KEY_OR_NEXT,
    end key flag HA_READ_AFTER_KEY (correct)

* 'like' queries (of key)
  - Numeric, full table scan
  - Non-numeric
      records_in_range: start_key 0 end_key 3
      other : start_key 1 end_key 3

* If the key flag is HA_READ_AFTER_KEY:
   if start_key, append >
   if end_key, append <=

* If create_where_key was called by records_in_range:

 - if the key is numeric:
    start key flag is 0 when end key is NULL, end key flag is 3 or 4
 - if create_where_key was called by any other function:
    start key flag is 1 when end key is NULL, end key flag is 3 or 4
 - if the key is non-numeric, or multipart
    When the query is an exact match, the start key flag is 0,
    end key flag is 3 for what should be a no-range condition where
    you should have 0 and max key NULL, which it is if called by
    read_range_first

Conclusion:

1. Need logic to determin if a key is min or max when the flag is
HA_READ_AFTER_KEY, and handle appending correct operator accordingly

2. Need a boolean flag to pass to create_where_from_key, used in the
switch statement. Add 1 to the flag if:
  - start key flag is HA_READ_KEY_EXACT and the end key is NULL

*/

bool ha_federated::create_where_from_key(String *to,
                                         KEY *key_info,
                                         const key_range *start_key,
                                         const key_range *end_key,
                                         bool records_in_range,
                                         bool eq_range)
{
  bool both_not_null=
    (start_key != NULL && end_key != NULL) ? TRUE : FALSE;
  const byte *ptr;
  uint remainder, length;
  char tmpbuff[FEDERATED_QUERY_BUFFER_SIZE];
  String tmp(tmpbuff, sizeof(tmpbuff), system_charset_info);
  const key_range *ranges[2]= { start_key, end_key };
  my_bitmap_map *old_map;
  DBUG_ENTER("ha_federated::create_where_from_key");

  tmp.length(0); 
  if (start_key == NULL && end_key == NULL)
    DBUG_RETURN(1);

  old_map= dbug_tmp_use_all_columns(table, table->write_set);
  for (uint i= 0; i <= 1; i++)
  {
    bool needs_quotes;
    KEY_PART_INFO *key_part;
    if (ranges[i] == NULL)
      continue;

    if (both_not_null)
    {
      if (i > 0)
        tmp.append(STRING_WITH_LEN(") AND ("));
      else
        tmp.append(STRING_WITH_LEN(" ("));
    }

    for (key_part= key_info->key_part,
         remainder= key_info->key_parts,
         length= ranges[i]->length,
         ptr= ranges[i]->key; ;
         remainder--,
         key_part++)
    {
      Field *field= key_part->field;
      uint store_length= key_part->store_length;
      uint part_length= min(store_length, length);
      needs_quotes= 1;
      DBUG_DUMP("key, start of loop", (char *) ptr, length);

      if (key_part->null_bit)
      {
        if (*ptr++)
        {
          if (emit_key_part_name(&tmp, key_part) ||
              tmp.append(STRING_WITH_LEN(" IS NULL ")))
            goto err;
          continue;
        }
      }

      if (tmp.append(STRING_WITH_LEN(" (")))
        goto err;

      switch (ranges[i]->flag) {
      case HA_READ_KEY_EXACT:
        DBUG_PRINT("info", ("federated HA_READ_KEY_EXACT %d", i));
        if (store_length >= length ||
            !needs_quotes ||
            key_part->type == HA_KEYTYPE_BIT ||
            field->result_type() != STRING_RESULT)
        {
          if (emit_key_part_name(&tmp, key_part))
            goto err;

          if (records_in_range)
          {
            if (tmp.append(STRING_WITH_LEN(" >= ")))
              goto err;
          }
          else
          {
            if (tmp.append(STRING_WITH_LEN(" = ")))
              goto err;
          }

          if (emit_key_part_element(&tmp, key_part, needs_quotes, 0, ptr,
                                    part_length))
            goto err;
        }
        else
        {
          /* LIKE */
          if (emit_key_part_name(&tmp, key_part) ||
              tmp.append(STRING_WITH_LEN(" LIKE ")) ||
              emit_key_part_element(&tmp, key_part, needs_quotes, 1, ptr,
                                    part_length))
            goto err;
        }
        break;
      case HA_READ_AFTER_KEY:
        if (eq_range)
        {
          if (tmp.append("1=1"))                // Dummy
            goto err;
          break;
        }
        DBUG_PRINT("info", ("federated HA_READ_AFTER_KEY %d", i));
        if (store_length >= length) /* end key */
        {
          if (emit_key_part_name(&tmp, key_part))
            goto err;

          if (i > 0) /* end key */
          {
            if (tmp.append(STRING_WITH_LEN(" <= ")))
              goto err;
          }
          else /* start key */
          {
            if (tmp.append(STRING_WITH_LEN(" > ")))
              goto err;
          }

          if (emit_key_part_element(&tmp, key_part, needs_quotes, 0, ptr,
                                    part_length))
          {
            goto err;
          }
          break;
        }
      case HA_READ_KEY_OR_NEXT:
        DBUG_PRINT("info", ("federated HA_READ_KEY_OR_NEXT %d", i));
        if (emit_key_part_name(&tmp, key_part) ||
            tmp.append(STRING_WITH_LEN(" >= ")) ||
            emit_key_part_element(&tmp, key_part, needs_quotes, 0, ptr,
              part_length))
          goto err;
        break;
      case HA_READ_BEFORE_KEY:
        DBUG_PRINT("info", ("federated HA_READ_BEFORE_KEY %d", i));
        if (store_length >= length)
        {
          if (emit_key_part_name(&tmp, key_part) ||
              tmp.append(STRING_WITH_LEN(" < ")) ||
              emit_key_part_element(&tmp, key_part, needs_quotes, 0, ptr,
                                    part_length))
            goto err;
          break;
        }
      case HA_READ_KEY_OR_PREV:
        DBUG_PRINT("info", ("federated HA_READ_KEY_OR_PREV %d", i));
        if (emit_key_part_name(&tmp, key_part) ||
            tmp.append(STRING_WITH_LEN(" <= ")) ||
            emit_key_part_element(&tmp, key_part, needs_quotes, 0, ptr,
                                  part_length))
          goto err;
        break;
      default:
        DBUG_PRINT("info",("cannot handle flag %d", ranges[i]->flag));
        goto err;
      }
      if (tmp.append(STRING_WITH_LEN(") ")))
        goto err;

next_loop:
      if (store_length >= length)
        break;
      DBUG_PRINT("info", ("remainder %d", remainder));
      DBUG_ASSERT(remainder > 1);
      length-= store_length;
      ptr+= store_length;
      if (tmp.append(STRING_WITH_LEN(" AND ")))
        goto err;

      DBUG_PRINT("info",
                 ("create_where_from_key WHERE clause: %s",
                  tmp.c_ptr_quick()));
    }
  }
  dbug_tmp_restore_column_map(table->write_set, old_map);

  if (both_not_null)
    if (tmp.append(STRING_WITH_LEN(") ")))
      DBUG_RETURN(1);

  if (to->append(STRING_WITH_LEN(" WHERE ")))
    DBUG_RETURN(1);

  if (to->append(tmp))
    DBUG_RETURN(1);

  DBUG_RETURN(0);

err:
  dbug_tmp_restore_column_map(table->write_set, old_map);
  DBUG_RETURN(1);
}

/*
  Example of simple lock controls. The "share" it creates is structure we will
  pass to each federated handler. Do you have to have one of these? Well, you
  have pieces that are used for locking, and they are needed to function.
*/

static FEDERATED_SHARE *get_share(const char *table_name, TABLE *table)
{
  char *select_query;
  char query_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  Field **field;
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  FEDERATED_SHARE *share= NULL, tmp_share;
  /*
    In order to use this string, we must first zero it's length,
    or it will contain garbage
  */
  query.length(0);

  pthread_mutex_lock(&federated_mutex);

  if (parse_url(&tmp_share, table, 0))
    goto error;

  /* TODO: change tmp_share.scheme to LEX_STRING object */
  if (!(share= (FEDERATED_SHARE *) hash_search(&federated_open_tables,
                                               (byte*) tmp_share.scheme,
                                               tmp_share.
                                               connect_string_length)))
  {
    query.set_charset(system_charset_info);
    query.append(STRING_WITH_LEN("SELECT "));
    for (field= table->field; *field; field++)
    {
      query.append(STRING_WITH_LEN("`"));
      query.append((*field)->field_name);
      query.append(STRING_WITH_LEN("`, "));
    }
    /* chops off trailing comma */
    query.length(query.length() - sizeof_trailing_comma);

    query.append(STRING_WITH_LEN(" FROM `"));

    if (!(share= (FEDERATED_SHARE *)
          my_multi_malloc(MYF(MY_WME),
                          &share, sizeof(*share),
                          &select_query,
                          query.length()+table->s->connect_string.length+1,
                          NullS)))
      goto error;

    memcpy(share, &tmp_share, sizeof(tmp_share));

    share->table_name_length= strlen(share->table_name);
    /* TODO: share->table_name to LEX_STRING object */
    query.append(share->table_name, share->table_name_length);
    query.append(STRING_WITH_LEN("`"));
    share->select_query= select_query;
    strmov(share->select_query, query.ptr());
    share->use_count= 0;
    DBUG_PRINT("info",
               ("share->select_query %s", share->select_query));

    if (my_hash_insert(&federated_open_tables, (byte*) share))
      goto error;
    thr_lock_init(&share->lock);
    pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
  }
  share->use_count++;
  pthread_mutex_unlock(&federated_mutex);

  return share;

error:
  pthread_mutex_unlock(&federated_mutex);
  my_free((gptr) tmp_share.scheme, MYF(MY_ALLOW_ZERO_PTR));
  my_free((gptr) share, MYF(MY_ALLOW_ZERO_PTR));
  return NULL;
}


/*
  Free lock controls. We call this whenever we close a table.
  If the table had the last reference to the share then we
  free memory associated with it.
*/

static int free_share(FEDERATED_SHARE *share)
{
  DBUG_ENTER("free_share");

  pthread_mutex_lock(&federated_mutex);
  if (!--share->use_count)
  {
    hash_delete(&federated_open_tables, (byte*) share);
    my_free((gptr) share->scheme, MYF(MY_ALLOW_ZERO_PTR));
    my_free((gptr) share->socket, MYF(MY_ALLOW_ZERO_PTR));
    thr_lock_delete(&share->lock);
    VOID(pthread_mutex_destroy(&share->mutex));
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&federated_mutex);

  DBUG_RETURN(0);
}


ha_rows ha_federated::records_in_range(uint inx, key_range *start_key,
                                   key_range *end_key)
{
  /*

  We really want indexes to be used as often as possible, therefore
  we just need to hard-code the return value to a very low number to
  force the issue

*/
  DBUG_ENTER("ha_federated::records_in_range");
  DBUG_RETURN(FEDERATED_RECORDS_IN_RANGE);
}
/*
  If frm_error() is called then we will use this to to find out
  what file extentions exist for the storage engine. This is
  also used by the default rename_table and delete_table method
  in handler.cc.
*/

const char **ha_federated::bas_ext() const
{
  static const char *ext[]=
  {
    NullS
  };
  return ext;
}


/*
  Used for opening tables. The name will be the name of the file.
  A table is opened when it needs to be opened. For instance
  when a request comes in for a select on the table (tables are not
  open and closed for each request, they are cached).

  Called from handler.cc by handler::ha_open(). The server opens
  all tables by calling ha_open() which then calls the handler
  specific open().
*/

int ha_federated::open(const char *name, int mode, uint test_if_locked)
{
  DBUG_ENTER("ha_federated::open");

  if (!(share= get_share(name, table)))
    DBUG_RETURN(1);
  thr_lock_data_init(&share->lock, &lock, NULL);

  /* Connect to foreign database mysql_real_connect() */
  mysql= mysql_init(0);
  if (!mysql || !mysql_real_connect(mysql,
                                   share->hostname,
                                   share->username,
                                   share->password,
                                   share->database,
                                   share->port,
                                   share->socket, 0))
  {
    free_share(share);
    DBUG_RETURN(stash_remote_error());
  }
  /*
    Since we do not support transactions at this version, we can let the client
    API silently reconnect. For future versions, we will need more logic to
    deal with transactions
  */
  mysql->reconnect= 1;

  ref_length= (table->s->primary_key != MAX_KEY ?
               table->key_info[table->s->primary_key].key_length :
               table->s->reclength);
  DBUG_PRINT("info", ("ref_length: %u", ref_length));

  DBUG_RETURN(0);
}


/*
  Closes a table. We call the free_share() function to free any resources
  that we have allocated in the "shared" structure.

  Called from sql_base.cc, sql_select.cc, and table.cc.
  In sql_select.cc it is only used to close up temporary tables or during
  the process where a temporary table is converted over to being a
  myisam table.
  For sql_base.cc look at close_data_tables().
*/

int ha_federated::close(void)
{
  int retval;
  DBUG_ENTER("ha_federated::close");

  /* free the result set */
  if (stored_result)
  {
    mysql_free_result(stored_result);
    stored_result= 0;
  }
  /* Disconnect from mysql */
  if (mysql)                                    // QQ is this really needed
    mysql_close(mysql);
  retval= free_share(share);
  DBUG_RETURN(retval);

}

/*

  Checks if a field in a record is SQL NULL.

  SYNOPSIS
    field_in_record_is_null()
      table     TABLE pointer, MySQL table object
      field     Field pointer, MySQL field object
      record    char pointer, contains record

    DESCRIPTION
      This method uses the record format information in table to track
      the null bit in record.

    RETURN VALUE
      1    if NULL
      0    otherwise
*/

inline uint field_in_record_is_null(TABLE *table,
                                    Field *field,
                                    char *record)
{
  int null_offset;
  DBUG_ENTER("ha_federated::field_in_record_is_null");

  if (!field->null_ptr)
    DBUG_RETURN(0);

  null_offset= (uint) ((char*)field->null_ptr - (char*)table->record[0]);

  if (record[null_offset] & field->null_bit)
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


/*
  write_row() inserts a row. No extra() hint is given currently if a bulk load
  is happeneding. buf() is a byte array of data. You can use the field
  information to extract the data from the native byte array type.
  Example of this would be:
  for (Field **field=table->field ; *field ; field++)
  {
    ...
  }

  Called from item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
  sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc, and sql_update.cc.
*/

int ha_federated::write_row(byte *buf)
{
  /*
    I need a bool again, in 5.0, I used table->s->fields to accomplish this.
    This worked as a flag that says there are fields with values or not.
    In 5.1, this value doesn't work the same, and I end up with the code
    truncating open parenthesis:

    the statement "INSERT INTO t1 VALUES ()" ends up being first built
    in two strings
      "INSERT INTO t1 ("
      and
      " VALUES ("

    If there are fields with values, they get appended, with commas, and 
    the last loop, a trailing comma is there

    "INSERT INTO t1 ( col1, col2, colN, "

    " VALUES ( 'val1', 'val2', 'valN', "

    Then, if there are fields, it should decrement the string by ", " length.

    "INSERT INTO t1 ( col1, col2, colN"
    " VALUES ( 'val1', 'val2', 'valN'"

    Then it adds a close paren to both - if there are fields

    "INSERT INTO t1 ( col1, col2, colN)"
    " VALUES ( 'val1', 'val2', 'valN')"

    Then appends both together
    "INSERT INTO t1 ( col1, col2, colN) VALUES ( 'val1', 'val2', 'valN')"

    So... the problem, is if you have the original statement:

    "INSERT INTO t1 VALUES ()"

    Which is legitimate, but if the code thinks there are fields

    "INSERT INTO t1 ("
    " VALUES ( "

    If the field flag is set, but there are no commas, reduces the 
    string by strlen(", ")

    "INSERT INTO t1 "
    " VALUES "

    Then adds the close parenthesis

    "INSERT INTO t1  )"
    " VALUES  )"

    So, I have to use a bool as before, set in the loop where fields and commas
    are appended to the string
  */
  my_bool commas_added= FALSE;
  char insert_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char values_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char insert_field_value_buffer[STRING_BUFFER_USUAL_SIZE];
  Field **field;

  /* The main insert query string */
  String insert_string(insert_buffer, sizeof(insert_buffer), &my_charset_bin);
  /* The string containing the values to be added to the insert */
  String values_string(values_buffer, sizeof(values_buffer), &my_charset_bin);
  /* The actual value of the field, to be added to the values_string */
  String insert_field_value_string(insert_field_value_buffer,
                                   sizeof(insert_field_value_buffer),
                                   &my_charset_bin);
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
  DBUG_ENTER("ha_federated::write_row");

  values_string.length(0);
  insert_string.length(0);
  insert_field_value_string.length(0);
  statistic_increment(table->in_use->status_var.ha_write_count, &LOCK_status);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();

  /*
    start both our field and field values strings
  */
  insert_string.append(STRING_WITH_LEN("INSERT INTO `"));
  insert_string.append(share->table_name, share->table_name_length);
  insert_string.append('`');
  insert_string.append(STRING_WITH_LEN(" ("));

  values_string.append(STRING_WITH_LEN(" VALUES "));
  values_string.append(STRING_WITH_LEN(" ("));

  /*
    loop through the field pointer array, add any fields to both the values
    list and the fields list that is part of the write set
  */
  for (field= table->field; *field; field++)
  {
    if (bitmap_is_set(table->write_set, (*field)->field_index))
    {
      commas_added= TRUE;
      if ((*field)->is_null())
        insert_field_value_string.append(STRING_WITH_LEN(" NULL "));
      else
      {
        (*field)->val_str(&insert_field_value_string);
        values_string.append('\'');
        insert_field_value_string.print(&values_string);
        values_string.append('\'');

        insert_field_value_string.length(0);
      }
      /* append the field name */
      insert_string.append((*field)->field_name);

      /* append the value */
      values_string.append(insert_field_value_string);
      insert_field_value_string.length(0);

      /* append commas between both fields and fieldnames */
      /*
        unfortunately, we can't use the logic if *(fields + 1) to
        make the following appends conditional as we don't know if the
        next field is in the write set
      */
      insert_string.append(STRING_WITH_LEN(", "));
      values_string.append(STRING_WITH_LEN(", "));
    }
  }
  dbug_tmp_restore_column_map(table->read_set, old_map);

  /*
    if there were no fields, we don't want to add a closing paren
    AND, we don't want to chop off the last char '('
    insert will be "INSERT INTO t1 VALUES ();"
  */
  if (commas_added)
  {
    insert_string.length(insert_string.length() - sizeof_trailing_comma);
    /* chops off leading commas */
    values_string.length(values_string.length() - sizeof_trailing_comma);
    insert_string.append(STRING_WITH_LEN(") "));
  }
  else
  {
    /* chops off trailing ) */
    insert_string.length(insert_string.length() - sizeof_trailing_closeparen);
  }

  /* we always want to append this, even if there aren't any fields */
  values_string.append(STRING_WITH_LEN(") "));

  /* add the values */
  insert_string.append(values_string);

  if (mysql_real_query(mysql, insert_string.ptr(), insert_string.length()))
  {
    DBUG_RETURN(stash_remote_error());
  }
  /*
    If the table we've just written a record to contains an auto_increment
    field, then store the last_insert_id() value from the foreign server
  */
  if (table->next_number_field)
    update_auto_increment();

  DBUG_RETURN(0);
}

/*
  ha_federated::update_auto_increment

  This method ensures that last_insert_id() works properly. What it simply does
  is calls last_insert_id() on the foreign database immediately after insert
  (if the table has an auto_increment field) and sets the insert id via
  thd->insert_id(ID)).
*/
void ha_federated::update_auto_increment(void)
{
  THD *thd= current_thd;
  DBUG_ENTER("ha_federated::update_auto_increment");

  thd->first_successful_insert_id_in_cur_stmt= 
    mysql->last_used_con->insert_id;
  DBUG_PRINT("info",("last_insert_id %d", stats.auto_increment_value));

  DBUG_VOID_RETURN;
}

int ha_federated::optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  char query_buffer[STRING_BUFFER_USUAL_SIZE];
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  DBUG_ENTER("ha_federated::optimize");
  
  query.length(0);

  query.set_charset(system_charset_info);
  query.append(STRING_WITH_LEN("OPTIMIZE TABLE `"));
  query.append(share->table_name, share->table_name_length);
  query.append(STRING_WITH_LEN("`"));

  if (mysql_real_query(mysql, query.ptr(), query.length()))
  {
    DBUG_RETURN(stash_remote_error());
  }

  DBUG_RETURN(0);
}


int ha_federated::repair(THD* thd, HA_CHECK_OPT* check_opt)
{
  char query_buffer[STRING_BUFFER_USUAL_SIZE];
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  DBUG_ENTER("ha_federated::repair");

  query.length(0);

  query.set_charset(system_charset_info);
  query.append(STRING_WITH_LEN("REPAIR TABLE `"));
  query.append(share->table_name, share->table_name_length);
  query.append(STRING_WITH_LEN("`"));
  if (check_opt->flags & T_QUICK)
    query.append(STRING_WITH_LEN(" QUICK"));
  if (check_opt->flags & T_EXTEND)
    query.append(STRING_WITH_LEN(" EXTENDED"));
  if (check_opt->sql_flags & TT_USEFRM)
    query.append(STRING_WITH_LEN(" USE_FRM"));

  if (mysql_real_query(mysql, query.ptr(), query.length()))
  {
    DBUG_RETURN(stash_remote_error());
  }

  DBUG_RETURN(0);
}


/*
  Yes, update_row() does what you expect, it updates a row. old_data will have
  the previous row record in it, while new_data will have the newest data in
  it.

  Keep in mind that the server can do updates based on ordering if an ORDER BY
  clause was used. Consecutive ordering is not guaranteed.
  Currently new_data will not have an updated auto_increament record, or
  and updated timestamp field. You can do these for federated by doing these:
  if (table->timestamp_on_update_now)
    update_timestamp(new_row+table->timestamp_on_update_now-1);
  if (table->next_number_field && record == table->record[0])
    update_auto_increment();

  Called from sql_select.cc, sql_acl.cc, sql_update.cc, and sql_insert.cc.
*/

int ha_federated::update_row(const byte *old_data, byte *new_data)
{
  /*
    This used to control how the query was built. If there was a
    primary key, the query would be built such that there was a where
    clause with only that column as the condition. This is flawed,
    because if we have a multi-part primary key, it would only use the
    first part! We don't need to do this anyway, because
    read_range_first will retrieve the correct record, which is what
    is used to build the WHERE clause. We can however use this to
    append a LIMIT to the end if there is NOT a primary key. Why do
    this? Because we only are updating one record, and LIMIT enforces
    this.
  */
  bool has_a_primary_key= test(table->s->primary_key != MAX_KEY);
  
  /*
    buffers for following strings
  */
  char field_value_buffer[STRING_BUFFER_USUAL_SIZE];
  char update_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char where_buffer[FEDERATED_QUERY_BUFFER_SIZE];

  /* Work area for field values */
  String field_value(field_value_buffer, sizeof(field_value_buffer),
                     &my_charset_bin);
  /* stores the update query */
  String update_string(update_buffer,
                       sizeof(update_buffer),
                       &my_charset_bin);
  /* stores the WHERE clause */
  String where_string(where_buffer,
                      sizeof(where_buffer),
                      &my_charset_bin);
  DBUG_ENTER("ha_federated::update_row");
  /*
    set string lengths to 0 to avoid misc chars in string
  */
  field_value.length(0);
  update_string.length(0);
  where_string.length(0);

  update_string.append(STRING_WITH_LEN("UPDATE `"));
  update_string.append(share->table_name);
  update_string.append(STRING_WITH_LEN("` SET "));

  /*
    In this loop, we want to match column names to values being inserted
    (while building INSERT statement).

    Iterate through table->field (new data) and share->old_field (old_data)
    using the same index to create an SQL UPDATE statement. New data is
    used to create SET field=value and old data is used to create WHERE
    field=oldvalue
  */

  for (Field **field= table->field; *field; field++)
  {
    if (bitmap_is_set(table->write_set, (*field)->field_index))
    {
      update_string.append((*field)->field_name);
      update_string.append(STRING_WITH_LEN(" = "));

      if ((*field)->is_null())
        update_string.append(STRING_WITH_LEN(" NULL "));
      else
      {
        my_bitmap_map *old_map= tmp_use_all_columns(table, table->read_set);
        /* otherwise = */
	(*field)->val_str(&field_value);
        update_string.append('\'');
        field_value.print(&update_string);
        update_string.append('\'');
        field_value.length(0);
        tmp_restore_column_map(table->read_set, old_map);
      }
      update_string.append(STRING_WITH_LEN(", "));
    }

    if (bitmap_is_set(table->read_set, (*field)->field_index))
    {
      where_string.append((*field)->field_name);
      if (field_in_record_is_null(table, *field, (char*) old_data))
        where_string.append(STRING_WITH_LEN(" IS NULL "));
      else
      {
        where_string.append(STRING_WITH_LEN(" = "));
        (*field)->val_str(&field_value,
                          (char*) (old_data + (*field)->offset()));
	where_string.append('\'');
        field_value.print(&where_string);
	where_string.append('\'');
        field_value.length(0);
      }
      where_string.append(STRING_WITH_LEN(" AND "));
    }
  }

  /* Remove last ', '. This works as there must be at least on updated field */
  update_string.length(update_string.length() - sizeof_trailing_comma);

  if (where_string.length())
  {
    /* chop off trailing AND */
    where_string.length(where_string.length() - sizeof_trailing_and);
    update_string.append(STRING_WITH_LEN(" WHERE "));
    update_string.append(where_string);
  }

  /*
    If this table has not a primary key, then we could possibly
    update multiple rows. We want to make sure to only update one!
  */
  if (!has_a_primary_key)
    update_string.append(STRING_WITH_LEN(" LIMIT 1"));

  if (mysql_real_query(mysql, update_string.ptr(), update_string.length()))
  {
    DBUG_RETURN(stash_remote_error());
  }
  DBUG_RETURN(0);
}

/*
  This will delete a row. 'buf' will contain a copy of the row to be =deleted.
  The server will call this right after the current row has been called (from
  either a previous rnd_next() or index call).
  If you keep a pointer to the last row or can access a primary key it will
  make doing the deletion quite a bit easier.
  Keep in mind that the server does no guarentee consecutive deletions.
  ORDER BY clauses can be used.

  Called in sql_acl.cc and sql_udf.cc to manage internal table information.
  Called in sql_delete.cc, sql_insert.cc, and sql_select.cc. In sql_select
  it is used for removing duplicates while in insert it is used for REPLACE
  calls.
*/

int ha_federated::delete_row(const byte *buf)
{
  char delete_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char data_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  String delete_string(delete_buffer, sizeof(delete_buffer), &my_charset_bin);
  String data_string(data_buffer, sizeof(data_buffer), &my_charset_bin);
  uint found= 0;
  DBUG_ENTER("ha_federated::delete_row");

  delete_string.length(0);
  delete_string.append(STRING_WITH_LEN("DELETE FROM `"));
  delete_string.append(share->table_name);
  delete_string.append(STRING_WITH_LEN("` WHERE "));

  for (Field **field= table->field; *field; field++)
  {
    Field *cur_field= *field;
    found++;
    if (bitmap_is_set(table->read_set, cur_field->field_index))
    {
      data_string.length(0);
      delete_string.append(cur_field->field_name);
      if (cur_field->is_null())
      {
        delete_string.append(STRING_WITH_LEN(" IS NULL "));
      }
      else
      {
      delete_string.append(STRING_WITH_LEN(" = "));
      cur_field->val_str(&data_string);
      delete_string.append('\'');
      data_string.print(&delete_string);
      delete_string.append('\'');
      }
      delete_string.append(STRING_WITH_LEN(" AND "));
    }
  }

  // Remove trailing AND
  delete_string.length(delete_string.length() - sizeof_trailing_and);
  if (!found)
    delete_string.length(delete_string.length() - sizeof_trailing_where);

  delete_string.append(STRING_WITH_LEN(" LIMIT 1"));
  DBUG_PRINT("info",
             ("Delete sql: %s", delete_string.c_ptr_quick()));
  if (mysql_real_query(mysql, delete_string.ptr(), delete_string.length()))
  {
    DBUG_RETURN(stash_remote_error());
  }
  stats.deleted+= mysql->affected_rows;
  stats.records-= mysql->affected_rows;
  DBUG_PRINT("info",
             ("rows deleted %d rows deleted for all time %d",
             int(mysql->affected_rows), stats.deleted));

  DBUG_RETURN(0);
}


/*
  Positions an index cursor to the index specified in the handle. Fetches the
  row if available. If the key value is null, begin at the first key of the
  index. This method, which is called in the case of an SQL statement having
  a WHERE clause on a non-primary key index, simply calls index_read_idx.
*/

int ha_federated::index_read(byte *buf, const byte *key,
                             uint key_len, ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_federated::index_read");

  if (stored_result)
    mysql_free_result(stored_result);
  DBUG_RETURN(index_read_idx_with_result_set(buf, active_index, key,
                                             key_len, find_flag,
                                             &stored_result));
}


/*
  Positions an index cursor to the index specified in key. Fetches the
  row if any.  This is only used to read whole keys.

  This method is called via index_read in the case of a WHERE clause using
  a primary key index OR is called DIRECTLY when the WHERE clause
  uses a PRIMARY KEY index.

  NOTES
    This uses an internal result set that is deleted before function
    returns.  We need to be able to be calable from ha_rnd_pos()
*/

int ha_federated::index_read_idx(byte *buf, uint index, const byte *key,
                                 uint key_len, enum ha_rkey_function find_flag)
{
  int retval;
  MYSQL_RES *mysql_result;
  DBUG_ENTER("ha_federated::index_read_idx");

  if ((retval= index_read_idx_with_result_set(buf, index, key,
                                              key_len, find_flag,
                                              &mysql_result)))
    DBUG_RETURN(retval);
  mysql_free_result(mysql_result);
  DBUG_RETURN(retval);
}


/*
  Create result set for rows matching query and return first row

  RESULT
    0	ok     In this case *result will contain the result set
	       table->status == 0 
    #   error  In this case *result will contain 0
               table->status == STATUS_NOT_FOUND
*/

int ha_federated::index_read_idx_with_result_set(byte *buf, uint index,
                                                 const byte *key,
                                                 uint key_len,
                                                 ha_rkey_function find_flag,
                                                 MYSQL_RES **result)
{
  int retval;
  char error_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char index_value[STRING_BUFFER_USUAL_SIZE];
  char sql_query_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  String index_string(index_value,
                      sizeof(index_value),
                      &my_charset_bin);
  String sql_query(sql_query_buffer,
                   sizeof(sql_query_buffer),
                   &my_charset_bin);
  key_range range;
  DBUG_ENTER("ha_federated::index_read_idx_with_result_set");

  *result= 0;                                   // In case of errors
  index_string.length(0);
  sql_query.length(0);
  statistic_increment(table->in_use->status_var.ha_read_key_count,
                      &LOCK_status);

  sql_query.append(share->select_query);

  range.key= key;
  range.length= key_len;
  range.flag= find_flag;
  create_where_from_key(&index_string,
                        &table->key_info[index],
                        &range,
                        NULL, 0, 0);
  sql_query.append(index_string);

  if (mysql_real_query(mysql, sql_query.ptr(), sql_query.length()))
  {
    my_sprintf(error_buffer, (error_buffer, "error: %d '%s'",
                              mysql_errno(mysql), mysql_error(mysql)));
    retval= ER_QUERY_ON_FOREIGN_DATA_SOURCE;
    goto error;
  }
  if (!(*result= mysql_store_result(mysql)))
  {
    retval= HA_ERR_END_OF_FILE;
    goto error;
  }
  if (!(retval= read_next(buf, *result)))
    DBUG_RETURN(retval);

  mysql_free_result(*result);
  *result= 0;
  table->status= STATUS_NOT_FOUND;
  DBUG_RETURN(retval);

error:
  table->status= STATUS_NOT_FOUND;
  my_error(retval, MYF(0), error_buffer);
  DBUG_RETURN(retval);
}


/* Initialized at each key walk (called multiple times unlike rnd_init()) */

int ha_federated::index_init(uint keynr, bool sorted)
{
  DBUG_ENTER("ha_federated::index_init");
  DBUG_PRINT("info", ("table: '%s'  key: %u", table->s->table_name, keynr));
  active_index= keynr;
  DBUG_RETURN(0);
}


/*
  Read first range
*/

int ha_federated::read_range_first(const key_range *start_key,
                                   const key_range *end_key,
                                   bool eq_range, bool sorted)
{
  char sql_query_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  int retval;
  String sql_query(sql_query_buffer,
                   sizeof(sql_query_buffer),
                   &my_charset_bin);
  DBUG_ENTER("ha_federated::read_range_first");

  DBUG_ASSERT(!(start_key == NULL && end_key == NULL));

  sql_query.length(0);
  sql_query.append(share->select_query);
  create_where_from_key(&sql_query,
                        &table->key_info[active_index],
                        start_key, end_key, 0, eq_range);

  if (stored_result)
  {
    mysql_free_result(stored_result);
    stored_result= 0;
  }
  if (mysql_real_query(mysql, sql_query.ptr(), sql_query.length()))
  {
    retval= ER_QUERY_ON_FOREIGN_DATA_SOURCE;
    goto error;
  }
  sql_query.length(0);

  if (!(stored_result= mysql_store_result(mysql)))
  {
    retval= HA_ERR_END_OF_FILE;
    goto error;
  }

  retval= read_next(table->record[0], stored_result);
  DBUG_RETURN(retval);

error:
  table->status= STATUS_NOT_FOUND;
  DBUG_RETURN(retval);
}


int ha_federated::read_range_next()
{
  int retval;
  DBUG_ENTER("ha_federated::read_range_next");
  retval= rnd_next(table->record[0]);
  DBUG_RETURN(retval);
}


/* Used to read forward through the index.  */
int ha_federated::index_next(byte *buf)
{
  DBUG_ENTER("ha_federated::index_next");
  statistic_increment(table->in_use->status_var.ha_read_next_count,
		      &LOCK_status);
  DBUG_RETURN(read_next(buf, stored_result));
}


/*
  rnd_init() is called when the system wants the storage engine to do a table
  scan.

  This is the method that gets data for the SELECT calls.

  See the federated in the introduction at the top of this file to see when
  rnd_init() is called.

  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.
*/

int ha_federated::rnd_init(bool scan)
{
  DBUG_ENTER("ha_federated::rnd_init");
  /*
    The use of the 'scan' flag is incredibly important for this handler
    to work properly, especially with updates containing WHERE clauses
    using indexed columns.

    When the initial query contains a WHERE clause of the query using an
    indexed column, it's index_read_idx that selects the exact record from
    the foreign database.

    When there is NO index in the query, either due to not having a WHERE
    clause, or the WHERE clause is using columns that are not indexed, a
    'full table scan' done by rnd_init, which in this situation simply means
    a 'select * from ...' on the foreign table.

    In other words, this 'scan' flag gives us the means to ensure that if
    there is an index involved in the query, we want index_read_idx to
    retrieve the exact record (scan flag is 0), and do not  want rnd_init
    to do a 'full table scan' and wipe out that result set.

    Prior to using this flag, the problem was most apparent with updates.

    An initial query like 'UPDATE tablename SET anything = whatever WHERE
    indexedcol = someval', index_read_idx would get called, using a query
    constructed with a WHERE clause built from the values of index ('indexcol'
    in this case, having a value of 'someval').  mysql_store_result would
    then get called (this would be the result set we want to use).

    After this rnd_init (from sql_update.cc) would be called, it would then
    unecessarily call "select * from table" on the foreign table, then call
    mysql_store_result, which would wipe out the correct previous result set
    from the previous call of index_read_idx's that had the result set
    containing the correct record, hence update the wrong row!

  */

  if (scan)
  {
    if (stored_result)
    {
      mysql_free_result(stored_result);
      stored_result= 0;
    }

    if (mysql_real_query(mysql,
                         share->select_query,
                         strlen(share->select_query)))
      goto error;

    stored_result= mysql_store_result(mysql);
    if (!stored_result)
      goto error;
  }
  DBUG_RETURN(0);

error:
  DBUG_RETURN(stash_remote_error());
}


int ha_federated::rnd_end()
{
  DBUG_ENTER("ha_federated::rnd_end");
  DBUG_RETURN(index_end());
}


int ha_federated::index_end(void)
{
  DBUG_ENTER("ha_federated::index_end");
  if (stored_result)
  {
    mysql_free_result(stored_result);
    stored_result= 0;
  }
  active_index= MAX_KEY;
  DBUG_RETURN(0);
}


/*
  This is called for each row of the table scan. When you run out of records
  you should return HA_ERR_END_OF_FILE. Fill buff up with the row information.
  The Field structure for the table is the key to getting data into buf
  in a manner that will allow the server to understand it.

  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.
*/

int ha_federated::rnd_next(byte *buf)
{
  DBUG_ENTER("ha_federated::rnd_next");

  if (stored_result == 0)
  {
    /*
      Return value of rnd_init is not always checked (see records.cc),
      so we can get here _even_ if there is _no_ pre-fetched result-set!
      TODO: fix it. We can delete this in 5.1 when rnd_init() is checked.
    */
    DBUG_RETURN(1);
  }
  DBUG_RETURN(read_next(buf, stored_result));
}


/*
  ha_federated::read_next

  reads from a result set and converts to mysql internal
  format

  SYNOPSIS
    field_in_record_is_null()
      buf       byte pointer to record 
      result    mysql result set 

    DESCRIPTION
     This method is a wrapper method that reads one record from a result
     set and converts it to the internal table format

    RETURN VALUE
      1    error
      0    no error 
*/

int ha_federated::read_next(byte *buf, MYSQL_RES *result)
{
  int retval;
  my_ulonglong num_rows;
  MYSQL_ROW row;
  DBUG_ENTER("ha_federated::read_next");

  table->status= STATUS_NOT_FOUND;              // For easier return

  /* Fetch a row, insert it back in a row format. */
  if (!(row= mysql_fetch_row(result)))
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  if (!(retval= convert_row_to_internal_format(buf, row, result)))
    table->status= 0;

  DBUG_RETURN(retval);
}


/*
  store reference to current row so that we can later find it for
  a re-read, update or delete.

  In case of federated, a reference is either a primary key or
  the whole record.

  Called from filesort.cc, sql_select.cc, sql_delete.cc and sql_update.cc.
*/

void ha_federated::position(const byte *record)
{
  DBUG_ENTER("ha_federated::position");
  if (table->s->primary_key != MAX_KEY)
    key_copy(ref, (byte *)record, table->key_info + table->s->primary_key,
             ref_length);
  else
    memcpy(ref, record, ref_length);
  DBUG_VOID_RETURN;
}


/*
  This is like rnd_next, but you are given a position to use to determine the
  row. The position will be of the type that you stored in ref.

  This method is required for an ORDER BY

  Called from filesort.cc records.cc sql_insert.cc sql_select.cc sql_update.cc.
*/

int ha_federated::rnd_pos(byte *buf, byte *pos)
{
  int result;
  DBUG_ENTER("ha_federated::rnd_pos");
  statistic_increment(table->in_use->status_var.ha_read_rnd_count,
                      &LOCK_status);
  if (table->s->primary_key != MAX_KEY)
  {
    /* We have a primary key, so use index_read_idx to find row */
    result= index_read_idx(buf, table->s->primary_key, pos,
                           ref_length, HA_READ_KEY_EXACT);
  }
  else
  {
    /* otherwise, get the old record ref as obtained in ::position */
    memcpy(buf, pos, ref_length);
    result= 0;
  }
  table->status= result ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(result);
}


/*
  ::info() is used to return information to the optimizer.
  Currently this table handler doesn't implement most of the fields
  really needed. SHOW also makes use of this data
  Another note, you will probably want to have the following in your
  code:
  if (records < 2)
    records = 2;
  The reason is that the server will optimize for cases of only a single
  record. If in a table scan you don't know the number of records
  it will probably be better to set records to two so you can return
  as many records as you need.
  Along with records a few more variables you may wish to set are:
    records
    deleted
    data_file_length
    index_file_length
    delete_length
    check_time
  Take a look at the public variables in handler.h for more information.

  Called in:
    filesort.cc
    ha_heap.cc
    item_sum.cc
    opt_sum.cc
    sql_delete.cc
    sql_delete.cc
    sql_derived.cc
    sql_select.cc
    sql_select.cc
    sql_select.cc
    sql_select.cc
    sql_select.cc
    sql_show.cc
    sql_show.cc
    sql_show.cc
    sql_show.cc
    sql_table.cc
    sql_union.cc
    sql_update.cc

*/

void ha_federated::info(uint flag)
{
  char error_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  char status_buf[FEDERATED_QUERY_BUFFER_SIZE];
  char escaped_table_name[FEDERATED_QUERY_BUFFER_SIZE];
  int error;
  uint error_code;
  MYSQL_RES *result= 0;
  MYSQL_ROW row;
  String status_query_string(status_buf, sizeof(status_buf), &my_charset_bin);
  DBUG_ENTER("ha_federated::info");

  error_code= ER_QUERY_ON_FOREIGN_DATA_SOURCE;
  /* we want not to show table status if not needed to do so */
  if (flag & (HA_STATUS_VARIABLE | HA_STATUS_CONST))
  {
    status_query_string.length(0);
    status_query_string.append(STRING_WITH_LEN("SHOW TABLE STATUS LIKE '"));
    escape_string_for_mysql(&my_charset_bin, (char *)escaped_table_name,
                            sizeof(escaped_table_name),
                            share->table_name,
                            share->table_name_length);
    status_query_string.append(escaped_table_name);
    status_query_string.append(STRING_WITH_LEN("'"));

    if (mysql_real_query(mysql, status_query_string.ptr(),
                         status_query_string.length()))
      goto error;

    status_query_string.length(0);

    result= mysql_store_result(mysql);
    if (!result)
      goto error;

    if (!mysql_num_rows(result))
      goto error;

    if (!(row= mysql_fetch_row(result)))
      goto error;

    if (flag & HA_STATUS_VARIABLE | HA_STATUS_CONST)
    {
      /*
        deleted is set in ha_federated::info
      */
      /*
        need to figure out what this means as far as federated is concerned,
        since we don't have a "file"

        data_file_length = ?
        index_file_length = ?
        delete_length = ?
      */
      if (row[4] != NULL)
        stats.records=   (ha_rows) my_strtoll10(row[4], (char**) 0,
                                                       &error);
      if (row[5] != NULL)
        stats.mean_rec_length= (ha_rows) my_strtoll10(row[5], (char**) 0, &error);

      stats.data_file_length= stats.records * stats.mean_rec_length;

      if (row[12] != NULL)
        stats.update_time=     (ha_rows) my_strtoll10(row[12], (char**) 0,
                                                      &error);
      if (row[13] != NULL)
        stats.check_time=      (ha_rows) my_strtoll10(row[13], (char**) 0,
                                                      &error);
    }
    /*
      size of IO operations (This is based on a good guess, no high science
      involved)
    */
    if (flag & HA_STATUS_CONST)
      stats.block_size= 4096;

  }

  if (result)
    mysql_free_result(result);

  DBUG_VOID_RETURN;

error:
  if (result)
    mysql_free_result(result);

  my_sprintf(error_buffer, (error_buffer, ": %d : %s",
                            mysql_errno(mysql), mysql_error(mysql)));
  my_error(error_code, MYF(0), error_buffer);
  DBUG_VOID_RETURN;
}


/*
  Used to delete all rows in a table. Both for cases of truncate and
  for cases where the optimizer realizes that all rows will be
  removed as a result of a SQL statement.

  Called from item_sum.cc by Item_func_group_concat::clear(),
  Item_sum_count_distinct::clear(), and Item_func_group_concat::clear().
  Called from sql_delete.cc by mysql_delete().
  Called from sql_select.cc by JOIN::reinit().
  Called from sql_union.cc by st_select_lex_unit::exec().
*/

int ha_federated::delete_all_rows()
{
  char query_buffer[FEDERATED_QUERY_BUFFER_SIZE];
  String query(query_buffer, sizeof(query_buffer), &my_charset_bin);
  DBUG_ENTER("ha_federated::delete_all_rows");

  query.length(0);

  query.set_charset(system_charset_info);
  query.append(STRING_WITH_LEN("TRUNCATE `"));
  query.append(share->table_name);
  query.append(STRING_WITH_LEN("`"));

  /*
    TRUNCATE won't return anything in mysql_affected_rows
  */
  if (mysql_real_query(mysql, query.ptr(), query.length()))
  {
    DBUG_RETURN(stash_remote_error());
  }
  stats.deleted+= stats.records;
  stats.records= 0;
  DBUG_RETURN(0);
}


/*
  The idea with handler::store_lock() is the following:

  The statement decided which locks we should need for the table
  for updates/deletes/inserts we get WRITE locks, for SELECT... we get
  read locks.

  Before adding the lock into the table lock handler (see thr_lock.c)
  mysqld calls store lock with the requested locks.  Store lock can now
  modify a write lock to a read lock (or some other lock), ignore the
  lock (if we don't want to use MySQL table locks at all) or add locks
  for many tables (like we do when we are using a MERGE handler).

  Berkeley DB for federated  changes all WRITE locks to TL_WRITE_ALLOW_WRITE
  (which signals that we are doing WRITES, but we are still allowing other
  reader's and writer's.

  When releasing locks, store_lock() are also called. In this case one
  usually doesn't have to do anything.

  In some exceptional cases MySQL may send a request for a TL_IGNORE;
  This means that we are requesting the same lock as last time and this
  should also be ignored. (This may happen when someone does a flush
  table when we have opened a part of the tables, in which case mysqld
  closes and reopens the tables and tries to get the same locks at last
  time).  In the future we will probably try to remove this.

  Called from lock.cc by get_lock_data().
*/

THR_LOCK_DATA **ha_federated::store_lock(THD *thd,
                                         THR_LOCK_DATA **to,
                                         enum thr_lock_type lock_type)
{
  DBUG_ENTER("ha_federated::store_lock");
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK TABLE or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !thd->in_lock_tables)
      lock_type= TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT && !thd->in_lock_tables)
      lock_type= TL_READ;

    lock.type= lock_type;
  }

  *to++= &lock;

  DBUG_RETURN(to);
}

/*
  create() does nothing, since we have no local setup of our own.
  FUTURE: We should potentially connect to the foreign database and
*/

int ha_federated::create(const char *name, TABLE *table_arg,
                         HA_CREATE_INFO *create_info)
{
  int retval;
  FEDERATED_SHARE tmp_share; // Only a temporary share, to test the url
  DBUG_ENTER("ha_federated::create");

  if (!(retval= parse_url(&tmp_share, table_arg, 1)))
    retval= check_foreign_data_source(&tmp_share, 1);

  my_free((gptr) tmp_share.scheme, MYF(MY_ALLOW_ZERO_PTR));
  DBUG_RETURN(retval);

}


int ha_federated::stash_remote_error()
{
  DBUG_ENTER("ha_federated::stash_remote_error()");
  remote_error_number= mysql_errno(mysql);
  strmake(remote_error_buf, mysql_error(mysql), sizeof(remote_error_buf)-1);
  DBUG_RETURN(HA_FEDERATED_ERROR_WITH_REMOTE_SYSTEM);
}


bool ha_federated::get_error_message(int error, String* buf)
{
  DBUG_ENTER("ha_federated::get_error_message");
  DBUG_PRINT("enter", ("error: %d", error));
  if (error == HA_FEDERATED_ERROR_WITH_REMOTE_SYSTEM)
  {
    buf->append(STRING_WITH_LEN("Error on remote system: "));
    buf->qs_append(remote_error_number);
    buf->append(STRING_WITH_LEN(": "));
    buf->append(remote_error_buf);

    remote_error_number= 0;
    remote_error_buf[0]= '\0';
  }
  DBUG_PRINT("exit", ("message: %s", buf->ptr()));
  DBUG_RETURN(FALSE);
}

int ha_federated::external_lock(THD *thd, int lock_type)
{
  int error= 0;
  ha_federated *trx= (ha_federated *)thd->ha_data[federated_hton.slot];
  DBUG_ENTER("ha_federated::external_lock");

  if (lock_type != F_UNLCK)
  {
    DBUG_PRINT("info",("federated not lock F_UNLCK"));
    if (!(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) 
    {
      DBUG_PRINT("info",("federated autocommit"));
      /* 
        This means we are doing an autocommit
      */
      error= connection_autocommit(TRUE);
      if (error)
      {
        DBUG_PRINT("info", ("error setting autocommit TRUE: %d", error));
        DBUG_RETURN(error);
      }
      trans_register_ha(thd, FALSE, &federated_hton);
    }
    else 
    { 
      DBUG_PRINT("info",("not autocommit"));
      if (!trx)
      {
        /* 
          This is where a transaction gets its start
        */
        error= connection_autocommit(FALSE);
        if (error)
        { 
          DBUG_PRINT("info", ("error setting autocommit FALSE: %d", error));
          DBUG_RETURN(error);
        }
        thd->ha_data[federated_hton.slot]= this;
        trans_register_ha(thd, TRUE, &federated_hton);
        /*
          Send a lock table to the remote end.
          We do not support this at the moment
        */
        if (thd->options & (OPTION_TABLE_LOCK))
        {
          DBUG_PRINT("info", ("We do not support lock table yet"));
        }
      }
      else
      {
        ha_federated *ptr;
        for (ptr= trx; ptr; ptr= ptr->trx_next)
          if (ptr == this)
            break;
          else if (!ptr->trx_next)
            ptr->trx_next= this;
      }
    }
  }
  DBUG_RETURN(0);
}


static int federated_commit(THD *thd, bool all)
{
  int return_val= 0;
  ha_federated *trx= (ha_federated *)thd->ha_data[federated_hton.slot];
  DBUG_ENTER("federated_commit");

  if (all)
  {
    int error= 0;
    ha_federated *ptr, *old= NULL;
    for (ptr= trx; ptr; old= ptr, ptr= ptr->trx_next)
    {
      if (old)
        old->trx_next= NULL;
      error= ptr->connection_commit();
      if (error && !return_val);
        return_val= error;
    }
    thd->ha_data[federated_hton.slot]= NULL;
  }

  DBUG_PRINT("info", ("error val: %d", return_val));
  DBUG_RETURN(return_val);
}


static int federated_rollback(THD *thd, bool all)
{
  int return_val= 0;
  ha_federated *trx= (ha_federated *)thd->ha_data[federated_hton.slot];
  DBUG_ENTER("federated_rollback");

  if (all)
  {
    int error= 0;
    ha_federated *ptr, *old= NULL;
    for (ptr= trx; ptr; old= ptr, ptr= ptr->trx_next)
    {
      if (old)
        old->trx_next= NULL;
      error= ptr->connection_rollback();
      if (error && !return_val)
        return_val= error;
    }
    thd->ha_data[federated_hton.slot]= NULL;
  }

  DBUG_PRINT("info", ("error val: %d", return_val));
  DBUG_RETURN(return_val);
}

int ha_federated::connection_commit()
{
  DBUG_ENTER("ha_federated::connection_commit");
  DBUG_RETURN(execute_simple_query("COMMIT", 6));
}


int ha_federated::connection_rollback()
{
  DBUG_ENTER("ha_federated::connection_rollback");
  DBUG_RETURN(execute_simple_query("ROLLBACK", 8));
}


int ha_federated::connection_autocommit(bool state)
{
  const char *text;
  DBUG_ENTER("ha_federated::connection_autocommit");
  text= (state == true) ? "SET AUTOCOMMIT=1" : "SET AUTOCOMMIT=0";
  DBUG_RETURN(execute_simple_query(text, 16));
}


int ha_federated::execute_simple_query(const char *query, int len)
{
  DBUG_ENTER("ha_federated::execute_simple_query");

  if (mysql_real_query(mysql, query, len))
  {
    DBUG_RETURN(stash_remote_error());
  }
  DBUG_RETURN(0);
}

struct st_mysql_storage_engine federated_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION, &federated_hton };

mysql_declare_plugin(federated)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &federated_storage_engine,
  "FEDERATED",
  "Patrick Galbraith and Brian Aker, MySQL AB",
  "Federated MySQL storage engine",
  federated_db_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  0
}
mysql_declare_plugin_end;

#endif
