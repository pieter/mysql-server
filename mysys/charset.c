/* Copyright (C) 2000 MySQL AB

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

#include "mysys_priv.h"
#include "mysys_err.h"
#include <m_ctype.h>
#include <m_string.h>
#include <my_dir.h>
#include <my_xml.h>


/*

  The code below implements this functionality:
  
    - Initializing charset related structures
    - Loading dynamic charsets
    - Searching for a proper CHARSET_INFO 
      using charset name, collation name or collatio ID
    - Setting server default character set
*/


static void set_max_sort_char(CHARSET_INFO *cs)
{
  uchar max_char;
  uint  i;
  
  if (!cs->sort_order)
    return;
  
  max_char=cs->sort_order[(uchar) cs->max_sort_char];
  for (i= 0; i < 256; i++)
  {
    if ((uchar) cs->sort_order[i] > max_char)
    {
      max_char=(uchar) cs->sort_order[i];
      cs->max_sort_char= (char) i;
    }
  }
}


static void simple_cs_init_functions(CHARSET_INFO *cs)
{
  
  if (cs->state & MY_CS_BINSORT)
  {
    CHARSET_INFO *b= &my_charset_bin;
    cs->strnxfrm    = b->strnxfrm;
    cs->like_range  = b->like_range;
    cs->wildcmp     = b->wildcmp;
    cs->strnncoll   = b->strnncoll;
    cs->strnncollsp = b->strnncollsp;
    cs->tosort      = b->tosort;
    cs->strcasecmp  = b->strcasecmp;
    cs->strncasecmp = b->strncasecmp;
    cs->hash_caseup = b->hash_caseup;
    cs->hash_sort   = b->hash_sort;
  }
  else
  {
    cs->strnxfrm    = my_strnxfrm_simple;
    cs->like_range  = my_like_range_simple;
    cs->wildcmp     = my_wildcmp_8bit;
    cs->strnncoll   = my_strnncoll_simple;
    cs->strnncollsp = my_strnncollsp_simple;
    cs->tosort      = my_tosort_8bit;
    cs->strcasecmp  = my_strcasecmp_8bit;
    cs->strncasecmp = my_strncasecmp_8bit;
    cs->hash_caseup = my_hash_caseup_simple;
    cs->hash_sort   = my_hash_sort_simple;
  }
  
  cs->caseup_str  = my_caseup_str_8bit;
  cs->casedn_str  = my_casedn_str_8bit;
  cs->caseup      = my_caseup_8bit;
  cs->casedn      = my_casedn_8bit;
  cs->mb_wc       = my_mb_wc_8bit;
  cs->wc_mb       = my_wc_mb_8bit;
  cs->snprintf	  = my_snprintf_8bit;
  cs->long10_to_str= my_long10_to_str_8bit;
  cs->longlong10_to_str= my_longlong10_to_str_8bit;
  cs->fill	  = my_fill_8bit;
  cs->strntol     = my_strntol_8bit;
  cs->strntoul    = my_strntoul_8bit;
  cs->strntoll    = my_strntoll_8bit;
  cs->strntoull   = my_strntoull_8bit;
  cs->strntod     = my_strntod_8bit;
  cs->scan	  = my_scan_8bit;
  cs->mbmaxlen    = 1;
  cs->numchars    = my_numchars_8bit;
  cs->charpos     = my_charpos_8bit;
}


typedef struct
{
  int		nchars;
  MY_UNI_IDX	uidx;
} uni_idx;

#define PLANE_SIZE	0x100
#define PLANE_NUM	0x100
#define PLANE_NUMBER(x)	(((x)>>8) % PLANE_NUM)

static int pcmp(const void * f, const void * s)
{
  const uni_idx *F= (const uni_idx*) f;
  const uni_idx *S= (const uni_idx*) s;
  int res;

  if (!(res=((S->nchars)-(F->nchars))))
    res=((F->uidx.from)-(S->uidx.to));
  return res;
}


static my_bool create_fromuni(CHARSET_INFO *cs)
{
  uni_idx	idx[PLANE_NUM];
  int		i,n;
  
  /* Clear plane statistics */
  bzero(idx,sizeof(idx));
  
  /* Count number of characters in each plane */
  for (i=0; i< 0x100; i++)
  {
    uint16 wc=cs->tab_to_uni[i];
    int pl= PLANE_NUMBER(wc);
    
    if (wc || !i)
    {
      if (!idx[pl].nchars)
      {
        idx[pl].uidx.from=wc;
        idx[pl].uidx.to=wc;
      }else
      {
        idx[pl].uidx.from=wc<idx[pl].uidx.from?wc:idx[pl].uidx.from;
        idx[pl].uidx.to=wc>idx[pl].uidx.to?wc:idx[pl].uidx.to;
      }
      idx[pl].nchars++;
    }
  }
  
  /* Sort planes in descending order */
  qsort(&idx,PLANE_NUM,sizeof(uni_idx),&pcmp);
  
  for (i=0; i < PLANE_NUM; i++)
  {
    int ch,numchars;
    
    /* Skip empty plane */
    if (!idx[i].nchars)
      break;
    
    numchars=idx[i].uidx.to-idx[i].uidx.from+1;
    idx[i].uidx.tab=(unsigned char*)my_once_alloc(numchars *
						  sizeof(*idx[i].uidx.tab),
						  MYF(MY_WME));
    bzero(idx[i].uidx.tab,numchars*sizeof(*idx[i].uidx.tab));
    
    for (ch=1; ch < PLANE_SIZE; ch++)
    {
      uint16 wc=cs->tab_to_uni[ch];
      if (wc >= idx[i].uidx.from && wc <= idx[i].uidx.to && wc)
      {
        int ofs= wc - idx[i].uidx.from;
        idx[i].uidx.tab[ofs]= ch;
      }
    }
  }
  
  /* Allocate and fill reverse table for each plane */
  n=i;
  cs->tab_from_uni= (MY_UNI_IDX*) my_once_alloc(sizeof(MY_UNI_IDX)*(n+1),
					       MYF(MY_WME));
  for (i=0; i< n; i++)
    cs->tab_from_uni[i]= idx[i].uidx;
  
  /* Set end-of-list marker */
  bzero(&cs->tab_from_uni[i],sizeof(MY_UNI_IDX));
  return FALSE;
}


static void simple_cs_copy_data(CHARSET_INFO *to, CHARSET_INFO *from)
{
  to->number= from->number ? from->number : to->number;
  to->state|= from->state;

  if (from->csname)
    to->csname= my_once_strdup(from->csname,MYF(MY_WME));
  
  if (from->name)
    to->name= my_once_strdup(from->name,MYF(MY_WME));
  
  if (from->ctype)
    to->ctype= (uchar*) my_once_memdup((char*) from->ctype,
				       MY_CS_CTYPE_TABLE_SIZE, MYF(MY_WME));
  if (from->to_lower)
    to->to_lower= (uchar*) my_once_memdup((char*) from->to_lower,
					  MY_CS_TO_LOWER_TABLE_SIZE, MYF(MY_WME));
  if (from->to_upper)
    to->to_upper= (uchar*) my_once_memdup((char*) from->to_upper,
					  MY_CS_TO_UPPER_TABLE_SIZE, MYF(MY_WME));
  if (from->sort_order)
  {
    to->sort_order= (uchar*) my_once_memdup((char*) from->sort_order,
					    MY_CS_SORT_ORDER_TABLE_SIZE,
					    MYF(MY_WME));
    set_max_sort_char(to);
  }
  if (from->tab_to_uni)
  {
    uint sz= MY_CS_TO_UNI_TABLE_SIZE*sizeof(uint16);
    to->tab_to_uni= (uint16*)  my_once_memdup((char*)from->tab_to_uni, sz,
					     MYF(MY_WME));
    create_fromuni(to);
  }
  to->mbmaxlen= 1;
}


static my_bool simple_cs_is_full(CHARSET_INFO *cs)
{
  return ((cs->csname && cs->tab_to_uni && cs->ctype && cs->to_upper &&
	   cs->to_lower) &&
	  (cs->number && cs->name &&
	  (cs->sort_order || (cs->state & MY_CS_BINSORT) )));
}


static int add_collation(CHARSET_INFO *cs)
{
  if (cs->name && (cs->number || (cs->number=get_charset_number(cs->name))))
  {
    if (!all_charsets[cs->number])
    {
      if (!(all_charsets[cs->number]=
         (CHARSET_INFO*) my_once_alloc(sizeof(CHARSET_INFO),MYF(0))))
        return MY_XML_ERROR;
      bzero((void*)all_charsets[cs->number],sizeof(CHARSET_INFO));
    }
    
    if (cs->primary_number == cs->number)
      cs->state |= MY_CS_PRIMARY;
      
    if (cs->primary_number == cs->number)
      cs->state |= MY_CS_BINSORT;
    
    if (!(all_charsets[cs->number]->state & MY_CS_COMPILED))
    {
      simple_cs_copy_data(all_charsets[cs->number],cs);
      if (simple_cs_is_full(all_charsets[cs->number]))
      {
        simple_cs_init_functions(all_charsets[cs->number]);
        all_charsets[cs->number]->state |= MY_CS_LOADED;
      }
    }
    else
    {
      all_charsets[cs->number]->state |= cs->state;
    }
    cs->number= 0;
    cs->name= NULL;
    cs->state= 0;
    cs->sort_order= NULL;
    cs->state= 0;
  }
  return MY_XML_OK;
}


#define MAX_BUF 1024*16
#define MY_CHARSET_INDEX "Index.xml"

const char *charsets_dir= NULL;
static int charset_initialized=0;


static my_bool my_read_charset_file(const char *filename, myf myflags)
{
  char *buf;
  int  fd;
  uint len;
  
  if (!(buf= (char *)my_malloc(MAX_BUF,myflags)))
    return FALSE;
  
  if ((fd=my_open(filename,O_RDONLY,myflags)) < 0)
  {
    my_free(buf,myflags);
    return TRUE;
  }
  len=read(fd,buf,MAX_BUF);
  my_close(fd,myflags);
  
  if (my_parse_charset_xml(buf,len,add_collation))
  {
#ifdef NOT_YET
    printf("ERROR at line %d pos %d '%s'\n",
	   my_xml_error_lineno(&p)+1,
	   my_xml_error_pos(&p),
	   my_xml_error_string(&p));
#endif
  }
  
  my_free(buf, myflags);  
  return FALSE;
}


char *get_charsets_dir(char *buf)
{
  const char *sharedir= SHAREDIR;
  DBUG_ENTER("get_charsets_dir");

  if (charsets_dir != NULL)
    strmake(buf, charsets_dir, FN_REFLEN-1);
  else
  {
    if (test_if_hard_path(sharedir) ||
	is_prefix(sharedir, DEFAULT_CHARSET_HOME))
      strxmov(buf, sharedir, "/", CHARSET_DIR, NullS);
    else
      strxmov(buf, DEFAULT_CHARSET_HOME, "/", sharedir, "/", CHARSET_DIR,
	      NullS);
  }
  convert_dirname(buf,buf,NullS);
  DBUG_PRINT("info",("charsets dir: '%s'", buf));
  DBUG_RETURN(strend(buf));
}

CHARSET_INFO *all_charsets[256];
CHARSET_INFO *default_charset_info = &my_charset_latin1;
CHARSET_INFO *system_charset_info  = &my_charset_latin1;

#define MY_ADD_CHARSET(x)	all_charsets[(x)->number]=(x)


static my_bool init_compiled_charsets(myf flags __attribute__((unused)))
{
  CHARSET_INFO *cs;

  MY_ADD_CHARSET(&my_charset_latin1);
  
  MY_ADD_CHARSET(&my_charset_bin);

#ifdef HAVE_CHARSET_big5
  MY_ADD_CHARSET(&my_charset_big5);
#endif

#ifdef HAVE_CHARSET_czech
  MY_ADD_CHARSET(&my_charset_czech);
#endif

#ifdef HAVE_CHARSET_euc_kr
  MY_ADD_CHARSET(&my_charset_euc_kr);
#endif

#ifdef HAVE_CHARSET_gb2312
  MY_ADD_CHARSET(&my_charset_gb2312);
#endif

#ifdef HAVE_CHARSET_gbk
  MY_ADD_CHARSET(&my_charset_gbk);
#endif

#ifdef HAVE_CHARSET_latin1_de
  MY_ADD_CHARSET(&my_charset_latin1_de);
#endif

#ifdef HAVE_CHARSET_sjis
  MY_ADD_CHARSET(&my_charset_sjis);
#endif

#ifdef HAVE_CHARSET_tis620
  MY_ADD_CHARSET(&my_charset_tis620);
#endif

#ifdef HAVE_CHARSET_ucs2
  MY_ADD_CHARSET(&my_charset_ucs2);
#endif

#ifdef HAVE_CHARSET_ujis
  MY_ADD_CHARSET(&my_charset_ujis);
#endif

#ifdef HAVE_CHARSET_utf8
  MY_ADD_CHARSET(&my_charset_utf8);
#endif

#ifdef HAVE_CHARSET_win1250ch
  MY_ADD_CHARSET(&my_charset_win1250ch);
#endif

  /* Copy compiled charsets */
  for (cs=compiled_charsets; cs->name; cs++)
  {
    all_charsets[cs->number]=cs;
  }
  
  return FALSE;
}

#ifdef __NETWARE__
my_bool STDCALL init_available_charsets(myf myflags)
#else
static my_bool init_available_charsets(myf myflags)
#endif
{
  char fname[FN_REFLEN];
  my_bool error=FALSE;
  /*
    We have to use charset_initialized to not lock on THR_LOCK_charset
    inside get_internal_charset...
  */
  if (!charset_initialized)
  {
    CHARSET_INFO **cs;
    /*
      To make things thread safe we are not allowing other threads to interfere
      while we may changing the cs_info_table
    */
    pthread_mutex_lock(&THR_LOCK_charset);

    bzero(&all_charsets,sizeof(all_charsets));
    init_compiled_charsets(myflags);
    
    /* Copy compiled charsets */
    for (cs=all_charsets; cs < all_charsets+255 ; cs++)
    {
      if (*cs)
        set_max_sort_char(*cs);
    }
    
    strmov(get_charsets_dir(fname), MY_CHARSET_INDEX);
    error= my_read_charset_file(fname,myflags);
    charset_initialized=1;
    pthread_mutex_unlock(&THR_LOCK_charset);
  }
  return error;
}


void free_charsets(void)
{
  charset_initialized=0;
}


static void get_charset_conf_name(const char *cs_name, char *buf)
{
  strxmov(get_charsets_dir(buf), cs_name, ".conf", NullS);
}


uint get_charset_number(const char *charset_name)
{
  CHARSET_INFO **cs;
  if (init_available_charsets(MYF(0)))	/* If it isn't initialized */
    return 0;
  
  for (cs= all_charsets; cs < all_charsets+255; ++cs)
  {
    if ( cs[0] && cs[0]->name && !strcmp(cs[0]->name, charset_name))
      return cs[0]->number;
  }  
  return 0;   /* this mimics find_type() */
}


const char *get_charset_name(uint charset_number)
{
  CHARSET_INFO *cs;
  if (init_available_charsets(MYF(0)))	/* If it isn't initialized */
    return "?";

  cs=all_charsets[charset_number];
  if (cs && (cs->number == charset_number) && cs->name )
    return (char*) cs->name;
  
  return (char*) "?";   /* this mimics find_type() */
}


static CHARSET_INFO *get_internal_charset(uint cs_number, myf flags)
{
  char  buf[FN_REFLEN];
  CHARSET_INFO *cs;
  /*
    To make things thread safe we are not allowing other threads to interfere
    while we may changing the cs_info_table
  */
  pthread_mutex_lock(&THR_LOCK_charset);

  cs= all_charsets[cs_number];

  if (cs && !(cs->state & (MY_CS_COMPILED | MY_CS_LOADED)))
  {
     strxmov(get_charsets_dir(buf), cs->csname, ".xml", NullS);
     my_read_charset_file(buf,flags);
     cs= (cs->state & MY_CS_LOADED) ? cs : NULL;
  }
  pthread_mutex_unlock(&THR_LOCK_charset);
  return cs;
}


CHARSET_INFO *get_charset(uint cs_number, myf flags)
{
  CHARSET_INFO *cs;
  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */
  
  if (!cs_number)
    return NULL;
  
  cs=get_internal_charset(cs_number, flags);

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN], cs_string[23];
    strmov(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    cs_string[0]='#';
    int10_to_str(cs_number, cs_string+1, 10);
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_string, index_file);
  }
  return cs;
}

my_bool set_default_charset(uint cs, myf flags)
{
  CHARSET_INFO *new_charset;
  DBUG_ENTER("set_default_charset");
  DBUG_PRINT("enter",("character set: %d",(int) cs));

  new_charset= get_charset(cs, flags);
  if (!new_charset)
  {
    DBUG_PRINT("error",("Couldn't set default character set"));
    DBUG_RETURN(TRUE);   /* error */
  }
  default_charset_info= new_charset;
  system_charset_info= new_charset;

  DBUG_RETURN(FALSE);
}

CHARSET_INFO *get_charset_by_name(const char *cs_name, myf flags)
{
  uint cs_number;
  CHARSET_INFO *cs;
  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */

  cs_number=get_charset_number(cs_name);
  cs= cs_number ? get_internal_charset(cs_number,flags) : NULL;

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN];
    strmov(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_name, index_file);
  }

  return cs;
}


CHARSET_INFO *get_charset_by_csname(const char *cs_name,
				    uint cs_flags,
				    myf flags)
{
  CHARSET_INFO *cs=NULL;
  CHARSET_INFO **css;
  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */
  
  for (css= all_charsets; css < all_charsets+255; ++css)
  {
    if ( css[0] && (css[0]->state & cs_flags) && 
         css[0]->csname && !strcmp(css[0]->csname, cs_name))
    {
      cs= css[0]->number ? get_internal_charset(css[0]->number,flags) : NULL;
      break;
    }
  }  
  
  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN];
    strmov(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_name, index_file);
  }

  return cs;
}


my_bool set_default_charset_by_name(const char *cs_name, myf flags)
{
  CHARSET_INFO *new_charset;
  DBUG_ENTER("set_default_charset_by_name");
  DBUG_PRINT("enter",("character set: %s", cs_name));

  new_charset= get_charset_by_name(cs_name, flags);
  if (!new_charset)
  {
    DBUG_PRINT("error",("Couldn't set default character set"));
    DBUG_RETURN(TRUE);   /* error */
  }

  default_charset_info= new_charset;
  system_charset_info= new_charset;
  DBUG_RETURN(FALSE);
}


/* Only append name if it doesn't exist from before */

static my_bool charset_in_string(const char *name, DYNAMIC_STRING *s)
{
  uint length= (uint) strlen(name);
  const char *pos;
  for (pos=s->str ; (pos=strstr(pos,name)) ; pos++)
  {
    if (! pos[length] || pos[length] == ' ')
      return TRUE;				/* Already existed */
  }
  return FALSE;
}


static void charset_append(DYNAMIC_STRING *s, const char *name)
{
  if (!charset_in_string(name, s))
  {
    dynstr_append(s, name);
    dynstr_append(s, " ");
  }
}


/* Returns a dynamically-allocated string listing the character sets
   requested.  The caller is responsible for freeing the memory. */

char *list_charsets(myf want_flags)
{
  DYNAMIC_STRING s;
  char *p;

  (void)init_available_charsets(MYF(0));
  init_dynamic_string(&s, NullS, 256, 1024);

  if (want_flags & MY_CS_COMPILED)
  {
    CHARSET_INFO **cs;
    for (cs= all_charsets; cs < all_charsets+255; cs++)
    {
      if (cs[0])
      {
        dynstr_append(&s, cs[0]->name);
        dynstr_append(&s, " ");
      }
    }
  }

  if (want_flags & MY_CS_CONFIG)
  {
    CHARSET_INFO **cs;
    char buf[FN_REFLEN];
    MY_STAT status;

    for (cs=all_charsets; cs < all_charsets+255; cs++)
    {
      if (!cs[0] || !cs[0]->name || charset_in_string(cs[0]->name, &s))
	continue;
      get_charset_conf_name(cs[0]->name, buf);
      if (!my_stat(buf, &status, MYF(0)))
	continue;       /* conf file doesn't exist */
      dynstr_append(&s, cs[0]->name);
      dynstr_append(&s, " ");
    }
  }

  if (want_flags & (MY_CS_INDEX|MY_CS_LOADED))
  {
    CHARSET_INFO **cs;
    for (cs= all_charsets; cs < all_charsets + 255; cs++)
      if (cs[0] && cs[0]->name && (cs[0]->state & want_flags) )
        charset_append(&s, cs[0]->name);
  }
  
  if (s.length)
  {
    s.str[s.length - 1]= '\0';   /* chop trailing space */
    p= my_strdup(s.str, MYF(MY_WME));
  }
  else
  {
    p= my_strdup("", MYF(MY_WME));
  }
  dynstr_free(&s);
  
  return p;
}


