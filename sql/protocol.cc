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

/*
  Low level functions for storing data to be send to the MySQL client
  The actual communction is handled by the net_xxx functions in net_serv.cc
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <stdarg.h>
#include <assert.h>

	/* Send a error string to client */

void send_error(THD *thd, uint sql_errno, const char *err)
{
  uint length;
  char buff[MYSQL_ERRMSG_SIZE+2];
  NET *net= &thd->net;
  DBUG_ENTER("send_error");
  DBUG_PRINT("enter",("sql_errno: %d  err: %s", sql_errno,
		      err ? err : net->last_error[0] ?
		      net->last_error : "NULL"));

  query_cache_abort(net);
  thd->query_error=  1; // needed to catch query errors during replication
  if (!err)
  {
    if (sql_errno)
      err=ER(sql_errno);
    else
    {
      if ((err=net->last_error)[0])
	sql_errno=net->last_errno;
      else
      {
	sql_errno=ER_UNKNOWN_ERROR;
	err=ER(sql_errno);	 /* purecov: inspected */
      }
    }
  }
  if (net->vio == 0)
  {
    if (thd->bootstrap)
    {
      /* In bootstrap it's ok to print on stderr */
      fprintf(stderr,"ERROR: %d  %s\n",sql_errno,err);
    }
    DBUG_VOID_RETURN;
  }

  if (net->return_errno)
  {				// new client code; Add errno before message
    int2store(buff,sql_errno);
    length= (uint) (strmake(buff+2,err,MYSQL_ERRMSG_SIZE-1) - buff);
    err=buff;
  }
  else
  {
    length=(uint) strlen(err);
    set_if_smaller(length,MYSQL_ERRMSG_SIZE-1);
  }
  VOID(net_write_command(net,(uchar) 255, "", 0, (char*) err,length));
  thd->fatal_error=0;			// Error message is given
  thd->net.report_error= 0;
  DBUG_VOID_RETURN;
}

/*
  Send an error to the client when a connection is forced close
  This is used by mysqld.cc, which doesn't have a THD
*/

void net_send_error(NET *net, uint sql_errno, const char *err)
{
  char buff[2];
  uint length;
  DBUG_ENTER("send_net_error");

  int2store(buff,sql_errno);
  length=(uint) strlen(err);
  set_if_smaller(length,MYSQL_ERRMSG_SIZE-1);
  net_write_command(net,(uchar) 255, buff, 2, err, length);
  DBUG_VOID_RETURN;
}


/*
  Send a warning to the end user

  SYNOPSIS
    send_warning()
    thd			Thread handler
    sql_errno		Warning number (error message)
    err			Error string.  If not set, use ER(sql_errno)

  DESCRIPTION
    Register the warning so that the user can get it with mysql_warnings()
    Send an ok (+ warning count) to the end user.
*/

void send_warning(THD *thd, uint sql_errno, const char *err)
{
  DBUG_ENTER("send_warning");  
  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, sql_errno,
	       err ? err : ER(sql_errno));
  send_ok(thd);
  DBUG_VOID_RETURN;
}


/*
   Write error package and flush to client
   It's a little too low level, but I don't want to use another buffer for
   this
*/

void
net_printf(THD *thd, uint errcode, ...)
{
  va_list args;
  uint length,offset;
  const char *format,*text_pos;
  int head_length= NET_HEADER_SIZE;
  NET *net= &thd->net;
  DBUG_ENTER("net_printf");
  DBUG_PRINT("enter",("message: %u",errcode));

  thd->query_error=  1; // needed to catch query errors during replication
  query_cache_abort(net);	// Safety
  va_start(args,errcode);
  /*
    The following is needed to make net_printf() work with 0 argument for
    errorcode and use the argument after that as the format string. This
    is useful for rare errors that are not worth the hassle to put in
    errmsg.sys, but at the same time, the message is not fixed text
  */
  if (errcode)
    format= ER(errcode);
  else
  {
    format=va_arg(args,char*);
    errcode= ER_UNKNOWN_ERROR;
  }
  offset= net->return_errno ? 2 : 0;
  text_pos=(char*) net->buff+head_length+offset+1;
  (void) vsprintf(my_const_cast(char*) (text_pos),format,args);
  length=(uint) strlen((char*) text_pos);
  if (length >= sizeof(net->last_error))
    length=sizeof(net->last_error)-1;		/* purecov: inspected */
  va_end(args);

  if (net->vio == 0)
  {
    if (thd->bootstrap)
    {
      /* In bootstrap it's ok to print on stderr */
      fprintf(stderr,"ERROR: %d  %s\n",errcode,text_pos);
      thd->fatal_error=1;
    }
    DBUG_VOID_RETURN;
  }

  int3store(net->buff,length+1+offset);
  net->buff[3]= (net->compress) ? 0 : (uchar) (net->pkt_nr++);
  net->buff[head_length]=(uchar) 255;		// Error package
  if (offset)
    int2store(text_pos-2, errcode);
  VOID(net_real_write(net,(char*) net->buff,length+head_length+1+offset));
  thd->fatal_error=0;			// Error message is given
  DBUG_VOID_RETURN;
}


/*
  Return ok to the client.

  SYNOPSIS
    send_ok()
    thd			Thread handler
    affected_rows	Number of rows changed by statement
    id			Auto_increment id for first row (if used)
    message		Message to send to the client (Used by mysql_status)

  DESCRIPTION
    The ok packet has the following structure

    0			Marker (1 byte)
    affected_rows	Stored in 1-9 bytes
    id			Stored in 1-9 bytes
    server_status	Copy of thd->server_status;  Can be used by client
			to check if we are inside an transaction
			New in 4.0 protocol
    warning_count	Stored in 2 bytes; New in 4.1 protocol
    message		Stored as packed length (1-9 bytes) + message
			Is not stored if no message

   If net->no_send_ok return without sending packet
*/    

void
send_ok(THD *thd, ha_rows affected_rows, ulonglong id, const char *message)
{
  NET *net= &thd->net;
  if (net->no_send_ok || !net->vio)	// hack for re-parsing queries
    return;

  char buff[MYSQL_ERRMSG_SIZE+10],*pos;
  DBUG_ENTER("send_ok");
  buff[0]=0;					// No fields
  pos=net_store_length(buff+1,(ulonglong) affected_rows);
  pos=net_store_length(pos, (ulonglong) id);
  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
  {
    int2store(pos,thd->server_status);
    pos+=2;

    /* We can only return up to 65535 warnings in two bytes */
    uint tmp= min(thd->total_warn_count, 65535);
    int2store(pos, tmp);
    pos+= 2;
  }
  else if (net->return_status)			// For 4.0 protocol
  {
    int2store(pos,thd->server_status);
    pos+=2;
  }
  if (message)
    pos=net_store_data((char*) pos, message, strlen(message));
  VOID(my_net_write(net,buff,(uint) (pos-buff)));
  VOID(net_flush(net));
  DBUG_VOID_RETURN;
}


/*
  Send eof (= end of result set) to the client

  SYNOPSIS
    send_eof()
    thd			Thread handler
    no_flush		Set to 1 if there will be more data to the client,
			like in send_fields().

  DESCRIPTION
    The eof packet has the following structure

    254			Marker (1 byte)
    warning_count	Stored in 2 bytes; New in 4.1 protocol
    status_flag		Stored in 2 bytes;
			For flags like SERVER_STATUS_MORE_RESULTS

    Note that the warning count will not be sent if 'no_flush' is set as
    we don't want to report the warning count until all data is sent to the
    client.
*/    

void
send_eof(THD *thd, bool no_flush)
{
  static char eof_buff[1]= { (char) 254 };	/* Marker for end of fields */
  NET *net= &thd->net;
  DBUG_ENTER("send_eof");
  if (net->vio != 0)
  {
    if (!no_flush && (thd->client_capabilities & CLIENT_PROTOCOL_41))
    {
      uchar buff[5];
      uint tmp= min(thd->total_warn_count, 65535);
      buff[0]=254;
      int2store(buff+1, tmp);
      int2store(buff+3, 0);			// No flags yet
      VOID(my_net_write(net,(char*) buff,5));
      VOID(net_flush(net));
    }
    else
    {
      VOID(my_net_write(net,eof_buff,1));
      if (!no_flush)
	VOID(net_flush(net));
    }
  }
  DBUG_VOID_RETURN;
}


/****************************************************************************
  Store a field length in logical packet

  This is used to code the string length for normal protocol
****************************************************************************/

char *
net_store_length(char *pkg, ulonglong length)
{
  uchar *packet=(uchar*) pkg;
  if (length < LL(251))
  {
    *packet=(uchar) length;
    return (char*) packet+1;
  }
  /* 251 is reserved for NULL */
  if (length < LL(65536))
  {
    *packet++=252;
    int2store(packet,(uint) length);
    return (char*) packet+2;
  }
  if (length < LL(16777216))
  {
    *packet++=253;
    int3store(packet,(ulong) length);
    return (char*) packet+3;
  }
  *packet++=254;
  int8store(packet,length);
  return (char*) packet+9;
}


/*
  Faster net_store_length when we know length is a 32 bit integer
*/

char *net_store_length(char *pkg, uint length)
{
  uchar *packet=(uchar*) pkg;
  if (length < 251)
  {
    *packet=(uchar) length;
    return (char*) packet+1;
  }
  *packet++=252;
  int2store(packet,(uint) length);
  return (char*) packet+2;
}


/*
  Used internally for storing strings in packet
*/

static bool net_store_data(String *packet, const char *from, uint length)
{
  ulong packet_length=packet->length();
  if (packet_length+5+length > packet->alloced_length() &&
      packet->realloc(packet_length+5+length))
    return 1;
  char *to=(char*) net_store_length((char*) packet->ptr()+packet_length,
				    (ulonglong) length);
  memcpy(to,from,length);
  packet->length((uint) (to+length-packet->ptr()));
  return 0;
}

/****************************************************************************
  Functions used by the protocol functions (like send_ok) to store strings
  and numbers in the header result packet.
****************************************************************************/

/* The following will only be used for short strings < 65K */

char *net_store_data(char *to,const char *from, uint length)
{
  to=net_store_length(to,length);
  memcpy(to,from,length);
  return to+length;
}

char *net_store_data(char *to,int32 from)
{
  char buff[20];
  uint length=(uint) (int10_to_str(from,buff,10)-buff);
  to=net_store_length(to,length);
  memcpy(to,buff,length);
  return to+length;
}

char *net_store_data(char *to,longlong from)
{
  char buff[22];
  uint length=(uint) (longlong10_to_str(from,buff,10)-buff);
  to=net_store_length(to,length);
  memcpy(to,buff,length);
  return to+length;
}

/*
  Function called by my_net_init() to set some check variables
*/

extern "C" {
void my_net_local_init(NET *net)
{
  net->max_packet=   (uint) global_system_variables.net_buffer_length;
  net->read_timeout= (uint) global_system_variables.net_read_timeout;
  net->write_timeout=(uint) global_system_variables.net_write_timeout;
  net->retry_count=  (uint) global_system_variables.net_retry_count;
  net->max_packet_size= max(global_system_variables.net_buffer_length,
			    global_system_variables.max_allowed_packet);
}
}


/*****************************************************************************
  Default Protocol functions
*****************************************************************************/

void Protocol::init(THD *thd_arg)
{
  thd=thd_arg;
  convert=thd->variables.convert_set;
  packet= &thd->packet;
#ifndef DEBUG_OFF
  field_types= 0;
#endif
}

/*
  Send name and type of result to client.

  SYNOPSIS
    send_fields()
    THD		Thread data object
    list	List of items to send to client
    convert	object used to convertation to another character set
    flag	Bit mask with the following functions:
		1 send number of rows
		2 send default values

  DESCRIPTION
    Sum fields has table name empty and field_name.
    Uses send_fields_convert() and send_fields() depending on
    if we have an active character set convert or not.

  RETURN VALUES
    0	ok
    1	Error  (Note that in this case the error is not sent to the client)
*/

bool Protocol::send_fields(List<Item> *list, uint flag)
{
  List_iterator_fast<Item> it(*list);
  Item *item;
  char buff[80];
  String tmp((char*) buff,sizeof(buff),default_charset_info);
  Protocol_simple prot(thd);
  String *packet= prot.storage_packet();
  DBUG_ENTER("send_fields");

  if (flag & 1)
  {				// Packet with number of elements
    char *pos=net_store_length(buff, (uint) list->elements);
    (void) my_net_write(&thd->net, buff,(uint) (pos-buff));
  }

#ifndef DEBUG_OFF
  field_types= (enum_field_types*) thd->alloc(sizeof(field_types) *
					      list->elements);
  uint count= 0;
#endif

  while ((item=it++))
  {
    char *pos;
    Send_field field;
    item->make_field(&field);
    prot.prepare_for_resend();

    if (thd->client_capabilities & CLIENT_PROTOCOL_41)
    {
      if (prot.store(field.db_name, (uint) strlen(field.db_name)) ||
	  prot.store(field.table_name, (uint) strlen(field.table_name)) ||
	  prot.store(field.org_table_name,
		     (uint) strlen(field.org_table_name)) ||
	  prot.store(field.col_name, (uint) strlen(field.col_name)) ||
	  prot.store(field.org_col_name, (uint) strlen(field.org_col_name)))
	goto err;
    }
    else
    {
      if (prot.store(field.table_name, (uint) strlen(field.table_name)) ||
	  prot.store(field.col_name, (uint) strlen(field.col_name)))
	goto err;
    }
    if (packet->realloc(packet->length()+10))
      goto err;
    pos= (char*) packet->ptr()+packet->length();

#ifdef TO_BE_DELETED_IN_6
    if (!(thd->client_capabilities & CLIENT_LONG_FLAG))
    {
      packet->length(packet->length()+9);
      pos[0]=3; int3store(pos+1,field.length);
      pos[4]=1; pos[5]=field.type;
      pos[6]=2; pos[7]=(char) field.flags; pos[8]= (char) field.decimals;
    }
    else
#endif
    {
      packet->length(packet->length()+10);
      pos[0]=3; int3store(pos+1,field.length);
      pos[4]=1; pos[5]=field.type;
      pos[6]=3; int2store(pos+7,field.flags); pos[9]= (char) field.decimals;
    }
    if (flag & 2)
      item->send(&prot, &tmp);			// Send default value
    if (prot.write())
      break;					/* purecov: inspected */
#ifndef DEBUG_OFF
    field_types[count++]= field.type;
#endif
  }

  send_eof(thd);
  DBUG_RETURN(prepare_for_send(list));

err:
  send_error(thd,ER_OUT_OF_RESOURCES);		/* purecov: inspected */
  DBUG_RETURN(1);				/* purecov: inspected */
}


bool Protocol::write()
{
  DBUG_ENTER("Protocol::write");
  DBUG_RETURN(my_net_write(&thd->net, packet->ptr(), packet->length()));
}


/*
  Send \0 end terminated string

  SYNOPSIS
    store()
    from	NullS or \0 terminated string

  NOTES
    In most cases one should use store(from, length) instead of this function

  RETURN VALUES
    0		ok
    1		error
*/

bool Protocol::store(const char *from)
{
  if (!from)
    return store_null();
  uint length= strlen(from);
  return store(from, length);
}


/*
  Send a set of strings as one long string with ',' in between
*/

bool Protocol::store(I_List<i_string>* str_list)
{
  char buf[256];
  String tmp(buf, sizeof(buf), default_charset_info);
  uint32 len;
  I_List_iterator<i_string> it(*str_list);
  i_string* s;

  tmp.length(0);
  while ((s=it++))
  {
    tmp.append(s->ptr);
    tmp.append(',');
  }
  if ((len= tmp.length()))
    len--;					// Remove last ','
  return store((char*) tmp.ptr(), len);
}


/****************************************************************************
  Functions to handle the simple (default) protocol where everything is
  This protocol is the one that is used by default between the MySQL server
  and client when you are not using prepared statements.

  All data are sent as 'packed-string-length' followed by 'string-data'

****************************************************************************/

void Protocol_simple::prepare_for_resend()
{
  packet->length(0);
#ifndef DEBUG_OFF
  field_pos= 0;
#endif
}

bool Protocol_simple::store_null()
{
#ifndef DEBUG_OFF
  field_pos++;
#endif
  char buff[1];
  buff[0]= (char)251;
  return packet->append(buff, sizeof(buff), PACKET_BUFFET_EXTRA_ALLOC);
}

bool Protocol_simple::store(const char *from, uint length)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DECIMAL ||
	      (field_types[field_pos] >= MYSQL_TYPE_ENUM &&
	       field_types[field_pos] <= MYSQL_TYPE_GEOMETRY));
  field_pos++;
#endif
  if (convert)
    return convert->store(packet, from, length);
  return net_store_data(packet, from, length);
}


bool Protocol_simple::store_tiny(longlong from)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 || field_types[field_pos++] == MYSQL_TYPE_TINY);
#endif
  char buff[20];
  return net_store_data(packet,(char*) buff,
			(uint) (int10_to_str((int) from,buff, -10)-buff));
}

bool Protocol_simple::store_short(longlong from)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos++] == MYSQL_TYPE_SHORT);
#endif
  char buff[20];
  return net_store_data(packet,(char*) buff,
			(uint) (int10_to_str((int) from,buff, -10)-buff));
}

bool Protocol_simple::store_long(longlong from)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 || field_types[field_pos++] == MYSQL_TYPE_LONG);
#endif
  char buff[20];
  return net_store_data(packet,(char*) buff,
			(uint) (int10_to_str((int) from,buff, -10)-buff));
}


bool Protocol_simple::store_longlong(longlong from, bool unsigned_flag)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos++] == MYSQL_TYPE_LONGLONG);
#endif
  char buff[22];
  return net_store_data(packet,(char*) buff,
			(uint) (longlong10_to_str(from,buff,
						  unsigned_flag ? 10 : -10)-
				buff));
}


bool Protocol_simple::store(float from, uint32 decimals, String *buffer)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos++] == MYSQL_TYPE_FLOAT);
#endif
  buffer->set((double) from, decimals, thd->variables.thd_charset);
  return net_store_data(packet,(char*) buffer->ptr(), buffer->length());
}

bool Protocol_simple::store(double from, uint32 decimals, String *buffer)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos++] == MYSQL_TYPE_DOUBLE);
#endif
  buffer->set(from, decimals, thd->variables.thd_charset);
  return net_store_data(packet,(char*) buffer->ptr(), buffer->length());
}


bool Protocol_simple::store(Field *field)
{
  if (field->is_null())
    return store_null();
#ifndef DEBUG_OFF
  field_pos++;
#endif
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),default_charset_info);
  field->val_str(&tmp,&tmp);
  if (convert)
    return convert->store(packet, tmp.ptr(), tmp.length());
  return net_store_data(packet, tmp.ptr(), tmp.length());
}


bool Protocol_simple::store(TIME *tm)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DATETIME ||
	      field_types[field_pos] == MYSQL_TYPE_TIMESTAMP);
  field_pos++;
#endif
  char buff[40];
  uint length;
  length= my_sprintf(buff,(buff, "%04d-%02d-%02d %02d:%02d:%02d",
			   (int) tm->year,
			   (int) tm->month,
			   (int) tm->day,
			   (int) tm->hour,
			   (int) tm->minute,
			   (int) tm->second));
  return net_store_data(packet, (char*) buff, length);
}


bool Protocol_simple::store_date(TIME *tm)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos++] == MYSQL_TYPE_DATE);
#endif
  char buff[40];
  uint length;
  length= my_sprintf(buff,(buff, "%04d-%02d-%02d",
			   (int) tm->year,
			   (int) tm->month,
			   (int) tm->day));
  return net_store_data(packet, (char*) buff, length);
}


bool Protocol_simple::store_time(TIME *tm)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos++] == MYSQL_TYPE_TIME);
#endif
  char buff[40];
  uint length;
  length= my_sprintf(buff,(buff, "%s%02ld:%02d:%02d",
			   tm->neg ? "-" : "",
			   (long) tm->day*3600L+(long) tm->hour,
			   (int) tm->minute,
			   (int) tm->second));
  return net_store_data(packet, (char*) buff, length);
}


/****************************************************************************
  Functions to handle the binary protocol used with prepared statements

  Data format:

   [ok:1]                            <-- reserved ok packet
   [null_field:(field_count+7+2)/8]  <-- reserved to send null data. The size is
                                         calculated using:
                                         bit_fields= (field_count+7+2)/8; 
                                         2 bits are reserved
   [[length]data]                    <-- data field (the length applies only for 
                                         string/binary/time/timestamp fields and 
                                         rest of them are not sent as they have 
                                         the default length that client understands
                                         based on the field type
   [..]..[[length]data]              <-- data
****************************************************************************/

bool Protocol_prep::prepare_for_send(List<Item> *item_list)
{
  field_count= item_list->elements;
  bit_fields= (field_count+9)/8;
  if (packet->alloc(bit_fields+1))
    return 1;
  /* prepare_for_resend will be called after this one */
  return 0;
}


void Protocol_prep::prepare_for_resend()
{
  packet->length(bit_fields+1);
  bzero((char*) packet->ptr(), 1+bit_fields);
  field_pos=0;
}


bool Protocol_prep::store(const char *from,uint length)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DECIMAL ||
	      (field_types[field_pos] >= MYSQL_TYPE_ENUM &&
	       field_types[field_pos] <= MYSQL_TYPE_GEOMETRY));
#endif
  field_pos++;
  if (convert)
    return convert->store(packet, from, length);
  return net_store_data(packet, from, length);
}


bool Protocol_prep::store_null()
{
  uint offset= (field_pos+2)/8+1, bit= (1 << ((field_pos+2) & 7));
  /* Room for this as it's allocated in prepare_for_send */
  char *to= (char*) packet->ptr()+offset;
  *to= (char) ((uchar) *to | (uchar) bit);
  field_pos++;
  return 0;
}


bool Protocol_prep::store_tiny(longlong from)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_TINY);
#endif
  char buff[1];
  field_pos++;
  buff[0]= (uchar) from;
  return packet->append(buff, sizeof(buff), PACKET_BUFFET_EXTRA_ALLOC);
}


bool Protocol_prep::store_short(longlong from)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_SHORT);
#endif
  field_pos++;
  char *to= packet->prep_append(2, PACKET_BUFFET_EXTRA_ALLOC);
  if (!to)
    return 1;
  int2store(to, (int) from);
  return 0;
}


bool Protocol_prep::store_long(longlong from)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_LONG);
#endif
  field_pos++;
  char *to= packet->prep_append(4, PACKET_BUFFET_EXTRA_ALLOC);
  if (!to)
    return 1;
  int4store(to, from);
  return 0;
}


bool Protocol_prep::store_longlong(longlong from, bool unsigned_flag)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_LONGLONG);
#endif
  field_pos++;
  char *to= packet->prep_append(8, PACKET_BUFFET_EXTRA_ALLOC);
  if (!to)
    return 1;
  int8store(to, from);
  return 0;
}


bool Protocol_prep::store(float from, uint32 decimals, String *buffer)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_FLOAT);
#endif
  field_pos++;
  char *to= packet->prep_append(4, PACKET_BUFFET_EXTRA_ALLOC);
  if (!to)
    return 1;
  float4store(to, from);
  return 0;
}


bool Protocol_prep::store(double from, uint32 decimals, String *buffer)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_DOUBLE);
#endif
  field_pos++;
  char *to= packet->prep_append(8, PACKET_BUFFET_EXTRA_ALLOC);
  if (!to)
    return 1;
  float8store(to, from);
  return 0;
}


bool Protocol_prep::store(Field *field)
{
  /*
    We should not count up field_pos here as send_binary() will call another
    protocol function to do this for us
  */
  if (field->is_null())
    return store_null();
  return field->send_binary(this);
}


bool Protocol_prep::store(TIME *tm)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
        field_types[field_pos] == MYSQL_TYPE_YEAR ||
	      field_types[field_pos] == MYSQL_TYPE_DATETIME ||
	      field_types[field_pos] == MYSQL_TYPE_DATE ||
	      field_types[field_pos] == MYSQL_TYPE_TIMESTAMP);
#endif
  char buff[12],*pos;
  uint length;
  field_pos++;
  pos= buff+1;
  
  int2store(pos, tm->year);
  int2store(pos+2, tm->month);
  int2store(pos+3, tm->day);
  int2store(pos+4, tm->hour);
  int2store(pos+5, tm->minute);
  int2store(pos+6, tm->second);
  int4store(pos+7, tm->second_part);
  if (tm->second_part)
    length=11;
  else if (tm->hour || tm->minute || tm->second)
    length=7;
  else if (tm->year || tm->month || tm->day)
    length=4;
  else
    length=0;
  buff[0]=(char) length;			// Length is stored first
  return packet->append(buff, length+1, PACKET_BUFFET_EXTRA_ALLOC);
}

bool Protocol_prep::store_date(TIME *tm)
{
  tm->hour= tm->minute= tm->second=0;
  tm->second_part= 0;
  return Protocol_prep::store(tm);
}


bool Protocol_prep::store_time(TIME *tm)
{
#ifndef DEBUG_OFF
  DBUG_ASSERT(field_types == 0 ||
	      field_types[field_pos] == MYSQL_TYPE_TIME);
#endif
  char buff[15],*pos;
  uint length;
  field_pos++;
  pos= buff+1;
  pos[0]= tm->neg ? 1 : 0;
  int4store(pos+1, tm->day);
  int2store(pos+5, tm->hour);
  int2store(pos+7, tm->minute);
  int2store(pos+9, tm->second);
  int4store(pos+11, tm->second_part);
  if (tm->second_part)
    length=14;
  else if (tm->hour || tm->minute || tm->second || tm->day)
    length=10;
  else
    length=0;
  buff[0]=(char) length;			// Length is stored first
  return packet->append(buff, length+1, PACKET_BUFFET_EXTRA_ALLOC);
}

#if 0
bool Protocol_prep::send_fields(List<Item> *list, uint flag) 
{
  return prepare_for_send(list);
};
#endif
