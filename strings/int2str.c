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
#include "m_string.h"

/*
  _dig_vec arrays are public because they are used in several outer places.
*/
char NEAR _dig_vec_upper[] =
  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char NEAR _dig_vec_lower[] =
  "0123456789abcdefghijklmnopqrstuvwxyz";


/*
  Convert integer to its string representation in given scale of notation.
   
  SYNOPSIS
    int2str()
      val     - value to convert
      dst     - points to buffer where string representation should be stored
      radix   - radix of scale of notation
      upcase  - flag indicating that whenever we should use upper-case digits

  DESCRIPTION
    Converts the (long) integer value to its character form and moves it to 
    the destination buffer followed by a terminating NUL. 
    If radix is -2..-36, val is taken to be SIGNED, if radix is  2..36, val is
    taken to be UNSIGNED. That is, val is signed if and only if radix is. 
    All other radixes treated as bad and nothing will be changed in this case.

    For conversion to decimal representation (radix is -10 or 10) one can use
    optimized int10_to_str() function.

  RETURN VALUE
    Pointer to ending NUL character or NullS if radix is bad.
*/
  
char *
int2str(register long int val, register char *dst, register int radix, 
        char upcase)
{
  char buffer[65];
  register char *p;
  long int new_val;
  char *dig_vec= upcase ? _dig_vec_upper : _dig_vec_lower;

  if (radix < 0) {
    if (radix < -36 || radix > -2) return NullS;
    if (val < 0) {
      *dst++ = '-';
      val = -val;
    }
    radix = -radix;
  } else {
    if (radix > 36 || radix < 2) return NullS;
  }
  /*  The slightly contorted code which follows is due to the
      fact that few machines directly support unsigned long / and %.
      Certainly the VAX C compiler generates a subroutine call.  In
      the interests of efficiency (hollow laugh) I let this happen
      for the first digit only; after that "val" will be in range so
      that signed integer division will do.  Sorry 'bout that.
      CHECK THE CODE PRODUCED BY YOUR C COMPILER.  The first % and /
      should be unsigned, the second % and / signed, but C compilers
      tend to be extraordinarily sensitive to minor details of style.
      This works on a VAX, that's all I claim for it.
      */
  p = &buffer[sizeof(buffer)-1];
  *p = '\0';
  new_val=(ulong) val / (ulong) radix;
  *--p = dig_vec[(uchar) ((ulong) val- (ulong) new_val*(ulong) radix)];
  val = new_val;
#ifdef HAVE_LDIV
  while (val != 0)
  {
    ldiv_t res;
    res=ldiv(val,radix);
    *--p = dig_vec[res.rem];
    val= res.quot;
  }
#else
  while (val != 0)
  {
    new_val=val/radix;
    *--p = dig_vec[(uchar) (val-new_val*radix)];
    val= new_val;
  }
#endif
  while ((*dst++ = *p++) != 0) ;
  return dst-1;
}


/*
  Converts integer to its string representation in decimal notation.
   
  SYNOPSIS
    int10_to_str()
      val     - value to convert
      dst     - points to buffer where string representation should be stored
      radix   - flag that shows whenever val should be taken as signed or not

  DESCRIPTION
    This is version of int2str() function which is optimized for normal case
    of radix 10/-10. It takes only sign of radix parameter into account and 
    not its absolute value.

  RETURN VALUE
    Pointer to ending NUL character.
*/

char *int10_to_str(long int val,char *dst,int radix)
{
  char buffer[65];
  register char *p;
  long int new_val;

  if (radix < 0)				/* -10 */
  {
    if (val < 0)
    {
      *dst++ = '-';
      val = -val;
    }
  }

  p = &buffer[sizeof(buffer)-1];
  *p = '\0';
  new_val= (long) ((unsigned long int) val / 10);
  *--p = '0'+ (char) ((unsigned long int) val - (unsigned long) new_val * 10);
  val = new_val;

  while (val != 0)
  {
    new_val=val/10;
    *--p = '0' + (char) (val-new_val*10);
    val= new_val;
  }
  while ((*dst++ = *p++) != 0) ;
  return dst-1;
}
