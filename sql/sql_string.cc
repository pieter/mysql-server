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

/* This file is originally from the mysql distribution. Coded by monty */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <m_ctype.h>
#ifdef HAVE_FCONVERT
#include <floatingpoint.h>
#endif

CHARSET_INFO *system_charset_info= &my_charset_utf8;
CHARSET_INFO *files_charset_info= &my_charset_utf8;
CHARSET_INFO *national_charset_info= &my_charset_utf8;

extern gptr sql_alloc(unsigned size);
extern void sql_element_free(void *ptr);
static uint32
copy_and_convert(char *to, uint32 to_length, CHARSET_INFO *to_cs, 
		 const char *from, uint32 from_length, CHARSET_INFO *from_cs);

#include "sql_string.h"

/*****************************************************************************
** String functions
*****************************************************************************/

bool String::real_alloc(uint32 arg_length)
{
  arg_length=ALIGN_SIZE(arg_length+1);
  str_length=0;
  if (Alloced_length < arg_length)
  {
    free();
    if (!(Ptr=(char*) my_malloc(arg_length,MYF(MY_WME))))
      return TRUE;
    Alloced_length=arg_length;
    alloced=1;
  }
  Ptr[0]=0;
  return FALSE;
}


/*
** Check that string is big enough. Set string[alloc_length] to 0
** (for C functions)
*/

bool String::realloc(uint32 alloc_length)
{
  uint32 len=ALIGN_SIZE(alloc_length+1);
  if (Alloced_length < len)
  {
    char *new_ptr;
    if (alloced)
    {
      if ((new_ptr= (char*) my_realloc(Ptr,len,MYF(MY_WME))))
      {
	Ptr=new_ptr;
	Alloced_length=len;
      }
      else
	return TRUE;				// Signal error
    }
    else if ((new_ptr= (char*) my_malloc(len,MYF(MY_WME))))
    {
      if (str_length)				// Avoid bugs in memcpy on AIX
	memcpy(new_ptr,Ptr,str_length);
      new_ptr[str_length]=0;
      Ptr=new_ptr;
      Alloced_length=len;
      alloced=1;
    }
    else
      return TRUE;			// Signal error
  }
  Ptr[alloc_length]=0;			// This make other funcs shorter
  return FALSE;
}

bool String::set(longlong num, CHARSET_INFO *cs)
{
  uint l=20*cs->mbmaxlen+1;

  if (alloc(l))
    return TRUE;
  str_length=(uint32) (cs->longlong10_to_str)(cs,Ptr,l,-10,num);
  str_charset=cs;
  return FALSE;
}

bool String::set(ulonglong num, CHARSET_INFO *cs)
{
  uint l=20*cs->mbmaxlen+1;

  if (alloc(l))
    return TRUE;
  str_length=(uint32) (cs->longlong10_to_str)(cs,Ptr,l,10,num);
  str_charset=cs;
  return FALSE;
}

bool String::set(double num,uint decimals, CHARSET_INFO *cs)
{
  char buff[331];

  str_charset=cs;
  if (decimals >= NOT_FIXED_DEC)
  {
    sprintf(buff,"%.14g",num);			// Enough for a DATETIME
    return copy(buff, (uint32) strlen(buff), &my_charset_latin1, cs);
  }
#ifdef HAVE_FCONVERT
  int decpt,sign;
  char *pos,*to;

  VOID(fconvert(num,(int) decimals,&decpt,&sign,buff+1));
  if (!my_isdigit(&my_charset_latin1, buff[1]))
  {						// Nan or Inf
    pos=buff+1;
    if (sign)
    {
      buff[0]='-';
      pos=buff;
    }
    return copy(pos,(uint32) strlen(pos), &my_charset_latin1, cs);
  }
  if (alloc((uint32) ((uint32) decpt+3+decimals)))
    return TRUE;
  to=Ptr;
  if (sign)
    *to++='-';

  pos=buff+1;
  if (decpt < 0)
  {					/* value is < 0 */
    *to++='0';
    if (!decimals)
      goto end;
    *to++='.';
    if ((uint32) -decpt > decimals)
      decpt= - (int) decimals;
    decimals=(uint32) ((int) decimals+decpt);
    while (decpt++ < 0)
      *to++='0';
  }
  else if (decpt == 0)
  {
    *to++= '0';
    if (!decimals)
      goto end;
    *to++='.';
  }
  else
  {
    while (decpt-- > 0)
      *to++= *pos++;
    if (!decimals)
      goto end;
    *to++='.';
  }
  while (decimals--)
    *to++= *pos++;

end:
  *to=0;
  str_length=(uint32) (to-Ptr);
  return FALSE;
#else
#ifdef HAVE_SNPRINTF
  buff[sizeof(buff)-1]=0;			// Safety
  snprintf(buff,sizeof(buff)-1, "%.*f",(int) decimals,num);
#else
  sprintf(buff,"%.*f",(int) decimals,num);
#endif
  return copy(buff,(uint32) strlen(buff), &my_charset_latin1, cs);
#endif
}


bool String::copy()
{
  if (!alloced)
  {
    Alloced_length=0;				// Force realloc
    return realloc(str_length);
  }
  return FALSE;
}

bool String::copy(const String &str)
{
  if (alloc(str.str_length))
    return TRUE;
  str_length=str.str_length;
  bmove(Ptr,str.Ptr,str_length);		// May be overlapping
  Ptr[str_length]=0;
  str_charset=str.str_charset;
  return FALSE;
}

bool String::copy(const char *str,uint32 arg_length, CHARSET_INFO *cs)
{
  if (alloc(arg_length))
    return TRUE;
  if ((str_length=arg_length))
    memcpy(Ptr,str,arg_length);
  Ptr[arg_length]=0;
  str_charset=cs;
  return FALSE;
}

	/* Copy with charset convertion */

bool String::copy(const char *str, uint32 arg_length,
		  CHARSET_INFO *from_cs, CHARSET_INFO *to_cs)
{
  if ((from_cs == &my_charset_bin) || (to_cs == &my_charset_bin))
  {
    return copy(str, arg_length, &my_charset_bin);
  }
  uint32 new_length= to_cs->mbmaxlen*arg_length;
  if (alloc(new_length))
    return TRUE;
  str_length=copy_and_convert((char*) Ptr, new_length, to_cs,
			      str, arg_length, from_cs);
  str_charset=to_cs;
  return FALSE;
}


/*
  Set a string to the value of a latin1-string, keeping the original charset
  
  SYNOPSIS
    copy_or_set()
    str			String of a simple charset (latin1)
    arg_length		Length of string

  IMPLEMENTATION
    If string object is of a simple character set, set it to point to the
    given string.
    If not, make a copy and convert it to the new character set.

  RETURN
    0	ok
    1	Could not allocate result buffer

*/

bool String::set_latin1(const char *str, uint32 arg_length)
{
  if (str_charset->mbmaxlen == 1)
  {
    set(str, arg_length, str_charset);
    return 0;
  }
  return copy(str, arg_length, &my_charset_latin1, str_charset);
}


/* This is used by mysql.cc */

bool String::fill(uint32 max_length,char fill_char)
{
  if (str_length > max_length)
    Ptr[str_length=max_length]=0;
  else
  {
    if (realloc(max_length))
      return TRUE;
    bfill(Ptr+str_length,max_length-str_length,fill_char);
    str_length=max_length;
  }
  return FALSE;
}

void String::strip_sp()
{
   while (str_length && my_isspace(str_charset,Ptr[str_length-1]))
    str_length--;
}

bool String::append(const String &s)
{
  if (s.length())
  {
    if (realloc(str_length+s.length()))
      return TRUE;
    memcpy(Ptr+str_length,s.ptr(),s.length());
    str_length+=s.length();
  }
  return FALSE;
}


/*
  Append a latin1 string to the a string of the current character set
*/


bool String::append(const char *s,uint32 arg_length)
{
  if (!arg_length)				// Default argument
    if (!(arg_length= (uint32) strlen(s)))
      return FALSE;
  if (str_charset->mbmaxlen > 1)
  {
    uint32 add_length=arg_length * str_charset->mbmaxlen;
    if (realloc(str_length+ add_length))
      return TRUE;
    str_length+= copy_and_convert(Ptr+str_length, add_length, str_charset,
				  s, arg_length, &my_charset_latin1);
    return FALSE;
  }
  if (realloc(str_length+arg_length))
    return TRUE;
  memcpy(Ptr+str_length,s,arg_length);
  str_length+=arg_length;
  return FALSE;
}


/*
  Append a string in the given charset to the string
  with character set recoding
*/


bool String::append(const char *s,uint32 arg_length, CHARSET_INFO *cs)
{
  if (!arg_length)				// Default argument
    if (!(arg_length= (uint32) strlen(s)))
      return FALSE;
  if (str_charset->mbmaxlen > 1)
  {
    uint32 add_length=arg_length * str_charset->mbmaxlen;
    if (realloc(str_length+ add_length))
      return TRUE;
    str_length+= copy_and_convert(Ptr+str_length, add_length, str_charset,
				  s, arg_length, cs);
    return FALSE;
  }
  if (realloc(str_length+arg_length))
    return TRUE;
  memcpy(Ptr+str_length,s,arg_length);
  str_length+=arg_length;
  return FALSE;
}


#ifdef TO_BE_REMOVED
bool String::append(FILE* file, uint32 arg_length, myf my_flags)
{
  if (realloc(str_length+arg_length))
    return TRUE;
  if (my_fread(file, (byte*) Ptr + str_length, arg_length, my_flags))
  {
    shrink(str_length);
    return TRUE;
  }
  str_length+=arg_length;
  return FALSE;
}
#endif

bool String::append(IO_CACHE* file, uint32 arg_length)
{
  if (realloc(str_length+arg_length))
    return TRUE;
  if (my_b_read(file, (byte*) Ptr + str_length, arg_length))
  {
    shrink(str_length);
    return TRUE;
  }
  str_length+=arg_length;
  return FALSE;
}

uint32 String::numchars()
{
  return str_charset->numchars(str_charset, Ptr, Ptr+str_length);
}

int String::charpos(int i,uint32 offset)
{
  if (i<0) return i;
  return str_charset->charpos(str_charset,Ptr+offset,Ptr+str_length,i);
}

int String::strstr(const String &s,uint32 offset)
{
  if (s.length()+offset <= str_length)
  {
    if (!s.length())
      return ((int) offset);	// Empty string is always found

    register const char *str = Ptr+offset;
    register const char *search=s.ptr();
    const char *end=Ptr+str_length-s.length()+1;
    const char *search_end=s.ptr()+s.length();
skipp:
    while (str != end)
    {
      if (*str++ == *search)
      {
	register char *i,*j;
	i=(char*) str; j=(char*) search+1;
	while (j != search_end)
	  if (*i++ != *j++) goto skipp;
	return (int) (str-Ptr) -1;
      }
    }
  }
  return -1;
}

/*
  Search after a string without regarding to case
  This needs to be replaced when we have character sets per string
*/

int String::strstr_case(const String &s,uint32 offset)
{
  if (s.length()+offset <= str_length)
  {
    if (!s.length())
      return ((int) offset);	// Empty string is always found

    register const char *str = Ptr+offset;
    register const char *search=s.ptr();
    const char *end=Ptr+str_length-s.length()+1;
    const char *search_end=s.ptr()+s.length();
skipp:
    while (str != end)
    {
      if (str_charset->sort_order[*str++] == str_charset->sort_order[*search])
      {
	register char *i,*j;
	i=(char*) str; j=(char*) search+1;
	while (j != search_end)
	  if (str_charset->sort_order[*i++] != 
              str_charset->sort_order[*j++]) 
            goto skipp;
	return (int) (str-Ptr) -1;
      }
    }
  }
  return -1;
}

/*
** Search string from end. Offset is offset to the end of string
*/

int String::strrstr(const String &s,uint32 offset)
{
  if (s.length() <= offset && offset <= str_length)
  {
    if (!s.length())
      return offset;				// Empty string is always found
    register const char *str = Ptr+offset-1;
    register const char *search=s.ptr()+s.length()-1;

    const char *end=Ptr+s.length()-2;
    const char *search_end=s.ptr()-1;
skipp:
    while (str != end)
    {
      if (*str-- == *search)
      {
	register char *i,*j;
	i=(char*) str; j=(char*) search-1;
	while (j != search_end)
	  if (*i-- != *j--) goto skipp;
	return (int) (i-Ptr) +1;
      }
    }
  }
  return -1;
}

/*
** replace substring with string
** If wrong parameter or not enough memory, do nothing
*/


bool String::replace(uint32 offset,uint32 arg_length,const String &to)
{
  long diff = (long) to.length()-(long) arg_length;
  if (offset+arg_length <= str_length)
  {
    if (diff < 0)
    {
      if (to.length())
	memcpy(Ptr+offset,to.ptr(),to.length());
      bmove(Ptr+offset+to.length(),Ptr+offset+arg_length,
	    str_length-offset-arg_length);
    }
    else
    {
      if (diff)
      {
	if (realloc(str_length+(uint32) diff))
	  return TRUE;
	bmove_upp(Ptr+str_length+diff,Ptr+str_length,
		  str_length-offset-arg_length);
      }
      if (to.length())
	memcpy(Ptr+offset,to.ptr(),to.length());
    }
    str_length+=(uint32) diff;
  }
  return FALSE;
}

// added by Holyfoot for "geometry" needs
int String::reserve(uint32 space_needed, uint32 grow_by)
{
  if (Alloced_length < str_length + space_needed)
  {
    if (realloc(Alloced_length + max(space_needed, grow_by) - 1))
      return TRUE;
  }
  return FALSE;
}

void String::qs_append(const char *str)
{
  int len = strlen(str);
  memcpy(Ptr + str_length, str, len + 1);
  str_length += len;
}

void String::qs_append(double d)
{
  char *buff = Ptr + str_length;
  sprintf(buff,"%.14g", d);
  str_length += strlen(buff);
}

void String::qs_append(double *d)
{
  double ld;
  float8get(ld, (char*) d);
  qs_append(ld);
}

void String::qs_append(const char &c)
{
  Ptr[str_length] = c;
  str_length += sizeof(c);
}


int sortcmp(const String *x,const String *y, CHARSET_INFO *cs)
{
  return cs->strnncollsp(cs,
                        (unsigned char *) x->ptr(),x->length(),
			(unsigned char *) y->ptr(),y->length());
}


String *copy_if_not_alloced(String *to,String *from,uint32 from_length)
{
  if (from->Alloced_length >= from_length)
    return from;
  if (from->alloced || !to || from == to)
  {
    (void) from->realloc(from_length);
    return from;
  }
  if (to->realloc(from_length))
    return from;				// Actually an error
  if ((to->str_length=min(from->str_length,from_length)))
    memcpy(to->Ptr,from->Ptr,to->str_length);
  to->str_charset=from->str_charset;
  return to;
}


/****************************************************************************
  Help functions
****************************************************************************/

/*
  copy a string from one character set to another
  
  SYNOPSIS
    copy_and_convert()
    to			Store result here
    to_cs		Character set of result string
    from		Copy from here
    from_length		Length of from string
    from_cs		From character set

  NOTES
    'to' must be big enough as form_length * to_cs->mbmaxlen

  RETURN
    length of bytes copied to 'to'
*/


static uint32
copy_and_convert(char *to, uint32 to_length, CHARSET_INFO *to_cs, 
		 const char *from, uint32 from_length, CHARSET_INFO *from_cs)
{
  int         cnvres;
  my_wc_t     wc;
  const uchar *from_end= (const uchar*) from+from_length;
  char *to_start= to;
  uchar *to_end= (uchar*) to+to_length;

  while (1)
  {
    if ((cnvres=from_cs->mb_wc(from_cs, &wc, (uchar*) from, from_end)) > 0)
      from+= cnvres;
    else if (cnvres == MY_CS_ILSEQ)
    {
      from++;
      wc= '?';
    }
    else
      break;					// Impossible char.

outp:
    if ((cnvres= to_cs->wc_mb(to_cs, wc, (uchar*) to, to_end)) > 0)
      to+= cnvres;
    else if (cnvres == MY_CS_ILUNI && wc != '?')
    {
      wc= '?';
      goto outp;
    }
    else
      break;
  }
  return (uint32) (to - to_start);
}
