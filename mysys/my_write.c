/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <errno.h>


	/* Write a chunk of bytes to a file */

uint my_write(int Filedes, const byte *Buffer, uint Count, myf MyFlags)
{
  uint writenbytes,errors;
  ulong written;
  DBUG_ENTER("my_write");
  DBUG_PRINT("my",("Fd: %d  Buffer: 0x%lx  Count: %d  MyFlags: %d",
		   Filedes, (long) Buffer, Count, MyFlags));
  errors=0; written=0L;

  for (;;)
  {
    if ((writenbytes = (uint) write(Filedes, Buffer, Count)) == Count)
      break;
    if ((int) writenbytes != -1)
    {						/* Safeguard */
      written+=writenbytes;
      Buffer+=writenbytes;
      Count-=writenbytes;
    }
    my_errno=errno;
    DBUG_PRINT("error",("Write only %d bytes, error: %d",
			writenbytes,my_errno));
#ifndef NO_BACKGROUND
#ifdef THREAD
    if (my_thread_var->abort)
      MyFlags&= ~ MY_WAIT_IF_FULL;		/* End if aborted by user */
#endif
    if ((my_errno == ENOSPC || my_errno == EDQUOT) &&
        (MyFlags & MY_WAIT_IF_FULL))
    {
      if (!(errors++ % MY_WAIT_GIVE_USER_A_MESSAGE))
	my_error(EE_DISK_FULL,MYF(ME_BELL | ME_NOREFRESH),
		 my_filename(Filedes),my_errno,MY_WAIT_FOR_USER_TO_FIX_PANIC);
      VOID(sleep(MY_WAIT_FOR_USER_TO_FIX_PANIC));
      continue;
    }

    if ((writenbytes == 0 || (int) writenbytes == -1))
    {
      if (my_errno == EINTR)
      {
        DBUG_PRINT("debug", ("my_write() was interrupted and returned %d",
                             (int) writenbytes));
        continue;                               /* Interrupted */
      }

      if (!writenbytes && !errors++)		/* Retry once */
      {
        /* We may come here if the file quota is exeeded */
        errno=EFBIG;				/* Assume this is the error */
        continue;
      }
    }
    else
      continue;					/* Retry */
#endif
    if (MyFlags & (MY_NABP | MY_FNABP))
    {
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
	my_error(EE_WRITE, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(Filedes),my_errno);
      }
      DBUG_RETURN(MY_FILE_ERROR);		/* Error on read */
    }
    else
      break;					/* Return bytes written */
  }
  if (MyFlags & (MY_NABP | MY_FNABP))
    DBUG_RETURN(0);			/* Want only errors */
  DBUG_RETURN(writenbytes+written);
} /* my_write */
