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

#include <my_global.h>
#include "m_ctype.h"
#include "m_string.h"

#ifdef USE_MB


void my_caseup_str_mb(CHARSET_INFO * cs, char *str)
{
  register uint32 l;
  register char *end=str+strlen(str); /* BAR TODO: remove strlen() call */
  register uchar *map=cs->to_upper;
  
  while (*str)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else
    { 
      *str=(char) map[(uchar)*str];
      str++;
    }
  }
}

void my_casedn_str_mb(CHARSET_INFO * cs, char *str)
{
  register uint32 l;
  register char *end=str+strlen(str);
  register uchar *map=cs->to_lower;
  
  while (*str)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else
    {
      *str=(char) map[(uchar)*str];
      str++;
    }
  }
}

void my_caseup_mb(CHARSET_INFO * cs, char *str, uint length)
{
  register uint32 l;
  register char *end=str+length;
  register uchar *map=cs->to_upper;
  
  while (str<end)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else 
    {
      *str=(char) map[(uchar)*str];
      str++;
    }
  }
}

void my_casedn_mb(CHARSET_INFO * cs, char *str, uint length)
{
  register uint32 l;
  register char *end=str+length;
  register uchar *map=cs->to_lower;
  
  while (str<end)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else
    {
      *str=(char) map[(uchar)*str];
      str++;
    }
  }
}

int my_strcasecmp_mb(CHARSET_INFO * cs,const char *s, const char *t)
{
  register uint32 l;
  register const char *end=s+strlen(s);
  register uchar *map=cs->to_upper;
  
  while (s<end)
  {
    if ((l=my_ismbchar(cs, s,end)))
    {
      while (l--)
        if (*s++ != *t++) 
          return 1;
    }
    else if (my_ismbhead(cs, *t)) 
      return 1;
    else if (map[(uchar) *s++] != map[(uchar) *t++])
      return 1;
  }
  return *t;
}


/*
** Compare string against string with wildcard
**	0 if matched
**	-1 if not matched with wildcard
**	 1 if matched with wildcard
*/

#define INC_PTR(cs,A,B) A+=((use_mb_flag && \
                          my_ismbchar(cs,A,B)) ? my_ismbchar(cs,A,B) : 1)

#ifdef LIKE_CMP_TOUPPER
#define likeconv(s,A) (uchar) my_toupper(s,A)
#else
#define likeconv(s,A) (uchar) (s)->sort_order[(uchar) (A)]
#endif

int my_wildcmp_mb(CHARSET_INFO *cs,
		  const char *str,const char *str_end,
		  const char *wildstr,const char *wildend,
		  int escape, int w_one, int w_many)
{
  int result= -1;				/* Not found, using wildcards */

  bool use_mb_flag=use_mb(cs);

  while (wildstr != wildend)
  {
    while (*wildstr != w_many && *wildstr != w_one)
    {
      int l;
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
      if (use_mb_flag &&
          (l = my_ismbchar(cs, wildstr, wildend)))
      {
	  if (str+l > str_end || memcmp(str, wildstr, l) != 0)
	      return 1;
	  str += l;
	  wildstr += l;
      }
      else
      if (str == str_end || likeconv(cs,*wildstr++) != likeconv(cs,*str++))
	return(1);				/* No match */
      if (wildstr == wildend)
	return (str != str_end);		/* Match if both are at end */
      result=1;					/* Found an anchor char */
    }
    if (*wildstr == w_one)
    {
      do
      {
	if (str == str_end)			/* Skip one char if possible */
	  return (result);
	INC_PTR(cs,str,str_end);
      } while (++wildstr < wildend && *wildstr == w_one);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == w_many)
    {						/* Found w_many */
      uchar cmp;
      const char* mb = wildstr;
      int mblen=0;
      
      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for (; wildstr != wildend ; wildstr++)
      {
	if (*wildstr == w_many)
	  continue;
	if (*wildstr == w_one)
	{
	  if (str == str_end)
	    return (-1);
	  INC_PTR(cs,str,str_end);
	  continue;
	}
	break;					/* Not a wild character */
      }
      if (wildstr == wildend)
	return(0);				/* Ok if w_many is last */
      if (str == str_end)
	return -1;
      
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;
	
      mb=wildstr;
      LINT_INIT(mblen);
      if (use_mb_flag)
        mblen = my_ismbchar(cs, wildstr, wildend);
      INC_PTR(cs,wildstr,wildend);		/* This is compared trough cmp */
      cmp=likeconv(cs,cmp);   
      do
      {
        if (use_mb_flag)
	{
          for (;;)
          {
            if (str >= str_end)
              return -1;
            if (mblen)
            {
              if (str+mblen <= str_end && memcmp(str, mb, mblen) == 0)
              {
                str += mblen;
                break;
              }
            }
            else if (!my_ismbchar(cs, str, str_end) &&
                     likeconv(cs,*str) == cmp)
            {
              str++;
              break;
            }
            INC_PTR(cs,str, str_end);
          }
	}
        else
        {
          while (str != str_end && likeconv(cs,*str) != cmp)
            str++;
          if (str++ == str_end) return (-1);
        }
	{
	  int tmp=my_wildcmp_mb(cs,str,str_end,wildstr,wildend,escape,w_one,w_many);
	  if (tmp <= 0)
	    return (tmp);
	}
      } while (str != str_end && wildstr[0] != w_many);
      return(-1);
    }
  }
  return (str != str_end ? 1 : 0);
}

uint my_numchars_mb(CHARSET_INFO *cs __attribute__((unused)),
		      const char *b, const char *e)
{
  register uint32 n=0,mblen;
  while (b < e) 
  {
    b+= (mblen= my_ismbchar(cs,b,e)) ? mblen : 1;
    ++n;
  }
  return n;
}

uint my_charpos_mb(CHARSET_INFO *cs __attribute__((unused)),
		     const char *b, const char *e, uint pos)
{
  uint mblen;
  const char *b0=b;
  
  while (pos && b<e)
  {
    b+= (mblen= my_ismbchar(cs,b,e)) ? mblen : 1;
    pos--;
  }
  return b-b0;
}


#endif
