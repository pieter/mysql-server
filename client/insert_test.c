/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#include <stdio.h>
#include <stdlib.h>
#include "mysql.h"

#define INSERT_QUERY "insert into test (name,num) values ('item %d', %d)"


int main(int argc, char **argv)
{
  int	count,num;
  MYSQL *sock,mysql;
  char	qbuf[160];

  if (argc != 3)
  {
    fprintf(stderr,"usage : insert_test <dbname> <Num>\n\n");
    exit(1);
  }

  if (!(sock = mysql_connect(&mysql,NULL,0,0)))
  {
    fprintf(stderr,"Couldn't connect to engine!\n%s\n",mysql_error(&mysql));
    perror("");
    exit(1);
  }

  if (mysql_select_db(sock,argv[1]))
  {
    fprintf(stderr,"Couldn't select database %s!\n%s\n",argv[1],
	    mysql_error(sock));
  }

  num = atoi(argv[2]);
  count = 0;
  while (count < num)
  {
    sprintf(qbuf,INSERT_QUERY,count,count);
    if(mysql_query(sock,qbuf))
    {
      fprintf(stderr,"Query failed (%s)\n",mysql_error(sock));
      exit(1);
    }
    count++;
  }
  mysql_close(sock);
  exit(0);
  return 0;
}
