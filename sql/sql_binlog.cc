/* Copyright (C) 2005 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include "mysql_priv.h"
#include "base64.h"

/*
  Execute a BINLOG statement

  TODO: This currently assumes a MySQL 5.x binlog.
  When we'll have binlog with a different format, to execute the
  BINLOG command properly the server will need to know which format
  the BINLOG command's event is in.  mysqlbinlog should then send
  the Format_description_log_event of the binlog it reads and the
  server thread should cache this format into
  rli->description_event_for_exec.
*/

void mysql_client_binlog_statement(THD* thd)
{
  DBUG_ENTER("mysql_client_binlog_statement");
  DBUG_PRINT("info",("binlog base64: '%*s'",
                     (thd->lex->comment.length < 2048 ?
                      thd->lex->comment.length : 2048),
                     thd->lex->comment.str));

  /*
    Temporarily turn off send_ok, since different events handle this
    differently
  */
  my_bool nsok= thd->net.no_send_ok;
  thd->net.no_send_ok= TRUE;

  my_size_t coded_len= thd->lex->comment.length + 1;
  my_size_t decoded_len= base64_needed_decoded_length(coded_len);
  DBUG_ASSERT(coded_len > 0);

  /*
    Allocation
  */
  if (!thd->rli_fake)
    thd->rli_fake= new RELAY_LOG_INFO;

  const Format_description_log_event *desc=
    new Format_description_log_event(4);

  const char *error= 0;
  char *buf= (char *) my_malloc(decoded_len, MYF(MY_WME));
  Log_event *ev = 0;

  /*
    Out of memory check
  */
  if (!(thd->rli_fake && desc && buf))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), 1);  /* needed 1 bytes */
    goto end;
  }

  thd->rli_fake->sql_thd= thd;
  thd->rli_fake->no_storage= TRUE;

  for (char const *strptr= thd->lex->comment.str ;
       strptr < thd->lex->comment.str + thd->lex->comment.length ; )
  {
    char const *endptr= 0;
    int bytes_decoded= base64_decode(strptr, coded_len, buf, &endptr);

    DBUG_PRINT("info",
               ("bytes_decoded: %d  strptr: 0x%lx  endptr: 0x%lx ('%c':%d)",
                bytes_decoded, (long) strptr, (long) endptr, *endptr,
                *endptr));

    if (bytes_decoded < 0)
    {
      my_error(ER_BASE64_DECODE_ERROR, MYF(0));
      goto end;
    }
    else if (bytes_decoded == 0)
      break; // If no bytes where read, the string contained only whitespace

    DBUG_ASSERT(bytes_decoded > 0);
    DBUG_ASSERT(endptr > strptr);
    coded_len-= endptr - strptr;
    strptr= endptr;

    /*
      Now we have one or more events stored in the buffer. The size of
      the buffer is computed based on how much base64-encoded data
      there were, so there should be ample space for the data (maybe
      even too much, since a statement can consist of a considerable
      number of events).

      TODO: Switch to use a stream-based base64 encoder/decoder in
      order to be able to read exactly what is necessary.
    */

    DBUG_PRINT("info",("binlog base64 decoded_len=%d, bytes_decoded=%d",
                       decoded_len, bytes_decoded));

    /*
      Now we start to read events of the buffer, until there are no
      more.
    */
    for (char *bufptr= buf ; bytes_decoded > 0 ; )
    {
      /*
        Checking that the first event in the buffer is not truncated.
      */
      ulong event_len= uint4korr(bufptr + EVENT_LEN_OFFSET);
      DBUG_PRINT("info", ("event_len=%lu, bytes_decoded=%d",
                          event_len, bytes_decoded));
      if (bytes_decoded < EVENT_LEN_OFFSET || (uint) bytes_decoded < event_len)
      {
        my_error(ER_SYNTAX_ERROR, MYF(0));
        goto end;
      }

      ev= Log_event::read_log_event(bufptr, event_len, &error, desc);

      DBUG_PRINT("info",("binlog base64 err=%s", error));
      if (!ev)
      {
        /*
          This could actually be an out-of-memory, but it is more likely
          causes by a bad statement
        */
        my_error(ER_SYNTAX_ERROR, MYF(0));
        goto end;
      }

      bytes_decoded -= event_len;
      bufptr += event_len;

      DBUG_PRINT("info",("ev->get_type_code()=%d", ev->get_type_code()));
      DBUG_PRINT("info",("bufptr+EVENT_TYPE_OFFSET: 0x%lx",
                         (long) (bufptr+EVENT_TYPE_OFFSET)));
      DBUG_PRINT("info", ("bytes_decoded: %d   bufptr: 0x%lx  buf[EVENT_LEN_OFFSET]: %lu",
                          bytes_decoded, (long) bufptr,
                          uint4korr(bufptr+EVENT_LEN_OFFSET)));
      ev->thd= thd;
      if (int err= ev->exec_event(thd->rli_fake))
      {
        DBUG_PRINT("error", ("exec_event() returned: %d", err));
        /*
          TODO: Maybe a better error message since the BINLOG statement
          now contains several events.
        */
        my_error(ER_UNKNOWN_ERROR, MYF(0), "Error executing BINLOG statement");
        goto end;
      }

      delete ev;
      ev= 0;
    }
  }

  /*
    Restore setting of no_send_ok
  */
  thd->net.no_send_ok= nsok;

  DBUG_PRINT("info",("binlog base64 execution finished successfully"));
  send_ok(thd);

end:
  /*
    Restore setting of no_send_ok
  */
  thd->net.no_send_ok= nsok;

  delete desc;
  my_free(buf, MYF(MY_ALLOW_ZERO_PTR));
  DBUG_VOID_RETURN;
}
