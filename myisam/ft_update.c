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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

/* functions to work with full-text indices */

#include "ftdefs.h"

/**************************************************************
   This is to make ft-code to ignore keyseg.length at all     *
   and to index the whole VARCHAR/BLOB instead...             */
#undef set_if_smaller
#define set_if_smaller(A,B)                          /* no op */
/**************************************************************/


/* parses a document i.e. calls ft_parse for every keyseg */
uint _mi_ft_parse(TREE *parsed, MI_INFO *info, uint keynr, const byte *record)
{
  byte *pos;
  uint i;
  MI_KEYSEG *keyseg=info->s->keyinfo[keynr].seg;

  for (i=info->s->keyinfo[keynr].keysegs-FT_SEGS ; i-- ; )
  {
    uint len;

    keyseg--;
    if (keyseg->null_bit && (record[keyseg->null_pos] & keyseg->null_bit))
	continue; /* NULL field */
    pos= (byte *)record+keyseg->start;
    if (keyseg->flag & HA_VAR_LENGTH)
    {
      len=uint2korr(pos);
      pos+=2;					 /* Skip VARCHAR length */
      set_if_smaller(len,keyseg->length);
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      len=_mi_calc_blob_length(keyseg->bit_start,pos);
      memcpy_fixed(&pos,pos+keyseg->bit_start,sizeof(char*));
      set_if_smaller(len,keyseg->length);
    }
    else
      len=keyseg->length;
    if (ft_parse(parsed, pos, len))
      return 1;
  }
  /* Handle the case where all columns are NULL */
  if (!is_tree_inited(parsed) && ft_parse(parsed, (byte*) "", 0))
    return 1;
  else
    return 0;
}

FT_WORD * _mi_ft_parserecord(MI_INFO *info, uint keynr, byte *keybuf,
				    const byte *record)
{
  TREE ptree;

  bzero(&ptree, sizeof(ptree));
  if (_mi_ft_parse(& ptree, info, keynr, record))
    return NULL;

  return ft_linearize(/*info, keynr, keybuf, */ & ptree);
}

static int _mi_ft_store(MI_INFO *info, uint keynr, byte *keybuf,
			FT_WORD *wlist, my_off_t filepos)
{
  uint key_length;

  while(wlist->pos)
  {
    key_length=_ft_make_key(info,keynr,keybuf,wlist,filepos);
    if (_mi_ck_write(info,keynr,(uchar*) keybuf,key_length))
      return 1;
    wlist++;
   }
   return 0;
}

static int _mi_ft_erase(MI_INFO *info, uint keynr, byte *keybuf, FT_WORD *wlist, my_off_t filepos)
{
  uint key_length, err=0;

  while(wlist->pos)
  {
    key_length=_ft_make_key(info,keynr,keybuf,wlist,filepos);
    if (_mi_ck_delete(info,keynr,(uchar*) keybuf,key_length))
      err=1;
    wlist++;
   }
   return err;
}

/* compares an appropriate parts of two WORD_KEY keys directly out of records */
/* returns 1 if they are different */

#define THOSE_TWO_DAMN_KEYS_ARE_REALLY_DIFFERENT 1
#define GEE_THEY_ARE_ABSOLUTELY_IDENTICAL	 0

int _mi_ft_cmp(MI_INFO *info, uint keynr, const byte *rec1, const byte *rec2)
{
  MI_KEYSEG *keyseg;
  byte *pos1, *pos2;
  uint i;

  i=info->s->keyinfo[keynr].keysegs-FT_SEGS;
  keyseg=info->s->keyinfo[keynr].seg;
  while(i--)
  {
    uint len1, len2;
    LINT_INIT(len1); LINT_INIT(len2);
    keyseg--;
    if (keyseg->null_bit)
    {
      if ( (rec1[keyseg->null_pos] ^ rec2[keyseg->null_pos])
	   & keyseg->null_bit )
	return THOSE_TWO_DAMN_KEYS_ARE_REALLY_DIFFERENT;
      if (rec1[keyseg->null_pos] & keyseg->null_bit )
	continue; /* NULL field */
    }
    pos1= (byte *)rec1+keyseg->start;
    pos2= (byte *)rec2+keyseg->start;
    if (keyseg->flag & HA_VAR_LENGTH)
    {
      len1=uint2korr(pos1);
      pos1+=2;					 /* Skip VARCHAR length */
      set_if_smaller(len1,keyseg->length);
      len2=uint2korr(pos2);
      pos2+=2;					 /* Skip VARCHAR length */
      set_if_smaller(len2,keyseg->length);
    }
    else if (keyseg->flag & HA_BLOB_PART)
    {
      len1=_mi_calc_blob_length(keyseg->bit_start,pos1);
      memcpy_fixed(&pos1,pos1+keyseg->bit_start,sizeof(char*));
      set_if_smaller(len1,keyseg->length);
      len2=_mi_calc_blob_length(keyseg->bit_start,pos2);
      memcpy_fixed(&pos2,pos2+keyseg->bit_start,sizeof(char*));
      set_if_smaller(len2,keyseg->length);
    }
    else /* fixed length key */
    {
      len1=len2=keyseg->length;
    }
    if ((len1 != len2) || memcmp(pos1, pos2, len1))
      return THOSE_TWO_DAMN_KEYS_ARE_REALLY_DIFFERENT;
  }
  return GEE_THEY_ARE_ABSOLUTELY_IDENTICAL;
}

/* update a document entry */
int _mi_ft_update(MI_INFO *info, uint keynr, byte *keybuf,
                  const byte *oldrec, const byte *newrec, my_off_t pos)
{
  int error= -1;
  FT_WORD *oldlist,*newlist, *old_word, *new_word;
  uint key_length;
  int cmp, cmp2;

  if (!(old_word=oldlist=_mi_ft_parserecord(info, keynr, keybuf, oldrec)))
    goto err0;
  if (!(new_word=newlist=_mi_ft_parserecord(info, keynr, keybuf, newrec)))
    goto err1;

  error=0;
  while(old_word->pos && new_word->pos)
  {
    cmp=_mi_compare_text(default_charset_info,
	                 (uchar*) old_word->pos,old_word->len,
			 (uchar*) new_word->pos,new_word->len,0);
    cmp2= cmp ? 0 : (abs(old_word->weight - new_word->weight) > 1.e-5);

    if (cmp < 0 || cmp2)
    {
      key_length=_ft_make_key(info,keynr,keybuf,old_word,pos);
      if ((error=_mi_ck_delete(info,keynr,(uchar*) keybuf,key_length)))
        goto err2;
    }
    if (cmp > 0 || cmp2)
    {
      key_length=_ft_make_key(info,keynr,keybuf,new_word,pos);
      if ((error=_mi_ck_write(info,keynr,(uchar*) keybuf,key_length)))
        goto err2;
    }
    if (cmp<=0) old_word++;
    if (cmp>=0) new_word++;
 }
 if (old_word->pos)
   error=_mi_ft_erase(info,keynr,keybuf,old_word,pos);
 else if (new_word->pos)
   error=_mi_ft_store(info,keynr,keybuf,new_word,pos);

err2:
    my_free((char*) newlist,MYF(0));
err1:
    my_free((char*) oldlist,MYF(0));
err0:
  return error;
}

/* adds a document to the collection */
int _mi_ft_add(MI_INFO *info, uint keynr, byte *keybuf, const byte *record,
	       my_off_t pos)
{
  int error= -1;
  FT_WORD *wlist;

  if ((wlist=_mi_ft_parserecord(info, keynr, keybuf, record)))
  {
    error=_mi_ft_store(info,keynr,keybuf,wlist,pos);
    my_free((char*) wlist,MYF(0));
  }
  return error;
}

/* removes a document from the collection */
int _mi_ft_del(MI_INFO *info, uint keynr, byte *keybuf, const byte *record,
	       my_off_t pos)
{
  int error= -1;
  FT_WORD *wlist;
  if ((wlist=_mi_ft_parserecord(info, keynr, keybuf, record)))
  {
    error=_mi_ft_erase(info,keynr,keybuf,wlist,pos);
    my_free((char*) wlist,MYF(0));
  }
  return error;
}

uint _ft_make_key(MI_INFO *info, uint keynr, byte *keybuf, FT_WORD *wptr,
		  my_off_t filepos)
{
  byte buf[HA_FT_MAXLEN+16];

#if HA_FT_WTYPE == HA_KEYTYPE_FLOAT
  float weight=(float) ((filepos==HA_OFFSET_ERROR) ? 0 : wptr->weight);
  mi_float4store(buf,weight);
#else
#error
#endif

#ifdef EVAL_RUN
  *(buf+HA_FT_WLEN)=wptr->cnt;
  int2store(buf+HA_FT_WLEN+1,wptr->len);
  memcpy(buf+HA_FT_WLEN+3,wptr->pos,wptr->len);
#else /* EVAL_RUN */
  int2store(buf+HA_FT_WLEN,wptr->len);
  memcpy(buf+HA_FT_WLEN+2,wptr->pos,wptr->len);
#endif /* EVAL_RUN */
  return _mi_make_key(info,keynr,(uchar*) keybuf,buf,filepos);
}
