/* Copyright (C) 2003 MySQL AB

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

#ifndef __BUFFER_HPP_INCLUDED__
#define __BUFFER_HPP_INCLUDED__

#include <ndb_global.h>

/* This class represents a buffer of binary data, where you can append
 * data at the end, and later read the entire bunch.
 * It will take care of the hairy details of realloc()ing the space
 * for you
 */
class UtilBuffer {
public:
  UtilBuffer() { data = NULL; len = 0; alloc_size = 0; };
  ~UtilBuffer() { if(data) free(data); data = NULL; len = 0; alloc_size = 0; };


  int reallocate(size_t newsize) {
    if(newsize < len) {
      errno = EINVAL;
      return -1;
    }
    void *newdata;
    if((newdata = realloc(data, newsize)) == NULL) {
      errno = ENOMEM;
      return -1;
    }
    alloc_size = newsize;
    data = newdata;
    return 0;
  };

  int grow(size_t l) {
    if(l > alloc_size)
      return reallocate(l);
    return 0;
  };

  int append(const void *d, size_t l) {
    int ret;
    ret = grow(len+l);
    if(ret != 0)
      return ret;
      
    memcpy((char *)data+len, d, l);
    len+=l;

    return 0;
  };

  int assign(const void * d, size_t l) {
    if (data) free(data);
    data = NULL;
    len = 0;
    alloc_size = 0;
    return append(d, l);
  }

  void clear() {
    len = 0;
  }

  int length() const { return len; };

  void *get_data() const { return data; };
private:
  void *data;          /* Pointer to data storage */
  size_t len;          /* Size of the stored data */
  size_t alloc_size;   /* Size of the allocated space,
			*  i.e. len can grow to this size */
};

#endif /* !__BUFFER_HPP_INCLUDED__ */
