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
#include "m_string.h"

/*
  Change size of file.

  SYNOPSIS
    my_chsize()
      fd		File descriptor
      new_length	New file size
      filler		If we don't have truncate, fill up all bytes after
			new_length with this character
      MyFlags		Flags

  DESCRIPTION
    my_chsize() truncates file if shorter, else fill with the filler character

  RETURN VALUE
    0	Ok
    1	Error 
*/

int my_chsize(File fd, my_off_t newlength, int filler, myf MyFlags)
{
  DBUG_ENTER("my_chsize");
  DBUG_PRINT("my",("fd: %d  length: %lu  MyFlags: %d",fd,(ulong) newlength,
		   MyFlags));

#ifdef HAVE_CHSIZE
  if (chsize(fd,(off_t) newlength))
  {
    DBUG_PRINT("error",("errno: %d",errno));
    my_errno=errno;
    if (MyFlags & MY_WME)
      my_error(EE_CANT_CHSIZE,MYF(ME_BELL+ME_WAITTANG),errno);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
#else
  /* if file is shorter, expand with null, else fill unused part with null */
  {
    my_off_t oldsize;
    char buff[IO_SIZE];

    oldsize = my_seek(fd, 0L, MY_SEEK_END, MYF(MY_WME+MY_FAE));
    DBUG_PRINT("info",("old_size: %ld", (ulong) oldsize));

#ifdef HAVE_FTRUNCATE
    if (oldsize > newlength)
    {
      if (ftruncate(fd, (off_t) newlength))
      {
	my_errno=errno;
	DBUG_PRINT("error",("errno: %d",errno));
	if (MyFlags & MY_WME)
	  my_error(EE_CANT_CHSIZE, MYF(ME_BELL+ME_WAITTANG), errno);
	DBUG_RETURN(1);
      }
      DBUG_RETURN(0);
    }
#else
    if (oldsize > newlength)
    {				/* Fill diff with null */
      VOID(my_seek(fd, newlength, MY_SEEK_SET, MYF(MY_WME+MY_FAE)));
      swap(my_off_t, newlength, oldsize);
    }
#endif
    /* Full file with 0 until it's as big as requested */
    bfill(buff, IO_SIZE, filler);
    while (newlength-oldsize > IO_SIZE)
    {
      if (my_write(fd,(byte*) buff,IO_SIZE,MYF(MY_NABP)))
	goto err;
      oldsize+= IO_SIZE;
    }
    if (my_write(fd,(byte*) buff,(uint) (newlength-oldsize),MYF(MY_NABP)))
      goto err;
    DBUG_RETURN(0);
  err:
    if (MyFlags & MY_WME)
      my_error(EE_CANT_CHSIZE,MYF(ME_BELL+ME_WAITTANG),my_errno);
    DBUG_PRINT("error",("errno: %d",my_errno));
    DBUG_RETURN(1);
  }
#endif
}				/* my_chsize */
