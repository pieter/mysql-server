/* Copyright (C) 2002 MySQL AB & tommy@valley.ne.jp.
   
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

/* This file is for binary pseudo charset, created by bar@mysql.com */


#include <my_global.h>
#include "m_string.h"
#include "m_ctype.h"

static uchar ctype_bin[]=
{
  0,
  32, 32, 32, 32, 32, 32, 32, 32, 32, 40, 40, 40, 40, 40, 32, 32,
  32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
  72, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
  132,132,132,132,132,132,132,132,132,132, 16, 16, 16, 16, 16, 16,
  16,129,129,129,129,129,129,  1,  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 16, 16, 16, 16, 16,
  16,130,130,130,130,130,130,  2,  2,  2,  2,  2,  2,  2,  2,  2,
  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 16, 16, 16, 16, 32,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  72, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1, 16,  1,  1,  1,  1,  1,  1,  1,  2,
  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
  2,  2,  2,  2,  2,  2,  2, 16,  2,  2,  2,  2,  2,  2,  2,  2
};


/* Dummy array for toupper / tolower / sortorder */

static uchar bin_char_array[] =
{
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};


static int my_strnncoll_binary(CHARSET_INFO * cs __attribute__((unused)),
				const uchar *s, uint slen,
				const uchar *t, uint tlen)
{
  int cmp= memcmp(s,t,min(slen,tlen));
  return cmp ? cmp : (int) (slen - tlen);
}

static int my_strnncollsp_binary(CHARSET_INFO * cs,
                               const uchar *s, uint slen,
                               const uchar *t, uint tlen)
{
  int len, cmp;

  for ( ; slen && my_isspace(cs, s[slen-1]) ; slen--);
  for ( ; tlen && my_isspace(cs, t[tlen-1]) ; tlen--);

  len  = ( slen > tlen ) ? tlen : slen;

  cmp= memcmp(s,t,len);
  return cmp ? cmp : (int) (slen - tlen);
}

static void my_caseup_str_bin(CHARSET_INFO *cs __attribute__((unused)),
		       char *str __attribute__((unused)))
{
}

static void my_casedn_str_bin(CHARSET_INFO * cs __attribute__((unused)),
		       char *str __attribute__((unused)))
{
}

static void my_caseup_bin(CHARSET_INFO * cs __attribute__((unused)),
		   char *str __attribute__((unused)),
		   uint length __attribute__((unused)))
{
}

static void my_casedn_bin(CHARSET_INFO * cs __attribute__((unused)),
		   char *str __attribute__((unused)),
		   uint length  __attribute__((unused)))
{
}

static void my_tosort_bin(CHARSET_INFO * cs __attribute__((unused)),
		   char *str __attribute__((unused)),
		   uint length  __attribute__((unused)))
{
}

static int my_strcasecmp_bin(CHARSET_INFO * cs __attribute__((unused)),
		      const char *s, const char *t)
{
  return strcmp(s,t);
}

static int my_strncasecmp_bin(CHARSET_INFO * cs __attribute__((unused)),
				const char *s, const char *t, uint len)
{
  return memcmp(s,t,len);
}

static int my_mb_wc_bin(CHARSET_INFO *cs __attribute__((unused)),
		  my_wc_t *wc,
		  const unsigned char *str,
		  const unsigned char *end __attribute__((unused)))
{
  if (str >= end)
    return MY_CS_TOOFEW(0);
  
  *wc=str[0];
  return 1;
}

static int my_wc_mb_bin(CHARSET_INFO *cs __attribute__((unused)),
		  my_wc_t wc,
		  unsigned char *s,
		  unsigned char *e __attribute__((unused)))
{
  if (s >= e)
    return MY_CS_TOOSMALL;

  if (wc < 256)
  {
    s[0]= (char) wc;
    return 1;
  }
  return MY_CS_ILUNI;
}


#ifndef NEW_HASH_FUNCTION

	/* Calc hashvalue for a key, case indepenently */

static uint my_hash_caseup_bin(CHARSET_INFO *cs __attribute__((unused)),
				const byte *key, uint length)
{
  register uint nr=1, nr2=4;
  
  while (length--)
  {
    nr^= (((nr & 63)+nr2)*
         ((uint) (uchar) *key++)) + (nr << 8);
    nr2+=3;
  }
  return((uint) nr);
}

#else

static uint my_hash_caseup_bin(CHARSET_INFO *cs __attribute__((unused)),
			       const byte *key, uint len)
{
  const byte *end=key+len;
  uint hash;
  for (hash = 0; key < end; key++)
  {
    hash *= 16777619;
    hash ^= (uint) (uchar) *key;
  }
  return (hash);
}

#endif
				  
void my_hash_sort_bin(CHARSET_INFO *cs __attribute__((unused)),
		      const uchar *key, uint len,ulong *nr1, ulong *nr2)
{
  const uchar *pos = key;
  
  key+= len;
  
  for (; pos < (uchar*) key ; pos++)
  {
    nr1[0]^=(ulong) ((((uint) nr1[0] & 63)+nr2[0]) * 
	     ((uint)*pos)) + (nr1[0] << 8);
    nr2[0]+=3;
  }
}


static int my_wildcmp_bin(CHARSET_INFO *cs,
			   const char *str,const char *str_end,
			   const char *wildstr,const char *wildend,
			   int escape, int w_one, int w_many)
{
  int result= -1;				/* Not found, using wildcards */
  
  while (wildstr != wildend)
  {
    while (*wildstr != w_many && *wildstr != w_one)
    {
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
      if (str == str_end || *wildstr++ != *str++)
      {
	return(1);
      }
      if (wildstr == wildend)
      {
	return(str != str_end);			/* Match if both are at end */
      }
      result=1;					/* Found an anchor char */
    }
    if (*wildstr == w_one)
    {
      do
      {
	if (str == str_end)			/* Skip one char if possible */
	  return(result);
	str++;
      } while (*++wildstr == w_one && wildstr != wildend);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == w_many)
    {						/* Found w_many */
      char cmp;
      
      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for (; wildstr != wildend ; wildstr++)
      {
	if (*wildstr == w_many)
	  continue;
	if (*wildstr == w_one)
	{
	  if (str == str_end)
	  {
	    return(-1);
	  }
	  str++;
	  continue;
	}
	break;					/* Not a wild character */
      }
      if (wildstr == wildend)
      {
	return(0);				/* Ok if w_many is last */
      }
      if (str == str_end)
      {
	return(-1);
      }
      
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;
      wildstr++;				/* This is compared trough cmp */
      do
      {
	while (str != str_end && *str != cmp)
	  str++;
	if (str++ == str_end)
	{ 
	  return(-1);
	}
	{
	  int tmp=my_wildcmp_bin(cs,str,str_end,wildstr,wildend,escape,w_one,w_many);
	  if (tmp <= 0)
	  {
	    return(tmp);
	  }
	}
      } while (str != str_end && wildstr[0] != w_many);
      return(-1);
    }
  }
  return(str != str_end ? 1 : 0);
}

static int my_strnxfrm_bin(CHARSET_INFO *cs __attribute__((unused)),
			    uchar * dest, uint len,
			    const uchar *src, 
			    uint srclen __attribute__((unused)))
{
  memcpy(dest,src,len= min(len,srclen));
  return len;
}

CHARSET_INFO my_charset_bin =
{
    63,0,0,			/* number        */
    MY_CS_COMPILED|MY_CS_BINSORT|MY_CS_PRIMARY,/* state        */
    "binary",			/* cs name    */
    "binary",			/* name          */
    "",				/* comment       */
    ctype_bin,			/* ctype         */
    bin_char_array,		/* to_lower      */
    bin_char_array,		/* to_upper      */
    bin_char_array,		/* sort_order    */
    NULL,			/* tab_to_uni    */
    NULL,			/* tab_from_uni  */
    "","",
    0,				/* strxfrm_multiply */
    my_strnncoll_binary,	/* strnncoll     */
    my_strnncollsp_binary,
    my_strnxfrm_bin,		/* strxnfrm      */
    my_like_range_simple,	/* like_range    */
    my_wildcmp_bin,		/* wildcmp       */
    1,				/* mbmaxlen      */
    NULL,			/* ismbchar      */
    NULL,			/* ismbhead      */
    NULL,			/* mbcharlen     */
    my_numchars_8bit,
    my_charpos_8bit,
    my_mb_wc_bin,		/* mb_wc         */
    my_wc_mb_bin,		/* wc_mb         */
    my_caseup_str_bin,		/* caseup_str    */
    my_casedn_str_bin,		/* casedn_str    */
    my_caseup_bin,		/* caseup        */
    my_casedn_bin,		/* casedn        */
    my_tosort_bin,		/* tosort        */
    my_strcasecmp_bin,		/* strcasecmp    */
    my_strncasecmp_bin,		/* strncasecmp   */
    my_hash_caseup_bin,		/* hash_caseup   */
    my_hash_sort_bin,		/* hash_sort     */
    (char) 255,			/* max_sort_char */
    my_snprintf_8bit,		/* snprintf      */
    my_long10_to_str_8bit,
    my_longlong10_to_str_8bit,
    my_fill_8bit,
    my_strntol_8bit,
    my_strntoul_8bit,
    my_strntoll_8bit,
    my_strntoull_8bit,
    my_strntod_8bit,
    my_scan_8bit
};
