#ifndef _BUFFER_ITERATOR_H
#define _BUFFER_ITERATOR_H

#include "mysql_priv.h"

using backup::byte;

/**
 * @class Buffer_iterator
 *
 * @brief Encapsulates data buffering functionality.
 *
 * This class is used in the backup drivers for buffering large blocks
 * of data into smaller windows. This allows the backup drivers to
 * store a large field in multiple blocks of data allocated by the
 * backup kernel.
 *
 * For example, if a driver needs to store a blob field of size 8000 
 * bytes, but the kernel provides a buffer of size 1024 bytes, this 
 * class can be used to break the data into 8 parts. Upon restore, 
 * this class can be used to reassemble the parts into the original
 * size of data and thus write the data as one block to the engine.
 *
 * The class provides two methods for creation. The class can take
 * a pointer to an existing memory block or if omitted will allocate
 * a buffer of the size passed. See the class constructor for more
 * details.
 *
 * To use this class for reading, instantiate the class passing in
 * a pointer to the block you want to read, the total size of the
 * block, and the size of the window you want to read. Then call
 * get_next() for each window you want to read. You can use the
 * num_windows() method to indicate how many windows are left to
 * read. This is best used in a loop-like arrangement like the 
 * following:
 *
 * byte *ptr;
 * byte *outptr;
 * ptr= (byte *)my_malloc(8000, MYF(0));
 * Buffer_iterator *my_buff = new Buffer_iterator(ptr, 8000, 1024);
 * while (my_buff->num_windows())
 * {
 *   bytes_read= my_buff->get_next(&out_ptr);
 *   // do something with the window in out_ptr here 
 * } 
 *
 * Note: If you want to permit the Buffer_iterator class to create
 * it's own buffer, you must use the put_next() method to copy the data
 * from your own buffer into the buffer in the class.
 *
 * To use this class for writing, instantiate the class passing in 
 * the total size of the block, and the size of the window you will be
 * writing. Note: The window size is not used for writing. Then call
 * put_next() to insert a window into the buffer. Once all of the data 
 * has been placed into the buffer, you can use the get_base_ptr() to
 * retrieve the pointer to the buffer. This is best used in a loop-like
 * arrangement like the following:
 *
 * long size; //contains size of window to write
 * byte *ptr; //contains the pointer to the window
 *
 * size= read_my_data(&ptr); //read data here
 * Buffer_iterator *my_buff = new Buffer_iterator(8000, 1024);
 * while (there is data to read)
 * {
 *   my_buff->put_next(&out_ptr, size);
 *   // read more here 
 * } 
 * write_my_reassembled_block(my_buff->get_base_ptr(), total_size);
 *
 */
class Buffer_iterator
{
  public:
    int initialize(byte *buff_ptr, size_t size);
    int initialize(size_t size);
    int reset();
    size_t get_next(byte **buff_ptr, size_t window);
    int put_next(byte *buff_ptr, size_t size);
    int num_windows(size_t size);
    byte *get_base_ptr();

  private: 
    byte *buffer;          ///< The pointer to the block of data to iterate
    byte *cur_ptr;         ///< The current position in the buffer
    size_t max_size;       ///< The maximum size of the block of data
    size_t window_size;    ///< The size of the window to read
    size_t cur_bytes_read; ///< The number of bytes read
    bool alloc_used;       ///< Indicates whether to dealloc memory or not
};

#endif
