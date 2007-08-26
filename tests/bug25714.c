/* Copyright (C) 2007 MySQL AB

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

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <m_string.h>
#include <assert.h>

int main (int argc, char **argv)
{
  MYSQL conn;
  int OK;

  const char* query4= "INSERT INTO federated.t1 SET Value=54";
  const char* query5= "INSERT INTO federated.t1 SET Value=55";

  MY_INIT(argv[0]);

  if (argc != 2)
    return -1;

  mysql_init(&conn);
  if (!mysql_real_connect(
                          &conn,
                          "127.0.0.1",
                          "root",
                          "",
                          "test",
                          atoi(argv[1]),
                          NULL,
                          CLIENT_FOUND_ROWS))
  {
    fprintf(stderr, "Failed to connect to database: Error: %s\n",
            mysql_error(&conn));
    return 1;
  } else {
    printf("%s\n", mysql_error(&conn));
  }

  OK = mysql_real_query (&conn, query4, strlen(query4));

  assert(0 == OK);

  printf("%ld inserted\n",
         (long) mysql_insert_id(&conn));

  OK = mysql_real_query (&conn, query5, strlen(query5));

  assert(0 == OK);

  printf("%ld inserted\n",
         (long) mysql_insert_id(&conn));

  mysql_close(&conn);
  my_end(0);

  return 0;
}

