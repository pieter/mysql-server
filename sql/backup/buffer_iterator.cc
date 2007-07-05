/* Copyright (C) 2004-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, 
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

/**
 * @file
 *
 * @brief Contains a buffering class for breaking large data into parts.
 *
 * This file contains a buffering class for buffering large chunks of
 * data. It can be used to store a large chunk of data and iterate
 * through windows of a specified size until all the data is read.
 * It can also be used to recombine the data from smaller windows.
  */

#include "buffer_iterator.h"

/**
 * @brief Create a buffer iterator.
 *
 * Given a pointer to a block of data, its maximum size, and
 * window size, start iterator for reading or writing data.
 *
 * @param  buff_ptr (in) a pointer to a block of memory
 * @param  size     (in) the maximum size of the data
 */
int Buffer_iterator::initialize(byte *buff_ptr, size_t size)
{
  DBUG_ENTER("buffer_iterator::initialize(buff_ptr, size, window)");
  buffer= buff_ptr;
  max_size= size;
  window_size= 0;
  alloc_used= false;
  cur_bytes_read= 0;
  cur_ptr= buffer;
  DBUG_RETURN(0); 
}

/**
 * @brief Create a buffer iterator.
 *
 * Given the maximum size of a block of data and the
 * window size, start iterator for reading or writing data.
 *
 * @param  size     (in) the maximum size of the data
 */
int Buffer_iterator::initialize(size_t size)
{
  DBUG_ENTER("buffer_iterator::initialize(size, window)");
  buffer= (byte *)my_malloc(size, MYF(MY_WME));
  max_size= size;
  window_size= 0;
  alloc_used= true;
  cur_bytes_read= 0;
  cur_ptr= buffer;
  DBUG_RETURN(0); 
}

/**
 * @brief Reset buffer iterator.
 *
 * Destroy any memory used.
 */
int Buffer_iterator::reset()
{
  DBUG_ENTER("buffer_iterator::reset()");
  if (alloc_used && buffer)
    my_free(buffer, MYF(0));
  alloc_used= FALSE;
  buffer= NULL;
  DBUG_RETURN(0);
}

/**
 * @brief Get the next window of data in the iterator.
 *
 * This method retrieves the next window in the iterator. It
 * returns the number of bytes read (may be less if last
 * window is smaller than the max window size), and updates
 * the pointer passed as an argument.
 *
 * @param  buff_ptr  (in) a pointer to the window to be read
 * @param  window    (in) the size of the window
 * 
 * @retval the size of the window
 */
size_t Buffer_iterator::get_next(byte **buff_ptr, size_t window)
{
  size_t bytes_read;

  DBUG_ENTER("buffer_iterator::get_next()");
  *buff_ptr= cur_ptr;
  if (*buff_ptr)
  {
    if (!window_size)
      window_size= window;
    if ((cur_bytes_read + window_size) < max_size)
    {
      cur_ptr= cur_ptr + window_size;
      bytes_read= window_size;
      cur_bytes_read= cur_bytes_read + window_size;
    }
    else
    { 
      cur_ptr= 0;
      bytes_read= max_size - cur_bytes_read;
      cur_bytes_read= max_size;
    }
  }
  else
    bytes_read= 0;
  DBUG_RETURN(bytes_read);
}

/**
 * @brief Insert the next window of data into the iterator.
 *
 * This method inserts the next window into the iterator. It
 * uses the pointer passed as an argument to copy data from 
 * that location to the internal buffer based on the size of
 * the window passed as an argument.
 *
 * @param  buff_ptr (in/out) a pointer to the window to be written
 * @param  size     (in) the size of the window to be written
 * 
 * @retval 0  success
 * @retval 1  window size exceeds maximum size of the block of data
 */
int Buffer_iterator::put_next(byte *buff_ptr, size_t size)
{
  DBUG_ENTER("buffer_iterator::put_next()");
  /*
    This needs to be a memory copy. Copy to the cur_ptr.
  */
  if (cur_bytes_read + size > max_size)
    DBUG_RETURN(-1); // error buffer overrun
  memcpy(cur_ptr, buff_ptr, size);
  cur_bytes_read= cur_bytes_read + size;
  cur_ptr= cur_ptr + size;
  DBUG_RETURN(0);
}

/**
 * @brief Determines the number of windows left to read.
 *
 * This method calculates how many windows are left to read in
 * the iterator. Use this method following initialize() to
 * determine the maximum windows you can write to the buffer or
 * use this method to determine how many more windows are 
 * remaining to be read.
 * 
 * @param  size     (in) the size of the window 
 *
 * @retval the number of windows left to read
 */
int Buffer_iterator::num_windows(size_t size)
{
  int num_windows;
  DBUG_ENTER("buffer_iterator::num_windows()");
  window_size= size;
  num_windows= (max_size - cur_bytes_read) / window_size;
  if ((max_size - cur_bytes_read) % window_size)
    num_windows++;
  DBUG_RETURN(num_windows);
}

/**
 * @brief Retrieve the pointer to the block of data.
 *
 * This method gets the base pointer to the block of data and 
 * returns it to the caller. This method can be used after writing
 * a series of windows to a buffer. When called, the method turns
 * off the free() mechanism for freeing the base memory allocated.
 * This was included to allow callers to reuse the memory. For 
 * example, this method is used in the default algorithm to read
 * and write blob data. On write, the pointer to the blob data (the
 * data in the buffer) is needed to write to the storage engine. Thus,
 * when this method is called the memory is not freed on destruction.
 * 
 * @retval the pointer to the buffer
 */
byte *Buffer_iterator::get_base_ptr()
{
  byte *ptr;
  DBUG_ENTER("buffer_iterator::get_base_ptr()");
  cur_bytes_read= 0;
  ptr= buffer;
  cur_ptr= 0;
  if (!alloc_used && buffer)
    buffer= 0;
  DBUG_RETURN(ptr);
}
