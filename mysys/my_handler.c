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

#include "my_handler.h"

int _mi_compare_text(CHARSET_INFO *charset_info, uchar *a, uint a_length,
                     uchar *b, uint b_length, my_bool part_key)
{
  int flag;

#ifdef USE_STRCOLL
  if (use_strcoll(charset_info))
  {
    /* QQ: This needs to work with part keys at some point */
    return my_strnncoll(charset_info, a, a_length, b, b_length);
  }
  else
#endif
  {
    uint length= min(a_length,b_length);
    uchar *end= a+ length;
    uchar *sort_order=charset_info->sort_order;
    while (a < end)
      if ((flag= (int) sort_order[*a++] - (int) sort_order[*b++]))
        return flag;
  }
  if (part_key && b_length < a_length)
    return 0;
  return (int) (a_length-b_length);
}

static int compare_bin(uchar *a, uint a_length, uchar *b, uint b_length,
                       my_bool part_key)
{
  uint length= min(a_length,b_length);
  uchar *end= a+ length;
  int flag;

  while (a < end)
    if ((flag= (int) *a++ - (int) *b++))
      return flag;
  if (part_key && b_length < a_length)
    return 0;
  return (int) (a_length-b_length);
}

#define CMP(a,b) (a<b ? -1 : a == b ? 0 : 1)
#define FCMP(A,B) ((int) (A) - (int) (B))

/*
Compare two keys
Returns <0, 0, >0 acording to which is bigger
Key_length specifies length of key to use.  Number-keys can't be splited
If flag <> SEARCH_FIND compare also position
*/
int ha_key_cmp(register HA_KEYSEG *keyseg, register uchar *a,
		register uchar *b, uint key_length, uint nextflag,
		uint *diff_pos)
{
  int flag;
  int16 s_1,s_2;
  int32 l_1,l_2;
  uint32 u_1,u_2;
  float f_1,f_2;
  double d_1,d_2;
  uint next_key_length;

  *diff_pos=0;
  for ( ; (int) key_length >0 ; key_length=next_key_length, keyseg++)
  {
    uchar *end;
    (*diff_pos)++;

    /* Handle NULL part */
    if (keyseg->null_bit)
    {
      key_length--;
      if (*a != *b)
      {
        flag = (int) *a - (int) *b;
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      }
      b++;
      if (!*a++)                                /* If key was NULL */
      {
        if (nextflag == (SEARCH_FIND | SEARCH_UPDATE))
          nextflag=SEARCH_SAME;                 /* Allow duplicate keys */
        next_key_length=key_length;
        continue;                               /* To next key part */
      }
    }
    end= a+ min(keyseg->length,key_length);
    next_key_length=key_length-keyseg->length;

    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_TEXT:                       /* Ascii; Key is converted */
      if (keyseg->flag & HA_SPACE_PACK)
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if ((flag=_mi_compare_text(keyseg->charset,a,a_length,b,b_length,
                                   (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                              next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
      else
      {
	uint length=(uint) (end-a), a_length=length, b_length=length;
	if (!(nextflag & SEARCH_PREFIX))
	{
	  while (a_length && a[a_length-1] == ' ')
	    a_length--;
	  while (b_length && b[b_length-1] == ' ')
	    b_length--;
	}
        if ((flag=_mi_compare_text(keyseg->charset,a,a_length,b,b_length,
                                   (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                              next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a=end;
        b+=length;
      }
      break;
    case HA_KEYTYPE_BINARY:
      if (keyseg->flag & HA_SPACE_PACK)
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if ((flag=compare_bin(a,a_length,b,b_length,
                              (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                         next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
      else
      {
        uint length=keyseg->length;
        if ((flag=compare_bin(a,length,b,length,
                              (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                         next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=length;
        b+=length;
      }
      break;
    case HA_KEYTYPE_VARTEXT:
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if ((flag=_mi_compare_text(keyseg->charset,a,a_length,b,b_length,
                                   (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                              next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
      break;
    case HA_KEYTYPE_VARBINARY:
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if ((flag=compare_bin(a,a_length,b,b_length,
                              (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                         next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
      break;
    case HA_KEYTYPE_INT8:
    {
      int i_1= (int) *((signed char*) a);
      int i_2= (int) *((signed char*) b);
      if ((flag = CMP(i_1,i_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a= end;
      b++;
      break;
    }
    case HA_KEYTYPE_SHORT_INT:
      s_1= mi_sint2korr(a);
      s_2= mi_sint2korr(b);
      if ((flag = CMP(s_1,s_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 2; /* sizeof(short int); */
      break;
    case HA_KEYTYPE_USHORT_INT:
      {
        uint16 us_1,us_2;
        us_1= mi_sint2korr(a);
        us_2= mi_sint2korr(b);
        if ((flag = CMP(us_1,us_2)))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a=  end;
        b+=2; /* sizeof(short int); */
        break;
      }
    case HA_KEYTYPE_LONG_INT:
      l_1= mi_sint4korr(a);
      l_2= mi_sint4korr(b);
      if ((flag = CMP(l_1,l_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 4; /* sizeof(long int); */
      break;
    case HA_KEYTYPE_ULONG_INT:
      u_1= mi_sint4korr(a);
      u_2= mi_sint4korr(b);
      if ((flag = CMP(u_1,u_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 4; /* sizeof(long int); */
      break;
    case HA_KEYTYPE_INT24:
      l_1=mi_sint3korr(a);
      l_2=mi_sint3korr(b);
      if ((flag = CMP(l_1,l_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 3;
      break;
    case HA_KEYTYPE_UINT24:
      l_1=mi_uint3korr(a);
      l_2=mi_uint3korr(b);
      if ((flag = CMP(l_1,l_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 3;
      break;
    case HA_KEYTYPE_FLOAT:
      mi_float4get(f_1,a);
      mi_float4get(f_2,b);
      if ((flag = CMP(f_1,f_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 4; /* sizeof(float); */
      break;
    case HA_KEYTYPE_DOUBLE:
      mi_float8get(d_1,a);
      mi_float8get(d_2,b);
      if ((flag = CMP(d_1,d_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 8;  /* sizeof(double); */
      break;
    case HA_KEYTYPE_NUM:                                /* Numeric key */
    {
      int swap_flag= 0;
      int alength,blength;
      
      if (keyseg->flag & HA_REVERSE_SORT)
      {
        swap(uchar*,a,b);       
        swap_flag=1;                            /* Remember swap of a & b */
        end= a+ (int) (end-b);
      }
      if (keyseg->flag & HA_SPACE_PACK)
      {
        alength= *a++; blength= *b++;
        end=a+alength;
        next_key_length=key_length-blength-1;
      }
      else
      {
        alength= (int) (end-a);
        blength=keyseg->length;
        /* remove pre space from keys */
        for ( ; alength && *a == ' ' ; a++, alength--) ;
        for ( ; blength && *b == ' ' ; b++, blength--) ;
      }

      if (*a == '-')
      {
        if (*b != '-')
          return -1;
        a++; b++;
        swap(uchar*,a,b);
        swap(int,alength,blength);
        swap_flag=1-swap_flag;
        alength--; blength--;
        end=a+alength;
      }
      else if (*b == '-')
        return 1;
      while (alength && (*a == '+' || *a == '0'))
      {
        a++; alength--;
      }
      while (blength && (*b == '+' || *b == '0'))
      {
        b++; blength--;
      }
      if (alength != blength)
        return (alength < blength) ? -1 : 1;
      while (a < end)
        if (*a++ !=  *b++)
          return ((int) a[-1] - (int) b[-1]);

      if (swap_flag)                            /* Restore pointers */
        swap(uchar*,a,b);
      break;
    }
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
    {
      longlong ll_a,ll_b;
      ll_a= mi_sint8korr(a);
      ll_b= mi_sint8korr(b);
      if ((flag = CMP(ll_a,ll_b)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 8;
      break;
    }
    case HA_KEYTYPE_ULONGLONG:
    {
      ulonglong ll_a,ll_b;
      ll_a= mi_uint8korr(a);
      ll_b= mi_uint8korr(b);
      if ((flag = CMP(ll_a,ll_b)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 8;
      break;
    }
#endif
    case HA_KEYTYPE_END:                        /* Ready */
      goto end;                                 /* diff_pos is incremented */
    }
  }
  (*diff_pos)++;
end:
  if (!(nextflag & SEARCH_FIND))
  {
    uint i;
    if (nextflag & (SEARCH_NO_FIND | SEARCH_LAST)) /* Find record after key */
      return (nextflag & (SEARCH_BIGGER | SEARCH_LAST)) ? -1 : 1;
    flag=0;
    for (i=keyseg->length ; i-- > 0 ; )
    {
      if (*a++ != *b++)
      {
        flag= FCMP(a[-1],b[-1]);
        break;
      }
    }
    if (nextflag & SEARCH_SAME)
      return (flag);                            /* read same */
    if (nextflag & SEARCH_BIGGER)
      return (flag <= 0 ? -1 : 1);              /* read next */
    return (flag < 0 ? -1 : 1);                 /* read previous */
  }
  return 0;
} /* my_key_cmp */

/*
Compare two keys
Returns <0, 0, >0 acording to which is bigger
Key_length specifies length of key to use.  Number-keys can't be splited
If flag <> SEARCH_FIND compare also position
*/
int hp_rb_key_cmp(register HA_KEYSEG *keyseg, register uchar *a,
		  register uchar *b, uint key_length, uint nextflag,
		  uint *diff_pos)
{
  int flag;
  int16 s_1,s_2;
  int32 l_1,l_2;
  uint32 u_1,u_2;
  float f_1,f_2;
  double d_1,d_2;
  uint next_key_length;

  *diff_pos=0;
  for ( ; (int) key_length >0 ; key_length=next_key_length, keyseg++)
  {
    uchar *end;
    (*diff_pos)++;

    /* Handle NULL part */
    if (keyseg->null_bit)
    {
      key_length--;
      if (*a != *b)
      {
        flag = (int) *a - (int) *b;
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      }
      b++;
      if (!*a++)                                /* If key was NULL */
      {
        if (nextflag == (SEARCH_FIND | SEARCH_UPDATE))
          nextflag=SEARCH_SAME;                 /* Allow duplicate keys */
        next_key_length=key_length;
        a+= keyseg->length;
        b+= keyseg->length;
        key_length-= keyseg->length;
        continue;                               /* To next key part */
      }
    }
    end= a+ min(keyseg->length,key_length);
    next_key_length=key_length-keyseg->length;

    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_TEXT:                       /* Ascii; Key is converted */
      if (keyseg->flag & HA_SPACE_PACK)
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if ((flag=_mi_compare_text(keyseg->charset,a,a_length,b,b_length,
                                   (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                              next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
      else
      {
	uint length=(uint) (end-a), a_length=length, b_length=length;
	if (!(nextflag & SEARCH_PREFIX))
	{
	  while (a_length && a[a_length-1] == ' ')
	    a_length--;
	  while (b_length && b[b_length-1] == ' ')
	    b_length--;
	}
        if ((flag=_mi_compare_text(keyseg->charset,a,a_length,b,b_length,
                                   (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                              next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a=end;
        b+=length;
      }
      break;
    case HA_KEYTYPE_BINARY:
      if (keyseg->flag & HA_SPACE_PACK)
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if ((flag=compare_bin(a,a_length,b,b_length,
                              (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                         next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
      else
      {
        uint length=keyseg->length;
        if ((flag=compare_bin(a,length,b,length,
                              (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                         next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=length;
        b+=length;
      }
      break;
    case HA_KEYTYPE_VARTEXT:
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if ((flag=_mi_compare_text(keyseg->charset,a,a_length,b,b_length,
                                   (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                              next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
      break;
    case HA_KEYTYPE_VARBINARY:
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if ((flag=compare_bin(a,a_length,b,b_length,
                              (my_bool) ((nextflag & SEARCH_PREFIX) &&
                                         next_key_length <= 0))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
      break;
    case HA_KEYTYPE_INT8:
    {
      int i_1= (int) *((signed char*) a);
      int i_2= (int) *((signed char*) b);
      if ((flag = CMP(i_1,i_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a= end;
      b++;
      break;
    }
    case HA_KEYTYPE_SHORT_INT:
      s_1= mi_sint2korr(a);
      s_2= mi_sint2korr(b);
      if ((flag = CMP(s_1,s_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 2; /* sizeof(short int); */
      break;
    case HA_KEYTYPE_USHORT_INT:
      {
        uint16 us_1,us_2;
        us_1= mi_sint2korr(a);
        us_2= mi_sint2korr(b);
        if ((flag = CMP(us_1,us_2)))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a=  end;
        b+=2; /* sizeof(short int); */
        break;
      }
    case HA_KEYTYPE_LONG_INT:
      l_1= mi_sint4korr(a);
      l_2= mi_sint4korr(b);
      if ((flag = CMP(l_1,l_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 4; /* sizeof(long int); */
      break;
    case HA_KEYTYPE_ULONG_INT:
      u_1= mi_sint4korr(a);
      u_2= mi_sint4korr(b);
      if ((flag = CMP(u_1,u_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 4; /* sizeof(long int); */
      break;
    case HA_KEYTYPE_INT24:
      l_1=mi_sint3korr(a);
      l_2=mi_sint3korr(b);
      if ((flag = CMP(l_1,l_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 3;
      break;
    case HA_KEYTYPE_UINT24:
      l_1=mi_uint3korr(a);
      l_2=mi_uint3korr(b);
      if ((flag = CMP(l_1,l_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 3;
      break;
    case HA_KEYTYPE_FLOAT:
      mi_float4get(f_1,a);
      mi_float4get(f_2,b);
      if ((flag = CMP(f_1,f_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 4; /* sizeof(float); */
      break;
    case HA_KEYTYPE_DOUBLE:
      mi_float8get(d_1,a);
      mi_float8get(d_2,b);
      if ((flag = CMP(d_1,d_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 8;  /* sizeof(double); */
      break;
    case HA_KEYTYPE_NUM:                                /* Numeric key */
    {
      int swap_flag= 0;
      int alength,blength;
      
      if (keyseg->flag & HA_REVERSE_SORT)
      {
        swap(uchar*,a,b);       
        swap_flag=1;                            /* Remember swap of a & b */
        end= a+ (int) (end-b);
      }
      if (keyseg->flag & HA_SPACE_PACK)
      {
        alength= *a++; blength= *b++;
        end=a+alength;
        next_key_length=key_length-blength-1;
      }
      else
      {
        alength= (int) (end-a);
        blength=keyseg->length;
        /* remove pre space from keys */
        for ( ; alength && *a == ' ' ; a++, alength--) ;
        for ( ; blength && *b == ' ' ; b++, blength--) ;
      }

      if (*a == '-')
      {
        if (*b != '-')
          return -1;
        a++; b++;
        swap(uchar*,a,b);
        swap(int,alength,blength);
        swap_flag=1-swap_flag;
        alength--; blength--;
        end=a+alength;
      }
      else if (*b == '-')
        return 1;
      while (alength && (*a == '+' || *a == '0'))
      {
        a++; alength--;
      }
      while (blength && (*b == '+' || *b == '0'))
      {
        b++; blength--;
      }
      if (alength != blength)
        return (alength < blength) ? -1 : 1;
      while (a < end)
        if (*a++ !=  *b++)
          return ((int) a[-1] - (int) b[-1]);

      if (swap_flag)                            /* Restore pointers */
        swap(uchar*,a,b);
      break;
    }
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
    {
      longlong ll_a,ll_b;
      ll_a= mi_sint8korr(a);
      ll_b= mi_sint8korr(b);
      if ((flag = CMP(ll_a,ll_b)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 8;
      break;
    }
    case HA_KEYTYPE_ULONGLONG:
    {
      ulonglong ll_a,ll_b;
      ll_a= mi_uint8korr(a);
      ll_b= mi_uint8korr(b);
      if ((flag = CMP(ll_a,ll_b)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 8;
      break;
    }
#endif
    case HA_KEYTYPE_END:                        /* Ready */
      goto end;                                 /* diff_pos is incremented */
    }
  }
  (*diff_pos)++;
end:
  if (!(nextflag & SEARCH_FIND))
  {
    uint i;
    if (nextflag & (SEARCH_NO_FIND | SEARCH_LAST)) /* Find record after key */
      return (nextflag & (SEARCH_BIGGER | SEARCH_LAST)) ? -1 : 1;
    flag=0;
    for (i=keyseg->length ; i-- > 0 ; )
    {
      if (*a++ != *b++)
      {
        flag= FCMP(a[-1],b[-1]);
        break;
      }
    }
    if (nextflag & SEARCH_SAME)
      return (flag);                            /* read same */
    if (nextflag & SEARCH_BIGGER)
      return (flag <= 0 ? -1 : 1);              /* read next */
    return (flag < 0 ? -1 : 1);                 /* read previous */
  }
  return 0;
} /* my_key_cmp */
