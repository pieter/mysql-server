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
  Functions to create a unireg form-file from a FIELD and a fieldname-fieldinfo
  struct.
  In the following functions FIELD * is an ordinary field-structure with
  the following exeptions:
    sc_length,typepos,row,kol,dtype,regnr and field need not to be set.
    str is a (long) to record position where 0 is the first position.
*/

#define USES_TYPES
#include "mysql_priv.h"
#include <m_ctype.h>
#include <assert.h>

#define FCOMP			17		/* Bytes for a packed field */

static uchar * pack_screens(List<create_field> &create_fields,
			    uint *info_length, uint *screens, bool small_file);
static uint pack_keys(uchar *keybuff,uint key_count, KEY *key_info,
                      ulong data_offset);
static bool pack_header(uchar *forminfo,enum db_type table_type,
			List<create_field> &create_fields,
			uint info_length, uint screens, uint table_options,
			ulong data_offset, handler *file);
static uint get_interval_id(uint *int_count,List<create_field> &create_fields,
			    create_field *last_field);
static bool pack_fields(File file, List<create_field> &create_fields,
                        ulong data_offset);
static bool make_empty_rec(THD *thd, int file, enum db_type table_type,
			   uint table_options,
			   List<create_field> &create_fields,
			   uint reclength, ulong data_offset,
                           handler *handler);

/*
  Create a frm (table definition) file

  SYNOPSIS
    mysql_create_frm()
    thd			Thread handler
    file_name		Name of file (including database and .frm)
    db                  Name of database
    table               Name of table
    create_info		create info parameters
    create_fields	Fields to create
    keys		number of keys to create
    key_info		Keys to create
    db_file		Handler to use. May be zero, in which case we use
    			create_info->db_type
  RETURN
    0  ok
    1  error
*/

bool mysql_create_frm(THD *thd, my_string file_name,
                      const char *db, const char *table,
		      HA_CREATE_INFO *create_info,
		      List<create_field> &create_fields,
		      uint keys, KEY *key_info,
		      handler *db_file)
{
  uint reclength,info_length,screens,key_info_length,maxlength;
  ulong key_buff_length;
  File file;
  ulong filepos, data_offset;
  uchar fileinfo[64],forminfo[288],*keybuff;
  TYPELIB formnames;
  uchar *screen_buff;
#ifdef HAVE_PARTITION_DB
  partition_info *part_info= thd->lex->part_info;
#endif
  DBUG_ENTER("mysql_create_frm");
#ifdef HAVE_PARTITION_DB
  thd->lex->part_info= NULL;
#endif

  formnames.type_names=0;
  if (!(screen_buff=pack_screens(create_fields,&info_length,&screens,0)))
    DBUG_RETURN(1);
  DBUG_ASSERT(db_file != NULL);

 /* If fixed row records, we need one bit to check for deleted rows */
  if (!(create_info->table_options & HA_OPTION_PACK_RECORD))
    create_info->null_bits++;
  data_offset= (create_info->null_bits + 7) / 8;

  if (pack_header(forminfo, create_info->db_type,create_fields,info_length,
		  screens, create_info->table_options,
                  data_offset, db_file))
  {
    my_free((gptr) screen_buff,MYF(0));
    if (thd->net.last_errno != ER_TOO_MANY_FIELDS)
      DBUG_RETURN(1);

    // Try again without UNIREG screens (to get more columns)
    thd->net.last_error[0]=0;
    if (!(screen_buff=pack_screens(create_fields,&info_length,&screens,1)))
      DBUG_RETURN(1);
    if (pack_header(forminfo, create_info->db_type, create_fields,info_length,
		    screens, create_info->table_options, data_offset, db_file))
    {
      my_free((gptr) screen_buff,MYF(0));
      DBUG_RETURN(1);
    }
  }
  reclength=uint2korr(forminfo+266);

  if ((file=create_frm(thd, file_name, db, table, reclength, fileinfo,
		       create_info, keys)) < 0)
  {
    my_free((gptr) screen_buff,MYF(0));
    DBUG_RETURN(1);
  }

  key_buff_length= uint4korr(fileinfo+47);
  keybuff=(uchar*) my_malloc(key_buff_length, MYF(0));
  key_info_length= pack_keys(keybuff, keys, key_info, data_offset);
  VOID(get_form_pos(file,fileinfo,&formnames));
  if (!(filepos=make_new_entry(file,fileinfo,&formnames,"")))
    goto err;
  maxlength=(uint) next_io_size((ulong) (uint2korr(forminfo)+1000));
  int2store(forminfo+2,maxlength);
  int4store(fileinfo+10,(ulong) (filepos+maxlength));
  fileinfo[26]= (uchar) test((create_info->max_rows == 1) &&
			     (create_info->min_rows == 1) && (keys == 0));
  int2store(fileinfo+28,key_info_length);
  strmake((char*) forminfo+47,create_info->comment ? create_info->comment : "",
	  60);
  forminfo[46]=(uchar) strlen((char*)forminfo+47);	// Length of comment

#ifdef HAVE_PARTITION_DB
  if (part_info)
  {
    int4store(fileinfo+55,part_info->part_info_len);
    fileinfo[61]= (uchar) part_info->default_engine_type;
  }
#endif
  int2store(fileinfo+59,db_file->extra_rec_buf_length());
  if (my_pwrite(file,(byte*) fileinfo,64,0L,MYF_RW) ||
      my_pwrite(file,(byte*) keybuff,key_info_length,
		(ulong) uint2korr(fileinfo+6),MYF_RW))
    goto err;
  VOID(my_seek(file,
	       (ulong) uint2korr(fileinfo+6)+ (ulong) key_buff_length,
	       MY_SEEK_SET,MYF(0)));
  if (make_empty_rec(thd,file,create_info->db_type,create_info->table_options,
		     create_fields,reclength, data_offset, db_file))
    goto err;
  if (create_info->connect_string.length)
  {
    char buff[2];
    int2store(buff,create_info->connect_string.length);
    if (my_write(file, buff, sizeof(buff), MYF(MY_NABP)) ||
        my_write(file, create_info->connect_string.str,
                 create_info->connect_string.length, MYF(MY_NABP)))
      goto err;
  }

  VOID(my_seek(file,filepos,MY_SEEK_SET,MYF(0)));
  if (my_write(file,(byte*) forminfo,288,MYF_RW) ||
      my_write(file,(byte*) screen_buff,info_length,MYF_RW) ||
      pack_fields(file, create_fields, data_offset))
    goto err;

#ifdef HAVE_PARTITION_DB
  if (part_info)
  {
    if (my_write(file, (byte*) part_info->part_info_string,
                 part_info->part_info_len, MYF_RW))
      goto err;
  }
#endif
#ifdef HAVE_CRYPTED_FRM
  if (create_info->password)
  {
    char tmp=2,*disk_buff=0;
    SQL_CRYPT *crypted=new SQL_CRYPT(create_info->password);
    if (!crypted || my_pwrite(file,&tmp,1,26,MYF_RW))	// Mark crypted
      goto err;
    uint read_length=uint2korr(forminfo)-256;
    VOID(my_seek(file,filepos+256,MY_SEEK_SET,MYF(0)));
    if (read_string(file,(gptr*) &disk_buff,read_length))
      goto err;
    crypted->encode(disk_buff,read_length);
    delete crypted;
    if (my_pwrite(file,disk_buff,read_length,filepos+256,MYF_RW))
    {
      my_free(disk_buff,MYF(0));
      goto err;
    }
    my_free(disk_buff,MYF(0));
  }
#endif

  my_free((gptr) screen_buff,MYF(0));
  my_free((gptr) keybuff, MYF(0));

  if (opt_sync_frm && !(create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      my_sync(file, MYF(MY_WME)))
    goto err2;
  if (my_close(file,MYF(MY_WME)))
    goto err3;

  {
    /* Unescape all UCS2 intervals: were escaped in pack_headers */
    List_iterator<create_field> it(create_fields);
    create_field *field;
    while ((field=it++))
    {
      if (field->interval && field->charset->mbminlen > 1)
        unhex_type2(field->interval);
    }
  }
  DBUG_RETURN(0);

err:
  my_free((gptr) screen_buff,MYF(0));
  my_free((gptr) keybuff, MYF(0));
err2:
  VOID(my_close(file,MYF(MY_WME)));
err3:
  my_delete(file_name,MYF(0));
  DBUG_RETURN(1);
} /* mysql_create_frm */


/*
  Create a frm (table definition) file and the tables

  SYNOPSIS
    rea_create_table()
    thd			Thread handler
    file_name		Name of file (including database and .frm)
    db                  Name of database
    table               Name of table
    create_info		create info parameters
    create_fields	Fields to create
    keys		number of keys to create
    key_info		Keys to create
    file		Handler to use.
  RETURN
    0  ok
    1  error
*/

int rea_create_table(THD *thd, my_string file_name,
                     const char *db, const char *table,
		     HA_CREATE_INFO *create_info,
		     List<create_field> &create_fields,
		     uint keys, KEY *key_info, handler *file)
{
  DBUG_ENTER("rea_create_table");

  if (mysql_create_frm(thd, file_name, db, table, create_info,
                       create_fields, keys, key_info, file))
    DBUG_RETURN(1);
  if (file->create_handler_files(file_name))
    goto err_handler;
  if (!create_info->frm_only && ha_create_table(file_name,create_info,0))
    goto err_handler;
  DBUG_RETURN(0);
err_handler:
  my_delete(file_name, MYF(0));
  DBUG_RETURN(1);
} /* rea_create_table */


	/* Pack screens to a screen for save in a form-file */

static uchar * pack_screens(List<create_field> &create_fields,
			    uint *info_length, uint *screens,
			    bool small_file)
{
  reg1 uint i;
  uint row,start_row,end_row,fields_on_screen;
  uint length,cols;
  uchar *info,*pos,*start_screen;
  uint fields=create_fields.elements;
  List_iterator<create_field> it(create_fields);
  DBUG_ENTER("pack_screens");

  start_row=4; end_row=22; cols=80; fields_on_screen=end_row+1-start_row;

  *screens=(fields-1)/fields_on_screen+1;
  length= (*screens) * (SC_INFO_LENGTH+ (cols>> 1)+4);

  create_field *field;
  while ((field=it++))
    length+=(uint) strlen(field->field_name)+1+TE_INFO_LENGTH+cols/2;

  if (!(info=(uchar*) my_malloc(length,MYF(MY_WME))))
    DBUG_RETURN(0);

  start_screen=0;
  row=end_row;
  pos=info;
  it.rewind();
  for (i=0 ; i < fields ; i++)
  {
    create_field *cfield=it++;
    if (row++ == end_row)
    {
      if (i)
      {
	length=(uint) (pos-start_screen);
	int2store(start_screen,length);
	start_screen[2]=(uchar) (fields_on_screen+1);
	start_screen[3]=(uchar) (fields_on_screen);
      }
      row=start_row;
      start_screen=pos;
      pos+=4;
      pos[0]= (uchar) start_row-2;	/* Header string */
      pos[1]= (uchar) (cols >> 2);
      pos[2]= (uchar) (cols >> 1) +1;
      strfill((my_string) pos+3,(uint) (cols >> 1),' ');
      pos+=(cols >> 1)+4;
    }
    length=(uint) strlen(cfield->field_name);
    if (length > cols-3)
      length=cols-3;

    if (!small_file)
    {
      pos[0]=(uchar) row;
      pos[1]=0;
      pos[2]=(uchar) (length+1);
      pos=(uchar*) strmake((char*) pos+3,cfield->field_name,length)+1;
    }
    cfield->row=(uint8) row;
    cfield->col=(uint8) (length+1);
    cfield->sc_length=(uint8) min(cfield->length,cols-(length+2));
  }
  length=(uint) (pos-start_screen);
  int2store(start_screen,length);
  start_screen[2]=(uchar) (row-start_row+2);
  start_screen[3]=(uchar) (row-start_row+1);

  *info_length=(uint) (pos-info);
  DBUG_RETURN(info);
} /* pack_screens */


	/* Pack keyinfo and keynames to keybuff for save in form-file. */

static uint pack_keys(uchar *keybuff, uint key_count, KEY *keyinfo,
                      ulong data_offset)
{
  uint key_parts,length;
  uchar *pos, *keyname_pos;
  KEY *key,*end;
  KEY_PART_INFO *key_part,*key_part_end;
  DBUG_ENTER("pack_keys");

  pos=keybuff+6;
  key_parts=0;
  for (key=keyinfo,end=keyinfo+key_count ; key != end ; key++)
  {
    int2store(pos, (key->flags ^ HA_NOSAME));
    int2store(pos+2,key->key_length);
    pos[4]= (uchar) key->key_parts;
    pos[5]= (uchar) key->algorithm;
    pos[6]=pos[7]=0;				// For the future
    pos+=8;
    key_parts+=key->key_parts;
    DBUG_PRINT("loop",("flags: %d  key_parts: %d at 0x%lx",
		       key->flags,key->key_parts,
		       key->key_part));
    for (key_part=key->key_part,key_part_end=key_part+key->key_parts ;
	 key_part != key_part_end ;
	 key_part++)

    {
      uint offset;
      DBUG_PRINT("loop",("field: %d  startpos: %lu  length: %ld",
			 key_part->fieldnr, key_part->offset + data_offset,
                         key_part->length));
      int2store(pos,key_part->fieldnr+1+FIELD_NAME_USED);
      offset= (uint) (key_part->offset+data_offset+1);
      int2store(pos+2, offset);
      pos[4]=0;					// Sort order
      int2store(pos+5,key_part->key_type);
      int2store(pos+7,key_part->length);
      pos+=9;
    }
  }
	/* Save keynames */
  keyname_pos=pos;
  *pos++=(uchar) NAMES_SEP_CHAR;
  for (key=keyinfo ; key != end ; key++)
  {
    uchar *tmp=(uchar*) strmov((char*) pos,key->name);
    *tmp++= (uchar) NAMES_SEP_CHAR;
    *tmp=0;
    pos=tmp;
  }
  *(pos++)=0;

  if (key_count > 127 || key_parts > 127)
  {
    keybuff[0]= (key_count & 0x7f) | 0x80;
    keybuff[1]= key_count >> 7;
    int2store(keybuff+2,key_parts);
  }
  else
  {
    keybuff[0]=(uchar) key_count;
    keybuff[1]=(uchar) key_parts;
    keybuff[2]= keybuff[3]= 0;
  }
  length=(uint) (pos-keyname_pos);
  int2store(keybuff+4,length);
  DBUG_RETURN((uint) (pos-keybuff));
} /* pack_keys */


	/* Make formheader */

static bool pack_header(uchar *forminfo, enum db_type table_type,
			List<create_field> &create_fields,
                        uint info_length, uint screens, uint table_options,
                        ulong data_offset, handler *file)
{
  uint length,int_count,int_length,no_empty, int_parts;
  uint time_stamp_pos,null_fields;
  ulong reclength, totlength, n_length, com_length;
  DBUG_ENTER("pack_header");

  if (create_fields.elements > MAX_FIELDS)
  {
    my_message(ER_TOO_MANY_FIELDS, ER(ER_TOO_MANY_FIELDS), MYF(0));
    DBUG_RETURN(1);
  }

  totlength= 0L;
  reclength= data_offset;
  no_empty=int_count=int_parts=int_length=time_stamp_pos=null_fields=
    com_length=0;
  n_length=2L;

	/* Check fields */

  List_iterator<create_field> it(create_fields);
  create_field *field;
  while ((field=it++))
  {
    totlength+= field->length;
    com_length+= field->comment.length;
    if (MTYP_TYPENR(field->unireg_check) == Field::NOEMPTY ||
	field->unireg_check & MTYP_NOEMPTY_BIT)
    {
      field->unireg_check= (Field::utype) ((uint) field->unireg_check |
					   MTYP_NOEMPTY_BIT);
      no_empty++;
    }
    /* 
      We mark first TIMESTAMP field with NOW() in DEFAULT or ON UPDATE 
      as auto-update field.
    */
    if (field->sql_type == FIELD_TYPE_TIMESTAMP &&
        MTYP_TYPENR(field->unireg_check) != Field::NONE &&
	!time_stamp_pos)
      time_stamp_pos= (uint) field->offset+ (uint) data_offset + 1;
    length=field->pack_length;
    /* Ensure we don't have any bugs when generating offsets */
    DBUG_ASSERT(reclength == field->offset + data_offset);
    if ((uint) field->offset+ (uint) data_offset+ length > reclength)
      reclength=(uint) (field->offset+ data_offset + length);
    n_length+= (ulong) strlen(field->field_name)+1;
    field->interval_id=0;
    if (field->interval)
    {
      uint old_int_count=int_count;

      if (field->charset->mbminlen > 1)
      {
        /* Escape UCS2 intervals using HEX notation */
        for (uint pos= 0; pos < field->interval->count; pos++)
        {
          char *dst;
          uint length= field->interval->type_lengths[pos], hex_length;
          const char *src= field->interval->type_names[pos];
          const char *srcend= src + length;
          hex_length= length * 2;
          field->interval->type_lengths[pos]= hex_length;
          field->interval->type_names[pos]= dst= sql_alloc(hex_length + 1);
          for ( ; src < srcend; src++)
          {
            *dst++= _dig_vec_upper[((uchar) *src) >> 4];
            *dst++= _dig_vec_upper[((uchar) *src) & 15];
          }
          *dst= '\0';
        }
      }

      field->interval_id=get_interval_id(&int_count,create_fields,field);
      if (old_int_count != int_count)
      {
	for (const char **pos=field->interval->type_names ; *pos ; pos++)
	  int_length+=(uint) strlen(*pos)+1;	// field + suffix prefix
	int_parts+=field->interval->count+1;
      }
    }
    if (f_maybe_null(field->pack_flag))
      null_fields++;
  }
  int_length+=int_count*2;			// 255 prefix + 0 suffix

	/* Save values in forminfo */

  if (reclength > (ulong) file->max_record_length())
  {
    my_error(ER_TOO_BIG_ROWSIZE, MYF(0), (uint) file->max_record_length());
    DBUG_RETURN(1);
  }
  /* Hack to avoid bugs with small static rows in MySQL */
  reclength=max(file->min_record_length(table_options),reclength);
  if (info_length+(ulong) create_fields.elements*FCOMP+288+
      n_length+int_length+com_length > 65535L || int_count > 255)
  {
    my_message(ER_TOO_MANY_FIELDS, ER(ER_TOO_MANY_FIELDS), MYF(0));
    DBUG_RETURN(1);
  }

  bzero((char*)forminfo,288);
  length=(info_length+create_fields.elements*FCOMP+288+n_length+int_length+
	  com_length);
  int2store(forminfo,length);
  forminfo[256] = (uint8) screens;
  int2store(forminfo+258,create_fields.elements);
  int2store(forminfo+260,info_length);
  int2store(forminfo+262,totlength);
  int2store(forminfo+264,no_empty);
  int2store(forminfo+266,reclength);
  int2store(forminfo+268,n_length);
  int2store(forminfo+270,int_count);
  int2store(forminfo+272,int_parts);
  int2store(forminfo+274,int_length);
  int2store(forminfo+276,time_stamp_pos);
  int2store(forminfo+278,80);			/* Columns needed */
  int2store(forminfo+280,22);			/* Rows needed */
  int2store(forminfo+282,null_fields);
  int2store(forminfo+284,com_length);
  /* Up to forminfo+288 is free to use for additional information */
  DBUG_RETURN(0);
} /* pack_header */


	/* get each unique interval each own id */

static uint get_interval_id(uint *int_count,List<create_field> &create_fields,
			    create_field *last_field)
{
  List_iterator<create_field> it(create_fields);
  create_field *field;
  TYPELIB *interval=last_field->interval;

  while ((field=it++) != last_field)
  {
    if (field->interval_id && field->interval->count == interval->count)
    {
      const char **a,**b;
      for (a=field->interval->type_names, b=interval->type_names ;
	   *a && !strcmp(*a,*b);
	   a++,b++) ;

      if (! *a)
      {
	return field->interval_id;		// Re-use last interval
      }
    }
  }
  return ++*int_count;				// New unique interval
}


	/* Save fields, fieldnames and intervals */

static bool pack_fields(File file, List<create_field> &create_fields,
                        ulong data_offset)
{
  reg2 uint i;
  uint int_count, comment_length=0;
  uchar buff[MAX_FIELD_WIDTH];
  create_field *field;
  DBUG_ENTER("pack_fields");

	/* Write field info */

  List_iterator<create_field> it(create_fields);

  int_count=0;
  while ((field=it++))
  {
    uint recpos;
    buff[0]= (uchar) field->row;
    buff[1]= (uchar) field->col;
    buff[2]= (uchar) field->sc_length;
    int2store(buff+3, field->length);
    /* The +1 is here becasue the col offset in .frm file have offset 1 */
    recpos= field->offset+1 + (uint) data_offset;
    int3store(buff+5,recpos);
    int2store(buff+8,field->pack_flag);
    int2store(buff+10,field->unireg_check);
    buff[12]= (uchar) field->interval_id;
    buff[13]= (uchar) field->sql_type; 
    if (field->sql_type == FIELD_TYPE_GEOMETRY)
    {
      buff[14]= (uchar) field->geom_type;
#ifndef HAVE_SPATIAL
      DBUG_ASSERT(0);                           // Should newer happen
#endif
    }
    else if (field->charset) 
      buff[14]= (uchar) field->charset->number;
    else
      buff[14]= 0;				// Numerical
    int2store(buff+15, field->comment.length);
    comment_length+= field->comment.length;
    set_if_bigger(int_count,field->interval_id);
    if (my_write(file,(byte*) buff,FCOMP,MYF_RW))
      DBUG_RETURN(1);
  }

	/* Write fieldnames */
  buff[0]=(uchar) NAMES_SEP_CHAR;
  if (my_write(file,(byte*) buff,1,MYF_RW))
    DBUG_RETURN(1);
  i=0;
  it.rewind();
  while ((field=it++))
  {
    char *pos= strmov((char*) buff,field->field_name);
    *pos++=NAMES_SEP_CHAR;
    if (i == create_fields.elements-1)
      *pos++=0;
    if (my_write(file,(byte*) buff,(uint) (pos-(char*) buff),MYF_RW))
      DBUG_RETURN(1);
    i++;
  }

	/* Write intervals */
  if (int_count)
  {
    String tmp((char*) buff,sizeof(buff), &my_charset_bin);
    tmp.length(0);
    it.rewind();
    int_count=0;
    while ((field=it++))
    {
      if (field->interval_id > int_count)
      {
	int_count=field->interval_id;
	tmp.append(NAMES_SEP_CHAR);
	for (const char **pos=field->interval->type_names ; *pos ; pos++)
	{
	  tmp.append(*pos);
	  tmp.append(NAMES_SEP_CHAR);
	}
	tmp.append('\0');			// End of intervall
      }
    }
    if (my_write(file,(byte*) tmp.ptr(),tmp.length(),MYF_RW))
      DBUG_RETURN(1);
  }
  if (comment_length)
  {
    it.rewind();
    int_count=0;
    while ((field=it++))
    {
      if (field->comment.length)
	if (my_write(file, (byte*) field->comment.str, field->comment.length,
		     MYF_RW))
	  DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}


	/* save an empty record on start of formfile */

static bool make_empty_rec(THD *thd, File file,enum db_type table_type,
			   uint table_options,
			   List<create_field> &create_fields,
			   uint reclength,
                           ulong data_offset,
                           handler *handler)
{
  int error;
  Field::utype type;
  uint null_count;
  uchar *buff,*null_pos;
  TABLE table;
  create_field *field;
  enum_check_fields old_count_cuted_fields= thd->count_cuted_fields;
  DBUG_ENTER("make_empty_rec");

  /* We need a table to generate columns for default values */
  bzero((char*) &table,sizeof(table));
  table.s= &table.share_not_to_be_used;

  if (!(buff=(uchar*) my_malloc((uint) reclength,MYF(MY_WME | MY_ZEROFILL))))
  {
    DBUG_RETURN(1);
  }

  table.in_use= thd;
  table.s->db_low_byte_first= handler->low_byte_first();
  table.s->blob_ptr_size= portable_sizeof_char_ptr;

  null_count=0;
  if (!(table_options & HA_OPTION_PACK_RECORD))
  {
    null_count++;			// Need one bit for delete mark
    *buff|= 1;
  }
  null_pos= buff;

  List_iterator<create_field> it(create_fields);
  thd->count_cuted_fields= CHECK_FIELD_WARN;    // To find wrong default values
  while ((field=it++))
  {
    /*
      regfield don't have to be deleted as it's allocated with sql_alloc()
    */
    Field *regfield=make_field((char*) buff+field->offset + data_offset,
                               field->length,
                               null_pos + null_count / 8,
			       null_count & 7,
			       field->pack_flag,
			       field->sql_type,
			       field->charset,
			       field->geom_type,
			       field->unireg_check,
			       field->interval,
			       field->field_name,
			       &table);
    if (!regfield)
      goto err;                                 // End of memory

    if (!(field->flags & NOT_NULL_FLAG))
    {
      *regfield->null_ptr|= regfield->null_bit;
      null_count++;
    }

    if (field->sql_type == FIELD_TYPE_BIT && !f_bit_as_char(field->pack_flag))
      null_count+= field->length & 7;

    type= (Field::utype) MTYP_TYPENR(field->unireg_check);

    if (field->def &&
	(regfield->real_type() != FIELD_TYPE_YEAR ||
	 field->def->val_int() != 0))
    {
      if (field->def->save_in_field(regfield, 1))
      {
        my_error(ER_INVALID_DEFAULT, MYF(0), regfield->field_name);
        error= 1;
        delete regfield; //To avoid memory leak
        goto err;
      }
    }
    else if (regfield->real_type() == FIELD_TYPE_ENUM &&
	     (field->flags & NOT_NULL_FLAG))
    {
      regfield->set_notnull();
      regfield->store((longlong) 1, TRUE);
    }
    else if (type == Field::YES)		// Old unireg type
      regfield->store(ER(ER_YES),(uint) strlen(ER(ER_YES)),system_charset_info);
    else if (type == Field::NO)			// Old unireg type
      regfield->store(ER(ER_NO), (uint) strlen(ER(ER_NO)),system_charset_info);
    else
      regfield->reset();
  }
  DBUG_ASSERT(data_offset == ((null_count + 7) / 8));

  /*
    We need to set the unused bits to 1. If the number of bits is a multiple
    of 8 there are no unused bits.
  */
  if (null_count & 7)
    *(null_pos + null_count / 8)|= ~(((uchar) 1 << (null_count & 7)) - 1);

  error=(int) my_write(file,(byte*) buff, (uint) reclength,MYF_RW);

err:
  my_free((gptr) buff,MYF(MY_FAE));
  thd->count_cuted_fields= old_count_cuted_fields;
  DBUG_RETURN(error);
} /* make_empty_rec */
