/* Copyright (C) 2000 MySQL AB
   
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

/* UCS2 support. Written by Alexander Barkov <bar@mysql.com> */

#include <my_global.h>
#include "m_string.h"
#include "m_ctype.h"
#include <errno.h>


#ifdef HAVE_CHARSET_ucs2

extern MY_UNICASE_INFO *uni_plane[256];

static uchar ctype_ucs2[] = {
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
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

static uchar to_lower_ucs2[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122, 91, 92, 93, 94, 95,
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

static uchar to_upper_ucs2[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};


static int my_ucs2_uni (CHARSET_INFO *cs __attribute__((unused)) , 
                 my_wc_t * pwc, const uchar *s, const uchar *e)
{
  if (s+2 > e) /* Need 2 characters */
    return MY_CS_TOOFEW(0);
  
  *pwc= ((unsigned char)s[0]) * 256  + ((unsigned char)s[1]);
  return 2;
}

static int my_uni_ucs2 (CHARSET_INFO *cs __attribute__((unused)) ,
                 my_wc_t wc, uchar *r, uchar *e)
{
  if ( r+2 > e ) 
    return MY_CS_TOOSMALL;
  
  r[0]=wc >> 8;
  r[1]=wc & 0xFF;
  return 2;
}


static void my_caseup_ucs2(CHARSET_INFO *cs, char *s, uint slen)
{
  my_wc_t wc;
  int res;
  char *e=s+slen;

  while ((s < e) && (res=my_ucs2_uni(cs,&wc, (uchar *)s, (uchar*)e))>0 )
  {
    int plane = (wc>>8) & 0xFF;
    wc = uni_plane[plane] ? uni_plane[plane][wc & 0xFF].toupper : wc;
    if (res != my_uni_ucs2(cs,wc,(uchar*)s,(uchar*)e))
      break;
    s+=res;
  }
}

static void my_hash_sort_ucs2(CHARSET_INFO *cs, const uchar *s, uint slen, ulong *n1, ulong *n2)
{
  my_wc_t wc;
  int res;
  const uchar *e=s+slen;

  while ((s < e) && (res=my_ucs2_uni(cs,&wc, (uchar *)s, (uchar*)e))>0 )
  {
    int plane = (wc>>8) & 0xFF;
    wc = uni_plane[plane] ? uni_plane[plane][wc & 0xFF].sort : wc;
    n1[0]^= (((n1[0] & 63)+n2[0])*(wc & 0xFF))+ (n1[0] << 8);
    n2[0]+=3;
    n1[0]^= (((n1[0] & 63)+n2[0])*(wc >> 8))+ (n1[0] << 8);
    n2[0]+=3;
    s+=res;
  }
}


static void my_caseup_str_ucs2(CHARSET_INFO * cs  __attribute__((unused)), 
                        char * s __attribute__((unused)))
{
}



static void my_casedn_ucs2(CHARSET_INFO *cs, char *s, uint slen)
{
  my_wc_t wc;
  int res;
  char *e=s+slen;

  while ((s < e) && (res=my_ucs2_uni(cs, &wc, (uchar*)s, (uchar*)e))>0)
  {
    int plane = (wc>>8) & 0xFF;
    wc = uni_plane[plane] ? uni_plane[plane][wc & 0xFF].tolower : wc;
    if (res != my_uni_ucs2(cs, wc, (uchar*)s, (uchar*)e))
    {
      break;
    }
    s+=res;
  }
}

static void my_casedn_str_ucs2(CHARSET_INFO *cs __attribute__((unused)), 
                        char * s __attribute__((unused)))
{
}


static int my_strnncoll_ucs2(CHARSET_INFO *cs, 
		const uchar *s, uint slen, const uchar *t, uint tlen)
{
  int s_res,t_res;
  my_wc_t s_wc,t_wc;
  const uchar *se=s+slen;
  const uchar *te=t+tlen;

  while ( s < se && t < te )
  {
    int plane;
    s_res=my_ucs2_uni(cs,&s_wc, s, se);
    t_res=my_ucs2_uni(cs,&t_wc, t, te);
    
    if ( s_res <= 0 || t_res <= 0 )
    {
      /* Incorrect string, compare by char value */
      return ((int)s[0]-(int)t[0]); 
    }
    
    plane=(s_wc>>8) & 0xFF;
    s_wc = uni_plane[plane] ? uni_plane[plane][s_wc & 0xFF].sort : s_wc;
    plane=(t_wc>>8) & 0xFF;
    t_wc = uni_plane[plane] ? uni_plane[plane][t_wc & 0xFF].sort : t_wc;
    if ( s_wc != t_wc )
    {
      return  ((int) s_wc) - ((int) t_wc);
    }
    
    s+=s_res;
    t+=t_res;
  }
  return ( (se-s) - (te-t) );
}

static int my_strncasecmp_ucs2(CHARSET_INFO *cs,
		const char *s, const char *t,  uint len)
{
  int s_res,t_res;
  my_wc_t s_wc,t_wc;
  const char *se=s+len;
  const char *te=t+len;
  
  while ( s < se && t < te )
  {
    int plane;
    
    s_res=my_ucs2_uni(cs,&s_wc, (const uchar*)s, (const uchar*)se);
    t_res=my_ucs2_uni(cs,&t_wc, (const uchar*)t, (const uchar*)te);
    
    if ( s_res <= 0 || t_res <= 0 )
    {
      /* Incorrect string, compare by char value */
      return ((int)s[0]-(int)t[0]); 
    }
    
    plane=(s_wc>>8) & 0xFF;
    s_wc = uni_plane[plane] ? uni_plane[plane][s_wc & 0xFF].tolower : s_wc;

    plane=(t_wc>>8) & 0xFF;
    t_wc = uni_plane[plane] ? uni_plane[plane][t_wc & 0xFF].tolower : t_wc;
    
    if ( s_wc != t_wc )
      return  ((int) s_wc) - ((int) t_wc);
    
    s+=s_res;
    t+=t_res;
  }
  return ( (se-s) - (te-t) );
}

static int my_strcasecmp_ucs2(CHARSET_INFO *cs, const char *s, const char *t)
{
  uint s_len=strlen(s);
  uint t_len=strlen(t);
  uint len = (s_len > t_len) ? s_len : t_len;
  return  my_strncasecmp_ucs2(cs, s, t, len);
}

static int my_strnxfrm_ucs2(CHARSET_INFO *cs, 
	uchar *dst, uint dstlen, const uchar *src, uint srclen)
{
  my_wc_t wc;
  int res;
  int plane;
  uchar *de = dst + dstlen;
  const uchar *se = src + srclen;
  const uchar *dst_orig = dst;

  while( src < se && dst < de )
  {
    if ((res=my_ucs2_uni(cs,&wc, src, se))<0)
    {
      break;
    }
    src+=res;
    srclen-=res;
    
    plane=(wc>>8) & 0xFF;
    wc = uni_plane[plane] ? uni_plane[plane][wc & 0xFF].sort : wc;
    
    if ((res=my_uni_ucs2(cs,wc,dst,de)) <0)
    {
      break;
    }
    dst+=res;
  }
  return dst - dst_orig;
}

static int my_ismbchar_ucs2(CHARSET_INFO *cs __attribute__((unused)),
                     const char *b __attribute__((unused)),
                     const char *e __attribute__((unused)))
{
  return 2;
}

static int my_mbcharlen_ucs2(CHARSET_INFO *cs  __attribute__((unused)) , 
                      uint c __attribute__((unused)))
{
  return 2;
}


#include <m_string.h>
#include <stdarg.h>
#include <assert.h>

static int my_vsnprintf_ucs2(char *dst, uint n, const char* fmt, va_list ap)
{
  char *start=dst, *end=dst+n-1;
  for (; *fmt ; fmt++)
  {
    if (fmt[0] != '%')
    {
      if (dst == end)			/* End of buffer */
	break;
      
      *dst++='\0'; *dst++= *fmt;	/* Copy ordinary char */
      continue;
    }
    
    fmt++;
    
    /* Skip if max size is used (to be compatible with printf) */
    while ( (*fmt>='0' && *fmt<='9') || *fmt == '.' || *fmt == '-')
      fmt++;
    
    if (*fmt == 'l')
      fmt++;
    
    if (*fmt == 's')				/* String parameter */
    {
      reg2 char	*par = va_arg(ap, char *);
      uint plen;
      uint left_len = (uint)(end-dst);
      if (!par) par = (char*)"(null)";
      plen = (uint) strlen(par);
      if (left_len <= plen*2)
	plen = left_len/2 - 1;

      for ( ; plen ; plen--, dst+=2, par++)
      {
        dst[0]='\0';
        dst[1]=par[0];
      }
      continue;
    }
    else if (*fmt == 'd' || *fmt == 'u')	/* Integer parameter */
    {
      register int iarg;
      char nbuf[16];
      char *pbuf=nbuf;
      
      if ((uint) (end-dst) < 32)
	break;
      iarg = va_arg(ap, int);
      if (*fmt == 'd')
	int10_to_str((long) iarg, nbuf, -10);
      else
	int10_to_str((long) (uint) iarg,nbuf,10);

      for (; pbuf[0]; pbuf++)
      {
        *dst++='\0';
        *dst++=*pbuf;
      }
      continue;
    }
    
    /* We come here on '%%', unknown code or too long parameter */
    if (dst == end)
      break;
    *dst++='\0';
    *dst++='%';				/* % used as % or unknown code */
  }
  
  DBUG_ASSERT(dst <= end);
  *dst='\0';				/* End of errmessage */
  return (uint) (dst - start);
}

static int my_snprintf_ucs2(CHARSET_INFO *cs __attribute__((unused))
			    ,char* to, uint n, const char* fmt, ...)
{
  va_list args;
  va_start(args,fmt);
  return my_vsnprintf_ucs2(to, n, fmt, args);
}


long        my_strntol_ucs2(CHARSET_INFO *cs,
			   const char *nptr, uint l, int base,
			   char **endptr, int *err)
{
  int      negative=0;
  int      overflow;
  int      cnv;
  my_wc_t  wc;
  register unsigned int cutlim;
  register ulong cutoff;
  register ulong res;
  register const uchar *s= (const uchar*) nptr;
  register const uchar *e= (const uchar*) nptr+l;
  const uchar *save;
  
  *err= 0;
  do
  {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      switch (wc)
      {
        case ' ' : break;
        case '\t': break;
        case '-' : negative= !negative; break;
        case '+' : break;
        default  : goto bs;
      }
    } 
    else /* No more characters or bad multibyte sequence */
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0] = (cnv==MY_CS_ILSEQ) ? EILSEQ : EDOM;
      return 0;
    } 
    s+=cnv;
  } while (1);
  
bs:

#ifdef NOT_USED  
  if (base <= 0 || base == 1 || base > 36)
    base = 10;
#endif
  
  overflow = 0;
  res = 0;
  save = s;
  cutoff = ((ulong)~0L) / (unsigned long int) base;
  cutlim = (uint) (((ulong)~0L) % (unsigned long int) base);
  
  do {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      s+=cnv;
      if ( wc>='0' && wc<='9')
        wc -= '0';
      else if ( wc>='A' && wc<='Z')
        wc = wc - 'A' + 10;
      else if ( wc>='a' && wc<='z')
        wc = wc - 'a' + 10;
      else
        break;
      if ((int)wc >= base)
        break;
      if (res > cutoff || (res == cutoff && wc > cutlim))
        overflow = 1;
      else
      {
        res *= (ulong) base;
        res += wc;
      }
    }
    else if (cnv==MY_CS_ILSEQ)
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0]=EILSEQ;
      return 0;
    } 
    else
    {
      /* No more characters */
      break;
    }
  } while(1);
  
  if (endptr != NULL)
    *endptr = (char *) s;
  
  if (s == save)
  {
    err[0]=EDOM;
    return 0L;
  }
  
  if (negative)
  {
    if (res > (ulong) LONG_MIN)
      overflow = 1;
  }
  else if (res > (ulong) LONG_MAX)
    overflow = 1;
  
  if (overflow)
  {
    err[0]=ERANGE;
    return negative ? LONG_MIN : LONG_MAX;
  }
  
  return (negative ? -((long) res) : (long) res);
}


ulong      my_strntoul_ucs2(CHARSET_INFO *cs,
			   const char *nptr, uint l, int base, 
			   char **endptr, int *err)
{
  int      negative=0;
  int      overflow;
  int      cnv;
  my_wc_t  wc;
  register unsigned int cutlim;
  register ulong cutoff;
  register ulong res;
  register const uchar *s= (const uchar*) nptr;
  register const uchar *e= (const uchar*) nptr+l;
  const uchar *save;
  
  *err= 0;
  do
  {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      switch (wc)
      {
        case ' ' : break;
        case '\t': break;
        case '-' : negative= !negative; break;
        case '+' : break;
        default  : goto bs;
      }
    } 
    else /* No more characters or bad multibyte sequence */
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0] = (cnv==MY_CS_ILSEQ) ? EILSEQ : EDOM;
      return 0;
    } 
    s+=cnv;
  } while (1);
  
bs:

#ifdef NOT_USED
  if (base <= 0 || base == 1 || base > 36)
    base = 10;
#endif

  overflow = 0;
  res = 0;
  save = s;
  cutoff = ((ulong)~0L) / (unsigned long int) base;
  cutlim = (uint) (((ulong)~0L) % (unsigned long int) base);
  
  do
  {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      s+=cnv;
      if ( wc>='0' && wc<='9')
        wc -= '0';
      else if ( wc>='A' && wc<='Z')
        wc = wc - 'A' + 10;
      else if ( wc>='a' && wc<='z')
        wc = wc - 'a' + 10;
      else
        break;
      if ((int)wc >= base)
        break;
      if (res > cutoff || (res == cutoff && wc > cutlim))
        overflow = 1;
      else
      {
        res *= (ulong) base;
        res += wc;
      }
    }
    else if (cnv==MY_CS_ILSEQ)
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0]=EILSEQ;
      return 0;
    } 
    else
    {
      /* No more characters */
      break;
    }
  } while(1);
  
  if (endptr != NULL)
    *endptr = (char *) s;
  
  if (s == save)
  {
    err[0]=EDOM;
    return 0L;
  }
  
  if (overflow)
  {
    err[0]=(ERANGE);
    return ((ulong)~0L);
  }
  
  return (negative ? -((long) res) : (long) res);
  
}



longlong  my_strntoll_ucs2(CHARSET_INFO *cs,
			   const char *nptr, uint l, int base,
			   char **endptr, int *err)
{
  int      negative=0;
  int      overflow;
  int      cnv;
  my_wc_t  wc;
  register ulonglong    cutoff;
  register unsigned int cutlim;
  register ulonglong    res;
  register const uchar *s= (const uchar*) nptr;
  register const uchar *e= (const uchar*) nptr+l;
  const uchar *save;
  
  *err= 0;
  do
  {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      switch (wc)
      {
        case ' ' : break;
        case '\t': break;
        case '-' : negative= !negative; break;
        case '+' : break;
        default  : goto bs;
      }
    } 
    else /* No more characters or bad multibyte sequence */
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0] = (cnv==MY_CS_ILSEQ) ? EILSEQ : EDOM;
      return 0;
    } 
    s+=cnv;
  } while (1);
  
bs:

#ifdef NOT_USED  
  if (base <= 0 || base == 1 || base > 36)
    base = 10;
#endif

  overflow = 0;
  res = 0;
  save = s;
  cutoff = (~(ulonglong) 0) / (unsigned long int) base;
  cutlim = (uint) ((~(ulonglong) 0) % (unsigned long int) base);

  do {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      s+=cnv;
      if ( wc>='0' && wc<='9')
        wc -= '0';
      else if ( wc>='A' && wc<='Z')
        wc = wc - 'A' + 10;
      else if ( wc>='a' && wc<='z')
        wc = wc - 'a' + 10;
      else
        break;
      if ((int)wc >= base)
        break;
      if (res > cutoff || (res == cutoff && wc > cutlim))
        overflow = 1;
      else
      {
        res *= (ulonglong) base;
        res += wc;
      }
    }
    else if (cnv==MY_CS_ILSEQ)
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0]=EILSEQ;
      return 0;
    } 
    else
    {
      /* No more characters */
      break;
    }
  } while(1);
  
  if (endptr != NULL)
    *endptr = (char *) s;
  
  if (s == save)
  {
    err[0]=EDOM;
    return 0L;
  }
  
  if (negative)
  {
    if (res  > (ulonglong) LONGLONG_MIN)
      overflow = 1;
  }
  else if (res > (ulonglong) LONGLONG_MAX)
    overflow = 1;
  
  if (overflow)
  {
    err[0]=ERANGE;
    return negative ? LONGLONG_MIN : LONGLONG_MAX;
  }
  
  return (negative ? -((longlong)res) : (longlong)res);
}




ulonglong  my_strntoull_ucs2(CHARSET_INFO *cs,
			   const char *nptr, uint l, int base,
			   char **endptr, int *err)
{
  int      negative=0;
  int      overflow;
  int      cnv;
  my_wc_t  wc;
  register ulonglong    cutoff;
  register unsigned int cutlim;
  register ulonglong    res;
  register const uchar *s= (const uchar*) nptr;
  register const uchar *e= (const uchar*) nptr+l;
  const uchar *save;
  
  *err= 0;
  do
  {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      switch (wc)
      {
        case ' ' : break;
        case '\t': break;
        case '-' : negative= !negative; break;
        case '+' : break;
        default  : goto bs;
      }
    } 
    else /* No more characters or bad multibyte sequence */
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0]= (cnv==MY_CS_ILSEQ) ? EILSEQ : EDOM;
      return 0;
    } 
    s+=cnv;
  } while (1);
  
bs:
  
#ifdef NOT_USED
  if (base <= 0 || base == 1 || base > 36)
    base = 10;
#endif

  overflow = 0;
  res = 0;
  save = s;
  cutoff = (~(ulonglong) 0) / (unsigned long int) base;
  cutlim = (uint) ((~(ulonglong) 0) % (unsigned long int) base);

  do
  {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      s+=cnv;
      if ( wc>='0' && wc<='9')
        wc -= '0';
      else if ( wc>='A' && wc<='Z')
        wc = wc - 'A' + 10;
      else if ( wc>='a' && wc<='z')
        wc = wc - 'a' + 10;
      else
        break;
      if ((int)wc >= base)
        break;
      if (res > cutoff || (res == cutoff && wc > cutlim))
        overflow = 1;
      else
      {
        res *= (ulonglong) base;
        res += wc;
      }
    }
    else if (cnv==MY_CS_ILSEQ)
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0]= EILSEQ;
      return 0;
    } 
    else
    {
      /* No more characters */
      break;
    }
  } while(1);
  
  if (endptr != NULL)
    *endptr = (char *) s;
  
  if (s == save)
  {
    err[0]= EDOM;
    return 0L;
  }
  
  if (overflow)
  {
    err[0]= ERANGE;
    return (~(ulonglong) 0);
  }

  return (negative ? -((longlong) res) : (longlong) res);
}


double      my_strntod_ucs2(CHARSET_INFO *cs __attribute__((unused)),
			   char *nptr, uint length, 
			   char **endptr, int *err)
{
  char     buf[256];
  double   res;
  register char *b=buf;
  register const uchar *s= (const uchar*) nptr;
  register const uchar *end;
  my_wc_t  wc;
  int      cnv;

  *err= 0;
  /* Cut too long strings */
  if (length >= sizeof(buf))
    length= sizeof(buf)-1;
  end= s+length;
 
  while ((cnv=cs->cset->mb_wc(cs,&wc,s,end)) > 0)
  {
    s+=cnv;
    if (wc > (int) (uchar) 'e' || !wc)
      break;					/* Can't be part of double */
    *b++=wc;
  }
  *b= 0;
  
  errno= 0;
  res=strtod(buf, endptr);
  *err= errno;
  if (endptr)
    *endptr=(char*) (*endptr-buf+nptr);
  return res;
}


/*
  This is a fast version optimized for the case of radix 10 / -10
*/

int my_l10tostr_ucs2(CHARSET_INFO *cs,
		     char *dst, uint len, int radix, long int val)
{
  char buffer[66];
  register char *p, *db, *de;
  long int new_val;
  int  sl=0;
  
  p = &buffer[sizeof(buffer)-1];
  *p='\0';
  
  if (radix < 0)
  {
    if (val < 0)
    {
      sl   = 1;
      val  = -val;
    }
  }
  
  new_val = (long) ((unsigned long int) val / 10);
  *--p    = '0'+ (char) ((unsigned long int) val - (unsigned long) new_val * 10);
  val     = new_val;
  
  while (val != 0)
  {
    new_val=val/10;
    *--p = '0' + (char) (val-new_val*10);
    val= new_val;
  }
  
  if (sl)
  {
    *--p='-';
  }
  
  for ( db=dst, de=dst+len ; (dst<de) && *p ; p++)
  {
    int cnvres=cs->cset->wc_mb(cs,(my_wc_t)p[0],(uchar*) dst, (uchar*) de);
    if (cnvres>0)
      dst+=cnvres;
    else
      break;
  }
  return (int) (dst-db);
}

int my_ll10tostr_ucs2(CHARSET_INFO *cs __attribute__((unused)),
		      char *dst, uint len, int radix, longlong val)
{
  char buffer[65];
  register char *p, *db, *de;
  long long_val;
  int  sl=0;
  
  if (radix < 0)
  {
    if (val < 0)
    {
      sl=1;
      val = -val;
    }
  }
  
  p = &buffer[sizeof(buffer)-1];
  *p='\0';
  
  if (val == 0)
  {
    *--p='0';
    goto cnv;
  }
  
  while ((ulonglong) val > (ulonglong) LONG_MAX)
  {
    ulonglong quo=(ulonglong) val/(uint) 10;
    uint rem= (uint) (val- quo* (uint) 10);
    *--p = '0' + rem;
    val= quo;
  }
  
  long_val= (long) val;
  while (long_val != 0)
  {
    long quo= long_val/10;
    *--p = '0' + (long_val - quo*10);
    long_val= quo;
  }
  
cnv:
  if (sl)
  {
    *--p='-';
  }
  
  for ( db=dst, de=dst+len ; (dst<de) && *p ; p++)
  {
    int cnvres=cs->cset->wc_mb(cs, (my_wc_t) p[0], (uchar*) dst, (uchar*) de);
    if (cnvres>0)
      dst+=cnvres;
    else
      break;
  }
  return (int) (dst-db);
}

static
uint my_numchars_ucs2(CHARSET_INFO *cs __attribute__((unused)),
		      const char *b, const char *e)
{
  return (e-b)/2;
}

static
uint my_charpos_ucs2(CHARSET_INFO *cs __attribute__((unused)),
		     const char *b  __attribute__((unused)),
		     const char *e  __attribute__((unused)),
		     uint pos)
{
  return pos*2;
}


static MY_COLLATION_HANDLER my_collation_ci_handler =
{
    my_strnncoll_ucs2,
    my_strnncoll_ucs2,
    my_strnxfrm_ucs2,
    my_like_range_simple,
    my_wildcmp_mb,
    my_strcasecmp_ucs2,
    my_hash_sort_ucs2
};

static MY_CHARSET_HANDLER my_charset_handler=
{
    my_ismbchar_ucs2,	/* ismbchar     */
    my_mbcharlen_ucs2,	/* mbcharlen    */
    my_numchars_ucs2,
    my_charpos_ucs2,
    my_ucs2_uni,	/* mb_wc        */
    my_uni_ucs2,	/* wc_mb        */
    my_caseup_str_ucs2,
    my_casedn_str_ucs2,
    my_caseup_ucs2,
    my_casedn_ucs2,
    my_snprintf_ucs2,
    my_l10tostr_ucs2,
    my_ll10tostr_ucs2,
    my_fill_8bit,
    my_strntol_ucs2,
    my_strntoul_ucs2,
    my_strntoll_ucs2,
    my_strntoull_ucs2,
    my_strntod_ucs2,
    my_scan_8bit
};



CHARSET_INFO my_charset_ucs2_general_ci=
{
    35,0,0,		/* number       */
    MY_CS_COMPILED|MY_CS_PRIMARY|MY_CS_STRNXFRM|MY_CS_UNICODE,	/* state */
    "ucs2",		/* cs name    */
    "ucs2_general_ci",	/* name         */
    "",			/* comment      */
    ctype_ucs2,		/* ctype        */
    to_lower_ucs2,	/* to_lower     */
    to_upper_ucs2,	/* to_upper     */
    to_upper_ucs2,	/* sort_order   */
    NULL,		/* tab_to_uni   */
    NULL,		/* tab_from_uni */
    "",
    "",
    1,			/* strxfrm_multiply */
    2,			/* mbmaxlen     */
    0,
    &my_charset_handler,
    &my_collation_ci_handler
};


CHARSET_INFO my_charset_ucs2_bin=
{
    90,0,0,		/* number       */
    MY_CS_COMPILED|MY_CS_BINSORT|MY_CS_UNICODE,	/* state */
    "ucs2",		/* cs name    */
    "ucs2_bin",		/* name         */
    "",			/* comment      */
    ctype_ucs2,		/* ctype        */
    to_lower_ucs2,	/* to_lower     */
    to_upper_ucs2,	/* to_upper     */
    to_upper_ucs2,	/* sort_order   */
    NULL,		/* tab_to_uni   */
    NULL,		/* tab_from_uni */
    "",
    "",
    0,			/* strxfrm_multiply */
    2,			/* mbmaxlen     */
    0,
    &my_charset_handler,
    &my_collation_bin_handler
};


#endif
